# 🐠 AquaSmart - ESP32 Aquarium Controller

<div align="center">

![Version](https://img.shields.io/badge/version-1.0.0-blue.svg)
![Platform](https://img.shields.io/badge/platform-ESP32-green.svg)
![License](https://img.shields.io/badge/license-MIT-orange.svg)
![Language](https://img.shields.io/badge/language-C%2B%2B-red.svg)

**Професійна система управління акваріумом з AI плануванням та веб-інтерфейсом**

[Функціональність](#-основні-функції) • [Установка](#-установка-та-налаштування) • [API](#-api-ендпоінти) • [Використання](#-як-використовувати) • [Архітектура](#-архітектура-системи)

</div>

---

## 🔔 Актуальний стан (оновлено 2026-01-30)

- LittleFS монтується централізовано в `src/main.cpp` під час старту (`LittleFS.begin(true)`). Інші модулі більше не викликають `LittleFS.begin()` щоб уникнути конфлікту монтування.
- `/api/status` тепер повертає поле `ssid` (окрім `wifi_ssid`/`rssi`), а також `ip`, `time`, `temperature` і масив `channels` — фронтенд використовує `data.ssid` і `data.rssi`.
- Додано AP fallback: SSID `aquaconfig`, пароль `11111111` — пристрій переходить в AP швидше при низькій кількості невдалих з'єднань.
- RTC: ручне встановлення часу зберігається в `Preferences`, при старті відновлюється і синхронізується із системним часом (щоб `/api/status` показував коректний час).
- Presets: додано створення простого `presets/lighting/default.json` при ініціалізації для уникнення помилок при відсутності файлу.

Ці зміни вже застосовані в коді (`src/`), збірка та заливка прошивки/FS — через PlatformIO (див. розділ «Швидкий старт»).

## 📖 Зміст

1. [Огляд](#-огляд-проекту)
2. [Основні функції](#-основні-функції)
3. [Вимоги](#-вимоги)
4. [Установка](#-установка-та-налаштування)
5. [API Ендпоінти](#-api-ендпоінти)
6. [Веб-інтерфейс](#-веб-інтерфейс)
7. [Архітектура](#-архітектура-системи)
8. [Лог та налагодження](#-лог-та-налагодження)
9. [Ліцензія](#-ліцензія)

---

## 🌊 Огляд проекту

**AquaSmart** - це потужна система управління акваріумом на базі ESP32 з використанням FreeRTOS. Система забезпечує:

✅ Автоматичне керування RGB/спектральним освітленням  
✅ Річне планування з щоденними розкладами  
✅ Моніторинг температури в реальному часі  
✅ Веб-інтерфейс з сенсорною оптимізацією  
✅ REST API для інтеграції  
✅ MQTT підтримка для Home Assistant/Node-RED  
✅ Безпечне збереження налаштувань  
✅ OTA оновлення прошивки  

---

## 🚀 Основні функції

### 🎨 Система управління освітленням

- **5-канальний PWM контроль** з корекцією гами (0-100%)
- **Ефекти освітлення:**
  - 🌅 Схід сонця (поступове збільшення)
  - 🌆 Захід сонця (поступове зменшення)
  - 🌙 Нічний режим
  - 🎯 Миттєве перемикання
- **Спектральні пресети:** Dawn, Midday, Sunset, Night, Exotic
- **Поточна яскравість в реальному часі**

### 📅 Річна система планування

- **Календар на весь рік** (365+ днів)
- **Денні розклади:** до 24 точок переходу в день
- **Гнучке планування:**
  - Закріпити пресет на конкретну годину
  - Автоматичні переходи між пресетами
  - Вихідні дні та вибірковий розклад
- **Сценарії:**
  - Експорт/імпорт розкладів (JSON)
  - Безперервна робота за розкладом

### 🌡️ Моніторинг та контроль

- **Датчик DS18B20** для температури води
- **RTC DS1307** для точного часу
- **NTP синхронізація** через WiFi (раз на годину)
- **Оповіщення** при відхиленні температури
- **Логування подій** в систему

### 🔗 Комунікація

- **REST API** (29+ ендпоїнтів)
- **MQTT клієнт** для Home Assistant/Node-RED
- **Веб-сервер** на :80 та :8080
- **WebSocket** дляReal-time оновлень
- **Аутентифікація:** базова HTTP auth

### 💾 Налаштування та зберігання

- **Пресети освітлення:** групи за категоріями
- **Пресети розкладів:** збереження сценаріїв
- **JSON экспорт/імпорт** всіх конфігурацій
- **LittleFS** для надійного зберігання
- **EEPROM** для критичних налаштувань

---

## 🛠 Вимоги

### Апаратне забезпечення
- **ESP32 Development Board** (з WiFi/BLE)
- **PCA9685** PWM драйвер (I2C) - 5 каналів
- **DS18B20** температурний датчик (1-wire)
- **DS1307** RTC модуль (I2C) *опціонально*
- **Живлення:** 3.3V-5V регульований виток

### ПО та бібліотеки
```
Platform: espressif32
Framework: arduino
Board: esp32dev

Залежності:
- AsyncTCP @ 3.4.9
- ESPAsyncWebServer @ 3.8.1
- ArduinoJson @ 7.4.2
- PubSubClient @ 2.8
- DallasTemperature @ 4.0.5
- RTClib @ 2.1.4
- ElegantOTA @ 3.1.7
```

### Ст розробки
- PlatformIO 6.0+
- Python 3.8+
- Git

---

## ⚙️ Установка та налаштування

### 1. Клонування репозиторію

```bash
git clone https://github.com/yourusername/aquasmart-controller.git
cd aquasmart-controller
```

### 2. Встановлення залежностей

```bash
# Через PlatformIO
pio project init --ide vscode

# Або автоматично при першому build
```

### 3. Конфігурація platformio.ini

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200

# I2C pins
build_flags =
    -DI2C_SDA=21
    -DI2C_SCL=22
```

### 4. Завантаження на пристрій

```bash
# Build проект
pio run

# Завантажити прошивку
pio run --target upload

# Монітор вихіду
pio run --target monitor
```

### 5. Перший запуск

1. ESP32 стартує в режимі AP (точка доступу)
2. Підключитися до `AquaSmart_XXXX` WiFi мережі
3. Перейти на `http://192.168.4.1`
4. Ввести SSID та пароль WiFi
5. Пристрій перезавантажується та підключається

---

## 📋 API Ендпоінти

### 📊 Статус та інформація

```http
GET /api/status
```
**Відповідь:**
```json
{
  "temperature": 26.5,
  "time": "2026-01-27 14:30:45",
  "rssi": -65,
  "uptime": 3600000,
  "freeHeap": 124856
}
```

---

### 🎨 Керування освітленням

#### Установка яскравості каналу
```http
POST /set-brightness
Content-Type: application/x-www-form-urlencoded

channel=0&value=128
```

| Параметр | Тип | Описання |
|----------|-----|---------|
| channel | int | 0-4 (Red, Green, Blue, ColdWhite, WarmWhite) |
| value | int | 0-255 яскравість |

**Приклад curl:**
```bash
curl -X POST "http://192.168.1.100/set-brightness" \
  -d "channel=0&value=200"
```

---

#### Отримання пресетів освітлення
```http
GET /api/presets/lighting
```

**Відповідь:**
```json
{
  "presets": [
    {
      "group": "Dawn",
      "name": "Morning Glow",
      "sliders": {
        "master": 80,
        "warmWhite": 60,
        "coldWhite": 20,
        "red": 10,
        "green": 0,
        "blue": 0
      }
    }
  ]
}
```

---

#### Завантаження пресету
```http
POST /api/presets/lighting/load
Content-Type: application/x-www-form-urlencoded

group=Dawn&preset=Morning%20Glow
```

---

#### Збереження пресету
```http
POST /api/presets/lighting/save
Content-Type: application/json

{
  "name": "My Preset",
  "group": "Custom",
  "sliders": {
    "master": 75,
    "warmWhite": 50,
    "coldWhite": 25,
    "red": 0,
    "green": 0,
    "blue": 0
  }
}
```

---

### 📅 Розклади

#### Отримання розкладу дня
```http
GET /api/schedule/day/2026-01-27
```

**Відповідь:**
```json
{
  "date": "2026-01-27",
  "schedule": [
    {
      "hour": 6,
      "preset": "Dawn: Morning Glow",
      "intensity": 80
    },
    {
      "hour": 12,
      "preset": "Midday: Bright Noon",
      "intensity": 100
    }
  ]
}
```

---

#### Установлення розкладу дня
```http
POST /api/schedule/day/2026-01-27
Content-Type: application/json

{
  "schedule": [
    {
      "hour": 6,
      "preset": "Dawn"
    },
    {
      "hour": 12,
      "preset": "Midday"
    },
    {
      "hour": 18,
      "preset": "Sunset"
    },
    {
      "hour": 22,
      "preset": "Night"
    }
  ]
}
```

---

#### Експорт розкладу
```http
GET /api/schedule/export
```

**Відповідь:** JSON файл всіх розкладів року

---

#### Імпорт розкладу
```http
POST /api/schedule/import
Content-Type: application/json

{
  "scheduleData": {...},
  "dateSchedules": {...}
}
```

---

### ⚙️ Налаштування

#### Встановлення часу
```http
POST /set-time
Content-Type: application/x-www-form-urlencoded

year=2026&month=1&day=27&hour=14&minute=30&second=0
```

---

#### WiFi налаштування
```http
POST /set-wifi
Content-Type: application/x-www-form-urlencoded

ssid=MyNetwork&password=MyPassword123
```

---

#### MQTT налаштування
```http
POST /set-mqtt
Content-Type: application/x-www-form-urlencoded

server=mqtt.example.com&port=1883&user=aquasmart&pass=secret
```

---

#### Сканування WiFi мереж
```http
GET /get-wifi-networks
```

**Відповідь:**
```json
{
  "networks": [
    {
      "ssid": "MyNetwork",
      "signal": -65,
      "security": "WPA2"
    }
  ]
}
```

---

### 💾 Пресети

#### Експорт всіх пресетів
```http
GET /api/presets/export
```

Завантажує `spectrum_presets.json`

---

#### Імпорт пресетів
```http
POST /api/presets/import
Content-Type: application/json

[
  {
    "group": "Custom",
    "name": "My Setup",
    "sliders": {...}
  }
]
```

---

#### Видалення пресету
```http
DELETE /api/presets/delete?group=Custom&preset=My%20Setup
```

---

## 🌐 Веб-інтерфейс

### Доступні сторінки

| Шлях | Описання |
|------|---------|
| `/` | Головна панель (шапка) |
| `/fullcontrol` | Повний контроль освітлення |
| `/schedule` | Менеджер розкладів |
| `/settings` | Налаштування системи |
| `/update` | OTA оновлення |
| `/filemanager` | Менеджер файлів |

### Особливості UI

- 📱 **Адаптивний дизайн** для мобільних та десктопу
- 🎨 **Спектральний графік** освітлення в реальному часі
- 📊 **6 слайдерів** для кожного каналу
- 🗓️ **Інтерактивний календар** для вибору дня
- 💾 **Локальне сховище** (localStorage) для офлайн режиму
- ⚡ **WebSocket** для live оновлень

### Експорт/Імпорт в інтерфейсі

1. Перейти в **Library** → **Експорт всіх пресетів**
2. Отримати `spectrum_presets.json`
3. На іншому пристрої: **Library** → **Імпорт пресетів**
4. Вставити JSON дані та натиснути **Імпортувати**

---

## 🏗️ Архітектура системи

### FreeRTOS Завдання (Tasks)

```
┌─────────────────────────────────────────┐
│         Main Core (Core 1)              │
├─────────────────────────────────────────┤
│ WiFiTask (1000 ms)        │ 2 пріоритет │
│ LightTask (50 ms)         │ 1 пріоритет │
│ TempTask (5000 ms)        │ 1 пріоритет │
│ MQTTTask (1000 ms)        │ 1 пріоритет │
│ ScheduleTask (60000 ms)   │ 1 пріоритет │
└─────────────────────────────────────────┘

┌─────────────────────────────────────────┐
│         App Core (Core 0)               │
├─────────────────────────────────────────┤
│ RTCTask (10000 ms)        │ 0 пріоритет │
└─────────────────────────────────────────┘
```

### Модульна структура

```cpp
src/
├── main.cpp                    // Точка входу, створення завдань
├── AquaWiFiControl.cpp         // WiFi з автоматичним підключенням
├── WebServerControl.cpp        // REST API сервер
├── LightControl.cpp            // PWM контроль освітлення
├── ScheduleControl.cpp         // Річне планування
├── TemperatureControl.cpp      // Датчик температури
├── MQTTControl.cpp            // MQTT комунікація
├── RTC.cpp                     // Час та синхронізація
├── PresetControl.cpp          // Управління пресетами
└── EEPROMControl.cpp          // Збереження даних

include/
└── *.h                         // Заголовки модулів
```

### Потоки обробки даних

```
Розклад YAML              LightControl
    ↓                          ↓
ScheduleControl ────→ setChannelBrightness
    ↓                          ↓
PWM (PCA9685) ←──────────────┘
    ↓
LED Strip (5 channels)
```

### Синхронізація часу

```
1. Запуск (startup)
   └→ Спроба читати RTC
   
2. WiFi підключення
   └→ RTCTask чекає підключення
   
3. Щогодини (через NTP)
   └→ syncTimeWithNTP("pool.ntp.org")
   └→ Обновити RTC
```

---

## 🔐 Безпека та Аутентифікація

### HTTP Basic Auth

Всі API запити вимагають базову аутентифікацію:

```http
Authorization: Basic YXF1YXNtYXJ0OmFxdWFzbWFydA==
```

Стандартні облікові дані:
- **Username:** `aquasmart`
- **Password:** `aquasmart`

⚠️ **Змініть пароль в production!** Див. `WebServerControl.cpp`

```cpp
const String http_username = "aquasmart";
const String http_password = "aquasmart";
```

---

### WiFi безпека

- Можна встановити WPA2 шифрування
- Пароль зберігається у LittleFS шифрованим
- MQTT також підтримує SSL (если налаштовано)

---

## 📡 MQTT Інтеграція

### Home Assistant приклад

```yaml
mqtt:
  broker: 192.168.1.100
  
light:
  - platform: mqtt
    name: "Aquarium Light"
    command_topic: "aquasmart/light/brightness"
    state_topic: "aquasmart/light/state"
    brightness_state_topic: "aquasmart/light/brightness"
    brightness_command_topic: "aquasmart/light/set_brightness"

sensor:
  - platform: mqtt
    name: "Tank Temperature"
    state_topic: "aquasmart/temp"
    unit_of_measurement: "°C"
```

### Topics

| Topic | Тип | Описання |
|-------|-----|---------|
| `aquasmart/light/state` | PUB | Статус освітлення (on/off) |
| `aquasmart/light/brightness` | PUB | Поточна яскравість (0-255) |
| `aquasmart/temp` | PUB | Температура води (°C) |
| `aquasmart/time` | PUB | Поточний час (HH:MM:SS) |
| `aquasmart/light/set_brightness` | SUB | Установка яскравості |
| `aquasmart/schedule/set` | SUB | Завантаження розкладу |

---

## 🐛 Лог та налагодження

### Висновок Серійного монітора

```
=== AquaSmart FreeRTOS стартує ===
[EEPROM] Ініціалізована
[WiFi] Шукаємо SSID: MyNetwork
[WiFi] Підключено! IP: 192.168.1.100
[RTC] Час синхронізований: 2026-01-27 14:30:45
[Temperature] DS18B20 готов
[MQTT] Підключено до mqtt.example.com:1883
[Schedule] Завантажений розклад на 2026-01-27
Усі таски створені.
```

### Налагодження

Активуйте детальне логування в `platformio.ini`:

```ini
build_flags =
    -DCORE_DEBUG_LEVEL=3
```

Рівні:
- 0 - No logging
- 1 - ERROR
- 2 - WARN
- 3 - INFO
- 4 - DEBUG
- 5 - VERBOSE

---

## 🔄 Оновлення

### OTA (Over-The-Air) Update

1. Перейти на `/update`
2. Вибрати файл `.bin` із build папки
3. Натиснути **Почати оновлення**
4. Пристрій перезавантажується

**Де знайти `.bin` файл:**
```
.pio/build/esp32dev/firmware.bin
```

### Ручне завантаження через USB

```bash
pio run --target upload
```

---

## 📊 Статистика проекту

```
├── Розмір прошивки: 1.0 MB
├── Використання RAM: 14.2% (46 KB з 327 KB)
├── Використання Flash: 77.4% (1.0 MB з 1.3 MB)
├── API Endpoints: 29
├── HTML сторінок: 6
├── FreeRTOS Tasks: 6
└── Залежностей бібліотек: 11
```

---

## 🚀 Швидкий старт

### 1. Встановити та запустити

```bash
git clone https://github.com/yourusername/aquasmart-controller.git
cd aquasmart-controller
pio run --target upload
pio run --target monitor
```

### 2. Налаштувати WiFi

Зайти на `http://192.168.4.1` та ввести WiFi облікові дані

### 3. Доступ до інтерфейсу

```
http://[IP-ADDRESS]:80
```

### 4. API тестування

```bash
curl -u aquasmart:aquasmart http://192.168.1.100/api/status
```

---

## 📝 Приклади використання

### Python скрипт для керування

```python
import requests
from requests.auth import HTTPBasicAuth

BASE_URL = "http://192.168.1.100"
AUTH = HTTPBasicAuth('aquasmart', 'aquasmart')

# Отримати статус
response = requests.get(f"{BASE_URL}/api/status", auth=AUTH)
print(response.json())

# Установити яскравість
requests.post(f"{BASE_URL}/set-brightness", 
              data={'channel': 0, 'value': 200},
              auth=AUTH)
```

### Node-RED flow

```json
[
  {
    "id": "mqtt_sub",
    "type": "mqtt in",
    "topic": "aquasmart/temp",
    "broker": "mqtt-broker"
  },
  {
    "id": "http_req",
    "type": "http request",
    "method": "POST",
    "url": "http://192.168.1.100/set-brightness"
  }
]
```

---

## 🤝 Внесення змін (Contributing)

Ми приймаємо pull requests! Для великих змін, будь ласка:

1. Fork репозиторій
2. Створіть feature гілку (`git checkout -b feature/AmazingFeature`)
3. Commit змін (`git commit -m 'Add AmazingFeature'`)
4. Push гілку (`git push origin feature/AmazingFeature`)
5. Відкрийте Pull Request

---

## 📜 Ліцензія

Проект ліцензований під [MIT License](LICENSE)

---

## 👨‍💼 Автор

**Nikolyas** - розроблення та дизайн

---

## 📞 Контакти та підтримка

- 🐙 GitHub: [aquasmart-controller](https://github.com/yourusername/aquasmart-controller)
- 💬 Issues: [Report Bug](https://github.com/yourusername/aquasmart-controller/issues)
- 📧 Email: support@aquasmart.io

---

## 🙏 Подяки

- ESP-IDF та Arduino core розробникам
- [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer)
- [ArduinoJson](https://github.com/bblanchon/ArduinoJson)
- Спільноті Home Assistant

---

<div align="center">

**⭐ If you found this project helpful, please consider giving it a star! ⭐**

Made with ❤️ for aquarium enthusiasts

</div>
- `GET /api/presets/schedule` - отримання пресетів розкладів
- `POST /api/presets/lighting/save` - збереження пресету освітлення
- `POST /api/presets/schedule/save` - збереження пресету розкладу
- `GET /api/presets/export` - експорт всіх пресетів
- `POST /api/presets/import` - імпорт пресетів

## 🔧 Встановлення та збірка

### Передумови
- PlatformIO IDE
- ESP32 плата
- Arduino framework

### Збірка
```bash
# Клонування репозиторію
git clone https://github.com/nikolyas83/aquasmart-controller.git
cd aquasmart-controller

# Відкриття в PlatformIO
platformio run
```

### Прошивка
```bash
platformio run --target upload
```

## 📁 Структура файлів

```
├── include/                 # Заголовочні файли
│   ├── AquaWiFiControl.h
│   ├── LightControl.h
│   ├── ScheduleControl.h
│   ├── PresetControl.h
│   └── ...
├── src/                     # Вихідний код
├── data/                    # Веб-ресурси (HTML, CSS, JS)
├── platformio.ini          # Конфігурація PlatformIO
└── README.md
```

## 🌐 Веб-інтерфейс

Система має вбудований веб-сервер з файловим менеджером, що дозволяє:
- Керувати файлами системи
- Динамічно оновлювати HTML інтерфейс без перепрошивки
- Моніторити стан системи
- Налаштовувати розклади та пресети

## 🔄 Архітектура розкладу

```json
{
  "2025": {
    "01-15": [
      {"time": "08:00", "preset": "sunrise"},
      {"time": "12:00", "preset": "daylight"},
      {"time": "20:00", "preset": "sunset"},
      {"time": "22:00", "preset": "night"}
    ]
  }
}
```

## 🤝 Внесок

Вітаються pull request'и та issues для покращення проекту!

## 📄 Ліцензія

Цей проект є відкритим програмним забезпеченням під ліцензією MIT.

## 📞 Контакти

- **Автор:** nikolyas83
- **Email:** nikolay.pestov@gmail.com
- **GitHub:** https://github.com/nikolyas83
