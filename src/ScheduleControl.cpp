#include "ScheduleControl.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "LightControl.h"
#include "RTC.h"
#include "PresetControl.h"

static const char* SCHEDULE_FILE = "/schedules/yearly.json";
static const char* TEMP_SCHEDULE_FILE = "/schedules/yearly.tmp";
static const size_t MAX_DAILY_TRANSITIONS = 24;

struct TransitionItem {
  int minutes;
  char preset[32];
};

static TransitionItem cachedTransitions[MAX_DAILY_TRANSITIONS];
static size_t cachedCount = 0;
static bool cacheValid = false;

static String currentPreset = "";
static String currentDay = "";
static SemaphoreHandle_t fsMutex = nullptr;

static bool lockFS(TickType_t timeout = pdMS_TO_TICKS(2000)) {
  if (!fsMutex) {
    fsMutex = xSemaphoreCreateMutex();
  }
  return (fsMutex != nullptr) && (xSemaphoreTake(fsMutex, timeout) == pdTRUE);
}

static void unlockFS() {
  if (fsMutex) {
    xSemaphoreGive(fsMutex);
  }
}

// Загрузка расписания на конкретный день в быстрый RAM-кэш
static void reloadDailyCache(const String& dateStr) {
  cachedCount = 0;
  cacheValid = false;

  if (!lockFS()) return;

  File file = LittleFS.open(SCHEDULE_FILE, "r");
  if (!file) {
    unlockFS();
    cacheValid = true;
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  unlockFS();

  if (error) {
    Serial.println("Cache parse error: " + String(error.c_str()));
    return;
  }

  char yearKey[8];
  dateStr.substring(0, 4).toCharArray(yearKey, sizeof(yearKey));
  String monthDay = dateStr.substring(5);

  if (doc[yearKey].is<JsonObject>() && doc[yearKey][monthDay].is<JsonArray>()) {
    JsonArray arr = doc[yearKey][monthDay].as<JsonArray>();
    for (JsonObject obj : arr) {
      if (cachedCount >= MAX_DAILY_TRANSITIONS) break;

      String tStr = obj["time"] | "";
      String pStr = obj["preset"] | "";
      tStr.trim();
      pStr.trim();

      if (tStr.length() >= 5 && !pStr.isEmpty()) {
        int h = tStr.substring(0, 2).toInt();
        int m = tStr.substring(3, 5).toInt();
        cachedTransitions[cachedCount].minutes = h * 60 + m;
        pStr.toCharArray(cachedTransitions[cachedCount].preset, sizeof(cachedTransitions[cachedCount].preset));
        cachedCount++;
      }
    }
  }

  cacheValid = true;
  Serial.printf("Schedule cache updated for %s (%u transitions)\n", dateStr.c_str(), (unsigned int)cachedCount);
}

void initSchedule() {
  if (!fsMutex) {
    fsMutex = xSemaphoreCreateMutex();
  }

  if (!LittleFS.begin(true)) {
    Serial.println("Failed to mount LittleFS for schedule");
    return;
  }

  if (!lockFS()) return;

  if (!LittleFS.exists("/schedules")) {
    LittleFS.mkdir("/schedules");
  }

  if (!LittleFS.exists(SCHEDULE_FILE)) {
    File file = LittleFS.open(SCHEDULE_FILE, "w");
    if (file) {
      file.println("{}");
      file.close();
      Serial.println("Created empty yearly.json");
    }
  }
  unlockFS();
  Serial.println("Schedule initialized");
}

String getDaySchedule(const String& date) {
  if (!lockFS()) return "[]";

  File file = LittleFS.open(SCHEDULE_FILE, "r");
  if (!file) {
    unlockFS();
    return "[]";
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  unlockFS();

  if (error) {
    Serial.println("Failed to parse yearly.json: " + String(error.c_str()));
    return "[]";
  }

  char yearKey[8];
  date.substring(0, 4).toCharArray(yearKey, sizeof(yearKey));
  String monthDay = date.substring(5);

  if (doc[yearKey].is<JsonObject>() && doc[yearKey][monthDay].is<JsonArray>()) {
    String json;
    serializeJson(doc[yearKey][monthDay], json);
    return json;
  }

  return "[]";
}

bool setDaySchedule(const String& date, const String& scheduleJson) {
  JsonDocument newSchedule;
  DeserializationError error = deserializeJson(newSchedule, scheduleJson);
  if (error || !newSchedule.is<JsonArray>()) {
    Serial.println("Invalid schedule JSON");
    return false;
  }

  if (!lockFS()) return false;

  JsonDocument doc;
  File file = LittleFS.open(SCHEDULE_FILE, "r");
  if (file) {
    deserializeJson(doc, file);
    file.close();
  }

  char yearKey[8];
  date.substring(0, 4).toCharArray(yearKey, sizeof(yearKey));
  String monthDay = date.substring(5);

  if (!doc[yearKey].is<JsonObject>()) {
    doc[yearKey].to<JsonObject>();
  }
  doc[yearKey][monthDay] = newSchedule;

  // Атомарная запись через временный файл
  File tmpFile = LittleFS.open(TEMP_SCHEDULE_FILE, "w");
  if (!tmpFile) {
    unlockFS();
    Serial.println("Failed to open temp schedule file");
    return false;
  }

  size_t written = serializeJson(doc, tmpFile);
  tmpFile.close();

  if (written > 0) {
    LittleFS.remove(SCHEDULE_FILE);
    LittleFS.rename(TEMP_SCHEDULE_FILE, SCHEDULE_FILE);
  }
  unlockFS();

  // Сбрасываем кэш, если изменили расписание на текущий день
  if (date == currentDay) {
    cacheValid = false;
    currentPreset = "";
  }

  Serial.println("Updated schedule for " + date);
  return written > 0;
}

String getYearSchedule(int year) {
  if (!lockFS()) return "{}";

  File file = LittleFS.open(SCHEDULE_FILE, "r");
  if (!file) {
    unlockFS();
    return "{}";
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  unlockFS();

  if (error) return "{}";

  char yearKey[8];
  snprintf(yearKey, sizeof(yearKey), "%04d", year);

  if (doc[yearKey].is<JsonObject>()) {
    String json;
    serializeJson(doc[yearKey], json);
    return json;
  }

  return "{}";
}

bool importSchedule(const String& json) {
  // Проверяем валидность JSON до записи на флеш
  JsonDocument checkDoc;
  if (deserializeJson(checkDoc, json) || !checkDoc.is<JsonObject>()) {
    Serial.println("Rejected invalid schedule import");
    return false;
  }

  if (!lockFS()) return false;

  File tmpFile = LittleFS.open(TEMP_SCHEDULE_FILE, "w");
  if (!tmpFile) {
    unlockFS();
    return false;
  }

  size_t written = tmpFile.print(json);
  tmpFile.close();

  if (written > 0) {
    LittleFS.remove(SCHEDULE_FILE);
    LittleFS.rename(TEMP_SCHEDULE_FILE, SCHEDULE_FILE);
  }
  unlockFS();

  cacheValid = false;
  currentPreset = "";
  Serial.println("Schedule imported successfully");
  return written > 0;
}

String exportSchedule() {
  if (!lockFS()) return "{}";

  File file = LittleFS.open(SCHEDULE_FILE, "r");
  if (!file) {
    unlockFS();
    return "{}";
  }

  String raw = file.readString();
  file.close();
  unlockFS();

  return raw.isEmpty() ? "{}" : raw;
}

String getCurrentDaySchedule() {
  String datetime = getCurrentDateTime();
  if (datetime == "----/--/-- --:--:--") return "[]";

  int year, month, day;
  if (sscanf(datetime.c_str(), "%d/%d/%d", &year, &month, &day) != 3) return "[]";

  char dateStr[11];
  snprintf(dateStr, sizeof(dateStr), "%04d-%02d-%02d", year, month, day);
  return getDaySchedule(String(dateStr));
}

void handleSchedule() {
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck < 15000) return; // Проверяем каждые 15 сек (теперь это бесплатно для CPU/Flash)
  lastCheck = millis();

  String datetime = getCurrentDateTime();
  if (datetime == "----/--/-- --:--:--") return;

  int year, month, day, hour, minute, second;
  if (sscanf(datetime.c_str(), "%d/%d/%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second) != 6) {
    return;
  }

  char dateStr[11];
  snprintf(dateStr, sizeof(dateStr), "%04d-%02d-%02d", year, month, day);
  String currentDate = String(dateStr);

  if (currentDay != currentDate) {
    currentDay = currentDate;
    currentPreset = "";
    cacheValid = false;
    Serial.println("New day: " + currentDate);
  }

  if (!cacheValid) {
    reloadDailyCache(currentDate);
  }

  if (cachedCount == 0) {
    if (currentPreset != "default") {
      if (loadLightingPreset("default")) {
        currentPreset = "default";
      }
    }
    return;
  }

  int currentMinutes = hour * 60 + minute;
  int bestMinutes = -1;
  String targetPreset = "";

  // Ищем самый поздний переход, время которого уже наступило (независимо от порядка в JSON)
  for (size_t i = 0; i < cachedCount; i++) {
    if (cachedTransitions[i].minutes <= currentMinutes && cachedTransitions[i].minutes >= bestMinutes) {
      bestMinutes = cachedTransitions[i].minutes;
      targetPreset = String(cachedTransitions[i].preset);
    }
  }

  if (!targetPreset.isEmpty() && targetPreset != currentPreset) {
    if (loadLightingPreset(targetPreset)) {
      Serial.printf("Loaded preset '%s' at %02d:%02d\n", targetPreset.c_str(), hour, minute);
      currentPreset = targetPreset;
    } else {
      Serial.println("Failed to load preset: " + targetPreset);
    }
  }
}
