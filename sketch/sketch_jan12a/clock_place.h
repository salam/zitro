// The world clock's places that aren't built in: what the stick asks zitro.spot/api/place.php and how it
// reads the reply. Plain C++ with ArduinoJson, tested on the host: tests/clock/run.sh
#pragma once

#include <ArduinoJson.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "clock_core.h"

static const size_t CLOCK_PLACE_KEY_MAX = 48;

struct ClockPlace {
  char name[64];    // "Knoxville"
  char label[76];   // "Knoxville, TN"
  float lat, lon;
  ClockRule rule;
};

enum ClockPlaceResult {
  CLOCK_PLACE_FOUND,
  CLOCK_PLACE_UNKNOWN,  // The server has no such place, or the name can't be asked for
  CLOCK_PLACE_BAD       // No answer, or one that doesn't make sense
};

// "Paris, Texas" to the folded name "paris" and hint "texas" the server takes. False if there's no usable name.
inline bool clockSplitPlaceQuery(const char* query, char* name, size_t nameSize, char* hint, size_t hintSize) {
  char part[64];
  const char* comma = strchr(query, ',');
  size_t length = comma ? (size_t)(comma - query) : strlen(query);
  if (length >= sizeof(part)) return false;
  memcpy(part, query, length);
  part[length] = 0;
  clockFold(part, name, nameSize);
  hint[0] = 0;
  if (comma) clockFold(comma + 1, hint, hintSize);
  const size_t n = strlen(name), h = strlen(hint);
  return n >= 2 && n <= CLOCK_PLACE_KEY_MAX && h <= CLOCK_PLACE_KEY_MAX && (h == 0 || h >= 2);
}

// Text from the server, for the screen: not empty, fits, no control characters
inline bool clockPlaceTextOk(const char* s, size_t limit) {
  if (!s || !s[0] || strlen(s) >= limit) return false;
  for (const unsigned char* p = (const unsigned char*)s; *p; p++) {
    if (*p < 0x20 || *p == 0x7F) return false;
  }
  return true;
}

// Reads and checks the reply. A place is only shown if every part of it makes sense: a wrong rule would be
// a wrong time shown as fact.
inline ClockPlaceResult clockParsePlace(const char* body, size_t length, ClockPlace& out) {
  JsonDocument doc;
  if (deserializeJson(doc, body, length)) return CLOCK_PLACE_BAD;
  if (!doc["found"].is<bool>()) return CLOCK_PLACE_BAD;
  if (!doc["found"].as<bool>()) return CLOCK_PLACE_UNKNOWN;
  const char* name = doc["name"], *region = doc["region"], *rule = doc["rule"];
  if (!doc["lat"].is<float>() || !doc["lon"].is<float>()) return CLOCK_PLACE_BAD;
  const float lat = doc["lat"].as<float>(), lon = doc["lon"].as<float>();
  if (!clockPlaceTextOk(name, sizeof(out.name)) || !clockPlaceTextOk(region, 8) || !clockPlaceTextOk(rule, 64)) return CLOCK_PLACE_BAD;
  if (!(fabsf(lat) <= 90.0f) || !(fabsf(lon) <= 180.0f)) return CLOCK_PLACE_BAD;
  if (!clockParseRule(rule, out.rule)) return CLOCK_PLACE_BAD;
  snprintf(out.name, sizeof(out.name), "%s", name);
  snprintf(out.label, sizeof(out.label), "%s, %s", name, region);
  out.lat = lat;
  out.lon = lon;
  return CLOCK_PLACE_FOUND;
}
