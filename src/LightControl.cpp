#include "LightControl.h"
#include <Arduino.h>
#include <math.h>

// 5 прямых PWM-выходов ESP32 (пустые [] не вырезаются при копировании)
static const uint8_t PWM_PINS[] = {16, 17, 18, 19, 23};

static const uint8_t PWM_RESOLUTION = 13;        // 13 бит = 0..8191 (плавный старт с 0%)
static const uint32_t PWM_MAX_DUTY  = 8191;
static float currentPwmFreq         = 5000.0f;   // 5 кГц — бесшумная работа драйверов

int savedBrightness[]     = {0, 0, 0, 0, 0};
int currentBrightness[]   = {0, 0, 0, 0, 0};
float gammaCoefficients[] = {2.8f, 2.8f, 2.8f, 2.8f, 2.8f};
int maxBrightness[]       = {255, 255, 255, 255, 255};

bool lightInitialized         = false;
String currentMode            = "manual";
unsigned long effectStartTime = 0;
unsigned long effectDuration  = 0;
int effectChannels[]          = {0, 0, 0, 0, 0};

static void writeChannelHardware(int channel, int brightness) {
  if (!lightInitialized || channel < 0 || channel >= 5) return;

  brightness = constrain(brightness, 0, maxBrightness[channel]);
  currentBrightness[channel] = brightness;

  float normalized = (maxBrightness[channel] > 0)
                       ? ((float)brightness / (float)maxBrightness[channel])
                       : 0.0f;
  float gammaCorrected = powf(normalized, gammaCoefficients[channel]);
  uint32_t duty = (uint32_t)(gammaCorrected * (float)PWM_MAX_DUTY + 0.5f);

  ledcWrite(PWM_PINS[channel], duty);
}

void initLight() {
  for (int i = 0; i < 5; i++) {
    pinMode(PWM_PINS[i], OUTPUT);
    digitalWrite(PWM_PINS[i], LOW);
    ledcAttach(PWM_PINS[i], (uint32_t)currentPwmFreq, PWM_RESOLUTION);
    ledcWrite(PWM_PINS[i], 0);
  }

  lightInitialized = true;
  Serial.println("ESP32 LEDC 5-channel PWM initialized (GPIO 16, 17, 18, 19, 23)");

  for (int i = 0; i < 5; i++) {
    writeChannelHardware(i, savedBrightness[i]);
  }
}

void handleLightEffects() {
  if (!lightInitialized) return;
  if (currentMode != "sunrise" && currentMode != "sunset") return;

  unsigned long now = millis();
  unsigned long elapsed = now - effectStartTime;

  if (elapsed >= effectDuration || effectDuration == 0) {
    for (int i = 0; i < 5; i++) {
      savedBrightness[i] = effectChannels[i];
      writeChannelHardware(i, effectChannels[i]);
    }
    currentMode = "manual";
    Serial.println("Effect completed");
    return;
  }

  float progress = (float)elapsed / (float)effectDuration;
  for (int i = 0; i < 5; i++) {
    int startBright  = savedBrightness[i];
    int targetBright = effectChannels[i];
    int nextBright   = startBright + (int)((targetBright - startBright) * progress);
    writeChannelHardware(i, nextBright);
  }
}

void setChannelBrightness(int channel, int brightness) {
  if (!lightInitialized || channel < 0 || channel >= 5) return;

  if (currentMode == "sunrise" || currentMode == "sunset") {
    currentMode = "manual";
  }

  brightness = constrain(brightness, 0, maxBrightness[channel]);
  savedBrightness[channel] = brightness;
  writeChannelHardware(channel, brightness);
}

int getChannelBrightness(int channel) {
  if (channel < 0 || channel >= 5) return 0;
  return currentBrightness[channel];
}

void startSunrise(int durationMinutes) {
  if (!lightInitialized) return;
  durationMinutes = max(1, durationMinutes);

  for (int i = 0; i < 5; i++) {
    savedBrightness[i] = currentBrightness[i];
    effectChannels[i]  = (int)(0.8f * maxBrightness[i]);
  }

  currentMode     = "sunrise";
  effectStartTime = millis();
  effectDuration  = (unsigned long)durationMinutes * 60000UL;
  Serial.println("Sunrise started");
}

void startSunset(int durationMinutes) {
  if (!lightInitialized) return;
  durationMinutes = max(1, durationMinutes);

  for (int i = 0; i < 5; i++) {
    savedBrightness[i] = currentBrightness[i];
    effectChannels[i]  = (int)(0.1f * maxBrightness[i]);
  }

  currentMode     = "sunset";
  effectStartTime = millis();
  effectDuration  = (unsigned long)durationMinutes * 60000UL;
  Serial.println("Sunset started");
}

void setNightMode() {
  if (!lightInitialized) return;
  currentMode = "night";
  for (int i = 0; i < 5; i++) {
    int nightBrightness = (int)(0.04f * maxBrightness[i]);
    savedBrightness[i] = nightBrightness;
    writeChannelHardware(i, nightBrightness);
  }
  Serial.println("Night mode");
}

void setPWMFrequency(float freq) {
  if (!lightInitialized) return;
  currentPwmFreq = constrain(freq, 100.0f, 20000.0f);
  for (int i = 0; i < 5; i++) {
    ledcChangeFrequency(PWM_PINS[i], (uint32_t)currentPwmFreq, PWM_RESOLUTION);
    writeChannelHardware(i, currentBrightness[i]);
  }
  Serial.printf("LEDC PWM frequency set to %.0f Hz\n", currentPwmFreq);
}

void setChannelParams(int channel, float gamma, int maxBright) {
  if (channel < 0 || channel >= 5) return;
  gammaCoefficients[channel] = constrain(gamma, 0.5f, 5.0f);
  maxBrightness[channel]     = constrain(maxBright, 0, 255);
  savedBrightness[channel]   = constrain(savedBrightness[channel], 0, maxBrightness[channel]);
  writeChannelHardware(channel, currentBrightness[channel]);
}
