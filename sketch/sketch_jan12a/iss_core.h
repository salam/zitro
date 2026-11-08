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

struct IssTrack {
  int64_t t0;     // Unix seconds of the first point
  int32_t step;   // Seconds between points
  int count;
  int16_t lat100[ISS_MAX_POINTS], lon100[ISS_MAX_POINTS];  // Degrees x 100
  uint16_t alt10[ISS_MAX_POINTS];                          // km x 10
  int32_t speedKmh;
  float periodMin;
};

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

// The written answer, all computed from the track
inline void issAnswer(char* out, size_t size, bool german, float lat, float lon, float altKm, long speedKmh, float periodMin) {
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
}

inline void issNoAnswer(char* out, size_t size, bool german, bool clockKnown) {
  if (!clockKnown) snprintf(out, size, "%s", german ? "Ich kenne die Uhrzeit gerade nicht, darum finde ich die ISS nicht. Ich brauche dazu WLAN."
                                                  : "I don't know the time right now, so I can't find the ISS. I need WiFi for it.");
  else snprintf(out, size, "%s", german ? "Ich kann die ISS gerade nicht finden: Der Zitro-Server antwortet nicht."
                                       : "I can't find the ISS right now: the Zitro server doesn't answer.");
}
