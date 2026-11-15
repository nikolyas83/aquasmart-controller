#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <LittleFS.h>
#include "AquaWiFiControl.h"
#include "WebServerControl.h"
#include "LightControl.h"
#include "TemperatureControl.h"
#include "EEPROMControl.h"
#include "RTC.h"
#include "MQTTControl.h"
#include "ScheduleControl.h"
#include "PresetControl.h"

// Глобальный мьютекс для защиты шины I2C (PCA9685 + DS1307)
SemaphoreHandle_t i2cMutex = nullptr;

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
    if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
      handleLightEffects();
      xSemaphoreGive(i2cMutex);
    }
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
    if (WiFi.status() == WL_CONNECTED && (lastSync == 0 || millis() - lastSync > 3600000UL)) {
      if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
        syncTimeWithNTP("pool.ntp.org");
        xSemaphoreGive(i2cMutex);
        lastSync = millis();
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10000));
  }
}

void MQTTTask(void* pv) {
  for (;;) {
    handleMQTT();
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

void ScheduleTask(void* pv) {
  for (;;) {
    if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
      handleSchedule();
      xSemaphoreGive(i2cMutex);
    }
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("=== AquaSmart FreeRTOS стартує ===");

  i2cMutex = xSemaphoreCreateMutex();

  initEEPROM();
  if (!LittleFS.begin(true)) {
    Serial.println("CRITICAL: LittleFS mount failed!");
  }

  Wire.begin();
  Wire.setTimeOut(50); // Защита от вечного зависания I2C при помехах

  initTemperature();
  initWiFi();
  initRTC();
  initLight();
  initPresets();
  initSchedule();
  initMQTT();
  initWebServer();

  xTaskCreatePinnedToCore(WiFiTask,     "WiFiTask",     4096, nullptr, 2, &wifiTaskHandle,     1);
  xTaskCreatePinnedToCore(LightTask,    "LightTask",    4096, nullptr, 2, &lightTaskHandle,    1);
  xTaskCreatePinnedToCore(TempTask,     "TempTask",     4096, nullptr, 1, &tempTaskHandle,     1);
  xTaskCreatePinnedToCore(RTCTask,      "RTCTask",      6144, nullptr, 1, &rtcTaskHandle,      1);
  xTaskCreatePinnedToCore(MQTTTask,     "MQTTTask",     8192, nullptr, 1, &mqttTaskHandle,     1);
  xTaskCreatePinnedToCore(ScheduleTask, "ScheduleTask", 8192, nullptr, 1, &scheduleTaskHandle, 1);

  Serial.println("Усі таски створені. Видаляємо пустий loopTask.");
  vTaskDelete(nullptr); // Освобождаем 8 КБ RAM от пустого loop()
}

void loop() {
  // Не используется, задача удалена в setup()
}
