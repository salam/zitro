// How big the anatomy view draws, shared by the stick and the host preview.
#pragma once

static const int ANATOMY_VIEW_FOOTER = 18;  // Same as EDU_FOOTER_HEIGHT
static const float ANATOMY_VIEW_CAMERA = 4.0f;  // Distance from the middle, in model radii; weak perspective keeps shapes true
static const float ANATOMY_VIEW_ZOOMS[3] = {1.0f, 1.6f, 2.4f};

// Pixels per model radius. At zoom 0 the whole model fits whichever way it's turned.
inline float anatomyViewScale(int viewHeight, int zoomStep) {
  return viewHeight * 0.5f * ANATOMY_VIEW_ZOOMS[zoomStep < 0 || zoomStep > 2 ? 0 : zoomStep];
}

// With a legend the model moves to the left, a little smaller, and the words stand beside it. Zoomed in there's no room for
// words: the model takes the whole screen.
static const int ANATOMY_LEGEND_X = 112;  // The width the model has beside the legend
static const int ANATOMY_LEGEND_LINE = 14;
static const int ANATOMY_LEGEND_CHIP = 9;
static const int ANATOMY_LEGEND_GAP = 3;
static const int ANATOMY_TAG_HEIGHT = 13;  // The amber tag of a schematic, top left

inline bool anatomyViewShowsLegend(int legendCount, int zoomStep) {
  return legendCount > 0 && zoomStep == 0;
}

inline int anatomyViewCentreX(int width, int legendCount, int zoomStep) {
  return anatomyViewShowsLegend(legendCount, zoomStep) ? ANATOMY_LEGEND_X / 2 : width / 2;
}

inline float anatomyViewScaleFor(int viewHeight, int legendCount, int zoomStep) {
  return anatomyViewShowsLegend(legendCount, zoomStep) ? ANATOMY_LEGEND_X * 0.49f : anatomyViewScale(viewHeight, zoomStep);
}

// Top of the first legend line: in the middle, but clear of the amber tag
inline int anatomyLegendTop(int viewHeight, int legendCount, bool schematic) {
  const int top = (viewHeight - legendCount * ANATOMY_LEGEND_LINE) / 2;
  return schematic && top < ANATOMY_TAG_HEIGHT ? ANATOMY_TAG_HEIGHT : top;
}
