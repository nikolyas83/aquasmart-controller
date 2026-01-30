#include <Arduino.h>
#include <WiFi.h>
#include "AquaWiFiControl.h"
#include "WebServerControl.h"
#include "LightControl.h"
#include "TemperatureControl.h"
#include "EEPROMControl.h"
#include "RTC.h"
#include "MQTTControl.h"
#include "ScheduleControl.h"
#include "PresetControl.h"
#include <Wire.h>
#include <LittleFS.h>

TaskHandle_t wifiTaskHandle = nullptr;
TaskHandle_t lightTaskHandle = nullptr;
TaskHandle_t tempTaskHandle = nullptr;
TaskHandle_t rtcTaskHandle = nullptr;
TaskHandle_t mqttTaskHandle = nullptr;
TaskHandle_t scheduleTaskHandle = nullptr;

void WiFiTask(void* pv) {
  for (;;) {
    handleWiFi();
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void LightTask(void* pv) {
  for (;;) {
    handleLightEffects();
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void TempTask(void* pv) {
  for (;;) {
    handleTemperature();
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

void RTCTask(void* pv) {
  static unsigned long lastSync = 0;
  for (;;) {
    if (WiFi.status() == WL_CONNECTED && millis() - lastSync > 3600000UL) {
      syncTimeWithNTP("pool.ntp.org");
      lastSync = millis();
    }
    vTaskDelay(pdMS_TO_TICKS(10000));
  }
}

void MQTTTask(void* pv) {
  for (;;) {
    handleMQTT();
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void ScheduleTask(void* pv) {
  for (;;) {
    handleSchedule();
    vTaskDelay(pdMS_TO_TICKS(60000));
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("=== AquaSmart FreeRTOS стартує ===");

  // Mount LittleFS once during setup to avoid multiple mounts
  if (!LittleFS.begin(true)) {
    Serial.println("Failed to mount LittleFS in setup");
  } else {
    Serial.println("LittleFS mounted");
  }

  initEEPROM();
  Wire.begin();
  initTemperature();
  initWiFi();
  initRTC();
  restoreTimeFromPreferences();
  initLight();
  initMQTT();
  initSchedule();
  initPresets();
  initWebServer();

  xTaskCreatePinnedToCore(WiFiTask, "WiFiTask", 4096, nullptr, 2, &wifiTaskHandle, 1);
  xTaskCreatePinnedToCore(LightTask, "LightTask", 2048, nullptr, 1, &lightTaskHandle, 1);
  xTaskCreatePinnedToCore(TempTask, "TempTask", 2048, nullptr, 1, &tempTaskHandle, 1);
  xTaskCreatePinnedToCore(RTCTask, "RTCTask", 2048, nullptr, 0, &rtcTaskHandle, 0);
  xTaskCreatePinnedToCore(MQTTTask, "MQTTTask", 4096, nullptr, 1, &mqttTaskHandle, 1);
  xTaskCreatePinnedToCore(ScheduleTask, "ScheduleTask", 4096, nullptr, 1, &scheduleTaskHandle, 1);

  Serial.println("Усі таски створені.");
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
