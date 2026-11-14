// The board Zitro is built for. One sketch, two sticks; the build picks the board (verify.sh
// compiles both). Functions live here, not in the .ino, so Arduino's generated prototypes stay
// below the types they use.
#pragma once

#include <Arduino.h>
#include <M5Unified.h>

// M5StickC Plus2: ESP32, buttons A (front), B (side) and C (PWR, other side), GPIO4 keeps it powered.
// M5Stack StickS3: ESP32-S3, buttons A (KEY1) and B (KEY2) only; the power button belongs to its
// power chip (M5PM1). ES8311 codec with a real speaker.
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  #define ZITRO_STICKS3 1
  #define BUTTON_A_GPIO GPIO_NUM_11
  #define BUTTON_B_GPIO GPIO_NUM_12
#else
  #define ZITRO_STICKS3 0
  #define BUTTON_A_GPIO GPIO_NUM_37
  #define BUTTON_B_GPIO GPIO_NUM_39
#endif

// The StickS3 can restart on battery above 75 % volume (M5Stack's StickS3 documentation)
inline int speakerVolume(int volume) {
  volume = constrain(volume, 0, 255);
#if ZITRO_STICKS3
  if (volume > 191) volume = 191;
#endif
  return volume;
}

// M5Unified before 0.2.29 breaks the StickS3: the microphone is never switched on, ESP32-S3
// audio samples come out of order and the clock chip is driven as the wrong part. Its build
// takes the pinned libraries from scripts/sticks3_libraries.sh (verify.sh does this).
#if ZITRO_STICKS3 && (M5UNIFIED_VERSION_MAJOR == 0 && M5UNIFIED_VERSION_MINOR == 2 && M5UNIFIED_VERSION_PATCH < 29)
  #error "The StickS3 build needs M5Unified 0.2.29 or later: build it with verify.sh"
#endif

// On the StickS3 the microphone and the speaker share one codec and its I2S pins, so only one
// of them runs at a time. The Plus2 has separate parts and keeps its speaker running.
inline bool micStart() {
#if ZITRO_STICKS3
  M5.Speaker.end();
#endif
  return M5.Mic.begin();
}

inline void micStop() {
  M5.Mic.end();
#if ZITRO_STICKS3
  M5.Speaker.begin();
#endif
}

// The StickS3 has no Button C, so Button B also does its job there: one click is B, a double click
// is C, holding it is holding C in a view where holding B does nothing, and three quick clicks open
// the setup page (three presses of C on the Plus2). A single click on B then counts once the
// double-click window has passed.
inline bool buttonBClicked() {
#if ZITRO_STICKS3
  return M5.BtnB.wasSingleClicked();
#else
  return M5.BtnB.wasClicked();
#endif
}

inline bool buttonCClicked() {
#if ZITRO_STICKS3
  return M5.BtnB.wasDoubleClicked();
#else
  return M5.BtnPWR.wasClicked();
#endif
}

inline bool buttonCHeld() {
#if ZITRO_STICKS3
  return M5.BtnB.wasHold();
#else
  return M5.BtnPWR.wasHold();
#endif
}

// A double click of Button B has its own job in some views (coloured notes in music). On the
// StickS3 a double click is Button C, so that job isn't there.
inline bool buttonBDoubleClicked() {
#if ZITRO_STICKS3
  return false;
#else
  return M5.BtnB.wasDoubleClicked();
#endif
}

inline int btnCClickCount = 0;
inline unsigned long btnCLastClickTime = 0;
const unsigned long TRIPLE_CLICK_TIMEOUT = 500;  // 500ms window for triple-click

// True once the three presses that open the setup page are done. Call it every loop.
// `counting` is false where Button C has another job, so presses there don't add up.
inline bool setupPressesDone(bool counting) {
#if ZITRO_STICKS3
  return counting && M5.BtnB.wasDecideClickCount() && M5.BtnB.getClickCount() >= 3;
#else
  // Presses, not clicks, so this doesn't get in the way of Button C's other handlers
  static bool wasPressed = false;
  bool isPressed = M5.BtnPWR.isPressed();
  bool pressedNow = isPressed && !wasPressed;
  wasPressed = isPressed;
  if (!pressedNow || !counting) return false;

  unsigned long now = millis();
  if (now - btnCLastClickTime > TRIPLE_CLICK_TIMEOUT) btnCClickCount = 0;
  btnCClickCount++;
  btnCLastClickTime = now;
  Serial.printf("Button C press detected (count: %d)\n", btnCClickCount);
  if (btnCClickCount < 3) return false;
  btnCClickCount = 0;
  return true;
#endif
}
