// 3D view: fetches real model data. The AI only names what to look up; if the source has
// nothing, there is no model and the caller falls back to the picture.

static const unsigned long MODEL_LOOKUP_DEADLINE_MS = 10000;
static const size_t MODEL_BODY_LIMIT = 64 * 1024;
static const unsigned int MODEL_LABEL_LIMIT = 100;

static String modelUrlEncode(const char* s) {
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
static bool modelHttpGet(const String& url, unsigned long deadline, char* buffer, size_t capacity, size_t& bodyLength) {
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

// Fills currentMolecule and modelLabel from PubChem. False on anything unexpected.
static bool fetchMoleculeFromPubChem(const char* name) {
  const unsigned long deadline = millis() + MODEL_LOOKUP_DEADLINE_MS;
  char* body = (char*)ps_malloc(MODEL_BODY_LIMIT);
  if (!body) return false;
  if (!currentMolecule) currentMolecule = (ModelMolecule*)malloc(sizeof(ModelMolecule));
  if (!currentMolecule) {
    free(body);
    return false;
  }
  const String base = "https://" + modelPubChemHost + "/rest/pug/compound/name/" + modelUrlEncode(name);
  size_t length = 0;
  bool ok = modelHttpGet(base + "/SDF?record_type=3d", deadline, body, MODEL_BODY_LIMIT, length) &&
            parseSdf(body, length, *currentMolecule);
  if (ok) {
    // The label comes from the record, so a wrong lookup is visible on screen
    ok = modelHttpGet(base + "/property/Title/JSON", deadline, body, MODEL_BODY_LIMIT, length);
    if (ok) {
      JsonDocument doc;
      ok = !deserializeJson(doc, body, length);
      const char* title = ok ? (doc["PropertyTable"]["Properties"][0]["Title"] | "") : "";
      ok = ok && title[0];
      if (ok) {
        // A label fits one line; a longer one can only come from a broken or hostile server
        modelLabel = title;
        if (modelLabel.length() > MODEL_LABEL_LIMIT) {
          int cut = MODEL_LABEL_LIMIT;
          while (cut > 0 && ((uint8_t)modelLabel[cut] & 0xC0) == 0x80) cut--;
          modelLabel.remove(cut);
        }
      }
    }
  }
  free(body);
  Serial.printf("Model lookup '%s': %s, %u atoms, %ld ms left\n", name, ok ? "found" : "not available",
                ok ? currentMolecule->atomCount : 0, (long)(deadline - millis()));
  return ok;
}

// Looks up a model. On failure there is no model.
bool fetchModel(const ModelRef& ref) {
  hasModel = false;
  modelViewEnd();  // TLS needs the memory, and the buffers fit the old model
  if (WiFi.status() != WL_CONNECTED) return false;
  if (ref.kind == MODEL_MOLECULE) hasModel = fetchMoleculeFromPubChem(ref.query);
  if (hasModel) {
    modelRefText = String("molecule:") + ref.query;
    // A slight tilt from the start, so it reads as 3D even before the first turn
    mat3Identity(modelRotation);
    mat3Rotate(modelRotation, 0.35f, 0.5f, 0.0f);
    modelZoomStep = 0;
  } else {
    modelRefText = "";
    modelLabel = "";
  }
  return hasModel;
}

// Serial "model <kind:query>": lookup and view, without voice or AI
void runModelCommand(const String& text) {
  ModelRef ref;
  if (!parseModelRef(text.c_str(), ref)) {
    Serial.println("model: not a valid reference");
    return;
  }
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
