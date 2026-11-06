// What Button B and Button C do depends on how the stick is held and what's on screen.
// Plain C++, tested on the host: tests/model/run.sh
#pragma once

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
