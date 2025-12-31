/*
  RTC.cpp
  Оновлено: Ретельні перевірки, асинхронна NTP-синхронізація
*/

#include "RTC.h"
#include <Wire.h>
#include <RTClib.h>
#include <WiFi.h>
#include <time.h>

RTC_DS1307 rtc;
bool rtcInitialized = false;

void initRTC() {
  if (!rtc.begin()) {
    Serial.println("Error: DS1307 not found");
    rtcInitialized = false;
    return;
  }
  rtcInitialized = true;
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
  rtc.adjust(DateTime(year, month, day, hour, minute, second));
  Serial.println("Manual time set: " + getCurrentDateTime());
}

void setTimeZone(long offset, int dst) {
  if (!rtcInitialized) {
    Serial.println("Error: RTC not initialized");
    return;
  }
  configTime(offset, dst * 3600, "pool.ntp.org");
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
  configTime(0, 0, ntpServer);
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
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
  DateTime rtcTime = rtc.now();
  uint32_t ntpUnix = ntpTime.unixtime();
  uint32_t rtcUnix = rtcTime.unixtime();
  uint32_t diff = (ntpUnix > rtcUnix) ? (ntpUnix - rtcUnix) : (rtcUnix - ntpUnix);
  if (diff > 5) {
    rtc.adjust(ntpTime);
    Serial.println("DS1307 synced with NTP server: " + String(ntpServer));
  } else {
    Serial.println("RTC time is close to NTP, no update needed");
  }
}

String getCurrentDateTime() {
  if (!rtcInitialized || !rtc.isrunning()) {
    return "----/--/-- --:--:--";
  }
  DateTime now = rtc.now();
  char buffer[20];
  snprintf(buffer, sizeof(buffer), "%04d/%02d/%02d %02d:%02d:%02d",
           now.year(), now.month(), now.day(),
           now.hour(), now.minute(), now.second());
  return String(buffer);
}