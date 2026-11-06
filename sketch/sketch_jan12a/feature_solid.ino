// Geometric solids: computed on the device, no network. See solid_core.h and edu_feature.h.
#include "solid_draw.h"

static int solidIndex = -1;
static bool solidGerman = true;
static SolidState* solidState = nullptr;

static void solidEnd() {
  free(solidState);
  solidState = nullptr;
}

static bool solidFetch(const char* query) {
  solidIndex = solidFindId(query);
  if (solidIndex < 0) return false;
  Preferences settings;
  settings.begin("settings", true);
  solidGerman = settings.getString("language", "DE") != "EN";
  settings.end();
  modelLabel = solidGerman ? SOLID_TABLE[solidIndex].de : SOLID_TABLE[solidIndex].en;
  modelSourceName = solidGerman ? "berechnet" : "computed";
  return true;
}

static bool solidBegin(M5Canvas& c) {
  (void)c;
  solidEnd();
  solidState = (SolidState*)ps_malloc(sizeof(SolidState));
  if (!solidState) return false;
  if (!solidBuild(solidIndex, solidState->mesh)) {
    solidEnd();
    return false;
  }
  solidState->color = SOLID_TABLE[solidIndex].color;
  return true;
}

static void solidFrame(M5Canvas& c, float dt) {
  (void)dt;
  if (!solidState) return;
  solidDrawFrame(c, *solidState, modelRotation, modelZoomStep, solidGerman, FontManager::getFont16pt(),
                 FontManager::getFont12pt(), WIDTH, HEIGHT - EDU_FOOTER_HEIGHT);
  eduDrawFooter(c, 0);
}

const EduFeature FEATURE_SOLID = {"solid",    "",        SOLID_PROMPT_RULE, true,    solidFetch, nullptr,
                                  solidBegin, solidFrame, solidEnd,         nullptr, nullptr,    nullptr};
