// Every feature, once. To add one: a kind in MODEL_KINDS, the extern in edu_feature.h, a line here.
const EduFeature* const EDU_FEATURES[] = {&FEATURE_MOLECULE, &FEATURE_MUSIC,   &FEATURE_GRAPH,   &FEATURE_RECIPE,
                                          &FEATURE_GLOBE,    &FEATURE_PROTEIN, &FEATURE_CRYSTAL, &FEATURE_SKY,
                                          &FEATURE_PHYSICS,  &FEATURE_SOLID,   &FEATURE_TIMELINE, &FEATURE_ANATOMY,
                                          &FEATURE_WOLFRAM,  &FEATURE_CLOCK};
const int EDU_FEATURE_COUNT = sizeof(EDU_FEATURES) / sizeof(EDU_FEATURES[0]);
