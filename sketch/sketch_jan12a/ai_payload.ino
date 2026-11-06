// Chat request bodies and response parsing for all providers - see ai_provider.h
// Pure functions (no network), unit-tested on the host by test/host/run_tests.sh.

// What may be stored as AI settings. The setup page and the installer share these rules.
bool isValidAiProvider(const String& provider) {
  return provider == AI_PROVIDER_ZITRO || provider == AI_PROVIDER_OPENAI || provider == AI_PROVIDER_ANTHROPIC ||
         provider == AI_PROVIDER_INFOMANIAK || provider == AI_PROVIDER_CUSTOM;
}

bool isValidSecretValue(const String& value) {
  return value.length() >= 8 && value.length() <= 300 && value.indexOf(' ') < 0;
}

bool isValidAiTextValue(const char* key, const String& value) {
  if (strcmp(key, "customUrl") == 0 && value.length() > 0 && !value.startsWith("https://")) return false;
  return value.length() <= 200;
}

// Checks settings from the installer: a flat JSON object of strings with the keys
// provider, ssid, password, and the secret and text keys above.
// Returns "" when all of it may be stored, otherwise which kind of field is wrong.
// Never includes a value: the answer goes out over serial.
String provisioningError(JsonVariantConst settings) {
  if (!settings.is<JsonObjectConst>()) return "json";
  JsonObjectConst fields = settings.as<JsonObjectConst>();
  if (fields.size() == 0) return "json";

  bool hasSsid = false, hasPassword = false;
  for (JsonPairConst field : fields) {
    const char* key = field.key().c_str();
    if (!field.value().is<const char*>()) return "json";
    String value = field.value().as<const char*>();

    if (strcmp(key, "provider") == 0) {
      if (!isValidAiProvider(value)) return "provider";
    } else if (strcmp(key, "ssid") == 0) {
      if (value.length() < 1 || value.length() > 32) return "ssid";
      hasSsid = true;
    } else if (strcmp(key, "password") == 0) {
      if (value.length() != 0 && (value.length() < 8 || value.length() > 63)) return "password";
      hasPassword = true;
    } else {
      bool known = false;
      for (const char* secretKey : SECRET_SETTING_KEYS) {
        if (strcmp(key, secretKey) == 0) {
          if (!isValidSecretValue(value)) return "key";
          known = true;
        }
      }
      for (const char* textKey : AI_TEXT_SETTING_KEYS) {
        if (strcmp(key, textKey) == 0) {
          if (!isValidAiTextValue(key, value)) return "text";
          known = true;
        }
      }
      if (!known) return "unknown";
    }
  }
  if (hasPassword && !hasSsid) return "ssid";
  return "";
}

String httpErrorText(int code, const String& provider) {
  if (code == 401 || code == 403) return "API key rejected";
  if (code == 402 || (code == 429 && provider == AI_PROVIDER_ZITRO)) return "Zitro quota reached";
  if (code == 429) return "Too many requests";
  return "HTTP " + String(code);
}

// Build the JSON body for one chat request.
// useFallback is set when the Anthropic server-side refusal fallback is requested
// (the caller must then send the matching anthropic-beta header).
String buildChatRequest(const ChatConfig& cfg, const String& systemPrompt, const String& userText,
                        int maxTokens, bool withMemory, bool jsonMode, bool& useFallback) {
  useFallback = false;
  JsonDocument req;
  req["model"] = cfg.model;
  JsonArray messages = req["messages"].to<JsonArray>();

  if (cfg.anthropic) {
    // Thinking is always on for current Claude models and shares max_tokens
    req["max_tokens"] = max(maxTokens, 2048);
    req["system"] = systemPrompt;
    req["output_config"]["effort"] = "low";
    if (cfg.model.indexOf("opus-5") >= 0 || cfg.model.indexOf("fable-5") >= 0 || cfg.model.indexOf("sonnet-5-5") >= 0) {
      req["fallbacks"] = "default";  // server-side refusal fallback
      useFallback = true;
    }
  } else {
    JsonObject sys = messages.add<JsonObject>();
    sys["role"] = "system";
    sys["content"] = systemPrompt;
    if (cfg.openaiStyle) {
      // Reasoning tokens count against this limit, so leave headroom
      req["max_completion_tokens"] = max(maxTokens, 1024);
      req["reasoning_effort"] = "low";
      if (jsonMode) req["response_format"]["type"] = "json_object";
    } else {
      req["max_tokens"] = maxTokens;
    }
  }

  if (withMemory) memoryAppendMessages(messages);

  JsonObject user = messages.add<JsonObject>();
  user["role"] = "user";
  user["content"] = userText;

  String body;
  serializeJson(req, body);
  return body;
}

// Extract the model's text from a provider response
ChatResult parseChatResponse(int httpCode, const String& resp, const ChatConfig& cfg) {
  ChatResult result = {false, "", ""};

  if (httpCode != 200) {
    result.error = httpErrorText(httpCode, cfg.provider);
    return result;
  }

  JsonDocument filter;
  filter["choices"][0]["message"]["content"] = true;
  filter["content"][0]["type"] = true;
  filter["content"][0]["text"] = true;
  filter["stop_reason"] = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, resp, DeserializationOption::Filter(filter));
  // With a filter, non-JSON input (e.g. a captive-portal HTML page) parses as an empty document
  if (err || !doc.is<JsonObject>()) {
    Serial.printf("AI response parse error: %s\n", err ? err.c_str() : "not a JSON object");
    result.error = "Parse error";
    return result;
  }

  if (cfg.anthropic) {
    if (String(doc["stop_reason"] | "") == "refusal") {
      result.error = "Refusal";
      return result;
    }
    // Skip thinking blocks; concatenate text blocks
    for (JsonObject block : doc["content"].as<JsonArray>()) {
      if (String(block["type"] | "") == "text") result.content += block["text"] | "";
    }
  } else {
    result.content = doc["choices"][0]["message"]["content"] | "";
  }

  result.content.trim();
  result.ok = result.content.length() > 0;
  if (!result.ok) result.error = "No content";
  return result;
}

// Lenient JSON field readers: models sometimes return booleans or numbers as strings
static bool jsonFlag(JsonVariantConst v) {
  if (v.is<bool>()) return v.as<bool>();
  if (v.is<const char*>()) return String(v.as<const char*>()) == "true";
  return false;
}

static String jsonText(JsonVariantConst v) {
  if (v.is<const char*>()) return String(v.as<const char*>());
  return "";
}

static int jsonInt(JsonVariantConst v) {
  if (v.is<const char*>()) return (int)lround(atof(v.as<const char*>()));
  if (v.is<int>()) return v.as<int>();
  if (v.is<double>()) return (int)lround(v.as<double>());  // e.g. 90.0
  return 0;
}

// Fill the answer fields from the model's JSON reply. Unknown or missing fields keep their defaults.
// Tolerates code fences or text around the JSON object. Returns false if no JSON object was found.
bool parseAiAnswer(const String& content, GPTResponse& out) {
  int jsonStart = content.indexOf('{');
  int jsonEnd = content.lastIndexOf('}');
  JsonDocument doc;
  DeserializationError err = DeserializationError::InvalidInput;
  if (jsonStart >= 0 && jsonEnd > jsonStart) {
    err = deserializeJson(doc, content.substring(jsonStart, jsonEnd + 1));
  }

  if (!err) {
    out.answer       = jsonText(doc["answer"]);
    out.imageQuery   = jsonText(doc["image_query"]);
    out.model        = jsonText(doc["model"]);
    out.createImage  = jsonFlag(doc["create_image"]);
    out.isWolfram    = jsonFlag(doc["is_wolfram"]);
    out.isAudio      = jsonFlag(doc["is_audio"]);
    out.isSetting    = jsonFlag(doc["is_setting"]);
    out.settingKey   = jsonText(doc["setting_key"]);
    out.settingValue = jsonText(doc["setting_value"]);
    if (out.settingValue.length() == 0 && !doc["setting_value"].isNull()) {
      serializeJson(doc["setting_value"], out.settingValue);  // numbers/bools
    }
    out.setTimer     = jsonFlag(doc["set_timer"]);
    out.timerMinutes = jsonInt(doc["timer_minutes"]);
    out.timerLabel   = jsonText(doc["timer_label"]);
    out.listTimers   = jsonFlag(doc["list_timers"]);
    out.clearTimer   = jsonFlag(doc["clear_timer"]);
    out.isOnboarding = jsonFlag(doc["is_onboarding"]);
  } else {
    Serial.printf("AI JSON parse error: %s\n", err.c_str());
  }
  return !err;
}
