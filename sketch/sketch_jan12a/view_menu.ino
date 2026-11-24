// A small menu inside a view that brings its own rows (EduFeature.menuRowCount: the wave lab's "Ton an"). Holding B
// opens it, and until the button module comes, so does Button C (the StickS3's double press of B). Rows: the view's,
// then "Text" and "Schliessen". It presses like the home menu: each click of B = next row, holding B = close,
// A = choose. The view keeps running underneath, so a sound switched on is heard at once.
// Interim: the button module's shared menu (feature/button-ui) is to take the view's rows over.
// Spec: docs/superpowers/specs/2026-11-16-wave-lab-design.md, "Buttons and the coming button module"
#include "view_menu_core.h"

static bool viewMenuOn = false;
static uint8_t viewMenuSel = 0;
static uint32_t viewMenuLastPress = 0;

bool viewMenuActive() { return viewMenuOn; }

void viewMenuStop() { viewMenuOn = false; }

static int viewMenuFeatureRows() {
  if (!activeFeature || !activeFeature->menuRowCount || !activeFeature->menuRowLabel || !activeFeature->menuRowChoose) {
    return 0;
  }
  const int n = activeFeature->menuRowCount();
  return n > 0 ? n : 0;
}

// Opens the menu over the view, if the view has rows of its own. False and nothing opens otherwise.
bool viewMenuOpen() {
  if (displaySleeping || currentDisplayMode != SHOW_MODEL || !hasModel || !modelViewActive()) return false;
  if (viewMenuFeatureRows() == 0) return false;
  viewMenuOn = true;
  viewMenuSel = 0;  // The view's first row
  viewMenuLastPress = millis();
  if (M5.BtnB.isPressed()) buttonBHeldForMenu = true;  // Holding on: no WiFi reset from loop() for this press
  forgetButtonBClicks();
  Serial.println("View menu: open");
  return true;
}

static const char* viewMenuLabel(int row, int featureRows, bool german) {
  const ViewMenuRow kind = viewMenuRowKind(row, featureRows);
  if (kind == VIEW_ROW_FEATURE) return activeFeature->menuRowLabel(row, german);
  return viewMenuFixedLabel(kind, german);
}

// Over the view's frame, on its sprite: three rows as the home menu draws them, and the menu's tabs
void viewMenuDraw(M5Canvas& c) {
  if (!viewMenuOn) return;
  const int featureRows = viewMenuFeatureRows();
  const int n = viewMenuCount(featureRows);
  const bool german = deviceLanguageIsGerman();
  const int first = homeFirstVisible(viewMenuSel, n);
  const int shown = n < 3 ? n : 3;
  c.fillRoundRect(14, 8, 212, shown * 36 + 6, 8, TFT_BLACK);
  c.drawRoundRect(14, 8, 212, shown * 36 + 6, 8, 0x7BEF);
  c.setFreeFont(FontManager::getFont12pt());
  c.setTextDatum(ML_DATUM);
  for (int k = 0; k < shown; k++) {
    const int row = first + k;
    const int y = 14 + k * 36;
    const bool on = row == viewMenuSel;
    if (on) c.fillRoundRect(20, y, 204, 30, 6, TFT_WHITE);
    c.setTextColor(on ? TFT_BLACK : TFT_WHITE);
    c.drawString(viewMenuLabel(row, featureRows, german), 30, y + 15);
  }
  if (n > 3) {
    c.fillRoundRect(230, 14, 4, 102, 2, 0x31A6);
    c.fillRoundRect(230, 14 + first * 102 / n, 4, 102 * 3 / n, 2, 0xAD55);
  }
  drawButtonHints(c, M5.Display.getRotation(), HINT_OK, HINT_NEXT, HINT_NONE);  // B is next, holding it closes
}

static void viewMenuChoose() {
  const int featureRows = viewMenuFeatureRows();
  const ViewMenuRow kind = viewMenuRowKind(viewMenuSel, featureRows);
  if (kind == VIEW_ROW_FEATURE) {
    if (!activeFeature->menuRowChoose(viewMenuSel)) viewMenuStop();
  } else if (kind == VIEW_ROW_TEXT) {
    viewMenuStop();
    leaveModelView();
    drawScreen(response);
  } else {
    viewMenuStop();
  }
}

// Runs from loop() while the menu is open; loop() draws the view (and the menu over it) right after
void viewMenuTick() {
  const uint32_t now = millis();
  // The view went away under it (an alert, a dark screen, another answer): the menu goes with it
  if (displaySleeping || currentPowerState != POWER_ACTIVE || timerAlertActive || !modelViewActive() ||
      currentDisplayMode != SHOW_MODEL || viewMenuFeatureRows() == 0 || viewMenuIdleClose(now, viewMenuLastPress)) {
    viewMenuStop();
    return;
  }
  // Each click of B is the next row at once, holding B closes, C isn't read (menuPressRead, home_menu_core.h). B's
  // clicks are forgotten as they come, so they never add up to a single, double or triple press for loop() later.
  const bool clickedB = M5.BtnB.wasClicked();
  const bool held = M5.BtnB.wasHold();
  const MenuPress press = menuPressRead(M5.BtnA.wasClicked(), clickedB, held);
  if (clickedB || held) forgetButtonBClicks();
  if (held) buttonBHeldForMenu = true;  // Keep holding: no WiFi reset from loop() for this press
  if (press == MENU_PRESS_NONE) return;
  viewMenuLastPress = now;
  resetToActiveState();
  if (press == MENU_PRESS_A) discardButtonA(0);  // The same click's late single-click must not start a recording
  const ViewMenuAction action = viewMenuPress(viewMenuSel, viewMenuCount(viewMenuFeatureRows()), press);
  if (action == VIEW_ACT_CHOOSE) viewMenuChoose();
  if (action == VIEW_ACT_CLOSE) viewMenuStop();
  if (press == MENU_PRESS_A) discardButtonA(0);
  forgetButtonBClicks();
}
