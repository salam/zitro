// The shop game ("Verkäuferlis"): a child practises English at a lemonade bar, a bakery or a shop, as the seller
// (guests order, the child serves by saying what it hands over) or as a guest (a shopping list in pictures, the child
// orders). shop_core.h decides what was heard and what guests and sellers answer, shop_draw.h draws. No chat model
// takes part: only speech-to-text and text-to-speech. Like the vocabulary trainer it takes over loop() while it runs.
// Requirements: docs/features/shop.md

#include "shop_core.h"
#include "shop_draw.h"

// ---------- Constants ----------

static const int SHOP_RATE = 16000;
static const int SHOP_SECONDS = 8;
static const int SHOP_MAX_TRIES = 3;
static const char* const SHOP_SLOW =
    "Speak British English very slowly and clearly, with a short pause between the words, for a child learning "
    "English.";

enum ShopScreen : uint8_t { SHOP_SCR_OFF, SHOP_SCR_START, SHOP_SCR_PLAY, SHOP_SCR_END, SHOP_SCR_MENU };
enum ShopButton : uint8_t { SHOP_BTN_A, SHOP_BTN_B, SHOP_BTN_C };
enum ShopMenuItem : uint8_t { SHOP_MENU_GO, SHOP_MENU_ROLE, SHOP_MENU_PLACE, SHOP_MENU_END, SHOP_MENU_ITEMS };

// ---------- State ----------

static bool shopOn = false;
static ShopScreen shopScreen = SHOP_SCR_OFF;
static ShopScreen shopMenuReturn = SHOP_SCR_PLAY;
static int shopMenuSel = 0;
static bool shopGerman = true;
static uint32_t shopLastPress = 0;
static bool shopWasAsleep = false;
static String shopTyped;  // "shoptype <text>" stands in for the next recording
static uint32_t shopRng = 1;

static ShopPlace shopPlace = SHOP_BAR;
static ShopRole shopRole = SHOP_SELLER;
static ShopScene shopNow;          // what the screen shows while playing
static ShopLook shopSeller;        // the seller of this round (the child shops)
static int shopTurn = 0;           // guest or shopping list of the round, 0..SHOP_GUESTS_PER_ROUND-1
static int shopTries = 0;          // wrong answers for this guest or list
static int shopHelp = 0;           // presses of "?" for this guest or list
static bool shopTurnDone = false;  // served: the next press brings the next guest or list
static std::string shopOrderLine;  // what the guest ordered, said again at "?"
static ShopHeard shopLastHeard;    // the child's last order, for the answer to "How many?"
static int shopAsked = -1;         // the thing the seller asked "How many?" about

static const char* st(const char* de, const char* en) {
  return shopGerman ? de : en;
}

bool shopActive() {
  return shopOn;
}

// ---------- Drawing ----------

static bool shopOpenCanvas(M5Canvas& c) {
  M5.Display.setRotation(getLandscapeRotation());
  c.setPsram(true);
  if (!c.createSprite(WIDTH, HEIGHT)) {
    Serial.println("Shop: no memory for the screen");
    return false;
  }
  shopFonts = {FontManager::getFont12pt(), FontManager::getFont16pt()};
  return true;
}

static void shopPush(M5Canvas& c) {
  captureCanvas(c);
  c.pushSprite(0, 0);
  c.deleteSprite();
}

static void shopTabs(M5Canvas& c, int kindA, int kindB, int kindC) {
  drawButtonHints(c, M5.Display.getRotation(), kindA, kindB, kindC);
}

static const char* shopPlaceName(ShopPlace place) {
  return shopGerman ? shopPlaceInfo(place).german : shopPlaceInfo(place).english;
}

// "Press [M5] to start" in white on the counter
static void shopPressRow(M5Canvas& c, int cy) {
  c.setFreeFont(FontManager::getFont12pt());
  const String a = String(st("Drück", "Press")) + " ";
  const String b = String(" ") + st("zum Starten", "to start");
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

static ShopLook shopStartPerson() {
  uint32_t rng = 1000 + shopPlace * 10 + shopRole;  // the same face each time the start screen shows
  return shopMakeLook(rng);
}

static void shopDrawStartScreen() {
  M5Canvas c(&M5.Display);
  if (!shopOpenCanvas(c)) return;
  shopDrawStart(c, shopPlace, shopRole, shopStartPerson(), shopPlaceName(shopPlace),
                shopRole == SHOP_SELLER ? st("Du verkaufst", "You sell") : st("Du kaufst ein", "You shop"));
  shopPressRow(c, SHOP_COUNTER_Y + 18);
  shopTabs(c, HINT_OK, HINT_CUT, HINT_NEXT);
  shopPush(c);
}

static void shopDrawPlay() {
  M5Canvas c(&M5.Display);
  if (!shopOpenCanvas(c)) return;
  shopDrawScene(c, shopNow);
  if (shopNow.recording < 0) {
    if (shopTurnDone) shopTabs(c, HINT_PLAY, HINT_MENU, HINT_NONE);
    else shopTabs(c, HINT_MIC, HINT_MENU, HINT_DONTKNOW);
  }
  shopPush(c);
}

static void shopDrawRecording(int secondsDone, int seconds) {
  shopNow.recording = seconds > 0 ? secondsDone * 100 / seconds : 0;
  shopDrawPlay();
}

static int shopScore() {
  int n = 0;
  for (uint8_t s : shopNow.stars) n += s == 1;
  return n;
}

static void shopDrawEndScreen() {
  M5Canvas c(&M5.Display);
  if (!shopOpenCanvas(c)) return;
  char line1[24];
  snprintf(line1, sizeof line1, st("%d von %d", "%d of %d"), shopScore(), (int)shopNow.stars.size());
  shopDrawEnd(c, shopPlace, shopNow.stars, line1, st("auf Anhieb richtig", "right first time"));
  shopTabs(c, HINT_PLAY, HINT_MENU, HINT_NONE);
  shopPush(c);
}

static const char* shopMenuLabel(int item) {
  switch (item) {
    case SHOP_MENU_GO: return st("Weiter", "Go on");
    case SHOP_MENU_ROLE:
      return shopRole == SHOP_SELLER ? st("Selber einkaufen", "Go shopping") : st("Selber verkaufen", "Be the seller");
    case SHOP_MENU_PLACE: {
      static const char* const de[] = {"Zur Bäckerei", "Zum Laden", "Zur Limonadenbar"};
      static const char* const en[] = {"To the bakery", "To the shop", "To the lemonade bar"};
      return shopGerman ? de[shopPlace] : en[shopPlace];
    }
    default: return st("Beenden", "Stop");
  }
}

static void shopDrawMenu() {
  M5Canvas c(&M5.Display);
  if (!shopOpenCanvas(c)) return;
  c.fillSprite(TFT_BLACK);
  const int n = SHOP_MENU_ITEMS;
  const int first = max(0, min(n - 3, shopMenuSel - 1));
  for (int k = 0; k < 3; k++) {
    const int idx = first + k;
    const int y = 14 + k * 36;
    const bool on = idx == shopMenuSel;
    const uint16_t ink = on ? TFT_BLACK : TFT_WHITE;
    if (on) c.fillRoundRect(20, y, 204, 30, 6, TFT_WHITE);
    const int cx = 36, cy = y + 15;
    if (idx == SHOP_MENU_END) drawCrossMark(c, cx, cy, 9, ink);
    else c.fillTriangle(cx - 4, cy - 5, cx - 4, cy + 5, cx + 5, cy, ink);
    c.setFreeFont(FontManager::getFont12pt());
    c.setTextColor(ink);
    c.setTextDatum(ML_DATUM);
    c.drawString(shopMenuLabel(idx), 50, cy);
  }
  c.fillRoundRect(230, 14, 4, 102, 2, 0x31A6);
  c.fillRoundRect(230, 14 + first * 102 / n, 4, 102 * 3 / n, 2, 0xAD55);
  shopTabs(c, HINT_OK, HINT_CANCEL, HINT_NEXT);
  shopPush(c);
}

void shopRedraw() {
  switch (shopScreen) {
    case SHOP_SCR_START: shopDrawStartScreen(); break;
    case SHOP_SCR_PLAY: shopDrawPlay(); break;
    case SHOP_SCR_END: shopDrawEndScreen(); break;
    case SHOP_SCR_MENU: shopDrawMenu(); break;
    default: break;
  }
}

static void shopShow(ShopScreen s) {
  shopScreen = s;
  shopRedraw();
}

// ---------- Speaking and listening ----------

static int shopVolume() {
  Preferences p;
  p.begin("settings", true);
  const int volume = p.getInt("volume", 210);
  p.end();
  return volume;
}

// The person on the screen says it with their own voice. False when nothing could be heard (no sound, no key,
// no network): the caller shows the pictures instead.
static bool shopSpeak(const std::string& text, bool slow) {
  Serial.printf("Shop: says '%s'\n", text.c_str());
  if (shopVolume() <= 0) return false;
  static const char* const where[] = {"a lemonade bar", "a bakery", "a shop"};
  const String style = slow ? String(SHOP_SLOW)
                            : String("You are a friendly ") + (shopRole == SHOP_SELLER ? "guest" : "shop assistant") +
                                  " at " + where[shopPlace] +
                                  ", talking to a child who is learning English. Speak British English clearly and a "
                                  "little slowly, cheerful.";
  return speakTextVoiced(String(text.c_str()), style.c_str(), shopVoiceName(shopNow.person.voice));
}

// Zitro itself (not the person in the shop), slowly: the words of the shopping list
static void shopNarrate(const std::string& text) {
  Serial.printf("Shop: hint '%s'\n", text.c_str());
  if (shopVolume() > 0) speakTextVoiced(String(text.c_str()), SHOP_SLOW, "nova");
}

// Draws the person talking, says the line, then shows them with the mood that follows
static bool shopSay(const std::string& text, ShopMood after, bool slow = false) {
  shopNow.mood = SHOP_MOOD_TALK;
  shopDrawPlay();
  const bool heard = shopSpeak(text, slow);
  shopNow.mood = after;
  shopDrawPlay();
  return heard;
}

static String shopPrompt() {
  String prompt = "A child plays shop in English and names things like: ";
  for (int k = 0; k < SHOP_GOODS_PER_PLACE; k++) {
    if (k) prompt += ", ";
    prompt += shopItemInfo(shopPlaceInfo(shopPlace).goods[k]).word;
  }
  return prompt + ", with numbers like one, two, three.";
}

// Records (or takes the typed test text) and transcribes. False when nothing usable came back.
static bool shopListen(String& text) {
  if (shopTyped.length()) {
    text = shopTyped;
    shopTyped = "";
    return true;
  }
  int16_t* buffer = nullptr;
  int samples = 0;
  bool cancelled = false;
  const bool ok = recordSpeech(SHOP_RATE, SHOP_SECONDS, true, buffer, samples, cancelled, shopDrawRecording);
  shopNow.recording = -1;
  if (!ok) {
    if (cancelled) Serial.println("Shop: recording cancelled");
    return false;
  }
  shopNow.note = st("Moment...", "One moment...");
  shopDrawPlay();
  String error;
  const bool heard = transcribeSamples(buffer, samples, SHOP_RATE, "en", shopPrompt().c_str(), text, error);
  free(buffer);
  shopNow.note = nullptr;
  text.trim();
  if (!heard) Serial.println("Shop: transcription failed: " + error);
  return heard && text.length() > 0;
}

// ---------- The round ----------

static void shopFinishTurn(bool firstTime) {
  shopNow.stars[shopTurn] = firstTime ? 1 : 2;
  shopTurnDone = true;
}

// A guest walks in and orders, or the seller greets the child with a new shopping list
static void shopNextTurn() {
  shopTries = 0;
  shopHelp = 0;
  shopTurnDone = false;
  shopAsked = -1;
  shopLastHeard = ShopHeard();
  shopNow.counter.clear();
  shopNow.note = nullptr;
  shopNow.order = shopMakeOrder(shopPlace, shopTurn, shopRng);
  shopNow.marks.assign(shopNow.order.size(), SHOP_MARK_NONE);
  shopScreen = SHOP_SCR_PLAY;
  if (shopRole == SHOP_SELLER) {
    shopNow.person = shopMakeLook(shopRng);
    shopNow.bubble = SHOP_BUBBLE_NONE;
    shopNow.mood = SHOP_MOOD_HAPPY;
    for (int x = SHOP_W + 30; x > 92; x -= 40) {  // walks in
      shopNow.personX = x;
      shopDrawPlay();
      delay(30);
    }
    shopNow.personX = 92;
    shopNow.bubble = SHOP_BUBBLE_DOTS;
    shopOrderLine = shopOrderSentence(shopNow.order, shopRng);
    if (!shopSay(shopOrderLine, SHOP_MOOD_HAPPY)) {
      shopNow.bubble = SHOP_BUBBLE_PICTURES;  // couldn't be heard: the order in pictures
      shopDrawPlay();
    }
  } else {
    shopNow.person = shopSeller;
    shopNow.personX = 127;
    shopSay(shopSellerGreeting(shopTurn, shopRng), SHOP_MOOD_HAPPY);
  }
}

static void shopBeginRound() {
  shopTurn = 0;
  shopNow = ShopScene();
  shopNow.place = shopPlace;
  shopNow.role = shopRole;
  shopNow.stars.assign(SHOP_GUESTS_PER_ROUND, 0);
  shopSeller = shopMakeLook(shopRng);
  shopNextTurn();
}

static void shopAdvance() {
  if (++shopTurn >= SHOP_GUESTS_PER_ROUND) {
    shopShow(SHOP_SCR_END);
    return;
  }
  shopNextTurn();
}

// The things of this place the child named, for the counter
static std::vector<ShopLine> shopOnCounter(const ShopHeard& heard) {
  std::vector<ShopLine> out;
  for (const ShopLine& l : heard.lines)
    if (shopPlaceOf(l.item) == shopPlace && l.qty > 0) out.push_back(l);
  return out;
}

static String shopHeardNote(const String& text) {
  return String(st("Gehört: «", "Heard: \"")) + text + st("»", "\"");
}

static String shopNoteText;  // keeps the note's text alive while the scene points at it

// The child sells: says what it hands over
static void shopServe(const String& text, const ShopHeard& heard) {
  std::vector<ShopDiff> diffs;
  const ShopMatch m = shopCompare(shopNow.order, heard, shopPlace, diffs);
  if (m == SHOP_MATCH_NOTHING) {
    shopNoteText = shopHeardNote(text);
    shopNow.note = shopNoteText.c_str();
    shopSay("Sorry?", SHOP_MOOD_NEUTRAL);
    return;
  }
  shopNow.counter = shopOnCounter(heard);
  if (m == SHOP_MATCH_EXACT) {
    shopNow.bubble = SHOP_BUBBLE_HEART;
    shopFinishTurn(shopTries == 0 && shopHelp < 2);
    shopSay(shopThanks(shopRng), SHOP_MOOD_HAPPY);
    return;
  }
  shopTries++;
  shopNow.bubble = SHOP_BUBBLE_PICTURES;
  if (shopTries >= SHOP_MAX_TRIES) {
    shopNow.counter = shopNow.order;
    shopFinishTurn(false);
    shopSay("Never mind. Thank you! Bye!", SHOP_MOOD_HAPPY);
    return;
  }
  shopSay(shopComplaint(diffs), SHOP_MOOD_NEUTRAL);
}

// The child shops: orders what's on the list
static void shopOrder(const String& text, ShopHeard heard) {
  // The answer to "How many apples?": just a number
  const int n = shopBareNumber(text.c_str());
  if (heard.lines.empty() && n > 0 && shopAsked >= 0) {
    heard = shopLastHeard;
    for (ShopLine& l : heard.lines)
      if ((int)l.item == shopAsked) l.qty = (int8_t)n;
  }
  std::vector<ShopDiff> diffs;
  const ShopMatch m = shopCompare(shopNow.order, heard, shopPlace, diffs);
  shopAsked = -1;
  if (m == SHOP_MATCH_NOTHING) {
    shopNoteText = shopHeardNote(text);
    shopNow.note = shopNoteText.c_str();
    shopSay(shopSellerReply(m, heard, shopPlace), SHOP_MOOD_NEUTRAL);
    return;
  }
  shopLastHeard = heard;
  shopNow.counter = shopOnCounter(heard);
  for (size_t k = 0; k < shopNow.order.size(); k++)
    shopNow.marks[k] = shopQtyOf(heard.lines, shopNow.order[k].item) == shopNow.order[k].qty ? SHOP_MARK_OK : SHOP_MARK_BAD;
  if (m == SHOP_MATCH_EXACT) {
    shopFinishTurn(shopTries == 0 && shopHelp < 2);
    shopSay(shopSellerReply(m, heard, shopPlace), SHOP_MOOD_HAPPY);
    return;
  }
  shopTries++;
  if (shopTries >= SHOP_MAX_TRIES) {
    shopNow.counter = shopNow.order;
    shopNow.marks.assign(shopNow.order.size(), SHOP_MARK_OK);
    shopFinishTurn(false);
    shopSay(shopModelAnswer(shopNow.order), SHOP_MOOD_HAPPY);
    return;
  }
  shopAsked = shopAskedItem(heard);
  shopSay(shopSellerReply(m, heard, shopPlace), SHOP_MOOD_TALK);
}

static void shopAnswer() {
  String text;
  shopNow.note = nullptr;
  if (!shopListen(text)) {
    discardButtonA(0);
    shopDrawPlay();
    return;
  }
  Serial.printf("Shop: heard '%s'\n", text.c_str());
  const ShopHeard heard = shopHear(text.c_str());
  if (shopRole == SHOP_SELLER) shopServe(text, heard);
  else shopOrder(text, heard);
  discardButtonA(0);
}

// "?": the guest says it again slowly, then shows it in pictures; as a guest, Zitro says the list's words, then
// the whole order
static void shopHint() {
  shopHelp++;
  shopNow.note = nullptr;
  if (shopRole == SHOP_SELLER) {
    if (shopHelp >= 2) shopNow.bubble = SHOP_BUBBLE_PICTURES;
    shopSay(shopOrderLine, shopNow.mood == SHOP_MOOD_TALK ? SHOP_MOOD_HAPPY : shopNow.mood, true);
    return;
  }
  std::string words;
  if (shopHelp == 1) {
    for (const ShopLine& l : shopNow.order) words += std::string(words.empty() ? "" : ", ") + shopItemInfo(l.item).word;
  } else {
    words = shopListPhrase(shopNow.order) + ", please.";
  }
  shopNarrate(words);
  discardButtonA(0);
}

// ---------- Starting and stopping ----------

bool shopStart(const String& request) {
  ShopRequest req;
  if (!shopParseRequest(request.c_str(), req)) return false;
  if (vocabActive()) vocabStop();
  Preferences p;
  p.begin("settings", true);
  shopGerman = p.getString("language", "DE") != "EN";
  p.end();
  // Like the trainer, the game replaces the last result: nothing of it comes back after the game
  modelViewEnd();
  hasModel = false;
  hasImage = false;
  currentImageUrl = "";
  currentDisplayMode = SHOW_TEXT;
  shopRng = esp_random() | 1;
  if (req.hasPlace) shopPlace = req.place;
  if (req.hasRole) shopRole = req.role;
  shopOn = true;
  shopLastPress = millis();
  shopWasAsleep = false;
  shopMenuSel = 0;
  shopShow(SHOP_SCR_START);
  Serial.printf("Shop: started at %s, child %s\n", shopPlaceInfo(shopPlace).english,
                shopRole == SHOP_SELLER ? "sells" : "shops");
  return true;
}

void shopStop() {
  shopOn = false;
  shopScreen = SHOP_SCR_OFF;
  shopNow = ShopScene();
  response = MAIN_QUESTION;
  currentDisplayMode = SHOW_TEXT;
  if (!apMode) drawScreen(MAIN_QUESTION);
  discardButtonA(800);
  Serial.println("Shop: game stopped");
}

// ---------- Buttons ----------

static void shopMenuChoose() {
  switch (shopMenuSel) {
    case SHOP_MENU_GO: shopShow(shopMenuReturn); break;
    case SHOP_MENU_ROLE:
      shopRole = shopRole == SHOP_SELLER ? SHOP_GUEST : SHOP_SELLER;
      shopShow(SHOP_SCR_START);
      break;
    case SHOP_MENU_PLACE:
      shopPlace = (ShopPlace)((shopPlace + 1) % SHOP_PLACES);
      shopShow(SHOP_SCR_START);
      break;
    default: shopStop(); break;
  }
}

static void shopPress(ShopButton b) {
  shopLastPress = millis();
  switch (shopScreen) {
    case SHOP_SCR_MENU:
      if (b == SHOP_BTN_C) {
        shopMenuSel = (shopMenuSel + 1) % SHOP_MENU_ITEMS;
        shopDrawMenu();
      } else if (b == SHOP_BTN_A) {
        shopMenuChoose();
      } else {
        shopShow(shopMenuReturn);
      }
      return;
    case SHOP_SCR_START:
      if (b == SHOP_BTN_A) {
        shopBeginRound();
      } else if (b == SHOP_BTN_B) {
        shopRole = shopRole == SHOP_SELLER ? SHOP_GUEST : SHOP_SELLER;
        shopDrawStartScreen();
      } else {
        shopPlace = (ShopPlace)((shopPlace + 1) % SHOP_PLACES);
        shopDrawStartScreen();
      }
      return;
    default: break;
  }
  if (b == SHOP_BTN_B) {
    shopMenuReturn = shopScreen;
    shopMenuSel = 0;
    shopShow(SHOP_SCR_MENU);
    return;
  }
  if (shopScreen == SHOP_SCR_END) {
    if (b == SHOP_BTN_A) shopBeginRound();
    return;
  }
  if (shopScreen != SHOP_SCR_PLAY) return;
  if (shopTurnDone) {
    if (b == SHOP_BTN_A) shopAdvance();
  } else if (b == SHOP_BTN_A) {
    shopAnswer();
  } else {
    shopHint();
  }
}

void shopTick() {
  const uint32_t now = millis();
  const bool awake = !displaySleeping && currentPowerState == POWER_ACTIVE && !wakeFromLightSleep;
  if (awake && shopWasAsleep) shopRedraw();
  shopWasAsleep = !awake;

  const bool clickedA = M5.BtnA.wasClicked();
  const bool clickedB = M5.BtnB.wasClicked();
  const bool clickedC = M5.BtnPWR.wasClicked();
  if (!guideAcceptsPress(awake, inWakeCooldown())) {
    if (clickedA || clickedB || clickedC) {
      shopLastPress = now;
      timeWokeFromSleep = now;  // the cooldown, so a second tap doesn't act on a screen that has just come back
      resetToActiveState();
      shopRedraw();
    }
    return;
  }
  if (guideKeepAwake(now, shopLastPress)) lastActivityTime = now;
  if (clickedA || clickedB || clickedC) {
    resetToActiveState();
    shopPress(clickedA ? SHOP_BTN_A : clickedB ? SHOP_BTN_B : SHOP_BTN_C);
  }
}

// ---------- Serial ----------

void shopSerial(const String& line) {
  if (line.startsWith("shoptype ")) {
    shopTyped = line.substring(9);
    Serial.println("Shop: next answer is '" + shopTyped + "'");
  } else if (line.startsWith("shopkey ") && line.length() > 8) {
    if (!shopActive()) {
      Serial.println("shopkey: the game isn't running");
      return;
    }
    resetToActiveState();
    const char k = line[8];
    shopPress(k == 'a' ? SHOP_BTN_A : k == 'b' ? SHOP_BTN_B : SHOP_BTN_C);
    Serial.printf("shop screen=%d turn=%d tries=%d help=%d done=%d score=%d\n", shopScreen, shopTurn, shopTries,
                  shopHelp, shopTurnDone, shopScore());
  } else if (line.startsWith("shop ")) {
    String value = line.substring(5);
    value.trim();
    Serial.printf("shop start: %d\n", shopStart(value));
  } else {
    Serial.println("shop play|<place>|<role> | shopkey a|b|c | shoptype <text>");
  }
}
