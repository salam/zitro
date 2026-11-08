// School physics scenes: computed and animated on the device, no network. See physics_core.h.
#include "physics_draw.h"

static int physicsSceneIndex = -1;
static uint8_t physicsParam = 0;  // Kept while the view closes and opens again: Button C and back
static bool physicsGerman = true;
static PhysicsState* physicsState = nullptr;
static const int PHYSICS_SPEAKER_CHANNEL = 1;  // As music; reading aloud and the alert use channel 0
static bool physicsSounded = false;

static void physicsEnd() {
  // Each tone ends by itself within two frames; this stops the last one at once
  if (physicsSounded) M5.Speaker.stop(PHYSICS_SPEAKER_CHANNEL);
  physicsSounded = false;
  free(physicsState);
  physicsState = nullptr;
}

// The label must always name what's on screen, and an atom's name changes with Button B
static void physicsSetLabel() {
  const PhysicsSceneInfo& info = PHYSICS_SCENES[physicsSceneIndex];
  if (info.scene == PHYSICS_ATOM) {
    const PhysicsElement& element = PHYSICS_ELEMENTS[physicsParam];
    modelLabel = physicsGerman ? element.de : element.en;
    modelSourceName = "PubChem";
  } else {
    modelLabel = physicsGerman ? info.de : info.en;
    modelSourceName = physicsGerman ? "berechnet" : "computed";
  }
}

static bool physicsFetch(const char* query) {
  int element = 0;
  if (!physicsParseQuery(query, physicsSceneIndex, element)) return false;
  physicsParam = element > 0 ? (uint8_t)(element - 1) : 0;
  Preferences settings;
  settings.begin("settings", true);
  physicsGerman = settings.getString("language", "DE") != "EN";
  settings.end();
  physicsSetLabel();
  return true;
}

static bool physicsBegin(M5Canvas& c) {
  (void)c;
  if (physicsSceneIndex < 0) return false;
  physicsEnd();
  physicsState = (PhysicsState*)ps_malloc(sizeof(PhysicsState));
  if (!physicsState) return false;
  memset(physicsState, 0, sizeof(PhysicsState));
  physicsState->sceneIndex = (uint8_t)physicsSceneIndex;
  physicsState->param = physicsParam;
  physicsState->scene = PHYSICS_SCENES[physicsSceneIndex].scene;
  physicsState->german = physicsGerman;
  physicsState->big = FontManager::getFont16pt();
  physicsState->normal = FontManager::getFont12pt();
  physicsState->clockKnown = physicsDayFromClock((double)time(nullptr), physicsState->startDay);
  // HOME_PLACE_HOOK: set from the home city setting
  physicsState->homeLatitude = PHYSICS_HOME_LATITUDE;
  physicsState->homeLongitude = PHYSICS_HOME_LONGITUDE;
  physicsState->random = (uint32_t)esp_random();
  physicsSceneReset(*physicsState);
  return true;
}

static void physicsFrame(M5Canvas& c, float dt) {
  if (!physicsState) return;
  physicsDrawFrame(c, *physicsState, dt);
  eduDrawFooter(c, 0);
  // A sound the scene asked for: a short tone at the device's volume, never waited for
  if (physicsState->toneMs) {
    M5.Speaker.begin();  // Sleep switches it off; returns at once if it's running
    M5.Speaker.tone(physicsState->toneHz, physicsState->toneMs, PHYSICS_SPEAKER_CHANNEL, true);
    physicsState->toneMs = 0;
    physicsSounded = true;
  }
}

static void physicsButtonB() {
  if (!physicsState) return;
  physicsSceneNextParam(*physicsState);
  physicsParam = physicsState->param;
  physicsSetLabel();
}

// B changes something in the scene, if it has more than one setting; C is the written answer, as by default
static void physicsHints(int& kindB, int& kindC) {
  (void)kindC;
  kindB = (physicsState && PHYSICS_SCENES[physicsState->sceneIndex].paramCount > 1) ? HINT_NEXT : HINT_NONE;
}

// Button C goes back to the text in every scene: nobody gets stuck in one.
const EduFeature FEATURE_PHYSICS = {"physics",    "",           PHYSICS_PROMPT_RULE, false,          physicsFetch, nullptr,
                                    physicsBegin, physicsFrame, physicsEnd,          physicsButtonB, nullptr,      nullptr, nullptr, physicsHints};
