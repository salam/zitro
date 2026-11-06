// How big the anatomy view draws, shared by the stick and the host preview.
#pragma once

static const int ANATOMY_VIEW_FOOTER = 18;  // Same as EDU_FOOTER_HEIGHT
static const float ANATOMY_VIEW_CAMERA = 4.0f;  // Distance from the middle, in model radii; weak perspective keeps shapes true
static const float ANATOMY_VIEW_ZOOMS[3] = {1.0f, 1.6f, 2.4f};

// Pixels per model radius. At zoom 0 the whole model fits whichever way it's turned.
inline float anatomyViewScale(int viewHeight, int zoomStep) {
  return viewHeight * 0.44f * ANATOMY_VIEW_ZOOMS[zoomStep < 0 || zoomStep > 2 ? 0 : zoomStep];
}
