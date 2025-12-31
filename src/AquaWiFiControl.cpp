#include "AquaWiFiControl.h"
#include <WiFi.h>
#include <ArduinoJson.h>

enum WiFiState { TRY_SAVED, TRY_AQUA, AP_MODE, CONNECTED, TRY_PENDING };

static WiFiState wifiState = TRY_SAVED;
static unsigned long apStartTime = 0;
const unsigned long AP_TIMEOUT = 120000;
static unsigned long lastActionTime = 0;
const unsigned long CHECK_INTERVAL = 5000;
const unsigned long RECONNECT_DELAY = 15000;
static String pendingSSID = "";
static String pendingPassword = "";
static String cachedNetworks = "[]";
static TaskHandle_t scanTaskHandle = nullptr;

void tryConnectSaved_nonblocking();
void tryConnectAqua_nonblocking();
void startAP();
void scanNetworksTask(void* pvParameters);

void initWiFi() {
  WiFi.persistent(true);
  WiFi.mode(WIFI_STA);
  wifiState = TRY_SAVED;
  lastActionTime = 0;
  Serial.println("WiFi ініціалізовано");
}

void requestWiFiConnect(const String& ssid, const String& password) {
  pendingSSID = ssid;
  pendingPassword = password;
  wifiState = TRY_PENDING;
  Serial.println("WiFi connect requested (non-blocking) for SSID: " + ssid);
  lastActionTime = 0;
}

void tryConnectSaved_nonblocking() {
  Serial.println("Try saved WiFi (non-blocking)");
  WiFi.mode(WIFI_STA);
  if (pendingSSID.length() > 0) {
    WiFi.begin(pendingSSID.c_str(), pendingPassword.c_str());
  } else {
    WiFi.begin();
  }
  lastActionTime = millis();
}

void tryConnectAqua_nonblocking() {
  Serial.println("Try AQUA:12345678 (non-blocking)");
  WiFi.mode(WIFI_STA);
  WiFi.begin("AQUA", "12345678");
  lastActionTime = millis();
}

void startAP() {
  Serial.println("Starting AP aquaconfig:11111111");
  WiFi.mode(WIFI_AP);
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
      lastActionTime = 0;
    }
    return;
  }

  if (wifiState == TRY_PENDING) {
    if (WiFi.status() == WL_CONNECTED) {
      wifiState = CONNECTED;
      Serial.println("Connected (pending) IP: " + WiFi.localIP().toString());
      pendingSSID = "";
      pendingPassword = "";
    } else if (millis() - lastActionTime > RECONNECT_DELAY) {
      tryConnectSaved_nonblocking();
    }
    return;
  }

  if (wifiState == TRY_SAVED) {
    if (millis() - lastActionTime > RECONNECT_DELAY) {
      tryConnectSaved_nonblocking();
    } else if (WiFi.status() == WL_CONNECTED) {
      wifiState = CONNECTED;
      Serial.println("Connected to saved WiFi, IP: " + WiFi.localIP().toString());
    } else if (millis() - lastActionTime > RECONNECT_DELAY * 2) {
      wifiState = TRY_AQUA;
      lastActionTime = 0;
    }
    return;
  }

  if (wifiState == TRY_AQUA) {
    if (millis() - lastActionTime > RECONNECT_DELAY) {
      tryConnectAqua_nonblocking();
    } else if (WiFi.status() == WL_CONNECTED) {
      wifiState = CONNECTED;
      Serial.println("Connected to AQUA, IP: " + WiFi.localIP().toString());
    } else if (millis() - lastActionTime > RECONNECT_DELAY * 4) {
      wifiState = AP_MODE;
      startAP();
    }
    return;
  }

  if (wifiState == AP_MODE) {
    if (WiFi.softAPgetStationNum() > 0) {
      apStartTime = 0;
    } else if (apStartTime > 0 && millis() - apStartTime > AP_TIMEOUT) {
      WiFi.softAPdisconnect(true);
      Serial.println("AP timeout, restarting WiFi process");
      wifiState = TRY_SAVED;
      lastActionTime = 0;
    }
  }
}

String getAvailableNetworks() {
  return cachedNetworks;
}

void forceScanNetworks(AsyncWebServerRequest *request) {
  if (scanTaskHandle != nullptr) {
    Serial.println("Scan already in progress, ignoring request");
    request->send(200, "application/json", cachedNetworks);
    return;
  }
  Serial.println("Starting WiFi scan task...");
  xTaskCreate(
      scanNetworksTask,
      "ScanNetworksTask",
      4096,
      request,
      1,
      &scanTaskHandle
  );
}

void scanNetworksTask(void* pvParameters) {
  AsyncWebServerRequest *request = (AsyncWebServerRequest *)pvParameters;
  Serial.println("Scanning WiFi networks...");
  int n = WiFi.scanNetworks();
  JsonDocument doc;
  JsonArray networks = doc.to<JsonArray>();
  if (n <= 0) {
    Serial.println("No WiFi networks found");
  } else {
    for (int i = 0; i < n; i++) {
      JsonObject network = networks.add<JsonObject>();
      network["ssid"] = WiFi.SSID(i);
      network["rssi"] = WiFi.RSSI(i);
    }
    Serial.println("Found " + String(n) + " networks");
  }
  String json;
  serializeJson(doc, json);
  cachedNetworks = json;
  WiFi.scanDelete();
  request->send(200, "application/json", json);
  scanTaskHandle = nullptr;
  vTaskDelete(nullptr);
}