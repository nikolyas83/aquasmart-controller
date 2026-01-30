#ifndef SCHEDULE_CONTROL_H
#define SCHEDULE_CONTROL_H

#include <Arduino.h>

void initSchedule();
void handleSchedule();
String getDaySchedule(const String& date);
bool setDaySchedule(const String& date, const String& scheduleJson);
String getYearSchedule(int year);
bool importSchedule(const String& json);
String exportSchedule();
String getCurrentDaySchedule();
void addScheduleEvent(const String& event);
void removeScheduleEvent(int id);
String getSchedule();

#endif
