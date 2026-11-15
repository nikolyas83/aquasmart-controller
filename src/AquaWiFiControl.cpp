#include "AquaWiFiControl.h"
#include <WiFi.h>
#include <ArduinoJson.h>

enum WiFiState { TRY_SAVED, TRY_AQUA, AP_MODE, CONNECTED, TRY_PENDING };

static WiFiState wifiState = TRY_SAVED;
static unsigned long apStartTime = 0;
const unsigned long AP_TIMEOUT = 120000;
static unsigned long lastActionTime = 0;
const unsigned long CHECK_INTERVAL = 2000;
const unsigned long RECONNECT_DELAY = 15000;

static int retryCount = 0;
static String pendingSSID = "";
static String pendingPassword = "";
static String cachedNetworks = "[]";
static volatile bool isScanning = false;

void tryConnectSaved_nonblocking();
void tryConnectAqua_nonblocking();
void startAP();
void scanNetworksTask(void* pvParameters);

void initWiFi() {
  WiFi.persistent(true);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  wifiState = TRY_SAVED;
  retryCount = 0;
  lastActionTime = 0;
  Serial.println("WiFi ініціалізовано");
}

void requestWiFiConnect(const String& ssid, const String& password) {
  pendingSSID = ssid;
  pendingPassword = password;
  wifiState = TRY_PENDING;
  retryCount = 0;
  lastActionTime = 0;
  Serial.println("WiFi connect requested for SSID: " + ssid);
}

void tryConnectSaved_nonblocking() {
  Serial.println("Try saved WiFi (attempt " + String(retryCount + 1) + ")");
  WiFi.mode(WIFI_STA);
  if (pendingSSID.length() > 0) {
    WiFi.begin(pendingSSID.c_str(), pendingPassword.c_str());
  } else {
    WiFi.begin();
  }
  lastActionTime = millis();
  retryCount++;
}

void tryConnectAqua_nonblocking() {
  Serial.println("Try AQUA:12345678 (attempt " + String(retryCount + 1) + ")");
  WiFi.mode(WIFI_STA);
  WiFi.begin("AQUA", "12345678");
  lastActionTime = millis();
  retryCount++;
}

void startAP() {
  Serial.println("Starting AP aquaconfig:11111111");
  // Используем WIFI_AP_STA, чтобы можно было сканировать сети, не разрывая AP
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("aquaconfig", "11111111");
  apStartTime = millis();
}

void handleWiFi() {
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck < CHECK_INTERVAL) return;
  lastCheck = millis();

  if (wifiState == CONNECTED) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected, switching to TRY_SAVED");
      wifiState = TRY_SAVED;
      retryCount = 0;
      lastActionTime = 0;
    }
    return;
  }

  // 1. Подключение к новой сети из веб-формы (макс. 2 попытки по 15 сек)
  if (wifiState == TRY_PENDING) {
    if (WiFi.status() == WL_CONNECTED) {
      wifiState = CONNECTED;
      retryCount = 0;
      Serial.println("Connected (pending) IP: " + WiFi.localIP().toString());
      pendingSSID = "";
      pendingPassword = "";
    } else if (lastActionTime == 0 || millis() - lastActionTime > RECONNECT_DELAY) {
      if (retryCount < 2) {
        tryConnectSaved_nonblocking();
      } else {
        Serial.println("Pending WiFi failed, falling back to AP_MODE");
        pendingSSID = "";
        pendingPassword = "";
        wifiState = AP_MODE;
        startAP();
      }
    }
    return;
  }

  // 2. Попытка подключиться к сохранённой во флеш сети (2 попытки по 15 сек)
  if (wifiState == TRY_SAVED) {
    if (WiFi.status() == WL_CONNECTED) {
      wifiState = CONNECTED;
      retryCount = 0;
      Serial.println("Connected to saved WiFi, IP: " + WiFi.localIP().toString());
    } else if (lastActionTime == 0 || millis() - lastActionTime > RECONNECT_DELAY) {
      if (retryCount < 2) {
        tryConnectSaved_nonblocking();
      } else {
        Serial.println("Saved WiFi unavailable, switching to TRY_AQUA");
        wifiState = TRY_AQUA;
        retryCount = 0;
        lastActionTime = 0;
      }
    }
    return;
  }

  // 3. Попытка подключиться к резервной сети AQUA (2 попытки по 15 сек)
  if (wifiState == TRY_AQUA) {
    if (WiFi.status() == WL_CONNECTED) {
      wifiState = CONNECTED;
      retryCount = 0;
      Serial.println("Connected to AQUA, IP: " + WiFi.localIP().toString());
    } else if (lastActionTime == 0 || millis() - lastActionTime > RECONNECT_DELAY) {
      if (retryCount < 2) {
        tryConnectAqua_nonblocking();
      } else {
        Serial.println("AQUA unavailable, switching to AP_MODE");
        wifiState = AP_MODE;
        retryCount = 0;
        startAP();
      }
    }
    return;
  }

  // 4. Режим точки доступа настройки (таймаут 2 минуты без клиентов)
  if (wifiState == AP_MODE) {
    if (WiFi.softAPgetStationNum() > 0) {
      apStartTime = 0;
    } else if (apStartTime > 0 && millis() - apStartTime > AP_TIMEOUT) {
      WiFi.softAPdisconnect(true);
      Serial.println("AP timeout, restarting WiFi process");
      wifiState = TRY_SAVED;
      retryCount = 0;
      lastActionTime = 0;
    }
  }
}

String getAvailableNetworks() {
  return cachedNetworks;
}

// Безопасное сканирование без передачи указателя request в фоновый поток
void forceScanNetworks(AsyncWebServerRequest *request) {
  if (!isScanning) {
    isScanning = true;
    Serial.println("Starting safe background WiFi scan...");
    xTaskCreate(scanNetworksTask, "ScanNetworksTask", 4096, nullptr, 1, nullptr);
  }
  // Сразу отдаём текущий кэш (фронтенд обновит список при следующем опросе)
  request->send(200, "application/json", cachedNetworks);
}

void scanNetworksTask(void* pvParameters) {
  int n = WiFi.scanNetworks();
  JsonDocument doc;
  JsonArray networks = doc.to<JsonArray>();

  if (n > 0) {
    for (int i = 0; i < n; i++) {
      JsonObject network = networks.add<JsonObject>();
      network["ssid"] = WiFi.SSID(i);
      network["rssi"] = WiFi.RSSI(i);
    }
    Serial.println("Scan complete, found " + String(n) + " networks");
  } else {
    Serial.println("Scan complete, no networks found");
  }

  String json;
  serializeJson(doc, json);
  cachedNetworks = json;
  WiFi.scanDelete();

  isScanning = false;
  vTaskDelete(nullptr);
}
