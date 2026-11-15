#include "PresetControl.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "LightControl.h"
#include "ScheduleControl.h"

static const char* LIGHTING_PRESETS_DIR = "/presets/lighting/";
static const char* SCHEDULE_PRESETS_DIR = "/presets/schedule/";
static SemaphoreHandle_t presetMutex = nullptr;

static bool lockPresets(TickType_t timeout = pdMS_TO_TICKS(1500)) {
  if (!presetMutex) {
    presetMutex = xSemaphoreCreateMutex();
  }
  return (presetMutex != nullptr) && (xSemaphoreTake(presetMutex, timeout) == pdTRUE);
}

static void unlockPresets() {
  if (presetMutex) {
    xSemaphoreGive(presetMutex);
  }
}

// Очистка имени пресета от опасных символов (защита от Path Traversal)
static String sanitizePresetName(const String& rawName) {
  String clean = rawName;
  clean.trim();
  clean.replace("/", "_");
  clean.replace("\\", "_");
  clean.replace("..", "_");
  return clean;
}

// Общий помощник для сканирования директории с гарантированным закрытием файлов
static String listPresetsInDir(const char* dirPath) {
  if (!lockPresets()) return "[]";

  File root = LittleFS.open(dirPath);
  if (!root || !root.isDirectory()) {
    if (root) root.close();
    unlockPresets();
    return "[]";
  }

  JsonDocument doc;
  JsonArray presets = doc.to<JsonArray>();

  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      String filename = String(file.name());
      // В некоторых версиях LittleFS file.name() возвращает полный путь — берём только имя файла
      int lastSlash = filename.lastIndexOf('/');
      if (lastSlash >= 0) {
        filename = filename.substring(lastSlash + 1);
      }

      if (filename.endsWith(".json")) {
        String name = filename.substring(0, filename.length() - 5);
        JsonObject preset = presets.add<JsonObject>();
        preset["name"] = name;
        preset["file"] = filename;
      }
    }
    file.close(); // Обязательно закрываем текущий дескриптор перед открытием следующего
    file = root.openNextFile();
  }
  root.close(); // Закрываем дескриптор директории
  unlockPresets();

  String json;
  serializeJson(doc, json);
  return json;
}

void initPresets() {
  if (!presetMutex) {
    presetMutex = xSemaphoreCreateMutex();
  }

  if (!LittleFS.begin(true)) {
    Serial.println("Failed to mount LittleFS for presets");
    return;
  }

  if (!lockPresets()) return;
  if (!LittleFS.exists("/presets")) LittleFS.mkdir("/presets");
  if (!LittleFS.exists(LIGHTING_PRESETS_DIR)) LittleFS.mkdir(LIGHTING_PRESETS_DIR);
  if (!LittleFS.exists(SCHEDULE_PRESETS_DIR)) LittleFS.mkdir(SCHEDULE_PRESETS_DIR);
  unlockPresets();

  Serial.println("Presets initialized");
}

String getLightingPresets() {
  return listPresetsInDir(LIGHTING_PRESETS_DIR);
}

String getSchedulePresets() {
  return listPresetsInDir(SCHEDULE_PRESETS_DIR);
}

void saveLightingPreset(const String& name, const String& config) {
  String safeName = sanitizePresetName(name);
  if (safeName.isEmpty()) return;

  // Если передан внешний конфиг, проверяем валидность JSON до записи
  if (!config.isEmpty()) {
    JsonDocument checkDoc;
    if (deserializeJson(checkDoc, config) || !checkDoc.is<JsonObject>()) {
      Serial.println("Rejected invalid lighting preset JSON");
      return;
    }
  }

  if (!lockPresets()) return;

  String filename = String(LIGHTING_PRESETS_DIR) + safeName + ".json";
  File file = LittleFS.open(filename, "w");
  if (!file) {
    unlockPresets();
    Serial.println("Failed to create lighting preset file: " + filename);
    return;
  }

  if (config.isEmpty()) {
    JsonDocument doc;
    JsonObject lighting = doc.to<JsonObject>();
    lighting["timestamp"] = millis();

    JsonArray channels = lighting["channels"].to<JsonArray>();
    for (int i = 0; i < 5; i++) {
      JsonObject channel = channels.add<JsonObject>();
      channel["id"] = i;
      channel["brightness"] = getChannelBrightness(i);
    }
    serializeJson(doc, file);
  } else {
    file.print(config);
  }

  file.close();
  unlockPresets();
  Serial.println("Lighting preset saved: " + safeName);
}

void saveSchedulePreset(const String& name, const String& config) {
  String safeName = sanitizePresetName(name);
  if (safeName.isEmpty()) return;

  if (!config.isEmpty()) {
    JsonDocument checkDoc;
    if (deserializeJson(checkDoc, config) || !checkDoc.is<JsonArray>()) {
      Serial.println("Rejected invalid schedule preset JSON");
      return;
    }
  }

  if (!lockPresets()) return;

  String filename = String(SCHEDULE_PRESETS_DIR) + safeName + ".json";
  File file = LittleFS.open(filename, "w");
  if (!file) {
    unlockPresets();
    Serial.println("Failed to create schedule preset file: " + filename);
    return;
  }

  if (config.isEmpty()) {
    JsonDocument doc;
    JsonArray schedule = doc.to<JsonArray>();

    JsonObject t1 = schedule.add<JsonObject>(); t1["time"] = "08:00"; t1["preset"] = "sunrise";
    JsonObject t2 = schedule.add<JsonObject>(); t2["time"] = "12:00"; t2["preset"] = "daylight";
    JsonObject t3 = schedule.add<JsonObject>(); t3["time"] = "20:00"; t3["preset"] = "sunset";
    JsonObject t4 = schedule.add<JsonObject>(); t4["time"] = "22:00"; t4["preset"] = "night";

    serializeJson(doc, file);
  } else {
    file.print(config);
  }

  file.close();
  unlockPresets();
  Serial.println("Schedule preset saved: " + safeName);
}

bool loadLightingPreset(const String& name) {
  String safeName = sanitizePresetName(name);
  if (safeName.isEmpty()) return false;

  if (!lockPresets()) return false;

  String filename = String(LIGHTING_PRESETS_DIR) + safeName + ".json";
  File file = LittleFS.open(filename, "r");
  if (!file) {
    unlockPresets();
    Serial.println("Failed to open lighting preset: " + filename);
    return false;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  unlockPresets();

  if (error) {
    Serial.println("Failed to parse lighting preset: " + String(error.c_str()));
    return false;
  }

  // Применяем яркость каналов уже после освобождения файлового мьютекса
  if (doc["channels"].is<JsonArray>()) {
    for (JsonObject channel : doc["channels"].as<JsonArray>()) {
      int id = channel["id"] | -1;
      int brightness = channel["brightness"] | 0;
      if (id >= 0 && id < 5) {
        setChannelBrightness(id, brightness);
      }
    }
  }

  Serial.println("Lighting preset loaded: " + safeName);
  return true;
}

void deletePreset(const String& type, const String& name) {
  String safeName = sanitizePresetName(name);
  if (safeName.isEmpty()) return;

  if (!lockPresets()) return;

  String dir = (type == "lighting") ? LIGHTING_PRESETS_DIR : SCHEDULE_PRESETS_DIR;
  String filename = dir + safeName + ".json";

  if (LittleFS.remove(filename)) {
    Serial.println("Preset deleted: " + type + "/" + safeName);
  } else {
    Serial.println("Failed to delete preset: " + filename);
  }
  unlockPresets();
}
