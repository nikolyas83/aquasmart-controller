#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "AquaWiFiControl.h"
#include "LightControl.h"
#include "TemperatureControl.h"
#include "EEPROMControl.h"
#include "RTC.h"
#include "MQTTControl.h"
#include "ScheduleControl.h"
#include "PresetControl.h"
#include <ElegantOTA.h>

extern void saveMQTTSettings(const String& server, int port, const String& user, const String& pass);

AsyncWebServer server(80);
String http_username = "admin";
String http_password = "admin123";

const char filemanager_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>AquaSmart — Файловий менеджер</title>
    <meta charset="UTF-8">
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #f8f9fa; }
        table { width: 100%; border-collapse: collapse; background: #fff; margin-top: 10px; }
        th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }
        th { background-color: #e9ecef; }
        button, input[type=submit] { padding: 6px 12px; cursor: pointer; margin: 2px; }
        .box { background: #fff; padding: 12px; border: 1px solid #ddd; margin: 10px 0; }
    </style>
</head>
<body>
    <h2>AquaSmart — Сервісна панель та файли</h2>
    <div class="box">
        <button onclick="openWiFiModal()">Налаштування Wi-Fi</button>
        <button onclick="openPWMModal()">Тест PWM каналу</button>
        <button onclick="openTimeModal()">Встановити час</button>
        <button onclick="openMQTTModal()">Налаштування MQTT</button>
        <button onclick="location.href='/update'">OTA Оновлення прошивки</button>
    </div>
    <div class="box">
        <form id="mkdirForm" style="display:inline-block; margin-right:20px;">
            <input type="text" id="folderName" placeholder="/нова_папка" required>
            <input type="submit" value="Створити папку">
        </form>
        <form id="uploadForm" enctype="multipart/form-data" style="display:inline-block;">
            <input type="file" name="file" id="fileInput" required>
            <input type="submit" value="Завантажити файл у /">
        </form>
    </div>
    <table>
        <thead><tr><th>Назва</th><th>Розмір (байти)</th><th>Дії</th></tr></thead>
        <tbody id="fileList"></tbody>
    </table>
    <script>
        function openWiFiModal() {
            let ssid = prompt("Введіть SSID:");
            let pass = prompt("Введіть пароль:");
            if (ssid !== null && pass !== null) {
                fetch('/set-wifi', { method: 'POST', body: new URLSearchParams({ ssid, password: pass }) })
                    .then(r => alert(r.ok ? "Підключення запущено" : "Помилка"));
            }
        }
        function openPWMModal() {
            let channel = prompt("Канал (0-4):");
            let value = prompt("Яскравість (0-255):");
            if (channel !== null && value !== null) {
                fetch('/set-brightness', { method: 'POST', body: new URLSearchParams({ channel, value }) })
                    .then(r => alert(r.ok ? "Встановлено" : "Помилка"));
            }
        }
        function openTimeModal() {
            let time = prompt("Час (YYYY-MM-DD HH:MM:SS):", "2026-11-15 12:00:00");
            if (time) {
                let [d, t] = time.split(' ');
                let [year, month, day] = d.split('-');
                let [hour, minute, second] = t.split(':');
                fetch('/set-time', { method: 'POST', body: new URLSearchParams({ year, month, day, hour, minute, second }) })
                    .then(r => alert(r.ok ? "Час оновлено" : "Помилка"));
            }
        }
        function openMQTTModal() {
            let server = prompt("MQTT сервер (IP або домен):");
            let port = prompt("MQTT порт:", "1883");
            let user = prompt("MQTT користувач (необов'язково):", "");
            let pass = prompt("MQTT пароль (необов'язково):", "");
            if (server !== null) {
                fetch('/set-mqtt', { method: 'POST', body: new URLSearchParams({ server, port, user, pass }) })
                    .then(r => alert(r.ok ? "MQTT збережено у пам'ять" : "Помилка"));
            }
        }
        function loadFiles(path = '/') {
            fetch('/list?path=' + encodeURIComponent(path))
                .then(r => r.json())
                .then(data => {
                    let tbody = document.getElementById('fileList');
                    tbody.innerHTML = '';
                    data.forEach(item => {
                        let tr = document.createElement('tr');
                        tr.innerHTML = `<td>${item.name}</td><td>${item.isDir ? '[Папка]' : item.size}</td><td><button onclick="deleteFile('${item.path}')">Видалити</button></td>`;
                        tbody.appendChild(tr);
                    });
                });
        }
        function deleteFile(path) {
            if (!confirm("Видалити " + path + "?")) return;
            fetch('/delete?path=' + encodeURIComponent(path), { method: 'DELETE' }).then(r => r.ok && loadFiles());
        }
        document.getElementById('mkdirForm').addEventListener('submit', e => {
            e.preventDefault();
            fetch('/mkdir', { method: 'POST', body: new URLSearchParams({ path: document.getElementById('folderName').value }) })
                .then(r => r.ok && loadFiles());
        });
        document.getElementById('uploadForm').addEventListener('submit', e => {
            e.preventDefault();
            fetch('/upload', { method: 'POST', body: new FormData(e.target) }).then(r => r.ok && loadFiles());
        });
        loadFiles();
    </script>
</body>
</html>
)rawliteral";

static bool checkAuth(AsyncWebServerRequest *request) {
  if (!request->authenticate(http_username.c_str(), http_password.c_str())) {
    request->requestAuthentication();
    return false;
  }
  return true;
}

static String* appendRequestBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index) {
  if (index == 0) {
    if (request->_tempObject) {
      delete static_cast<String*>(request->_tempObject);
    }
    request->_tempObject = new String();
  }
  String* body = static_cast<String*>(request->_tempObject);
  if (body && len > 0) {
    body->concat(reinterpret_cast<const char*>(data), len);
  }
  return body;
}

void initWebServer() {
  if (!LittleFS.begin(true)) {
    Serial.println("Failed to mount LittleFS");
    return;
  }

  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;
    JsonDocument doc;
    doc["temp"] = getTemperature();
    doc["datetime"] = getCurrentDateTime();
    doc["ssid"] = WiFi.SSID();
    doc["rssi"] = WiFi.RSSI();
    doc["ip"] = WiFi.localIP().toString();
    doc["heap"] = ESP.getFreeHeap();

    JsonArray ch = doc["channels"].to<JsonArray>();
    for (int i = 0; i < 5; i++) {
      ch.add(getChannelBrightness(i));
    }
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  });
  server.on("/get-wifi-networks", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;
    forceScanNetworks(request);
  });

  server.on("/set-wifi", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;
    if (request->hasParam("ssid", true) && request->hasParam("password", true)) {
      requestWiFiConnect(request->getParam("ssid", true)->value(), request->getParam("password", true)->value());
      request->send(200, "text/plain", "OK");
    } else {
      request->send(400, "text/plain", "Missing ssid or password");
    }
  });

  server.on("/set-brightness", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;
    if (request->hasParam("channel", true) && request->hasParam("value", true)) {
      int ch = request->getParam("channel", true)->value().toInt();
      int val = request->getParam("value", true)->value().toInt();
      if (ch >= 0 && ch <= 4 && val >= 0 && val <= 255) {
        setChannelBrightness(ch, val);
        request->send(200, "text/plain", "OK");
        return;
      }
    }
    request->send(400, "text/plain", "Invalid parameters");
  });

  server.on("/set-time", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;
    if (request->hasParam("year", true) && request->hasParam("month", true) &&
        request->hasParam("day", true) && request->hasParam("hour", true) &&
        request->hasParam("minute", true) && request->hasParam("second", true)) {
      setManualTime(
        request->getParam("year", true)->value().toInt(),
        request->getParam("month", true)->value().toInt(),
        request->getParam("day", true)->value().toInt(),
        request->getParam("hour", true)->value().toInt(),
        request->getParam("minute", true)->value().toInt(),
        request->getParam("second", true)->value().toInt()
      );
      request->send(200, "text/plain", "OK");
    } else {
      request->send(400, "text/plain", "Missing time parameters");
    }
  });

  server.on("/set-mqtt", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;
    if (request->hasParam("server", true) && request->hasParam("port", true)) {
      String srv = request->getParam("server", true)->value();
      int port = request->getParam("port", true)->value().toInt();
      String usr = request->hasParam("user", true) ? request->getParam("user", true)->value() : "";
      String pwd = request->hasParam("pass", true) ? request->getParam("pass", true)->value() : "";
      saveMQTTSettings(srv, port, usr, pwd);
      request->send(200, "text/plain", "OK");
    } else {
      request->send(400, "text/plain", "Missing MQTT parameters");
    }
  });

  server.on("/api/presets/lighting", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;
    request->send(200, "application/json", getLightingPresets());
  });

  server.on("/api/presets/schedule", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;
    request->send(200, "application/json", getSchedulePresets());
  });

  server.on("/api/presets/lighting/save", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;
    if (!request->hasParam("name", true)) return request->send(400, "text/plain", "Missing name");
    String cfg = request->hasParam("config", true) ? request->getParam("config", true)->value() : "";
    saveLightingPreset(request->getParam("name", true)->value(), cfg);
    request->send(200, "text/plain", "OK");
  });

  server.on("/api/presets/schedule/save", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;
    if (!request->hasParam("name", true)) return request->send(400, "text/plain", "Missing name");
    String cfg = request->hasParam("config", true) ? request->getParam("config", true)->value() : "";
    saveSchedulePreset(request->getParam("name", true)->value(), cfg);
    request->send(200, "text/plain", "OK");
  });

  server.on("/api/presets/lighting/load", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;
    if (!request->hasParam("name", true)) return request->send(400, "text/plain", "Missing name");
    bool ok = loadLightingPreset(request->getParam("name", true)->value());
    request->send(ok ? 200 : 404, "text/plain", ok ? "OK" : "Preset not found");
  });

  server.on("/api/presets/delete", HTTP_DELETE, [](AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;
    if (!request->hasParam("type") || !request->hasParam("name")) return request->send(400, "text/plain", "Missing params");
    deletePreset(request->getParam("type")->value(), request->getParam("name")->value());
    request->send(200, "text/plain", "OK");
  });
  server.on("^\\/api\\/schedule\\/day\\/([0-9]{4}-[0-9]{2}-[0-9]{2})$", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;
    request->send(200, "application/json", getDaySchedule(request->pathArg(0)));
  });

  server.on("^\\/api\\/schedule\\/day\\/([0-9]{4}-[0-9]{2}-[0-9]{2})$", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;
  }, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    String* body = appendRequestBody(request, data, len, index);
    if (index + len == total && body) {
      bool ok = setDaySchedule(request->pathArg(0), *body);
      delete body;
      request->_tempObject = nullptr;
      request->send(ok ? 200 : 400, "text/plain", ok ? "OK" : "Failed to set schedule");
    }
  });

  server.on("^\\/api\\/schedule\\/year\\/([0-9]{4})$", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;
    request->send(200, "application/json", getYearSchedule(request->pathArg(0).toInt()));
  });

  server.on("/api/schedule/import", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;
  }, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    String* body = appendRequestBody(request, data, len, index);
    if (index + len == total && body) {
      bool ok = importSchedule(*body);
      delete body;
      request->_tempObject = nullptr;
      request->send(ok ? 200 : 400, "text/plain", ok ? "Schedule imported" : "Invalid JSON");
    }
  });

  server.on("/api/schedule/export", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;
    request->send(200, "application/json", exportSchedule());
  });

  server.on("/list", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;
    String path = request->hasParam("path") ? request->getParam("path")->value() : "/";
    File root = LittleFS.open(path);
    if (!root || !root.isDirectory()) {
      if (root) root.close();
      return request->send(400, "text/plain", "Invalid path");
    }

    JsonDocument doc;
    JsonArray items = doc.to<JsonArray>();
    File file = root.openNextFile();
    while (file) {
      JsonObject item = items.add<JsonObject>();
      item["name"] = String(file.name());
      item["size"] = file.size();
      item["isDir"] = file.isDirectory();
      item["path"] = String(file.path());
      file.close();
      file = root.openNextFile();
    }
    root.close();

    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  });

  server.on("/mkdir", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;
    if (!request->hasParam("path", true)) return request->send(400, "text/plain", "Missing path");
    String path = request->getParam("path", true)->value();
    if (!path.startsWith("/")) path = "/" + path;
    request->send(LittleFS.mkdir(path) ? 200 : 400, "text/plain", "OK");
  });

  server.on("/delete", HTTP_DELETE, [](AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;
    if (!request->hasParam("path")) return request->send(400, "text/plain", "Missing path");
    String path = request->getParam("path")->value();
    bool ok = LittleFS.remove(path) || LittleFS.rmdir(path);
    request->send(ok ? 200 : 400, "text/plain", ok ? "OK" : "Failed");
  });

  server.on(
    "/upload", HTTP_POST,
    [](AsyncWebServerRequest *request) {
      if (!checkAuth(request)) return;
      request->send(200, "text/plain", "OK");
    },
    [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
      if (!request->authenticate(http_username.c_str(), http_password.c_str())) return;

      if (index == 0) {
        int slash = filename.lastIndexOf('/');
        if (slash >= 0) filename = filename.substring(slash + 1);
        String path = "/" + filename;
        File* f = new File(LittleFS.open(path, "w"));
        if (!f || !(*f)) {
          delete f;
          return;
        }
        request->_tempObject = f;
      }

      File* f = static_cast<File*>(request->_tempObject);
      if (f && *f && len > 0) {
        f->write(data, len);
      }

      if (final && f) {
        f->close();
        delete f;
        request->_tempObject = nullptr;
      }
    }
  );

  server.on("/files", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;
    request->send(200, "text/html", filemanager_html);
  });

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;
    if (LittleFS.exists("/index.html")) {
      request->send(LittleFS, "/index.html", "text/html");
    } else {
      request->send(200, "text/html", filemanager_html);
    }
  });

  server.serveStatic("/", LittleFS, "/").setAuthentication(http_username.c_str(), http_password.c_str());

  ElegantOTA.begin(&server, http_username.c_str(), http_password.c_str());
  server.begin();
  Serial.println("Web server started");
}
