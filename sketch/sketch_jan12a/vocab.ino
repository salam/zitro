// The vocabulary trainer: a child practises, spells or prints a unit of words their parents uploaded to
// zitro.spot/voci. vocab_core.h decides what is right; this file draws, records, fetches and keeps the round.
// While it runs, loop() hands over right after the timer blocks (like the guided tour), so Button A answers.
// Design: docs/superpowers/specs/2026-11-13-vocab-trainer-design.md

#include "vocab_core.h"

// ---------- Constants ----------

static const char* const VOCAB_API_DEFAULT = "https://zitro.spot/api/voci.php";
static const char* const VOCAB_PAGE_URL = "https://zitro.spot/voci/#";
static const size_t VOCAB_BODY_LIMIT = 64 * 1024;
static const unsigned long VOCAB_FETCH_MS = 10000;
static const int VOCAB_SPELL_RATE = 16000;
static const int VOCAB_SPELL_SECONDS = 15;
static const int VOCAB_MAX_REMEMBERED = 50;  // lists whose round state is kept in Preferences
static const uint16_t VOCAB_OK_GREEN = 0x2D0B;   // #2fa35a
static const uint16_t VOCAB_BAD_RED = 0xE227;    // #e0453a
static const uint16_t VOCAB_MISS_ORANGE = 0xF440;  // #f28b05
static const uint16_t VOCAB_GREY = 0x9CD3;
// A prompt without a real word: one in the prompt pulled a transcript towards it in the 2026-11-13 test
static const char* const VOCAB_SPELL_PROMPT = "A child spells an English word letter by letter, for example: Q, R, S, T.";
// zitro.spot's certificate chains to ISRG Root X1 (Let's Encrypt, valid until 2035-06-04). The stick's code is a
// secret, so it is only sent over a connection whose certificate checks out.
static const char VOCAB_ROOT_CA[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw\n"
    "TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n"
    "cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4\n"
    "WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu\n"
    "ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY\n"
    "MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc\n"
    "h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+\n"
    "0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U\n"
    "A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW\n"
    "T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH\n"
    "B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC\n"
    "B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv\n"
    "KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn\n"
    "OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn\n"
    "jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw\n"
    "qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI\n"
    "rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV\n"
    "HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq\n"
    "hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL\n"
    "ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ\n"
    "3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK\n"
    "NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5\n"
    "ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur\n"
    "TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC\n"
    "jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc\n"
    "oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq\n"
    "4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA\n"
    "mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d\n"
    "emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=\n"
    "-----END CERTIFICATE-----\n";
static const char* const VOCAB_SAY_ENGLISH =
    "Say this English word or phrase clearly in British English, slowly, for a child learning English.";

enum VocabScreen : uint8_t {
  VOCAB_SCR_OFF,
  VOCAB_SCR_QR,       // upload code, also "no lists" and "not found"
  VOCAB_SCR_MESSAGE,  // a short note; any button leaves the trainer
  VOCAB_SCR_CHOOSE,   // which book or which unit
  VOCAB_SCR_START,
  VOCAB_SCR_CARD,
  VOCAB_SCR_RESULT,
  VOCAB_SCR_END,
  VOCAB_SCR_MENU,
};

enum VocabResultKind : uint8_t { VOCAB_RES_RIGHT, VOCAB_RES_WRONG, VOCAB_RES_CLOSE, VOCAB_RES_UNSURE, VOCAB_RES_SPELLED };

enum VocabMenuItem : uint8_t { VOCAB_MENU_GO, VOCAB_MENU_DIRECTION, VOCAB_MENU_SPELL, VOCAB_MENU_PRINT, VOCAB_MENU_END };

struct VocabEntry {
  uint32_t t, b;  // offsets of the trained-language side and the German side in vocabPool
};

// ---------- State ----------

static bool vocabOn = false;
static VocabScreen vocabScreen = VOCAB_SCR_OFF;
static VocabScreen vocabMenuReturn = VOCAB_SCR_CARD;
static bool vocabGerman = true;
static bool vocabSpelling = false;     // round type: spelling instead of saying
static bool vocabEnglishFirst = false;  // direction: false = German shown, English answered
static uint32_t vocabLastPress = 0;
static bool vocabWasAsleep = false;
static String vocabApiUrl = VOCAB_API_DEFAULT;  // "vocabhost <url>" points it at a test server
static String vocabTyped;                       // "vocabtype <text>" stands in for the next recording

static std::vector<VocabListRef> vocabLists;
static std::vector<int> vocabListCounts;
static std::string vocabCurrentBook;
static std::vector<int> vocabChoices;
static bool vocabChoosingBooks = true;
static int vocabChooseSel = 0;
static String vocabPendingMode = "practice";

static String vocabListId, vocabBook, vocabName;
static char* vocabPool = nullptr;
static VocabEntry* vocabEntries = nullptr;
static uint16_t vocabCount = 0;

static std::vector<uint16_t> vocabQueue;       // cards of this round, wrong ones appended once more
static size_t vocabPos = 0;
static size_t vocabRoundSize = 0;
static std::vector<uint16_t> vocabWrongNow;    // wrong at the first attempt this round
static std::vector<uint16_t> vocabRequeued;
static int vocabRightFirst = 0;

static VocabResultKind vocabResult = VOCAB_RES_RIGHT;
static String vocabHeard;
static std::vector<VocabCell> vocabCells;
static String vocabMessage, vocabMessage2;
static String vocabQrTitle;
static int vocabMenuSel = 0;

bool vocabSheetPending = false;

static const char* vt(const char* de, const char* en) {
  return vocabGerman ? de : en;
}

bool vocabActive() {
  return vocabOn && !vocabSheetPending;  // while the sheet prints, the print menu has the buttons
}

// ---------- PSRAM for JSON ----------

struct VocabSpiRamAllocator : ArduinoJson::Allocator {
  void* allocate(size_t size) override { return ps_malloc(size); }
  void deallocate(void* p) override { free(p); }
  void* reallocate(void* p, size_t n) override { return ps_realloc(p, n); }
};
static VocabSpiRamAllocator vocabAllocator;

// ---------- The stick's code ----------

static String vocabCode() {
  Preferences p;
  p.begin("voci", false);
  String code = p.getString("code", "");
  if (code.length() != 26) {
    uint8_t bytes[16];
    for (int i = 0; i < 16; i += 4) {
      uint32_t r = esp_random();
      memcpy(bytes + i, &r, 4);
    }
    code = vocabBase32(bytes, 16).c_str();
    p.putString("code", code);
    Serial.println("Vocab: new stick code made");
  }
  p.end();
  return code;
}

// ---------- Entries ----------

static const char* vocabSide(uint16_t i, bool trained) {
  if (!vocabPool || i >= vocabCount) return "";
  return vocabPool + (trained ? vocabEntries[i].t : vocabEntries[i].b);
}

// What the child sees on the card, and what the answer is checked against
static const char* vocabPromptSide(uint16_t i) { return vocabSide(i, vocabEnglishFirst); }
static const char* vocabAnswerSide(uint16_t i) { return vocabSide(i, !vocabEnglishFirst || vocabSpelling); }

static void vocabFreeList() {
  free(vocabPool);
  free(vocabEntries);
  vocabPool = nullptr;
  vocabEntries = nullptr;
  vocabCount = 0;
}

// For the screen and the voice: "(pl.)" and the like left out, "…" read as a pause
static String vocabShown(const char* side) {
  String out;
  int depth = 0;
  for (const char* p = side; *p; p++) {
    if (*p == '(') depth++;
    else if (*p == ')') { if (depth > 0) depth--; }
    else if (depth == 0) out += *p;
  }
  if (out.length() == 0) out = side;
  // Signs the screen and print fonts lack (they cover U+0020 to U+05FF)
  out.replace("\xE2\x80\xA6", "...");
  out.replace("\xE2\x80\x98", "'");
  out.replace("\xE2\x80\x99", "'");
  out.replace("\xE2\x80\x9C", "\"");
  out.replace("\xE2\x80\x9D", "\"");
  out.replace("\xE2\x80\x93", "-");
  out.replace("\xE2\x80\x94", "-");
  while (out.indexOf("  ") >= 0) out.replace("  ", " ");
  out.trim();
  return out;
}

// ---------- Network ----------

static char* vocabFetch(const String& query, size_t& length) {
  char* body = (char*)ps_malloc(VOCAB_BODY_LIMIT);
  if (!body) return nullptr;
  const String code = vocabCode();
  // Plain http:// only reaches a test server set over the serial console ("vocabhost")
  const bool ok = eduHttpGetWithHeader(vocabApiUrl + "?" + query, millis() + VOCAB_FETCH_MS, body, VOCAB_BODY_LIMIT - 1,
                                       length, "X-Voci-Code", code.c_str(), VOCAB_ROOT_CA);
  if (!ok) {
    Serial.printf("Vocab: %s failed\n", query.c_str());
    free(body);
    return nullptr;
  }
  body[length] = 0;
  return body;
}

static bool vocabLoadIndex() {
  vocabLists.clear();
  vocabListCounts.clear();
  vocabCurrentBook.clear();
  size_t length = 0;
  char* body = vocabFetch("op=index", length);
  if (!body) return false;
  JsonDocument doc(&vocabAllocator);
  const bool ok = !deserializeJson(doc, body, length);
  free(body);
  if (!ok) return false;
  vocabCurrentBook = (const char*)(doc["current"] | "");
  for (JsonObject l : doc["lists"].as<JsonArray>()) {
    vocabLists.push_back({(const char*)(l["id"] | ""), (const char*)(l["book"] | ""), (const char*)(l["name"] | "")});
    vocabListCounts.push_back(l["count"] | 0);
  }
  Serial.printf("Vocab: %d lists, current book '%s'\n", (int)vocabLists.size(), vocabCurrentBook.c_str());
  return true;
}

// Loads one list into PSRAM: all text in one pool, entries as offsets
static bool vocabLoadList(int index) {
  vocabFreeList();
  if (index < 0 || index >= (int)vocabLists.size()) return false;
  size_t length = 0;
  char* body = vocabFetch("op=list&id=" + String(vocabLists[index].id.c_str()), length);
  if (!body) return false;
  JsonDocument doc(&vocabAllocator);
  const bool ok = !deserializeJson(doc, body, length);
  free(body);
  if (!ok) return false;
  JsonArray entries = doc["entries"].as<JsonArray>();
  size_t poolSize = 1, n = 0;
  for (JsonObject e : entries) {
    poolSize += strlen(e["t"] | "") + strlen(e["b"] | "") + 2;
    n++;
  }
  if (n == 0 || n > 500) return false;
  vocabPool = (char*)ps_malloc(poolSize);
  vocabEntries = (VocabEntry*)ps_malloc(n * sizeof(VocabEntry));
  if (!vocabPool || !vocabEntries) {
    vocabFreeList();
    return false;
  }
  size_t at = 0;
  for (JsonObject e : entries) {
    const char* t = e["t"] | "";
    const char* b = e["b"] | "";
    vocabEntries[vocabCount].t = at;
    strcpy(vocabPool + at, t);
    at += strlen(t) + 1;
    vocabEntries[vocabCount].b = at;
    strcpy(vocabPool + at, b);
    at += strlen(b) + 1;
    vocabCount++;
  }
  vocabListId = vocabLists[index].id.c_str();
  vocabBook = vocabLists[index].book.c_str();
  vocabName = vocabLists[index].name.c_str();
  Serial.printf("Vocab: loaded %s / %s, %d entries\n", vocabBook.c_str(), vocabName.c_str(), vocabCount);
  return true;
}

// ---------- Remembered round state (Preferences "voci") ----------

static void vocabRememberId(Preferences& p) {
  String ids = p.getString("ids", "");
  std::vector<String> list;
  int from = 0;
  while (from < (int)ids.length()) {
    int comma = ids.indexOf(',', from);
    if (comma < 0) comma = ids.length();
    String id = ids.substring(from, comma);
    if (id.length() && id != vocabListId) list.push_back(id);
    from = comma + 1;
  }
  list.push_back(vocabListId);
  while ((int)list.size() > VOCAB_MAX_REMEMBERED) {
    p.remove(("w" + list[0]).c_str());
    p.remove(("c" + list[0]).c_str());
    list.erase(list.begin());
  }
  String joined;
  for (auto& id : list) joined += (joined.length() ? "," : "") + id;
  p.putString("ids", joined);
  p.putString("last", vocabListId);
}

static void vocabLoadRoundState(std::vector<uint16_t>& lastWrong, uint16_t& cursor) {
  Preferences p;
  p.begin("voci", true);
  uint16_t wrong[VOCAB_ROUND_SIZE];
  const size_t got = p.getBytes(("w" + vocabListId).c_str(), wrong, sizeof(wrong));
  for (size_t i = 0; i < got / 2; i++) lastWrong.push_back(wrong[i]);
  cursor = p.getUShort(("c" + vocabListId).c_str(), 0);
  p.end();
}

static void vocabSaveCursor(uint16_t cursor) {
  Preferences p;
  p.begin("voci", false);
  p.putUShort(("c" + vocabListId).c_str(), cursor);
  vocabRememberId(p);
  p.end();
}

static void vocabSaveWrong() {
  Preferences p;
  p.begin("voci", false);
  uint16_t wrong[VOCAB_ROUND_SIZE];
  size_t n = 0;
  for (auto i : vocabWrongNow)
    if (n < VOCAB_ROUND_SIZE) wrong[n++] = i;
  if (n) p.putBytes(("w" + vocabListId).c_str(), wrong, n * 2);
  else p.remove(("w" + vocabListId).c_str());
  p.end();
}

static String vocabLastListId() {
  Preferences p;
  p.begin("voci", true);
  String id = p.getString("last", "");
  p.end();
  return id;
}

// ---------- Drawing helpers ----------

static bool vocabOpenCanvas(M5Canvas& c) {
  M5.Display.setRotation(getLandscapeRotation());
  c.setPsram(true);
  if (!c.createSprite(WIDTH, HEIGHT)) {
    Serial.println("Vocab: no memory for the screen");
    return false;
  }
  c.fillSprite(TFT_BLACK);
  c.setTextColor(TFT_WHITE);
  c.setFreeFont(FontManager::getFont12pt());
  return true;
}

static void vocabPush(M5Canvas& c) {
  captureCanvas(c);
  c.pushSprite(0, 0);
  c.deleteSprite();
}

static void vocabTabs(M5Canvas& c, int kindA, int kindB, int kindC) {
  drawButtonHints(c, M5.Display.getRotation(), kindA, kindB, kindC);
}

static void vocabCentered(M5Canvas& c, const String& text, int y, uint16_t color) {
  c.setTextColor(color);
  c.setTextDatum(MC_DATUM);
  c.drawString(text, WIDTH / 2, y);
}

// The word large: 14 pt if it fits, else 10 pt, else two lines of 10 pt
static void vocabBigText(M5Canvas& c, const String& text, int centerY, uint16_t color) {
  c.setTextColor(color);
  c.setTextDatum(MC_DATUM);
  c.setFreeFont(FontManager::getFont16pt());
  if (c.textWidth(text) <= 196) {
    c.drawString(text, WIDTH / 2, centerY);
    return;
  }
  c.setFreeFont(FontManager::getFont12pt());
  if (c.textWidth(text) <= 196) {
    c.drawString(text, WIDTH / 2, centerY);
    return;
  }
  String lines[2];
  const int n = wrapToLines(c, text, 196, lines, 2);
  c.setTextDatum(MC_DATUM);
  for (int i = 0; i < n; i++) c.drawString(lines[i], WIDTH / 2, centerY - (n - 1) * 11 + i * 22);
}

// "Press [M5] and ..." centered
static void vocabPressRow(M5Canvas& c, const char* before, const char* after, int cy) {
  c.setFreeFont(FontManager::getFont12pt());
  const String a = String(before) + " ";
  const String b = String(" ") + after;
  const int capW = drawKeycap(c, "M5", 0, 0, true);
  c.setFreeFont(FontManager::getFont12pt());
  int x = (WIDTH - (c.textWidth(a) + capW + c.textWidth(b))) / 2;
  c.setTextColor(TFT_WHITE);
  c.setTextDatum(ML_DATUM);
  c.drawString(a, x, cy);
  x += c.textWidth(a);
  drawKeycap(c, "M5", x, cy);
  x += capW;
  c.setFreeFont(FontManager::getFont12pt());
  c.setTextColor(TFT_WHITE);
  c.setTextDatum(ML_DATUM);
  c.drawString(b, x, cy);
}

// Header of a card: right of Button C's tab on top, or at the bottom when the stick is turned the other way
static void vocabHeader(M5Canvas& c) {
  const bool top = M5.Display.getRotation() == 3;
  const int y = top ? 8 : HEIGHT - 9;
  const int x0 = top ? 48 : 8;
  c.setFont(&fonts::DejaVu12);
  c.setTextDatum(ML_DATUM);
  c.setTextColor(0xDEFB);
  c.drawString(vocabName, x0, y);
  int x = x0 + c.textWidth(vocabName) + 10;
  const size_t n = vocabRoundSize;
  const size_t done = vocabPos < n ? vocabPos : n;
  for (size_t k = 0; k < n && x < WIDTH - 50; k++, x += 7) {
    const bool now = k == done;
    c.fillCircle(x, y, now ? 3 : 2, k < done ? CITRUS : now ? TFT_WHITE : 0x52AA);
  }
}

static void vocabBoxes(M5Canvas& c, const std::vector<VocabCell>& cells, int y, size_t shown, bool marked) {
  const int n = cells.size();
  if (n == 0) return;
  int bw = min(20, 212 / n - 2);
  if (bw < 8) bw = 8;
  const int total = n * bw + (n - 1) * 2;
  int x = (WIDTH - total) / 2;
  c.setFreeFont(FontManager::getFont12pt());
  c.setTextDatum(MC_DATUM);
  for (int i = 0; i < n; i++, x += bw + 2) {
    const bool filled = (size_t)i < shown;
    uint16_t edge = 0x7BEF, ink = TFT_WHITE;
    if (filled && marked) {
      const VocabMark m = cells[i].mark;
      edge = m == VOCAB_OK ? VOCAB_OK_GREEN : m == VOCAB_MISS ? VOCAB_MISS_ORANGE : VOCAB_BAD_RED;
      ink = m == VOCAB_OK ? TFT_WHITE : edge;
      c.fillRoundRect(x, y, bw, 24, 3, 0x18E3);
      c.drawRoundRect(x, y, bw, 24, 3, edge);
      c.drawRoundRect(x + 1, y + 1, bw - 2, 22, 2, edge);
    } else {
      c.drawRoundRect(x, y, bw, 24, 3, filled ? TFT_WHITE : 0x7BEF);
    }
    if (filled && cells[i].ch != ' ') {
      c.setTextColor(ink);
      char s[2] = {cells[i].ch, 0};
      c.drawString(s, x + bw / 2, y + 12);
    }
  }
}

static std::vector<VocabCell> vocabBlankCells(const std::string& word) {
  std::vector<VocabCell> cells;
  for (char ch : word) cells.push_back({(ch == '-' || ch == '\'') ? ch : ' ', VOCAB_OK});
  return cells;
}

// ---------- Screens ----------

static void vocabDrawQr() {
  M5Canvas c(&M5.Display);
  if (!vocabOpenCanvas(c)) return;
  const String code = vocabCode();
  const int qrSize = 99, qrX = WIDTH - qrSize - 8, qrY = (HEIGHT - qrSize) / 2;
  c.fillRect(qrX - 4, qrY - 4, qrSize + 8, qrSize + 8, TFT_WHITE);
  c.qrcode(String(VOCAB_PAGE_URL) + code, qrX, qrY, qrSize, 4);
  c.setTextDatum(ML_DATUM);
  c.setTextColor(CITRUS);
  String lines[2];
  c.setFreeFont(FontManager::getFont12pt());
  const int n = wrapToLines(c, vocabQrTitle, 120, lines, 2);
  for (int i = 0; i < n; i++) c.drawString(lines[i], 8, 14 + i * 19);
  c.setTextColor(TFT_WHITE);
  const int y = n > 1 ? 58 : 46;
  c.drawString(vt("Scanne den Code", "Scan the code"), 8, y);
  c.drawString(vt("mit dem Handy:", "with a phone:"), 8, y + 19);
  c.setTextColor(CITRUS);
  c.drawString("zitro.spot/voci", 8, y + 42);
  c.drawFastHLine(8, y + 52, c.textWidth("zitro.spot/voci"), CITRUS);
  c.setFont(&fonts::DejaVu9);
  c.setTextColor(VOCAB_GREY);
  c.drawString("Stick .." + code.substring(23), 8, HEIGHT - 8);
  vocabPush(c);
}

static void vocabDrawMessage() {
  M5Canvas c(&M5.Display);
  if (!vocabOpenCanvas(c)) return;
  String lines[3];
  const int n = wrapToLines(c, vocabMessage, 210, lines, 3);
  c.setTextDatum(MC_DATUM);
  c.setTextColor(TFT_WHITE);
  for (int i = 0; i < n; i++) c.drawString(lines[i], WIDTH / 2, 50 - (n - 1) * 11 + i * 22);
  if (vocabMessage2.length()) vocabCentered(c, vocabMessage2, 100, CITRUS);
  vocabPush(c);
}

static void vocabDrawBusy(const char* text) {
  M5Canvas c(&M5.Display);
  if (!vocabOpenCanvas(c)) return;
  drawLemon(c, WIDTH / 2, 40);
  vocabCentered(c, text, 76, TFT_WHITE);
  vocabPush(c);
}

static String vocabChoiceLabel(int k) {
  const VocabListRef& l = vocabLists[vocabChoices[k]];
  return vocabChoosingBooks ? String(l.book.c_str()) : String(l.name.c_str());
}

static void vocabDrawChoose() {
  M5Canvas c(&M5.Display);
  if (!vocabOpenCanvas(c)) return;
  const int n = vocabChoices.size();
  const int first = max(0, min(n - 3, vocabChooseSel - 1));
  c.setFont(&fonts::DejaVu12);
  vocabCentered(c, vocabChoosingBooks ? vt("Welches Buch?", "Which book?") : vt("Welche Liste?", "Which list?"), 10,
                0xDEFB);
  c.setFreeFont(FontManager::getFont12pt());
  for (int k = 0; k < 3 && first + k < n; k++) {
    const int y = 22 + k * 33;
    const bool on = first + k == vocabChooseSel;
    if (on) c.fillRoundRect(20, y, 200, 29, 6, TFT_WHITE);
    c.setTextColor(on ? TFT_BLACK : TFT_WHITE);
    c.setTextDatum(ML_DATUM);
    c.drawString(vocabChoiceLabel(first + k), 32, y + 15);
  }
  if (n > 3) {
    c.fillRoundRect(230, 22, 4, 95, 2, 0x31A6);
    c.fillRoundRect(230, 22 + first * 95 / n, 4, 95 * 3 / n, 2, 0xAD55);
  }
  vocabTabs(c, HINT_OK, HINT_CANCEL, HINT_NEXT);
  vocabPush(c);
}

static void vocabDrawStart() {
  M5Canvas c(&M5.Display);
  if (!vocabOpenCanvas(c)) return;
  drawLemon(c, 34, 24);
  c.setFont(&fonts::DejaVu12);
  c.setTextDatum(ML_DATUM);
  c.setTextColor(VOCAB_GREY);
  c.drawString(vocabBook, 52, 16);
  c.setFreeFont(FontManager::getFont16pt());
  c.setTextColor(TFT_WHITE);
  c.drawString(vocabName, 52, 36);
  String what;
  if (vocabSpelling) {
    what = String(vocabRoundSize) + vt(" Wörter buchstabieren", " words to spell");
  } else {
    what = String(vocabRoundSize) + vt(" Wörter · ", " words · ") +
           (vocabEnglishFirst ? vt("auf Deutsch", "in German") : vt("auf Englisch", "in English"));
  }
  c.setFreeFont(FontManager::getFont12pt());
  vocabCentered(c, what, 70, CITRUS);
  vocabPressRow(c, vt("Drück", "Press"), vt("zum Starten", "to start"), 100);
  vocabTabs(c, HINT_OK, HINT_MENU, HINT_NONE);
  vocabPush(c);
}

static uint16_t vocabCurrentCard() {
  return vocabPos < vocabQueue.size() ? vocabQueue[vocabPos] : 0;
}

// The card: only a header and the word (user, 2026-11-13). Spelling adds the empty boxes.
static void vocabDrawCard() {
  M5Canvas c(&M5.Display);
  if (!vocabOpenCanvas(c)) return;
  vocabHeader(c);
  const uint16_t i = vocabCurrentCard();
  const String shown = vocabShown(vocabPromptSide(i));
  if (vocabSpelling) {
    vocabBigText(c, shown, 50, TFT_WHITE);
    std::vector<VocabCell> blank = vocabBlankCells(vocabSpellWord(vocabSide(i, true)));
    vocabBoxes(c, blank, 76, 0, false);
  } else {
    vocabBigText(c, shown, 67, TFT_WHITE);
  }
  vocabTabs(c, HINT_MIC, HINT_MENU, HINT_DONTKNOW);
  vocabPush(c);
}

static void vocabDrawRecording(int secondsDone, int seconds) {
  M5Canvas c(&M5.Display);
  if (!vocabOpenCanvas(c)) return;
  const uint16_t i = vocabCurrentCard();
  String label = vocabShown(vocabPromptSide(i));
  if (vocabSpelling) label += vt(" · buchstabiere!", " · spell it!");
  c.setFreeFont(FontManager::getFont12pt());
  String lines[1];
  wrapToLines(c, label, 220, lines, 1);
  vocabCentered(c, lines[0], 30, TFT_WHITE);
  drawIconScaled(c, micIcon, MICICON_WIDTH, MICICON_HEIGHT, 8, HEIGHT / 2 - MICICON_HEIGHT + 8, 2, TFT_WHITE);
  const int slot = 180 / seconds;
  for (int k = 0; k < seconds; k++) {
    const int x = 52 + k * slot;
    if (k < secondsDone) c.fillRoundRect(x, 70, slot - 4, 14, 3, TFT_WHITE);
    else c.drawRoundRect(x, 70, slot - 4, 14, 3, TFT_WHITE);
  }
  vocabPush(c);
}

static void vocabDrawResult() {
  M5Canvas c(&M5.Display);
  if (!vocabOpenCanvas(c)) return;
  const uint16_t i = vocabCurrentCard();
  const String solution = vocabShown(vocabAnswerSide(i));
  const String prompt = vocabShown(vocabPromptSide(i));
  if (vocabResult == VOCAB_RES_SPELLED) {
    const bool right = vocabAllOk(vocabCells);
    vocabBoxes(c, vocabCells, 22, vocabCells.size(), true);
    c.setFreeFont(FontManager::getFont12pt());
    if (right) {
      vocabCentered(c, prompt, 68, VOCAB_GREY);
      c.fillCircle(WIDTH / 2, 98, 12, VOCAB_OK_GREEN);
      drawCheckMark(c, WIDTH / 2, 98, 12, TFT_WHITE);
    } else {
      String said;
      for (char ch : vocabHeard) {
        if (said.length()) said += "-";
        said += ch;
      }
      vocabCentered(c, vt("Du: ", "You: ") + said, 66, 0xCE59);
      int missing = -1, bad = -1, extra = -1;
      for (size_t k = 0; k < vocabCells.size(); k++) {
        if (vocabCells[k].mark == VOCAB_MISS && missing < 0) missing = k;
        if (vocabCells[k].mark == VOCAB_BAD && bad < 0) bad = k;
        if (vocabCells[k].mark == VOCAB_EXTRA && extra < 0) extra = k;
      }
      String note;
      uint16_t color = VOCAB_BAD_RED;
      if (missing >= 0) {
        note = String("«") + vocabCells[missing].ch + vt("» fehlt", "» is missing");
        color = VOCAB_MISS_ORANGE;
      } else if (bad >= 0) {
        note = String("«") + vocabCells[bad].ch + vt("» ist anders", "» is different");
      } else {
        note = vt("ein Buchstabe zu viel", "one letter too many");
      }
      vocabCentered(c, note, 92, color);
    }
  } else if (vocabResult == VOCAB_RES_RIGHT) {
    c.fillCircle(WIDTH / 2, 30, 12, VOCAB_OK_GREEN);
    drawCheckMark(c, WIDTH / 2, 30, 12, TFT_WHITE);
    vocabBigText(c, solution, 70, TFT_WHITE);
    c.setFreeFont(FontManager::getFont12pt());
    vocabCentered(c, prompt, 102, VOCAB_GREY);
  } else {
    c.setFont(&fonts::DejaVu12);
    int y = 22;
    if (vocabResult == VOCAB_RES_CLOSE) {
      c.setFreeFont(FontManager::getFont12pt());
      vocabCentered(c, vt("Fast!", "Almost!"), y, CITRUS);
      y += 20;
    } else if (vocabResult == VOCAB_RES_UNSURE) {
      vocabCentered(c, vt("Ich bin nicht sicher.", "I'm not sure."), y, VOCAB_GREY);
      y += 18;
    }
    if (vocabHeard.length()) {
      c.setFont(&fonts::DejaVu12);
      vocabCentered(c, vt("Ich habe gehört:", "I heard:"), y, VOCAB_GREY);
      c.setFreeFont(FontManager::getFont12pt());
      String heard[1];
      wrapToLines(c, "«" + vocabHeard + "»", 210, heard, 1);
      vocabCentered(c, heard[0], y + 17, 0xCE59);
      y += 38;
    } else {
      y += 6;
    }
    c.setFont(&fonts::DejaVu12);
    vocabCentered(c, vt("Richtig ist:", "The answer:"), y, VOCAB_GREY);
    vocabBigText(c, solution, y + 24, CITRUS);
    if (vocabResult == VOCAB_RES_WRONG) {
      c.setFont(&fonts::DejaVu9);
      vocabCentered(c, vt("kommt nochmal", "comes again"), HEIGHT - 22, VOCAB_GREY);
    }
  }
  vocabTabs(c, HINT_NONE, HINT_MENU, HINT_NEXT);
  vocabPush(c);
}

static void vocabDrawEnd() {
  M5Canvas c(&M5.Display);
  if (!vocabOpenCanvas(c)) return;
  drawLemon(c, WIDTH / 2, 18);
  c.setFreeFont(FontManager::getFont16pt());
  vocabCentered(c, String(vocabRightFirst) + vt(" von ", " of ") + String(vocabRoundSize), 46, TFT_WHITE);
  c.setFreeFont(FontManager::getFont12pt());
  const int wrong = vocabWrongNow.size();
  String line = wrong == 0 ? String(vt("Alle richtig!", "All right!"))
                           : String(wrong) + (wrong == 1 ? vt(" Wort üben wir nochmal", " word to practise again")
                                                         : vt(" Wörter üben wir nochmal", " words to practise again"));
  vocabCentered(c, line, 76, CITRUS);
  vocabPressRow(c, vt("Drück", "Press"), vt("für die nächste Runde", "for the next round"), 106);
  vocabTabs(c, HINT_OK, HINT_MENU, HINT_NONE);
  vocabPush(c);
}

static int vocabMenuRows(VocabMenuItem out[5]) {
  int n = 0;
  out[n++] = VOCAB_MENU_GO;
  if (!vocabSpelling) out[n++] = VOCAB_MENU_DIRECTION;
  out[n++] = VOCAB_MENU_SPELL;
  out[n++] = VOCAB_MENU_PRINT;
  out[n++] = VOCAB_MENU_END;
  return n;
}

static const char* vocabMenuLabel(VocabMenuItem item) {
  switch (item) {
    case VOCAB_MENU_GO: return vt("Weiter", "Continue");
    case VOCAB_MENU_DIRECTION:
      return vocabEnglishFirst ? vt("Auf Englisch antworten", "Answer in English") : vt("Auf Deutsch antworten", "Answer in German");
    case VOCAB_MENU_SPELL: return vocabSpelling ? vt("Wörter sagen", "Say the words") : vt("Buchstabieren", "Spelling");
    case VOCAB_MENU_PRINT: return vt("Voci-Blatt drucken", "Print a word sheet");
    default: return vt("Beenden", "Stop");
  }
}

static void vocabDrawMenu() {
  M5Canvas c(&M5.Display);
  if (!vocabOpenCanvas(c)) return;
  VocabMenuItem rows[5];
  const int n = vocabMenuRows(rows);
  const int first = max(0, min(n - 3, vocabMenuSel - 1));
  for (int k = 0; k < 3; k++) {
    const int idx = first + k;
    const int y = 14 + k * 36;
    const bool on = idx == vocabMenuSel;
    const uint16_t ink = on ? TFT_BLACK : TFT_WHITE;
    if (on) c.fillRoundRect(20, y, 204, 30, 6, TFT_WHITE);
    const int cx = 36, cy = y + 15;
    if (rows[idx] == VOCAB_MENU_END) drawCrossMark(c, cx, cy, 9, ink);
    else c.fillTriangle(cx - 4, cy - 5, cx - 4, cy + 5, cx + 5, cy, ink);
    c.setFreeFont(FontManager::getFont12pt());
    c.setTextColor(ink);
    c.setTextDatum(ML_DATUM);
    c.drawString(vocabMenuLabel(rows[idx]), 50, cy);
  }
  c.fillRoundRect(230, 14, 4, 102, 2, 0x31A6);
  c.fillRoundRect(230, 14 + first * 102 / n, 4, 102 * 3 / n, 2, 0xAD55);
  vocabTabs(c, HINT_OK, HINT_CANCEL, HINT_NEXT);
  vocabPush(c);
}

void vocabRedraw() {
  if (!vocabOn) return;
  switch (vocabScreen) {
    case VOCAB_SCR_QR: vocabDrawQr(); break;
    case VOCAB_SCR_MESSAGE: vocabDrawMessage(); break;
    case VOCAB_SCR_CHOOSE: vocabDrawChoose(); break;
    case VOCAB_SCR_START: vocabDrawStart(); break;
    case VOCAB_SCR_CARD: vocabDrawCard(); break;
    case VOCAB_SCR_RESULT: vocabDrawResult(); break;
    case VOCAB_SCR_END: vocabDrawEnd(); break;
    case VOCAB_SCR_MENU: vocabDrawMenu(); break;
    default: break;
  }
}

static void vocabShow(VocabScreen s) {
  vocabScreen = s;
  vocabRedraw();
}

// ---------- Rounds ----------

static void vocabNewRound() {
  std::vector<bool> eligible(vocabCount, true);
  if (vocabSpelling)
    for (uint16_t i = 0; i < vocabCount; i++) eligible[i] = vocabSpellable(vocabSide(i, true));
  std::vector<uint16_t> lastWrong;
  uint16_t cursor = 0;
  vocabLoadRoundState(lastWrong, cursor);
  vocabQueue = vocabPlanRound(lastWrong, cursor, eligible, VOCAB_ROUND_SIZE);
  vocabSaveCursor(cursor);
  vocabRoundSize = vocabQueue.size();
  vocabPos = 0;
  vocabWrongNow.clear();
  vocabRequeued.clear();
  vocabRightFirst = 0;
}

static void vocabBeginRound() {
  vocabNewRound();
  if (vocabRoundSize == 0) {
    vocabMessage = vocabSpelling ? vt("In dieser Liste gibt es keine einzelnen Wörter zum Buchstabieren.",
                                      "This list has no single words to spell.")
                                 : vt("Diese Liste ist leer.", "This list is empty.");
    vocabMessage2 = "";
    vocabSpelling = false;
    vocabShow(VOCAB_SCR_MESSAGE);
    return;
  }
  vocabShow(VOCAB_SCR_START);
}

static bool vocabFirstAttempt(uint16_t card) {
  // A card is at its first attempt unless it was put back into the queue before this position
  for (size_t k = 0; k < vocabPos && k < vocabQueue.size(); k++)
    if (vocabQueue[k] == card) return false;
  return true;
}

static void vocabScore(bool right, bool storeWrong) {
  const uint16_t card = vocabCurrentCard();
  if (!vocabFirstAttempt(card)) return;
  if (right) {
    vocabRightFirst++;
    return;
  }
  if (!storeWrong) return;
  vocabWrongNow.push_back(card);
  bool requeued = false;
  for (auto r : vocabRequeued)
    if (r == card) requeued = true;
  if (!requeued) {
    vocabRequeued.push_back(card);
    vocabQueue.push_back(card);
  }
}

static void vocabNextCard() {
  vocabPos++;
  if (vocabPos >= vocabQueue.size()) {
    vocabSaveWrong();
    vocabShow(VOCAB_SCR_END);
    return;
  }
  vocabShow(VOCAB_SCR_CARD);
}

// ---------- Recording ----------

// Records into PSRAM. stopOnSilence: ends 1.2 s after the speech stops (never before 1.5 s). One tap on Button A
// restarts, a second quick tap cancels.
static bool vocabRecord(int rate, int seconds, bool stopOnSilence, int16_t*& buffer, int& samples, bool& cancelled) {
  cancelled = false;
  samples = 0;
  buffer = (int16_t*)ps_malloc((size_t)rate * seconds * sizeof(int16_t));
  if (!buffer) return false;
  const int chunk = rate / 20;  // 50 ms
  const int total = rate * seconds;
  bool restart;
  do {
    restart = false;
    int pos = 0, lastSecond = -1, calibrated = 0, quietMs = 0;
    long noise = 0;
    bool speech = false;
    M5.Mic.begin();
    while (pos + chunk <= total) {
      const int second = pos / rate;
      if (second != lastSecond) {
        lastSecond = second;
        vocabDrawRecording(second + 1, seconds);
      }
      M5.Mic.record(buffer + pos, chunk, rate);
      while (M5.Mic.isRecording()) {
        M5.update();
        if (M5.BtnA.wasPressed()) {
          M5.Mic.end();
          const unsigned long tap = millis();
          bool second = false;
          while (millis() - tap < MULTI_TAP_WINDOW) {
            M5.update();
            if (M5.BtnA.wasPressed()) {
              second = true;
              break;
            }
            delay(10);
          }
          if (second) {
            discardButtonA(800);
            cancelled = true;
            free(buffer);
            buffer = nullptr;
            return false;
          }
          restart = true;
          break;
        }
        delay(1);
      }
      if (restart) break;
      // Loudness of this 50 ms: mean distance from its own mean, so the microphone's offset doesn't count
      long sum = 0;
      for (int k = 0; k < chunk; k++) sum += buffer[pos + k];
      const long mean = sum / chunk;
      long dev = 0;
      for (int k = 0; k < chunk; k++) dev += labs(buffer[pos + k] - mean);
      dev /= chunk;
      pos += chunk;
      if (!stopOnSilence) continue;
      if (calibrated < 6) {
        noise += dev;
        if (++calibrated == 6) noise /= 6;
        continue;
      }
      const long threshold = max(200L, noise * 3);
      if (dev > threshold) {
        speech = true;
        quietMs = 0;
      } else if (speech) {
        quietMs += 50;
      }
      const int elapsedMs = pos * 1000 / rate;
      if (speech && quietMs >= 1200 && elapsedMs >= 1500) break;
    }
    if (!restart) {
      M5.Mic.end();
      samples = pos;
    }
  } while (restart);
  discardButtonA(0);
  return samples > 0;
}

// Records (or takes the typed test text) and transcribes. False when nothing usable came back.
static bool vocabListen(String& text) {
  if (vocabTyped.length()) {
    text = vocabTyped;
    vocabTyped = "";
    return true;
  }
  const int rate = vocabSpelling ? VOCAB_SPELL_RATE : activeSampleRate;
  const int seconds = vocabSpelling ? VOCAB_SPELL_SECONDS : activeRecordSeconds;
  int16_t* buffer = nullptr;
  int samples = 0;
  bool cancelled = false;
  if (!vocabRecord(rate, seconds, vocabSpelling, buffer, samples, cancelled)) {
    if (cancelled) Serial.println("Vocab: recording cancelled");
    return false;
  }
  vocabDrawBusy(vt("Moment...", "One moment..."));
  String error;
  const char* language = vocabSpelling || !vocabEnglishFirst ? "en" : "de";
  const bool ok = transcribeSamples(buffer, samples, rate, language, vocabSpelling ? VOCAB_SPELL_PROMPT : nullptr, text,
                                    error);
  free(buffer);
  text.trim();
  return ok && text.length() > 0;
}

// Asks the AI whether a sentence means the same as the list's. -1: no answer.
static int vocabAskJudge(const char* prompt, const char* solution, const String& answer) {
  const String system =
      "Du pruefst Antworten eines Kindes im Vokabeltraining. Antworte nur als JSON {\"same\": true} oder "
      "{\"same\": false}. same ist true, wenn die Antwort des Kindes dasselbe sagt wie die Loesung und im "
      "Sprachunterricht als richtig gelten wuerde; kleine Abweichungen wie Artikel, Kurzformen oder eine sinnvoll "
      "gefuellte Luecke (...) sind ok.";
  const String user = "Aufgabe: " + String(prompt) + "\nLoesung: " + solution + "\nAntwort des Kindes: " + answer;
  ChatResult r = aiChat(system, user, 1024, false, true);
  if (!r.ok) return -1;
  const int start = r.content.indexOf('{'), end = r.content.lastIndexOf('}');
  if (start < 0 || end <= start) return -1;
  JsonDocument doc;
  if (deserializeJson(doc, r.content.substring(start, end + 1))) return -1;
  if (!doc["same"].is<bool>()) return -1;
  return doc["same"].as<bool>() ? 1 : 0;
}

static void vocabSay(const String& text, bool english) {
  Preferences p;
  p.begin("settings", true);
  const int volume = p.getInt("volume", 210);
  p.end();
  if (volume <= 0) return;
  speakTextStyled(text, english ? VOCAB_SAY_ENGLISH : nullptr);
}

static void vocabAnswer() {
  String text;
  if (!vocabListen(text)) {
    discardButtonA(0);
    vocabShow(VOCAB_SCR_CARD);
    return;
  }
  const uint16_t i = vocabCurrentCard();
  const bool englishAnswer = !vocabEnglishFirst || vocabSpelling;
  Serial.printf("Vocab: heard '%s'\n", text.c_str());

  if (vocabSpelling) {
    VocabLetters letters = vocabLettersFrom(text.c_str());
    if (letters.letters.empty() || (letters.spokeWord && letters.letters.size() < 2)) {
      vocabMessage = vt("Bitte Buchstabe für Buchstabe.", "Letter by letter, please.");
      M5Canvas c(&M5.Display);
      if (vocabOpenCanvas(c)) {
        vocabCentered(c, vocabMessage, 60, CITRUS);
        vocabPush(c);
      }
      delay(1800);
      vocabShow(VOCAB_SCR_CARD);
      return;
    }
    const std::string word = vocabSpellWord(vocabSide(i, true));
    vocabCells = vocabAlign(word.c_str(), letters.letters.c_str());
    vocabHeard = letters.letters.c_str();
    // The boxes fill one after another with what the child spelled
    std::vector<VocabCell> said;
    for (char ch : letters.letters) said.push_back({ch, VOCAB_OK});
    for (size_t k = 1; k <= said.size(); k++) {
      M5Canvas c(&M5.Display);
      if (!vocabOpenCanvas(c)) break;
      vocabHeader(c);
      vocabBigText(c, vocabShown(vocabPromptSide(i)), 50, TFT_WHITE);
      vocabBoxes(c, said, 76, k, false);
      vocabPush(c);
      delay(110);
    }
    delay(400);
    const bool right = vocabAllOk(vocabCells);
    vocabScore(right, true);
    vocabResult = VOCAB_RES_SPELLED;
    vocabShow(VOCAB_SCR_RESULT);
    vocabSay(vocabShown(vocabSide(i, true)), true);
    return;
  }

  VocabVerdict v = vocabCheck(text.c_str(), vocabAnswerSide(i));
  if (v == VOCAB_ASK_AI) {
    vocabDrawBusy(vt("Ich prüfe...", "Checking..."));
    const int same = vocabAskJudge(vocabPromptSide(i), vocabAnswerSide(i), text);
    v = same == 1 ? VOCAB_RIGHT : same == 0 ? VOCAB_WRONG : VOCAB_ASK_AI;
  }
  vocabHeard = text;
  if (v == VOCAB_RIGHT) {
    vocabResult = VOCAB_RES_RIGHT;
    vocabScore(true, false);
  } else if (v == VOCAB_CLOSE) {
    vocabResult = VOCAB_RES_CLOSE;  // neither right nor stored as wrong
    vocabScore(false, false);
  } else if (v == VOCAB_ASK_AI) {
    vocabResult = VOCAB_RES_UNSURE;  // the AI didn't answer: not stored as wrong
    vocabScore(false, false);
  } else {
    vocabResult = VOCAB_RES_WRONG;
    vocabScore(false, true);
  }
  vocabShow(VOCAB_SCR_RESULT);
  vocabSay(vocabShown(vocabAnswerSide(i)), englishAnswer);
}

static void vocabDontKnow() {
  const uint16_t i = vocabCurrentCard();
  vocabHeard = "";
  vocabCells.clear();
  vocabResult = VOCAB_RES_WRONG;
  vocabScore(false, true);
  vocabShow(VOCAB_SCR_RESULT);
  vocabSay(vocabShown(vocabSide(i, !vocabEnglishFirst || vocabSpelling)), !vocabEnglishFirst || vocabSpelling);
}

// ---------- Opening a list ----------

static void vocabShowQr(const String& title) {
  vocabQrTitle = title;
  vocabShow(VOCAB_SCR_QR);
}

static void vocabRequestSheet();

static void vocabOpen(int index, const String& mode) {
  vocabDrawBusy(vt("Ich hole die Liste...", "Getting the list..."));
  if (!vocabLoadList(index)) {
    vocabMessage = vt("Ich konnte die Liste nicht laden.", "I couldn't load the list.");
    vocabMessage2 = vt("Ist das WLAN da?", "Is the WiFi on?");
    vocabShow(VOCAB_SCR_MESSAGE);
    return;
  }
  vocabSpelling = mode == "spell";
  vocabEnglishFirst = false;
  vocabScreen = VOCAB_SCR_START;
  if (mode == "print") {
    vocabNewRound();
    vocabRequestSheet();
    return;
  }
  vocabBeginRound();
}

bool vocabStart(const String& request) {
  VocabRequest req;
  if (!vocabParseRequest(request.c_str(), req)) return false;
  Preferences p;
  p.begin("settings", true);
  vocabGerman = p.getString("language", "DE") != "EN";
  p.end();
  modelViewEnd();
  hasModel = false;
  currentDisplayMode = SHOW_TEXT;
  vocabOn = true;
  vocabSheetPending = false;
  vocabLastPress = millis();
  vocabWasAsleep = false;
  vocabMenuSel = 0;

  if (req.mode == "upload") {
    vocabShowQr(vt("Wörterlisten", "Word lists"));
    return true;
  }
  vocabDrawBusy(vt("Ich suche die Liste...", "Looking for the list..."));
  if (!vocabLoadIndex()) {
    vocabMessage = vt("zitro.spot antwortet gerade nicht.", "zitro.spot isn't answering.");
    vocabMessage2 = vt("Versuch es später nochmal.", "Please try again later.");
    vocabShow(VOCAB_SCR_MESSAGE);
    return true;
  }
  vocabPendingMode = req.mode.c_str();
  // No book and no unit: the list practised last, if it is still there
  if (req.book.empty() && req.unit.empty()) {
    const String last = vocabLastListId();
    for (size_t k = 0; k < vocabLists.size(); k++)
      if (last.length() && vocabLists[k].id == last.c_str()) {
        vocabOpen(k, vocabPendingMode);
        return true;
      }
  }
  int found = -1;
  const VocabFindResult r =
      vocabFind(vocabLists, vocabCurrentBook.c_str(), req.book.c_str(), req.unit.c_str(), found, vocabChoices);
  if (r == VOCAB_NO_LISTS) {
    vocabShowQr(vt("Noch keine Wörterlisten", "No word lists yet"));
  } else if (r == VOCAB_NOT_FOUND) {
    String what = req.unit.length() ? String(req.unit.c_str()) : String(req.book.c_str());
    vocabShowQr(vt("Keine Liste «", "No list \"") + what + vt("»", "\""));
  } else if (r == VOCAB_CHOOSE) {
    vocabChoosingBooks = !req.unit.empty();
    vocabChooseSel = 0;
    vocabShow(VOCAB_SCR_CHOOSE);
  } else {
    vocabOpen(found, vocabPendingMode);
  }
  return true;
}

void vocabStop() {
  vocabOn = false;
  vocabSheetPending = false;
  vocabScreen = VOCAB_SCR_OFF;
  vocabFreeList();
  vocabLists.clear();
  vocabQueue.clear();
  response = MAIN_QUESTION;
  currentDisplayMode = SHOW_TEXT;
  if (!apMode) drawScreen(MAIN_QUESTION);
  discardButtonA(800);
  Serial.println("Vocab: trainer stopped");
}

// ---------- The printed sheet ----------

static void vocabRequestSheet() {
  drawPrintStatus("Searching...", PRINT_BADGE_NONE);
  discoverPrinters();
  if (printerCount == 0) {
    drawPrintStatus("No printer\nfound", PRINT_BADGE_FAIL);
    delay(2500);
    vocabRedraw();
    return;
  }
  vocabSheetPending = true;  // vocabActive() is false now: loop() runs the print menu
  selectedPrinterIndex = 0;
  printMenuReset();
  inPrinterSelectionMode = true;
  displayPrinterSelection();
}

void vocabSheetDone() {
  vocabSheetPending = false;
  vocabLastPress = millis();
  if (vocabScreen == VOCAB_SCR_START && vocabRoundSize == 0) vocabNewRound();
  vocabRedraw();
}

#define VOCAB_SHEET_ROWS 18

static String vocabSheetFootnote(const String& gap) {
  return String(vt("Wörterliste: eigene Datei", "Word list: your own file")) + "  \xC2\xB7  " +
         vt("Gedruckt mit ", "Printed with ") + gap + PRINT_SITE_URL;
}

// Draws text in the left top corner of box (w x 2 lines), wrapped to two lines, bottom-aligned on baseY
static void vocabSheetCell(M5Canvas& c, const String& text, int x, int w, int baseY) {
  String lines[2];
  const int lineH = FontManager::getFont12pt()->yAdvance;
  const int n = wrapToLines(c, text, w, lines, 2);
  for (int i = 0; i < n; i++) c.drawString(lines[i], x, baseY - (n - i) * lineH);
}

// One A4 page of the sheet at this resolution: title, Name and Datum lines, then German left and a writing line
// (or the answer) right. Uses the print code's text layers and footnote. Returns a PWG_SEND_* value.
static int vocabSheetPage(PrinterInfo& printer, const PwgAttempt& attempt, int pageIndex, int pagesEach,
                          bool solutions) {
  PwgPage page;
  pwgSetA4(page, attempt.dpi);
  page.grayscale = attempt.grayscale;
  const int W = page.width, H = page.height;
  const int px10mm = attempt.dpi * 100 / 254;
  const int margin = px10mm, bottom = px10mm * 14 / 10, gap = px10mm * 3 / 10;
  const int textScale = max(1, attempt.dpi / 300);
  const int sheetScale = textScale * 2;  // twice the picture pages' text: room to write

  PwgBitmap foot, table, lemon, leaf;
  const GFXfont* footFont = FontManager::getFont12pt();
  const int lemonK = max(1, attempt.dpi / 200);
  const int lemonW = 21 * lemonK, lemonH = 15 * lemonK, lemonPad = attempt.dpi / 50;
  String lemonGap;
  {
    M5Canvas m;
    m.setFreeFont(footFont);
    int spaceW = m.textWidth(PRINT_NBSP);
    if (spaceW < 1) spaceW = 5;
    const int need = (lemonW + lemonPad + textScale - 1) / textScale;
    for (int i = 0; i * spaceW < need; i++) lemonGap += PRINT_NBSP;
  }
  int urlX = 0, urlLine = 0;
  bool ok = renderTextBitmap(vocabSheetFootnote(lemonGap), footFont, (W - 2 * margin) / textScale, 3, foot,
                             PRINT_SITE_URL, &urlX, &urlLine);
  const int footY = H - bottom - foot.height * textScale;
  const int contentBottom = footY - gap * 2;
  if (ok && foot.bits) {
    PwgBitmap& b = page.bitmaps[page.bitmapCount++];
    b = foot;
    b.scale = textScale;
    b.x = margin;
    b.y = footY;
    b.r = b.g = b.b = 0x66;
    const int lineH = footFont->yAdvance * textScale;
    const int lemonX = margin + urlX * textScale - lemonW - lemonPad;
    const int lemonY = footY + urlLine * lineH + (lineH - lemonH) / 2;
    ok = renderLemonLayer(false, lemonK, lemon) && renderLemonLayer(true, lemonK, leaf);
    if (ok) {
      PwgBitmap& l1 = page.bitmaps[page.bitmapCount++];
      l1 = lemon;
      l1.x = lemonX;
      l1.y = lemonY;
      l1.r = 0xF2; l1.g = 0xB7; l1.b = 0x05;
      PwgBitmap& l2 = page.bitmaps[page.bitmapCount++];
      l2 = leaf;
      l2.x = lemonX;
      l2.y = lemonY;
      l2.r = 0x4A; l2.g = 0x82; l2.b = 0x31;
    }
  }

  // Title, lines and rows in one 1-bit layer
  const int bw = (W - 2 * margin) / sheetScale, bh = (contentBottom - margin) / sheetScale;
  if (ok) {
    M5Canvas c;
    c.setPsram(true);
    c.setColorDepth(1);
    ok = c.createSprite(bw, bh);
    if (ok) {
      c.fillSprite(TFT_BLACK);
      c.setTextColor(TFT_WHITE);
      c.setTextDatum(TL_DATUM);
      c.setFreeFont(FontManager::getFont16pt());
      String title = vocabBook + " \xC2\xB7 " + vocabName;
      if (solutions) title += vt(" \xC2\xB7 Lösungen", " \xC2\xB7 Answers");
      if (pagesEach > 1) title += "  " + String(pageIndex + 1) + "/" + String(pagesEach);
      c.drawString(title, 0, 0);
      const int titleH = FontManager::getFont16pt()->yAdvance;
      c.setFreeFont(FontManager::getFont12pt());
      const int lineH = FontManager::getFont12pt()->yAdvance;
      if (solutions) {
        c.drawString(vt("Deutsch - Englisch", "German - English"), 0, titleH + 4);
      } else {
        c.drawString(vt("Name:", "Name:"), 0, titleH + 4);
        c.drawFastHLine(c.textWidth("Name: "), titleH + 4 + lineH - 4, bw / 2 - c.textWidth("Name: ") - 20, TFT_WHITE);
        c.drawString(vt("Datum:", "Date:"), bw / 2 + 10, titleH + 4);
        const int dx = bw / 2 + 10 + c.textWidth(vt("Datum: ", "Date: "));
        c.drawFastHLine(dx, titleH + 4 + lineH - 4, bw - dx, TFT_WHITE);
      }
      const int headerH = titleH + lineH + 16;
      const int rowH = (bh - headerH) / VOCAB_SHEET_ROWS;
      const int colW = bw / 2 - 12;
      const int first = pageIndex * VOCAB_SHEET_ROWS;
      for (int r = 0; r < VOCAB_SHEET_ROWS && first + r < vocabCount; r++) {
        const int rowBottom = headerH + (r + 1) * rowH;
        vocabSheetCell(c, vocabShown(vocabSide(first + r, false)), 0, colW, rowBottom - 4);
        if (solutions) {
          vocabSheetCell(c, vocabShown(vocabSide(first + r, true)), bw / 2 + 10, colW, rowBottom - 4);
        } else {
          c.drawFastHLine(bw / 2 + 10, rowBottom - 6, colW, TFT_WHITE);
          c.drawFastHLine(bw / 2 + 10, rowBottom - 5, colW, TFT_WHITE);
        }
      }
      ok = packCanvasBits(c, bw, bh, table);
      c.deleteSprite();
    }
    if (ok) {
      PwgBitmap& b = page.bitmaps[page.bitmapCount++];
      b = table;
      b.scale = sheetScale;
      b.x = margin;
      b.y = margin;
      b.r = b.g = b.b = 0x1D;
    }
  }
  const int result = ok ? sendPwgPrintJob(printer, page) : PWG_SEND_REJECTED;
  if (!ok) Serial.println("Vocab sheet: not enough memory for the page");
  freeTextBitmap(foot);
  freeTextBitmap(table);
  freeTextBitmap(lemon);
  freeTextBitmap(leaf);
  return result;
}

// Prints the whole list: pages to fill in, then the same pages with the answers. One job per page; the first page
// finds a resolution the printer takes, the rest use it.
int vocabPrintSheet(PrinterInfo& printer) {
  if (!vocabPool || vocabCount == 0) return PRINT_COMPOSE_UNAVAILABLE;
  PwgCaps caps;
  queryPwgCaps(printer, caps);
  PwgAttempt attempts[4];
  const int count = pwgPlanAttempts(caps, attempts, 4);
  if (count == 0) return PRINT_COMPOSE_UNAVAILABLE;
  const int pagesEach = (vocabCount + VOCAB_SHEET_ROWS - 1) / VOCAB_SHEET_ROWS;
  int chosen = -1;
  for (int p = 0; p < pagesEach * 2; p++) {
    const bool solutions = p >= pagesEach;
    drawPrintStatus(String("Printing...\n") + String(p + 1) + "/" + String(pagesEach * 2), PRINT_BADGE_NONE);
    lastActivityTime = millis();
    if (chosen >= 0) {
      if (vocabSheetPage(printer, attempts[chosen], p % pagesEach, pagesEach, solutions) != PWG_SEND_OK)
        return PRINT_COMPOSE_FAILED;
      continue;
    }
    for (int a = 0; a < count && chosen < 0; a++) {
      const int sent = vocabSheetPage(printer, attempts[a], p % pagesEach, pagesEach, solutions);
      if (sent == PWG_SEND_OK) chosen = a;
      else if (sent == PWG_SEND_FAILED) return PRINT_COMPOSE_FAILED;
      else Serial.printf("Vocab sheet: %d dpi rejected\n", attempts[a].dpi);
    }
    if (chosen < 0) return PRINT_COMPOSE_FAILED;
  }
  return PRINT_COMPOSE_OK;
}

// ---------- Buttons ----------

enum VocabButton { VOCAB_BTN_A, VOCAB_BTN_B, VOCAB_BTN_C };

static void vocabMenuChoose() {
  VocabMenuItem rows[5];
  const int n = vocabMenuRows(rows);
  const VocabMenuItem item = rows[min(vocabMenuSel, n - 1)];
  switch (item) {
    case VOCAB_MENU_GO: vocabShow(vocabMenuReturn); break;
    case VOCAB_MENU_DIRECTION:
      vocabEnglishFirst = !vocabEnglishFirst;
      vocabBeginRound();
      break;
    case VOCAB_MENU_SPELL:
      vocabSpelling = !vocabSpelling;
      vocabEnglishFirst = false;
      vocabBeginRound();
      break;
    case VOCAB_MENU_PRINT:
      vocabScreen = vocabMenuReturn;
      vocabRequestSheet();
      break;
    default: vocabStop(); break;
  }
}

static void vocabPress(VocabButton b) {
  vocabLastPress = millis();
  switch (vocabScreen) {
    case VOCAB_SCR_QR:
    case VOCAB_SCR_MESSAGE: vocabStop(); return;
    case VOCAB_SCR_CHOOSE:
      if (b == VOCAB_BTN_C) {
        vocabChooseSel = (vocabChooseSel + 1) % max(1, (int)vocabChoices.size());
        vocabDrawChoose();
      } else if (b == VOCAB_BTN_A && !vocabChoices.empty()) {
        // A book (its list with the unit asked for) or a list: either way the choice is a list
        vocabOpen(vocabChoices[vocabChooseSel], vocabPendingMode);
      } else if (b == VOCAB_BTN_B) {
        vocabStop();
      }
      return;
    case VOCAB_SCR_MENU: {
      VocabMenuItem rows[5];
      const int n = vocabMenuRows(rows);
      if (b == VOCAB_BTN_C) {
        vocabMenuSel = (vocabMenuSel + 1) % n;
        vocabDrawMenu();
      } else if (b == VOCAB_BTN_A) {
        vocabMenuChoose();
      } else {
        vocabShow(vocabMenuReturn);
      }
      return;
    }
    default: break;
  }
  if (b == VOCAB_BTN_B) {
    vocabMenuReturn = vocabScreen;
    vocabMenuSel = 0;
    vocabShow(VOCAB_SCR_MENU);
    return;
  }
  switch (vocabScreen) {
    case VOCAB_SCR_START:
      if (b == VOCAB_BTN_A) vocabShow(VOCAB_SCR_CARD);
      break;
    case VOCAB_SCR_CARD:
      if (b == VOCAB_BTN_A) vocabAnswer();
      else vocabDontKnow();
      break;
    case VOCAB_SCR_RESULT: vocabNextCard(); break;
    case VOCAB_SCR_END:
      if (b == VOCAB_BTN_A) {
        vocabNewRound();
        vocabShow(VOCAB_SCR_CARD);
      }
      break;
    default: break;
  }
}

void vocabTick() {
  const uint32_t now = millis();
  const bool awake = !displaySleeping && currentPowerState == POWER_ACTIVE && !wakeFromLightSleep;
  if (awake && vocabWasAsleep) vocabRedraw();
  vocabWasAsleep = !awake;

  const bool clickedA = M5.BtnA.wasClicked();
  const bool clickedB = M5.BtnB.wasClicked();
  const bool clickedC = M5.BtnPWR.wasClicked();
  if (!guideAcceptsPress(awake, inWakeCooldown())) {
    if (clickedA || clickedB || clickedC) {
      vocabLastPress = now;
      timeWokeFromSleep = now;  // the cooldown, so a second tap doesn't act on a screen that has just come back
      resetToActiveState();
      vocabRedraw();
    }
    return;
  }
  if (guideKeepAwake(now, vocabLastPress)) lastActivityTime = now;
  if (clickedA || clickedB || clickedC) {
    resetToActiveState();
    vocabPress(clickedA ? VOCAB_BTN_A : clickedB ? VOCAB_BTN_B : VOCAB_BTN_C);
  }
}

// ---------- Serial ----------

void vocabSerial(const String& line) {
  if (line == "vocabcode") {
    Serial.println("Vocab code: " + vocabCode() + "  page: " + VOCAB_PAGE_URL + vocabCode());
  } else if (line == "vocabnewcode") {
    Preferences p;
    p.begin("voci", false);
    p.remove("code");
    p.end();
    Serial.println("Vocab code: " + vocabCode());
  } else if (line.startsWith("vocabhost ")) {
    vocabApiUrl = line.substring(10);
    vocabApiUrl.trim();
    if (vocabApiUrl.length() == 0) vocabApiUrl = VOCAB_API_DEFAULT;
    Serial.println("Vocab API: " + vocabApiUrl);
  } else if (line.startsWith("vocabtype ")) {
    vocabTyped = line.substring(10);
    Serial.println("Vocab: next answer is '" + vocabTyped + "'");
  } else if (line.startsWith("vocabkey ") && line.length() > 9) {
    if (!vocabActive()) {
      Serial.println("vocabkey: the trainer isn't running");
      return;
    }
    resetToActiveState();
    const char k = line[9];
    vocabPress(k == 'a' ? VOCAB_BTN_A : k == 'b' ? VOCAB_BTN_B : VOCAB_BTN_C);
    Serial.printf("vocab screen=%d pos=%d/%d right=%d\n", vocabScreen, (int)vocabPos, (int)vocabQueue.size(),
                  vocabRightFirst);
  } else if (line.startsWith("vocab ")) {
    String value = line.substring(6);
    value.trim();
    Serial.printf("vocab start: %d\n", vocabStart(value));
  } else {
    Serial.println("vocab <value> | vocabkey a|b|c | vocabtype <text> | vocabcode | vocabnewcode | vocabhost <url>");
  }
}
