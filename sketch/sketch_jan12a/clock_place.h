// The world clock's places that aren't built in: what the stick asks zitro.spot/api/place.php and how it
// reads the reply. Plain C++ with ArduinoJson, tested on the host: tests/clock/run.sh
#pragma once

#include <ArduinoJson.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "clock_core.h"

static const size_t CLOCK_PLACE_KEY_MAX = 48;
static const char CLOCK_PLACE_URL[] = "https://zitro.spot/api/place.php";

struct ClockPlace {
  char name[64];    // "Knoxville"
  char label[76];   // "Knoxville, TN"
  char region[8];   // "TN": the US state, else the country
  char zone[48];    // "America/New_York": names the country's helpline (care_core.h)
  char posix[64];   // "EST5EDT,M3.2.0,M11.1.0": the device's own time zone when this is home
  char until[48];   // Fixed offsets before the rule, for a few zones (clockParseUntil); usually ""
  float lat, lon;
  ClockRule rule;   // posix and until, read
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

// An IANA zone name: letters, digits and / _ + -, as in "America/Argentina/Buenos_Aires"
inline bool clockZoneNameOk(const char* s, size_t limit) {
  if (!clockPlaceTextOk(s, limit)) return false;
  for (const char* p = s; *p; p++) {
    const char c = *p;
    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '/' || c == '_' ||
          c == '+' || c == '-')) {
      return false;
    }
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
  const char* name = doc["name"], *region = doc["region"], *rule = doc["rule"], *zone = doc["zone"];
  const char* until = doc["until"] | "";  // Older servers don't send it
  if (!doc["lat"].is<float>() || !doc["lon"].is<float>()) return CLOCK_PLACE_BAD;
  const float lat = doc["lat"].as<float>(), lon = doc["lon"].as<float>();
  if (!clockPlaceTextOk(name, sizeof(out.name)) || !clockPlaceTextOk(region, sizeof(out.region)) ||
      !clockPlaceTextOk(rule, sizeof(out.posix)) || !clockZoneNameOk(zone, sizeof(out.zone))) {
    return CLOCK_PLACE_BAD;
  }
  if (!(fabsf(lat) <= 90.0f) || !(fabsf(lon) <= 180.0f)) return CLOCK_PLACE_BAD;
  if (strlen(until) >= sizeof(out.until)) return CLOCK_PLACE_BAD;
  if (!clockParseRule(rule, out.rule) || !clockParseUntil(until, out.rule)) return CLOCK_PLACE_BAD;
  snprintf(out.name, sizeof(out.name), "%s", name);
  snprintf(out.label, sizeof(out.label), "%s, %s", name, region);
  snprintf(out.region, sizeof(out.region), "%s", region);
  snprintf(out.zone, sizeof(out.zone), "%s", zone);
  snprintf(out.posix, sizeof(out.posix), "%s", rule);
  snprintf(out.until, sizeof(out.until), "%s", until);
  out.lat = lat;
  out.lon = lon;
  return CLOCK_PLACE_FOUND;
}

// Home anywhere (setting homeCity, requested 2026-11-15). A home that isn't built in is kept with all the
// stick needs, so the time zone is right at boot with no network. Stored as Preferences bytes: homePlace for
// a place the parent named, homeFound for the one zitro.spot/api/locate.php found.
static const uint32_t CLOCK_HOME_MAGIC = 0x5A484D32;  // "ZHM2": a record of this layout (ZHM1 had no until)

struct ClockHomeRecord {
  uint32_t magic;
  char key[64];     // The folded setting it belongs to ("paristexas"); empty for a found home
  char name[64];
  char region[8];
  char zone[48];
  char posix[64];
  char until[48];
  float lat, lon;
  int64_t at;       // A found home: when it was found, UTC seconds. 0 otherwise
};

inline void clockHomeFromPlace(const ClockPlace& p, const char* key, int64_t at, ClockHomeRecord& out) {
  memset(&out, 0, sizeof(out));
  out.magic = CLOCK_HOME_MAGIC;
  snprintf(out.key, sizeof(out.key), "%s", key ? key : "");
  snprintf(out.name, sizeof(out.name), "%s", p.name);
  snprintf(out.region, sizeof(out.region), "%s", p.region);
  snprintf(out.zone, sizeof(out.zone), "%s", p.zone);
  snprintf(out.posix, sizeof(out.posix), "%s", p.posix);
  snprintf(out.until, sizeof(out.until), "%s", p.until);
  out.lat = p.lat;
  out.lon = p.lon;
  out.at = at;
}

// A stored record back to a place. False for anything that isn't a whole, sensible record: flash that was
// never written, an older layout, or a rule that doesn't read.
inline bool clockHomeToPlace(const ClockHomeRecord& r, size_t size, ClockPlace& out) {
  if (size != sizeof(ClockHomeRecord) || r.magic != CLOCK_HOME_MAGIC) return false;
  if (!memchr(r.key, 0, sizeof(r.key)) || !memchr(r.name, 0, sizeof(r.name)) || !memchr(r.region, 0, sizeof(r.region)) ||
      !memchr(r.zone, 0, sizeof(r.zone)) || !memchr(r.posix, 0, sizeof(r.posix)) || !memchr(r.until, 0, sizeof(r.until))) {
    return false;
  }
  if (!clockPlaceTextOk(r.name, sizeof(r.name)) || !clockPlaceTextOk(r.region, sizeof(r.region)) ||
      !clockZoneNameOk(r.zone, sizeof(r.zone)) || !(fabsf(r.lat) <= 90.0f) || !(fabsf(r.lon) <= 180.0f)) {
    return false;
  }
  if (!clockParseRule(r.posix, out.rule) || !clockParseUntil(r.until, out.rule)) return false;
  snprintf(out.name, sizeof(out.name), "%s", r.name);
  snprintf(out.label, sizeof(out.label), "%s, %s", r.name, r.region);
  snprintf(out.region, sizeof(out.region), "%s", r.region);
  snprintf(out.zone, sizeof(out.zone), "%s", r.zone);
  snprintf(out.posix, sizeof(out.posix), "%s", r.posix);
  snprintf(out.until, sizeof(out.until), "%s", r.until);
  out.lat = r.lat;
  out.lon = r.lon;
  return true;
}

// The folded key a homeCity setting is stored under: "Paris, Texas" -> "paristexas". False if unusable.
inline bool clockHomeKey(const char* setting, char* key, size_t size) {
  char name[64], hint[64];
  if (!clockSplitPlaceQuery(setting, name, sizeof(name), hint, sizeof(hint))) return false;
  return (size_t)snprintf(key, size, "%s%s", name, hint) < size;
}
