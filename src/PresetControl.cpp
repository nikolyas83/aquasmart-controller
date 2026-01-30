#include "PresetControl.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "LightControl.h"
#include "ScheduleControl.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static const char* LIGHTING_PRESETS_DIR = "/presets/lighting";
static const char* SCHEDULE_PRESETS_DIR = "/presets/schedule";
static SemaphoreHandle_t presetMutex = nullptr;

void initPresets() {
  if (presetMutex == nullptr) {
    presetMutex = xSemaphoreCreateMutex();
  }
  
  // LittleFS should be mounted once in main setup(); do not remount here

  // Створюємо директорії якщо не існують
  if (!LittleFS.exists("/presets")) {
    if (!LittleFS.mkdir("/presets")) {
      Serial.println("Failed to create /presets directory");
      return;
    }
  }
  if (!LittleFS.exists(LIGHTING_PRESETS_DIR)) {
    if (!LittleFS.mkdir(LIGHTING_PRESETS_DIR)) {
      Serial.println("Failed to create lighting presets directory");
    }
  }
  if (!LittleFS.exists(SCHEDULE_PRESETS_DIR)) {
    if (!LittleFS.mkdir(SCHEDULE_PRESETS_DIR)) {
      Serial.println("Failed to create schedule presets directory");
    }
  }

  // Create a default lighting preset if missing
  String defaultLightingFile = String(LIGHTING_PRESETS_DIR) + "/default.json";
  if (!LittleFS.exists(defaultLightingFile)) {
    File f = LittleFS.open(defaultLightingFile, "w");
    if (f) {
      // simple default channels
      f.print("{\"channels\":[{\"id\":0,\"brightness\":128},{\"id\":1,\"brightness\":128},{\"id\":2,\"brightness\":128},{\"id\":3,\"brightness\":128},{\"id\":4,\"brightness\":128}]} ");
      f.close();
      Serial.println("Created default lighting preset");
    } else {
      Serial.println("Failed to create default lighting preset");
    }
  }

  Serial.println("Presets initialized");
}

String getLightingPresets() {
  if (xSemaphoreTake(presetMutex, pdMS_TO_TICKS(500)) != pdTRUE) {
    Serial.println("Failed to acquire preset mutex");
    return "[]";
  }
  
  JsonDocument doc;
  JsonArray presets = doc.to<JsonArray>();

  File root = LittleFS.open(LIGHTING_PRESETS_DIR);
  if (!root || !root.isDirectory()) {
    xSemaphoreGive(presetMutex);
    return "[]";
  }

  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      String filename = file.name();
      if (filename.endsWith(".json")) {
        String base = filename;
        int p = filename.lastIndexOf('/');
        if (p >= 0) base = filename.substring(p + 1);
        String name = base.substring(0, base.length() - 5);
        JsonObject preset = presets.add<JsonObject>();
        preset["name"] = name;
        preset["file"] = filename;
      }
    }
    file = root.openNextFile();
  }

  String json;
  serializeJson(doc, json);
  xSemaphoreGive(presetMutex);
  return json;
}

String getSchedulePresets() {
  if (xSemaphoreTake(presetMutex, pdMS_TO_TICKS(500)) != pdTRUE) {
    Serial.println("Failed to acquire preset mutex");
    return "[]";
  }
  
  JsonDocument doc;
  JsonArray presets = doc.to<JsonArray>();

  File root = LittleFS.open(SCHEDULE_PRESETS_DIR);
  if (!root || !root.isDirectory()) {
    xSemaphoreGive(presetMutex);
    return "[]";
  }

  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      String filename = file.name();
      if (filename.endsWith(".json")) {
        String name = filename.substring(0, filename.length() - 5);
        JsonObject preset = presets.add<JsonObject>();
        preset["name"] = name;
        preset["file"] = filename;
      }
    }
    file = root.openNextFile();
  }

  String json;
  serializeJson(doc, json);
  xSemaphoreGive(presetMutex);
  return json;
}

void saveLightingPreset(const String& name, const String& config) {
  if (name.length() == 0 || name.length() > 100) {
    Serial.println("Invalid preset name length");
    return;
  }
  
  if (xSemaphoreTake(presetMutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
    Serial.println("Failed to acquire preset mutex");
    return;
  }
  
  String filename = String(LIGHTING_PRESETS_DIR) + "/" + name + ".json";
  File file = LittleFS.open(filename, "w");
  if (!file) {
    Serial.println("Failed to create lighting preset file: " + filename);
    xSemaphoreGive(presetMutex);
    return;
  }

  // Якщо config порожній, зберігаємо поточні налаштування
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
  Serial.println("Lighting preset saved: " + name);
  xSemaphoreGive(presetMutex);
}

void saveSchedulePreset(const String& name, const String& config) {
  if (name.length() == 0 || name.length() > 100) {
    Serial.println("Invalid preset name length");
    return;
  }
  
  if (xSemaphoreTake(presetMutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
    Serial.println("Failed to acquire preset mutex");
    return;
  }
  
  String filename = String(SCHEDULE_PRESETS_DIR) + "/" + name + ".json";
  File file = LittleFS.open(filename, "w");
  if (!file) {
    Serial.println("Failed to create schedule preset file: " + filename);
    xSemaphoreGive(presetMutex);
    return;
  }

  if (config.isEmpty()) {
    // Створюємо приклад шаблону розкладу
    JsonDocument doc;
    JsonArray schedule = doc.to<JsonArray>();

    JsonObject transition1 = schedule.add<JsonObject>();
    transition1["time"] = "08:00";
    transition1["preset"] = "sunrise";

    JsonObject transition2 = schedule.add<JsonObject>();
    transition2["time"] = "12:00";
    transition2["preset"] = "daylight";

    JsonObject transition3 = schedule.add<JsonObject>();
    transition3["time"] = "20:00";
    transition3["preset"] = "sunset";

    JsonObject transition4 = schedule.add<JsonObject>();
    transition4["time"] = "22:00";
    transition4["preset"] = "night";

    serializeJson(doc, file);
  } else {
    file.print(config);
  }

  file.close();
  Serial.println("Schedule preset saved: " + name);
  xSemaphoreGive(presetMutex);
}

bool loadLightingPreset(const String& name) {
  String filename = String(LIGHTING_PRESETS_DIR) + "/" + name + ".json";
  File file = LittleFS.open(filename, "r");
  if (!file) {
    Serial.println("Failed to open lighting preset: " + filename);
    return false;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    Serial.println("Failed to parse lighting preset: " + String(error.c_str()));
    return false;
  }

  if (doc["channels"].is<JsonArray>()) {
    for (JsonObject channel : doc["channels"].as<JsonArray>()) {
      int id = channel["id"];
      int brightness = channel["brightness"];
      setChannelBrightness(id, brightness);
    }
  }

  Serial.println("Lighting preset loaded: " + name);
  return true;
}

void deletePreset(const String& type, const String& name) {
  String dir = (type == "lighting") ? LIGHTING_PRESETS_DIR : SCHEDULE_PRESETS_DIR;
  String filename = String(dir) + "/" + name + ".json";

  if (LittleFS.remove(filename)) {
    Serial.println("Preset deleted: " + type + "/" + name);
  } else {
    Serial.println("Failed to delete preset: " + filename);
  }
}
