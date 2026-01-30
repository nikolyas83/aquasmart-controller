/*
  RTC.cpp
  Оновлено: Ретельні перевірки, асинхронна NTP-синхронізація
*/

#include "RTC.h"
#include <Wire.h>
#include <RTClib.h>
#include <WiFi.h>
#include <time.h>
#include <Preferences.h>
#include <sys/time.h>

RTC_DS1307 rtc;
bool rtcInitialized = false;
static Preferences prefs;

void initRTC() {
  if (!rtc.begin()) {
    Serial.println("Error: DS1307 not found");
    rtcInitialized = false;
    return;
  }
  rtcInitialized = true;
  prefs.begin("aquasmart", false);
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
  // Save to Preferences
  prefs.putInt("time_year", year);
  prefs.putInt("time_month", month);
  prefs.putInt("time_day", day);
  prefs.putInt("time_hour", hour);
  prefs.putInt("time_minute", minute);
  prefs.putInt("time_second", second);
  // Also update system time so API endpoints using time() reflect the new value
  time_t t = rtc.now().unixtime();
  struct timeval tv;
  tv.tv_sec = t;
  tv.tv_usec = 0;
  settimeofday(&tv, NULL);
  Serial.println("Manual time set: " + getCurrentDateTime());
}

void setTimeZone(long offset, int dst) {
  if (!rtcInitialized) {
    Serial.println("Error: RTC not initialized");
    return;
  }
  // offset - UTC offset в секундах, dst - DST offset в секундах
  configTime(offset, dst, "pool.ntp.org");
  Serial.println("Time zone set: offset=" + String(offset) + "s, DST=" + String(dst) + "s");
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
  
  // Timeout перевірка (максимум 10 сек)
  unsigned long startTime = millis();
  const unsigned long MAX_WAIT = 10000;
  struct tm timeinfo = {0};
  
  while (!getLocalTime(&timeinfo) && (millis() - startTime) < MAX_WAIT) {
    vTaskDelay(pdMS_TO_TICKS(500));
  }
  
  if (timeinfo.tm_year == 0) {  // getLocalTime не встановив дату
    Serial.println("Error: Failed to get NTP time (timeout)");
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

void restoreTimeFromPreferences() {
  if (!rtcInitialized) {
    Serial.println("Error: RTC not initialized");
    return;
  }
  if (prefs.isKey("time_year")) {
    int year = prefs.getInt("time_year", 2026);
    int month = prefs.getInt("time_month", 1);
    int day = prefs.getInt("time_day", 1);
    int hour = prefs.getInt("time_hour", 0);
    int minute = prefs.getInt("time_minute", 0);
    int second = prefs.getInt("time_second", 0);
    rtc.adjust(DateTime(year, month, day, hour, minute, second));
    Serial.println("Restored time from Preferences: " + getCurrentDateTime());
    // Sync system time from RTC
    time_t t = rtc.now().unixtime();
    struct timeval tv;
    tv.tv_sec = t;
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);
  }
}