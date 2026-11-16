// The home menu. One press of B on the home screen opens it (the button module wires that; until then serial
// "home"). The stick's functions come first ("Timer anzeigen" on top), then a line, then 16 example questions
// that open their view from a fixed answer, without the AI. Looks like the trainer's menu and presses like its lists:
// each click of B = next row, holding B = close, A = choose, C not read (menuPressRead()).
// Spec: docs/superpowers/specs/2026-11-15-home-menu-design.md
#include "home_menu_core.h"
#include "home_menu_icons.h"

static bool homeOn = false;
static uint8_t homeSel = 0;
static uint32_t homeLastPress = 0;
static bool homeWasAsleep = false;

bool homeMenuActive() { return homeOn; }

// Deep sleep: the menu doesn't come back after the wake
void homeMenuStop() { homeOn = false; }

static const char* homeRowLabel(int row) {
  const bool german = deviceLanguageIsGerman();
  const int q = homeRowQuestion(row);
  if (q < 0) return HOME_FUNCTION_LABELS[row][german ? 0 : 1];
  return german ? HOME_QUESTIONS[q].labelDe : HOME_QUESTIONS[q].labelEn;
}

// A row's icon in the row's ink: white on black, black on the selected white row (Matthias, 2026-11-16)
static void homeDrawIcon(M5Canvas& c, int row, int x, int y, bool selected) {
  const uint8_t* cover = HOME_ROW_ICONS[row];
  for (int j = 0; j < HOME_ICON_SIZE; j++) {
    for (int i = 0; i < HOME_ICON_SIZE; i++) {
      const uint8_t v = cover[j * HOME_ICON_SIZE + i];
      if (!v) continue;
      const uint8_t g = selected ? 255 - v : v;
      c.drawPixel(x + i, y + j, c.color565(g, g, g));
    }
  }
}

void homeMenuRedraw() {
  if (!homeOn) return;
  M5.Display.setRotation(getLandscapeRotation());
  M5Canvas c(&M5.Display);
  c.setPsram(true);
  if (!c.createSprite(WIDTH, HEIGHT)) {
    Serial.println("Home menu: no memory for the screen");
    return;
  }
  c.fillSprite(TFT_BLACK);
  c.setFreeFont(FontManager::getFont12pt());
  c.setTextDatum(ML_DATUM);
  const int n = HOME_ROW_COUNT;
  const int first = homeFirstVisible(homeSel, n);
  for (int k = 0; k < 3; k++) {
    const int row = first + k;
    const int y = 14 + k * 36;
    if (row == HOME_FN_COUNT) c.drawFastHLine(20, y - 3, 204, 0x7BEF);  // the line above the questions
    const bool on = row == homeSel;
    const uint16_t ink = on ? TFT_BLACK : TFT_WHITE;
    if (on) c.fillRoundRect(20, y, 204, 30, 6, TFT_WHITE);
    const int cy = y + 15;
    homeDrawIcon(c, row, HOME_ICON_X, cy - HOME_ICON_SIZE / 2, on);
    c.setTextColor(ink);
    c.drawString(homeRowLabel(row), 50, cy);
  }
  c.fillRoundRect(230, 14, 4, 102, 2, 0x31A6);
  c.fillRoundRect(230, 14 + first * 102 / n, 4, 102 * 3 / n, 2, 0xAD55);
  drawButtonHints(c, M5.Display.getRotation(), HINT_OK, HINT_NEXT, HINT_NONE);  // B is next, holding it closes; no C
  captureCanvas(c);
  c.pushSprite(0, 0);
  c.deleteSprite();
}

// An open 3D view gives its memory back first; the menu closes onto the written answer
static void homeLeaveModelView() {
  modelViewEnd();
  if (currentDisplayMode == SHOW_MODEL) {
    currentDisplayMode = SHOW_TEXT;
    modelResetRotateToggle = true;
  }
}

void homeMenuOpen() {
  homeLeaveModelView();
  homeOn = true;
  homeSel = HOME_FN_TIMERS;
  homeLastPress = millis();
  homeWasAsleep = false;
  resetToActiveState();
  Serial.println("Home menu: open");
  homeMenuRedraw();
}

static void homeMenuClose() {
  homeOn = false;
  Serial.println("Home menu: closed");
  redrawPreviousResult();
}

// As a spoken question blocks it: the night lock holds for learning and playing, not for the timers, the
// settings or closing
static bool homeMenuNightLocked(int row) {
  if (!homeMenuBlockedAtNight(row) || !parentalLockEnabled || !isRestrictedHours()) return false;
  Serial.println("Home menu: parental lock is on and the time is restricted (8pm-6am)");
  showNightLockAndSleep();
  return true;
}

// As a spoken question waits for WiFi: on the WiFi page. No known network around: the setup page.
static bool homeMenuWaitForWifi() {
  if (wifiLinkUp() || wifiWaitForLink(WIFI_QUESTION_WAIT_MS, WIFI_SHOW_PAGE)) return true;
  Serial.println("Home menu: no known WiFi found - opening setup");
  wifiPageShow(WIFI_PAGE_NONE);
  delay(2000);
  startAPMode();
  return false;
}

// One of the example questions: its fixed answer goes the way a spoken question's answer goes, so the view,
// the text, the tabs, printing and follow-up questions work the same. No AI, no read-aloud.
void homeAskQuestion(int index) {
  if (index < 0 || index >= HOME_QUESTION_COUNT) return;
  const HomeQuestion& q = HOME_QUESTIONS[index];
  const bool german = deviceLanguageIsGerman();
  Serial.printf("Home menu: question %d, %s\n", index + 1, q.model[0] ? q.model : q.imageQuery);
  homeLeaveModelView();
  resetToActiveState();
  if (!homeMenuWaitForWifi()) return;
  GPTResponse r{};
  r.valid = true;
  r.answer = german ? q.answerDe : q.answerEn;
  r.model = q.model;
  r.imageQuery = q.imageQuery;
  showAnswer(r, german ? q.labelDe : q.labelEn, false);
}

static void homeMenuChoose(int row) {
  homeOn = false;
  Serial.printf("Home menu: row %d chosen\n", row + 1);
  if (homeMenuNightLocked(row)) return;
  const int q = homeRowQuestion(row);
  if (q >= 0) {
    homeAskQuestion(q);
    return;
  }
  switch (row) {
    case HOME_FN_TIMERS:
      showTimerList(false);
      break;
    case HOME_FN_TOUR:
      guideStartNow();
      break;
    case HOME_FN_SURPRISE:
      showSurprise();
      break;
    case HOME_FN_WORDS:  // the list practised last, else the choice of lists
      if (homeMenuWaitForWifi() && !vocabStart("practice")) redrawPreviousResult();
      break;
    case HOME_FN_SETTINGS:
      openSettings();
      break;
    default:  // "Menü schliessen"
      redrawPreviousResult();
      break;
  }
}

void homeMenuPressed(HomePress press) {
  switch (homeMenuPress(homeSel, press)) {
    case HOME_ACT_REDRAW:
      homeMenuRedraw();
      break;
    case HOME_ACT_CLOSE:
      homeMenuClose();
      break;
    case HOME_ACT_CHOOSE:
      homeMenuChoose(homeSel);
      break;
  }
}

void homeMenuTick() {
  const uint32_t now = millis();
  const bool awake = !displaySleeping && currentPowerState == POWER_ACTIVE && !wakeFromLightSleep;
  if (awake && homeWasAsleep) homeMenuRedraw();
  homeWasAsleep = !awake;

  // Each click of B is the next row at once, holding B closes, C isn't read (menuPressRead, home_menu_core.h). B's
  // clicks are forgotten as they come, so they never add up to a single, double or triple press for loop() later.
  const bool clickedB = M5.BtnB.wasClicked();
  const bool held = M5.BtnB.wasHold();
  const MenuPress press = menuPressRead(M5.BtnA.wasClicked(), clickedB, held);
  if (clickedB || held) forgetButtonBClicks();
  if (held) buttonBHeldForMenu = true;  // keep holding: no WiFi reset from loop() for this press
  const bool pressA = press == MENU_PRESS_A;
  const bool pressNext = press == MENU_PRESS_NEXT;
  const bool pressBack = press == MENU_PRESS_BACK;
  if (!guideAcceptsPress(awake, inWakeCooldown())) {
    if (pressA || pressNext || pressBack) {
      homeLastPress = now;
      timeWokeFromSleep = now;  // the cooldown, so a second tap doesn't act on a screen that has just come back
      resetToActiveState();
      homeMenuRedraw();
    }
    return;
  }
  if (guideKeepAwake(now, homeLastPress)) lastActivityTime = now;
  if (pressA || pressNext || pressBack) {
    homeLastPress = now;
    resetToActiveState();
    // The same click's late single-click must not start a recording, and taps while an answer loads mustn't queue
    if (pressA) discardButtonA(0);
    homeMenuPressed(homePressFromMenu(press));
    if (pressA) discardButtonA(0);
    forgetButtonBClicks();  // taps while a chosen view loaded
  }
}

// Serial: "home" opens the menu, "home <n>" chooses row n (1 to 22), "homekey a|b|c" presses in the open menu
void homeMenuSerial(const String& line) {
  if (line == "home") {
    homeMenuOpen();
    return;
  }
  if (line.startsWith("homekey ") && line.length() > 8) {
    if (!homeOn) {
      Serial.println("Home menu: not open");
      return;
    }
    const char k = line[8];
    homeMenuPressed(k == 'a' ? HOME_PRESS_A : k == 'b' ? HOME_PRESS_NEXT : HOME_PRESS_BACK);
    return;
  }
  const int n = line.substring(5).toInt();
  if (n < 1 || n > HOME_ROW_COUNT) {
    Serial.printf("Home menu: rows 1 to %d\n", HOME_ROW_COUNT);
    return;
  }
  homeSel = (uint8_t)(n - 1);
  homeMenuChoose(n - 1);
}
