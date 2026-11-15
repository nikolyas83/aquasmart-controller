#include "LightControl.h"
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <math.h>

// Используем глобальный мьютекс шины I2C из main.cpp
extern SemaphoreHandle_t i2cMutex;

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);
int savedBrightness[5] = {0, 0, 0, 0, 0};    // Опорная яркость (старт для эффектов)
int currentBrightness[5] = {0, 0, 0, 0, 0};  // Фактическая текущая яркость для API/UI
float gammaCoefficients[5] = {2.8, 2.8, 2.8, 2.8, 2.8};
int maxBrightness[5] = {255, 255, 255, 255, 255};

bool lightInitialized = false;
String currentMode = "manual";
unsigned long effectStartTime = 0;
unsigned long effectDuration = 0;
int effectChannels[5] = {0, 0, 0, 0, 0};

static bool lockI2C(TickType_t timeout = pdMS_TO_TICKS(100)) {
  if (!i2cMutex) return true; // До старта FreeRTOS работаем напрямую
  return xSemaphoreTake(i2cMutex, timeout) == pdTRUE;
}

static void unlockI2C() {
  if (i2cMutex) xSemaphoreGive(i2cMutex);
}

// Внутренняя функция записи в PCA9685 без повторного захвата мьютекса
static void writeChannelHardware(int channel, int brightness) {
  if (!lightInitialized || channel < 0 || channel >= 5) return;

  brightness = constrain(brightness, 0, maxBrightness[channel]);
  currentBrightness[channel] = brightness;

  float normalized = (maxBrightness[channel] > 0)
                       ? ((float)brightness / (float)maxBrightness[channel])
                       : 0.0f;
  float gammaCorrected = powf(normalized, gammaCoefficients[channel]);
  int pwmValue = (int)(gammaCorrected * 4095.0f + 0.5f);

  if (lockI2C()) {
    pwm.setPWM(channel, 0, pwmValue);
    unlockI2C();
  }
}

void initLight() {
  if (!lockI2C(pdMS_TO_TICKS(500))) return;

  Wire.beginTransmission(0x40);
  if (Wire.endTransmission() != 0) {
    unlockI2C();
    Serial.println("Error: PCA9685 not found");
    lightInitialized = false;
    return;
  }

  pwm.begin();
  pwm.setPWMFreq(1000);
  lightInitialized = true;
  unlockI2C();

  Serial.println("PCA9685 initialized");
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
    // Фиксируем точное финальное значение на драйвере перед переходом в manual
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
    int startBright = savedBrightness[i];
    int targetBright = effectChannels[i];
    int nextBright = startBright + (int)((targetBright - startBright) * progress);
    writeChannelHardware(i, nextBright);
  }
}

void setChannelBrightness(int channel, int brightness) {
  if (!lightInitialized || channel < 0 || channel >= 5) return;

  // Ручное изменение прерывает автоматический рассвет/закат
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

  // Запоминаем текущую яркость как стартовую (плавный выход из ночи без скачка в 0)
  for (int i = 0; i < 5; i++) {
    savedBrightness[i] = currentBrightness[i];
    effectChannels[i] = (int)(0.8f * maxBrightness[i]);
  }

  currentMode = "sunrise";
  effectStartTime = millis();
  effectDuration = (unsigned long)durationMinutes * 60000UL;
  Serial.println("Sunrise started");
}

void startSunset(int durationMinutes) {
  if (!lightInitialized) return;
  durationMinutes = max(1, durationMinutes);

  for (int i = 0; i < 5; i++) {
    savedBrightness[i] = currentBrightness[i];
    effectChannels[i] = (int)(0.1f * maxBrightness[i]);
  }

  currentMode = "sunset";
  effectStartTime = millis();
  effectDuration = (unsigned long)durationMinutes * 60000UL;
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
  freq = constrain(freq, 24.0f, 1526.0f);
  if (lockI2C()) {
    pwm.setPWMFreq(freq);
    unlockI2C();
    Serial.println("PWM frequency set to " + String(freq) + " Hz");
  }
}

void setChannelParams(int channel, float gamma, int maxBright) {
  if (channel < 0 || channel >= 5) return;
  gammaCoefficients[channel] = constrain(gamma, 0.5f, 5.0f);
  maxBrightness[channel] = constrain(maxBright, 0, 255);
  savedBrightness[channel] = constrain(savedBrightness[channel], 0, maxBrightness[channel]);
  writeChannelHardware(channel, currentBrightness[channel]);
}
