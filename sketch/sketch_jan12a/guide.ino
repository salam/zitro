// The guided tour: one real question, three special answers, then everything else on a sliding grid.
// guide_core.h decides what each button does; this file draws the screens and does the work.
// While it runs, loop() hands over right after the timer blocks, so no other button handler sees a press.

#include "guide_moon.h"

GuideState guideState = {GUIDE_OFF, 0, false, false, 0, false};
GuideVoice lastVoiceOutcome = GUIDE_VOICE_ANSWERED;

static bool guideGerman = true;
static uint32_t guideLastPress = 0;
static uint32_t guideQuoteUntil = 0;
static bool guideWasAsleep = false;
static int guideDrawnRotation = -1;
static bool guideShowPhotoText = false;
static GuideRotate guideRotate = {false};
static float guideGridPos = 0;
static TimelineTilt guideTilt = {0, 0, 0};
static String guideAnswerRef;  // The child's own result, to bring back on "one step back"

static const uint32_t GUIDE_FRAME_MS = 40;  // As in the result views

static const char* gt(const char* de, const char* en) {
  return guideGerman ? de : en;
}

bool guideActive() {
  return guideBlocksSettings(guideState);
}

static bool guideOpenCanvas(M5Canvas& c) {
  M5.Display.setRotation(getLandscapeRotation());
  c.setPsram(true);  // The menu opens while the result view still holds its own sprite
  if (!c.createSprite(WIDTH, HEIGHT)) {
    Serial.println("Guide: no memory for the screen");
    return false;
  }
  c.fillSprite(TFT_BLACK);
  c.setTextColor(TFT_WHITE);
  c.setFreeFont(FontManager::getFont12pt());
  guideDrawnRotation = M5.Display.getRotation();
  return true;
}

static void guidePush(M5Canvas& c) {
  captureCanvas(c);
  c.pushSprite(0, 0);
  c.deleteSprite();
}

static void guideTabs(M5Canvas& c, int kindA, int kindB, int kindC) {
  drawButtonHints(c, M5.Display.getRotation(), kindA, kindB, kindC);
}

static uint16_t guideCitrus(M5Canvas& c) {
  return c.color565(0xF2, 0xB7, 0x05);
}

// Centered lines around centerY, in the current font
static void guideCenteredLines(M5Canvas& c, const String* lines, int count, int centerY, int lineHeight) {
  c.setTextDatum(MC_DATUM);
  for (int i = 0; i < count; i++) {
    c.drawString(lines[i], WIDTH / 2, centerY + i * lineHeight - (count - 1) * lineHeight / 2);
  }
}

// "Press [M5] and say:" centered on the screen
static void guidePressRow(M5Canvas& c, const char* before, const char* after, int cy) {
  c.setFreeFont(FontManager::getFont12pt());
  const String a = String(before) + " ";
  const String b = String(" ") + after;
  const int capW = drawKeycap(c, "M5", 0, 0, true);
  int x = (WIDTH - (c.textWidth(a) + capW + c.textWidth(b))) / 2;
  c.setTextColor(TFT_WHITE);
  c.setTextDatum(ML_DATUM);
  c.drawString(a, x, cy);
  x += c.textWidth(a);
  drawKeycap(c, "M5", x, cy);
  x += capW;
  c.setTextColor(TFT_WHITE);
  c.setTextDatum(ML_DATUM);
  c.setFreeFont(FontManager::getFont12pt());
  c.drawString(b, x, cy);
}

static void guideDrawAsk() {
  M5Canvas c(&M5.Display);
  if (!guideOpenCanvas(c)) return;
  const String hello = gt("Hallo, ich bin Zitro!", "Hi, I'm Zitro!");
  const int x = (WIDTH - (21 + 7 + c.textWidth(hello))) / 2;
  drawLemon(c, x + 10, 20);
  c.setTextDatum(ML_DATUM);
  c.drawString(hello, x + 28, 20);
  guidePressRow(c, gt("Drück", "Press"), gt("und sprich:", "and say:"), 50);
  String lines[2];
  c.setTextColor(guideCitrus(c));
  const int n = wrapToLines(c, gt("«Wie weit ist der Mond weg?»", "\"How far away is the Moon?\""), 204, lines, 2);
  guideCenteredLines(c, lines, n, 88, 21);
  guideTabs(c, HINT_MIC, HINT_MENU, HINT_NONE);
  guidePush(c);
}

static void guideDrawRetry() {
  M5Canvas c(&M5.Display);
  if (!guideOpenCanvas(c)) return;
  String lines[2];
  const int n = wrapToLines(c, gt("Das habe ich nicht verstanden.", "I didn't catch that."), 204, lines, 2);
  guideCenteredLines(c, lines, n, 42, 23);
  guidePressRow(c, gt("Drück", "Press"), gt("nochmal", "again"), 92);
  guideTabs(c, HINT_MIC, HINT_MENU, HINT_LATER);
  guidePush(c);
}

static void guideDrawDone() {
  M5Canvas c(&M5.Display);
  if (!guideOpenCanvas(c)) return;
  drawLemon(c, WIDTH / 2, 24);
  c.setFreeFont(FontManager::getFont16pt());
  c.setTextDatum(MC_DATUM);
  c.drawString(gt("Jetzt du!", "Your turn!"), WIDTH / 2, 58);
  c.setFreeFont(FontManager::getFont12pt());
  c.drawString(gt("Frag mich, was du willst.", "Ask me anything."), WIDTH / 2, 92);
  guideTabs(c, HINT_MIC, HINT_MENU, HINT_NONE);
  guidePush(c);
}

static const char* guideQuoteText() {
  switch (guideState.step) {
    case GUIDE_PHOTO: return gt("«Wie sieht der Mond aus?»", "\"What does the Moon look like?\"");
    case GUIDE_SKY: return gt("«Wo ist Orion?»", "\"Where is Orion?\"");
    default: return gt("«Spiel Twinkle Twinkle Little Star»", "\"Play Twinkle Twinkle Little Star\"");
  }
}

// A question in big letters if it fits on two lines, otherwise a size smaller
static void guideDrawBigText(M5Canvas& c, const String& text, int centerY) {
  String lines[3];
  c.setFreeFont(FontManager::getFont16pt());
  int lineHeight = 26;
  int n = wrapToLines(c, text, 216, lines, 3);
  if (n > 2 || lines[n - 1].endsWith("...")) {
    c.setFreeFont(FontManager::getFont12pt());
    lineHeight = 21;
    n = wrapToLines(c, text, 216, lines, 3);
  }
  guideCenteredLines(c, lines, n, centerY, lineHeight);
}

static void guideDrawQuote() {
  M5Canvas c(&M5.Display);
  if (!guideOpenCanvas(c)) return;
  drawLemon(c, WIDTH / 2, 18);
  c.setTextDatum(MC_DATUM);
  c.setTextColor(TFT_LIGHTGREY);
  c.drawString(gt("Du fragst:", "You ask:"), WIDTH / 2, 40);
  c.setTextColor(TFT_WHITE);
  guideDrawBigText(c, guideQuoteText(), 82);
  guideTabs(c, HINT_NONE, HINT_MENU, HINT_LATER);
  guidePush(c);
}

static void guideDrawPhoto() {
  M5Canvas c(&M5.Display);
  if (!guideOpenCanvas(c)) return;
  if (guideShowPhotoText) {
    String lines[3];
    const int n = wrapToLines(c, gt("Der Mond ist grau und voller Krater. Sie stammen von Einschlägen.",
                                    "The Moon is grey and full of craters. They come from impacts."),
                              216, lines, 3);
    guideCenteredLines(c, lines, n, HEIGHT / 2, 24);
  } else {
    c.drawJpg(GUIDE_MOON_JPG, GUIDE_MOON_JPG_LEN, 0, 0);
    c.fillRect(66, 106, 18, 14, TFT_BLACK);  // The source file has a stray mark below the disc
    const char* hint = gt("Stell mich hochkant!", "Stand me upright!");
    c.setFont(&fonts::DejaVu9);
    c.setTextSize(1);
    const int w = c.textWidth(hint) + 12;
    c.fillRoundRect(WIDTH - 4 - w, 16, w, 15, 7, guideCitrus(c));
    c.setTextColor(TFT_BLACK);
    c.setTextDatum(ML_DATUM);
    c.drawString(hint, WIDTH - 4 - w + 6, 24);
    c.setTextColor(TFT_LIGHTGREY);
    c.setTextDatum(textdatum_t::baseline_left);
    c.drawString(gt("Mond - NASA", "Moon - NASA"), 2, HEIGHT - 4);
  }
  guideTabs(c, HINT_NONE, HINT_MENU, HINT_LATER);
  guidePush(c);
}

static void guideDrawMenu() {
  M5Canvas c(&M5.Display);
  if (!guideOpenCanvas(c)) return;
  GuideMenuItem rows[3];
  const int count = guideMenuRows(guideState, rows);
  const int rowH = 30, gap = 4;
  const int top = HEIGHT / 2 - (count * rowH + (count - 1) * gap) / 2;
  for (int i = 0; i < count; i++) {
    const int y = top + i * (rowH + gap);
    const bool on = i == guideState.menuSel;
    const uint16_t ink = on ? TFT_BLACK : TFT_WHITE;
    if (on) c.fillRoundRect(20, y, 200, rowH, 6, TFT_WHITE);
    const int cx = 36, cy = y + rowH / 2;
    const char* label;
    if (rows[i] == GUIDE_MENU_GO) {
      c.fillTriangle(cx - 4, cy - 5, cx - 4, cy + 5, cx + 5, cy, ink);
      label = gt("Weiter", "Continue");
    } else if (rows[i] == GUIDE_MENU_BACK) {
      c.fillTriangle(cx + 4, cy - 5, cx + 4, cy + 5, cx - 5, cy, ink);
      label = gt("Ein Schritt zurück", "One step back");
    } else {
      drawCrossMark(c, cx, cy, 9, ink);
      label = gt("Tour beenden", "End tour");
    }
    c.setTextColor(ink);
    c.setTextDatum(ML_DATUM);
    c.drawString(label, 50, cy);
  }
  guideTabs(c, HINT_OK, HINT_CANCEL, HINT_NEXT);
  guidePush(c);
}

static void guideDrawGridNow() {
  M5Canvas c(&M5.Display);
  if (!guideOpenCanvas(c)) return;
  guideDrawGrid(c, guideGridPos, M5.Display.getRotation());
  guideTabs(c, HINT_NONE, HINT_MENU, HINT_LATER);
  guidePush(c);
}

// Closes the result view and frees what it loaded
static void guideCloseView() {
  if (currentDisplayMode == SHOW_MODEL) leaveModelView();
  hasModel = false;
  eduUnloadActive();
  modelRefText = "";
}

static void guideStepEntered();

// A built-in result couldn't open (no memory): the tour goes on without it
static void guideOpenView(const char* text) {
  ModelRef ref;
  bool opened = false;
  if (parseModelRef(text, ref) && fetchModel(ref)) {
    currentDisplayMode = SHOW_MODEL;
    opened = modelViewShow();
  }
  if (opened) return;
  Serial.printf("Guide: %s didn't open\n", text);
  guideStepForward(guideState);
  guideStepEntered();
}

static void guideShowAnswer() {
  if (guideAnswerRef.length() > 0 && modelRefText != guideAnswerRef) {
    ModelRef ref;
    if (parseModelRef(guideAnswerRef.c_str(), ref)) fetchModel(ref);
  }
  if (hasModel) {
    currentDisplayMode = SHOW_MODEL;
    modelViewShow();
  } else {
    currentDisplayMode = SHOW_TEXT;
    drawScreen(response);
  }
}

void guideRenderStep() {
  if (!guideActive() || inTimerListMode) return;
  const GuideState& s = guideState;
  if (s.menuOpen) {
    guideDrawMenu();
    return;
  }
  if (s.quote) {
    guideDrawQuote();
    return;
  }
  switch (s.step) {
    case GUIDE_ASK: guideDrawAsk(); break;
    case GUIDE_RETRY: guideDrawRetry(); break;
    case GUIDE_ANSWER: guideShowAnswer(); break;
    case GUIDE_PHOTO: guideDrawPhoto(); break;
    case GUIDE_SKY:
    case GUIDE_MUSIC:
      if (hasModel && modelRefText.length() > 0) {
        modelViewShow();
      } else {
        guideOpenView(s.step == GUIDE_SKY ? GUIDE_SKY_REF : GUIDE_MUSIC_REF);
      }
      break;
    case GUIDE_GRID: guideDrawGridNow(); break;
    case GUIDE_DONE: guideDrawDone(); break;
    default: break;
  }
}

// The step just changed: set up what it needs and draw it
static void guideStepEntered() {
  const GuideStep step = guideState.step;
  if (step != GUIDE_ANSWER) guideCloseView();
  if (guideState.quote) guideQuoteUntil = millis() + GUIDE_QUOTE_MS;
  guideShowPhotoText = false;
  guideRotate.sawPortrait = false;
  if (step == GUIDE_GRID) {
    guideGridPos = guideState.page;
    guideTilt = {0, 0, 0};
    setGyroEnabled(true);
  } else {
    updateGyroPower();
  }
  guideRenderStep();
}

void guideStartNow() {
  guideGerman = deviceLanguageIsGerman();
  guideCloseView();
  guideStart(guideState);
  guideAnswerRef = "";
  guideLastPress = millis();
  guideWasAsleep = false;
  guideRenderStep();
  Serial.println("Guide: tour started");
}

bool guideAutoStart() {
  Preferences prefs;
  prefs.begin("settings", true);
  const bool done = prefs.getBool("tourDone", false);
  prefs.end();
  if (!guideShouldAutoStart(done, wakeFromDeepSleep, aiChatConfigured(), audioEndpoint().available)) return false;
  guideStartNow();
  return true;
}

void guideStop(bool markDone) {
  if (markDone) {
    Preferences prefs;
    prefs.begin("settings", false);
    prefs.putBool("tourDone", true);
    prefs.end();
  }
  guideState.step = GUIDE_OFF;
  guideState.menuOpen = false;
  guideCloseView();
  guideAnswerRef = "";
  hasImage = false;
  currentImageUrl = "";
  response = MAIN_QUESTION;
  currentDisplayMode = SHOW_TEXT;
  updateGyroPower();
  if (!apMode) drawScreen(MAIN_QUESTION);
  // M5Unified reports wasSingleClicked() after the click the tour already used, and a double tap on
  // "End tour" would leave a second one: neither may start a recording
  discardButtonA(800);
  Serial.printf("Guide: tour stopped, done=%d\n", markDone);
}

void guideOnHeard(const String& question) {
  M5Canvas c(&M5.Display);
  if (!guideOpenCanvas(c)) return;
  c.setTextDatum(MC_DATUM);
  c.setTextColor(TFT_LIGHTGREY);
  c.drawString(gt("Du hast gesagt:", "You said:"), WIDTH / 2, 22);
  c.setTextColor(TFT_WHITE);
  guideDrawBigText(c, question, 78);
  guidePush(c);
  delay(GUIDE_HEARD_MS);
}

void guideDecorateScreen(M5Canvas& c) {
  drawButtonHints(c, M5.Display.getRotation(), HINT_NONE, HINT_MENU, HINT_LATER);
}

void guideDecorateProgress(M5Canvas& c) {
  c.setFreeFont(FontManager::getFont12pt());
  c.setTextColor(TFT_WHITE);
  c.setTextDatum(MC_DATUM);
  c.drawString(gt("Sprich jetzt!", "Speak now!"), WIDTH / 2, 30);
}

void guideDrawViewHints(M5Canvas& c) {
  guideDecorateScreen(c);
}

static void guideRunVoice() {
  handleVoiceQuery(false);
  discardButtonA(0);
  guideLastPress = millis();
  if (apMode) {
    guideStop(false);
    return;
  }
  const GuideVoice outcome = lastVoiceOutcome;
  if (outcome == GUIDE_VOICE_ANSWERED) guideAnswerRef = hasModel ? modelRefText : "";
  if (guideVoiceResult(guideState, outcome) == GUIDE_EV_ABORT) {
    guideStop(false);
    return;
  }
  guideRenderStep();
}

static void guideHandleEvent(GuideEvent event, bool menuWasOpen) {
  switch (event) {
    case GUIDE_EV_RECORD: guideRunVoice(); break;
    case GUIDE_EV_STEP: guideStepEntered(); break;
    case GUIDE_EV_PAGE:
      if (menuWasOpen) guideRenderStep();  // Otherwise the slide draws it
      break;
    case GUIDE_EV_QUOTE_SKIP: guideRenderStep(); break;
    case GUIDE_EV_MENU:
      if (guideState.menuOpen && guideState.step == GUIDE_MUSIC) M5.Speaker.stop(MUSIC_SPEAKER_CHANNEL);
      if (!guideState.menuOpen && guideState.quote) guideQuoteUntil = millis() + GUIDE_QUOTE_MS;
      guideRenderStep();
      break;
    case GUIDE_EV_FINISH: guideStop(true); break;
    case GUIDE_EV_FINISH_ASK:
      guideStop(true);
      handleVoiceQuery(false);
      discardButtonA(0);
      break;
    default: break;
  }
}

// Serial "guidekey a|b|c": presses a button, so the tour can be walked through and captured without hands
void guideSerialKey(char key) {
  if (!guideActive()) {
    Serial.println("guidekey: the tour isn't running");
    return;
  }
  guideLastPress = millis();
  resetToActiveState();
  const bool menuWasOpen = guideState.menuOpen;
  const GuideButton button = key == 'a' ? GUIDE_BTN_A : key == 'b' ? GUIDE_BTN_B : GUIDE_BTN_C;
  guideHandleEvent(guidePress(guideState, button), menuWasOpen);
  Serial.printf("guide step=%d page=%d quote=%d menu=%d sel=%d\n", guideState.step, guideState.page, guideState.quote,
                guideState.menuOpen, guideState.menuSel);
}

static void guideGridTick(uint32_t now) {
  static uint32_t lastFrame = 0;
  if (now - lastFrame < GUIDE_FRAME_MS) return;
  const float dt = min((now - lastFrame) / 1000.0f, 0.1f);
  lastFrame = now;
  if (gyroReady()) {
    M5.Imu.update();
    const auto imu = M5.Imu.getImuData();
    float dps[3];
    gyroToScreen(M5.Display.getRotation(), imu.gyro.x, imu.gyro.y, imu.gyro.z, dps);
    const int step = timelineTiltUpdate(guideTilt, TIMELINE_TILT_SIGN * dps[2], dt);
    if (step != 0) guideLastPress = now;
    if (step > 0 && guideState.page + 1 < GUIDE_PAGES) guideState.page++;
    if (step < 0 && guideState.page > 0) guideState.page--;
  }
  const float next = guideSlideStep(guideGridPos, guideState.page, dt);
  if (next != guideGridPos) {
    guideGridPos = next;
    guideDrawGridNow();
  }
}

void guideTick() {
  const uint32_t now = millis();
  const bool awake = !displaySleeping && currentPowerState == POWER_ACTIVE && !wakeFromLightSleep;
  // The wake-up code draws the last answer by itself
  if (awake && guideWasAsleep) {
    if (guideState.step == GUIDE_GRID) setGyroEnabled(true);  // Waking up leaves it off
    guideRenderStep();
  }
  if (!awake && !guideWasAsleep) updateGyroPower();  // Off while the screen is dark
  guideWasAsleep = !awake;

  const bool clickedA = M5.BtnA.wasClicked();
  const bool clickedB = buttonBClicked();
  const bool clickedC = buttonCClicked();
  if (!guideAcceptsPress(awake, inWakeCooldown())) {
    if (clickedA || clickedB || clickedC) {
      guideLastPress = now;
      timeWokeFromSleep = now;  // The cooldown, so a second tap doesn't act on a screen that has just come back
      resetToActiveState();
      guideRenderStep();
    }
    return;
  }
  if (guideKeepAwake(now, guideLastPress)) lastActivityTime = now;

  if (clickedA || clickedB || clickedC) {
    guideLastPress = now;
    resetToActiveState();
    const bool menuWasOpen = guideState.menuOpen;
    const GuideButton button = clickedA ? GUIDE_BTN_A : clickedB ? GUIDE_BTN_B : GUIDE_BTN_C;
    guideHandleEvent(guidePress(guideState, button), menuWasOpen);
    if (!guideActive()) return;
  }

  const GuideState& s = guideState;
  if (s.menuOpen) return;
  if (s.quote) {
    if ((int32_t)(now - guideQuoteUntil) >= 0) {
      guideState.quote = false;
      guideRenderStep();
    }
    return;
  }

  static bool photoUpright = false;  // Kept between checks: an upright stick mustn't flip the picture
  static uint32_t lastOrientationCheck = 0;
  if (s.step == GUIDE_PHOTO && now - lastOrientationCheck >= 100) {
    lastOrientationCheck = now;
    const bool landscape = detectOrientation() == LANDSCAPE;
    photoUpright = !landscape;
    if (guideRotateStep(guideRotate, landscape)) {
      guideShowPhotoText = !guideShowPhotoText;
      guideRenderStep();
    }
  }

  if (currentDisplayMode == SHOW_MODEL && hasModel) {
    modelViewTick();
    return;  // The view keeps its rotation until it ends
  }
  if (s.step == GUIDE_GRID) guideGridTick(now);

  static uint32_t lastRotationCheck = 0;
  if (!(s.step == GUIDE_PHOTO && photoUpright) && now - lastRotationCheck >= 200) {
    lastRotationCheck = now;
    const int rotation = getLandscapeRotation();
    if (rotation != guideDrawnRotation) {
      guideDrawnRotation = rotation;  // drawScreen doesn't report it
      guideRenderStep();
    }
  }
}
