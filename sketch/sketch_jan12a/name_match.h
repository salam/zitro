// Shared name normalising for exact matching of proteins and song titles.
// Plain C++ with no Arduino includes, tested on the host: tests/protein/run.sh and tests/music/run.sh
#pragma once

#include <stddef.h>

// Lower case, umlauts spelled out, hyphens and repeated spaces as one space, no space at the ends.
// forTitles == false (proteins): False for empty text, for any other non-ASCII character, and for the
// index's own separators.
// forTitles == true (songs): also folds Latin-1 accents to the base letter, drops apostrophes (' and U+2019)
// with no space, and turns any other ASCII character that is no letter, digit or space into a space.
inline bool eduNormaliseName(const char* in, char* out, size_t capacity, bool forTitles) {
  if (!in || !out || capacity == 0) return false;
  size_t n = 0;
  const auto put = [&](char c) {
    if (n + 1 >= capacity) return false;
    out[n++] = c;
    return true;
  };
  for (const unsigned char* p = (const unsigned char*)in; *p; p++) {
    const unsigned char c = *p;
    const char* spelled = nullptr;
    if (c == 0xC3) {
      p++;
      if (*p == 0xA4 || *p == 0x84) spelled = "ae";
      else if (*p == 0xB6 || *p == 0x96) spelled = "oe";
      else if (*p == 0xBC || *p == 0x9C) spelled = "ue";
      else if (*p == 0x9F) spelled = "ss";
      else if (forTitles) {
        // Latin-1 letters: second byte 0x80..0xBF is code point 0xC0..0xFF, capitals fold like small letters.
        const unsigned char l = (*p >= 0x80 && *p <= 0x9F) ? (unsigned char)(*p + 0x20) : *p;
        char base = 0;
        if (l >= 0xA0 && l <= 0xA5) base = 'a';
        else if (l == 0xA7) base = 'c';
        else if (l >= 0xA8 && l <= 0xAB) base = 'e';
        else if (l >= 0xAC && l <= 0xAF) base = 'i';
        else if (l == 0xB1) base = 'n';
        else if (l >= 0xB2 && l <= 0xB5) base = 'o';
        else if (l >= 0xB9 && l <= 0xBB) base = 'u';
        if (!base) return false;
        if (!put(base)) return false;
        continue;
      } else return false;
      if (!put(spelled[0]) || !put(spelled[1])) return false;
    } else if (forTitles && c == '\'') {
      continue;
    } else if (forTitles && c == 0xE2 && p[1] == 0x80 && p[2] == 0x99) {
      p += 2;
    } else if (c == ' ' || c == '-' || c == '_' || c == '\t' ||
               (forTitles && c < 0x7F && !((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')))) {
      if (n > 0 && out[n - 1] != ' ' && !put(' ')) return false;
    } else if (c < 0x20 || c >= 0x7F || c == '|' || c == ';') {
      return false;
    } else {
      if (!put((c >= 'A' && c <= 'Z') ? (char)(c + 32) : (char)c)) return false;
    }
  }
  while (n > 0 && out[n - 1] == ' ') n--;
  out[n] = 0;
  return n > 0;
}
