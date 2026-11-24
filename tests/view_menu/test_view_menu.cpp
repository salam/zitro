// The menu a view can bring (the wave lab's sound): its rows, its presses, closing by itself.
#include <cstring>
#include "../physics/test_common.h"
#include "../../sketch/sketch_jan12a/view_menu_core.h"

static void testRows() {
  CHECK(viewMenuCount(1) == 3);
  CHECK(viewMenuRowKind(0, 1) == VIEW_ROW_FEATURE);
  CHECK(viewMenuRowKind(1, 1) == VIEW_ROW_TEXT);
  CHECK(viewMenuRowKind(2, 1) == VIEW_ROW_CLOSE);
  CHECK(viewMenuRowKind(0, 0) == VIEW_ROW_TEXT);
  CHECK(viewMenuRowKind(1, 0) == VIEW_ROW_CLOSE);
  CHECK(strcmp(viewMenuFixedLabel(VIEW_ROW_TEXT, true), "Text") == 0);
  CHECK(strcmp(viewMenuFixedLabel(VIEW_ROW_TEXT, false), "Text") == 0);
  CHECK(strcmp(viewMenuFixedLabel(VIEW_ROW_CLOSE, true), "Schliessen") == 0);
  CHECK(strcmp(viewMenuFixedLabel(VIEW_ROW_CLOSE, false), "Close") == 0);
}

static void testPresses() {
  uint8_t sel = 2;
  CHECK(viewMenuPress(sel, 3, MENU_PRESS_NEXT) == VIEW_ACT_REDRAW);
  CHECK(sel == 0);  // Wraps round
  CHECK(viewMenuPress(sel, 3, MENU_PRESS_NEXT) == VIEW_ACT_REDRAW);
  CHECK(sel == 1);
  CHECK(viewMenuPress(sel, 3, MENU_PRESS_A) == VIEW_ACT_CHOOSE);
  CHECK(sel == 1);
  CHECK(viewMenuPress(sel, 3, MENU_PRESS_BACK) == VIEW_ACT_CLOSE);
  CHECK(viewMenuPress(sel, 3, MENU_PRESS_NONE) == VIEW_ACT_NONE);
  CHECK(sel == 1);
}

static void testIdleClose() {
  CHECK(viewMenuIdleClose(20000, 0));
  CHECK(!viewMenuIdleClose(19999, 0));
  CHECK(!viewMenuIdleClose(5, 0xFFFFFFF0u));  // millis() wrapped
}

int main() {
  testRows();
  testPresses();
  testIdleClose();
  return finish("view menu");
}
