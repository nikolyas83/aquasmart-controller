#include "TemperatureControl.h"
#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 4

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

static bool sensorInitialized = false;
static volatile float cachedTemperature = 0.0f;

// Внутреннее чтение с датчика с защитой от сбоев CRC и значения сброса 85.0°C
static float readSensorHardware() {
  sensors.requestTemperatures();
  float temp = sensors.getTempCByIndex(0);

  // Повторная попытка при сбое шины 1-Wire или коде сброса 85.0°C
  if (temp == DEVICE_DISCONNECTED_C || temp == 85.0f || temp < -50.0f || temp > 100.0f) {
    delay(50);
    sensors.requestTemperatures();
    temp = sensors.getTempCByIndex(0);
  }

  if (temp == DEVICE_DISCONNECTED_C || temp == 85.0f || temp < -50.0f || temp > 100.0f) {
    return DEVICE_DISCONNECTED_C;
  }
  return temp;
}

void initTemperature() {
  sensors.begin();
  sensors.setResolution(12); // Точность 0.0625 °C
  sensors.setWaitForConversion(true);

  float temp = readSensorHardware();
  if (temp != DEVICE_DISCONNECTED_C) {
    sensorInitialized = true;
    cachedTemperature = temp;
    Serial.printf("DS18B20 initialized: %.2f °C\n", cachedTemperature);
  } else {
    sensorInitialized = false;
    cachedTemperature = 0.0f;
    Serial.println("Warning: DS18B20 not detected at startup");
  }
}

// Мгновенный возврат кэшированной температуры без блокировки веб-сервера и MQTT
float getTemperature() {
  if (!sensorInitialized) return 0.0f;
  return cachedTemperature;
}

void handleTemperature() {
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck < 5000) return;
  lastCheck = millis();

  // Если датчик не был найден при старте, пробуем переинициализировать
  if (!sensorInitialized) {
    sensors.begin();
    if (sensors.getDeviceCount() > 0) {
      sensors.setResolution(12);
      sensors.setWaitForConversion(true);
      sensorInitialized = true;
      Serial.println("DS18B20 reconnected!");
    } else {
      return;
    }
  }

  float temp = readSensorHardware();
  if (temp != DEVICE_DISCONNECTED_C) {
    cachedTemperature = temp;
    Serial.printf("Temperature: %.1f °C\n", cachedTemperature);
  } else {
    Serial.println("Error: DS18B20 read failed (CRC or disconnected)");
    sensorInitialized = false;
  }
}
