// Reads Wikipedia's coordinates reply for the globe. No Arduino includes (ArduinoJson works on its own),
// tested on the host with real replies: tests/globe/run.sh
#pragma once

#include <ArduinoJson.h>
#include <math.h>
#include <stddef.h>
#include <string.h>

static const size_t GLOBE_TITLE_BYTES = 100;  // A label fits one line; longer ones are cut

struct GlobePlace {
  char title[GLOBE_TITLE_BYTES + 1];    // English article title, after redirects
  char titleDe[GLOBE_TITLE_BYTES + 1];  // Title of the German article; empty if there is none
  float lat, lon;
};

// Copies a title, cut to fit but never inside a UTF-8 character. False if empty or with control characters.
inline bool globeCopyTitle(const char* in, char out[GLOBE_TITLE_BYTES + 1]) {
  out[0] = 0;
  if (!in || !in[0]) return false;
  size_t length = 0;
  for (const char* p = in; *p; p++, length++) {
    if ((unsigned char)*p < 0x20 || *p == 0x7F) return false;
  }
  if (length > GLOBE_TITLE_BYTES) {
    length = GLOBE_TITLE_BYTES;
    while (length > 0 && ((unsigned char)in[length] & 0xC0) == 0x80) length--;
  }
  memcpy(out, in, length);
  out[length] = 0;
  return true;
}

// True only for one real article whose coordinates are all on Earth and make sense. Anything else, a moon
// crater or Mars for example, is false: Zitro then shows the picture, never a globe under another name.
inline bool globeParseWikiReply(const char* body, size_t length, GlobePlace& out) {
  memset(&out, 0, sizeof(out));
  if (!body || length == 0) return false;
  JsonDocument doc;
  if (deserializeJson(doc, body, length)) return false;
  JsonArrayConst pages = doc["query"]["pages"].as<JsonArrayConst>();
  if (pages.size() != 1) return false;  // A "|" in the title asks for several articles
  JsonObjectConst page = pages[0].as<JsonObjectConst>();
  if (page.isNull() || page["missing"].as<bool>() || page["invalid"].as<bool>()) return false;
  if (!page["ns"].is<int>() || page["ns"].as<int>() != 0) return false;  // Articles only
  JsonArrayConst coordinates = page["coordinates"].as<JsonArrayConst>();
  if (coordinates.size() < 1) return false;
  for (JsonObjectConst coordinate : coordinates) {
    const char* globe = coordinate["globe"].as<const char*>();
    if (!globe || strcmp(globe, "earth") != 0) return false;
  }
  JsonObjectConst first = coordinates[0].as<JsonObjectConst>();
  if (!first["lat"].is<double>() || !first["lon"].is<double>()) return false;
  const double lat = first["lat"].as<double>(), lon = first["lon"].as<double>();
  if (!isfinite(lat) || !isfinite(lon) || fabs(lat) > 90 || fabs(lon) > 180) return false;
  if (!globeCopyTitle(page["title"].as<const char*>(), out.title)) return false;
  out.lat = (float)lat;
  out.lon = (float)lon;
  // The German name is a bonus: one that's broken just isn't used
  for (JsonObjectConst link : page["langlinks"].as<JsonArrayConst>()) {
    const char* lang = link["lang"].as<const char*>();
    if (lang && strcmp(lang, "de") == 0) {
      globeCopyTitle(link["title"].as<const char*>(), out.titleDe);
      break;
    }
  }
  return true;
}
