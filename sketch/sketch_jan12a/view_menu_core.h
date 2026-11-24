// A small menu inside a view, for a view that brings its own rows (the wave lab's "Ton an"). The view's rows come
// first, then "Text" (back to the written answer) and "Schliessen". It presses like the home menu: A chooses, each
// click of B is the next row, holding B closes. Interim: the button module's shared menu (feature/button-ui) is to
// take the view's rows over. Plain C++, tested on the host: tests/view_menu/run.sh
#pragma once

#include <stdint.h>
#include "home_menu_core.h"  // MenuPress, menuPressRead(), homeFirstVisible()

enum ViewMenuRow : uint8_t { VIEW_ROW_FEATURE, VIEW_ROW_TEXT, VIEW_ROW_CLOSE };

inline int viewMenuCount(int featureRows) { return featureRows + 2; }

inline ViewMenuRow viewMenuRowKind(int row, int featureRows) {
  if (row < featureRows) return VIEW_ROW_FEATURE;
  return row == featureRows ? VIEW_ROW_TEXT : VIEW_ROW_CLOSE;
}

inline const char* viewMenuFixedLabel(ViewMenuRow kind, bool german) {
  if (kind == VIEW_ROW_TEXT) return "Text";
  return german ? "Schliessen" : "Close";
}

enum ViewMenuAction : uint8_t { VIEW_ACT_NONE, VIEW_ACT_REDRAW, VIEW_ACT_CHOOSE, VIEW_ACT_CLOSE };

inline ViewMenuAction viewMenuPress(uint8_t& sel, int count, MenuPress press) {
  if (press == MENU_PRESS_A) return VIEW_ACT_CHOOSE;
  if (press == MENU_PRESS_BACK) return VIEW_ACT_CLOSE;
  if (press == MENU_PRESS_NONE || count <= 0) return VIEW_ACT_NONE;
  sel = (uint8_t)((sel + 1) % count);
  return VIEW_ACT_REDRAW;
}

// Left alone this long, the menu closes by itself, as the button module's menus do
static const uint32_t VIEW_MENU_IDLE_CLOSE_MS = 20000;
inline bool viewMenuIdleClose(uint32_t now, uint32_t lastPress) { return now - lastPress >= VIEW_MENU_IDLE_CLOSE_MS; }
