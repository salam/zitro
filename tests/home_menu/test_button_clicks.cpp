// Runs M5Unified's own Button_Class on the host with simulated presses, to check that a menu which forgets Button B's
// clicks as it reads them (buttonForgetClicks, home_menu_core.h) never hands them back to loop() as a single, double
// or triple press: the triple press opens the setup page (Matthias's hand test, 2026-11-16).
#include <cstdio>
#include <cstdint>
#include "utility/Button_Class.hpp"
#include "../../sketch/sketch_jan12a/home_menu_core.h"

static int failures = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { failures++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

using m5::Button_Class;

// One stick: B as the firmware sets it up (setup(): debounce 10 ms, hold 1000 ms), updated every 10 ms
struct Sim {
  Button_Class b;
  uint32_t t = 1000;
  Sim() {
    b.setDebounceThresh(10);
    b.setHoldThresh(1000);
    b.setRawState(t, false);
  }
  void step(bool pressed) {
    t += 10;
    b.setRawState(t, pressed);
  }
};

// What loop() would see afterwards: any decided click count (single = home menu, double = C, three = setup page)
static int decidedAfter(Sim& s, int ms) {
  int decided = 0;
  for (int k = 0; k < ms / 10; k++) {
    s.step(false);
    if (s.b.wasDecideClickCount() && s.b.getClickCount() > 0) decided = s.b.getClickCount();
  }
  return decided;
}

// Three quick taps, 60 ms down and 60 ms up. With `forget`, each click is read as the menu reads it and then forgotten.
static int tapThree(Sim& s, bool forget, int& nexts) {
  nexts = 0;
  for (int tap = 0; tap < 3; tap++) {
    for (int k = 0; k < 6; k++) s.step(true);
    for (int k = 0; k < 6; k++) {
      s.step(false);
      const bool clicked = s.b.wasClicked();
      if (menuPressRead(false, clicked, s.b.wasHold()) == MENU_PRESS_NEXT) nexts++;
      if (forget && clicked) buttonForgetClicks(s.b, s.t);
    }
  }
  return decidedAfter(s, 2000);
}

static void test_control_three_taps_open_setup() {
  Sim s;
  int nexts = 0;
  CHECK(tapThree(s, false, nexts) == 3);  // without forgetting, loop() sees the triple press: the bug Matthias hit
  CHECK(nexts == 3);
}

static void test_menu_forgets_its_taps() {
  Sim s;
  int nexts = 0;
  CHECK(tapThree(s, true, nexts) == 0);  // nothing decided afterwards: no menu, no C, no setup page
  CHECK(nexts == 3);                     // and each tap still moved one row
}

// B pressed during a recording: the recorder waits for the release, then forgets (releaseButtonBAfterCancel)
static void test_cancel_press_is_forgotten() {
  Sim s;
  bool pressed = false;
  for (int k = 0; k < 3 && !pressed; k++) {  // the press shows after the 10 ms debounce
    s.step(true);
    pressed = s.b.wasPressed();
  }
  CHECK(pressed);
  for (int k = 0; k < 12 && s.b.isPressed(); k++) s.step(k < 8);  // held 80 ms more, then let go
  while (s.b.isPressed()) s.step(false);
  buttonForgetClicks(s.b, s.t);
  CHECK(decidedAfter(s, 2000) == 0);  // no single click left over to open the home menu
}

static void test_hold_is_back_and_leaves_nothing() {
  Sim s;
  bool back = false;
  for (int k = 0; k < 130; k++) {  // 1.3 s down: past the hold threshold
    s.step(true);
    if (menuPressRead(false, s.b.wasClicked(), s.b.wasHold()) == MENU_PRESS_BACK) back = true;
  }
  CHECK(back);
  buttonForgetClicks(s.b, s.t);
  s.step(false);
  CHECK(!s.b.wasClicked());  // a hold isn't also a click
  CHECK(decidedAfter(s, 2000) == 0);
}

int main() {
  test_control_three_taps_open_setup();
  test_menu_forgets_its_taps();
  test_cancel_press_is_forgotten();
  test_hold_is_back_and_leaves_nothing();
  printf("button clicks: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
