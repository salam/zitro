// Settings from the installer, sent over USB once the firmware runs:
//   provision {"provider":"openai","apiKey":"...","ssid":"...","password":"..."}
// All or nothing: one bad field stores nothing. Fields left out keep what is stored.
// Answers "@@PROVISION OK", or "@@PROVISION FAIL <kind of field>", and never echoes a value.
// Write-only by design: no serial command reads a key back.

void handleProvisionCommand(const String& json) {
  JsonDocument doc;
  if (deserializeJson(doc, json)) {
    Serial.println("@@PROVISION FAIL json");
    return;
  }
  JsonVariantConst settings = doc.as<JsonVariantConst>();
  String error = provisioningError(settings);
  if (error.length() > 0) {
    Serial.println("@@PROVISION FAIL " + error);
    return;
  }

  JsonObjectConst fields = settings.as<JsonObjectConst>();
  Preferences settingsPrefs;
  settingsPrefs.begin("settings", false);
  for (JsonPairConst field : fields) {
    const char* key = field.key().c_str();
    if (strcmp(key, "ssid") == 0 || strcmp(key, "password") == 0) continue;
    settingsPrefs.putString(key, field.value().as<const char*>());
  }
  settingsPrefs.end();

  if (fields["ssid"].is<const char*>()) {
    saveWiFiCredential(fields["ssid"].as<const char*>(), fields["password"] | "");
  }

  Serial.println("@@PROVISION OK");
  Serial.flush();
  delay(200);
  ESP.restart();  // Boot with the new settings: connect to WiFi, leave setup mode
}
