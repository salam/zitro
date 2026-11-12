// Pure logic of the dinosaur fact sheet: name lookup, sheet parsing, rows, pages and the size comparison.
// Plain C++ with no Arduino includes, tested on the host: tests/dino/run.sh
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "name_match.h"

#define DINO_INDEX_LIMIT 16384  // Bytes of one index-<pp>.txt
#define DINO_TEXT_LIMIT 2048    // Bytes of one <slug>.txt
#define DINO_IMAGE_LIMIT 32768  // Bytes of one picture
#define DINO_IMAGE_W 240
#define DINO_IMAGE_H 117  // The footer takes the bottom 18 px (EDU_FOOTER_HEIGHT)
#define DINO_FIELD_LIMIT 160    // Bytes of one text field
#define DINO_CREDIT_LIMIT 80
#define DINO_PERSON_CM 170
#define DINO_MAX_ROWS 12
#define DINO_LINES_PER_PAGE 5  // Below the name line, on one facts page
#define DINO_MAX_LINES 24      // Twelve rows of two lines at most
#define DINO_MAX_PAGES 7
#define DINO_SIZE_AREA_W 224  // The area of the size page; the view draws into the same one
#define DINO_SIZE_AREA_H 79
#define DINO_FUN_LINES 5  // Of the fun fact, below its heading

enum DinoGroup : uint8_t { DINO_DINOSAUR, DINO_EARLY_BIRD, DINO_PTEROSAUR, DINO_MARINE };

struct DinoRange {
  int64_t lo, hi;
  bool present;
};

struct DinoText {
  const char* de;  // nullptr: absent
  const char* en;
};

// Every string points into the text that was parsed, which must stay alive as long as the sheet.
struct DinoSheet {
  const char* slug;
  const char* qid;
  const char* name;
  DinoGroup group;
  bool hasImage;
  uint32_t imageLength;
  uint16_t imageW, imageH;
  uint32_t imageHash;
  const char* credit;  // "" for none
  DinoRange length, height, wingspan;  // cm
  DinoRange mass;                      // g
  DinoRange speed;                     // 0.1 km/h
  DinoRange egg;                       // mm
  DinoRange lived;                     // years ago: lo is the older, hi the younger
  int32_t named;                       // 0 for none
  DinoText era, meaning, diet, found, fun;
  const char* funSource;
  const char* source;  // Footer text of the facts pages
};

struct DinoRow {
  char label[20];
  char value[DINO_FIELD_LIMIT + 40];
};

enum DinoPage { DINO_PAGE_PICTURE, DINO_PAGE_FACTS, DINO_PAGE_SIZE, DINO_PAGE_FUN };

// One page of the answer. For DINO_PAGE_FACTS, firstLine and lineCount say which of the lines from
// dinoFactLines it shows (lineCount is 0 for the other kinds).
struct DinoPageRef {
  DinoPage kind;
  uint8_t firstLine;
  uint8_t lineCount;
};

// A line of the facts: the label only on the first line of a row, empty on its continuation.
struct DinoLine {
  char label[20];
  char value[DINO_FIELD_LIMIT + 40];
};

// Width of a text in pixels, as the display measures it
typedef int (*DinoMeasure)(const char* text, void* context);

struct DinoSizeLayout {
  int personX, personY, personW, personH;
  int bodyX, bodyY, bodyW, bodyH;
  int rangeW;  // The uncertain end of the bar: its width from lo to hi. 0 if they are the same
  bool lineOnly;
  bool wingspan;
};

// ---- Names ----

inline bool dinoPrefixChar(char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'; }

// The first two characters of a normalised name for the index file name; anything else becomes "_".
inline void dinoPrefixOf(const char* normalised, char* pp) {
  for (int i = 0; i < 2; i++) {
    const char c = (normalised[0] && (i == 0 || normalised[1])) ? normalised[i] : '_';
    pp[i] = dinoPrefixChar(c) ? c : '_';
  }
  pp[2] = 0;
}

inline bool dinoIndexName(const char* query, char* normalised, size_t cap, char* indexFile, size_t fileCap) {
  if (!normalised || !indexFile || fileCap < 14) return false;  // "index-xx.txt" and a NUL
  if (!eduNormaliseName(query, normalised, cap, false)) return false;
  char pp[3];
  dinoPrefixOf(normalised, pp);
  snprintf(indexFile, fileCap, "index-%s.txt", pp);
  return true;
}

inline bool dinoValidSlug(const char* s, size_t len) {
  if (len < 1 || len > 40) return false;
  for (size_t i = 0; i < len; i++) {
    const char c = s[i];
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
  }
  return true;
}

// Strict decimal: only digits with an optional minus, nothing around them, and it has to fit.
inline bool dinoParseInt(const char* s, int64_t minimum, int64_t maximum, int64_t& out) {
  const char* p = s;
  if (*p == '-') p++;
  if (!*p || strlen(p) > 12) return false;
  for (const char* d = p; *d; d++) {
    if (*d < '0' || *d > '9') return false;
  }
  int64_t v = 0;
  for (; *p; p++) v = v * 10 + (*p - '0');
  if (*s == '-') v = -v;
  if (v < minimum || v > maximum) return false;
  out = v;
  return true;
}

// Looks a normalised name up in an index (ZDX1). The whole index is checked, so a cut or damaged one
// never answers. The name may be a full name ("tyrannosaurus"), a form with a dot ("t. rex"), or a
// species whose genus is in the index ("velociraptor mongoliensis"); an exact entry wins over the genus.
// Reads only: the index is not changed.
inline bool dinoResolve(char* index, size_t len, const char* prefix, const char* normalised, char* slug,
                        size_t slugCap) {
  if (!index || !prefix || !normalised || !slug || slugCap == 0 || len == 0 || len > DINO_INDEX_LIMIT) return false;
  if (!dinoPrefixChar(prefix[0]) || !dinoPrefixChar(prefix[1]) || prefix[2]) return false;

  // The forms to look for, best first: as written; dots as spaces; the first word of that
  char folded[80];
  const size_t qlen = strlen(normalised);
  if (qlen == 0 || qlen >= sizeof(folded)) return false;
  size_t n = 0;
  for (size_t i = 0; i < qlen; i++) {
    const char c = normalised[i] == '.' ? ' ' : normalised[i];
    if (c == ' ' && (n == 0 || folded[n - 1] == ' ')) continue;
    folded[n++] = c;
  }
  while (n > 0 && folded[n - 1] == ' ') n--;
  folded[n] = 0;
  const char* forms[3] = {normalised, folded, folded};
  size_t formLen[3] = {qlen, n, n};
  for (size_t i = 0; i < n; i++) {
    if (folded[i] == ' ') {
      formLen[2] = i;
      break;
    }
  }
  bool usable[3];
  for (int f = 0; f < 3; f++) {
    char pp[3], wanted[3];
    char head[80];
    memcpy(head, forms[f], formLen[f]);
    head[formLen[f]] = 0;
    dinoPrefixOf(head, pp);
    wanted[0] = prefix[0];
    wanted[1] = prefix[1];
    wanted[2] = 0;
    usable[f] = formLen[f] > 0 && strcmp(pp, wanted) == 0;
  }
  if (formLen[2] == n) usable[2] = false;  // One word: the same as the second form

  const char* end = index + len;
  const char* nl = (const char*)memchr(index, '\n', len);
  if (!nl) return false;
  // Header: ZDX1 <tab> pp <tab> count
  char header[40] = {0};
  const size_t headerLen = (size_t)(nl - index);
  if (headerLen >= sizeof(header)) return false;
  memcpy(header, index, headerLen);
  header[headerLen] = 0;
  if (strncmp(header, "ZDX1\t", 5) != 0 || header[7] != '\t') return false;
  if (header[5] != prefix[0] || header[6] != prefix[1]) return false;
  int64_t count;
  if (!dinoParseInt(header + 8, 0, 100000, count)) return false;

  int bestForm = 3;
  const char* bestSlug = nullptr;
  size_t bestLen = 0;
  int64_t lines = 0;
  const char* p = nl + 1;
  while (p < end) {
    const char* lineEnd = (const char*)memchr(p, '\n', (size_t)(end - p));
    if (!lineEnd) return false;  // Cut off in the middle of a line
    const char* tab = (const char*)memchr(p, '\t', (size_t)(lineEnd - p));
    if (!tab || tab == p) return false;
    const char* slugStart = tab + 1;
    const size_t slugLen = (size_t)(lineEnd - slugStart);
    if (!dinoValidSlug(slugStart, slugLen)) return false;
    const size_t nameLen = (size_t)(tab - p);
    for (size_t i = 0; i < nameLen; i++) {
      if ((unsigned char)p[i] < 0x20 || (unsigned char)p[i] > 0x7E) return false;
    }
    for (int f = 0; f < 3; f++) {
      if (f < bestForm && usable[f] && nameLen == formLen[f] && memcmp(p, forms[f], nameLen) == 0) {
        bestForm = f;
        bestSlug = slugStart;
        bestLen = slugLen;
      }
    }
    lines++;
    p = lineEnd + 1;
  }
  if (lines != count || !bestSlug || bestLen >= slugCap) return false;
  memcpy(slug, bestSlug, bestLen);
  slug[bestLen] = 0;
  return true;
}

// ---- Sheet ----

// Device text: the font ends at U+05FF, so UTF-8 of two bytes at most. The en dash is refused.
inline bool dinoValidText(const char* s, size_t limit, bool mayBeEmpty) {
  const size_t len = strlen(s);
  if (len > limit || (!mayBeEmpty && len == 0)) return false;
  for (size_t i = 0; i < len;) {
    const unsigned char c = (unsigned char)s[i];
    int extra;
    if (c < 0x20 || c == 0x7F) return false;
    if (c < 0x80) extra = 0;
    else if (c >= 0xC2 && c <= 0xDF) extra = 1;
    else return false;
    for (int k = 1; k <= extra; k++) {
      if (i + k >= len || ((unsigned char)s[i + k] & 0xC0) != 0x80) return false;
    }
    i += extra + 1;
  }
  return true;
}

// Splits on tabs in place. Returns the field count; more than maxFields gives maxFields + 1.
inline int dinoSplit(char* line, char** fields, int maxFields) {
  int n = 0;
  char* p = line;
  while (true) {
    if (n == maxFields) return maxFields + 1;
    fields[n++] = p;
    char* tab = strchr(p, '\t');
    if (!tab) return n;
    *tab = 0;
    p = tab + 1;
  }
}

inline bool dinoParseHash(const char* s, uint32_t& out) {
  if (strlen(s) != 8) return false;
  uint32_t v = 0;
  for (int i = 0; i < 8; i++) {
    const char c = s[i];
    int d;
    if (c >= '0' && c <= '9') d = c - '0';
    else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
    else return false;
    v = (v << 4) | (uint32_t)d;
  }
  out = v;
  return true;
}

inline bool dinoParseRange(char** f, int64_t minimum, int64_t maximum, bool older, DinoRange& r) {
  int64_t lo, hi;
  if (!dinoParseInt(f[1], minimum, maximum, lo) || !dinoParseInt(f[2], minimum, maximum, hi)) return false;
  if (older ? lo < hi : lo > hi) return false;  // lived: the older comes first
  r.lo = lo;
  r.hi = hi;
  r.present = true;
  return true;
}

inline bool dinoParseText(char** f, DinoText& t) {
  if (!dinoValidText(f[1], DINO_FIELD_LIMIT, false) || !dinoValidText(f[2], DINO_FIELD_LIMIT, false)) return false;
  t.de = f[1];
  t.en = f[2];
  return true;
}

// Parses <slug>.txt (ZDS1) in place: the text must stay alive as long as the sheet, and needs room for
// one more byte than len. False on anything unexpected, and "end" has to be the last line.
inline bool dinoParseSheet(char* text, size_t len, size_t capacity, const char* expectedSlug, DinoSheet& out) {
  memset(&out, 0, sizeof(out));
  if (!text || !expectedSlug || len == 0 || len >= capacity || len > DINO_TEXT_LIMIT) return false;
  text[len] = 0;
  if (strlen(text) != len) return false;  // A NUL inside
  if (!dinoValidSlug(expectedSlug, strlen(expectedSlug))) return false;

  DinoSheet s;
  memset(&s, 0, sizeof(s));
  s.credit = s.funSource = s.source = "";
  char* p = text;
  char* nl = strchr(p, '\n');
  if (!nl) return false;
  *nl = 0;
  char* f[10];
  if (dinoSplit(p, f, 3) != 3) return false;
  if (strcmp(f[0], "ZDS1") != 0 || strcmp(f[1], expectedSlug) != 0) return false;
  if (f[2][0] != 'Q' || strlen(f[2]) < 2 || strlen(f[2]) > 12) return false;
  int64_t dummy;
  if (!dinoParseInt(f[2] + 1, 1, 999999999999LL, dummy) || f[2][1] == '-') return false;
  s.slug = f[1];
  s.qid = f[2];

  static const char* const KEYS[] = {"name", "group", "image", "length", "height", "wingspan", "mass", "speed",
                                     "egg",  "lived", "named", "era",    "meaning", "diet",    "found", "fun",
                                     "source"};
  const int KEY_COUNT = (int)(sizeof(KEYS) / sizeof(KEYS[0]));
  uint32_t seen = 0;
  bool ended = false;
  p = nl + 1;
  while (true) {
    nl = strchr(p, '\n');
    if (!nl) return false;  // Cut off: a sheet with a missing end looks finished, so refuse it
    *nl = 0;
    if (strcmp(p, "end") == 0) {
      ended = true;
      p = nl + 1;
      break;
    }
    const int count = dinoSplit(p, f, 8);
    int key = -1;
    for (int k = 0; k < KEY_COUNT; k++) {
      if (strcmp(f[0], KEYS[k]) == 0) key = k;
    }
    p = nl + 1;
    if (key < 0) {
      if (f[0][0] == 0) return false;  // An empty line is no key
      continue;                        // A key from a later version: ignored
    }
    if (seen & (1UL << key)) return false;  // Every key at most once
    seen |= 1UL << key;
    const int expected = key == 2 ? 6 : key == 15 ? 4 : (key >= 3 && key <= 9) || (key >= 11 && key <= 14) ? 3 : 2;
    if (count != expected) return false;
    int64_t v, w, h;
    switch (key) {
      case 0:
        if (!dinoValidText(f[1], DINO_FIELD_LIMIT, false)) return false;
        s.name = f[1];
        break;
      case 1:
        if (!dinoParseInt(f[1], 0, 3, v) || f[1][1]) return false;
        s.group = (DinoGroup)v;
        break;
      case 2: {
        uint32_t hash;
        if (!dinoParseInt(f[1], 1, DINO_IMAGE_LIMIT, v) || !dinoParseInt(f[2], 1, DINO_IMAGE_W, w) ||
            !dinoParseInt(f[3], 1, DINO_IMAGE_H, h) || !dinoParseHash(f[4], hash)) {
          return false;
        }
        if (strcmp(f[5], "-") != 0) {
          if (!dinoValidText(f[5], DINO_CREDIT_LIMIT, false)) return false;
          s.credit = f[5];
        }
        s.hasImage = true;
        s.imageLength = (uint32_t)v;
        s.imageW = (uint16_t)w;
        s.imageH = (uint16_t)h;
        s.imageHash = hash;
        break;
      }
      case 3: if (!dinoParseRange(f, 1, 100000, false, s.length)) return false; break;
      case 4: if (!dinoParseRange(f, 1, 100000, false, s.height)) return false; break;
      case 5: if (!dinoParseRange(f, 1, 100000, false, s.wingspan)) return false; break;
      case 6: if (!dinoParseRange(f, 1, 999999999999LL, false, s.mass)) return false; break;
      case 7: if (!dinoParseRange(f, 1, 10000, false, s.speed)) return false; break;
      case 8: if (!dinoParseRange(f, 1, 10000, false, s.egg)) return false; break;
      case 9: if (!dinoParseRange(f, 0, 5000000000LL, true, s.lived) || s.lived.lo == 0) return false; break;
      case 10:
        if (!dinoParseInt(f[1], 1, 9999, v)) return false;
        s.named = (int32_t)v;
        break;
      case 11: if (!dinoParseText(f, s.era)) return false; break;
      case 12: if (!dinoParseText(f, s.meaning)) return false; break;
      case 13: if (!dinoParseText(f, s.diet)) return false; break;
      case 14: if (!dinoParseText(f, s.found)) return false; break;
      case 15:
        if (!dinoParseText(f, s.fun) || !dinoValidText(f[3], DINO_FIELD_LIMIT, false)) return false;
        s.funSource = f[3];
        break;
      case 16:
        if (!dinoValidText(f[1], DINO_FIELD_LIMIT, false)) return false;
        s.source = f[1];
        break;
    }
  }
  if (!ended || *p) return false;  // Lines after end
  if (!s.name) return false;
  out = s;
  return true;
}

inline uint32_t dinoFnv1a(const uint8_t* data, size_t length) {
  uint32_t h = 0x811C9DC5UL;
  for (size_t i = 0; i < length; i++) h = (h ^ data[i]) * 0x01000193UL;
  return h;
}

// True only if all of the picture arrived and it's byte for byte the one the sheet names.
// have: bytes received.
inline bool dinoImageOk(const DinoSheet& s, const uint8_t* data, size_t have) {
  if (!s.hasImage || !data || s.imageLength < 4 || have < s.imageLength) return false;
  const uint8_t* j = data;
  return j[0] == 0xFF && j[1] == 0xD8 && j[s.imageLength - 2] == 0xFF && j[s.imageLength - 1] == 0xD9 &&
         dinoFnv1a(j, s.imageLength) == s.imageHash;
}

// The licence of a picture credit: the text after the last ", ". The generator writes "maker, licence" or the
// licence alone, makers may contain ", " and licences never do. The whole text if there is no ", ".
// The footer shows this alone when the whole credit is too wide for it.
inline const char* dinoCreditLicence(const char* credit) {
  if (!credit) return "";
  const char* licence = credit;
  for (const char* p = strstr(credit, ", "); p; p = strstr(p + 1, ", ")) licence = p + 2;
  return licence;
}

// The text in the language asked for, else in the other one, else "".
inline const char* dinoText(const DinoText& t, bool english) {
  const char* first = english ? t.en : t.de;
  const char* second = english ? t.de : t.en;
  if (first) return first;
  return second ? second : "";
}

// Added to the AI's answer when it named an animal but no sheet came. The AI never mentions the sheet itself,
// because it answers before the lookup.
inline const char* dinoMissLine(bool english) {
  return english ? "I don't have a fact sheet for that." : "Einen Steckbrief dazu habe ich nicht.";
}

inline const char* dinoGroupNote(DinoGroup g, bool english) {
  switch (g) {
    case DINO_EARLY_BIRD: return english ? "Early bird" : "Urvogel";
    case DINO_PTEROSAUR: return english ? "Pterosaur - not a dinosaur" : "Flugsaurier - kein Dinosaurier";
    case DINO_MARINE: return english ? "Marine reptile - not a dinosaur" : "Meeresreptil - kein Dinosaurier";
    default: return "";
  }
}

// ---- Number formats ----

// value / divisor (10 or 100) with the decimal point of the language; trailing zeros trimmed
inline void dinoDecimal(int64_t value, int divisor, bool english, char* out, size_t size) {
  const long long whole = (long long)(value / divisor), frac = (long long)(value % divisor);
  const char point = english ? '.' : ',';
  if (frac == 0) {
    snprintf(out, size, "%lld", whole);
  } else if (divisor == 10 || frac % 10 == 0) {
    snprintf(out, size, "%lld%c%lld", whole, point, divisor == 10 ? frac : frac / 10);
  } else {
    snprintf(out, size, "%lld%c%02lld", whole, point, frac);
  }
}

// "<lo>-<hi><suffix>", or one value when both read the same
inline void dinoPair(const char* lo, const char* hi, const char* suffix, char* out, size_t size) {
  if (strcmp(lo, hi) == 0) {
    snprintf(out, size, "%s%s", lo, suffix);
  } else {
    snprintf(out, size, "%s-%s%s", lo, hi, suffix);
  }
}

// Below 100 cm in cm, else metres with up to two decimals. The upper end sets the unit.
inline void dinoFormatLength(int64_t loCm, int64_t hiCm, bool english, char* out, size_t size) {
  char a[24], b[24];
  if (hiCm < 100) {
    snprintf(a, sizeof(a), "%lld", (long long)loCm);
    snprintf(b, sizeof(b), "%lld", (long long)hiCm);
    dinoPair(a, b, " cm", out, size);
  } else {
    dinoDecimal(loCm, 100, english, a, sizeof(a));
    dinoDecimal(hiCm, 100, english, b, sizeof(b));
    dinoPair(a, b, " m", out, size);
  }
}

// Grams below 1000 g, kg below 1 t (one decimal below 10 kg), else tonnes with one decimal.
inline void dinoFormatMass(int64_t loG, int64_t hiG, bool english, char* out, size_t size) {
  char a[24], b[24];
  auto write = [&](int64_t g, char* o) {
    if (hiG < 1000) {
      snprintf(o, 24, "%lld", (long long)g);
    } else if (hiG < 1000000) {
      if (hiG < 10000) {
        dinoDecimal((g + 50) / 100, 10, english, o, 24);
      } else {
        snprintf(o, 24, "%lld", (long long)((g + 500) / 1000));
      }
    } else {
      int64_t tenths = (g + 50000) / 100000;
      if (tenths == 0 && g > 0) tenths = 1;
      dinoDecimal(tenths, 10, english, o, 24);
    }
  };
  write(loG, a);
  write(hiG, b);
  dinoPair(a, b, hiG < 1000 ? " g" : hiG < 1000000 ? " kg" : " t", out, size);
}

// Tenths of km/h
inline void dinoFormatSpeed(int64_t lo, int64_t hi, bool english, char* out, size_t size) {
  char a[24], b[24];
  dinoDecimal(lo, 10, english, a, sizeof(a));
  dinoDecimal(hi, 10, english, b, sizeof(b));
  dinoPair(a, b, " km/h", out, size);
}

// mm below 100 mm, else cm with one decimal
inline void dinoFormatEgg(int64_t loMm, int64_t hiMm, bool english, char* out, size_t size) {
  char a[24], b[24];
  if (hiMm < 100) {
    snprintf(a, sizeof(a), "%lld", (long long)loMm);
    snprintf(b, sizeof(b), "%lld", (long long)hiMm);
    dinoPair(a, b, " mm", out, size);
  } else {
    dinoDecimal(loMm, 10, english, a, sizeof(a));
    dinoDecimal(hiMm, 10, english, b, sizeof(b));
    dinoPair(a, b, " cm", out, size);
  }
}

// "vor 68-66 Mio. Jahren" / "68-66 million years ago": the older end first. Millions, one decimal below 10.
inline void dinoFormatLived(int64_t olderYears, int64_t youngerYears, bool english, char* out, size_t size) {
  char a[24], b[24], text[64];
  if (olderYears >= 1000000) {
    auto write = [&](int64_t years, char* o) {
      if (olderYears >= 10000000) {
        snprintf(o, 24, "%lld", (long long)((years + 500000) / 1000000));
      } else {
        dinoDecimal((years + 50000) / 100000, 10, english, o, 24);
      }
    };
    write(olderYears, a);
    write(youngerYears, b);
    dinoPair(a, b, english ? " million years ago" : " Mio. Jahren", text, sizeof(text));
  } else {
    snprintf(a, sizeof(a), "%lld", (long long)olderYears);
    snprintf(b, sizeof(b), "%lld", (long long)youngerYears);
    dinoPair(a, b, english ? " years ago" : " Jahren", text, sizeof(text));
  }
  snprintf(out, size, english ? "%s" : "vor %s", text);
}

// ---- Rows, pages, size ----

inline bool dinoHasText(const DinoText& t) { return t.de || t.en; }

inline int dinoRowCount(const DinoSheet& s) {
  return (s.length.present ? 1 : 0) + (s.height.present ? 1 : 0) + (s.wingspan.present ? 1 : 0) +
         (s.mass.present ? 1 : 0) + (s.speed.present ? 1 : 0) + (s.egg.present ? 1 : 0) +
         (dinoHasText(s.era) ? 1 : 0) + (s.lived.present ? 1 : 0) + (s.named ? 1 : 0) +
         (dinoHasText(s.meaning) ? 1 : 0) + (dinoHasText(s.diet) ? 1 : 0) + (dinoHasText(s.found) ? 1 : 0);
}

// The fact rows in their fixed order; rows without a value are left out. Returns how many were written.
inline int dinoFactRows(const DinoSheet& s, bool english, DinoRow* rows, int max) {
  int n = 0;
  auto start = [&](const char* de, const char* en) -> DinoRow* {
    if (n >= max) return nullptr;
    DinoRow* r = &rows[n++];
    snprintf(r->label, sizeof(r->label), "%s", english ? en : de);
    r->value[0] = 0;
    return r;
  };
  auto number = [&](const DinoRange& r, const char* de, const char* en, void (*format)(int64_t, int64_t, bool, char*, size_t)) {
    if (!r.present) return;
    DinoRow* row = start(de, en);
    if (row) format(r.lo, r.hi, english, row->value, sizeof(row->value));
  };
  auto text = [&](const DinoText& t, const char* de, const char* en) {
    if (!dinoHasText(t)) return;
    DinoRow* row = start(de, en);
    if (row) snprintf(row->value, sizeof(row->value), "%s", dinoText(t, english));
  };
  number(s.length, "L\xC3\xA4nge", "Length", dinoFormatLength);
  number(s.height, "H\xC3\xB6he", "Height", dinoFormatLength);
  number(s.wingspan, "Spannweite", "Wingspan", dinoFormatLength);
  number(s.mass, "Gewicht", "Weight", dinoFormatMass);
  if (s.speed.present) {
    DinoRow* row = start("Tempo", "Speed");
    if (row) {
      char range[40];
      dinoFormatSpeed(s.speed.lo, s.speed.hi, english, range, sizeof(range));
      snprintf(row->value, sizeof(row->value), english ? "%s, estimated" : "%s, gesch\xC3\xA4tzt", range);
    }
  }
  number(s.egg, "Ei", "Egg", dinoFormatEgg);
  text(s.era, "Zeit", "Era");
  number(s.lived, "Lebte", "Lived", dinoFormatLived);
  if (s.named) {
    DinoRow* row = start("Benannt", "Named");
    if (row) snprintf(row->value, sizeof(row->value), "%ld", (long)s.named);
  }
  text(s.meaning, "Bedeutet", "Means");  // Short: a long label narrows every value of the sheet
  text(s.diet, "Nahrung", "Diet");
  text(s.found, "Fundort", "Found in");
  return n;
}

// Shown only with its own picture or at least two fact rows; else the usual Wikipedia picture is better.
inline bool dinoWorthShowing(const DinoSheet& s) { return s.hasImage || dinoRowCount(s) >= 2; }

// The size comparison: the person (170 cm) beside the animal's bar, both at one scale and inside the area.
// The bar is the length, or the wingspan when there is no length. Without a height it is a line 5 px high.
inline bool dinoSizeLayout(const DinoSheet& s, int areaW, int areaH, DinoSizeLayout& out) {
  memset(&out, 0, sizeof(out));
  const bool useWingspan = !s.length.present && s.wingspan.present;
  if (!s.length.present && !s.wingspan.present) return false;
  const DinoRange& span = useWingspan ? s.wingspan : s.length;
  const bool line = useWingspan || !s.height.present;
  const int gap = 4;
  const int lineHeight = 5;
  if (areaW < 12 || areaH < lineHeight) return false;

  // A person is about a third as wide as tall; width and height are both kept inside the area
  const float personWide = DINO_PERSON_CM * 0.3f;
  const float spanCm = span.hi > DINO_PERSON_CM ? (float)span.hi : (float)DINO_PERSON_CM;
  const float tallCm = (!line && s.height.hi > DINO_PERSON_CM) ? (float)s.height.hi : (float)DINO_PERSON_CM;
  float scale = (float)(areaW - gap) / (spanCm + personWide);
  const float byHeight = (float)areaH / tallCm;
  if (byHeight < scale) scale = byHeight;

  int personH = (int)(DINO_PERSON_CM * scale + 0.5f);
  if (personH < 2) personH = 2;
  if (personH > areaH) personH = areaH;
  int personW = (int)(personH * 0.3f + 0.5f);
  if (personW < 1) personW = 1;
  int bodyH = line ? lineHeight : (int)((float)s.height.hi * scale + 0.5f);
  if (bodyH < 1) bodyH = 1;
  if (bodyH > areaH) bodyH = areaH;
  int bodyW = (int)((float)span.hi * scale + 0.5f);
  if (bodyW < 1) bodyW = 1;
  if (personW + gap + bodyW > areaW) bodyW = areaW - personW - gap;
  if (bodyW < 1) return false;
  int rangeW = (int)((float)(span.hi - span.lo) * scale + 0.5f);
  if (span.hi == span.lo) rangeW = 0;
  if (rangeW > bodyW) rangeW = bodyW;

  out.personX = 0;
  out.personY = areaH - personH;
  out.personW = personW;
  out.personH = personH;
  out.bodyX = personW + gap;
  out.bodyY = areaH - bodyH;
  out.bodyW = bodyW;
  out.bodyH = bodyH;
  out.rangeW = rangeW;
  out.lineOnly = line;
  out.wingspan = useWingspan;
  return true;
}

// Fits the rows into lines of the given width: a label column as wide as the widest label of these rows
// plus 6 px, the value after it. A value is broken at spaces (else after hyphens, never inside a UTF-8
// character) into at most two lines; a row that needs more is left out, and so is any row that no longer
// fits in maxLines. Returns the number of lines.
inline int dinoFactLines(const DinoRow* rows, int rowCount, int width, DinoMeasure measure, void* context,
                         DinoLine* lines, int maxLines) {
  int column = 0;
  for (int i = 0; i < rowCount; i++) {
    const int w = measure(rows[i].label, context);
    if (w > column) column = w;
  }
  column += 6;
  const int valueWidth = width - column;
  if (valueWidth <= 0) return 0;
  int n = 0;
  for (int i = 0; i < rowCount; i++) {
    const char* value = rows[i].value;
    const size_t len = strlen(value);
    char first[sizeof(DinoLine::value)];
    const char* rest = nullptr;  // The second line, if there is one
    if (measure(value, context) <= valueWidth) {
      memcpy(first, value, len + 1);
    } else {
      // The longest first line that ends at a break and fits
      size_t breakAt = 0;
      for (size_t k = 1; k < len; k++) {
        if (value[k] != ' ' && value[k - 1] != '-') continue;
        char trial[sizeof(DinoLine::value)];
        size_t cut = k;
        while (cut > 0 && value[cut - 1] == ' ') cut--;
        if (cut == 0) continue;
        memcpy(trial, value, cut);
        trial[cut] = 0;
        if (measure(trial, context) <= valueWidth) breakAt = k;
      }
      if (breakAt == 0) continue;  // One long word
      size_t cut = breakAt;
      while (cut > 0 && value[cut - 1] == ' ') cut--;
      memcpy(first, value, cut);
      first[cut] = 0;
      rest = value + breakAt;
      while (*rest == ' ') rest++;
      if (!*rest || measure(rest, context) > valueWidth) continue;  // Needs three lines
    }
    if (n + (rest ? 2 : 1) > maxLines) break;
    snprintf(lines[n].label, sizeof(lines[n].label), "%s", rows[i].label);
    snprintf(lines[n].value, sizeof(lines[n].value), "%s", first);
    n++;
    if (rest) {
      lines[n].label[0] = 0;
      snprintf(lines[n].value, sizeof(lines[n].value), "%s", rest);
      n++;
    }
  }
  return n;
}

// The pages in the order they are shown: picture, facts, size, fun fact. Returns how many.
// The facts take five lines a page, but the two lines of a row never part: where a row's second line
// would start a new page, the row starts it instead, and the page before is a line short. A facts page
// shows lines[firstLine .. firstLine + lineCount) of dinoFactLines, and so the view needs those lines too.
inline int dinoPages(const DinoSheet& s, const DinoLine* lines, int lineCount, DinoPageRef* pages, int max) {
  int n = 0;
  auto add = [&](DinoPage kind, int first, int count) {
    if (n < max) {
      pages[n].kind = kind;
      pages[n].firstLine = (uint8_t)first;
      pages[n].lineCount = (uint8_t)count;
      n++;
    }
  };
  DinoSizeLayout layout;
  if (s.hasImage) add(DINO_PAGE_PICTURE, 0, 0);
  for (int first = 0; first < lineCount;) {
    int count = lineCount - first < DINO_LINES_PER_PAGE ? lineCount - first : DINO_LINES_PER_PAGE;
    if (first + count < lineCount && lines[first + count].label[0] == 0 && count > 1) count--;
    add(DINO_PAGE_FACTS, first, count);
    first += count;
  }
  if (dinoSizeLayout(s, DINO_SIZE_AREA_W, DINO_SIZE_AREA_H, layout)) add(DINO_PAGE_SIZE, 0, 0);
  if (dinoHasText(s.fun)) add(DINO_PAGE_FUN, 0, 0);
  return n;
}

// Breaks a text at spaces into lines no wider than width, for the fun-fact page; a word wider than a line
// stands on a line of its own. Writes at most maxLines lines of lineSize bytes (lines may be nullptr to only
// count). If the text needs more, the last line written ends in "..." and still fits.
// Returns the number of lines the whole text needs.
inline int dinoWrapText(const char* text, int width, DinoMeasure measure, void* context, char* lines, int maxLines,
                        size_t lineSize) {
  char line[DINO_FIELD_LIMIT + 4];
  char trial[DINO_FIELD_LIMIT + 4];
  line[0] = 0;
  int count = 0;
  auto put = [&](const char* s) {
    if (lines && lineSize > 0 && count < maxLines) snprintf(lines + count * lineSize, lineSize, "%s", s);
    count++;
  };
  auto setWord = [&](const char* word, size_t len) {
    if (len >= sizeof(line)) len = sizeof(line) - 1;
    memcpy(line, word, len);
    line[len] = 0;
  };
  for (const char* p = text ? text : ""; *p;) {
    if (*p == ' ') {
      p++;
      continue;
    }
    const char* end = p;
    while (*end && *end != ' ') end++;
    const size_t wordLen = (size_t)(end - p);
    const size_t lineLen = strlen(line);
    if (lineLen == 0) {
      setWord(p, wordLen);
    } else if (lineLen + 1 + wordLen < sizeof(trial)) {
      memcpy(trial, line, lineLen);
      trial[lineLen] = ' ';
      memcpy(trial + lineLen + 1, p, wordLen);
      trial[lineLen + 1 + wordLen] = 0;
      if (measure(trial, context) <= width) {
        memcpy(line, trial, lineLen + 2 + wordLen);
      } else {
        put(line);
        setWord(p, wordLen);
      }
    } else {
      put(line);
      setWord(p, wordLen);
    }
    p = end;
  }
  if (line[0]) put(line);
  if (count > maxLines && maxLines > 0 && lines && lineSize > 4) {
    // Shorten the last line shown until it and the dots fit, never inside a UTF-8 character
    char* last = lines + (maxLines - 1) * lineSize;
    size_t n = strlen(last);
    while (true) {
      while (n > 0 && last[n - 1] == ' ') n--;
      snprintf(trial, sizeof(trial), "%.*s...", (int)n, last);
      if (n == 0 || (measure(trial, context) <= width && strlen(trial) < lineSize)) break;
      n--;
      while (n > 0 && ((unsigned char)last[n] & 0xC0) == 0x80) n--;
    }
    snprintf(last, lineSize, "%s", trial);
  }
  return count;
}
