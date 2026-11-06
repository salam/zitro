// Lays a Wolfram answer out for the 240x135 screen, as a list of text pieces and lines per page.
// Plain C++: text widths come through a callback. Tested on the host: tests/wolfram/run.sh
//
// Every text piece is a slice of Wolfram's own response, so a number can't change on the way to the screen.
// Symbols the fonts don't have are drawn as lines. Order of the pieces follows the source, which is how
// the tests pin that all digits arrive unchanged and in order.
#pragma once

#include "wolfram_core.h"
#include "wolfram_formula.h"

enum WolframFontId : uint8_t { WOLFRAM_FONT_SMALL, WOLFRAM_FONT_10, WOLFRAM_FONT_14 };  // The small one is ASCII only
enum WolframColorId : uint8_t { WOLFRAM_COLOR_MAIN, WOLFRAM_COLOR_DIM, WOLFRAM_COLOR_TITLE };
enum WolframOpType : uint8_t { WOLFRAM_OP_TEXT, WOLFRAM_OP_LINE };

struct WolframOp {
  WolframOpType type;
  uint8_t font, color;
  int16_t x, y;  // Text: left end of the baseline. Line: first end
  uint16_t a, b;  // Text: offset and length in the document. Line: second end (as int16_t)
};

struct WolframPage {
  uint16_t titleFirst, titleCount;
  uint16_t noteFirst, noteCount;  // What Wolfram assumed: at the bottom of every page of its section
  uint16_t first, count;
  uint8_t section;  // Index in WolframDoc::sections
  bool plot;        // Shows the plot image instead
};

static const int WOLFRAM_MAX_OPS = 1400;
static const int WOLFRAM_MAX_PAGES = 24;
static const int WOLFRAM_MAX_ROWS = 80;  // In one section
static const int WOLFRAM_SCREEN_WIDTH = 240;
static const int WOLFRAM_CONTENT_BOTTOM = 117;  // Above the bottom line
static const int WOLFRAM_MARGIN = 3;
static const int WOLFRAM_ROW_WIDTH = WOLFRAM_SCREEN_WIDTH - 2 * WOLFRAM_MARGIN;
static const int WOLFRAM_ROW_GAP = 2;

struct WolframLayout {
  WolframOp ops[WOLFRAM_MAX_OPS];
  uint16_t opCount;
  WolframPage pages[WOLFRAM_MAX_PAGES];
  uint8_t pageCount;
};

struct WolframRow {
  uint16_t first, end;
  int16_t width, ascent, descent;
  bool center;
};

typedef int (*WolframMeasure)(uint8_t font, const char* text, int len);

// Working memory for building a layout. Large, so not on the stack.
struct WolframScratch {
  WolframFormula formula;
  WolframRow rows[WOLFRAM_MAX_ROWS];
};

struct WolframBuilder {
  WolframLayout* layout;
  WolframScratch* scratch;
  const char* text;
  WolframMeasure measure;
  int rowCount;
  uint8_t baseFont;
  bool overflow;
};

inline int wolframFontAscent(uint8_t font) {
  return font == WOLFRAM_FONT_14 ? 20 : (font == WOLFRAM_FONT_10 ? 14 : 8);
}

inline int wolframFontDescent(uint8_t font) {
  return font == WOLFRAM_FONT_14 ? 5 : (font == WOLFRAM_FONT_10 ? 4 : 2);
}

// Height of a small letter: where symbols sit
inline int wolframFontXHeight(uint8_t font) {
  return font == WOLFRAM_FONT_14 ? 14 : (font == WOLFRAM_FONT_10 ? 10 : 5);
}

// ---------------------------------------------------------------------------------------------------------
// Ops

inline void wolframAddText(WolframBuilder& b, uint8_t font, uint8_t color, int x, int y, uint16_t start, uint16_t len) {
  WolframLayout& L = *b.layout;
  if (L.opCount >= WOLFRAM_MAX_OPS) {
    b.overflow = true;
    return;
  }
  WolframOp& op = L.ops[L.opCount++];
  op.type = WOLFRAM_OP_TEXT;
  op.font = font;
  op.color = color;
  op.x = (int16_t)x;
  op.y = (int16_t)y;
  op.a = start;
  op.b = len;
}

inline void wolframAddLine(WolframBuilder& b, uint8_t color, int x0, int y0, int x1, int y1) {
  WolframLayout& L = *b.layout;
  if (L.opCount >= WOLFRAM_MAX_OPS) {
    b.overflow = true;
    return;
  }
  WolframOp& op = L.ops[L.opCount++];
  op.type = WOLFRAM_OP_LINE;
  op.font = 0;
  op.color = color;
  op.x = (int16_t)x0;
  op.y = (int16_t)y0;
  op.a = (uint16_t)(int16_t)x1;
  op.b = (uint16_t)(int16_t)y1;
}

// A stroke as heavy as the font's: two lines side by side at the large size
inline void wolframStroke(WolframBuilder& b, uint8_t font, uint8_t color, int x0, int y0, int x1, int y1) {
  wolframAddLine(b, color, x0, y0, x1, y1);
  if (font != WOLFRAM_FONT_14) return;
  const int dx = x1 > x0 ? x1 - x0 : x0 - x1;
  const int dy = y1 > y0 ? y1 - y0 : y0 - y1;
  if (dx >= dy) {
    wolframAddLine(b, color, x0, y0 + 1, x1, y1 + 1);
  } else {
    wolframAddLine(b, color, x0 + 1, y0, x1 + 1, y1);
  }
}

// ---------------------------------------------------------------------------------------------------------
// Symbols

inline WolframSymbolId wolframSymbolForChar(uint32_t cp) {
  switch (cp) {
    case 0x2248: return WOLFRAM_SYM_APPROX;
    case 0x2260: return WOLFRAM_SYM_NOT_EQUAL;
    case 0x2264: return WOLFRAM_SYM_LESS_EQUAL;
    case 0x2265: return WOLFRAM_SYM_GREATER_EQUAL;
    case 0x221E: return WOLFRAM_SYM_INFINITY;
    case 0x221A: return WOLFRAM_SYM_ROOT;
    case 0x222B: return WOLFRAM_SYM_INTEGRAL;
    case 0x2211: return WOLFRAM_SYM_SUM;
    case 0x220F: return WOLFRAM_SYM_PRODUCT;
    case 0x2192: return WOLFRAM_SYM_ARROW;
    case 0x2212: return WOLFRAM_SYM_MINUS;
    case 0x2213: return WOLFRAM_SYM_MINUS_PLUS;
    case 0x2026: return WOLFRAM_SYM_DOTS;
    case 0x22C5: return WOLFRAM_SYM_DOT;
    case 0x2013:
    case 0x2014: return WOLFRAM_SYM_DASH;
    case 0x2018:
    case 0x2019:
    case 0x201C:
    case 0x201D: return WOLFRAM_SYM_QUOTE;
    default: return cp > 0x5FF ? WOLFRAM_SYM_BOX : WOLFRAM_SYM_NONE;
  }
}

inline int wolframSymbolWidth(WolframSymbolId symbol, uint8_t font) {
  const int h = wolframFontXHeight(font);
  switch (symbol) {
    case WOLFRAM_SYM_INFINITY: return h + h / 2 + 2;
    case WOLFRAM_SYM_ARROW: return h + 2;
    case WOLFRAM_SYM_APPROX:
    case WOLFRAM_SYM_NOT_EQUAL:
    case WOLFRAM_SYM_LESS_EQUAL:
    case WOLFRAM_SYM_GREATER_EQUAL:
    case WOLFRAM_SYM_MINUS:
    case WOLFRAM_SYM_MINUS_PLUS:
    case WOLFRAM_SYM_SUM:
    case WOLFRAM_SYM_PRODUCT:
    case WOLFRAM_SYM_ROOT:
    case WOLFRAM_SYM_BOX: return h + 1;
    case WOLFRAM_SYM_INTEGRAL: return h / 2 + 4;
    case WOLFRAM_SYM_DOTS: return h + 3;
    case WOLFRAM_SYM_DASH: return h;
    case WOLFRAM_SYM_DOT:
    case WOLFRAM_SYM_QUOTE: return 3;
    case WOLFRAM_SYM_THREE: return 4;
    default: return 0;
  }
}

// Draws a symbol with its left end at x, standing on the baseline y
inline void wolframDrawSymbol(WolframBuilder& b, WolframSymbolId symbol, int x, int y, uint8_t font, uint8_t color) {
  const int h = wolframFontXHeight(font);
  const int w = wolframSymbolWidth(symbol, font);
  const int r = x + w - 2;  // Right end, a pixel of air beside it
  const int left = x + 1;
  const int mid = y - h / 2;
  const int gap = h / 4 < 2 ? 2 : h / 4;
  switch (symbol) {
    case WOLFRAM_SYM_APPROX: {
      const int third = (r - left) / 3;
      for (int line = -1; line <= 1; line += 2) {
        const int yy = mid + line * (gap - 1) + (line > 0 ? 1 : 0);
        wolframStroke(b, font, color, left, yy + 1, left + third, yy - 1);
        wolframStroke(b, font, color, left + third, yy - 1, r - third, yy + 1);
        wolframStroke(b, font, color, r - third, yy + 1, r, yy - 1);
      }
      break;
    }
    case WOLFRAM_SYM_NOT_EQUAL:
      wolframStroke(b, font, color, left, mid - gap / 2 - 1, r, mid - gap / 2 - 1);
      wolframStroke(b, font, color, left, mid + gap / 2 + 1, r, mid + gap / 2 + 1);
      wolframAddLine(b, color, r - 1, mid - gap - 2, left + 1, mid + gap + 2);
      break;
    case WOLFRAM_SYM_LESS_EQUAL:
      wolframStroke(b, font, color, r, mid - gap - 1, left, mid);
      wolframStroke(b, font, color, left, mid, r, mid + gap + 1);
      wolframStroke(b, font, color, left, mid + gap + 3, r, mid + gap + 3);
      break;
    case WOLFRAM_SYM_GREATER_EQUAL:
      wolframStroke(b, font, color, left, mid - gap - 1, r, mid);
      wolframStroke(b, font, color, r, mid, left, mid + gap + 1);
      wolframStroke(b, font, color, left, mid + gap + 3, r, mid + gap + 3);
      break;
    case WOLFRAM_SYM_INFINITY: {
      // Two loops crossing in the middle
      const int span = r - left;
      const int q = span / 4;
      const int c = left + span / 2;
      const int top = mid - h / 4 - 1, bottom = mid + h / 4 + 1;
      const int xs[12] = {left, left + 1, left + q, c, c + q, r - 1, r, r - 1, c + q, c, left + q, left + 1};
      const int ys[12] = {mid, top + 1, top, mid, bottom, bottom - 1, mid, top + 1, top, mid, bottom, bottom - 1};
      for (int i = 0; i < 12; i++) {
        const int j = (i + 1) % 12;
        wolframStroke(b, font, color, xs[i], ys[i], xs[j], ys[j]);
      }
      break;
    }
    case WOLFRAM_SYM_ROOT:
      wolframStroke(b, font, color, left, mid + 1, left + 2, mid);
      wolframStroke(b, font, color, left + 2, mid, left + 4, y);
      wolframStroke(b, font, color, left + 4, y, left + 6, y - h - 2);
      wolframStroke(b, font, color, left + 6, y - h - 2, r, y - h - 2);
      break;
    case WOLFRAM_SYM_INTEGRAL: {
      const int top = y - wolframFontAscent(font) - 2, bottom = y + wolframFontDescent(font) + 2;
      wolframStroke(b, font, color, r, top + 1, r - 1, top);
      wolframStroke(b, font, color, r - 1, top, r - 2, top + 1);
      wolframStroke(b, font, color, r - 2, top + 1, left + 2, bottom - 1);
      wolframStroke(b, font, color, left + 2, bottom - 1, left + 1, bottom);
      wolframStroke(b, font, color, left + 1, bottom, left, bottom - 1);
      break;
    }
    case WOLFRAM_SYM_SUM: {
      const int top = y - wolframFontAscent(font) + 2, bottom = y + 1;
      const int c = (top + bottom) / 2;
      wolframStroke(b, font, color, r, top, left, top);
      wolframStroke(b, font, color, left, top, left + (r - left) / 2, c);
      wolframStroke(b, font, color, left + (r - left) / 2, c, left, bottom);
      wolframStroke(b, font, color, left, bottom, r, bottom);
      break;
    }
    case WOLFRAM_SYM_PRODUCT: {
      const int top = y - wolframFontAscent(font) + 2, bottom = y + 1;
      wolframStroke(b, font, color, left, top, r, top);
      wolframStroke(b, font, color, left + 1, top, left + 1, bottom);
      wolframStroke(b, font, color, r - 1, top, r - 1, bottom);
      break;
    }
    case WOLFRAM_SYM_ARROW:
      wolframStroke(b, font, color, left, mid, r, mid);
      wolframStroke(b, font, color, r, mid, r - gap, mid - gap);
      wolframStroke(b, font, color, r, mid, r - gap, mid + gap);
      break;
    case WOLFRAM_SYM_MINUS:
      wolframStroke(b, font, color, left, mid, r, mid);
      break;
    case WOLFRAM_SYM_MINUS_PLUS: {
      const int c = (left + r) / 2;
      wolframStroke(b, font, color, left, mid - gap - 1, r, mid - gap - 1);
      wolframStroke(b, font, color, left, mid + 1, r, mid + 1);
      wolframStroke(b, font, color, c, mid - gap + 1, c, mid + gap + 2);
      wolframStroke(b, font, color, left, mid + gap + 3, r, mid + gap + 3);
      break;
    }
    case WOLFRAM_SYM_DOTS:
      for (int i = 0; i < 3; i++) {
        const int dot = left + i * (w - 2) / 3;
        wolframAddLine(b, color, dot, y - 1, dot + 1, y - 1);
        wolframAddLine(b, color, dot, y, dot + 1, y);
      }
      break;
    case WOLFRAM_SYM_DOT:
      wolframAddLine(b, color, x, mid, x + 1, mid);
      wolframAddLine(b, color, x, mid + 1, x + 1, mid + 1);
      break;
    case WOLFRAM_SYM_DASH:
      wolframStroke(b, font, color, x, mid, x + w - 1, mid);
      break;
    case WOLFRAM_SYM_QUOTE: {
      const int top = y - h - 3;
      wolframStroke(b, font, color, x + 1, top, x + 1, top + 3);
      break;
    }
    case WOLFRAM_SYM_THREE:
      wolframAddLine(b, color, x, y - 6, x + 2, y - 6);
      wolframAddLine(b, color, x + 3, y - 5, x + 3, y - 4);
      wolframAddLine(b, color, x + 1, y - 3, x + 2, y - 3);
      wolframAddLine(b, color, x + 3, y - 2, x + 3, y - 1);
      wolframAddLine(b, color, x, y, x + 2, y);
      break;
    case WOLFRAM_SYM_BOX: {
      const int top = y - h - 1;
      wolframAddLine(b, color, left, top, r, top);
      wolframAddLine(b, color, r, top, r, y);
      wolframAddLine(b, color, r, y, left, y);
      wolframAddLine(b, color, left, y, left, top);
      break;
    }
    default:
      break;
  }
}

// ---------------------------------------------------------------------------------------------------------
// Formula: measuring

// Scripts and nested fractions step down one size, then stop
inline uint8_t wolframFontAtLevel(uint8_t base, uint8_t level) {
  if (level == 0) return base;
  return base == WOLFRAM_FONT_14 ? WOLFRAM_FONT_10 : WOLFRAM_FONT_SMALL;
}

inline uint8_t wolframTextFont(const WolframBuilder& b, uint8_t font, uint16_t start, uint16_t len) {
  if (font != WOLFRAM_FONT_SMALL) return font;
  for (uint16_t i = 0; i < len; i++) {
    if ((uint8_t)b.text[start + i] >= 0x80) return WOLFRAM_FONT_10;  // pi in an exponent: the small font is ASCII only
  }
  return font;
}

inline bool wolframHasFraction(const WolframFormula& f, int16_t i) {
  if (i < 0) return false;
  const WolframNode& n = f.nodes[i];
  if (n.type == WOLFRAM_NODE_FRAC) return true;
  if (n.type == WOLFRAM_NODE_ROW) {
    for (int16_t child = n.a; child >= 0; child = f.nodes[child].next) {
      if (wolframHasFraction(f, child)) return true;
    }
    return false;
  }
  return wolframHasFraction(f, n.a) || wolframHasFraction(f, n.b) || wolframHasFraction(f, n.c);
}

inline int wolframGapBetween(const WolframFormula& f, int16_t prev, int16_t node, uint8_t base, uint8_t level) {
  if (prev < 0) return 0;
  const WolframNode& p = f.nodes[prev];
  const WolframNode& n = f.nodes[node];
  const bool nearRelation = (p.flags | n.flags) & WOLFRAM_FLAG_RELATION;
  const bool nearOp = (n.type == WOLFRAM_NODE_OP && (n.flags & WOLFRAM_FLAG_BINARY)) ||
                      (p.type == WOLFRAM_NODE_OP && (p.flags & WOLFRAM_FLAG_BINARY));
  if (level > 0) return (nearRelation || nearOp || (n.flags & WOLFRAM_FLAG_SPACE_BEFORE)) ? 2 : 0;
  const int large = base == WOLFRAM_FONT_14 ? 1 : 0;
  if (nearRelation) return 5 + large;
  if (nearOp) return 4 + large;
  if (n.flags & WOLFRAM_FLAG_SPACE_BEFORE) return 3 + large;
  return 0;
}

inline void wolframMeasureNode(WolframBuilder& b, int16_t i, uint8_t level);

inline void wolframMeasureText(WolframBuilder& b, WolframNode& n, uint8_t level) {
  const uint8_t font = wolframTextFont(b, wolframFontAtLevel(b.baseFont, level), n.start, n.len);
  n.width = (int16_t)b.measure(font, b.text + n.start, n.len);
  n.ascent = (int16_t)wolframFontAscent(font);
  n.descent = (int16_t)wolframFontDescent(font);
}

inline void wolframMeasureNode(WolframBuilder& b, int16_t i, uint8_t level) {
  WolframFormula& f = b.scratch->formula;
  WolframNode& n = f.nodes[i];
  n.level = level;
  const uint8_t font = wolframFontAtLevel(b.baseFont, level);
  n.width = n.ascent = n.descent = 0;
  switch (n.type) {
    case WOLFRAM_NODE_ROW: {
      int16_t prev = -1;
      int x = 0, ascent = 0, descent = 0;
      for (int16_t child = n.a; child >= 0; child = f.nodes[child].next) {
        wolframMeasureNode(b, child, level);
        const WolframNode& c = f.nodes[child];
        x += wolframGapBetween(f, prev, child, b.baseFont, level) + c.width;
        if (c.ascent > ascent) ascent = c.ascent;
        if (c.descent > descent) descent = c.descent;
        prev = child;
      }
      n.width = (int16_t)x;
      n.ascent = (int16_t)ascent;
      n.descent = (int16_t)descent;
      break;
    }
    case WOLFRAM_NODE_TEXT:
      wolframMeasureText(b, n, level);
      break;
    case WOLFRAM_NODE_OP:
    case WOLFRAM_NODE_SYMBOL:
      if (n.symbol != WOLFRAM_SYM_NONE) {
        n.width = (int16_t)wolframSymbolWidth(n.symbol, font);
        n.ascent = (int16_t)wolframFontAscent(font);
        n.descent = (int16_t)wolframFontDescent(font);
      } else {
        wolframMeasureText(b, n, level);
      }
      break;
    case WOLFRAM_NODE_GLOSS: {
      const uint8_t glossFont = WOLFRAM_FONT_10;
      n.width = (int16_t)b.measure(glossFont, b.text + n.start, n.len);
      n.ascent = (int16_t)wolframFontAscent(glossFont);
      n.descent = (int16_t)wolframFontDescent(glossFont);
      break;
    }
    case WOLFRAM_NODE_GROUP: {
      wolframMeasureNode(b, n.a, level);
      const WolframNode& inner = f.nodes[n.a];
      n.ascent = inner.ascent;
      n.descent = inner.descent;
      n.width = inner.width;
      if (n.flags & WOLFRAM_FLAG_HIDDEN) break;
      if (inner.ascent + inner.descent > wolframFontAscent(font) + wolframFontDescent(font)) {
        n.flags |= WOLFRAM_FLAG_TALL;
        const int parenWidth = font == WOLFRAM_FONT_14 ? 7 : 5;
        n.width = (int16_t)(inner.width + 2 * parenWidth);
      } else {
        n.ascent = (int16_t)wolframFontAscent(font);
        n.descent = (int16_t)wolframFontDescent(font);
        const int paren = b.measure(font, "(", 1), close = b.measure(font, ")", 1);
        n.width = (int16_t)(inner.width + paren + close);
      }
      break;
    }
    case WOLFRAM_NODE_FRAC: {
      if (level > 0) {
        // Too small to stack: written a/b, with the parentheses back
        n.flags |= WOLFRAM_FLAG_INLINE;
        if (f.nodes[n.a].type == WOLFRAM_NODE_GROUP) f.nodes[n.a].flags &= (uint8_t)~WOLFRAM_FLAG_HIDDEN;
        if (f.nodes[n.b].type == WOLFRAM_NODE_GROUP) f.nodes[n.b].flags &= (uint8_t)~WOLFRAM_FLAG_HIDDEN;
        wolframMeasureNode(b, n.a, level);
        wolframMeasureNode(b, n.b, level);
        const WolframNode& top = f.nodes[n.a];
        const WolframNode& below = f.nodes[n.b];
        n.width = (int16_t)(top.width + b.measure(font, "/", 1) + below.width);
        n.ascent = top.ascent > below.ascent ? top.ascent : below.ascent;
        n.descent = top.descent > below.descent ? top.descent : below.descent;
        break;
      }
      const uint8_t inner = (wolframHasFraction(f, n.a) || wolframHasFraction(f, n.b)) ? 1 : 0;
      wolframMeasureNode(b, n.a, inner);
      wolframMeasureNode(b, n.b, inner);
      const WolframNode& top = f.nodes[n.a];
      const WolframNode& below = f.nodes[n.b];
      const int axis = wolframFontXHeight(font) / 2;
      n.width = (int16_t)((top.width > below.width ? top.width : below.width) + 4);
      n.ascent = (int16_t)(axis + 1 + top.ascent + top.descent);
      n.descent = (int16_t)(below.ascent + below.descent + 2 - axis);
      break;
    }
    case WOLFRAM_NODE_SCRIPT: {
      wolframMeasureNode(b, n.a, level);
      const WolframNode& base = f.nodes[n.a];
      int side = 0, ascent = base.ascent, descent = base.descent;
      if (n.c >= 0) {
        wolframMeasureNode(b, n.c, (uint8_t)(level + 1));
        const WolframNode& above = f.nodes[n.c];
        const int rise1 = base.ascent * 3 / 5, rise2 = base.ascent - above.ascent;
        const int rise = rise1 > rise2 ? rise1 : rise2;
        if (rise + above.ascent > ascent) ascent = rise + above.ascent;
        if (above.width > side) side = above.width;
      }
      if (n.b >= 0) {
        wolframMeasureNode(b, n.b, (uint8_t)(level + 1));
        const WolframNode& below = f.nodes[n.b];
        const int drop = base.descent > 3 ? base.descent : 3;
        if (drop + below.descent > descent) descent = drop + below.descent;
        if (below.width > side) side = below.width;
      }
      n.width = (int16_t)(base.width + side + 1);
      n.ascent = (int16_t)ascent;
      n.descent = (int16_t)descent;
      break;
    }
    case WOLFRAM_NODE_ROOT: {
      wolframMeasureNode(b, n.a, level);
      const WolframNode& inner = f.nodes[n.a];
      const bool cube = n.len == 8;
      const int tick = (font == WOLFRAM_FONT_14 ? 9 : 7) + (cube ? 2 : 0);
      n.width = (int16_t)(tick + inner.width + 2);
      n.ascent = (int16_t)(inner.ascent + 3);
      n.descent = inner.descent;
      break;
    }
    case WOLFRAM_NODE_BIGOP: {
      if (n.b >= 0) wolframMeasureNode(b, n.b, (uint8_t)(level + 1));
      if (n.c >= 0) wolframMeasureNode(b, n.c, (uint8_t)(level + 1));
      const int belowWidth = n.b >= 0 ? f.nodes[n.b].width : 0;
      const int aboveWidth = n.c >= 0 ? f.nodes[n.c].width : 0;
      const int belowHeight = n.b >= 0 ? f.nodes[n.b].ascent + f.nodes[n.b].descent : 0;
      const int aboveHeight = n.c >= 0 ? f.nodes[n.c].ascent + f.nodes[n.c].descent : 0;
      const int widest = belowWidth > aboveWidth ? belowWidth : aboveWidth;
      const int fontAscent = wolframFontAscent(font), fontDescent = wolframFontDescent(font);
      if (n.len == 8) {  // "integral"; the parser lets no other 8-letter name through
        // Limits beside the sign, at its two ends
        n.symbol = WOLFRAM_SYM_INTEGRAL;
        const int sign = wolframSymbolWidth(WOLFRAM_SYM_INTEGRAL, font);
        n.width = (int16_t)(sign + (widest > 0 ? widest + 2 : 0));
        n.ascent = (int16_t)(fontAscent + 2);
        n.descent = (int16_t)(fontDescent + 2);
        if (n.b >= 0) {
          const int low = f.nodes[n.b].ascent - 1 + f.nodes[n.b].descent;  // Starts just below the baseline
          if (low > n.descent) n.descent = (int16_t)low;
        }
      } else if (n.len == 3 && memcmp(b.text + n.start, "lim", 3) == 0) {
        // The word, with what it runs over below it
        n.symbol = WOLFRAM_SYM_NONE;
        const int word = b.measure(font, b.text + n.start, n.len);
        n.width = (int16_t)(word > widest ? word : widest);
        n.ascent = (int16_t)fontAscent;
        n.descent = (int16_t)(fontDescent + belowHeight);
      } else {
        // Sum and product, with limits below and above
        n.symbol = n.len == 3 ? WOLFRAM_SYM_SUM : WOLFRAM_SYM_PRODUCT;
        const int sign = wolframSymbolWidth(n.symbol, font);
        n.width = (int16_t)(sign > widest ? sign : widest);
        n.ascent = (int16_t)(fontAscent + aboveHeight);
        n.descent = (int16_t)(3 + belowHeight);
      }
      break;
    }
  }
}

// ---------------------------------------------------------------------------------------------------------
// Formula: drawing

inline void wolframEmitNode(WolframBuilder& b, int16_t i, int x, int y, uint8_t color) {
  WolframFormula& f = b.scratch->formula;
  const WolframNode& n = f.nodes[i];
  const uint8_t level = n.level;
  const uint8_t font = wolframFontAtLevel(b.baseFont, level);
  switch (n.type) {
    case WOLFRAM_NODE_ROW: {
      int16_t prev = -1;
      int cx = x;
      for (int16_t child = n.a; child >= 0; child = f.nodes[child].next) {
        cx += wolframGapBetween(f, prev, child, b.baseFont, level);
        wolframEmitNode(b, child, cx, y, color);
        cx += f.nodes[child].width;
        prev = child;
      }
      break;
    }
    case WOLFRAM_NODE_TEXT:
      wolframAddText(b, wolframTextFont(b, font, n.start, n.len), color, x, y, n.start, n.len);
      break;
    case WOLFRAM_NODE_OP:
    case WOLFRAM_NODE_SYMBOL:
      if (n.symbol != WOLFRAM_SYM_NONE) {
        wolframDrawSymbol(b, n.symbol, x, y, font, color);
      } else {
        wolframAddText(b, wolframTextFont(b, font, n.start, n.len), color, x, y, n.start, n.len);
      }
      break;
    case WOLFRAM_NODE_GLOSS:
      wolframAddText(b, WOLFRAM_FONT_10, WOLFRAM_COLOR_DIM, x, y, n.start, n.len);
      break;
    case WOLFRAM_NODE_GROUP: {
      const WolframNode& inner = f.nodes[n.a];
      if (n.flags & WOLFRAM_FLAG_HIDDEN) {
        wolframEmitNode(b, n.a, x, y, color);
      } else if (n.flags & WOLFRAM_FLAG_TALL) {
        const int w = font == WOLFRAM_FONT_14 ? 7 : 5;
        const int top = y - inner.ascent, bottom = y + inner.descent;
        const int bend = (bottom - top) / 4;
        wolframStroke(b, font, color, x + w - 2, top, x + 1, top + bend);
        wolframStroke(b, font, color, x + 1, top + bend, x + 1, bottom - bend);
        wolframStroke(b, font, color, x + 1, bottom - bend, x + w - 2, bottom);
        wolframEmitNode(b, n.a, x + w, y, color);
        const int right = x + w + inner.width;
        wolframStroke(b, font, color, right + 1, top, right + w - 2, top + bend);
        wolframStroke(b, font, color, right + w - 2, top + bend, right + w - 2, bottom - bend);
        wolframStroke(b, font, color, right + w - 2, bottom - bend, right + 1, bottom);
      } else {
        const int paren = b.measure(font, "(", 1);
        wolframAddText(b, font, color, x, y, n.start, 1);
        wolframEmitNode(b, n.a, x + paren, y, color);
        wolframAddText(b, font, color, x + paren + inner.width, y, n.closeAt, 1);
      }
      break;
    }
    case WOLFRAM_NODE_FRAC: {
      const WolframNode& top = f.nodes[n.a];
      const WolframNode& below = f.nodes[n.b];
      if (n.flags & WOLFRAM_FLAG_INLINE) {
        wolframEmitNode(b, n.a, x, y, color);
        const int slashX = x + top.width;
        wolframAddText(b, font, color, slashX, y, n.start, 1);
        wolframEmitNode(b, n.b, slashX + b.measure(font, "/", 1), y, color);
        break;
      }
      const int axis = wolframFontXHeight(font) / 2;
      const int bar = y - axis;
      wolframEmitNode(b, n.a, x + (n.width - top.width) / 2, bar - 1 - top.descent, color);
      wolframStroke(b, font, color, x + 1, bar, x + n.width - 2, bar);
      wolframEmitNode(b, n.b, x + (n.width - below.width) / 2, bar + 2 + below.ascent, color);
      break;
    }
    case WOLFRAM_NODE_SCRIPT: {
      const WolframNode& base = f.nodes[n.a];
      wolframEmitNode(b, n.a, x, y, color);
      const int sideX = x + base.width + 1;
      const bool supFirst = (n.flags & WOLFRAM_FLAG_SUP_FIRST) != 0;
      for (int pass = 0; pass < 2; pass++) {
        const bool drawSup = (pass == 0) == supFirst;
        if (drawSup && n.c >= 0) {
          const WolframNode& above = f.nodes[n.c];
          const int rise1 = base.ascent * 3 / 5, rise2 = base.ascent - above.ascent;
          wolframEmitNode(b, n.c, sideX, y - (rise1 > rise2 ? rise1 : rise2), color);
        } else if (!drawSup && n.b >= 0) {
          wolframEmitNode(b, n.b, sideX, y + (base.descent > 3 ? base.descent : 3), color);
        }
      }
      break;
    }
    case WOLFRAM_NODE_ROOT: {
      const WolframNode& inner = f.nodes[n.a];
      const bool cube = n.len == 8;
      const int tick = (font == WOLFRAM_FONT_14 ? 9 : 7) + (cube ? 2 : 0);
      const int top = y - inner.ascent - 2;
      const int bottom = y + (inner.descent > 1 ? inner.descent - 1 : 1);
      const int low = y - wolframFontXHeight(font) / 2;
      if (cube) wolframDrawSymbol(b, WOLFRAM_SYM_THREE, x, low - 3, WOLFRAM_FONT_SMALL, color);
      const int tx = x + (cube ? 2 : 0);
      wolframStroke(b, font, color, tx, low + 1, tx + 2, low);
      wolframStroke(b, font, color, tx + 2, low, tx + 4, bottom);
      wolframStroke(b, font, color, tx + 4, bottom, x + tick - 1, top);
      wolframStroke(b, font, color, x + tick - 1, top, x + n.width - 1, top);
      wolframEmitNode(b, n.a, x + tick, y, color);
      break;
    }
    case WOLFRAM_NODE_BIGOP: {
      const bool supFirst = (n.flags & WOLFRAM_FLAG_SUP_FIRST) != 0;
      const int fontAscent = wolframFontAscent(font), fontDescent = wolframFontDescent(font);
      if (n.symbol == WOLFRAM_SYM_INTEGRAL) {
        const int sign = wolframSymbolWidth(WOLFRAM_SYM_INTEGRAL, font);
        wolframDrawSymbol(b, WOLFRAM_SYM_INTEGRAL, x, y, font, color);
        for (int pass = 0; pass < 2; pass++) {
          const bool drawSup = (pass == 0) == supFirst;
          if (drawSup && n.c >= 0) {
            wolframEmitNode(b, n.c, x + sign + 1, y - fontAscent + f.nodes[n.c].ascent - 2, color);
          } else if (!drawSup && n.b >= 0) {
            wolframEmitNode(b, n.b, x + sign + 1, y + f.nodes[n.b].ascent - 1, color);
          }
        }
      } else if (n.symbol == WOLFRAM_SYM_NONE) {
        wolframAddText(b, font, color, x + (n.width - b.measure(font, b.text + n.start, n.len)) / 2, y, n.start, n.len);
        if (n.b >= 0) {
          const WolframNode& below = f.nodes[n.b];
          wolframEmitNode(b, n.b, x + (n.width - below.width) / 2, y + fontDescent + below.ascent, color);
        }
      } else {
        const int sign = wolframSymbolWidth(n.symbol, font);
        for (int pass = 0; pass < 2; pass++) {
          const bool drawSup = (pass == 0) == supFirst;
          if (drawSup && n.c >= 0) {
            const WolframNode& above = f.nodes[n.c];
            wolframEmitNode(b, n.c, x + (n.width - above.width) / 2, y - fontAscent - above.descent + 1, color);
          } else if (!drawSup && n.b >= 0) {
            const WolframNode& below = f.nodes[n.b];
            wolframEmitNode(b, n.b, x + (n.width - below.width) / 2, y + 3 + below.ascent, color);
          }
        }
        wolframDrawSymbol(b, n.symbol, x + (n.width - sign) / 2, y, font, color);
      }
      break;
    }
  }
}

// ---------------------------------------------------------------------------------------------------------
// Rows

inline bool wolframOpenRow(WolframBuilder& b, bool center) {
  if (b.rowCount >= WOLFRAM_MAX_ROWS) {
    b.overflow = true;
    return false;
  }
  WolframRow& row = b.scratch->rows[b.rowCount];
  row.first = b.layout->opCount;
  row.end = b.layout->opCount;
  row.width = row.ascent = row.descent = 0;
  row.center = center;
  return true;
}

inline void wolframCloseRow(WolframBuilder& b, int width, int ascent, int descent) {
  if (b.rowCount >= WOLFRAM_MAX_ROWS) {
    b.overflow = true;
    return;
  }
  WolframRow& row = b.scratch->rows[b.rowCount++];
  row.end = b.layout->opCount;
  row.width = (int16_t)width;
  row.ascent = (int16_t)ascent;
  row.descent = (int16_t)descent;
}

// Lays out a formula line in the current base font, breaking it at = + - where it has to.
// False: it doesn't fit, nothing was added, and the caller shows the line as text.
inline bool wolframAddFormula(WolframBuilder& b, WolframSpan line) {
  WolframFormula& f = b.scratch->formula;
  if (!wolframFormulaParse(b.text, line, f) || !wolframFormulaRoundTrips(f, line)) return false;
  wolframMeasureNode(b, f.root, 0);

  const uint16_t opStart = b.layout->opCount;
  const int rowStart = b.rowCount;
  const int contentHeight = WOLFRAM_CONTENT_BOTTOM - 14;

  // First try to break only at operators and before words; then between factors too
  int16_t breaksBefore[WOLFRAM_MAX_ROWS];
  int breakCount = 0;
  bool fits = false;
  for (int pass = 0; pass < 2 && !fits; pass++) {
    fits = true;
    breakCount = 0;
    int rowWidth = 0, chunkWidth = 0;
    int16_t prev = -1, chunkFirst = -1, beforeChunk = -1;
    const auto closeChunk = [&]() {
      if (chunkWidth > WOLFRAM_ROW_WIDTH) {
        fits = false;
        return;
      }
      const int gap = rowWidth > 0 ? wolframGapBetween(f, beforeChunk, chunkFirst, b.baseFont, 0) : 0;
      if (rowWidth > 0 && rowWidth + gap + chunkWidth > WOLFRAM_ROW_WIDTH) {
        if (breakCount >= WOLFRAM_MAX_ROWS) {
          fits = false;
          return;
        }
        breaksBefore[breakCount++] = chunkFirst;
        rowWidth = chunkWidth;
      } else {
        rowWidth += gap + chunkWidth;
      }
    };
    for (int16_t child = f.nodes[f.root].a; child >= 0 && fits; child = f.nodes[child].next) {
      const WolframNode& c = f.nodes[child];
      const bool canBreak = prev >= 0 && ((c.type == WOLFRAM_NODE_OP && (c.flags & WOLFRAM_FLAG_BINARY)) ||
                                          c.type == WOLFRAM_NODE_GLOSS ||
                                          (pass == 1 && (c.flags & WOLFRAM_FLAG_SPACE_BEFORE)));
      if (prev < 0 || canBreak) {
        if (canBreak) closeChunk();
        chunkFirst = child;
        beforeChunk = prev;
        chunkWidth = c.width;
      } else {
        chunkWidth += wolframGapBetween(f, prev, child, b.baseFont, 0) + c.width;
      }
      prev = child;
    }
    if (fits) closeChunk();
  }
  if (!fits) return false;

  // Now draw row by row
  int16_t child = f.nodes[f.root].a;
  for (int part = 0; part <= breakCount; part++) {
    const int16_t stop = part < breakCount ? breaksBefore[part] : (int16_t)-1;
    if (!wolframOpenRow(b, true)) break;
    int x = 0, ascent = 0, descent = 0;
    int16_t prev = -1;
    while (child >= 0 && child != stop) {
      x += wolframGapBetween(f, prev, child, b.baseFont, 0);
      wolframEmitNode(b, child, x, 0, WOLFRAM_COLOR_MAIN);
      const WolframNode& c = f.nodes[child];
      x += c.width;
      if (c.ascent > ascent) ascent = c.ascent;
      if (c.descent > descent) descent = c.descent;
      prev = child;
      child = c.next;
    }
    wolframCloseRow(b, x, ascent, descent);
    if (ascent + descent > contentHeight || x > WOLFRAM_ROW_WIDTH) {
      b.layout->opCount = opStart;  // Too tall for the screen: rather as text
      b.rowCount = rowStart;
      return false;
    }
  }
  if (b.overflow) return false;
  return true;
}

// Length of the exponent after a "^" in running text: digits, maybe with a minus. 0 if it isn't one.
// "0.583^_" and "x^(2)" stay as they are.
inline uint16_t wolframPlainExponent(const char* text, uint16_t pos, uint16_t end, bool afterValue) {
  if (!afterValue || pos >= end || text[pos] != '^') return 0;
  uint16_t p = (uint16_t)(pos + 1);
  if (p < end && text[p] == '-') p++;
  const uint16_t digits = p;
  while (p < end && wolframIsDigit(text[p])) p++;
  if (p == digits) return 0;
  if (p < end) {
    const char c = text[p];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '^' || c == '_' || wolframIsDigit(c)) return 0;
    if (c == '.' && p + 1 < end && wolframIsDigit(text[p + 1])) return 0;
  }
  return (uint16_t)(p - pos - 1);
}

struct WolframFlow {
  uint8_t font, color;
  int x, ascent, descent;
  uint16_t runStart;
  int runWidth;
  bool hasRun, emit;
};

inline void wolframFlowFlush(WolframBuilder& b, WolframFlow& flow, uint16_t pos) {
  if (flow.hasRun && flow.emit) {
    wolframAddText(b, flow.font, flow.color, flow.x - flow.runWidth, 0, flow.runStart, (uint16_t)(pos - flow.runStart));
  }
  flow.hasRun = false;
  flow.runWidth = 0;
}

inline bool wolframFlowNewRow(WolframBuilder& b, WolframFlow& flow) {
  wolframCloseRow(b, flow.x, flow.ascent, flow.descent);
  flow.x = 0;
  flow.ascent = wolframFontAscent(flow.font);
  flow.descent = wolframFontDescent(flow.font);
  return wolframOpenRow(b, false);
}

// Width of one word of running text, or draws it. A word wider than a whole row wraps at any character:
// a long number goes on in the next row, every digit there.
inline int wolframFlowWord(WolframBuilder& b, WolframFlow& flow, uint16_t start, uint16_t end, bool emit) {
  const char* s = b.text;
  flow.emit = emit;
  flow.hasRun = false;
  flow.runWidth = 0;
  int measured = 0;
  uint16_t i = start;
  bool afterValue = false;
  while (i < end) {
    const uint16_t exponent = wolframPlainExponent(s, i, end, afterValue);
    if (exponent > 0) {
      wolframFlowFlush(b, flow, i);
      const uint8_t small = WOLFRAM_FONT_SMALL;
      const int w = b.measure(small, s + i + 1, exponent);
      measured += w;
      if (emit) {
        if (flow.x + w > WOLFRAM_ROW_WIDTH && flow.x > 0) {
          if (!wolframFlowNewRow(b, flow)) return measured;
        }
        wolframAddText(b, small, flow.color, flow.x, -wolframFontXHeight(flow.font) + 2, (uint16_t)(i + 1), exponent);
        flow.x += w;
        if (flow.ascent < wolframFontXHeight(flow.font) + 6) flow.ascent = wolframFontXHeight(flow.font) + 6;
      }
      i = (uint16_t)(i + 1 + exponent);
      afterValue = true;
      continue;
    }
    uint32_t cp = 0;
    int n = 0;
    if (!wolframDecodeUtf8(s + i, end - i, cp, n)) {
      cp = 0xFFFD;  // Can't happen after wolframValidBody; the box shows it if it does
      n = 1;
    }
    const WolframSymbolId symbol = wolframSymbolForChar(cp);
    const int w = symbol != WOLFRAM_SYM_NONE ? wolframSymbolWidth(symbol, flow.font) : b.measure(flow.font, s + i, n);
    measured += w;
    if (emit) {
      if (flow.x + w > WOLFRAM_ROW_WIDTH && flow.x > 0) {
        wolframFlowFlush(b, flow, i);
        if (!wolframFlowNewRow(b, flow)) return measured;
      }
      if (symbol != WOLFRAM_SYM_NONE) {
        wolframFlowFlush(b, flow, i);
        wolframDrawSymbol(b, symbol, flow.x, 0, flow.font, flow.color);
        flow.x += w;
      } else {
        if (!flow.hasRun) {
          flow.hasRun = true;
          flow.runStart = i;
          flow.runWidth = 0;
        }
        flow.x += w;
        flow.runWidth += w;
      }
    }
    afterValue = (cp >= '0' && cp <= '9') || (cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z') || cp == ')' ||
                 (cp >= 0x391 && cp <= 0x3C9);
    i = (uint16_t)(i + n);
  }
  wolframFlowFlush(b, flow, end);
  return measured;
}

// Next word of running text: from start, up to the next space. A multi-byte space counts.
inline uint16_t wolframWordEnd(const char* s, uint16_t start, uint16_t end) {
  uint16_t i = start;
  while (i < end) {
    uint32_t cp = 0;
    int n = 0;
    if (!wolframDecodeUtf8(s + i, end - i, cp, n)) return (uint16_t)(i + 1);
    if (wolframIsSpaceLike(cp)) return i;
    i = (uint16_t)(i + n);
  }
  return end;
}

inline uint16_t wolframSkipSpaces(const char* s, uint16_t start, uint16_t end) {
  uint16_t i = start;
  while (i < end) {
    uint32_t cp = 0;
    int n = 0;
    if (!wolframDecodeUtf8(s + i, end - i, cp, n) || !wolframIsSpaceLike(cp)) return i;
    i = (uint16_t)(i + n);
  }
  return i;
}

// A line as plain running text, wrapped: what Wolfram wrote, with drawn symbols and raised exponents
inline void wolframAddPlain(WolframBuilder& b, WolframSpan line, uint8_t font, uint8_t color) {
  const char* s = b.text;
  WolframFlow flow;
  memset(&flow, 0, sizeof(flow));
  flow.font = font;
  flow.color = color;
  flow.ascent = wolframFontAscent(font);
  flow.descent = wolframFontDescent(font);
  if (!wolframOpenRow(b, false)) return;
  const int spaceWidth = b.measure(font, " ", 1);
  const uint16_t end = (uint16_t)(line.start + line.len);
  uint16_t i = wolframSkipSpaces(s, line.start, end);
  bool first = true;
  while (i < end && !b.overflow) {
    const uint16_t wordEnd = wolframWordEnd(s, i, end);
    const int width = wolframFlowWord(b, flow, i, wordEnd, false);
    if (!first) {
      if (flow.x + spaceWidth + width > WOLFRAM_ROW_WIDTH && width <= WOLFRAM_ROW_WIDTH) {
        if (!wolframFlowNewRow(b, flow)) return;
      } else {
        flow.x += spaceWidth;
      }
    }
    wolframFlowWord(b, flow, i, wordEnd, true);
    first = false;
    i = wolframSkipSpaces(s, wordEnd, end);
  }
  wolframCloseRow(b, flow.x, flow.ascent, flow.descent);
}

// ---------------------------------------------------------------------------------------------------------
// Sections and pages

inline bool wolframIsAscii(const char* s, WolframSpan span) {
  for (uint16_t i = 0; i < span.len; i++) {
    if ((uint8_t)s[span.start + i] >= 0x80) return false;
  }
  return true;
}

// Moves a row's pieces to their place on the screen
inline void wolframPlaceRow(WolframLayout& L, const WolframRow& row, int dx, int dy) {
  for (uint16_t i = row.first; i < row.end; i++) {
    WolframOp& op = L.ops[i];
    op.x = (int16_t)(op.x + dx);
    op.y = (int16_t)(op.y + dy);
    if (op.type == WOLFRAM_OP_LINE) {
      op.a = (uint16_t)(int16_t)((int16_t)op.a + dx);
      op.b = (uint16_t)(int16_t)((int16_t)op.b + dy);
    }
  }
}

inline uint8_t wolframNoteFont(const char* text, WolframSpan span) {
  return wolframIsAscii(text, span) ? WOLFRAM_FONT_SMALL : WOLFRAM_FONT_10;
}

// "(assuming inner radius b cm)". Other notes, like "(Taylor series)", belong with the line above them.
inline bool wolframLineIsPinned(const char* text, const WolframLine& line) {
  return line.note && wolframSpanStarts(text, line.span, "(assuming");
}

inline int wolframRowHeight(const WolframRow& row) {
  return row.ascent + row.descent;
}

// All rows of one section in one base font. False: this font doesn't suit, nothing to keep.
inline bool wolframBuildSectionRows(WolframBuilder& b, const WolframDoc& doc, const WolframSection& section,
                                    bool withAssumptions, bool pinned, uint8_t baseFont, int contentHeight) {
  const bool largeTry = baseFont == WOLFRAM_FONT_14;
  b.baseFont = baseFont;
  bool firstIsPlain = false;
  for (int i = 0; i < section.lineCount; i++) {
    const WolframLine& line = doc.lines[section.firstLine + i];
    if (pinned && wolframLineIsPinned(doc.text, line)) continue;
    if (line.note) {
      wolframAddPlain(b, line.span, wolframNoteFont(doc.text, line.span), WOLFRAM_COLOR_DIM);
    } else {
      const int before = b.rowCount;
      const bool formula = wolframAddFormula(b, line.span);
      if (largeTry && (!formula || b.rowCount - before > 1)) return false;
      if (!formula) {
        wolframAddPlain(b, line.span, WOLFRAM_FONT_10, WOLFRAM_COLOR_MAIN);
        if (before == 0) firstIsPlain = true;
      }
    }
    if (b.overflow) return false;
  }
  if (withAssumptions && !pinned) {
    for (int i = 0; i < doc.assumptionCount; i++) {
      wolframAddPlain(b, doc.assumptions[i], wolframNoteFont(doc.text, doc.assumptions[i]), WOLFRAM_COLOR_DIM);
      if (b.overflow) return false;
    }
  }
  // A table with a formula between its rows reads better all on one side
  if (firstIsPlain) {
    for (int i = 0; i < b.rowCount; i++) b.scratch->rows[i].center = false;
  }
  if (largeTry) {
    int total = 0;
    for (int i = 0; i < b.rowCount; i++) total += wolframRowHeight(b.scratch->rows[i]) + WOLFRAM_ROW_GAP;
    if (total - WOLFRAM_ROW_GAP > contentHeight) return false;
  }
  return true;
}

inline bool wolframAddPage(WolframLayout& L, const WolframPage& page) {
  if (L.pageCount >= WOLFRAM_MAX_PAGES) return false;
  L.pages[L.pageCount++] = page;
  return true;
}

// One section onto as many pages as it needs. False: no room, and nothing of it stays.
inline bool wolframLayoutSection(WolframBuilder& b, const WolframDoc& doc, uint8_t sectionIndex, bool plotReady) {
  const WolframSection& section = doc.sections[sectionIndex];
  const bool withAssumptions = sectionIndex == 0;
  WolframLayout& L = *b.layout;
  const uint16_t opStart = L.opCount;
  const uint8_t pageStart = L.pageCount;
  b.overflow = false;

  const auto giveUp = [&]() {
    L.opCount = opStart;
    L.pageCount = pageStart;
    return false;
  };

  // Title, the same on every page of the section
  b.rowCount = 0;
  wolframAddPlain(b, section.title, WOLFRAM_FONT_10, WOLFRAM_COLOR_TITLE);
  if (b.overflow) return giveUp();
  int titleBottom = 1;
  for (int i = 0; i < b.rowCount; i++) {
    const WolframRow& row = b.scratch->rows[i];
    wolframPlaceRow(L, row, WOLFRAM_MARGIN, titleBottom + row.ascent);
    titleBottom += wolframRowHeight(row);
  }
  const uint16_t titleEnd = L.opCount;
  const int contentTop = titleBottom + 2;

  // What Wolfram assumed stays in view with every page of what it's about, unless that would crowd it out
  b.rowCount = 0;
  for (int i = 0; i < section.lineCount; i++) {
    const WolframLine& line = doc.lines[section.firstLine + i];
    if (wolframLineIsPinned(doc.text, line)) {
      wolframAddPlain(b, line.span, wolframNoteFont(doc.text, line.span), WOLFRAM_COLOR_DIM);
    }
  }
  if (withAssumptions) {
    for (int i = 0; i < doc.assumptionCount; i++) {
      wolframAddPlain(b, doc.assumptions[i], wolframNoteFont(doc.text, doc.assumptions[i]), WOLFRAM_COLOR_DIM);
    }
  }
  if (b.overflow) return giveUp();
  int pinnedHeight = 0;
  for (int i = 0; i < b.rowCount; i++) pinnedHeight += wolframRowHeight(b.scratch->rows[i]);
  const bool pinned = b.rowCount > 0 && pinnedHeight <= (WOLFRAM_CONTENT_BOTTOM - contentTop) / 2;
  if (pinned) {
    int y = WOLFRAM_CONTENT_BOTTOM - pinnedHeight;
    for (int i = 0; i < b.rowCount; i++) {
      const WolframRow& row = b.scratch->rows[i];
      wolframPlaceRow(L, row, WOLFRAM_MARGIN, y + row.ascent);
      y += wolframRowHeight(row);
    }
  } else {
    L.opCount = titleEnd;
    pinnedHeight = 0;
  }
  const uint16_t noteEnd = L.opCount;
  const int contentHeight = WOLFRAM_CONTENT_BOTTOM - contentTop - pinnedHeight;

  WolframPage page;
  page.titleFirst = opStart;
  page.titleCount = (uint16_t)(titleEnd - opStart);
  page.noteFirst = titleEnd;
  page.noteCount = (uint16_t)(noteEnd - titleEnd);
  page.section = sectionIndex;
  page.first = page.count = 0;
  page.plot = false;

  if (section.lineCount > 0) {
    // Large if it all fits large, without wrapping; otherwise the normal size
    b.rowCount = 0;
    b.overflow = false;
    if (!wolframBuildSectionRows(b, doc, section, withAssumptions, pinned, WOLFRAM_FONT_14, contentHeight)) {
      L.opCount = noteEnd;
      b.rowCount = 0;
      b.overflow = false;
      if (!wolframBuildSectionRows(b, doc, section, withAssumptions, pinned, WOLFRAM_FONT_10, contentHeight)) {
        return giveUp();
      }
    }
    // Rows onto pages, each page centred in the height it has
    int first = 0;
    while (first < b.rowCount) {
      int used = 0, last = first;
      while (last < b.rowCount) {
        const int height = wolframRowHeight(b.scratch->rows[last]) + (last > first ? WOLFRAM_ROW_GAP : 0);
        if (used + height > contentHeight && last > first) break;
        used += height;
        last++;
      }
      int y = contentTop + (used < contentHeight ? (contentHeight - used) / 2 : 0);
      for (int i = first; i < last; i++) {
        const WolframRow& row = b.scratch->rows[i];
        const int x = row.center && row.width < WOLFRAM_ROW_WIDTH ? (WOLFRAM_ROW_WIDTH - row.width) / 2 : 0;
        wolframPlaceRow(L, row, WOLFRAM_MARGIN + x, y + row.ascent);
        y += wolframRowHeight(row) + WOLFRAM_ROW_GAP;
      }
      page.first = b.scratch->rows[first].first;
      page.count = (uint16_t)(b.scratch->rows[last - 1].end - page.first);
      if (!wolframAddPage(L, page)) return giveUp();
      first = last;
    }
  }
  if (section.isPlot && plotReady) {
    page.first = page.count = 0;
    page.plot = true;
    if (!wolframAddPage(L, page)) return giveUp();
  }
  return true;
}

// The whole answer. False only if not even the first section fits. Later sections that don't are left out whole.
inline bool wolframLayoutBuild(const WolframDoc& doc, bool plotReady, WolframMeasure measure, WolframLayout& layout,
                               WolframScratch& scratch) {
  layout.opCount = 0;
  layout.pageCount = 0;
  WolframBuilder b;
  b.layout = &layout;
  b.scratch = &scratch;
  b.text = doc.text;
  b.measure = measure;
  b.rowCount = 0;
  b.baseFont = WOLFRAM_FONT_10;
  b.overflow = false;
  for (int i = 0; i < doc.sectionCount; i++) {
    if (!wolframLayoutSection(b, doc, (uint8_t)i, plotReady)) {
      if (i == 0) return false;
      break;
    }
  }
  return layout.pageCount > 0;
}
