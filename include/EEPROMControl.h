#ifndef EEPROM_CONTROL_H
#define EEPROM_CONTROL_H

#include <Arduino.h>

void initEEPROM();
void saveToEEPROM();
void loadFromEEPROM();
void saveMQTTSettings(const String& server, int port, const String& user, const String& pass);

#endif
