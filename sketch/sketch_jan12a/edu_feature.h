// Plug-in structure for what Zitro can show besides pictures: molecules, music, graphs and so on.
// Each feature lives in its own feature_<name>.ino and fills in one EduFeature.
//
// The rule that keeps it honest: the AI only names what to look up, in the answer field
// "model" ("<kind>:<query>"). Everything shown as fact comes from a real source, a checked
// table or exact math. If there's nothing, fetch() returns false and Zitro shows the picture.
#pragma once

#include <M5Unified.h>

#include "model_core.h"

static const unsigned long EDU_LOOKUP_DEADLINE_MS = 10000;
static const int EDU_FOOTER_HEIGHT = 18;
// Prepared data on the Zitro server: webpage2/data/ in this repo
#define EDU_DATA_BASE_URL "https://zitro.spot/data/"

struct EduFeature {
  const char* kind;    // As in "<kind>:<query>"; listed in MODEL_KINDS
  const char* source;  // Named on the bottom line; fetch() may change modelSourceName
  const char* promptRule;  // Added to the AI prompt, in German: when to set "model". nullptr: never asked for
  bool turnsWithGyro;      // True: the view turns modelRotation with the hand and idles slowly by itself
  bool (*fetch)(const char* query);  // Load the data, set modelLabel. No screen, no sprite. nullptr: not available
  void (*unload)();                  // Free what fetch() loaded. May be nullptr
  bool (*begin)(M5Canvas& c);        // The view opens: allocate what drawing needs. May be nullptr
  void (*frame)(M5Canvas& c, float dt);  // Draw one whole frame, dt in seconds. Up to 25 per second; never block
  void (*end)();                         // The view closes (may reopen later without fetch). May be nullptr
  void (*buttonB)();                     // Button B click. nullptr: modelZoomStep cycles 0, 1, 2
  bool (*buttonC)();                     // Button C click. nullptr or false: back to the text
  bool (*keepAwake)();                   // True while something plays: the sleep timers wait. May be nullptr
};

// One per kind. All are registered in feature_registry.ino.
extern const EduFeature FEATURE_MOLECULE, FEATURE_MUSIC, FEATURE_GRAPH, FEATURE_RECIPE, FEATURE_GLOBE, FEATURE_PROTEIN,
    FEATURE_CRYSTAL, FEATURE_SKY, FEATURE_PHYSICS, FEATURE_SOLID, FEATURE_TIMELINE, FEATURE_ANATOMY;
extern const EduFeature* const EDU_FEATURES[];
extern const int EDU_FEATURE_COUNT;
extern const EduFeature* activeFeature;

// Shared state
extern String modelLabel;       // Name on the bottom line; set by fetch(), from the source record
extern String modelSourceName;  // Source on the bottom line
extern Mat3 modelRotation;      // Current turn, for features with turnsWithGyro
extern uint8_t modelZoomStep;   // 0, 1 or 2
extern float modelGyroDps[3];   // Hand motion in screen axes, degrees per second; 0 when the gyro is off

// Shared helpers
String eduUrlEncode(const char* s);
// GET into a buffer. True only for HTTP 200 with a complete body inside the deadline (a millis() value).
bool eduHttpGet(const String& url, unsigned long deadline, char* buffer, size_t capacity, size_t& bodyLength);
// Draws the bottom line: modelLabel and modelSourceName. reserveRight keeps room for a legend.
void eduDrawFooter(M5Canvas& c, int reserveRight);
