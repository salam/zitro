// What a shake can show besides a fact. Only results that are built in and need no network, so the
// surprise comes at once, also offline. Every entry is checked against its feature's closed list by
// tests/shake/run.sh, so a renamed scene can't leave a dead entry here.
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "clock_data.h"

static const char* const SHAKE_MEDIA[] = {
    // Physics and the sky, computed on the device
    "physics:falling", "physics:pendulum", "physics:buoyancy", "physics:lever", "physics:ramp", "physics:magnet",
    "physics:mirror", "physics:refraction", "physics:sound", "physics:matter", "physics:seasons", "physics:moon",
    "physics:atom-C", "physics:atom-O", "physics:atom-Fe",
    "physics:double-pendulum", "physics:double-slit", "physics:prism",
    // Astronomy, computed on the device. Doppler and half-life are left out: they beep, and a shake stays quiet
    "physics:planets", "physics:retrograde", "physics:sunpath", "physics:satellites", "physics:sizes",
    // Shapes to turn
    "solid:icosahedron", "solid:dodecahedron", "solid:torus", "solid:octahedron", "solid:hexagonal-prism",
    // Star maps
    "sky:Orion", "sky:Ursa Major", "sky:Cassiopeia", "sky:Crux", "sky:Leo", "sky:all",
    // The time somewhere: a random big city, see SHAKE_CLOCK_CITIES
    "clock:*",
};
static const int SHAKE_MEDIA_COUNT = sizeof(SHAKE_MEDIA) / sizeof(SHAKE_MEDIA[0]);
static const int SHAKE_CLOCK_CITIES = 80;  // CLOCK_CITIES is sorted biggest first: names kids may have heard

// Turns two random numbers into a result reference. False if it doesn't fit.
inline bool shakePickMedia(uint32_t pick, uint32_t cityPick, char* out, size_t size) {
  const char* ref = SHAKE_MEDIA[pick % SHAKE_MEDIA_COUNT];
  int written;
  if (strcmp(ref, "clock:*") == 0) {
    const int count = SHAKE_CLOCK_CITIES < CLOCK_CITY_COUNT ? SHAKE_CLOCK_CITIES : CLOCK_CITY_COUNT;
    written = snprintf(out, size, "clock:%s", CLOCK_CITIES[cityPick % count].en);
  } else {
    written = snprintf(out, size, "%s", ref);
  }
  return written > 0 && (size_t)written < size;
}
