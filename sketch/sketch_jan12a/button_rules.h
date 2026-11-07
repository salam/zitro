// What Button B and Button C do depends on how the stick is held and what's on screen.
// Plain C++, tested on the host: tests/model/run.sh
#pragma once

#include <stdint.h>

// Scroll direction of a picture: -1 up, 1 down. The upper button scrolls up.
// Rotation 3 has the buttons on the left, Button C on top; rotation 1 is the same view
// turned by half a circle, with Button B on top.
inline int imageScrollDirection(int rotation, bool buttonC) {
  const bool buttonIsOnTop = (rotation == 3) == buttonC;
  return buttonIsOnTop ? -1 : 1;
}

// Three quick Button C presses open the settings only where Button C has no other job:
// in lists it steps, on a picture it scrolls, and with a picture or model it switches views.
inline bool buttonCCountsForSettings(bool listOrAlertOpen, bool hasPicture, bool hasModel) {
  return !listOrAlertOpen && !hasPicture && !hasModel;
}

// Button tabs in a result view: shown when the view opens and after each button press, then gone,
// so the content has the whole screen. Times are millis() values, 32 bits as on the stick, so the subtraction survives the wrap.
static const uint32_t MODEL_HINT_MS = 3000;
inline bool modelHintsShowing(uint32_t now, uint32_t shownAt) {
  return now - shownAt < MODEL_HINT_MS;
}

// A model only turns by itself once the hand has let it be for a while: lying on the table, not
// held still for a moment to look at something.
static const uint32_t MODEL_IDLE_AFTER_HAND_MS = 3000;
inline bool modelIdleTurnAllowed(uint32_t now, uint32_t handTurnedAt) {
  return now - handTurnedAt >= MODEL_IDLE_AFTER_HAND_MS;
}
