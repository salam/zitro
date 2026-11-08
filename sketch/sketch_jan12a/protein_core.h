// Pure logic of the protein view: index lookup, the compact structure file, colours and depth order.
// Plain C++ with no Arduino includes, tested on the host: tests/protein/run.sh
// Files come from scripts/make_protein_data.py. Anything unexpected in them means no model.
#pragma once

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "model_core.h"
#include "name_match.h"

#define PROTEIN_MAX_POINTS 1500
#define PROTEIN_MAX_CHAINS 64
#define PROTEIN_LABEL_SIZE 24
#define PROTEIN_NORMALISED_SIZE 128
#define PROTEIN_INDEX_LIMIT (16 * 1024)
#define PROTEIN_FILE_LIMIT (32 * 1024)
#define PROTEIN_DEPTH_BUCKETS 48
#define PROTEIN_COORD_LIMIT 32000L

struct ProteinPoint {
  float x, y, z;
  uint8_t t;          // Place along its chain: 0 start, 255 end
  uint8_t connected;  // 1: a line runs here from the point before
};

struct ProteinEntry {
  char id[5];
  char de[PROTEIN_LABEL_SIZE];
  char en[PROTEIN_LABEL_SIZE];
};

// Lower case, umlauts spelled out, hyphens and repeated spaces as one space, no space at the ends.
// False for empty text, for any other non-ASCII character, and for the index's own separators.
inline bool proteinNormalise(const char* in, char* out, size_t capacity) {
  return eduNormaliseName(in, out, capacity, false);
}

inline bool proteinValidId(const char* s, size_t len) {
  if (len != 4 || s[0] < '1' || s[0] > '9') return false;
  for (size_t i = 1; i < 4; i++) {
    const char c = s[i];
    if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z'))) return false;
  }
  return true;
}

// Whole number from text without a terminator. False unless all of it is one number in range.
inline bool proteinParseLong(const char* s, size_t len, long limit, long& value) {
  if (len == 0 || len > 7) return false;
  size_t i = 0;
  bool negative = false;
  if (s[0] == '-') {
    if (len == 1) return false;
    negative = true;
    i = 1;
  }
  long v = 0;
  for (; i < len; i++) {
    if (s[i] < '0' || s[i] > '9') return false;
    v = v * 10 + (s[i] - '0');
  }
  if (v > limit) return false;
  value = negative ? -v : v;
  return true;
}

// Splits at single spaces. The number of fields, or -1 for more than maxFields or an empty one.
inline int proteinSplit(const char* line, int len, const char** fields, int* lengths, int maxFields) {
  int count = 0, start = 0;
  for (int i = 0; i <= len; i++) {
    if (i == len || line[i] == ' ') {
      if (i == start || count == maxFields) return -1;
      fields[count] = line + start;
      lengths[count++] = i - start;
      start = i + 1;
    }
  }
  return count;
}

inline bool proteinCopyLabel(const char* s, int len, char* out) {
  if (len < 1 || len >= PROTEIN_LABEL_SIZE) return false;
  for (int i = 0; i < len; i++) {
    if ((unsigned char)s[i] < 0x20 || (unsigned char)s[i] >= 0x7F) return false;
  }
  memcpy(out, s, len);
  out[len] = 0;
  return true;
}

inline bool proteinSameText(const char* a, size_t aLen, const char* text) {
  return strlen(text) == aLen && memcmp(a, text, aLen) == 0;
}

// Finds a name in the index: an exact match only, never a close one. The whole index must be
// sound, because a cut-off or damaged one can't be trusted for what it does contain.
// Only names count: a PDB ID as the query finds nothing, so the AI can't pick a record by its ID.
inline bool proteinResolve(const char* index, size_t indexLen, const char* query, ProteinEntry& out) {
  memset(&out, 0, sizeof(out));
  char wanted[PROTEIN_NORMALISED_SIZE];
  if (!index || !proteinNormalise(query, wanted, sizeof(wanted))) return false;
  const char* p = index;
  const char* end = index + indexLen;
  const char* line = nullptr;
  int len = sdfNextLine(p, end, line);
  const char* head[3];
  int headLen[3];
  long expected = 0;
  if (len < 0 || proteinSplit(line, len, head, headLen, 3) != 3 ||
      !proteinSameText(head[0], headLen[0], "ZITRO-PROTEIN-INDEX") || !proteinSameText(head[1], headLen[1], "1") ||
      !proteinParseLong(head[2], headLen[2], 1000, expected) || expected < 1) {
    return false;
  }
  long seen = 0;
  bool found = false, sawEnd = false;
  ProteinEntry current, match;
  while ((len = sdfNextLine(p, end, line)) >= 0) {
    if (sawEnd) {
      if (len != 0) return false;  // Nothing may follow the end marker
      continue;
    }
    if (len == 3 && memcmp(line, "END", 3) == 0) {
      sawEnd = true;
      continue;
    }
    const char* bar1 = (const char*)memchr(line, '|', len);
    if (!bar1) return false;
    const char* lineEnd = line + len;
    const char* bar2 = (const char*)memchr(bar1 + 1, '|', lineEnd - bar1 - 1);
    if (!bar2) return false;
    const char* bar3 = (const char*)memchr(bar2 + 1, '|', lineEnd - bar2 - 1);
    if (!bar3 || memchr(bar3 + 1, '|', lineEnd - bar3 - 1)) return false;
    if (!proteinValidId(line, bar1 - line)) return false;
    memcpy(current.id, line, 4);
    current.id[4] = 0;
    if (!proteinCopyLabel(bar1 + 1, (int)(bar2 - bar1 - 1), current.de) ||
        !proteinCopyLabel(bar2 + 1, (int)(bar3 - bar2 - 1), current.en)) {
      return false;
    }
    const char* name = bar3 + 1;
    if (name >= lineEnd) return false;
    bool isMatch = false;
    while (name <= lineEnd) {
      const char* semi = (const char*)memchr(name, ';', lineEnd - name);
      const char* nameEnd = semi ? semi : lineEnd;
      if (nameEnd == name) return false;
      if (proteinSameText(name, nameEnd - name, wanted)) isMatch = true;
      if (!semi) break;
      name = semi + 1;
    }
    if (isMatch) {
      if (found && strcmp(match.id, current.id) != 0) return false;  // One name, two records: don't guess
      match = current;
      found = true;
    }
    seen++;
  }
  if (!sawEnd || seen != expected || !found) return false;
  out = match;
  return true;
}

// The structure file. out has room for capacity points; on success it is centred with the farthest
// point at radius 1. Must be complete and be the record it was asked for.
inline bool proteinParseStructure(const char* text, size_t textLen, const char* wantedId, ProteinPoint* out,
                                  uint16_t capacity, uint16_t& pointCount, uint16_t& chainCount) {
  pointCount = chainCount = 0;
  if (!text || !wantedId || !out || textLen == 0) return false;
  const char* p = text;
  const char* end = text + textLen;
  const char* line = nullptr;
  int len = sdfNextLine(p, end, line);
  const char* f[5];
  int fl[5];
  long total = 0, chains = 0;
  if (len < 0 || proteinSplit(line, len, f, fl, 5) != 5 || !proteinSameText(f[0], fl[0], "ZITRO-PROTEIN") ||
      !proteinSameText(f[1], fl[1], "1") || !proteinValidId(f[2], fl[2]) || !proteinSameText(f[2], fl[2], wantedId) ||
      !proteinParseLong(f[3], fl[3], PROTEIN_MAX_POINTS, total) || !proteinParseLong(f[4], fl[4], PROTEIN_MAX_CHAINS, chains) ||
      total < 2 || chains < 1 || total > capacity) {
    return false;
  }
  long n = 0, chainsSeen = 0, chainStart = 0, chainSize = 0;
  bool gapNext = false, sawEnd = false;
  double sx = 0, sy = 0, sz = 0;
  while ((len = sdfNextLine(p, end, line)) >= 0) {
    if (sawEnd) {
      if (len != 0) return false;
      continue;
    }
    if (len == 3 && memcmp(line, "END", 3) == 0) {
      sawEnd = true;
    } else if (len == 1 && line[0] == 'G') {
      // A gap in the chain: the stretch between isn't in the record, so no line may cross it
      if (gapNext || chainsSeen == 0 || n == chainStart || n - chainStart >= chainSize) return false;
      gapNext = true;
    } else if (len >= 3 && line[0] == 'C' && line[1] == ' ') {
      long size = 0;
      if (!proteinParseLong(line + 2, len - 2, PROTEIN_MAX_POINTS, size) || size < 1) return false;
      if (gapNext || n - chainStart != chainSize || chainsSeen >= chains || n + size > total) return false;
      chainStart = n;
      chainSize = size;
      chainsSeen++;
    } else {
      long v[3];
      if (proteinSplit(line, len, f, fl, 3) != 3 || chainsSeen == 0 || n - chainStart >= chainSize) return false;
      for (int i = 0; i < 3; i++) {
        if (!proteinParseLong(f[i], fl[i], PROTEIN_COORD_LIMIT, v[i])) return false;
      }
      ProteinPoint& point = out[n];
      point.x = (float)v[0];
      point.y = (float)v[1];
      point.z = (float)v[2];
      const long place = n - chainStart;
      point.t = chainSize > 1 ? (uint8_t)(place * 255 / (chainSize - 1)) : 0;
      point.connected = (place > 0 && !gapNext) ? 1 : 0;
      gapNext = false;
      sx += v[0];
      sy += v[1];
      sz += v[2];
      n++;
    }
  }
  if (!sawEnd || n != total || chainsSeen != chains || n - chainStart != chainSize || gapNext) return false;
  sx /= n;
  sy /= n;
  sz /= n;
  float maxR2 = 0;
  for (long i = 0; i < n; i++) {
    out[i].x -= (float)sx;
    out[i].y -= (float)sy;
    out[i].z -= (float)sz;
    const float r2 = out[i].x * out[i].x + out[i].y * out[i].y + out[i].z * out[i].z;
    if (r2 > maxR2) maxR2 = r2;
  }
  if (!(maxR2 > 0) || !isfinite(maxR2)) return false;  // All in one spot isn't a structure
  const float scale = 1.0f / sqrtf(maxR2);
  for (long i = 0; i < n; i++) {
    out[i].x *= scale;
    out[i].y *= scale;
    out[i].z *= scale;
  }
  pointCount = (uint16_t)n;
  chainCount = (uint16_t)chains;
  return true;
}

inline uint16_t proteinRgb565(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

// Blue at the start of a chain, through cyan, green and yellow, to red at its end
inline uint16_t proteinRampColor(uint8_t t) {
  static const uint8_t stops[5][3] = {{40, 90, 255}, {0, 210, 230}, {40, 220, 60}, {250, 220, 30}, {250, 40, 40}};
  const int scaled = t * 4;
  const int stop = scaled / 255 >= 4 ? 3 : scaled / 255;
  const int rest = scaled - stop * 255;  // 0 to 255 between this stop and the next
  uint8_t c[3];
  for (int i = 0; i < 3; i++) c[i] = (uint8_t)((stops[stop][i] * (255 - rest) + stops[stop + 1][i] * rest) / 255);
  return proteinRgb565(c[0], c[1], c[2]);
}

// Far away is darker, which is what makes a tangle of lines read as depth. Level 3 is full colour.
inline uint16_t proteinShade565(uint16_t color, uint8_t level) {
  const uint16_t r = (color >> 11) & 0x1F, g = (color >> 5) & 0x3F, b = color & 0x1F;
  const uint16_t factor = (uint16_t)(level + 2);  // 2/5 to 5/5
  return (uint16_t)(((r * factor / 5) << 11) | ((g * factor / 5) << 5) | (b * factor / 5));
}

// Indices far first (lowest z), by bucket. Time grows only with n, whatever the order was before.
inline void proteinDepthOrder(const float* z, uint16_t n, uint16_t* order) {
  uint16_t starts[PROTEIN_DEPTH_BUCKETS + 1];
  memset(starts, 0, sizeof(starts));
  const auto bucket = [](float v) {
    if (!(v > -1.0f)) return 0;  // Also catches a NaN, which no input should ever make
    if (v >= 1.0f) return PROTEIN_DEPTH_BUCKETS - 1;
    return (int)((v + 1.0f) * 0.5f * PROTEIN_DEPTH_BUCKETS);
  };
  for (uint16_t i = 0; i < n; i++) starts[bucket(z[i]) + 1]++;
  for (int b = 0; b < PROTEIN_DEPTH_BUCKETS; b++) starts[b + 1] += starts[b];
  for (uint16_t i = 0; i < n; i++) order[starts[bucket(z[i])]++] = i;
}

// Line width in pixels: thick for small models, thinner when many lines would fill the screen
inline uint8_t proteinLineWidth(uint16_t points, uint8_t zoomStep) {
  uint8_t width = points <= 150 ? 5 : (points <= 600 ? 3 : 2);
  if (zoomStep > 0) width++;
  return width;
}
