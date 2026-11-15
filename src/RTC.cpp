#include "RTC.h"
#include <Wire.h>
#include <RTClib.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <time.h>

// Используем глобальный мьютекс шины I2C из main.cpp
extern SemaphoreHandle_t i2cMutex;

RTC_DS1307 rtc;
bool rtcInitialized = false;

// Сохраняем текущий часовой пояс, чтобы NTP-синхронизация не сбрасывала его в UTC
static long savedGmtOffset = 0;
static int savedDaylightOffset = 0;

static bool lockI2C(TickType_t timeout = pdMS_TO_TICKS(100)) {
  if (!i2cMutex) return true;
  return xSemaphoreTake(i2cMutex, timeout) == pdTRUE;
}

static void unlockI2C() {
  if (i2cMutex) xSemaphoreGive(i2cMutex);
}

void initRTC() {
  if (!lockI2C(pdMS_TO_TICKS(500))) return;

  if (!rtc.begin()) {
    unlockI2C();
    Serial.println("Error: DS1307 not found");
    rtcInitialized = false;
    return;
  }

  // Если батарейка садилась и генератор остановлен — запускаем с временем компиляции
  if (!rtc.isrunning()) {
    Serial.println("DS1307 was halted, starting with compile time...");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  rtcInitialized = true;
  unlockI2C();
  Serial.println("DS1307 initialized");
}

void setManualTime(int year, int month, int day, int hour, int minute, int second) {
  if (!rtcInitialized) {
    Serial.println("Error: RTC not initialized");
    return;
  }
  if (year < 2000 || year > 2099 || month < 1 || month > 12 || day < 1 || day > 31 ||
      hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) {
    Serial.println("Error: Invalid time parameters");
    return;
  }

  if (lockI2C()) {
    rtc.adjust(DateTime(year, month, day, hour, minute, second));
    unlockI2C();
    Serial.println("Manual time set: " + getCurrentDateTime());
  }
}

void setTimeZone(long offset, int dst) {
  savedGmtOffset = offset;
  savedDaylightOffset = dst * 3600;
  configTime(savedGmtOffset, savedDaylightOffset, "pool.ntp.org");
  Serial.println("Time zone set: offset=" + String(offset) + "s, DST=" + String(dst) + "h");
}

void syncTimeWithNTP(const char* ntpServer) {
  if (!rtcInitialized) {
    Serial.println("Error: RTC not initialized");
    return;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("No WiFi, skipping NTP sync");
    return;
  }

  Serial.println("Starting NTP sync with: " + String(ntpServer));
  // Используем сохранённый часовой пояс вместо нулей (0, 0)
  configTime(savedGmtOffset, savedDaylightOffset, ntpServer);

  struct tm timeinfo;
  // Сетевой запрос выполняем БЕЗ удержания i2cMutex, чтобы не тормозить свет!
  if (!getLocalTime(&timeinfo, 3000)) {
    Serial.println("Error: Failed to get NTP time");
    return;
  }

  DateTime ntpTime(
    timeinfo.tm_year + 1900,
    timeinfo.tm_mon + 1,
    timeinfo.tm_mday,
    timeinfo.tm_hour,
    timeinfo.tm_min,
    timeinfo.tm_sec
  );

  // Захватываем I2C только на микросекунды чтения и корректировки чипа
  if (lockI2C()) {
    DateTime rtcTime = rtc.now();
    uint32_t ntpUnix = ntpTime.unixtime();
    uint32_t rtcUnix = rtcTime.unixtime();
    uint32_t diff = (ntpUnix > rtcUnix) ? (ntpUnix - rtcUnix) : (rtcUnix - ntpUnix);

    if (diff > 5) {
      rtc.adjust(ntpTime);
      unlockI2C();
      Serial.println("DS1307 synced with NTP server: " + String(ntpServer));
    } else {
      unlockI2C();
      Serial.println("RTC time is close to NTP, no update needed");
    }
  }
}

String getCurrentDateTime() {
  if (!rtcInitialized) {
    return "----/--/-- --:--:--";
  }

  bool running = false;
  DateTime now;

  if (lockI2C()) {
    running = rtc.isrunning();
    if (running) {
      now = rtc.now();
    }
    unlockI2C();
  } else {
    return "----/--/-- --:--:--";
  }

  if (!running) {
    return "----/--/-- --:--:--";
  }

  char buffer[24];
  snprintf(buffer, sizeof(buffer), "%04d/%02d/%02d %02d:%02d:%02d",
           now.year(), now.month(), now.day(),
           now.hour(), now.minute(), now.second());
  return String(buffer);
}
