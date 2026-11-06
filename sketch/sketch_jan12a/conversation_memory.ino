// Sliding-window conversation memory - see ai_provider.h
// Kept in RTC slow memory: survives light and deep sleep, lost on power-off, never written to flash.

struct MemoryTurn {
  uint32_t timestamp;          // epoch seconds
  char question[MEMORY_Q_LEN];
  char answer[MEMORY_A_LEN];
};

// RTC slow memory: kept across light and deep sleep, lost on power-off, never in flash
RTC_DATA_ATTR static MemoryTurn memoryTurns[MEMORY_MAX_TURNS];
RTC_DATA_ATTR static uint8_t memoryCount = 0;

static bool memoryClockValid(time_t now) {
  return now > 1700000000;  // NTP synced (after Nov 2023)
}

bool memoryEnabled() {
  Preferences p;
  p.begin("settings", true);
  bool on = p.getBool("memory", true);
  p.end();
  return on;
}

void memoryClear() {
  memoryCount = 0;
  memset(memoryTurns, 0, sizeof(memoryTurns));
  Serial.println("Conversation memory cleared");
}

// Copy into a fixed buffer without cutting a UTF-8 sequence in half
static void copyUtf8(char* dst, size_t dstSize, const String& src) {
  size_t n = src.length();
  if (n >= dstSize) {
    n = dstSize - 1;
    while (n > 0 && (src[n] & 0xC0) == 0x80) n--;
  }
  memcpy(dst, src.c_str(), n);
  dst[n] = '\0';
}

// Drop turns older than the window (or everything if the clock is unknown)
static void memoryPrune() {
  time_t now = time(nullptr);
  if (!memoryClockValid(now)) {
    memoryCount = 0;
    return;
  }
  int keepFrom = 0;
  while (keepFrom < memoryCount &&
         (uint32_t)now - memoryTurns[keepFrom].timestamp > MEMORY_WINDOW_SECONDS) {
    keepFrom++;
  }
  if (keepFrom > 0) {
    for (int i = keepFrom; i < memoryCount; i++) memoryTurns[i - keepFrom] = memoryTurns[i];
    memoryCount -= keepFrom;
  }
}

void memoryAdd(const String& question, const String& answer) {
  if (!memoryEnabled() || question.length() == 0 || answer.length() == 0) return;
  time_t now = time(nullptr);
  if (!memoryClockValid(now)) return;

  memoryPrune();
  if (memoryCount == MEMORY_MAX_TURNS) {
    for (int i = 1; i < MEMORY_MAX_TURNS; i++) memoryTurns[i - 1] = memoryTurns[i];
    memoryCount--;
  }
  MemoryTurn& t = memoryTurns[memoryCount++];
  t.timestamp = (uint32_t)now;
  copyUtf8(t.question, sizeof(t.question), question);
  copyUtf8(t.answer, sizeof(t.answer), answer);
  Serial.printf("Memory: %d turn(s) in the last %d min\n", memoryCount, MEMORY_WINDOW_SECONDS / 60);
}

// Append remembered turns as alternating user/assistant messages.
// Assistant turns are replayed as the JSON shape the model is asked to produce.
void memoryAppendMessages(JsonArray messages) {
  if (!memoryEnabled()) return;
  memoryPrune();
  for (int i = 0; i < memoryCount; i++) {
    JsonObject u = messages.add<JsonObject>();
    u["role"] = "user";
    u["content"] = memoryTurns[i].question;

    JsonDocument answerDoc;
    answerDoc["answer"] = memoryTurns[i].answer;
    String answerJson;
    serializeJson(answerDoc, answerJson);
    JsonObject a = messages.add<JsonObject>();
    a["role"] = "assistant";
    a["content"] = answerJson;
  }
}
