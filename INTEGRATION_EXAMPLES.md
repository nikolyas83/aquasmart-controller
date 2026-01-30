# Приклади інтеграції з системами автоматизації

## 🏠 Home Assistant

### 1. MQTT Discovery (рекомендується)

Додайте до `configuration.yaml`:

```yaml
mqtt:
  discovery: true
  discovery_prefix: homeassistant
  broker: YOUR_MQTT_BROKER_IP
  username: aquasmart
  password: your_password
```

AquaSmart автоматично створить entities в Home Assistant.

### 2. Ручна конфігурація

```yaml
mqtt:
  broker: 192.168.1.100

light:
  - platform: mqtt
    unique_id: aquasmart_light
    name: "Aquarium Light"
    command_topic: "aquasmart/light/brightness/set"
    state_topic: "aquasmart/light/brightness"
    brightness_state_topic: "aquasmart/light/brightness"
    brightness_command_topic: "aquasmart/light/brightness/set"
    brightness_scale: 255
    on_command_type: "brightness"
    payload_on: '{"brightness": 255}'
    payload_off: '{"brightness": 0}'
    optimistic: false

sensor:
  - platform: mqtt
    unique_id: aquasmart_temp
    name: "Tank Temperature"
    state_topic: "aquasmart/temperature"
    unit_of_measurement: "°C"
    device_class: temperature
    value_template: "{{ value | float(0) }}"

  - platform: mqtt
    unique_id: aquasmart_time
    name: "AquaSmart Time"
    state_topic: "aquasmart/time"
    icon: mdi:clock

script:
  aquasmart_morning:
    alias: "Morning Aquarium Schedule"
    sequence:
      - service: mqtt.publish
        data:
          topic: "aquasmart/preset/set"
          payload: '{"group": "Dawn", "preset": "Morning Glow"}'

  aquasmart_night:
    alias: "Night Aquarium Schedule"
    sequence:
      - service: mqtt.publish
        data:
          topic: "aquasmart/preset/set"
          payload: '{"group": "Night", "preset": "Starry Night"}'

automation:
  - alias: "Aquarium Morning Light"
    trigger:
      platform: time
      at: "06:00:00"
    action:
      service: script.aquasmart_morning

  - alias: "Aquarium Night Light"
    trigger:
      platform: time
      at: "22:00:00"
    action:
      service: script.aquasmart_night

  - alias: "Alert High Temperature"
    trigger:
      platform: numeric_state
      entity_id: sensor.tank_temperature
      above: 28
    action:
      service: notify.telegram
      data:
        message: "⚠️ Aquarium temperature is {{ states('sensor.tank_temperature') }}°C"
```

---

## 🔵 Node-RED

### 1. Отримання статусу

**Nodes:**
```
Inject (5s interval)
    ↓
HTTP Request (GET /api/status)
    ↓
Parse JSON
    ↓
Debug / Dashboard Gauge
```

**HTTP Request Configuration:**
```
URL: http://192.168.1.100/api/status
Auth: Basic
  Username: aquasmart
  Password: aquasmart
```

### 2. Установка яскравості

**Flow:**
```json
[
  {
    "id": "light_control",
    "type": "ui_slider",
    "label": "Aquarium Brightness",
    "min": 0,
    "max": 255,
    "step": 1,
    "topic": "brightness",
    "payload": "",
    "x": 100,
    "y": 100,
    "wires": [["http_request"]]
  },
  {
    "id": "http_request",
    "type": "http request",
    "method": "POST",
    "url": "http://192.168.1.100/set-brightness",
    "useAuth": true,
    "user": "aquasmart",
    "password": "aquasmart",
    "payload": "channel={{payload.channel}}&value={{payload.value}}",
    "wires": [["response"]]
  },
  {
    "id": "response",
    "type": "debug",
    "active": true
  }
]
```

### 3. Розклад з вибором дня

```json
[
  {
    "id": "date_picker",
    "type": "ui_date_picker",
    "label": "Select Day",
    "x": 100,
    "y": 200,
    "wires": [["fetch_schedule"]]
  },
  {
    "id": "fetch_schedule",
    "type": "http request",
    "method": "GET",
    "url": "http://192.168.1.100/api/schedule/day/{{payload}}",
    "useAuth": true,
    "user": "aquasmart",
    "password": "aquasmart",
    "wires": [["display"]]
  },
  {
    "id": "display",
    "type": "ui_template",
    "template": "<table><tr><th>Hour</th><th>Preset</th><th>Intensity</th></tr>{{#schedule}}<tr><td>{{hour}}</td><td>{{preset}}</td><td>{{intensity}}</td></tr>{{/schedule}}</table>"
  }
]
```

---

## 🐍 Python Script

### Базовий приклад

```python
import requests
from requests.auth import HTTPBasicAuth
import json
 
# Актуальний стан (оновлено 2026-01-30)
# - `/api/status` повертає `ssid`, `rssi`, `ip`, `time`, `temperature`.
# - Для роботи з файловим менеджером використовуйте `/list`, `/upload`, `/delete`, `/mkdir`.

def get_status(ip, user='aquasmart', password='aquasmart'):
  url = f'http://{ip}/api/status'
  r = requests.get(url, auth=HTTPBasicAuth(user, password), timeout=5)
  return r.json()

def upload_file(ip, filepath, user='aquasmart', password='aquasmart'):
  url = f'http://{ip}/upload'
  with open(filepath, 'rb') as f:
    files = {'file': (filepath, f)}
    r = requests.post(url, files=files, auth=HTTPBasicAuth(user, password))
  return r.status_code == 200

BASE_URL = "http://192.168.1.100"
AUTH = HTTPBasicAuth('aquasmart', 'aquasmart')

class AquaSmart:
    def __init__(self, base_url, auth):
        self.base_url = base_url
        self.auth = auth
    
    def get_status(self):
        """Get device status"""
        response = requests.get(f"{self.base_url}/api/status", auth=self.auth)
        return response.json()
    
    def set_brightness(self, channel, value):
        """Set channel brightness (0-255)"""
        requests.post(
            f"{self.base_url}/set-brightness",
            data={'channel': channel, 'value': value},
            auth=self.auth
        )
    
    def load_preset(self, group, preset_name):
        """Load lighting preset"""
        requests.post(
            f"{self.base_url}/api/presets/lighting/load",
            data={'group': group, 'preset': preset_name},
            auth=self.auth
        )
    
    def get_schedule(self, date):
        """Get day schedule (format: YYYY-MM-DD)"""
        response = requests.get(
            f"{self.base_url}/api/schedule/day/{date}",
            auth=self.auth
        )
        return response.json()
    
    def set_time(self, year, month, day, hour, minute, second):
        """Set device time"""
        requests.post(
            f"{self.base_url}/set-time",
            data={
                'year': year,
                'month': month,
                'day': day,
                'hour': hour,
                'minute': minute,
                'second': second
            },
            auth=self.auth
        )

# Usage
aqua = AquaSmart(BASE_URL, AUTH)

# Get status
status = aqua.get_status()
print(f"Temperature: {status['temperature']}°C")
print(f"Time: {status['time']}")

# Set brightness
aqua.set_brightness(0, 200)  # Red channel to 200

# Load preset
aqua.load_preset("Dawn", "Morning Glow")

# Get schedule
schedule = aqua.get_schedule("2026-01-27")
print(json.dumps(schedule, indent=2))
```

### Автоматичний розклад

```python
import schedule
import time
from datetime import datetime

def morning_routine():
    print("🌅 Starting morning routine...")
    aqua.load_preset("Dawn", "Morning Glow")

def daytime_routine():
    print("☀️ Starting daytime routine...")
    aqua.load_preset("Midday", "Clear Day")

def evening_routine():
    print("🌆 Starting evening routine...")
    aqua.load_preset("Sunset", "Evening Hue")

def night_routine():
    print("🌙 Starting night routine...")
    aqua.load_preset("Night", "Starry Night")

# Schedule tasks
schedule.every().day.at("06:00").do(morning_routine)
schedule.every().day.at("12:00").do(daytime_routine)
schedule.every().day.at("18:00").do(evening_routine)
schedule.every().day.at("22:00").do(night_routine)

# Run scheduler
while True:
    schedule.run_pending()
    time.sleep(60)
```

---

## 📱 REST API з curl

### Отримати статус

```bash
curl -u aquasmart:aquasmart http://192.168.1.100/api/status
```

### Установити яскравість

```bash
curl -u aquasmart:aquasmart \
  -X POST \
  -d "channel=0&value=150" \
  http://192.168.1.100/set-brightness
```

### Завантажити пресет

```bash
curl -u aquasmart:aquasmart \
  -X POST \
  -d "group=Dawn&preset=Morning%20Glow" \
  http://192.168.1.100/api/presets/lighting/load
```

### Отримати розклад на день

```bash
curl -u aquasmart:aquasmart \
  http://192.168.1.100/api/schedule/day/2026-01-27
```

### Установити час

```bash
curl -u aquasmart:aquasmart \
  -X POST \
  -d "year=2026&month=1&day=27&hour=14&minute=30&second=0" \
  http://192.168.1.100/set-time
```

---

## 🔔 IFTTT Integration

### Webhook автоматизація

```
Trigger: If temperature > 28°C
Action: Make a web request

URL: https://ifttt.com/channels/aquasmart/actions/alert
Method: POST
Content Type: application/json

{
  "value1": "{{temperature}}",
  "value2": "High temperature warning",
  "value3": "Aquarium"
}
```

---

## 🎵 Інші ідеї

1. **Інтеграція з Amazon Alexa:**
   ```
   "Alexa, set aquarium light to 75%"
   → MQTT → AquaSmart
   ```

2. **Інтеграція з Google Home:**
   ```
   "Hey Google, turn on aquarium"
   → Google Home → MQTT → AquaSmart
   ```

3. **Telebot для моніторингу:**
   ```python
   import telebot
   
   @bot.message_handler(commands=['status'])
   def status(message):
       s = aqua.get_status()
       bot.send_message(
           message.chat.id,
           f"🐠 Tank: {s['temperature']}°C\n⏰ Time: {s['time']}"
       )
   ```

4. **InfluxDB + Grafana логування:**
   ```
   AquaSmart → MQTT → Node-RED → InfluxDB → Grafana
   ```

---

## 📚 Корисні посилання

- [Home Assistant MQTT Docs](https://www.home-assistant.io/integrations/mqtt/)
- [Node-RED Documentation](https://nodered.org/docs/)
- [PubSubClient (Arduino MQTT)](https://github.com/knolleary/pubsubclient)
- [MQTT Protocol Overview](https://mqtt.org/)

---

**Happy automation! 🚀**
