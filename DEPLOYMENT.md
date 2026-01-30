# 🚀 Гайд розгортання на GitHub

Цей документ описує, як правильно підготувати проект для публікації на GitHub.

---

## 📋 Перевірка перед публікацією

### 1. Важливі файли присутні?

```
✅ README.md           - Основна документація
✅ LICENSE             - MIT ліцензія
✅ CONTRIBUTING.md     - Гайд для контрибʼюторів
✅ FAQ.md              - Часті запитання
✅ .gitignore          - Ігнор непотрібних файлів
✅ platformio.ini      - Конфігурація проекту
✅ setup.sh            - Скрипт автоматичної установки
✅ .github/workflows/  - CI/CD конфіги
```

### 2. Видалити чутливі дані

```bash
# Перевірити на пароллі та облікові дані
grep -r "password\|secret\|token\|api_key" src/ include/

# Видалити файли
rm -f src/*_local.cpp
rm -f secrets.h
rm -f config_local.h
```

### 3. Очистити репозиторій

```bash
# Видалити .pio директорію
rm -rf .pio/

# Видалити артефакти компіляції
rm -rf build/
rm -f *.bin *.elf *.hex

# Очистити логи
rm -f build_log.txt
```

### 4. Перевірити .gitignore

```bash
git status --ignored

# Має бути чисто або тільки очікувані файли
```

---

## 🔧 Налаштування для GitHub

### 1. Створити репозиторій

```bash
# Ініціалізувати локально (якщо ще не зроблено)
git init
git config user.name "Your Name"
git config user.email "your.email@example.com"

# Додати все
git add .

# Перший коміт
git commit -m "[INITIAL] AquaSmart aquarium controller project"

# Додати remote
git remote add origin https://github.com/yourusername/aquasmart-controller.git

# Завантажити на GitHub
git branch -M main
git push -u origin main
```

### 2. Налаштування GitHub репозиторію

**Settings → General:**
- Description: "Professional aquarium lighting controller with schedule management"
- Topics: `esp32`, `iot`, `aquarium`, `automation`, `mqtt`, `arduino`
- Visibility: Public
- Wiki: Disable (документація в README)
- Projects: Enable
- Discussions: Enable

**Settings → Branches:**
- Default branch: `main`
- Protect main branch:
  - ✅ Require pull request reviews
  - ✅ Dismiss stale pull request approvals
  - ✅ Require status checks to pass

**Settings → Actions:**
- Allow all actions and reusable workflows

**Settings → Pages:** (опціонально)
- Source: Deploy from branch
- Branch: `gh-pages`
- Folder: `/ (root)`

### 3. Додати badges до README

Додайте до верху README.md:

```markdown
![Build Status](https://github.com/yourusername/aquasmart-controller/workflows/Build%20and%20Test/badge.svg)
![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Version](https://img.shields.io/badge/version-1.0.0-green.svg)
![Platform](https://img.shields.io/badge/platform-ESP32-red.svg)
```

### 4. Налаштувати GitHub Actions

Робочій процес уже налаштований в `.github/workflows/build.yml`

Він автоматично:
- Компілює код при push/PR
- Перевіряє розмір прошивки
- Завантажує артефакти

---

## 🔁 Актуалізація розгортання (коротко)

- LittleFS: перед заливкою образу файлів переконайтесь, що `board_build.filesystem = littlefs` в `platformio.ini` і використовуйте:
```bash
pio run -t uploadfs
```
- LittleFS монтується один раз у `src/main.cpp` на стартапі — не викликайте `LittleFS.begin()` в інших модулях.
- Збірка/завантаження прошивки:
```bash
pio run -t upload
```
- Відкрийте серійний лог для перевірки:
```bash
pio device monitor -p COM3 -b 115200
```

Якщо виникають помилки створення директорій (`open(): /littlefs/presets/... does not exist, no permits for creation`), пересоберіть `littlefs.bin` і зaлeйтe `uploadfs` (це створює потрібні каталоги та файли в образі).

## 📦 Структура репозиторію

```
aquasmart-controller/
├── .github/
│   └── workflows/
│       └── build.yml                  # CI/CD конфіг
├── .gitignore                         # Git фільтр
├── .vscode/
│   ├── c_cpp_properties.json
│   ├── extensions.json
│   └── settings.json
├── data/
│   └── index.html                     # Веб-інтерфейс
├── include/                           # Заголовки
│   ├── AquaWiFiControl.h
│   ├── EEPROMControl.h
│   ├── LightControl.h
│   ├── MQTTControl.h
│   ├── PresetControl.h
│   ├── RTC.h
│   ├── ScheduleControl.h
│   ├── TemperatureControl.h
│   └── WebServerControl.h
├── src/                               # Вихідний код
│   ├── main.cpp
│   ├── AquaWiFiControl.cpp
│   ├── EEPROMControl.cpp
│   ├── LightControl.cpp
│   ├── MQTTControl.cpp
│   ├── PresetControl.cpp
│   ├── RTC.cpp
│   ├── ScheduleControl.cpp
│   ├── TemperatureControl.cpp
│   └── WebServerControl.cpp
├── CONTRIBUTING.md                    # Гайд контрибʼюторів
├── FAQ.md                            # Часті запитання
├── INTEGRATION_EXAMPLES.md           # Приклади інтеграції
├── LICENSE                           # MIT ліцензія
├── README.md                         # Основна документація
├── platformio.ini                    # Конфігурація PlatformIO
└── setup.sh                          # Скрипт установки
```

---

## 🔐 Безпека

### Чутливі дані

**Не комітте:**
```
❌ WiFi credentials
❌ MQTT passwords
❌ API keys
❌ Private certificates
❌ Secret tokens
```

**Де зберігати:**
- Локально в `secrets.h` (додано в `.gitignore`)
- В GitHub Secrets для CI/CD
- В облікових даних користувача

### Налаштування GitHub Secrets

```
Settings → Secrets and variables → Actions → New repository secret

Приклад:
- Name: WIFI_SSID
  Value: YourNetworkName

- Name: WIFI_PASSWORD
  Value: YourPassword
```

Використання у workflow:

```yaml
- name: Build with secrets
  env:
    WIFI_SSID: ${{ secrets.WIFI_SSID }}
    WIFI_PASSWORD: ${{ secrets.WIFI_PASSWORD }}
```

---

## 📝 Git best practices

### Commit messages

Дотримуйтесь формату:

```
[TYPE] Brief description (50 chars max)

Detailed explanation...
- Point 1
- Point 2

Fixes #123
```

**Types:**
- `[FEATURE]` - Нова функціональність
- `[FIX]` - Виправлення помилки
- `[DOCS]` - Документація
- `[REFACTOR]` - Рефакторинг
- `[TEST]` - Тестування
- `[PERF]` - Оптимізація
- `[CHORE]` - Обслуговування
- `[RELEASE]` - Випуск версії

### Гілки

```
main                    # Production-ready
├── develop            # Development branch
├── feature/XXX        # Feature branches
├── fix/YYY           # Bugfix branches
└── hotfix/ZZZ        # Hotfix branches
```

---

## 🚢 Випуск версій (Releases)

### Створити GitHub Release

1. **Перейти на GitHub репозиторій**
2. **Releases → Create a new release**
3. **Заповнити:**
   ```
   Tag version: v1.0.0
   Release title: Version 1.0.0
   Description:
   
   ## 🎉 New Features
   - RGB lighting control
   - Annual scheduling
   
   ## 🐛 Bugfixes
   - Fixed WiFi reconnection
   
   ## 📦 Assets
   - firmware.bin (attached)
   ```

4. **Завантажити .bin файл**

### Semantic Versioning

```
vMAJOR.MINOR.PATCH

v1.0.0 - First release
v1.1.0 - New features
v1.0.1 - Bugfix
v2.0.0 - Breaking changes
```

---

## 🤝 Спілкування з спільнотою

### Додати Issues

Enable in Settings → Features → ✅ Issues

Шаблон для Issues:

```markdown
## Bug Report / Feature Request

### Description
[Опис проблеми або ідеї]

### Steps to Reproduce (для bugs)
1. Do this
2. Then this
3. Observe this

### Expected behavior
[Що мало бути]

### Actual behavior
[Що сталось]

### System info
- ESP32 Board: [model]
- PlatformIO version: [version]
- Arduino core: [version]
```

### Додати Discussions

Settings → Features → ✅ Discussions

Категорії:
- 💬 General - Загальне обговорення
- 💡 Ideas - Пропозиції
- 📚 Show and tell - Демо проектів
- ❓ Q&A - Питання та відповіді

---

## 🔄 Continuous Integration

### Auto-build на push

Вже налаштовано в `.github/workflows/build.yml`

Перевіряє:
- ✅ Успішна компіляція
- ✅ Розмір прошивки
- ✅ Артефакти

### Додати статус badge

```markdown
[![Build Status](https://github.com/yourusername/aquasmart-controller/actions/workflows/build.yml/badge.svg)](https://github.com/yourusername/aquasmart-controller/actions/workflows/build.yml)
```

---

## 📊 GitHub Analytics

### Показати статистику

Automatic:
- Insights → Traffic
- Insights → Network
- Insights → Pulse

---

## ✅ Фінальна перевірка

Перед першим публічним git push:

```bash
# Перевірити .gitignore
git status --ignored

# Перевірити комісти
git log --oneline -10

# Перевірити розміри файлів (великі файли?)
git ls-files --size

# Сухий біг push (без змін)
git push --dry-run origin main

# Перевірити, що немає великих бінарних файлів
find . -size +1M -type f | grep -v .git
```

---

## 🎯 Рекомендації

1. **Додати CHANGELOG.md** для отслідження версій
2. **Додати CODE_OF_CONDUCT.md** для поведінки спільноти
3. **Додати SECURITY.md** для повідомлення уразливостей
4. **Додати ROADMAP.md** для планів розвитку
5. **Додати CREDITS.md** для дякування дежавелопаттам

---

## 🚀 Після публікації

### Промоція проекту

1. **Додати до тематичних списків:**
   - Awesome ESP32
   - Awesome Arduino
   - Awesome IoT

2. **Поділитися в спільнотах:**
   - Reddit: r/esp32, r/arduino, r/homeautomation
   - GitHub Discussions
   - Arduino Forum

3. **Написати про проект:**
   - Блог пост
   - Відео туторіал
   - Apresentация

---

**Готово! Проект готовий до GitHub! 🎉**

[← Повернутись до README](README.md)
