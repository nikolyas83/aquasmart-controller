# Release v1.0.1 — 2026-01-30

Короткі реліз-ноти для GitHub Release

## Основне
- Централізовано монтування LittleFS у `src/main.cpp` (усунено дублікати `LittleFS.begin()`).
- Виправлено роботу пресетів у `src/PresetControl.cpp`: коректні шляхи, побудова імен файлів, створення `presets/lighting/default.json` при ініціалізації.
- Додано поле `ssid` до відповіді `/api/status` для сумісності з UI; додано OTA alias `/api/ota/upload` та файлові ендпоінти (`/list`, `/upload`, `/delete`, `/mkdir`).
- Покращено AP-fallback: SSID `aquaconfig`, пароль `11111111`, швидше перехід у AP при невдалих підключеннях.
- RTC: ручне встановлення часу зберігається в `Preferences` і синхронізує системний час.
- Frontend: `data/fullcontrol_5.html` — виправлена шапка (температура, час, IP, SSID), RSSI-бари.
- Оновлена документація (`README.md`, `DEPLOYMENT.md`, `FAQ.md`, `CONTRIBUTING.md`, `INTEGRATION_EXAMPLES.md`, `GITHUB_READY.md`) і додано `CHANGELOG.md`.

## Кроки після пулу
1. Скомпіліть і залийте прошивку:
```bash
pio run -t upload
```
2. Завантажте образ LittleFS (щоб оновити статичні файли у `data/`):
```bash
pio run -t uploadfs
```
3. Перевірте серійний лог:
```bash
pio device monitor -p COM3 -b 115200
```

## Примітки
- Якщо `git push origin v1.0.1` не вдається — перевірте, чи додано `origin` і чи є у вас права на push у віддалений репозиторій.
- У разі питань або потреби у додаткових правках реліз-ноти можна відредагувати локально перед пушем.
