#include "PresetControl.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "LightControl.h"
#include "ScheduleControl.h"

static const char* LIGHTING_PRESETS_DIR = "/presets/lighting/";
static const char* SCHEDULE_PRESETS_DIR = "/presets/schedule/";

void initPresets() {
  if (!LittleFS.begin(true)) {
    Serial.println("Failed to mount LittleFS for presets");
    return;
  }

  // Створюємо директорії якщо не існують
  if (!LittleFS.exists("/presets")) {
    LittleFS.mkdir("/presets");
  }
  if (!LittleFS.exists(LIGHTING_PRESETS_DIR)) {
    LittleFS.mkdir(LIGHTING_PRESETS_DIR);
  }
  if (!LittleFS.exists(SCHEDULE_PRESETS_DIR)) {
    LittleFS.mkdir(SCHEDULE_PRESETS_DIR);
  }

  Serial.println("Presets initialized");
}

String getLightingPresets() {
  JsonDocument doc;
  JsonArray presets = doc.to<JsonArray>();

  File root = LittleFS.open(LIGHTING_PRESETS_DIR);
  if (!root || !root.isDirectory()) {
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
  return json;
}

String getSchedulePresets() {
  JsonDocument doc;
  JsonArray presets = doc.to<JsonArray>();

  File root = LittleFS.open(SCHEDULE_PRESETS_DIR);
  if (!root || !root.isDirectory()) {
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
  return json;
}

void saveLightingPreset(const String& name, const String& config) {
  String filename = LIGHTING_PRESETS_DIR + name + ".json";
  File file = LittleFS.open(filename, "w");
  if (!file) {
    Serial.println("Failed to create lighting preset file: " + filename);
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
      // Додамо гама та maxBrightness якщо потрібно
    }

    serializeJson(doc, file);
  } else {
    file.print(config);
  }

  file.close();
  Serial.println("Lighting preset saved: " + name);
}

void saveSchedulePreset(const String& name, const String& config) {
  String filename = SCHEDULE_PRESETS_DIR + name + ".json";
  File file = LittleFS.open(filename, "w");
  if (!file) {
    Serial.println("Failed to create schedule preset file: " + filename);
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
}

bool loadLightingPreset(const String& name) {
  String filename = LIGHTING_PRESETS_DIR + name + ".json";
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
  String filename = dir + name + ".json";

  if (LittleFS.remove(filename)) {
    Serial.println("Preset deleted: " + type + "/" + name);
  } else {
    Serial.println("Failed to delete preset: " + filename);
  }
}
