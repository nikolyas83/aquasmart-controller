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

  initEEPROM();
  Wire.begin();
  initTemperature();
  initWiFi();
  initRTC();
  initLight();
  initMQTT();
  initSchedule();
  initPresets();
  initWebServer();

  xTaskCreatePinnedToCore(WiFiTask, "WiFiTask", 4096, nullptr, 2, &wifiTaskHandle, 1);
  xTaskCreatePinnedToCore(LightTask, "LightTask", 4096, nullptr, 1, &lightTaskHandle, 1);
  xTaskCreatePinnedToCore(TempTask, "TempTask", 4096, nullptr, 1, &tempTaskHandle, 1);
  xTaskCreatePinnedToCore(RTCTask, "RTCTask", 4096, nullptr, 0, &rtcTaskHandle, 0);
  xTaskCreatePinnedToCore(MQTTTask, "MQTTTask", 8192, nullptr, 1, &mqttTaskHandle, 1);
  xTaskCreatePinnedToCore(ScheduleTask, "ScheduleTask", 8192, nullptr, 1, &scheduleTaskHandle, 1);

  Serial.println("Усі таски створені.");
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
