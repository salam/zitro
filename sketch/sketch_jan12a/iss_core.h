// Pure logic of the live ISS view: reading the track zitro.spot/api/iss.php serves (computed there from
// NASA's ephemeris), where the ISS is at a given moment, and the written answer. Plain C++ with ArduinoJson,
// tested on the host: tests/iss/run.sh
#pragma once

#include <ArduinoJson.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const int ISS_MAX_POINTS = 400;
static const int ISS_MIN_POINTS = 10;
// Place names under the track (iss.php?places=1). The server sends none if there are more; measured at most
// 106 names and 138 changes in a reply (tests/iss/test_iss.php).
static const int ISS_MAX_PLACES = 160;
static const int ISS_MAX_PLACE_CHANGES = 300;
static const int ISS_PLACE_BYTES = 48;  // With the terminating zero; the data script refuses longer names

struct IssTrack {
  int64_t t0;     // Unix seconds of the first point
  int32_t step;   // Seconds between points
  int count;
  int16_t lat100[ISS_MAX_POINTS], lon100[ISS_MAX_POINTS];  // Degrees x 100
  uint16_t alt10[ISS_MAX_POINTS];                          // km x 10
  int32_t speedKmh;
  float periodMin;
  // From placeT[i] seconds after t0 the ISS is over placeI[i] (-1: no name) until the next change
  int placeCount, placeChanges;
  char placeDe[ISS_MAX_PLACES][ISS_PLACE_BYTES], placeEn[ISS_MAX_PLACES][ISS_PLACE_BYTES];
  int32_t placeT[ISS_MAX_PLACE_CHANGES];
  int16_t placeI[ISS_MAX_PLACE_CHANGES];
};

// A name the stick can show: 1 to 47 bytes of well-formed UTF-8, nothing below U+0020 and nothing past the
// font's last glyph (U+05FF)
inline bool issPlaceNameOk(const char* s) {
  const size_t n = s ? strlen(s) : 0;
  if (n == 0 || n >= (size_t)ISS_PLACE_BYTES) return false;
  for (size_t i = 0; i < n;) {
    const uint8_t c = (uint8_t)s[i];
    uint32_t cp;
    int more;
    if (c < 0x80) { cp = c; more = 0; }
    else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; more = 1; }
    else return false;  // Three bytes and more are past U+05FF anyway
    if (i + more >= n) return false;  // Cut off in the middle of a character
    for (int k = 1; k <= more; k++) {
      if (((uint8_t)s[i + k] & 0xC0) != 0x80) return false;
      cp = (cp << 6) | ((uint8_t)s[i + k] & 0x3F);
    }
    if (cp < 0x20 || cp > 0x5FF || (more == 1 && cp < 0x80)) return false;
    i += 1 + more;
  }
  return true;
}

// The place names, if the reply has them and they all make sense. Anything wrong drops the places only:
// the track itself is still right, and no name is better than a wrong one.
inline void issParsePlaces(const JsonDocument& doc, IssTrack& out) {
  out.placeCount = out.placeChanges = 0;
  JsonArrayConst names = doc["places"], times = doc["place_t"], indices = doc["place_i"];
  if (names.isNull() || times.isNull() || indices.isNull()) return;
  const int n = (int)names.size(), changes = (int)times.size();
  if (n > ISS_MAX_PLACES || changes < 1 || changes > ISS_MAX_PLACE_CHANGES || (int)indices.size() != changes) return;
  for (int i = 0; i < n; i++) {
    JsonArrayConst pair = names[i];
    if (pair.isNull() || pair.size() != 2 || !pair[0].is<const char*>() || !pair[1].is<const char*>()) return;
    const char* de = pair[0].as<const char*>();
    const char* en = pair[1].as<const char*>();
    if (!issPlaceNameOk(de) || !issPlaceNameOk(en)) return;
    strcpy(out.placeDe[i], de);
    strcpy(out.placeEn[i], en);
  }
  const int64_t end = (int64_t)(out.count - 1) * out.step;
  for (int i = 0; i < changes; i++) {
    if (!times[i].is<int64_t>() || !indices[i].is<int>()) return;
    const int64_t t = times[i].as<int64_t>();
    const int k = indices[i].as<int>();
    if (t < 0 || t > end || (i > 0 && t <= out.placeT[i - 1]) || k < -1 || k >= n) return;
    out.placeT[i] = (int32_t)t;
    out.placeI[i] = (int16_t)k;
  }
  out.placeCount = n;
  out.placeChanges = changes;
}

// Reads and checks the server's reply. False unless every part is there and makes sense: a wrong track
// must never be drawn as the real ISS.
inline bool issParseTrack(const char* body, size_t length, IssTrack& out) {
  JsonDocument doc;
  if (deserializeJson(doc, body, length)) return false;
  JsonArrayConst lat = doc["lat"], lon = doc["lon"], alt = doc["alt"];
  if (!doc["t0"].is<int64_t>() || !doc["step"].is<int>() || lat.isNull() || lon.isNull() || alt.isNull()) return false;
  const int n = (int)lat.size();
  if (n < ISS_MIN_POINTS || n > ISS_MAX_POINTS || (int)lon.size() != n || (int)alt.size() != n) return false;
  out.t0 = doc["t0"].as<int64_t>();
  out.step = doc["step"].as<int>();
  if (out.step < 10 || out.step > 600 || out.t0 < 1704067200LL) return false;
  for (int i = 0; i < n; i++) {
    if (!lat[i].is<float>() || !lon[i].is<float>() || !alt[i].is<float>()) return false;
    const float a = lat[i].as<float>(), o = lon[i].as<float>(), h = alt[i].as<float>();
    if (!(fabsf(a) <= 90.0f) || !(fabsf(o) <= 180.0f) || !(h > 100.0f && h < 2000.0f)) return false;
    out.lat100[i] = (int16_t)lroundf(a * 100);
    out.lon100[i] = (int16_t)lroundf(o * 100);
    out.alt10[i] = (uint16_t)lroundf(h * 10);
  }
  out.count = n;
  out.speedKmh = doc["speed_kmh"] | 0;
  out.periodMin = doc["period_min"] | 0.0f;
  if (out.speedKmh < 20000 || out.speedKmh > 35000 || !(out.periodMin > 80 && out.periodMin < 120)) return false;
  issParsePlaces(doc, out);
  return true;
}

// Where the ISS is at time t (seconds, with fractions): false outside the track
inline bool issPositionAt(const IssTrack& tr, double t, float& lat, float& lon, float& altKm) {
  const double i = (t - (double)tr.t0) / tr.step;
  if (!(i >= 0) || i > tr.count - 1) return false;
  int k = (int)i;
  if (k >= tr.count - 1) k = tr.count - 2;
  const float f = (float)(i - k);
  lat = (tr.lat100[k] + (tr.lat100[k + 1] - tr.lat100[k]) * f) / 100.0f;
  float d = (tr.lon100[k + 1] - tr.lon100[k]) / 100.0f;
  if (d > 180) d -= 360;  // Across the date line
  if (d < -180) d += 360;
  lon = tr.lon100[k] / 100.0f + d * f;
  if (lon > 180) lon -= 360;
  if (lon < -180) lon += 360;
  altKm = (tr.alt10[k] + (tr.alt10[k + 1] - tr.alt10[k]) * f) / 10.0f;
  return true;
}

// The name of the place under the ISS at time t, or nullptr: no places, no name there, or outside the track
inline const char* issPlaceAt(const IssTrack& tr, double t, bool german) {
  if (tr.placeChanges == 0) return nullptr;
  const double since = t - (double)tr.t0;
  if (!(since >= 0) || since > (double)(tr.count - 1) * tr.step) return nullptr;
  int k = -1;
  for (int i = 0; i < tr.placeChanges && tr.placeT[i] <= since; i++) k = i;
  if (k < 0 || tr.placeI[k] < 0) return nullptr;
  return german ? tr.placeDe[tr.placeI[k]] : tr.placeEn[tr.placeI[k]];
}

// "27 544" with a space between thousands, as both languages accept
inline void issFormatThousands(char* out, size_t size, long v) {
  if (v >= 1000) snprintf(out, size, "%ld %03ld", v / 1000, v % 1000);
  else snprintf(out, size, "%ld", v);
}

// "25,3° S" / "25.3° S"; east is O in German
inline void issFormatCoordinate(char* out, size_t size, float deg, bool latitude, bool german) {
  const char* side = latitude ? (deg >= 0 ? "N" : "S") : (deg >= 0 ? (german ? "O" : "E") : (german ? "W" : "W"));
  char number[16];
  snprintf(number, sizeof(number), "%.1f", fabsf(deg));
  if (german) {
    for (char* c = number; *c; c++) if (*c == '.') *c = ',';
  }
  snprintf(out, size, "%s\xC2\xB0 %s", number, side);
}

// The written answer, all computed from the track. place: the name under the ISS (issPlaceAt), or nullptr.
// The name comes as the source spells it, after a colon, so German needs no article or case ending.
inline void issAnswer(char* out, size_t size, bool german, float lat, float lon, float altKm, long speedKmh, float periodMin,
                      const char* place = nullptr) {
  char la[24], lo[24], speed[16];
  issFormatCoordinate(la, sizeof(la), lat, true, german);
  issFormatCoordinate(lo, sizeof(lo), lon, false, german);
  issFormatThousands(speed, sizeof(speed), speedKmh);
  if (german) {
    snprintf(out, size, "Die ISS fliegt gerade %d km hoch bei %s, %s, mit %s km/h. Eine Runde um die Erde dauert %d Minuten.",
             (int)lroundf(altKm), la, lo, speed, (int)lroundf(periodMin));
  } else {
    snprintf(out, size, "The ISS is %d km up right now, at %s, %s, flying %s km/h. One trip around the Earth takes %d minutes.",
             (int)lroundf(altKm), la, lo, speed, (int)lroundf(periodMin));
  }
  if (place && *place) {
    const size_t used = strlen(out);
    snprintf(out + used, size - used, german ? " Unter ihr liegt gerade: %s." : " Right below it: %s.", place);
  }
}

inline void issNoAnswer(char* out, size_t size, bool german, bool clockKnown) {
  if (!clockKnown) snprintf(out, size, "%s", german ? "Ich kenne die Uhrzeit gerade nicht, darum finde ich die ISS nicht. Ich brauche dazu WLAN."
                                                  : "I don't know the time right now, so I can't find the ISS. I need WiFi for it.");
  else snprintf(out, size, "%s", german ? "Ich kann die ISS gerade nicht finden: Der Zitro-Server antwortet nicht."
                                       : "I can't find the ISS right now: the Zitro server doesn't answer.");
}
