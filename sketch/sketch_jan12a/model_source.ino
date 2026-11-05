// 3D view: fetches real model data. The AI only names what to look up; if the source has
// nothing, there is no model and the caller falls back to the picture.

static const unsigned long MODEL_LOOKUP_DEADLINE_MS = 10000;
static const size_t MODEL_BODY_LIMIT = 64 * 1024;

// Collects a response in a fixed buffer, whatever the transfer encoding. Refuses to grow past it.
class ModelBodyWriter : public Stream {
 public:
  ModelBodyWriter(char* buffer, size_t capacity) : buffer_(buffer), capacity_(capacity) {}
  size_t write(uint8_t b) override { return write(&b, 1); }
  size_t write(const uint8_t* data, size_t size) override {
    if (length_ + size > capacity_) {
      overflow_ = true;
      return 0;
    }
    memcpy(buffer_ + length_, data, size);
    length_ += size;
    return size;
  }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override {}
  size_t length() const { return length_; }
  bool overflowed() const { return overflow_; }

 private:
  char* buffer_;
  size_t capacity_;
  size_t length_ = 0;
  bool overflow_ = false;
};

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
  if (!http.begin(client, url)) return false;
  lastActivityTime = millis();
  const int code = http.GET();
  bool ok = false;
  if (code == 200) {
    ModelBodyWriter writer(buffer, capacity);
    const int written = http.writeToStream(&writer);
    bodyLength = writer.length();
    ok = written > 0 && !writer.overflowed() && (long)(deadline - millis()) > 0;
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
      if (ok) modelLabel = title;
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
  if (WiFi.status() != WL_CONNECTED) return false;
  if (ref.kind == MODEL_MOLECULE) hasModel = fetchMoleculeFromPubChem(ref.query);
  if (hasModel) {
    modelRefText = String("molecule:") + ref.query;
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
  if (!found) return;
  const unsigned long shown = millis();
  if (!modelViewBegin()) {
    Serial.println("model: no memory for the sprite");
    return;
  }
  mat3Identity(modelRotation);
  const String footer = modelLabel + " - PubChem";
  while (millis() - shown < 4000) {
    mat3Rotate(modelRotation, 0.01f, 0.03f, 0.0f);
    modelViewFrame(footer.c_str());
    yield();
  }
  modelViewEnd();
  redrawPreviousResult();
}
