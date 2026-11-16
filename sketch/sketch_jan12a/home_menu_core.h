// The home menu that one press of B opens: the stick's functions, a line, and example questions. Each question
// opens one view from a fixed answer, so choosing one asks no AI and costs nothing.
// Spec: docs/superpowers/specs/2026-11-15-home-menu-design.md
// Plain C++ with no Arduino includes, tested on the host: tests/home_menu/run.sh
#pragma once

#include <stdint.h>

struct HomeQuestion {
  const char* labelDe;     // the row in the menu, at most 174 px in the menu font
  const char* labelEn;
  const char* answerDe;    // one short line without facts: the view brings them from its sources
  const char* answerEn;
  const char* model;       // "<kind>:<query>" as the AI would send it, or "" for a photo
  const char* imageQuery;  // English Wikipedia title: the photo itself, else the fallback when the view can't open
};

// Space first. Never Wolfram (paid) or an AI picture (paid), never war topics.
static const HomeQuestion HOME_QUESTIONS[] = {
    {"Wo ist Orion?", "Where is Orion?", "Hier ist Orion.", "Here is Orion.", "sky:Orion", "Orion (constellation)"},
    {"Wo ist die ISS?", "Where is the ISS?", "Hier ist die ISS.", "Here is the ISS.", "iss:now",
     "International Space Station"},
    {"Mondlandungen", "Moon landings", "Die Mondlandungen nacheinander.", "The Moon landings, one after another.",
     "timeline:moon_missions", "Apollo 11"},
    {"Mondphasen", "Moon phases", "So entstehen die Mondphasen.", "This is how the Moon's phases come about.",
     "physics:moon", "Lunar phase"},
    {"Foto: Marie Curie", "Photo: Marie Curie", "Das ist Marie Curie.", "This is Marie Curie.", "", "Marie Curie"},
    {"T-Rex", "T. rex", "Hier ist der T-Rex.", "Here is T. rex.", "dino:Tyrannosaurus", "Tyrannosaurus"},
    {"Das Herz", "The heart", "Hier ist das Herz.", "Here is the heart.", "anatomy:heart", "Heart"},
    {"DNA in 3D", "DNA in 3D", "Hier ist die DNA.", "Here is DNA.", "protein:dna", "DNA"},
    {"Salzkristall", "Salt crystal", "Hier ist ein Salzkristall.", "Here is a salt crystal.", "crystal:salt",
     "Sodium chloride"},
    {"Wassermolekül", "Water molecule", "Hier ist ein Wassermolekül.", "Here is a water molecule.", "molecule:water",
     "Water"},
    {"Ikosaeder", "Icosahedron", "Hier ist ein Ikosaeder.", "Here is an icosahedron.", "solid:icosahedron",
     "Regular icosahedron"},
    {"Kurve von x²", "Graph of x²", "Hier ist die Kurve von x².", "Here is the graph of x².", "graph:x^2",
     "Quadratic function"},
    {"Matterhorn", "The Matterhorn", "Hier ist das Matterhorn.", "Here is the Matterhorn.", "globe:Matterhorn",
     "Matterhorn"},
    {"Uhrzeit in Tokio", "Time in Tokyo", "Uhrzeit in Tokio.", "Time in Tokyo.", "clock:Tokyo", "Tokyo"},
    {"Pfannkuchen", "Pancakes", "Hier ist ein Rezept für Pfannkuchen.", "Here is a recipe for pancakes.",
     "recipe:pancakes", "Pancake"},
    // The title only, as the music rule asks of the AI: Zitro says so itself when the song doesn't come
    {"Twinkle Twinkle", "Twinkle Twinkle", "Twinkle Twinkle", "Twinkle Twinkle", "music:twinkle-twinkle",
     "Twinkle, Twinkle, Little Star"},
};
static const int HOME_QUESTION_COUNT = sizeof(HOME_QUESTIONS) / sizeof(HOME_QUESTIONS[0]);

// The rows above the line, in this order. "Timer anzeigen" on top: the user wants it prominent.
enum HomeFunction : uint8_t {
  HOME_FN_TIMERS,
  HOME_FN_TOUR,
  HOME_FN_SURPRISE,
  HOME_FN_WORDS,
  HOME_FN_SETTINGS,
  HOME_FN_CLOSE,
  HOME_FN_COUNT
};
static const char* const HOME_FUNCTION_LABELS[HOME_FN_COUNT][2] = {
    {"Timer anzeigen", "Show timers"}, {"Tour starten", "Start the tour"}, {"Überraschung", "Surprise"},
    {"Wörter üben", "Practise words"}, {"Einstellungen", "Settings"},     {"Menü schliessen", "Close menu"},
};

// Every row: the functions, then (below the line) the questions
static const int HOME_ROW_COUNT = HOME_FN_COUNT + HOME_QUESTION_COUNT;

// The question a row holds, or -1 for a function row (or no row at all)
inline int homeRowQuestion(int row) {
  if (row < HOME_FN_COUNT || row >= HOME_ROW_COUNT) return -1;
  return row - HOME_FN_COUNT;
}

// Each row's icon (home_menu_icons.h, 20x20) sits left of the label at x 50, inside the selected row's box at x 20
static const int HOME_ICON_X = 26;

enum HomePress : uint8_t { HOME_PRESS_A, HOME_PRESS_NEXT, HOME_PRESS_BACK };
enum HomeAction : uint8_t { HOME_ACT_REDRAW, HOME_ACT_CLOSE, HOME_ACT_CHOOSE };

// A chooses the selected row, next moves on (and wraps), back closes the menu
inline HomeAction homeMenuPress(uint8_t& sel, HomePress press) {
  if (press == HOME_PRESS_A) return HOME_ACT_CHOOSE;
  if (press == HOME_PRESS_BACK) return HOME_ACT_CLOSE;
  sel = (uint8_t)((sel + 1) % HOME_ROW_COUNT);
  return HOME_ACT_REDRAW;
}

// How a menu reads the buttons, on both sticks (Matthias, 2026-11-16): A chooses, each click of B is the next row at
// once (a double click has no job of its own, so there's no double-click window to wait for), holding B goes back.
// Button C isn't read. Used by the home menu and the trainer's lists. Pass M5.BtnA.wasClicked(), M5.BtnB.wasClicked(),
// M5.BtnB.wasHold().
enum MenuPress : uint8_t { MENU_PRESS_NONE, MENU_PRESS_A, MENU_PRESS_NEXT, MENU_PRESS_BACK };

inline MenuPress menuPressRead(bool clickedA, bool clickedB, bool heldB) {
  if (clickedA) return MENU_PRESS_A;
  if (heldB) return MENU_PRESS_BACK;
  return clickedB ? MENU_PRESS_NEXT : MENU_PRESS_NONE;
}

// Forget a button's clicks so far: M5Unified resets its click count when the button leaves the "decide click count"
// state. A menu calls it for every click of B it reads, so its clicks never come back to loop() as a single, double
// or triple press (the setup page). Tested against M5Unified's own Button_Class: tests/home_menu/test_button_clicks.cpp
template <class Button>
inline void buttonForgetClicks(Button& b, uint32_t msec) {
  b.setState(msec, Button::state_decide_click_count);
  b.setState(msec, Button::state_nochange);
}

inline HomePress homePressFromMenu(MenuPress p) {
  return p == MENU_PRESS_A ? HOME_PRESS_A : p == MENU_PRESS_BACK ? HOME_PRESS_BACK : HOME_PRESS_NEXT;
}

// The first of the 3 visible rows, as the trainer's menu scrolls: the selected row in the middle where it can be
inline int homeFirstVisible(int sel, int count) {
  int first = sel - 1;
  if (first > count - 3) first = count - 3;
  if (first < 0) first = 0;
  return first;
}

// With the night lock on (8 pm to 6 am), learning and playing wait for the morning, as a spoken question does.
// The timers, the settings and closing still work.
inline bool homeMenuBlockedAtNight(int row) {
  return row != HOME_FN_TIMERS && row != HOME_FN_SETTINGS && row != HOME_FN_CLOSE;
}
