// 3D view: fetches real model data. The AI only names what to look up; if the source has
// nothing, there is no model and the caller falls back to the picture.


String eduUrlEncode(const char* s) {
  String out;
  char hex[4];
  for (; *s; s++) {
    const unsigned char c = (unsigned char)*s;
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out += (char)c;
    } else {
      snprintf(hex, sizeof(hex), "%%%02X", c);
      out += hex;
    }
  }
  return out;
}

// GET into buffer. True only for HTTP 200 with a complete body inside the deadline.
bool eduHttpGet(const String& url, unsigned long deadline, char* buffer, size_t capacity, size_t& bodyLength) {
  bodyLength = 0;
  const long remaining = (long)(deadline - millis());
  if (remaining <= 500) return false;
  WiFiClientSecure client;
  client.setInsecure();
  client.setHandshakeTimeout(max(1L, remaining / 1000));
  HTTPClient http;
  http.setConnectTimeout(remaining);
  http.setTimeout(remaining);
  http.setUserAgent("Zitro");
  http.useHTTP10(true);  // No chunked replies, so the body can be read directly
  if (!http.begin(client, url)) return false;
  lastActivityTime = millis();
  const int code = http.GET();
  bool ok = false;
  if (code == 200) {
    // Read by hand: the library's own read can wait far beyond any deadline if the server stalls
    const int expected = http.getSize();  // -1 if unknown: then the end of the connection ends it
    WiFiClient* stream = http.getStreamPtr();
    bool overflow = false;
    while ((long)(deadline - millis()) > 0) {
      const int available = stream->available();
      if (available > 0) {
        if (bodyLength + available > capacity) {
          overflow = true;
          break;
        }
        const int got = stream->read((uint8_t*)buffer + bodyLength, available);
        if (got > 0) bodyLength += got;
      } else if (!stream->connected()) {
        break;
      } else {
        delay(5);
      }
      if (expected >= 0 && (int)bodyLength >= expected) break;
    }
    const bool inTime = (long)(deadline - millis()) > 0;
    ok = !overflow && inTime && bodyLength > 0 && (expected < 0 || (int)bodyLength == expected);
  } else {
    Serial.printf("Model lookup: HTTP %d\n", code);
  }
  http.end();
  lastActivityTime = millis();
  return ok;
}

// Looks up a result of any kind. On failure there is none, and the caller shows the picture.
String modelAnswerText;

bool fetchModel(const ModelRef& ref) {
  hasModel = false;
  modelAnswerText = "";
  modelViewEnd();  // TLS needs the memory, and the buffers fit the old result
  eduUnloadActive();
  modelRefText = "";
  modelLabel = "";
  const EduFeature* feature = nullptr;
  for (int i = 0; i < EDU_FEATURE_COUNT; i++) {
    if (strcmp(EDU_FEATURES[i]->kind, ref.kind) == 0) feature = EDU_FEATURES[i];
  }
  if (!feature || !feature->fetch) return false;
  modelSourceName = feature->source;
  // A slight tilt from the start, so it reads as 3D even before the first turn
  mat3Identity(modelRotation);
  mat3Rotate(modelRotation, 0.35f, 0.5f, 0.0f);
  modelZoomStep = 0;
  if (!feature->fetch(ref.query)) {
    if (feature->unload) feature->unload();
    modelLabel = "";
    return false;
  }
  activeFeature = feature;
  hasModel = true;
  modelRefText = String(ref.kind) + ":" + ref.query;
  return true;
}

// Frees what the current result loaded; it's being replaced or dropped
void eduUnloadActive() {
  if (activeFeature && activeFeature->unload) activeFeature->unload();
  activeFeature = nullptr;
}

// Serial "model <kind:query>": lookup and view, without voice or AI
void runModelCommand(const String& text) {
  ModelRef ref;
  if (!parseModelRef(text.c_str(), ref)) {
    Serial.println("model: not a valid reference");
    return;
  }
  resetToActiveState();  // Offline results don't touch the sleep timers otherwise, which misleads tests
  const unsigned long start = millis();
  const bool found = fetchModel(ref);
  Serial.printf("model %s: %s in %lu ms, label '%s'\n", text.c_str(), found ? "found" : "not available", millis() - start,
                modelLabel.c_str());
  if (!found) {
    if (currentDisplayMode == SHOW_MODEL) leaveModelView();
    redrawPreviousResult();
    return;
  }
  response = modelLabel;
  currentDisplayMode = SHOW_MODEL;
  modelViewShow();
}

// A shake's surprise: a built-in result, opened like an answer, so it's also what deep sleep keeps.
// False if it didn't open; the caller shows a fact instead.
bool openShakeMedia() {
  char text[80];
  if (!shakePickMedia((uint32_t)random(0, 1 << 30), (uint32_t)random(0, 1 << 30), text, sizeof(text))) return false;
  ModelRef ref;
  if (!parseModelRef(text, ref)) return false;
  hasImage = false;
  currentImageUrl = "";
  isWolframImage = false;
  if (!fetchModel(ref)) {
    Serial.printf("Shake: %s didn't open\n", text);
    return false;
  }
  Serial.printf("Shake: %s, label '%s'\n", text, modelLabel.c_str());
  // Button C shows this text: the computed time for the clock, otherwise the name of what's shown
  response = wordWrap(modelAnswerText.length() > 0 ? modelAnswerText : modelLabel, 25);
  M5.Display.setRotation(getLandscapeRotation());
  imageZoomMode = CONTAIN;
  currentDisplayMode = SHOW_MODEL;
  modelViewShow();
  resetToActiveState();
  return true;
}
