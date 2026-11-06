// AI provider layer - see ai_provider.h
// Settings lookup and the network side of chat requests. Request bodies and response parsing
// live in ai_payload.ino, the sliding-window memory in conversation_memory.ino.
// Concatenated after sketch_jan12a.ino by the Arduino build, so globals from there are visible.

// =============== SETTINGS ACCESS ===============

static String aiPref(const char* key, const char* def = "") {
  Preferences p;
  p.begin("settings", true);
  String v = p.getString(key, def);
  p.end();
  v.trim();
  return v;
}

String aiProvider() {
  String p = aiPref("provider", AI_PROVIDER_DEFAULT);
  if (p != AI_PROVIDER_ZITRO && p != AI_PROVIDER_OPENAI && p != AI_PROVIDER_ANTHROPIC &&
      p != AI_PROVIDER_INFOMANIAK && p != AI_PROVIDER_CUSTOM) {
    p = AI_PROVIDER_DEFAULT;
  }
  return p;
}

static String withDefault(const String& value, const char* def) {
  return value.length() > 0 ? value : String(def);
}

static ChatConfig chatConfig() {
  ChatConfig c;
  c.provider = aiProvider();
  c.anthropic = false;
  c.openaiStyle = false;

  if (c.provider == AI_PROVIDER_ZITRO) {
    c.url = "https://" ZITRO_RELAY_HOST "/v1/chat/completions";
    c.key = aiPref("zitroKey");
    c.model = DEFAULT_ZITRO_MODEL;
    c.openaiStyle = true;
  } else if (c.provider == AI_PROVIDER_ANTHROPIC) {
    c.url = "https://" ANTHROPIC_HOST "/v1/messages";
    c.key = aiPref("claudeKey");
    c.model = withDefault(aiPref("claudeModel"), DEFAULT_CLAUDE_MODEL);
    c.anthropic = true;
  } else if (c.provider == AI_PROVIDER_INFOMANIAK) {
    String product = aiPref("ikProduct");
    c.url = product.length() > 0
      ? "https://" INFOMANIAK_HOST "/2/ai/" + product + "/openai/v1/chat/completions"
      : "";
    c.key = aiPref("ikKey");
    c.model = withDefault(aiPref("ikModel"), DEFAULT_INFOMANIAK_MODEL);
  } else if (c.provider == AI_PROVIDER_CUSTOM) {
    String base = aiPref("customUrl");
    while (base.endsWith("/")) base.remove(base.length() - 1);
    c.url = base.startsWith("https://") ? base + "/chat/completions" : "";
    c.key = aiPref("customKey");
    c.model = aiPref("customModel");
  } else {
    c.url = "https://" OPENAI_HOST "/v1/chat/completions";
    c.key = aiPref("apiKey");
    c.model = withDefault(aiPref("oaModel"), DEFAULT_OPENAI_MODEL);
    c.openaiStyle = true;
  }
  return c;
}

// True when the selected chat provider has everything it needs
bool aiChatConfigured() {
  ChatConfig c = chatConfig();
  return c.url.length() > 0 && c.key.length() > 0 && c.model.length() > 0;
}

// Speech-to-text, text-to-speech and image generation use the OpenAI API.
// Zitro subscribers get them through the relay; everyone else needs an OpenAI key,
// because Claude, Infomaniak and most compatible providers offer no such endpoints.
AudioEndpoint audioEndpoint() {
  AudioEndpoint e;
  e.available = false;
  if (aiProvider() == AI_PROVIDER_ZITRO) {
    e.host = ZITRO_RELAY_HOST;
    e.key = aiPref("zitroKey");
  } else {
    e.host = OPENAI_HOST;
    e.key = aiPref("apiKey");
  }
  e.available = e.key.length() > 0;
  return e;
}


// =============== CHAT ===============

// Send one chat request to the selected provider.
// withMemory: include the remembered turns of the last 10 minutes.
// jsonMode: ask OpenAI-style providers for a JSON object response.
ChatResult aiChat(const String& systemPrompt, const String& userText, int maxTokens, bool withMemory, bool jsonMode) {
  ChatResult result = {false, "", ""};
  ChatConfig cfg = chatConfig();

  if (cfg.url.length() == 0 || cfg.key.length() == 0) {
    result.error = "No AI key";
    return result;
  }

  if (WiFi.status() != WL_CONNECTED && !reconnectWiFiNow()) {
    result.error = "No WiFi";
    return result;
  }

  bool useFallback = false;
  String body = buildChatRequest(cfg, systemPrompt, userText, maxTokens, withMemory, jsonMode, useFallback);

  Serial.printf("AI chat -> %s (%s), %d bytes\n", cfg.provider.c_str(), cfg.model.c_str(), body.length());

  WiFiClientSecure client;
  client.setInsecure();  // TODO: pin provider CAs (see docs/ZITRO_RELAY_DESIGN.md)
  HTTPClient http;

  int httpCode = -1;
  for (int attempt = 0; attempt < 2; attempt++) {
    http.begin(client, cfg.url);
    http.addHeader("Content-Type", "application/json");
    if (cfg.anthropic) {
      http.addHeader("x-api-key", cfg.key);
      http.addHeader("anthropic-version", ANTHROPIC_VERSION);
      if (useFallback) http.addHeader("anthropic-beta", "server-side-fallback-2026-07-01");
    } else {
      http.addHeader("Authorization", "Bearer " + cfg.key);
    }
    http.setTimeout(90000);
    httpCode = http.POST(body);
    Serial.printf("AI chat HTTP %d\n", httpCode);
    if (httpCode >= 0) break;

    http.end();
    if (attempt == 0 && !reconnectWiFiNow()) break;
  }

  if (httpCode < 0) {
    http.end();
    result.error = "Connection Error";
    return result;
  }

  String resp = http.getString();
  http.end();

  if (httpCode != 200) Serial.println("AI error body: " + resp.substring(0, 400));
  return parseChatResponse(httpCode, resp, cfg);
}
