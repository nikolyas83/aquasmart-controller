#include <Arduino.h>
#include <WiFi.h>
#include <ctime>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "AquaWiFiControl.h"
#include "LightControl.h"
#include "TemperatureControl.h"
#include "EEPROMControl.h"
#include "RTC.h"
#include "MQTTControl.h"
#include "ScheduleControl.h"
#include "PresetControl.h"

AsyncWebServer server(80);
String http_username = "admin";
String http_password = "admin123";

// OTA Update状態
static bool otaInProgress = false;
static int otaProgress = 0;
static SemaphoreHandle_t otaMutex = nullptr;

const char* filemanager_html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Файловий менеджер</title>
    <meta charset="UTF-8">
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        table { width: 100%; border-collapse: collapse; }
        th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }
        th { background-color: #f2f2f2; }
        .mkdir-form, .upload-form { margin: 10px 0; }
        .modal { display: none; position: fixed; top: 0; left: 0; width: 100%; height: 100%; background: rgba(0,0,0,0.5); }
        .modal-content { background: white; margin: 15% auto; padding: 20px; width: 70%; }
    </style>
</head>
<body>
    <h2>Файловий менеджер</h2>
    <button onclick="openWiFiModal()">Налаштування Wi-Fi</button>
    <button onclick="openPWMModal()">Налаштування PWM</button>
    <button onclick="openTimeModal()">Налаштування часу</button>
    <button onclick="openMQTTModal()">Налаштування MQTT</button>
    <button onclick="openScheduleModal()">Налаштування розкладу</button>
    <div class="breadcrumbs" id="breadcrumbs"></div>
    <div class="mkdir-form">
        <form id="mkdirForm">
            <input type="text" id="folderName" placeholder="Назва папки" required>
            <input type="submit" value="Створити папку">
        </form>
    </div>
    <div class="upload-form">
        <form id="uploadForm" enctype="multipart/form-data">
            <input type="file" name="file" id="fileInput">
            <input type="submit" value="Завантажити">
        </form>
    </div>
    <table>
        <thead>
            <tr>
                <th>Назва</th>
                <th>Розмір (байти)</th>
                <th>Дії</th>
            </tr>
        </thead>
        <tbody id="fileList"></tbody>
    </table>
    <div id="scheduleModal" class="modal">
        <div class="modal-content">
            <h3>Налаштування розкладу</h3>
            <form id="scheduleForm">
                <label>Тип:</label>
                <select id="scheduleType">
                    <option value="hourly">Погодинно</option>
                    <option value="daily">Щоденно</option>
                    <option value="weekly">Щотижня</option>
                    <option value="monthly">Щомісяця</option>
                    <option value="yearly">Щорічно</option>
                    <option value="once">Одноразово</option>
                </select><br>
                <label>Час (HH:MM):</label>
                <input type="text" id="scheduleTime" placeholder="08:00" required><br>
                <label id="dayLabel" style="display:none;">День тижня:</label>
                <select id="scheduleDay" style="display:none;">
                    <option value="Monday">Понеділок</option>
                    <option value="Tuesday">Вівторок</option>
                    <option value="Wednesday">Середа</option>
                    <option value="Thursday">Четвер</option>
                    <option value="Friday">П’ятниця</option>
                    <option value="Saturday">Субота</option>
                    <option value="Sunday">Неділя</option>
                </select><br>
                <label id="dateLabel" style="display:none;">Дата (YYYY-MM-DD):</label>
                <input type="text" id="scheduleDate" style="display:none;" placeholder="2025-12-25"><br>
                <label>Дія:</label>
                <select id="actionType">
                    <option value="effect">Ефект</option>
                    <option value="brightness">Яскравість</option>
                </select><br>
                <label id="effectLabel">Ефект:</label>
                <select id="actionValueEffect">
                    <option value="sunrise">Схід сонця</option>
                    <option value="sunset">Захід сонця</option>
                    <option value="night">Нічний режим</option>
                </select>
                <label id="durationLabel">Тривалість (хв):</label>
                <input type="number" id="actionDuration" value="10"><br>
                <label id="channelLabel" style="display:none;">Канал (0-4):</label>
                <input type="number" id="actionChannel" style="display:none;" min="0" max="4"><br>
                <label id="brightnessLabel" style="display:none;">Яскравість (0-255):</label>
                <input type="number" id="actionBrightness" style="display:none;" min="0" max="255"><br>
                <input type="submit" value="Додати подію">
                <button type="button" onclick="closeScheduleModal()">Закрити</button>
            </form>
            <h3>Поточний розклад</h3>
            <table id="scheduleTable">
                <thead>
                    <tr>
                        <th>ID</th>
                        <th>Тип</th>
                        <th>Час</th>
                        <th>Дія</th>
                        <th>Дії</th>
                    </tr>
                </thead>
                <tbody id="scheduleList"></tbody>
            </table>
        </div>
    </div>
    <script>
        function openWiFiModal() {
            let ssid = prompt("Введіть SSID:");
            let pass = prompt("Введіть пароль:");
            if (ssid && pass) {
                fetch('/set-wifi', { method: 'POST', body: new URLSearchParams({ ssid, password: pass }) })
                    .then(response => alert(response.ok ? "WiFi налаштування відправлено" : "Помилка"));
            }
        }
        function openPWMModal() {
            let channel = prompt("Канал (0-4):");
            let brightness = prompt("Яскравість (0-255):");
            if (channel && brightness) {
                fetch('/set-brightness', { method: 'POST', body: new URLSearchParams({ channel, value: brightness }) })
                    .then(response => alert(response.ok ? "Яскравість встановлено" : "Помилка"));
            }
        }
        function openTimeModal() {
            let time = prompt("Введіть час (YYYY-MM-DD HH:MM:SS):");
            if (time) {
                let [date, t] = time.split(' ');
                let [year, month, day] = date.split('-');
                let [hour, minute, second] = t.split(':');
                fetch('/set-time', { method: 'POST', body: new URLSearchParams({ year, month, day, hour, minute, second }) })
                    .then(response => alert(response.ok ? "Час встановлено" : "Помилка"));
            }
        }
        function openMQTTModal() {
            let server = prompt("MQTT сервер:");
            let port = prompt("MQTT порт:");
            let user = prompt("MQTT користувач:");
            let pass = prompt("MQTT пароль:");
            if (server && port && user && pass) {
                fetch('/set-mqtt', { method: 'POST', body: new URLSearchParams({ server, port, user, pass }) })
                    .then(response => alert(response.ok ? "MQTT налаштування збережено" : "Помилка"));
            }
        }
        function openScheduleModal() {
            document.getElementById('scheduleModal').style.display = 'block';
            loadSchedule();
        }
        function closeScheduleModal() {
            document.getElementById('scheduleModal').style.display = 'none';
        }
        function loadSchedule() {
            fetch('/get-schedule')
                .then(response => response.json())
                .then(data => {
                    let tbody = document.getElementById('scheduleList');
                    tbody.innerHTML = '';
                    data.forEach(item => {
                        let tr = document.createElement('tr');
                        let actionDesc = item.action.type == 'effect' ? item.action.value : `Канал ${item.action.channel}: ${item.action.value}`;
                        tr.innerHTML = `<td>${item.id}</td><td>${item.type}</td><td>${item.time}</td><td>${actionDesc}</td><td><button onclick="removeSchedule(${item.id})">Видалити</button></td>`;
                        tbody.appendChild(tr);
                    });
                });
        }
        function removeSchedule(id) {
            fetch('/remove-schedule', { method: 'POST', body: new URLSearchParams({ id }) })
                .then(response => response.ok && loadSchedule());
        }
        document.getElementById('scheduleType').addEventListener('change', function() {
            let type = this.value;
            document.getElementById('dayLabel').style.display = type == 'weekly' ? 'block' : 'none';
            document.getElementById('scheduleDay').style.display = type == 'weekly' ? 'block' : 'none';
            document.getElementById('dateLabel').style.display = (type == 'monthly' || type == 'yearly' || type == 'once') ? 'block' : 'none';
            document.getElementById('scheduleDate').style.display = (type == 'monthly' || type == 'yearly' || type == 'once') ? 'block' : 'none';
        });
        document.getElementById('actionType').addEventListener('change', function() {
            let type = this.value;
            document.getElementById('effectLabel').style.display = type == 'effect' ? 'block' : 'none';
            document.getElementById('actionValueEffect').style.display = type == 'effect' ? 'block' : 'none';
            document.getElementById('durationLabel').style.display = type == 'effect' ? 'block' : 'none';
            document.getElementById('actionDuration').style.display = type == 'effect' ? 'block' : 'none';
            document.getElementById('channelLabel').style.display = type == 'brightness' ? 'block' : 'none';
            document.getElementById('actionChannel').style.display = type == 'brightness' ? 'block' : 'none';
            document.getElementById('brightnessLabel').style.display = type == 'brightness' ? 'block' : 'none';
            document.getElementById('actionBrightness').style.display = type == 'brightness' ? 'block' : 'none';
        });
        document.getElementById('scheduleForm').addEventListener('submit', function(e) {
            e.preventDefault();
            let event = {
                type: document.getElementById('scheduleType').value,
                time: document.getElementById('scheduleTime').value,
                action: {
                    type: document.getElementById('actionType').value
                }
            };
            if (event.type == 'weekly') event.day = document.getElementById('scheduleDay').value;
            if (event.type == 'monthly' || event.type == 'yearly' || event.type == 'once') event.date = document.getElementById('scheduleDate').value;
            if (event.action.type == 'effect') {
                event.action.value = document.getElementById('actionValueEffect').value;
                event.action.duration = parseInt(document.getElementById('actionDuration').value);
            } else {
                event.action.channel = parseInt(document.getElementById('actionChannel').value);
                event.action.value = parseInt(document.getElementById('actionBrightness').value);
            }
            fetch('/add-schedule', {
                method: 'POST',
                body: new URLSearchParams({ event: JSON.stringify(event) })
            }).then(response => {
                if (response.ok) {
                    loadSchedule();
                    closeScheduleModal();
                } else {
                    alert('Помилка додавання події');
                }
            });
        });
        function loadFiles(path = '/') {
            fetch('/list?path=' + encodeURIComponent(path))
                .then(response => response.json())
                .then(data => {
                    let tbody = document.getElementById('fileList');
                    tbody.innerHTML = '';
                    data.forEach(item => {
                        let tr = document.createElement('tr');
                        tr.innerHTML = `<td>${item.name}</td><td>${item.isDir ? 'Папка' : item.size}</td><td><button onclick="deleteFile('${item.path}')">Видалити</button></td>`;
                        tbody.appendChild(tr);
                    });
                });
        }
        function deleteFile(path) {
            fetch('/delete?path=' + encodeURIComponent(path), { method: 'DELETE' })
                .then(response => {
                    if (response.ok) loadFiles();
                    else alert('Помилка видалення');
                });
        }
        document.getElementById('mkdirForm').addEventListener('submit', function(e) {
            e.preventDefault();
            let folder = document.getElementById('folderName').value;
            fetch('/mkdir', { method: 'POST', body: new URLSearchParams({ path: folder }) })
                .then(response => response.ok && loadFiles());
        });
        document.getElementById('uploadForm').addEventListener('submit', function(e) {
            e.preventDefault();
            let formData = new FormData(this);
            fetch('/upload', { method: 'POST', body: formData })
                .then(response => response.ok && loadFiles());
        });
        loadFiles();
    </script>
</body>
</html>
)rawliteral";

void initWebServer() {
  // Ініціалізуємо OTA Mutex
  if (otaMutex == nullptr) {
    otaMutex = xSemaphoreCreateMutex();
  }
  
  // LittleFS initialized in main setup

  server.on("/get-wifi-networks", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username.c_str(), http_password.c_str())) {
      return request->requestAuthentication();
    }
    forceScanNetworks(request);
  });

  server.on("/set-wifi", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username.c_str(), http_password.c_str())) {
      return request->requestAuthentication();
    }
    if (request->hasParam("ssid", true) && request->hasParam("password", true)) {
      char ssid[32];
      char password[64];
      strncpy(ssid, request->getParam("ssid", true)->value().c_str(), sizeof(ssid));
      strncpy(password, request->getParam("password", true)->value().c_str(), sizeof(password));
      requestWiFiConnect(ssid, password);
      request->send(200, "text/plain", "OK");
    } else {
      request->send(400, "text/plain", "Missing ssid or password");
    }
  });

  server.on("/set-brightness", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username.c_str(), http_password.c_str())) {
      return request->requestAuthentication();
    }
    if (request->hasParam("channel", true) && request->hasParam("value", true)) {
      int channel = request->getParam("channel", true)->value().toInt();
      int value = request->getParam("value", true)->value().toInt();
      if (channel >= 0 && channel <= 4 && value >= 0 && value <= 255) {
        setChannelBrightness(channel, value);
        request->send(200, "text/plain", "OK");
      } else {
        request->send(400, "text/plain", "Invalid channel or value");
      }
    } else {
      request->send(400, "text/plain", "Missing channel or value");
    }
  });

  server.on("/set-time", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username.c_str(), http_password.c_str())) {
      return request->requestAuthentication();
    }
    if (request->hasParam("year", true) && request->hasParam("month", true) &&
        request->hasParam("day", true) && request->hasParam("hour", true) &&
        request->hasParam("minute", true) && request->hasParam("second", true)) {
      int year = request->getParam("year", true)->value().toInt();
      int month = request->getParam("month", true)->value().toInt();
      int day = request->getParam("day", true)->value().toInt();
      int hour = request->getParam("hour", true)->value().toInt();
      int minute = request->getParam("minute", true)->value().toInt();
      int second = request->getParam("second", true)->value().toInt();
      setManualTime(year, month, day, hour, minute, second);
      request->send(200, "text/plain", "OK");
    } else {
      request->send(400, "text/plain", "Missing time parameters");
    }
  });

  server.on("/set-mqtt", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username.c_str(), http_password.c_str())) {
      return request->requestAuthentication();
    }
    if (request->hasParam("server", true) && request->hasParam("port", true) &&
        request->hasParam("user", true) && request->hasParam("pass", true)) {
      char server[64];
      int port = request->getParam("port", true)->value().toInt();
      char user[32];
      char pass[32];
      strncpy(server, request->getParam("server", true)->value().c_str(), sizeof(server));
      strncpy(user, request->getParam("user", true)->value().c_str(), sizeof(user));
      strncpy(pass, request->getParam("pass", true)->value().c_str(), sizeof(pass));
      setMQTTConfig(server, port, user, pass);
      request->send(200, "text/plain", "OK");
    } else {
      request->send(400, "text/plain", "Missing MQTT parameters");
    }
  });

  server.on("/get-schedule", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username.c_str(), http_password.c_str())) {
      return request->requestAuthentication();
    }
    String json = getSchedule();
    request->send(200, "application/json", json);
  });

  server.on("/add-schedule", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username.c_str(), http_password.c_str())) {
      return request->requestAuthentication();
    }
    if (!request->hasParam("event", true)) {
      request->send(400, "text/plain", "Missing event parameter");
      return;
    }
    String event = request->getParam("event", true)->value();
    addScheduleEvent(event);
    request->send(200, "text/plain", "OK");
  });

  server.on("/remove-schedule", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username.c_str(), http_password.c_str())) {
      return request->requestAuthentication();
    }
    if (!request->hasParam("id", true)) {
      request->send(400, "text/plain", "Missing id parameter");
      return;
    }
    int id = request->getParam("id", true)->value().toInt();
    removeScheduleEvent(id);
    request->send(200, "text/plain", "OK");
  });

  // API для пресетів освітлення
  server.on("/api/lighting-presets", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username.c_str(), http_password.c_str())) {
      return request->requestAuthentication();
    }
    String json = getLightingPresets();
    request->send(200, "application/json", json);
  });

  server.on("/api/save-lighting-preset", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username.c_str(), http_password.c_str())) {
      return request->requestAuthentication();
    }
    if (!request->hasParam("name", true)) {
      request->send(400, "text/plain", "Missing name parameter");
      return;
    }
    String name = request->getParam("name", true)->value();
    String config = request->hasParam("config", true) ? request->getParam("config", true)->value() : "";
    saveLightingPreset(name, config);
    request->send(200, "text/plain", "OK");
  });

  server.on("/api/load-lighting-preset", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username.c_str(), http_password.c_str())) {
      return request->requestAuthentication();
    }
    if (!request->hasParam("name", true)) {
      request->send(400, "text/plain", "Missing name parameter");
      return;
    }
    String name = request->getParam("name", true)->value();
    loadLightingPreset(name);
    request->send(200, "text/plain", "OK");
  });

  // API для пресетів розкладу
  server.on("/api/schedule-presets", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username.c_str(), http_password.c_str())) {
      return request->requestAuthentication();
    }
    String json = getSchedulePresets();
    request->send(200, "application/json", json);
  });

  server.on("/api/save-schedule-preset", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username.c_str(), http_password.c_str())) {
      return request->requestAuthentication();
    }
    if (!request->hasParam("name", true)) {
      request->send(400, "text/plain", "Missing name parameter");
      return;
    }
    String name = request->getParam("name", true)->value();
    String config = request->hasParam("config", true) ? request->getParam("config", true)->value() : "";
    saveSchedulePreset(name, config);
    request->send(200, "text/plain", "OK");
  });

  server.on("/api/delete-preset", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username.c_str(), http_password.c_str())) {
      return request->requestAuthentication();
    }
    if (!request->hasParam("type", true) || !request->hasParam("name", true)) {
      request->send(400, "text/plain", "Missing type or name parameter");
      return;
    }
    String type = request->getParam("type", true)->value();
    String name = request->getParam("name", true)->value();
    deletePreset(type, name);
    request->send(200, "text/plain", "OK");
  });

  // API ендпоінти для пресетів
  server.on("/api/presets/lighting", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username.c_str(), http_password.c_str())) {
      return request->requestAuthentication();
    }
    String json = getLightingPresets();
    request->send(200, "application/json", json);
  });

  server.on("/api/presets/schedule", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username.c_str(), http_password.c_str())) {
      return request->requestAuthentication();
    }
    String json = getSchedulePresets();
    request->send(200, "application/json", json);
  });

  server.on("/api/presets/lighting/save", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username.c_str(), http_password.c_str())) {
      return request->requestAuthentication();
    }
    if (!request->hasParam("name", true)) {
      request->send(400, "text/plain", "Missing name parameter");
      return;
    }
    String name = request->getParam("name", true)->value();
    String config = request->hasParam("config", true) ? request->getParam("config", true)->value() : "";
    saveLightingPreset(name, config);
    request->send(200, "text/plain", "OK");
  });

  server.on("/api/presets/schedule/save", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username.c_str(), http_password.c_str())) {
      return request->requestAuthentication();
    }
    if (!request->hasParam("name", true)) {
      request->send(400, "text/plain", "Missing name parameter");
      return;
    }
    String name = request->getParam("name", true)->value();
    String config = request->hasParam("config", true) ? request->getParam("config", true)->value() : "";
    saveSchedulePreset(name, config);
    request->send(200, "text/plain", "OK");
  });

  server.on("/api/presets/lighting/load", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username.c_str(), http_password.c_str())) {
      return request->requestAuthentication();
    }
    if (!request->hasParam("name", true)) {
      request->send(400, "text/plain", "Missing name parameter");
      return;
    }
    String name = request->getParam("name", true)->value();
    loadLightingPreset(name);
    request->send(200, "text/plain", "OK");
  });

  server.on("/api/presets/delete", HTTP_DELETE, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username.c_str(), http_password.c_str())) {
      return request->requestAuthentication();
    }
    if (!request->hasParam("type") || !request->hasParam("name")) {
      request->send(400, "text/plain", "Missing type or name parameter");
      return;
    }
    String type = request->getParam("type")->value();
    String name = request->getParam("name")->value();
    deletePreset(type, name);
    request->send(200, "text/plain", "OK");
  });

  // API для денних розкладів
  server.on("^\\/api\\/schedule\\/day\\/([0-9]{4}-[0-9]{2}-[0-9]{2})$", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username.c_str(), http_password.c_str())) {
      return request->requestAuthentication();
    }
    String date = request->pathArg(0);
    String schedule = getDaySchedule(date);
    request->send(200, "application/json", schedule);
  });

  server.on("^\\/api\\/schedule\\/day\\/([0-9]{4}-[0-9]{2}-[0-9]{2})$", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username.c_str(), http_password.c_str())) {
      return request->requestAuthentication();
    }
  }, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    static String body;
    if (index == 0) body = "";
    body += String((char*)data).substring(0, len);

    if (index + len == total) {
      String date = request->pathArg(0);
      if (setDaySchedule(date, body)) {
        request->send(200, "text/plain", "OK");
      } else {
        request->send(400, "text/plain", "Failed to set schedule");
      }
    }
  });

  // API для річного розкладу
  server.on("^\\/api\\/schedule\\/year\\/([0-9]{4})$", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username.c_str(), http_password.c_str())) {
      return request->requestAuthentication();
    }
    int year = request->pathArg(0).toInt();
    String schedule = getYearSchedule(year);
    request->send(200, "application/json", schedule);
  });

  // Імпорт/експорт розкладів
  server.on("/api/schedule/import", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username.c_str(), http_password.c_str())) {
      return request->requestAuthentication();
    }
  }, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    static String body;
    if (index == 0) body = "";
    body += String((char*)data).substring(0, len);

    if (index + len == total) {
      if (importSchedule(body)) {
        request->send(200, "text/plain", "Schedule imported");
      } else {
        request->send(400, "text/plain", "Failed to import schedule");
      }
    }
  });

  server.on("/api/schedule/export", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username.c_str(), http_password.c_str())) {
      return request->requestAuthentication();
    }
    String schedule = exportSchedule();
    request->send(200, "application/json", schedule);
  });

  // Імпорт/експорт пресетів
  server.on("/api/presets/export", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username.c_str(), http_password.c_str())) {
      return request->requestAuthentication();
    }
    JsonDocument doc;
    JsonObject exportData = doc.to<JsonObject>();

    // Експортуємо lighting presets
    JsonDocument lightingPresets;
    deserializeJson(lightingPresets, getLightingPresets());
    exportData["lighting"] = lightingPresets;

    // Експортуємо schedule presets
    JsonDocument schedulePresets;
    deserializeJson(schedulePresets, getSchedulePresets());
    exportData["schedule"] = schedulePresets;

    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  });

  server.on("/api/presets/import", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username.c_str(), http_password.c_str())) {
      return request->requestAuthentication();
    }
  }, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    static String body;
    if (index == 0) body = "";
    body += String((char*)data).substring(0, len);

    if (index + len == total) {
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, body);
      if (error) {
        request->send(400, "text/plain", "Invalid JSON");
        return;
      }

      // Імпортуємо lighting presets
      if (doc["lighting"].is<JsonArray>()) {
        for (JsonObject preset : doc["lighting"].as<JsonArray>()) {
          String name = preset["name"];
          // Тут можна додати логіку для імпорту конкретних пресетів
        }
      }

      // Імпортуємо schedule presets
      if (doc["schedule"].is<JsonArray>()) {
        for (JsonObject preset : doc["schedule"].as<JsonArray>()) {
          String name = preset["name"];
          // Тут можна додати логіку для імпорту конкретних пресетів
        }
      }

      request->send(200, "text/plain", "Presets imported");
    }
  });

  server.on("/list", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username.c_str(), http_password.c_str())) {
      return request->requestAuthentication();
    }
    String path = request->hasParam("path") ? request->getParam("path")->value() : "/";
    File root = LittleFS.open(path);
    if (!root || !root.isDirectory()) {
      request->send(400, "text/plain", "Invalid path");
      return;
    }
    JsonDocument doc;
    JsonArray items = doc.to<JsonArray>();
    File file = root.openNextFile();
    while (file) {
      JsonObject item = items.add<JsonObject>();
      item["name"] = file.name();
      item["size"] = file.size();
      item["isDir"] = file.isDirectory();
      item["path"] = file.path();
      file = root.openNextFile();
    }
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  });

  server.on("/mkdir", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username.c_str(), http_password.c_str())) {
      return request->requestAuthentication();
    }
    if (!request->hasParam("path", true)) {
      request->send(400, "text/plain", "Missing path");
      return;
    }
    String path = request->getParam("path", true)->value();
    if (LittleFS.mkdir(path)) {
      request->send(200, "text/plain", "OK");
    } else {
      request->send(400, "text/plain", "Failed to create directory");
    }
  });

  server.on("/delete", HTTP_DELETE, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username.c_str(), http_password.c_str())) {
      return request->requestAuthentication();
    }
    if (!request->hasParam("path")) {
      request->send(400, "text/plain", "Missing path");
      return;
    }
    String path = request->getParam("path")->value();
    if (LittleFS.remove(path) || LittleFS.rmdir(path)) {
      request->send(200, "text/plain", "OK");
    } else {
      request->send(400, "text/plain", "Failed to delete");
    }
  });

  server.on(
      "/upload", HTTP_POST, [](AsyncWebServerRequest *request) {},
      [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
        static File file;
        static size_t totalWritten = 0;
        const size_t MAX_FILE_SIZE = 1048576;  // 1 MB макс
        
        if (!index) {
          if (!request->authenticate(http_username.c_str(), http_password.c_str())) {
            return request->requestAuthentication();
          }
          
          // Валідація імені файлу
          if (filename.length() == 0 || filename.length() > 256) {
            request->send(400, "text/plain", "Invalid filename length");
            return;
          }
          
          // Забезпечуємо, що файл знаходиться у корені
          if (filename.indexOf("/") != -1 && filename.indexOf("/") != 0) {
            request->send(400, "text/plain", "Invalid path");
            return;
          }
          
          String path = "/" + filename;
          Serial.println("Upload started: " + path);
          
          file = LittleFS.open(path, "w");
          if (!file) {
            request->send(400, "text/plain", "Failed to open file for writing");
            return;
          }
          totalWritten = 0;
        }
        
        if (len) {
          totalWritten += len;
          if (totalWritten > MAX_FILE_SIZE) {
            file.close();
            LittleFS.remove("/" + filename);
            request->send(413, "text/plain", "File too large");
            return;
          }
          
          if (file.write(data, len) != len) {
            file.close();
            LittleFS.remove("/" + filename);
            request->send(500, "text/plain", "Write error");
            return;
          }
        }
        
        if (final) {
          file.close();
          request->send(200, "text/plain", "Upload OK");
          Serial.println("Upload completed: " + filename + " (" + String(totalWritten) + " bytes)");
        }
      });

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username.c_str(), http_password.c_str())) {
      return request->requestAuthentication();
    }
    request->send(200, "text/html", filemanager_html);
  });

  // API Endpoint для отримання статусу
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username.c_str(), http_password.c_str())) {
      return request->requestAuthentication();
    }
    JsonDocument doc;
    doc["wifi_connected"] = (WiFi.status() == WL_CONNECTED);
    doc["wifi_ssid"] = WiFi.SSID();
    doc["ssid"] = WiFi.SSID(); // alias for frontend
    doc["wifi_rssi"] = WiFi.RSSI();
    doc["rssi"] = WiFi.RSSI();  // Для fullcontrol_5.html
    doc["ip"] = WiFi.localIP().toString();
    doc["temperature"] = getTemperature();
    
    // Час
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    char timestr[20];
    strftime(timestr, sizeof(timestr), "%H:%M:%S", timeinfo);
    doc["time"] = String(timestr);
    
    JsonArray channels = doc["channels"].to<JsonArray>();
    for (int i = 0; i < 5; i++) {
      channels.add(getChannelBrightness(i));
    }
    
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  });

  // API Endpoint для встановлення яскравості каналу
  server.on("/api/brightness", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username.c_str(), http_password.c_str())) {
      return request->requestAuthentication();
    }
    if (request->hasParam("channel", true) && request->hasParam("value", true)) {
      int channel = request->getParam("channel", true)->value().toInt();
      int value = request->getParam("value", true)->value().toInt();
      if (channel >= 0 && channel < 5 && value >= 0 && value <= 255) {
        setChannelBrightness(channel, value);
        request->send(200, "application/json", "{\"status\":\"ok\"}");
      } else {
        request->send(400, "application/json", "{\"status\":\"error\",\"msg\":\"Invalid parameters\"}");
      }
    } else {
      request->send(400, "application/json", "{\"status\":\"error\",\"msg\":\"Missing parameters\"}");
    }
  });

  // API Endpoint для WiFi сканування
  server.on("/api/wifi/scan", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username.c_str(), http_password.c_str())) {
      return request->requestAuthentication();
    }
    
    // Виконуємо сканування в асинхронному режимі
    WiFi.scanNetworks(true); // true = async mode
    
    // Для швидкої відповіді, повертаємо empty array
    // Клієнт повинен повторити запит через секунду
    request->send(200, "application/json", "{\"networks\":[],\"scanning\":true}");
  });

  // API Endpoint для отримання результатів сканування
  server.on("/api/wifi/results", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username.c_str(), http_password.c_str())) {
      return request->requestAuthentication();
    }
    
    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING) {
      request->send(200, "application/json", "{\"status\":\"scanning\"}");
      return;
    }
    
    JsonDocument doc;
    JsonArray networks = doc["networks"].to<JsonArray>();
    
    if (n > 0) {
      // Обмежуємо до 20 мереж щоб не перевантажити буфер
      int limit = (n > 20) ? 20 : n;
      for (int i = 0; i < limit; i++) {
        JsonObject net = networks.add<JsonObject>();
        net["ssid"] = WiFi.SSID(i);
        net["rssi"] = WiFi.RSSI(i);
        net["channel"] = WiFi.channel(i);
      }
    }
    
    WiFi.scanDelete();
    
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  });

  // API Endpoint для збереження WiFi
  server.on("/api/wifi/save", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username.c_str(), http_password.c_str())) {
      return request->requestAuthentication();
    }
    if (request->hasParam("ssid", true) && request->hasParam("password", true)) {
      String ssid = request->getParam("ssid", true)->value();
      String password = request->getParam("password", true)->value();
      requestWiFiConnect(ssid.c_str(), password.c_str());
      request->send(200, "application/json", "{\"status\":\"ok\"}");
    } else {
      request->send(400, "application/json", "{\"status\":\"error\"}");
    }
  });

  // API Endpoint для MQTT
  server.on("/api/mqtt/save", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username.c_str(), http_password.c_str())) {
      return request->requestAuthentication();
    }
    if (request->hasParam("server", true) && request->hasParam("port", true)) {
      String server = request->getParam("server", true)->value();
      int port = request->getParam("port", true)->value().toInt();
      String user = request->hasParam("user", true) ? request->getParam("user", true)->value() : "";
      String pass = request->hasParam("pass", true) ? request->getParam("pass", true)->value() : "";
      setMQTTConfig(server.c_str(), port, user.c_str(), pass.c_str());
      request->send(200, "application/json", "{\"status\":\"ok\"}");
    } else {
      request->send(400, "application/json", "{\"status\":\"error\"}");
    }
  });

  // API Endpoint для встановлення часу
  server.on("/api/time/set", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username.c_str(), http_password.c_str())) {
      return request->requestAuthentication();
    }
    if (request->hasParam("year", true) && request->hasParam("month", true) &&
        request->hasParam("day", true) && request->hasParam("hour", true) &&
        request->hasParam("minute", true) && request->hasParam("second", true)) {
      int year = request->getParam("year", true)->value().toInt();
      int month = request->getParam("month", true)->value().toInt();
      int day = request->getParam("day", true)->value().toInt();
      int hour = request->getParam("hour", true)->value().toInt();
      int minute = request->getParam("minute", true)->value().toInt();
      int second = request->getParam("second", true)->value().toInt();
      setManualTime(year, month, day, hour, minute, second);
      request->send(200, "application/json", "{\"status\":\"ok\"}");
    } else {
      request->send(400, "application/json", "{\"status\":\"error\"}");
    }
  });

  // API Endpoint для встановлення мови (заглушка)
  server.on("/api/language/set", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username.c_str(), http_password.c_str())) {
      return request->requestAuthentication();
    }
    request->send(200, "application/json", "{\"status\":\"ok\"}");
  });

  server.onNotFound([](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username.c_str(), http_password.c_str())) {
      return request->requestAuthentication();
    }
    
    String path = request->url();
    // Перевіряємо, чи це запит до HTML файлу
    if (path.endsWith(".html")) {
      if (LittleFS.exists(path)) {
        File file = LittleFS.open(path, "r");
        request->send(file, path, String("text/html"));
      } else {
        request->send(404, "text/plain", "File not found");
      }
    } else {
      request->send(404, "text/plain", "Not found");
    }
  });

  // OTA Update Handlers - обидва /update та /api/ota/upload для сумісності
  auto otaUploadHandler = [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username.c_str(), http_password.c_str())) {
      return request->requestAuthentication();
    }
    
    if (xSemaphoreTake(otaMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      bool success = !Update.hasError();
      otaInProgress = false;
      xSemaphoreGive(otaMutex);
      
      if (success) {
        request->send(200, "text/plain", "Update successful. Device will reboot.");
        // Перезавантажимо пристрій через 2 сек
        vTaskDelay(pdMS_TO_TICKS(2000));
        ESP.restart();
      } else {
        String error = "Update failed: " + String(Update.getError());
        request->send(400, "text/plain", error);
      }
    } else {
      request->send(503, "text/plain", "OTA update in progress");
    }
  };

  auto otaUploadBody = [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (!request->authenticate(http_username.c_str(), http_password.c_str())) {
      return;
    }
    
    if (!index) {
      if (xSemaphoreTake(otaMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        Serial.println("Another OTA update in progress");
        return;
      }
      
      otaInProgress = true;
      otaProgress = 0;
      
      Serial.println("Update started, filename: " + filename);
      
      // Перевіряємо розмір вільної пам'яті
      uint32_t freeSketchSpace = ESP.getFreeSketchSpace();
      if (freeSketchSpace < 1000000) {  // Менше 1MB
        Serial.println("Not enough free space for update: " + String(freeSketchSpace) + " bytes");
        otaInProgress = false;
        xSemaphoreGive(otaMutex);
        request->send(400, "text/plain", "Not enough free space");
        return;
      }
      
      if (!Update.begin((freeSketchSpace - 0x1000) & 0xFFFFF000)) {
        Serial.println("Update.begin failed");
        Serial.println("Error: " + String(Update.getError()));
        otaInProgress = false;
        xSemaphoreGive(otaMutex);
        return;
      }
    }
    
    if (otaInProgress && Update.write(data, len) == len) {
      otaProgress = index + len;
      Serial.println("Update progress: " + String(otaProgress) + " bytes");
    } else if (otaInProgress) {
      Serial.println("Update.write failed");
      Serial.println("Error: " + String(Update.getError()));
      Update.abort();
    }
    
    if (final) {
      if (Update.end(true)) {
        Serial.println("Update finished successfully, rebooting...");
      } else {
        Serial.println("Update failed");
        Serial.println("Error: " + String(Update.getError()));
      }
      otaInProgress = false;
      xSemaphoreGive(otaMutex);
    }
  };

  // Реєструємо обидва endpoints для OTA
  server.on("/update", HTTP_POST, otaUploadHandler, otaUploadBody);
  server.on("/api/ota/upload", HTTP_POST, otaUploadHandler, otaUploadBody);

  // Обробник для отримання списку файлів
  server.on("/list", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username.c_str(), http_password.c_str())) {
      return request->requestAuthentication();
    }
    String path = request->hasParam("path") ? request->getParam("path")->value() : "/";
    JsonDocument doc;
    JsonArray files = doc.to<JsonArray>();
    
    File root = LittleFS.open(path);
    if (!root || !root.isDirectory()) {
      request->send(200, "application/json", "[]");
      return;
    }
    
    File file = root.openNextFile();
    while (file) {
      JsonObject obj = files.add<JsonObject>();
      obj["name"] = file.name();
      obj["isDir"] = file.isDirectory();
      obj["size"] = file.size();
      obj["path"] = String(path) + String(file.name());
      file = root.openNextFile();
    }
    
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  });

  // Обробник для завантаження файлів
  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username.c_str(), http_password.c_str())) {
      return request->requestAuthentication();
    }
    request->send(200, "text/plain", "Upload finished");
  }, [](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (!request->authenticate(http_username.c_str(), http_password.c_str())) {
      return;
    }
    
    static File uploadFile;
    
    if (index == 0) {
      String filepath = "/" + filename;
      Serial.println("Upload start: " + filepath);
      uploadFile = LittleFS.open(filepath, "w");
      if (!uploadFile) {
        Serial.println("Failed to open file for writing");
        return;
      }
    }
    
    if (uploadFile) {
      uploadFile.write(data, len);
    }
    
    if (final) {
      if (uploadFile) {
        uploadFile.close();
        Serial.println("Upload finished: " + String(filename));
      }
    }
  });

  // Обробник для видалення файлів
  server.on("/delete", HTTP_DELETE, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username.c_str(), http_password.c_str())) {
      return request->requestAuthentication();
    }
    if (!request->hasParam("path")) {
      request->send(400, "text/plain", "Missing path parameter");
      return;
    }
    String path = request->getParam("path")->value();
    if (LittleFS.remove(path)) {
      request->send(200, "text/plain", "OK");
    } else {
      request->send(400, "text/plain", "Failed to delete file");
    }
  });

  // Обробник для створення папок
  server.on("/mkdir", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(http_username.c_str(), http_password.c_str())) {
      return request->requestAuthentication();
    }
    if (!request->hasParam("path", true)) {
      request->send(400, "text/plain", "Missing path parameter");
      return;
    }
    String path = request->getParam("path", true)->value();
    if (!path.startsWith("/")) path = "/" + path;
    if (LittleFS.mkdir(path)) {
      request->send(200, "text/plain", "OK");
    } else {
      request->send(400, "text/plain", "Failed to create directory");
    }
  });

  server.begin();
  Serial.println("Web server started");
}
