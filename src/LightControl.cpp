/*
  LightControl.cpp
  Оновлено: Додано Wire.begin() і захист ініціалізації
*/

#include "LightControl.h"
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <math.h>

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);
int savedBrightness[5] = {0, 0, 0, 0, 0};
float gammaCoefficients[5] = {2.8, 2.8, 2.8, 2.8, 2.8};
int maxBrightness[5] = {255, 255, 255, 255, 255};
bool lightInitialized = false;
String currentMode = "manual";
unsigned long effectStartTime = 0;
int effectDuration = 0;
int effectChannels[5] = {0, 0, 0, 0, 0};

void initLight() {
  Wire.begin();
  Wire.beginTransmission(0x40);
  if (Wire.endTransmission() != 0) {
    Serial.println("Error: PCA9685 not found");
    lightInitialized = false;
    return;
  }
  pwm.begin();
  pwm.setPWMFreq(1000);
  lightInitialized = true;
  Serial.println("PCA9685 initialized");
  for (int i = 0; i < 5; i++) {
    setChannelBrightness(i, savedBrightness[i]);
  }
}

void handleLightEffects() {
  if (!lightInitialized) return;
  if (currentMode == "manual") return;
  unsigned long now = millis();
  if (now >= effectStartTime + (unsigned long)effectDuration) {
    for (int i = 0; i < 5; i++) {
      savedBrightness[i] = effectChannels[i];
    }
    currentMode = "manual";
    Serial.println("Effect completed");
    return;
  }
  float progress = (now - effectStartTime) / (float)effectDuration;
  for (int i = 0; i < 5; i++) {
    int startBright = (currentMode == "sunrise") ? 0 : savedBrightness[i];
    int targetBright = effectChannels[i];
    int currentBright = startBright + (int)((targetBright - startBright) * progress);
    setChannelBrightness(i, currentBright);
  }
}

void setChannelBrightness(int channel, int brightness) {
  if (!lightInitialized) return;
  if (channel < 0 || channel >= 5) return;
  brightness = constrain(brightness, 0, maxBrightness[channel]);
  float normalized = (float)brightness / (float)maxBrightness[channel];
  float gammaCorrected = pow(normalized, gammaCoefficients[channel]);
  int pwmValue = (int)(gammaCorrected * 4095.0f);
  pwm.setPWM(channel, 0, pwmValue);
  if (currentMode == "manual") {
    savedBrightness[channel] = brightness;
  }
}

int getChannelBrightness(int channel) {
  if (channel < 0 || channel >= 5) return 0;
  return savedBrightness[channel];
}

void startSunrise(int durationMinutes) {
  if (!lightInitialized) return;
  currentMode = "sunrise";
  effectStartTime = millis();
  effectDuration = durationMinutes * 60000;
  for (int i = 0; i < 5; i++) {
    effectChannels[i] = (int)(0.8 * maxBrightness[i]);
  }
  Serial.println("Sunrise started");
}

void startSunset(int durationMinutes) {
  if (!lightInitialized) return;
  currentMode = "sunset";
  effectStartTime = millis();
  effectDuration = durationMinutes * 60000;
  for (int i = 0; i < 5; i++) {
    effectChannels[i] = (int)(0.1 * maxBrightness[i]);
  }
  Serial.println("Sunset started");
}

void setNightMode() {
  if (!lightInitialized) return;
  currentMode = "night";
  for (int i = 0; i < 5; i++) {
    int nightBrightness = (int)(0.04 * maxBrightness[i]);
    setChannelBrightness(i, nightBrightness);
    savedBrightness[i] = nightBrightness;
  }
  Serial.println("Night mode");
}

void setPWMFrequency(float freq) {
  if (!lightInitialized) return;
  freq = constrain(freq, 24, 1526);
  pwm.setPWMFreq(freq);
  Serial.println("PWM frequency set to " + String(freq) + " Hz");
}

void setChannelParams(int channel, float gamma, int maxBright) {
  if (channel < 0 || channel >= 5) return;
  gammaCoefficients[channel] = constrain(gamma, 0.5, 5.0);
  maxBrightness[channel] = constrain(maxBright, 0, 255);
  Serial.println("Channel " + String(channel) + ": gamma=" + String(gammaCoefficients[channel]) + ", maxBrightness=" + String(maxBrightness[channel]));
  setChannelBrightness(channel, savedBrightness[channel]);
}