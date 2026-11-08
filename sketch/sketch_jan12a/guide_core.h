// Pure logic of the guided tour: what every button does, the menu, and the content of the grid.
// Plain C++ with no Arduino includes, tested on the host: tests/guide/run.sh
// The drawing is in guide.ino and guide_tiles.ino. Spec: docs/superpowers/specs/2026-11-08-guided-tour-design.md
#pragma once

#include <math.h>
#include <stdint.h>

enum GuideStep : uint8_t { GUIDE_OFF, GUIDE_ASK, GUIDE_RETRY, GUIDE_ANSWER, GUIDE_PHOTO, GUIDE_SKY, GUIDE_MUSIC, GUIDE_GRID, GUIDE_DONE };
enum GuideButton : uint8_t { GUIDE_BTN_A, GUIDE_BTN_B, GUIDE_BTN_C };
// GUIDE_EV_STEP: the step changed. GUIDE_EV_MENU: the menu opened, closed or moved.
enum GuideEvent : uint8_t { GUIDE_EV_NONE, GUIDE_EV_RECORD, GUIDE_EV_STEP, GUIDE_EV_PAGE, GUIDE_EV_QUOTE_SKIP, GUIDE_EV_MENU, GUIDE_EV_FINISH, GUIDE_EV_FINISH_ASK, GUIDE_EV_ABORT };
enum GuideVoice : uint8_t { GUIDE_VOICE_ANSWERED, GUIDE_VOICE_NOT_HEARD, GUIDE_VOICE_CANCELLED, GUIDE_VOICE_BLOCKED };
enum GuideMenuItem : uint8_t { GUIDE_MENU_GO, GUIDE_MENU_BACK, GUIDE_MENU_END };

struct GuideState {
  GuideStep step;
  uint8_t page;   // Grid page
  bool quote;     // The "you ask" screen comes before this step's view
  bool menuOpen;
  uint8_t menuSel;
  bool answered;  // The first question got an answer: back from the photo goes there, otherwise to the retry screen
};

struct GuideTile {
  const char* icon;
  const char* titleDe;  // 12 pt font, has umlauts
  const char* sayDe;    // DejaVu9, ASCII only
  const char* titleEn;
  const char* sayEn;
};

struct GuideRotate {
  bool sawPortrait;
};

static const int GUIDE_PAGES = 4;
static const int GUIDE_TILES_PER_PAGE = 4;
static const uint32_t GUIDE_QUOTE_MS = 1900;
static const uint32_t GUIDE_HEARD_MS = 1800;
static const uint32_t GUIDE_KEEP_AWAKE_MS = 300000UL;  // As long as MODEL_MOTION_CAP_MS
static const char* const GUIDE_SKY_REF = "sky:Orion";
static const char* const GUIDE_MUSIC_REF = "music:twinkle-twinkle";

static const GuideTile GUIDE_TILES[GUIDE_PAGES * GUIDE_TILES_PER_PAGE] = {
    {"print", "Drucken", "Bild zeigen, B 1 Sek. halten", "Print", "Show picture, hold B 1 s"},
    {"time", "Zeitstrahl", "Zeitstrahl der Raumfahrt", "Timeline", "Timeline of spaceflight"},
    {"mol", "Moleküle", "Wie ist Koffein aufgebaut?", "Molecule", "How is caffeine built?"},
    {"body", "Körper", "Was ist im Herz?", "Body", "What's inside the heart?"},
    {"globe", "Globus", "Wo liegt das Matterhorn?", "Globe", "Where is the Matterhorn?"},
    {"iss", "ISS live", "Wo ist die ISS gerade?", "ISS live", "Where is the ISS?"},
    {"physics", "Physik", "Wie funktioniert ein Hebel?", "Physics", "How does a lever work?"},
    {"graph", "Kurven", "Zeichne sin(x)", "Curves", "Draw sin(x)"},
    {"math", "Rechnen", "Was ist 12 mal 34?", "Math", "What is 12 times 34?"},
    {"timer", "Timer", "Timer auf 5 Minuten", "Timer", "Timer for 5 minutes"},
    {"recipe", "Rezepte", "Wie mache ich Pfannkuchen?", "Recipes", "How do I make pancakes?"},
    {"clock", "Weltuhr", "Wie viel Uhr ist es in Tokio?", "Clock", "What time is it in Tokyo?"},
    {"speak", "Vorlesen", "M5 zweimal schnell", "Speak", "Press M5 twice"},
    {"shake", "Schütteln", "Bringt etwas Neues", "Shake", "For a surprise"},
    {"paint", "KI-Bilder", "Male einen Drachen mit Hut", "AI art", "Paint a dragon with a hat"},
    {"camera", "Kamera", "M5 dreimal, mit Kamera-Modul", "Camera", "M5 3x, with camera module"},
};

inline void guideStart(GuideState& s) {
  s.step = GUIDE_ASK;
  s.page = 0;
  s.quote = false;
  s.menuOpen = false;
  s.menuSel = 0;
  s.answered = false;
}

inline int guideMenuRows(const GuideState& s, GuideMenuItem out[3]) {
  int n = 0;
  out[n++] = GUIDE_MENU_GO;
  if (s.step != GUIDE_ASK) out[n++] = GUIDE_MENU_BACK;
  out[n++] = GUIDE_MENU_END;
  return n;
}

inline void guideEnter(GuideState& s, GuideStep step, uint8_t page, bool quote) {
  s.step = step;
  s.page = page;
  s.quote = quote;
}

inline GuideEvent guideStepBack(GuideState& s) {
  switch (s.step) {
    case GUIDE_ANSWER:
    case GUIDE_RETRY: guideEnter(s, GUIDE_ASK, 0, false); return GUIDE_EV_STEP;
    case GUIDE_PHOTO: guideEnter(s, s.answered ? GUIDE_ANSWER : GUIDE_RETRY, 0, false); return GUIDE_EV_STEP;
    case GUIDE_SKY: guideEnter(s, GUIDE_PHOTO, 0, false); return GUIDE_EV_STEP;
    case GUIDE_MUSIC: guideEnter(s, GUIDE_SKY, 0, false); return GUIDE_EV_STEP;
    case GUIDE_GRID:
      if (s.page > 0) {
        s.page--;
        return GUIDE_EV_PAGE;
      }
      guideEnter(s, GUIDE_MUSIC, 0, false);
      return GUIDE_EV_STEP;
    case GUIDE_DONE: guideEnter(s, GUIDE_GRID, GUIDE_PAGES - 1, false); return GUIDE_EV_STEP;
    default: return GUIDE_EV_NONE;
  }
}

inline GuideEvent guideStepForward(GuideState& s) {
  switch (s.step) {
    case GUIDE_RETRY:
    case GUIDE_ANSWER: guideEnter(s, GUIDE_PHOTO, 0, true); return GUIDE_EV_STEP;
    case GUIDE_PHOTO: guideEnter(s, GUIDE_SKY, 0, true); return GUIDE_EV_STEP;
    case GUIDE_SKY: guideEnter(s, GUIDE_MUSIC, 0, true); return GUIDE_EV_STEP;
    case GUIDE_MUSIC: guideEnter(s, GUIDE_GRID, 0, false); return GUIDE_EV_STEP;
    case GUIDE_GRID:
      if (s.page + 1 < GUIDE_PAGES) {
        s.page++;
        return GUIDE_EV_PAGE;
      }
      guideEnter(s, GUIDE_DONE, 0, false);
      return GUIDE_EV_STEP;
    default: return GUIDE_EV_NONE;
  }
}

inline GuideEvent guideMenuPress(GuideState& s, GuideButton b) {
  GuideMenuItem rows[3];
  const int count = guideMenuRows(s, rows);
  if (b == GUIDE_BTN_C) {
    s.menuSel = (s.menuSel + 1) % count;
    return GUIDE_EV_MENU;
  }
  s.menuOpen = false;
  if (b == GUIDE_BTN_B) return GUIDE_EV_MENU;
  const GuideMenuItem chosen = rows[s.menuSel];
  if (chosen == GUIDE_MENU_GO) return GUIDE_EV_MENU;
  if (chosen == GUIDE_MENU_BACK) return guideStepBack(s);
  s.step = GUIDE_OFF;
  return GUIDE_EV_FINISH;
}

inline GuideEvent guidePress(GuideState& s, GuideButton b) {
  if (s.step == GUIDE_OFF) return GUIDE_EV_NONE;
  if (s.menuOpen) return guideMenuPress(s, b);
  if (b == GUIDE_BTN_B) {
    s.menuOpen = true;
    s.menuSel = 0;
    return GUIDE_EV_MENU;
  }
  // Here M5 is the task itself, so it doesn't open the menu
  if (s.step == GUIDE_ASK || s.step == GUIDE_RETRY) {
    if (b == GUIDE_BTN_A) return GUIDE_EV_RECORD;
    return s.step == GUIDE_RETRY ? guideStepForward(s) : GUIDE_EV_NONE;
  }
  if (s.step == GUIDE_DONE) {
    if (b != GUIDE_BTN_A) return GUIDE_EV_NONE;
    s.step = GUIDE_OFF;
    return GUIDE_EV_FINISH_ASK;
  }
  if (b == GUIDE_BTN_A) {
    s.menuOpen = true;
    s.menuSel = 0;
    return GUIDE_EV_MENU;
  }
  if (s.quote) {
    s.quote = false;
    return GUIDE_EV_QUOTE_SKIP;
  }
  return guideStepForward(s);
}

inline GuideEvent guideVoiceResult(GuideState& s, GuideVoice v) {
  switch (v) {
    case GUIDE_VOICE_ANSWERED:
      s.answered = true;
      guideEnter(s, GUIDE_ANSWER, 0, false);
      return GUIDE_EV_STEP;
    case GUIDE_VOICE_NOT_HEARD:
      guideEnter(s, GUIDE_RETRY, 0, false);
      return GUIDE_EV_STEP;
    case GUIDE_VOICE_CANCELLED:
      if (s.step == GUIDE_ASK) return GUIDE_EV_NONE;
      guideEnter(s, GUIDE_ASK, 0, false);
      return GUIDE_EV_STEP;
    default:
      s.step = GUIDE_OFF;
      return GUIDE_EV_ABORT;
  }
}

inline bool guideShouldAutoStart(bool done, bool deepSleepWake, bool chatKey, bool voiceKey) {
  return !done && !deepSleepWake && chatKey && voiceKey;
}

// Button C is "next" during the tour, so three quick presses must not open the settings
inline bool guideBlocksSettings(const GuideState& s) {
  return s.step != GUIDE_OFF;
}

inline bool guideAcceptsPress(bool displayAwake, bool inWakeCooldown) {
  return displayAwake && !inWakeCooldown;
}

inline bool guideKeepAwake(uint32_t now, uint32_t lastPress) {
  return now - lastPress < GUIDE_KEEP_AWAKE_MS;
}

// Upright and back again, as everywhere else on the stick
inline bool guideRotateStep(GuideRotate& r, bool landscape) {
  if (!landscape) {
    r.sawPortrait = true;
    return false;
  }
  const bool completed = r.sawPortrait;
  r.sawPortrait = false;
  return completed;
}

inline float guideSlideStep(float pos, int target, float dt) {
  const float next = pos + ((float)target - pos) * (1.0f - (float)exp(-12.0f * dt));
  return fabsf(next - (float)target) < 0.004f ? (float)target : next;
}
