#include "ScheduleControl.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "LightControl.h"
#include "RTC.h"
#include "PresetControl.h"

static const char* SCHEDULE_FILE = "/schedules/yearly.json";
static String currentPreset = "";
static String currentDay = "";

void initSchedule() {
  if (!LittleFS.begin(true)) {
    Serial.println("Failed to mount LittleFS for schedule");
    return;
  }

  // Створюємо директорію якщо потрібно
  if (!LittleFS.exists("/schedules")) {
    LittleFS.mkdir("/schedules");
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
  File file = LittleFS.open(SCHEDULE_FILE, "r");
  if (!file) {
    return "[]";
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    Serial.println("Failed to parse yearly.json: " + String(error.c_str()));
    return "[]";
  }

  // Розбираємо дату YYYY-MM-DD на рік та MM-DD
  int year = date.substring(0, 4).toInt();
  String monthDay = date.substring(5);

  if (doc.containsKey(String(year)) && doc[String(year)].containsKey(monthDay)) {
    String json;
    serializeJson(doc[String(year)][monthDay], json);
    return json;
  }

  return "[]";
}

bool setDaySchedule(const String& date, const String& scheduleJson) {
  File file = LittleFS.open(SCHEDULE_FILE, "r");
  JsonDocument doc;

  if (file) {
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error) {
      Serial.println("Failed to parse yearly.json: " + String(error.c_str()));
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
    return false;
  }

  // Створюємо структуру року якщо потрібно
  if (!doc.containsKey(String(year))) {
    doc.createNestedObject(String(year));
  }

  // Зберігаємо розклад на день
  doc[String(year)][monthDay] = newSchedule;

  // Записуємо назад у файл
  file = LittleFS.open(SCHEDULE_FILE, "w");
  if (!file) {
    Serial.println("Failed to write yearly.json");
    return false;
  }

  serializeJson(doc, file);
  file.close();

  Serial.println("Updated schedule for " + date);
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

  if (doc.containsKey(String(year))) {
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
