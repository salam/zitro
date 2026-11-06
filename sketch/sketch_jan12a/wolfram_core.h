// Wolfram|Alpha LLM API answers: splits the plain text into sections and picks what a child sees first.
// Plain C++, no Arduino. Tested on the host: tests/wolfram/run.sh
//
// Nothing here rewrites what Wolfram said. Sections and lines are spans into a copy of the response,
// and the only thing added is a German title for sections it knows.
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

static const size_t WOLFRAM_BODY_CAPACITY = 6144;  // We ask for 900 characters; a larger answer isn't a normal one
static const size_t WOLFRAM_TEXT_CAPACITY = WOLFRAM_BODY_CAPACITY + 768;  // Plus the German section titles
static const int WOLFRAM_MAX_SECTIONS = 16;
static const int WOLFRAM_MAX_LINES = 64;
static const int WOLFRAM_MAX_ASSUMPTIONS = 4;
static const size_t WOLFRAM_TITLE_LIMIT = 100;
static const size_t WOLFRAM_URL_LIMIT = 160;

struct WolframSpan {
  uint16_t start, len;
};

struct WolframLine {
  WolframSpan span;
  bool note;  // "(assuming inner radius b cm)": said by Wolfram about the line above, shown smaller
};

struct WolframSection {
  WolframSpan title;
  uint8_t firstLine, lineCount;
  bool isInput;  // "Input" or "Input interpretation"
  bool isPlot;   // Holds the plot image (WolframDoc::plotUrl); may have no text of its own
};

struct WolframDoc {
  char text[WOLFRAM_TEXT_CAPACITY];
  uint16_t textLen;
  WolframSection sections[WOLFRAM_MAX_SECTIONS];  // Display order: what to see first comes first
  uint8_t sectionCount;
  WolframLine lines[WOLFRAM_MAX_LINES];
  uint8_t lineCount;
  WolframSpan assumptions[WOLFRAM_MAX_ASSUMPTIONS];  // "Assuming atomic mass": shown with the first section
  uint8_t assumptionCount;
  WolframSpan label;    // What Wolfram understood, for the bottom line. May be empty
  WolframSpan plotUrl;  // First Plot[...] image. May be empty
};

// ---------------------------------------------------------------------------------------------------------
// Small helpers

inline char wolframLower(char c) {
  return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
}

inline bool wolframSpanEquals(const char* text, WolframSpan s, const char* word) {
  const size_t n = strlen(word);
  if (s.len != n) return false;
  for (size_t i = 0; i < n; i++) {
    if (wolframLower(text[s.start + i]) != word[i]) return false;
  }
  return true;
}

inline bool wolframSpanStarts(const char* text, WolframSpan s, const char* word) {
  const size_t n = strlen(word);
  if (s.len < n) return false;
  for (size_t i = 0; i < n; i++) {
    if (wolframLower(text[s.start + i]) != word[i]) return false;
  }
  return true;
}

inline bool wolframSpanEnds(const char* text, WolframSpan s, const char* word) {
  const size_t n = strlen(word);
  if (s.len < n) return false;
  WolframSpan tail = {(uint16_t)(s.start + s.len - n), (uint16_t)n};
  return wolframSpanEquals(text, tail, word);
}

inline bool wolframSpanContains(const char* text, WolframSpan s, const char* word) {
  const size_t n = strlen(word);
  if (n == 0 || s.len < n) return false;
  for (size_t i = 0; i + n <= s.len; i++) {
    if (memcmp(text + s.start + i, word, n) == 0) return true;
  }
  return false;
}

// Reads one UTF-8 character. False if the bytes aren't valid UTF-8 (bad, overlong, surrogate, too large, cut off).
inline bool wolframDecodeUtf8(const char* p, size_t available, uint32_t& cp, int& length) {
  if (available == 0) return false;
  const uint8_t b0 = (uint8_t)p[0];
  if (b0 < 0x80) {
    cp = b0;
    length = 1;
    return true;
  }
  int n;
  uint32_t min;
  if (b0 >= 0xC2 && b0 <= 0xDF) {
    n = 2;
    cp = b0 & 0x1F;
    min = 0x80;
  } else if (b0 >= 0xE0 && b0 <= 0xEF) {
    n = 3;
    cp = b0 & 0x0F;
    min = 0x800;
  } else if (b0 >= 0xF0 && b0 <= 0xF4) {
    n = 4;
    cp = b0 & 0x07;
    min = 0x10000;
  } else {
    return false;
  }
  if (available < (size_t)n) return false;
  for (int i = 1; i < n; i++) {
    const uint8_t b = (uint8_t)p[i];
    if ((b & 0xC0) != 0x80) return false;
    cp = (cp << 6) | (b & 0x3F);
  }
  if (cp < min || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return false;
  length = n;
  return true;
}

// An answer we can trust to be text: valid UTF-8, no NUL or control characters except line ends and tabs
inline bool wolframValidBody(const char* body, size_t len) {
  size_t i = 0;
  while (i < len) {
    uint32_t cp = 0;
    int n = 0;
    if (!wolframDecodeUtf8(body + i, len - i, cp, n)) return false;
    if ((cp < 0x20 && cp != '\n' && cp != '\r' && cp != '\t') || cp == 0x7F) return false;
    i += n;
  }
  return true;
}

// Fonts reach U+05FF; these others are drawn as lines (wolfram_layout.h) or read as a space.
inline bool wolframIsDrawnSymbol(uint32_t cp) {
  switch (cp) {
    case 0x2248:  // approximately equal
    case 0x2260:  // not equal
    case 0x2264:  // less or equal
    case 0x2265:  // greater or equal
    case 0x221E:  // infinity
    case 0x221A:  // root
    case 0x222B:  // integral
    case 0x2211:  // sum
    case 0x220F:  // product
    case 0x2192:  // arrow
    case 0x2212:  // minus
    case 0x2213:  // minus or plus
    case 0x2026:  // dots
    case 0x22C5:  // dot
    case 0x2013:  // dashes
    case 0x2014:
    case 0x2018:  // quotes
    case 0x2019:
    case 0x201C:
    case 0x201D:
      return true;
    default:
      return false;
  }
}

inline bool wolframIsSpaceLike(uint32_t cp) {
  return cp == ' ' || cp == '\t' || cp == 0xA0 || cp == 0x2009 || cp == 0x200A || cp == 0x202F;
}

inline bool wolframCanShow(uint32_t cp) {
  return cp <= 0x5FF || wolframIsDrawnSymbol(cp) || wolframIsSpaceLike(cp);
}

inline bool wolframSpanCanShow(const char* text, WolframSpan s) {
  size_t i = 0;
  while (i < s.len) {
    uint32_t cp = 0;
    int n = 0;
    if (!wolframDecodeUtf8(text + s.start + i, s.len - i, cp, n)) return false;
    if (!wolframCanShow(cp)) return false;
    i += n;
  }
  return true;
}

// ---------------------------------------------------------------------------------------------------------
// What each section is

// Never shown: the echo of the question, instructions for other programs, the website link, and
// two sections whose text only makes sense as Wolfram's own picture or table.
inline bool wolframTitleIsNoise(const char* text, WolframSpan t) {
  return wolframSpanEquals(text, t, "query") || wolframSpanEquals(text, t, "assumption") ||
         wolframSpanEquals(text, t, "assumptions") || wolframSpanStarts(text, t, "wolfram|alpha website result") ||
         wolframSpanEquals(text, t, "manipulatives illustration") ||
         wolframSpanStarts(text, t, "typical human computation") || wolframSpanEquals(text, t, "interpretations") ||
         wolframSpanEquals(text, t, "interpretation");
}

inline bool wolframTitleIsInput(const char* text, WolframSpan t) {
  return wolframSpanEquals(text, t, "input") || wolframSpanEquals(text, t, "input interpretation");
}

// The answer itself: the first of these in Wolfram's order comes first. An operation's own section counts
// ("Derivative", "Indefinite integral", "Limit", "Infinite sum"). Without any, the first real section does.
inline bool wolframTitleIsAnswer(const char* text, WolframSpan t) {
  static const char* const exact[] = {"result",  "equation", "solution", "solutions", "statement",
                                      "value",   "sum",      "product",  "definition"};
  for (const char* word : exact) {
    if (wolframSpanEquals(text, t, word)) return true;
  }
  static const char* const starts[] = {"derivative", "indefinite integral", "definite integral", "limit",
                                       "infinite sum", "infinite product"};
  for (const char* word : starts) {
    if (wolframSpanStarts(text, t, word)) return true;
  }
  static const char* const ends[] = {" result", " solution", " solutions", " results"};
  for (const char* word : ends) {
    if (wolframSpanEnds(text, t, word)) return true;
  }
  return false;
}

// German titles where they're certain; anything else stays as Wolfram wrote it
inline const char* wolframGermanTitle(const char* text, WolframSpan t) {
  static const char* const table[][2] = {
      {"result", "Ergebnis"},
      {"exact result", "Exaktes Ergebnis"},
      {"current result", "Aktuelles Ergebnis"},
      {"decimal approximation", "Dezimalzahl"},
      {"decimal form", "Dezimalzahl"},
      {"scientific notation", "Zehnerpotenz"},
      {"solution", "L\xC3\xB6sung"},
      {"solutions", "L\xC3\xB6sungen"},
      {"integer solutions", "Ganzzahlige L\xC3\xB6sungen"},
      {"equation", "Formel"},
      {"associated equation", "Formel"},
      {"statement", "Aussage"},
      {"derivative", "Ableitung"},
      {"indefinite integral", "Unbestimmtes Integral"},
      {"definite integral", "Bestimmtes Integral"},
      {"limit", "Grenzwert"},
      {"infinite sum", "Unendliche Summe"},
      {"sum convergence", "Konvergenz"},
      {"input", "Eingabe"},
      {"input interpretation", "Verstanden als"},
      {"input values", "Eingesetzte Werte"},
      {"alternate form", "Andere Form"},
      {"alternate forms", "Andere Formen"},
      {"plot", "Kurve"},
      {"plots", "Kurven"},
      {"root plot", "Kurve"},
      {"inequality plot", "Kurve"},
      {"plots of the integral", "Kurve des Integrals"},
      {"visual representation of the integral", "Integral als Bild"},
      {"number name", "Zahlwort"},
      {"number names", "Zahlw\xC3\xB6rter"},
      {"number length", "Stellen"},
      {"unit conversions", "Umrechnungen"},
      {"additional conversions", "Weitere Umrechnungen"},
      {"sum of roots", "Summe der Nullstellen"},
      {"product of roots", "Produkt der Nullstellen"},
      {"interval notation", "Intervalle"},
      {"divisors", "Teiler"},
      {"domain", "Definitionsbereich"},
      {"range", "Wertebereich"},
      {"parity", "Symmetrie"},
      {"history", "Geschichte"},
      {"properties", "Eigenschaften"},
  };
  for (const auto& row : table) {
    if (wolframSpanEquals(text, t, row[0])) return row[1];
  }
  return nullptr;
}

// A plot URL we'd fetch: only https, only Wolfram's own hosts, a plain .png path.
inline bool wolframPlotUrlOk(const char* url, size_t len) {
  static const char scheme[] = "https://";
  static const char suffix[] = ".wolframalpha.com";
  const size_t schemeLen = sizeof(scheme) - 1, suffixLen = sizeof(suffix) - 1;
  if (len <= schemeLen + suffixLen || len > WOLFRAM_URL_LIMIT) return false;
  if (memcmp(url, scheme, schemeLen) != 0) return false;
  size_t hostEnd = schemeLen;
  while (hostEnd < len && url[hostEnd] != '/') {
    const char c = url[hostEnd];
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '-')) return false;
    hostEnd++;
  }
  if (hostEnd == len || hostEnd - schemeLen <= suffixLen) return false;
  if (memcmp(url + hostEnd - suffixLen, suffix, suffixLen) != 0) return false;
  for (size_t i = hostEnd; i < len; i++) {
    const char c = url[i];
    const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '/' ||
                    c == '_' || c == '-' || c == '.';
    if (!ok) return false;
  }
  return len > 4 && memcmp(url + len - 4, ".png", 4) == 0;
}

// ---------------------------------------------------------------------------------------------------------
// Parsing

// A whole line in parentheses with words in it: "(assuming inner radius b cm)", "(Taylor series)".
// Not "(x + 1) (x + 2)" or "(2, inf)".
inline bool wolframLineIsNote(const char* text, WolframSpan s) {
  if (s.len < 3 || text[s.start] != '(' || text[s.start + s.len - 1] != ')') return false;
  int depth = 0, letters = 0;
  bool longWord = false;
  for (uint16_t i = 0; i < s.len; i++) {
    const char c = text[s.start + i];
    if (c == '(') depth++;
    if (c == ')') {
      depth--;
      if (depth == 0 && i + 1 < s.len) return false;
    }
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
      if (++letters >= 4) longWord = true;
    } else {
      letters = 0;
    }
  }
  return depth == 0 && longWord;
}

inline bool wolframLineIsNoise(const char* text, WolframSpan s) {
  return wolframSpanStarts(text, s, "wolfram language code:") || wolframSpanStarts(text, s, "http://") ||
         wolframSpanStarts(text, s, "https://") || wolframSpanContains(text, s, "assumption=");
}

inline void wolframTrim(const char* text, WolframSpan& s) {
  while (s.len > 0 && (text[s.start] == ' ' || text[s.start] == '\t')) {
    s.start++;
    s.len--;
  }
  while (s.len > 0) {
    const char c = text[s.start + s.len - 1];
    if (c != ' ' && c != '\t' && c != '\r') break;
    s.len--;
  }
}

inline bool wolframLineIsHeader(const char* text, WolframSpan s) {
  if (s.len < 2 || s.len > WOLFRAM_TITLE_LIMIT || text[s.start + s.len - 1] != ':') return false;
  return !wolframSpanStarts(text, s, "image:") && !wolframSpanStarts(text, s, "http");
}

// The equation "K = (m v^2)/2 | " has a table bar with nothing after it
inline void wolframStripEmptyCell(const char* text, WolframSpan& s) {
  while (s.len > 0 && text[s.start + s.len - 1] == '|') {
    s.len--;
    wolframTrim(text, s);
  }
}

inline WolframSpan wolframStripQuotes(const char* text, WolframSpan s) {
  if (s.len >= 2 && text[s.start] == '"' && text[s.start + s.len - 1] == '"') {
    s.start++;
    s.len -= 2;
  }
  return s;
}

// Fills doc from the response. False if there's nothing a person could read in it: the caller shows the picture.
inline bool wolframParse(const char* body, size_t len, bool german, WolframDoc& doc) {
  doc.textLen = 0;
  doc.sectionCount = doc.lineCount = doc.assumptionCount = 0;
  doc.label = doc.plotUrl = WolframSpan{0, 0};
  if (!body || len == 0 || len > WOLFRAM_BODY_CAPACITY || !wolframValidBody(body, len)) return false;
  memcpy(doc.text, body, len);
  doc.text[len] = 0;
  doc.textLen = (uint16_t)len;
  const char* text = doc.text;

  WolframSection raw[WOLFRAM_MAX_SECTIONS];
  int rawCount = 0;
  int current = -1;      // Section being read, -1 before the first or in one we drop
  bool inAssumptions = false, inQuery = false;
  bool blank = true, afterHeader = false, full = false;
  WolframSpan image = {0, 0}, query = {0, 0}, inputLine = {0, 0}, interpretationLine = {0, 0};
  int imageSection = -1, plotSection = -1;
  bool hasText[WOLFRAM_MAX_SECTIONS] = {};  // More than remarks about something that isn't here

  size_t pos = 0;
  while (pos <= len && !full) {
    size_t end = pos;
    while (end < len && text[end] != '\n') end++;
    WolframSpan line = {(uint16_t)pos, (uint16_t)(end - pos)};
    wolframTrim(text, line);
    pos = end + 1;

    if (line.len == 0) {
      blank = true;
      afterHeader = false;
      continue;
    }
    if ((blank || afterHeader) && wolframLineIsHeader(text, line)) {
      WolframSpan title = {line.start, (uint16_t)(line.len - 1)};
      wolframTrim(text, title);
      inAssumptions = wolframSpanEquals(text, title, "assumption") || wolframSpanEquals(text, title, "assumptions");
      inQuery = wolframSpanEquals(text, title, "query");
      current = -1;
      image = WolframSpan{0, 0};
      if (!wolframTitleIsNoise(text, title)) {
        if (rawCount >= WOLFRAM_MAX_SECTIONS) {
          full = true;
        } else {
          current = rawCount++;
          raw[current].title = title;
          raw[current].firstLine = doc.lineCount;
          raw[current].lineCount = 0;
          raw[current].isInput = wolframTitleIsInput(text, title);
          raw[current].isPlot = false;
        }
      }
      blank = false;
      afterHeader = true;
      continue;
    }
    blank = afterHeader = false;

    if (inQuery) {
      if (query.len == 0) query = wolframStripQuotes(text, line);
      continue;
    }
    if (inAssumptions) {
      // What Wolfram took for granted stays visible; how to change it is for other programs
      if (wolframSpanStarts(text, line, "assuming ") && !wolframSpanContains(text, line, "assumption=") &&
          doc.assumptionCount < WOLFRAM_MAX_ASSUMPTIONS) {
        doc.assumptions[doc.assumptionCount++] = line;
      }
      continue;
    }
    if (current < 0) continue;

    if (wolframSpanStarts(text, line, "image:")) {
      image = WolframSpan{(uint16_t)(line.start + 6), (uint16_t)(line.len - 6)};
      wolframTrim(text, image);
      imageSection = current;
      continue;
    }
    if (wolframSpanStarts(text, line, "wolfram language code: plot[") && image.len > 0 && imageSection == current &&
        doc.plotUrl.len == 0 && wolframPlotUrlOk(text + image.start, image.len)) {
      doc.plotUrl = image;
      plotSection = current;
      raw[current].isPlot = true;
      continue;
    }
    if (wolframLineIsNoise(text, line)) continue;

    wolframStripEmptyCell(text, line);
    if (line.len == 0) continue;
    if (doc.lineCount >= WOLFRAM_MAX_LINES) {
      // Never half a list of solutions: this section goes, and so does everything after it
      doc.lineCount = raw[current].firstLine;
      raw[current].lineCount = 0;
      hasText[current] = false;
      if (plotSection == current) {
        plotSection = -1;
        doc.plotUrl = WolframSpan{0, 0};
        raw[current].isPlot = false;
      }
      current = -1;
      full = true;
      break;
    }
    doc.lines[doc.lineCount].span = line;
    doc.lines[doc.lineCount].note = wolframLineIsNote(text, line);
    doc.lineCount++;
    raw[current].lineCount++;
    if (!doc.lines[doc.lineCount - 1].note) hasText[current] = true;
    if (raw[current].isInput) {
      if (wolframSpanEquals(text, raw[current].title, "input interpretation")) {
        if (interpretationLine.len == 0) interpretationLine = line;
      } else if (inputLine.len == 0) {
        inputLine = line;
      }
    }
  }

  // What counts as a page: text, or the plot
  int answer = -1, firstReal = -1, firstAny = -1;
  for (int i = 0; i < rawCount; i++) {
    if (!hasText[i]) continue;
    if (firstAny < 0) firstAny = i;
    if (raw[i].isInput) continue;
    if (firstReal < 0) firstReal = i;
    if (answer < 0 && wolframTitleIsAnswer(text, raw[i].title)) answer = i;
  }
  if (answer < 0) answer = firstReal;
  if (answer < 0) answer = firstAny;
  if (answer < 0) return false;  // No text at all: a picture alone isn't an answer

  // The answer, then what Wolfram understood, then the rest in Wolfram's order
  doc.sections[doc.sectionCount++] = raw[answer];
  for (int i = 0; i < rawCount; i++) {
    if (i != answer && raw[i].isInput && hasText[i]) doc.sections[doc.sectionCount++] = raw[i];
  }
  for (int i = 0; i < rawCount; i++) {
    if (i == answer || raw[i].isInput) continue;
    if (hasText[i] || raw[i].isPlot) doc.sections[doc.sectionCount++] = raw[i];
  }

  // A first page with symbols no font or drawing can show would mislead: the caller shows the picture instead
  const WolframSection& first = doc.sections[0];
  for (int i = 0; i < first.lineCount; i++) {
    if (!wolframSpanCanShow(text, doc.lines[first.firstLine + i].span)) return false;
  }

  doc.label = interpretationLine.len > 0 ? interpretationLine : (inputLine.len > 0 ? inputLine : query);

  if (german) {
    for (int i = 0; i < doc.sectionCount; i++) {
      const char* title = wolframGermanTitle(text, doc.sections[i].title);
      if (!title) continue;
      const size_t n = strlen(title);
      if (doc.textLen + n + 1 >= WOLFRAM_TEXT_CAPACITY) break;
      memcpy(doc.text + doc.textLen + 1, title, n);  // After a NUL, so the response stays a string of its own
      doc.sections[i].title = WolframSpan{(uint16_t)(doc.textLen + 1), (uint16_t)n};
      doc.textLen = (uint16_t)(doc.textLen + 1 + n);
      doc.text[doc.textLen] = 0;
    }
  }
  return true;
}

// ---------------------------------------------------------------------------------------------------------
// Plot image helpers

// Width and height from a PNG's header. False if it isn't one.
inline bool wolframPngSize(const uint8_t* data, size_t len, uint32_t& width, uint32_t& height) {
  static const uint8_t magic[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
  if (!data || len < 24 || memcmp(data, magic, 8) != 0 || memcmp(data + 12, "IHDR", 4) != 0) return false;
  width = ((uint32_t)data[16] << 24) | ((uint32_t)data[17] << 16) | ((uint32_t)data[18] << 8) | data[19];
  height = ((uint32_t)data[20] << 24) | ((uint32_t)data[21] << 16) | ((uint32_t)data[22] << 8) | data[23];
  return width > 0 && height > 0;
}

// The size that fits a picture into a box, keeping its shape. Never enlarges.
inline void wolframFitSize(uint32_t width, uint32_t height, int boxWidth, int boxHeight, int& outWidth,
                           int& outHeight) {
  if (width <= (uint32_t)boxWidth && height <= (uint32_t)boxHeight) {
    outWidth = (int)width;
    outHeight = (int)height;
    return;
  }
  // Compare the two scale factors without floating point
  if ((uint64_t)width * (uint32_t)boxHeight >= (uint64_t)height * (uint32_t)boxWidth) {
    outWidth = boxWidth;
    outHeight = (int)((uint64_t)height * (uint32_t)boxWidth / width);
  } else {
    outHeight = boxHeight;
    outWidth = (int)((uint64_t)width * (uint32_t)boxHeight / height);
  }
  if (outWidth < 1) outWidth = 1;
  if (outHeight < 1) outHeight = 1;
}

// Shrinks a picture by averaging, so a thin curve stays a line. Pixels are RGB565 with swapped bytes,
// the way a sprite stores them. dst may not overlap src.
inline void wolframShrink565(const uint16_t* src, int srcWidth, int srcHeight, uint16_t* dst, int dstWidth,
                             int dstHeight) {
  for (int y = 0; y < dstHeight; y++) {
    const int y0 = (int)((int64_t)y * srcHeight / dstHeight);
    int y1 = (int)((int64_t)(y + 1) * srcHeight / dstHeight);
    if (y1 <= y0) y1 = y0 + 1;
    for (int x = 0; x < dstWidth; x++) {
      const int x0 = (int)((int64_t)x * srcWidth / dstWidth);
      int x1 = (int)((int64_t)(x + 1) * srcWidth / dstWidth);
      if (x1 <= x0) x1 = x0 + 1;
      uint32_t r = 0, g = 0, b = 0, count = 0;
      for (int sy = y0; sy < y1; sy++) {
        const uint16_t* row = src + (size_t)sy * srcWidth;
        for (int sx = x0; sx < x1; sx++) {
          const uint16_t swapped = row[sx];
          const uint16_t p = (uint16_t)((swapped << 8) | (swapped >> 8));
          r += (p >> 11) & 0x1F;
          g += (p >> 5) & 0x3F;
          b += p & 0x1F;
          count++;
        }
      }
      const uint16_t p = (uint16_t)(((r / count) << 11) | ((g / count) << 5) | (b / count));
      dst[(size_t)y * dstWidth + x] = (uint16_t)((p << 8) | (p >> 8));
    }
  }
}
