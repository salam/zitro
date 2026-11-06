// School physics scenes: computed and animated on the device, no network. See physics_core.h.
#include "physics_draw.h"

static int physicsSceneIndex = -1;
static bool physicsGerman = true;
static PhysicsState* physicsState = nullptr;

static void physicsEnd() {
  free(physicsState);
  physicsState = nullptr;
}

static bool physicsFetch(const char* query) {
  physicsSceneIndex = physicsFindScene(query);
  if (physicsSceneIndex < 0) return false;
  Preferences settings;
  settings.begin("settings", true);
  physicsGerman = settings.getString("language", "DE") != "EN";
  settings.end();
  modelLabel = physicsGerman ? PHYSICS_SCENES[physicsSceneIndex].de : PHYSICS_SCENES[physicsSceneIndex].en;
  modelSourceName = physicsGerman ? "berechnet" : "computed";
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
  physicsState->scene = PHYSICS_SCENES[physicsSceneIndex].scene;
  physicsState->german = physicsGerman;
  physicsState->big = FontManager::getFont16pt();
  physicsState->normal = FontManager::getFont12pt();
  physicsState->clockKnown = physicsDayFromClock((double)time(nullptr), physicsState->startDay);
  physicsSceneReset(*physicsState);
  return true;
}

static void physicsFrame(M5Canvas& c, float dt) {
  if (!physicsState) return;
  physicsDrawFrame(c, *physicsState, dt);
  eduDrawFooter(c, 0);
}

static void physicsButtonB() {
  if (physicsState) physicsSceneNextParam(*physicsState);
}

// Button C goes back to the text in every scene: nobody gets stuck in one.
const EduFeature FEATURE_PHYSICS = {"physics",    "",           PHYSICS_PROMPT_RULE, false,          physicsFetch, nullptr,
                                    physicsBegin, physicsFrame, physicsEnd,          physicsButtonB, nullptr,      nullptr};
