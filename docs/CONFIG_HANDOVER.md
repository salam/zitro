# Configuration Handover from Installer to Device

## Overview

The installer uploads a `config.json` file to the device's SPIFFS partition during the flashing process. The Arduino firmware should read this configuration on first boot and use it as the initial device configuration.

## Installer Behavior

The macOS installer:
1. Collects user configuration during the setup wizard:
   - OpenAI API key (optional)
   - WiFi SSID and password (optional)
2. Creates a `config.json` file with these settings
3. Generates a SPIFFS filesystem image containing `config.json`
4. Flashes this SPIFFS image to partition address `0x670000`

## Arduino Firmware Implementation Required

The Arduino firmware needs to:

### 1. Read Configuration from SPIFFS on Boot

```cpp
#include <SPIFFS.h>
#include <ArduinoJson.h>

void loadInstallerConfig() {
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS mount failed");
    return;
  }

  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("No installer config found");
    return;
  }

  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, configFile);
  configFile.close();

  if (error) {
    Serial.println("Failed to parse config.json");
    return;
  }

  // Extract configuration
  const char* openai_key = doc["openai_api_key"];
  const char* wifi_ssid = doc["wifi_ssid"];
  const char* wifi_password = doc["wifi_password"];

  // Apply configuration if not already set locally
  if (openai_key && strlen(openai_key) > 0) {
    if (!hasLocalOpenAIKey()) {
      setOpenAIKey(openai_key);
      Serial.println("Applied OpenAI key from installer");
    }
  }

  if (wifi_ssid && strlen(wifi_ssid) > 0) {
    if (!hasLocalWiFiConfig()) {
      setWiFiCredentials(wifi_ssid, wifi_password);
      Serial.println("Applied WiFi config from installer");
    }
  }
}
```

### 2. Priority System

Configuration priority (highest to lowest):
1. **User-configured settings** (set via device menus or web interface)
2. **Installer-provided settings** (from `/config.json` on SPIFFS)
3. **Factory defaults** (built into firmware)

### 3. Implementation Flow

```
┌─────────────────────────────────────────────┐
│          Device Boot Sequence               │
└─────────────────────────────────────────────┘
                    │
                    ▼
        ┌───────────────────────┐
        │  Initialize SPIFFS    │
        └───────────────────────┘
                    │
                    ▼
        ┌───────────────────────┐
        │ Load /config.json     │
        │  (if exists)          │
        └───────────────────────┘
                    │
                    ▼
        ┌───────────────────────┐
        │ Check for local       │
        │ user configuration    │
        └───────────────────────┘
                    │
        ┌───────────┴───────────┐
        │                       │
        ▼                       ▼
  ┌─────────┐          ┌────────────────┐
  │ Exists? │          │ Does not exist │
  └─────────┘          └────────────────┘
        │                       │
        ▼                       ▼
  ┌─────────┐          ┌────────────────┐
  │  Use    │          │  Use installer │
  │  local  │          │     config     │
  └─────────┘          └────────────────┘
                               │
                               ▼
                    ┌──────────────────────┐
                    │ If WiFi configured:  │
                    │ Skip setup portal    │
                    │                      │
                    │ If API key set:      │
                    │ Enable AI features   │
                    └──────────────────────┘
                               │
                               ▼
                    ┌──────────────────────┐
                    │  Start main app loop │
                    └──────────────────────┘
```

### 4. Skip Setup Portal Logic

```cpp
void setup() {
  // ... initialization ...

  loadInstallerConfig();

  // Check if we can skip setup portal
  if (hasValidWiFiConfig() && hasValidOpenAIKey()) {
    // All required config present - go straight to app
    Serial.println("Configuration complete, starting app");
    startMainApplication();
  } else {
    // Missing config - show setup portal
    Serial.println("Configuration incomplete, starting setup portal");
    startSetupPortal();
  }
}
```

### 5. Configuration Storage

Store user preferences separately from installer config:
- **Installer config**: `/config.json` (read-only, set once during install)
- **User preferences**: `/user_config.json` or NVS (Preferences library)

This allows:
- Preserving installer config as a backup/reference
- Users to override installer settings without losing them
- Factory reset to restore installer-provided defaults

### 6. Example config.json Format

```json
{
  "openai_api_key": "sk-proj-...",
  "wifi_ssid": "MyNetwork",
  "wifi_password": "password123"
}
```

All fields are optional. If not provided during installation, they will be absent from the JSON.

## Benefits

1. **Zero-touch Setup**: Users who configure during installation can use the device immediately
2. **Flexible**: Users can still configure manually if they skip installer setup
3. **Factory Reset**: Can restore to installer-provided defaults
4. **Upgradable**: Firmware updates don't overwrite user configuration

## Implementation Checklist

- [ ] Add SPIFFS support to Arduino project
- [ ] Add ArduinoJson library dependency
- [ ] Implement `loadInstallerConfig()` function
- [ ] Add config priority logic (user > installer > defaults)
- [ ] Update setup flow to skip portal if configured
- [ ] Test with config provided
- [ ] Test with no config (manual setup)
- [ ] Test config override (user changes installer settings)
- [ ] Add factory reset feature (restore installer config)

## Testing

### Test Case 1: Full Configuration
1. Install with API key and WiFi
2. Device should boot directly to app
3. WiFi should connect automatically
4. AI features should work immediately

### Test Case 2: Partial Configuration
1. Install with WiFi only (no API key)
2. Device should connect to WiFi
3. Device should prompt for API key in app

### Test Case 3: No Configuration
1. Install without any config
2. Device should show setup portal
3. User configures manually

### Test Case 4: Configuration Override
1. Install with config
2. User changes settings in device menu
3. User config should take precedence
4. Installer config remains as backup

---

**Note**: This feature allows the installer to provide a seamless first-boot experience while maintaining user control and flexibility.
