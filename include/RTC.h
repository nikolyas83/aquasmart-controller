#ifndef RTC_H
#define RTC_H

#include <Arduino.h>

void initRTC();
String getCurrentDateTime();
void setManualTime(int year, int month, int day, int hour, int minute, int second);
void setTimeZone(long offset, int dst);
void syncTimeWithNTP(const char* ntpServer);

#endif