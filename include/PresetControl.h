#ifndef PRESET_CONTROL_H
#define PRESET_CONTROL_H

#include <Arduino.h>

void initPresets();
String getLightingPresets();
String getSchedulePresets();
void saveLightingPreset(const String& name, const String& config);
void saveSchedulePreset(const String& name, const String& config);
bool loadLightingPreset(const String& name);
void deletePreset(const String& type, const String& name);

#endif
