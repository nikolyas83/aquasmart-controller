## CHANGELOG

All notable changes to this project will be documented in this file.

### 2026-01-30 — v1.0.1
- Centralized LittleFS mount in `src/main.cpp` (removed duplicate `LittleFS.begin()` calls).
- Fixed presets handling (`src/PresetControl.cpp`):
  - Corrected directory paths and filename construction.
  - Create `presets/lighting/default.json` on init to avoid missing-file errors.
- Web server (`src/WebServerControl.cpp`): added `ssid` field to `/api/status` JSON for frontend compatibility; added alias `/api/ota/upload` for OTA; file manager endpoints improved (`/list`, `/upload`, `/delete`, `/mkdir`).
- WiFi behavior (`src/AquaWiFiControl.cpp`): faster AP fallback, AP SSID `aquaconfig` with password `11111111`, added failed-attempts threshold to trigger AP sooner.
- RTC (`src/RTC.cpp`): manual time is saved to `Preferences` and restored at boot; system time synchronized from RTC so `/api/status` shows correct time.
- Frontend (`data/fullcontrol_5.html`): header fixed — `temp`, `time`, `ip`, `ssid` elements restored; RSSI bars resized and mapped correctly; defensive DOM updates to avoid script errors.
- Documentation: updated `README.md`, `DEPLOYMENT.md`, `DOKUMENTACIJA_GOTOVA.md`, `FAQ.md`, `CONTRIBUTING.md`, `INTEGRATION_EXAMPLES.md`, `GITHUB_READY.md` to reflect current state and deployment notes.
- PlatformIO: `platformio.ini` updated with `upload_port = COM3` for local development convenience.

### Notes
- After pulling these changes, run:
```bash
pio run -t upload    # flash firmware
pio run -t uploadfs  # upload LittleFS image (data/)
pio device monitor -p COM3 -b 115200
```
- If you see LittleFS permission errors, re-upload the littlefs image with `uploadfs`.
