#include "ScheduleControl.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "LightControl.h"
#include "RTC.h"
#include "PresetControl.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static const char* SCHEDULE_FILE = "/schedules/yearly.json";
static String currentPreset = "";
static String currentDay = "";
static SemaphoreHandle_t scheduleMutex = nullptr;

// Валідація формату дати YYYY-MM-DD
bool isValidDate(const String& date) {
  if (date.length() != 10) return false;
  if (date[4] != '-' || date[7] != '-') return false;
  
  int year = date.substring(0, 4).toInt();
  int month = date.substring(5, 7).toInt();
  int day = date.substring(8, 10).toInt();
  
  if (year < 2000 || year > 2099) return false;
  if (month < 1 || month > 12) return false;
  if (day < 1 || day > 31) return false;
  
  return true;
}

void initSchedule() {
  if (scheduleMutex == nullptr) {
    scheduleMutex = xSemaphoreCreateMutex();
  }
  
  // LittleFS mounted in main setup(); do not remount here

  // Створюємо директорію якщо потрібно
  if (!LittleFS.exists("/schedules")) {
    if (!LittleFS.mkdir("/schedules")) {
      Serial.println("Failed to create /schedules directory");
      return;
    }
  }

  if (!LittleFS.exists(SCHEDULE_FILE)) {
    File file = LittleFS.open(SCHEDULE_FILE, "w");
    if (file) {
      file.println("{}");
      file.close();
      Serial.println("Created empty yearly.json");
    } else {
      Serial.println("Failed to create yearly.json");
    }
  }
  Serial.println("Schedule initialized");
}

String getDaySchedule(const String& date) {
  if (!isValidDate(date)) {
    Serial.println("Invalid date format: " + date);
    return "[]";
  }
  
  if (xSemaphoreTake(scheduleMutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
    Serial.println("Failed to acquire schedule mutex");
    return "[]";
  }
  
  File file = LittleFS.open(SCHEDULE_FILE, "r");
  if (!file) {
    xSemaphoreGive(scheduleMutex);
    return "[]";
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    Serial.println("Failed to parse yearly.json: " + String(error.c_str()));
    xSemaphoreGive(scheduleMutex);
    return "[]";
  }

  // Розбираємо дату YYYY-MM-DD на рік та MM-DD
  int year = date.substring(0, 4).toInt();
  String monthDay = date.substring(5);

  String result = "[]";
  if (doc[String(year)].is<JsonObject>() && doc[String(year)][monthDay].is<JsonArray>()) {
    serializeJson(doc[String(year)][monthDay], result);
  }

  xSemaphoreGive(scheduleMutex);
  return result;
}

bool setDaySchedule(const String& date, const String& scheduleJson) {
  if (!isValidDate(date)) {
    Serial.println("Invalid date format: " + date);
    return false;
  }
  
  // Перевіряємо розмір JSON (макс 2 KB на день)
  if (scheduleJson.length() > 2048) {
    Serial.println("Schedule JSON too large: " + String(scheduleJson.length()) + " bytes");
    return false;
  }
  
  if (xSemaphoreTake(scheduleMutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
    Serial.println("Failed to acquire schedule mutex");
    return false;
  }
  
  File file = LittleFS.open(SCHEDULE_FILE, "r");
  JsonDocument doc;

  if (file) {
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error) {
      Serial.println("Failed to parse yearly.json: " + String(error.c_str()));
      xSemaphoreGive(scheduleMutex);
      return false;
    }
  }

  // Розбираємо дату YYYY-MM-DD на рік та MM-DD
  int year = date.substring(0, 4).toInt();
  String monthDay = date.substring(5);

  // Парсимо новий розклад
  JsonDocument newSchedule;
  DeserializationError error = deserializeJson(newSchedule, scheduleJson);
  if (error) {
    Serial.println("Failed to parse schedule JSON: " + String(error.c_str()));
    xSemaphoreGive(scheduleMutex);
    return false;
  }

  // Створюємо структуру року якщо потрібно
  if (!doc[String(year)].is<JsonObject>()) {
    doc[String(year)].to<JsonObject>();
  }

  // Зберігаємо розклад на день
  doc[String(year)][monthDay] = newSchedule;

  // Записуємо назад у файл
  file = LittleFS.open(SCHEDULE_FILE, "w");
  if (!file) {
    Serial.println("Failed to write yearly.json");
    xSemaphoreGive(scheduleMutex);
    return false;
  }
  
  serializeJson(doc, file);
  file.close();
  
  Serial.println("Updated schedule for " + date);
  xSemaphoreGive(scheduleMutex);
  return true;
}

String getYearSchedule(int year) {
  File file = LittleFS.open(SCHEDULE_FILE, "r");
  if (!file) {
    return "{}";
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    Serial.println("Failed to parse yearly.json: " + String(error.c_str()));
    return "{}";
  }

  if (doc[String(year)].is<JsonObject>()) {
    String json;
    serializeJson(doc[String(year)], json);
    return json;
  }

  return "{}";
}

bool importSchedule(const String& json) {
  File file = LittleFS.open(SCHEDULE_FILE, "w");
  if (!file) {
    Serial.println("Failed to write yearly.json");
    return false;
  }

  file.print(json);
  file.close();

  Serial.println("Schedule imported successfully");
  return true;
}

String exportSchedule() {
  File file = LittleFS.open(SCHEDULE_FILE, "r");
  if (!file) {
    return "{}";
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    Serial.println("Failed to parse yearly.json: " + String(error.c_str()));
    return "{}";
  }

  String json;
  serializeJson(doc, json);
  return json;
}

String getCurrentDaySchedule() {
  String datetime = getCurrentDateTime();
  if (datetime == "----/--/-- --:--:--") {
    return "[]";
  }

  int year, month, day;
  sscanf(datetime.c_str(), "%d/%d/%d", &year, &month, &day);

  char dateStr[11];
  snprintf(dateStr, sizeof(dateStr), "%04d-%02d-%02d", year, month, day);

  return getDaySchedule(String(dateStr));
}

void handleSchedule() {
  static unsigned long lastCheck = 0;

  if (millis() - lastCheck < 60000) return; // Перевіряємо щохвилини
  lastCheck = millis();

  String datetime = getCurrentDateTime();
  if (datetime == "----/--/-- --:--:--") return;

  int year, month, day, hour, minute, second;
  sscanf(datetime.c_str(), "%d/%d/%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second);

  char dateStr[11];
  snprintf(dateStr, sizeof(dateStr), "%04d-%02d-%02d", year, month, day);
  String currentDate = String(dateStr);

  // Створюємо час у хвилинах від початку доби для порівняння
  int currentMinutes = hour * 60 + minute;

  // Перевіряємо чи змінився день
  if (currentDay != currentDate) {
    currentDay = currentDate;
    currentPreset = "";
    Serial.println("New day: " + currentDate);
  }

  // Отримуємо розклад на поточний день
  String dayScheduleJson = getDaySchedule(currentDate);
  if (dayScheduleJson == "[]") {
    // Немає розкладу на цей день, використовуємо дефолтний пресет
    if (currentPreset != "default") {
      loadLightingPreset("default");
      currentPreset = "default";
    }
    return;
  }

  // Парсимо розклад дня
  JsonDocument daySchedule;
  DeserializationError error = deserializeJson(daySchedule, dayScheduleJson);
  if (error) {
    Serial.println("Failed to parse day schedule: " + String(error.c_str()));
    return;
  }

  // Знаходимо поточний період
  String targetPreset = "";
  JsonArray schedule = daySchedule.as<JsonArray>();

  for (size_t i = 0; i < schedule.size(); i++) {
    JsonObject transition = schedule[i];
    String timeStr = transition["time"];
    String preset = transition["preset"];

    // Розбираємо час HH:MM
    int transitionHour = timeStr.substring(0, 2).toInt();
    int transitionMinute = timeStr.substring(3, 5).toInt();
    int transitionMinutes = transitionHour * 60 + transitionMinute;

    if (currentMinutes >= transitionMinutes) {
      targetPreset = preset;
    } else {
      break; // Переходи відсортовані за часом, можна зупинитися
    }
  }

  // Якщо знайдено цільовий пресет і він відрізняється від поточного
  if (!targetPreset.isEmpty() && targetPreset != currentPreset) {
    if (loadLightingPreset(targetPreset)) {
      Serial.println("Loaded preset '" + targetPreset + "' for " + String(hour) + ":" + String(minute < 10 ? "0" : "") + String(minute));
      currentPreset = targetPreset;
    } else {
      Serial.println("Failed to load preset: " + targetPreset);
    }
  }
}

void addScheduleEvent(const String& event) {
  // Парсимо подію з JSON
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, event);
  if (error) {
    Serial.println("Failed to parse schedule event: " + String(error.c_str()));
    return;
  }

  // Отримуємо поточну дату
  String datetime = getCurrentDateTime();
  if (datetime == "----/--/-- --:--:--") {
    Serial.println("Failed to get current date time");
    return;
  }

  int year, month, day;
  sscanf(datetime.c_str(), "%d/%d/%d", &year, &month, &day);

  char dateStr[11];
  snprintf(dateStr, sizeof(dateStr), "%04d-%02d-%02d", year, month, day);
  String currentDate = String(dateStr);

  // Отримуємо поточний розклад на день
  String dayScheduleJson = getDaySchedule(currentDate);
  JsonDocument daySchedule;
  DeserializationError error2 = deserializeJson(daySchedule, dayScheduleJson);
  if (error2) {
    Serial.println("Failed to parse day schedule: " + String(error2.c_str()));
    return;
  }

  // Додаємо нову подію в розклад
  JsonObject newEvent = daySchedule.as<JsonArray>().add<JsonObject>();
  newEvent["id"] = daySchedule.as<JsonArray>().size() + 1;
  newEvent["type"] = doc["type"];
  newEvent["time"] = doc["time"];
  newEvent["action"] = doc["action"];

  // Зберігаємо оновлений розклад
  String updatedScheduleJson;
  serializeJson(daySchedule, updatedScheduleJson);
  setDaySchedule(currentDate, updatedScheduleJson);

  Serial.println("Added schedule event: " + event);
}

void removeScheduleEvent(int id) {
  // Отримуємо поточну дату
  String datetime = getCurrentDateTime();
  if (datetime == "----/--/-- --:--:--") {
    Serial.println("Failed to get current date time");
    return;
  }

  int year, month, day;
  sscanf(datetime.c_str(), "%d/%d/%d", &year, &month, &day);

  char dateStr[11];
  snprintf(dateStr, sizeof(dateStr), "%04d-%02d-%02d", year, month, day);
  String currentDate = String(dateStr);

  // Отримуємо поточний розклад на день
  String dayScheduleJson = getDaySchedule(currentDate);
  JsonDocument daySchedule;
  DeserializationError error = deserializeJson(daySchedule, dayScheduleJson);
  if (error) {
    Serial.println("Failed to parse day schedule: " + String(error.c_str()));
    return;
  }

  // Видаляємо подію з розкладу
  JsonArray schedule = daySchedule.as<JsonArray>();
  for (size_t i = 0; i < schedule.size(); i++) {
    JsonObject event = schedule[i];
    if (event["id"] == id) {
      schedule.remove(i);
      break;
    }
  }

  // Зберігаємо оновлений розклад
  String updatedScheduleJson;
  serializeJson(daySchedule, updatedScheduleJson);
  setDaySchedule(currentDate, updatedScheduleJson);

  Serial.println("Removed schedule event with id: " + String(id));
}

String getSchedule() {
  // Повертаємо поточний розклад або список подій розкладу
  // Для простоти повертаємо поточний денний розклад
  return getCurrentDaySchedule();
}
