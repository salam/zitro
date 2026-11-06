// Reads one line of Wolfram's plain text as a formula. Plain C++, tested on the host: tests/wolfram/run.sh
//
// Only / ^ _ and function calls bind anything here; everything else stays a flat row in the order Wolfram
// wrote it, so typesetting can't change what a line means. Wolfram writes a product with a space and
// reads left to right: "1/4 (b - 5)^2" is a quarter times (b - 5) squared, "1/(2 x)" has the x below the bar.
//
// A line this can't read fully isn't a formula, and the caller shows it as plain text. Writing the tree back
// must give the source again, character for character apart from spaces: wolframFormulaRoundTrips().
#pragma once

#include "wolfram_core.h"

enum WolframNodeType : uint8_t {
  WOLFRAM_NODE_ROW,     // a: first child, children linked by next
  WOLFRAM_NODE_TEXT,    // A number, letter or word: start and len in the source, shown exactly so
  WOLFRAM_NODE_SYMBOL,  // Infinity, drawn as lines
  WOLFRAM_NODE_OP,      // + - = and so on
  WOLFRAM_NODE_GROUP,   // a: the row inside; start: "(", closeAt: ")"
  WOLFRAM_NODE_FRAC,    // a over b; start: the "/"
  WOLFRAM_NODE_SCRIPT,  // a: base, b: below, c: above
  WOLFRAM_NODE_ROOT,    // a: hidden group; start, len: "sqrt" or "cuberoot"
  WOLFRAM_NODE_BIGOP,   // integral, sum, product, lim; start, len: the name; b: below, c: above
  WOLFRAM_NODE_GLOSS,   // "(cubic centimeters)": words after a value, kept whole
};

enum WolframSymbolId : uint8_t {
  WOLFRAM_SYM_NONE,
  WOLFRAM_SYM_APPROX,
  WOLFRAM_SYM_NOT_EQUAL,
  WOLFRAM_SYM_LESS_EQUAL,
  WOLFRAM_SYM_GREATER_EQUAL,
  WOLFRAM_SYM_INFINITY,
  WOLFRAM_SYM_ROOT,
  WOLFRAM_SYM_INTEGRAL,
  WOLFRAM_SYM_SUM,
  WOLFRAM_SYM_PRODUCT,
  WOLFRAM_SYM_ARROW,
  WOLFRAM_SYM_MINUS,
  WOLFRAM_SYM_MINUS_PLUS,
  WOLFRAM_SYM_DOTS,
  WOLFRAM_SYM_DOT,
  WOLFRAM_SYM_DASH,
  WOLFRAM_SYM_QUOTE,
  WOLFRAM_SYM_THREE,  // The 3 of a cube root
  WOLFRAM_SYM_BOX,    // A character nothing here can show: never silently left out
};

static const uint8_t WOLFRAM_FLAG_SPACE_BEFORE = 1;  // Wolfram wrote a space before this node
static const uint8_t WOLFRAM_FLAG_HIDDEN = 2;        // Group whose parentheses the shape replaces: x^(2 y), 1/(2 x)
static const uint8_t WOLFRAM_FLAG_SUP_FIRST = 4;     // Script: "^" came before "_" in the source
static const uint8_t WOLFRAM_FLAG_BINARY = 8;        // Op between two values, not a sign
static const uint8_t WOLFRAM_FLAG_RELATION = 16;     // = < > and friends
static const uint8_t WOLFRAM_FLAG_INLINE = 32;       // Fraction written a/b, in small script
static const uint8_t WOLFRAM_FLAG_TALL = 64;         // Group with parentheses drawn taller than the font's

struct WolframNode {
  WolframNodeType type;
  uint8_t flags;
  WolframSymbolId symbol;
  uint8_t level;       // Set by layout: 0 full size, above that script
  uint16_t start, len;
  uint16_t closeAt;
  int16_t a, b, c, next;
  int16_t width, ascent, descent;  // Set by layout
};

static const int WOLFRAM_MAX_NODES = 120;
static const int WOLFRAM_MAX_DEPTH = 4;  // Real answers need 2 or 3, and this keeps the stack small on the stick
static const size_t WOLFRAM_FORMULA_LINE_LIMIT = 220;
static const int WOLFRAM_MAX_LONG_WORDS = 1;  // Two real words means a sentence, not a formula

struct WolframFormula {
  WolframNode nodes[WOLFRAM_MAX_NODES];
  int16_t count;
  int16_t root;
  const char* text;
  uint16_t pos, end;
  int8_t depth;
  int8_t longWords;
};

// ---------------------------------------------------------------------------------------------------------
// Tokens

enum WolframTokenKind : uint8_t {
  WOLFRAM_TOKEN_END,
  WOLFRAM_TOKEN_NUMBER,
  WOLFRAM_TOKEN_WORD,
  WOLFRAM_TOKEN_ADD,       // + - plus-minus
  WOLFRAM_TOKEN_TIMES,     // * and the times and dot signs
  WOLFRAM_TOKEN_RELATION,  // = < > ...
  WOLFRAM_TOKEN_SLASH,
  WOLFRAM_TOKEN_CARET,
  WOLFRAM_TOKEN_UNDER,
  WOLFRAM_TOKEN_OPEN,
  WOLFRAM_TOKEN_CLOSE,
  WOLFRAM_TOKEN_POSTFIX,   // ! %
  WOLFRAM_TOKEN_DOTS,
  WOLFRAM_TOKEN_INFINITY,
  WOLFRAM_TOKEN_BAD,  // Anything else: this line isn't a formula
};

struct WolframToken {
  WolframTokenKind kind;
  WolframSymbolId symbol;
  bool spaceBefore;
  uint16_t start, len;
};

inline bool wolframIsDigit(char c) {
  return c >= '0' && c <= '9';
}

inline bool wolframIsLetter(uint32_t cp) {
  return (cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z') || (cp >= 0x391 && cp <= 0x3C9) || cp == 0xB5 ||
         cp == 0xB0;  // Greek, micro, degree
}

inline WolframToken wolframNextToken(const WolframFormula& f, uint16_t pos) {
  const char* s = f.text;
  WolframToken t = {WOLFRAM_TOKEN_END, WOLFRAM_SYM_NONE, false, pos, 0};
  while (pos < f.end && s[pos] == ' ') {
    pos++;
    t.spaceBefore = true;
  }
  t.start = pos;
  if (pos >= f.end) return t;
  const char c = s[pos];

  if (wolframIsDigit(c) || (c == '.' && pos + 1 < f.end && wolframIsDigit(s[pos + 1]))) {
    uint16_t p = pos;
    bool point = false;
    while (p < f.end && (wolframIsDigit(s[p]) || (s[p] == '.' && !point && p + 1 < f.end && wolframIsDigit(s[p + 1])))) {
      if (s[p] == '.') point = true;
      p++;
    }
    if (p + 2 < f.end && s[p] == '.' && s[p + 1] == '.' && s[p + 2] == '.') {
      p += 3;  // Wolfram's "1.41421356..." says the digits go on: part of the number
    } else if (p < f.end && s[p] == '.') {
      t.kind = WOLFRAM_TOKEN_BAD;  // "c^2." or "1.2.3": the end of a sentence or no number at all
      return t;
    }
    t.kind = WOLFRAM_TOKEN_NUMBER;
    t.len = (uint16_t)(p - pos);
    return t;
  }

  uint32_t cp = 0;
  int n = 0;
  if (!wolframDecodeUtf8(s + pos, f.end - pos, cp, n)) {
    t.kind = WOLFRAM_TOKEN_BAD;
    return t;
  }
  t.len = (uint16_t)n;

  if (wolframIsLetter(cp)) {
    uint16_t p = pos;
    while (p < f.end) {
      uint32_t next = 0;
      int m = 0;
      if (!wolframDecodeUtf8(s + p, f.end - p, next, m)) break;
      const bool digit = next >= '0' && next <= '9';
      if (!wolframIsLetter(next) && !digit && next != '\'') break;
      p += m;
    }
    t.kind = WOLFRAM_TOKEN_WORD;
    t.len = (uint16_t)(p - pos);
    return t;
  }

  const char following = pos + 1 < f.end ? s[pos + 1] : 0;
  switch (cp) {
    case '+':
    case 0xB1:  // plus-minus
      t.kind = WOLFRAM_TOKEN_ADD;
      return t;
    case 0x2213:
      t.kind = WOLFRAM_TOKEN_ADD;
      t.symbol = WOLFRAM_SYM_MINUS_PLUS;
      return t;
    case 0x2212:
      t.kind = WOLFRAM_TOKEN_ADD;
      t.symbol = WOLFRAM_SYM_MINUS;
      return t;
    case '-':
      if (following == '>') {
        t.kind = WOLFRAM_TOKEN_RELATION;
        t.symbol = WOLFRAM_SYM_ARROW;
        t.len = 2;
      } else {
        t.kind = WOLFRAM_TOKEN_ADD;
      }
      return t;
    case '*':
    case 0xD7:  // times
    case 0xB7:  // dot
      t.kind = WOLFRAM_TOKEN_TIMES;
      return t;
    case 0x22C5:
      t.kind = WOLFRAM_TOKEN_TIMES;
      t.symbol = WOLFRAM_SYM_DOT;
      return t;
    case '=':
      t.kind = WOLFRAM_TOKEN_RELATION;
      return t;
    case '<':
      t.kind = WOLFRAM_TOKEN_RELATION;
      if (following == '=') {
        t.symbol = WOLFRAM_SYM_LESS_EQUAL;
        t.len = 2;
      }
      return t;
    case '>':
      t.kind = WOLFRAM_TOKEN_RELATION;
      if (following == '=') {
        t.symbol = WOLFRAM_SYM_GREATER_EQUAL;
        t.len = 2;
      }
      return t;
    case '!':
      if (following == '=') {
        t.kind = WOLFRAM_TOKEN_RELATION;
        t.symbol = WOLFRAM_SYM_NOT_EQUAL;
        t.len = 2;
      } else {
        t.kind = WOLFRAM_TOKEN_POSTFIX;
      }
      return t;
    case '%':
      t.kind = WOLFRAM_TOKEN_POSTFIX;
      return t;
    case 0x2248:
      t.kind = WOLFRAM_TOKEN_RELATION;
      t.symbol = WOLFRAM_SYM_APPROX;
      return t;
    case 0x2260:
      t.kind = WOLFRAM_TOKEN_RELATION;
      t.symbol = WOLFRAM_SYM_NOT_EQUAL;
      return t;
    case 0x2264:
      t.kind = WOLFRAM_TOKEN_RELATION;
      t.symbol = WOLFRAM_SYM_LESS_EQUAL;
      return t;
    case 0x2265:
      t.kind = WOLFRAM_TOKEN_RELATION;
      t.symbol = WOLFRAM_SYM_GREATER_EQUAL;
      return t;
    case 0x2192:
      t.kind = WOLFRAM_TOKEN_RELATION;
      t.symbol = WOLFRAM_SYM_ARROW;
      return t;
    case 0x221E:
      t.kind = WOLFRAM_TOKEN_INFINITY;
      t.symbol = WOLFRAM_SYM_INFINITY;
      return t;
    case '/':
      t.kind = WOLFRAM_TOKEN_SLASH;
      return t;
    case '^':
      t.kind = WOLFRAM_TOKEN_CARET;
      return t;
    case '_':
      t.kind = WOLFRAM_TOKEN_UNDER;
      return t;
    case '(':
      t.kind = WOLFRAM_TOKEN_OPEN;
      return t;
    case ')':
      t.kind = WOLFRAM_TOKEN_CLOSE;
      return t;
    case '.':
      if (following == '.' && pos + 2 < f.end && s[pos + 2] == '.') {
        t.kind = WOLFRAM_TOKEN_DOTS;
        t.len = 3;
        return t;
      }
      break;
    default:
      break;
  }
  t.kind = WOLFRAM_TOKEN_BAD;
  return t;
}

inline WolframToken wolframPeek(const WolframFormula& f) {
  return wolframNextToken(f, f.pos);
}

inline void wolframConsume(WolframFormula& f, const WolframToken& t) {
  f.pos = (uint16_t)(t.start + t.len);
}

// ---------------------------------------------------------------------------------------------------------
// Tree

inline int16_t wolframNewNode(WolframFormula& f, WolframNodeType type, uint16_t start, uint16_t len) {
  if (f.count >= WOLFRAM_MAX_NODES) return -1;
  const int16_t i = f.count++;
  WolframNode& n = f.nodes[i];
  memset(&n, 0, sizeof(n));
  n.type = type;
  n.start = start;
  n.len = len;
  n.a = n.b = n.c = n.next = -1;
  return i;
}

// Appends child to a row
inline void wolframRowAdd(WolframFormula& f, int16_t row, int16_t child, int16_t& last) {
  if (last < 0) {
    f.nodes[row].a = child;
  } else {
    f.nodes[last].next = child;
  }
  last = child;
}

inline bool wolframIsFunctionName(const char* s, uint16_t len) {
  static const char* const names[] = {"sin",    "cos",    "tan",    "cot",     "sec",     "csc",    "sinh",
                                      "cosh",   "tanh",   "arcsin", "arccos",  "arctan",  "log",    "ln",
                                      "exp",    "abs",    "sqrt",   "cuberoot", "gamma",  "erf"};
  for (const char* name : names) {
    if (strlen(name) == len && memcmp(s, name, len) == 0) return true;
  }
  return false;
}

inline bool wolframWordIs(const char* s, const WolframToken& t, const char* word) {
  const size_t n = strlen(word);
  return t.len == n && memcmp(s + t.start, word, n) == 0;
}

inline int wolframWordLetters(const char* s, uint16_t start, uint16_t len) {
  int letters = 0;
  uint16_t i = 0;
  while (i < len) {
    uint32_t cp = 0;
    int n = 0;
    if (!wolframDecodeUtf8(s + start + i, len - i, cp, n)) return letters;
    if (wolframIsLetter(cp)) letters++;
    i += n;
  }
  return letters;
}

// Does an integral have its "dx" further along this line? Otherwise "integral" is just a word.
inline bool wolframHasDifferentialAhead(const WolframFormula& f, uint16_t pos) {
  int guard = 0;
  while (guard++ < WOLFRAM_MAX_NODES) {
    const WolframToken t = wolframNextToken(f, pos);
    if (t.kind == WOLFRAM_TOKEN_END || t.kind == WOLFRAM_TOKEN_BAD) return false;
    if (t.kind == WOLFRAM_TOKEN_WORD && t.len == 2 && f.text[t.start] == 'd') return true;
    pos = (uint16_t)(t.start + t.len);
  }
  return false;
}

// "(cubic centimeters)": a parenthesis of words after a value. It can hold commas that a formula can't.
inline bool wolframIsGlossAt(const WolframFormula& f, const WolframToken& open, uint16_t& closeAt) {
  int depth = 0;
  bool word = false;
  uint16_t p = open.start;
  while (p < f.end) {
    const char c = f.text[p];
    if (c == '(') depth++;
    if (c == ')') {
      depth--;
      if (depth == 0) {
        closeAt = p;
        return word;
      }
    }
    if (c == '^' || c == '/' || c == '=' || (uint8_t)c >= 0xE0) return false;  // Math, or a symbol that needs drawing
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
      const uint16_t begin = p;
      while (p < f.end && ((f.text[p] >= 'a' && f.text[p] <= 'z') || (f.text[p] >= 'A' && f.text[p] <= 'Z'))) p++;
      if (p - begin >= 4 && !wolframIsFunctionName(f.text + begin, (uint16_t)(p - begin))) word = true;
      continue;
    }
    p++;
  }
  return false;
}

inline int16_t wolframParseRow(WolframFormula& f, bool top);

inline int16_t wolframParseGroup(WolframFormula& f, bool hidden) {
  const WolframToken open = wolframPeek(f);
  if (open.kind != WOLFRAM_TOKEN_OPEN || f.depth >= WOLFRAM_MAX_DEPTH) return -1;
  wolframConsume(f, open);
  const int16_t group = wolframNewNode(f, WOLFRAM_NODE_GROUP, open.start, 1);
  if (group < 0) return -1;
  f.depth++;
  const int16_t inner = wolframParseRow(f, false);
  f.depth--;
  if (inner < 0) return -1;
  const WolframToken close = wolframPeek(f);
  if (close.kind != WOLFRAM_TOKEN_CLOSE) return -1;
  wolframConsume(f, close);
  f.nodes[group].a = inner;
  f.nodes[group].closeAt = close.start;
  if (hidden) f.nodes[group].flags |= WOLFRAM_FLAG_HIDDEN;
  return group;
}

// A word, with its call if it has one: sin(x), f(x), O(x^6). "x (y)" with a space is a product.
inline int16_t wolframParseWord(WolframFormula& f, const WolframToken& word, bool allowCall) {
  wolframConsume(f, word);
  const int16_t text = wolframNewNode(f, WOLFRAM_NODE_TEXT, word.start, word.len);
  if (text < 0) return -1;
  const WolframToken next = wolframPeek(f);
  if (!allowCall || next.kind != WOLFRAM_TOKEN_OPEN || next.spaceBefore) return text;
  const bool known = wolframIsFunctionName(f.text + word.start, word.len);
  if (!known && wolframWordLetters(f.text, word.start, word.len) != 1) return text;
  const int16_t row = wolframNewNode(f, WOLFRAM_NODE_ROW, word.start, 0);
  const int16_t group = wolframParseGroup(f, false);
  if (row < 0 || group < 0) return -1;
  f.nodes[row].a = text;
  f.nodes[text].next = group;
  return row;
}

// What follows ^ or _: one number or word (with its sign, above the line), or a group without its parentheses
inline int16_t wolframParseScript(WolframFormula& f, bool sup) {
  WolframToken t = wolframPeek(f);
  if (t.spaceBefore) return -1;
  int16_t row = -1, sign = -1;
  if (sup && t.kind == WOLFRAM_TOKEN_ADD) {
    wolframConsume(f, t);
    row = wolframNewNode(f, WOLFRAM_NODE_ROW, t.start, 0);
    sign = wolframNewNode(f, WOLFRAM_NODE_OP, t.start, t.len);
    if (row < 0 || sign < 0) return -1;
    f.nodes[sign].symbol = t.symbol;
    t = wolframPeek(f);
    if (t.spaceBefore) return -1;
  }
  int16_t arg = -1;
  if (t.kind == WOLFRAM_TOKEN_NUMBER) {
    wolframConsume(f, t);
    arg = wolframNewNode(f, WOLFRAM_NODE_TEXT, t.start, t.len);
  } else if (t.kind == WOLFRAM_TOKEN_WORD) {
    arg = wolframParseWord(f, t, true);
  } else if (t.kind == WOLFRAM_TOKEN_INFINITY) {
    wolframConsume(f, t);
    arg = wolframNewNode(f, WOLFRAM_NODE_SYMBOL, t.start, t.len);
    if (arg >= 0) f.nodes[arg].symbol = t.symbol;
  } else if (t.kind == WOLFRAM_TOKEN_OPEN) {
    arg = wolframParseGroup(f, true);
    // H_k^(2) and f^(n) mean an order, not a power: one number or letter keeps its parentheses, or it would read as one.
    // Sub- and superscripts with more in them, like x^(2 y), lose nothing without them.
    if (sup && arg >= 0) {
      const WolframNode& inner = f.nodes[f.nodes[arg].a];
      const int16_t only = inner.a;
      if (only >= 0 && f.nodes[only].next < 0 &&
          (f.nodes[only].type == WOLFRAM_NODE_TEXT || f.nodes[only].type == WOLFRAM_NODE_SYMBOL)) {
        f.nodes[arg].flags &= (uint8_t)~WOLFRAM_FLAG_HIDDEN;
      }
    }
  }
  if (arg < 0) return -1;
  if (sign < 0) return arg;
  f.nodes[row].a = sign;
  f.nodes[sign].next = arg;
  return row;
}

// A number, word, group and so on, with a trailing ! or %
inline int16_t wolframParseAtom(WolframFormula& f) {
  const WolframToken t = wolframPeek(f);
  int16_t atom = -1;
  switch (t.kind) {
    case WOLFRAM_TOKEN_NUMBER:
    case WOLFRAM_TOKEN_DOTS:
      wolframConsume(f, t);
      atom = wolframNewNode(f, WOLFRAM_NODE_TEXT, t.start, t.len);
      break;
    case WOLFRAM_TOKEN_INFINITY:
      wolframConsume(f, t);
      atom = wolframNewNode(f, WOLFRAM_NODE_SYMBOL, t.start, t.len);
      if (atom >= 0) f.nodes[atom].symbol = t.symbol;
      break;
    case WOLFRAM_TOKEN_WORD: {
      const char* s = f.text;
      const uint16_t after = (uint16_t)(t.start + t.len);
      const WolframToken next = wolframNextToken(f, after);
      if ((wolframWordIs(s, t, "sqrt") || wolframWordIs(s, t, "cuberoot")) && next.kind == WOLFRAM_TOKEN_OPEN &&
          !next.spaceBefore) {
        wolframConsume(f, t);
        const int16_t root = wolframNewNode(f, WOLFRAM_NODE_ROOT, t.start, t.len);
        const int16_t group = wolframParseGroup(f, true);
        if (root < 0 || group < 0) return -1;
        f.nodes[root].a = group;
        atom = root;
        break;
      }
      const bool isIntegral = wolframWordIs(s, t, "integral") && wolframHasDifferentialAhead(f, after);
      const bool isLimited = (wolframWordIs(s, t, "sum") || wolframWordIs(s, t, "product") || wolframWordIs(s, t, "lim")) &&
                             next.kind == WOLFRAM_TOKEN_UNDER;
      if (isIntegral || isLimited) {
        wolframConsume(f, t);
        return wolframNewNode(f, WOLFRAM_NODE_BIGOP, t.start, t.len);  // The caller reads its limits
      }
      const int letters = wolframWordLetters(s, t.start, t.len);
      if (letters >= 4 && !wolframIsFunctionName(s + t.start, t.len) && f.depth == 0) {
        if (++f.longWords > WOLFRAM_MAX_LONG_WORDS) return -1;
      }
      atom = wolframParseWord(f, t, true);
      break;
    }
    case WOLFRAM_TOKEN_OPEN: {
      uint16_t closeAt = 0;
      if (f.depth == 0 && wolframIsGlossAt(f, t, closeAt)) {
        f.pos = (uint16_t)(closeAt + 1);
        return wolframNewNode(f, WOLFRAM_NODE_GLOSS, t.start, (uint16_t)(closeAt + 1 - t.start));
      }
      atom = wolframParseGroup(f, false);
      break;
    }
    default:
      return -1;
  }
  if (atom < 0) return -1;
  const WolframToken post = wolframPeek(f);
  if (post.kind == WOLFRAM_TOKEN_POSTFIX && !post.spaceBefore) {
    wolframConsume(f, post);
    const int16_t row = wolframNewNode(f, WOLFRAM_NODE_ROW, t.start, 0);
    const int16_t mark = wolframNewNode(f, WOLFRAM_NODE_TEXT, post.start, post.len);
    if (row < 0 || mark < 0) return -1;
    f.nodes[row].a = atom;
    f.nodes[atom].next = mark;
    return row;
  }
  return atom;
}

inline bool wolframIsAtomStart(const WolframToken& t) {
  return t.kind == WOLFRAM_TOKEN_NUMBER || t.kind == WOLFRAM_TOKEN_WORD || t.kind == WOLFRAM_TOKEN_OPEN ||
         t.kind == WOLFRAM_TOKEN_DOTS || t.kind == WOLFRAM_TOKEN_INFINITY;
}

// Atom with what's written below and above it
inline int16_t wolframParsePower(WolframFormula& f) {
  const WolframToken first = wolframPeek(f);
  const int16_t base = wolframParseAtom(f);
  if (base < 0) return -1;
  const bool bigOp = f.nodes[base].type == WOLFRAM_NODE_BIGOP;
  if (f.nodes[base].type == WOLFRAM_NODE_GLOSS) return base;

  int16_t below = -1, above = -1;
  bool supFirst = false, sawScript = false;
  for (int i = 0; i < 2; i++) {
    const WolframToken t = wolframPeek(f);
    if (t.spaceBefore) break;
    if (t.kind == WOLFRAM_TOKEN_CARET && above < 0) {
      wolframConsume(f, t);
      above = wolframParseScript(f, true);
      if (above < 0) return -1;
      if (below < 0) supFirst = true;
    } else if (t.kind == WOLFRAM_TOKEN_UNDER && below < 0) {
      wolframConsume(f, t);
      below = wolframParseScript(f, false);
      if (below < 0) return -1;
    } else {
      break;
    }
    sawScript = true;
  }
  const WolframToken next = wolframPeek(f);
  if (next.kind == WOLFRAM_TOKEN_CARET || next.kind == WOLFRAM_TOKEN_UNDER) return -1;  // x^y^z, ^_ and so on

  if (bigOp) {
    f.nodes[base].b = below;
    f.nodes[base].c = above;
    if (supFirst) f.nodes[base].flags |= WOLFRAM_FLAG_SUP_FIRST;
    return base;
  }
  if (!sawScript) return base;

  const int16_t script = wolframNewNode(f, WOLFRAM_NODE_SCRIPT, first.start, 0);
  if (script < 0) return -1;
  f.nodes[script].a = base;
  f.nodes[script].b = below;
  f.nodes[script].c = above;
  if (supFirst) f.nodes[script].flags |= WOLFRAM_FLAG_SUP_FIRST;

  // sin^2(x): the power belongs to the function and the argument stays with it
  if (next.kind == WOLFRAM_TOKEN_OPEN && !next.spaceBefore && f.nodes[base].type == WOLFRAM_NODE_TEXT &&
      wolframIsFunctionName(f.text + f.nodes[base].start, f.nodes[base].len)) {
    const int16_t row = wolframNewNode(f, WOLFRAM_NODE_ROW, first.start, 0);
    const int16_t group = wolframParseGroup(f, false);
    if (row < 0 || group < 0) return -1;
    f.nodes[row].a = script;
    f.nodes[script].next = group;
    return row;
  }
  // "x^2y" could mean two things. Wolfram writes "x^2 y", so this isn't Wolfram's and isn't a formula.
  const WolframToken after = wolframPeek(f);
  const int16_t top = above >= 0 ? above : below;
  if (!after.spaceBefore && wolframIsAtomStart(after) && f.nodes[top].type != WOLFRAM_NODE_GROUP) return -1;
  return script;
}

// Is this fit to stand above or below a fraction bar? Not a word: "and/or" is no fraction, "m/s" and "dx" are fine.
inline bool wolframFractionOperandOk(const WolframFormula& f, int16_t node) {
  const WolframNode& n = f.nodes[node];
  switch (n.type) {
    case WOLFRAM_NODE_BIGOP:
    case WOLFRAM_NODE_GLOSS:
    case WOLFRAM_NODE_OP:
      return false;
    case WOLFRAM_NODE_TEXT:
      return wolframWordLetters(f.text, n.start, n.len) <= 2;
    case WOLFRAM_NODE_SCRIPT:
      return wolframFractionOperandOk(f, n.a);
    default:
      return true;
  }
}

// A power, or two with a bar between them. Nothing else binds this tightly.
inline int16_t wolframParseFactor(WolframFormula& f) {
  const WolframToken before = wolframPeek(f);
  const int16_t left = wolframParsePower(f);
  if (left < 0) return -1;
  const WolframToken slash = wolframPeek(f);
  if (slash.kind != WOLFRAM_TOKEN_SLASH) return left;
  if (!wolframFractionOperandOk(f, left)) return -1;
  wolframConsume(f, slash);

  // d/dx(f): the operator is the fraction, and what it acts on stays outside. "dx" takes no call, see below.
  const bool isOperator = before.kind == WOLFRAM_TOKEN_WORD && before.len == 1 && f.text[before.start] == 'd';
  // "1/x(x + 1)": is x(x + 1) all below the bar? Wolfram writes 1/(x (x + 1)), so this isn't Wolfram's.
  const WolframToken t = wolframPeek(f);
  if (t.kind == WOLFRAM_TOKEN_WORD && !wolframIsFunctionName(f.text + t.start, t.len) &&
      wolframWordLetters(f.text, t.start, t.len) == 1) {
    const WolframToken call = wolframNextToken(f, (uint16_t)(t.start + t.len));
    if (call.kind == WOLFRAM_TOKEN_OPEN && !call.spaceBefore) return -1;
  }
  const int16_t right = wolframParsePower(f);
  if (right < 0 || !wolframFractionOperandOk(f, right)) return -1;

  const int16_t frac = wolframNewNode(f, WOLFRAM_NODE_FRAC, slash.start, slash.len);
  if (frac < 0) return -1;
  f.nodes[frac].a = left;
  f.nodes[frac].b = right;
  if (f.nodes[left].type == WOLFRAM_NODE_GROUP) f.nodes[left].flags |= WOLFRAM_FLAG_HIDDEN;
  if (f.nodes[right].type == WOLFRAM_NODE_GROUP) f.nodes[right].flags |= WOLFRAM_FLAG_HIDDEN;

  const WolframToken after = wolframPeek(f);
  if (after.kind == WOLFRAM_TOKEN_SLASH) return -1;  // a/b/c is written differently in different places
  // "1/2x" could mean x on either side of the bar
  if (!after.spaceBefore && wolframIsAtomStart(after) && f.nodes[right].type != WOLFRAM_NODE_GROUP && !isOperator) {
    return -1;
  }
  return frac;
}

// A flat row: values and operators in the order they were written. Top: the whole line.
inline int16_t wolframParseRow(WolframFormula& f, bool top) {
  const int16_t row = wolframNewNode(f, WOLFRAM_NODE_ROW, f.pos, 0);
  if (row < 0) return -1;
  int16_t last = -1;
  bool afterValue = false, afterOp = false, empty = true;
  while (true) {
    const WolframToken t = wolframPeek(f);
    if (t.kind == WOLFRAM_TOKEN_END || t.kind == WOLFRAM_TOKEN_CLOSE) break;
    const bool isRelation = t.kind == WOLFRAM_TOKEN_RELATION;
    const bool isAdd = t.kind == WOLFRAM_TOKEN_ADD;
    const bool isTimes = t.kind == WOLFRAM_TOKEN_TIMES;
    if (isRelation || isAdd || isTimes) {
      // A sign after a relation or at the start is fine; "x = = 2" or "x * " isn't anything
      const bool sign = isAdd && !afterValue;
      const bool leadingRelation = isRelation && top && empty;
      if (!afterValue && !sign && !leadingRelation) return -1;
      wolframConsume(f, t);
      const int16_t op = wolframNewNode(f, WOLFRAM_NODE_OP, t.start, t.len);
      if (op < 0) return -1;
      f.nodes[op].symbol = t.symbol;
      if (afterValue) f.nodes[op].flags |= WOLFRAM_FLAG_BINARY;
      if (isRelation) f.nodes[op].flags |= WOLFRAM_FLAG_RELATION | WOLFRAM_FLAG_BINARY;
      wolframRowAdd(f, row, op, last);
      afterValue = false;
      afterOp = true;
      empty = false;
      continue;
    }
    if (!wolframIsAtomStart(t)) return -1;
    const int16_t value = wolframParseFactor(f);
    if (value < 0) return -1;
    if (t.spaceBefore && afterValue) f.nodes[value].flags |= WOLFRAM_FLAG_SPACE_BEFORE;
    wolframRowAdd(f, row, value, last);
    afterValue = true;
    afterOp = false;
    empty = false;
  }
  if (empty || afterOp) return -1;
  return row;
}

// Reads the span as a formula. False: it isn't one, show it as text.
inline bool wolframFormulaParse(const char* text, WolframSpan line, WolframFormula& f) {
  f.count = 0;
  f.root = -1;
  f.text = text;
  f.pos = line.start;
  f.end = (uint16_t)(line.start + line.len);
  f.depth = 0;
  f.longWords = 0;
  if (line.len == 0 || line.len > WOLFRAM_FORMULA_LINE_LIMIT) return false;
  const int16_t root = wolframParseRow(f, true);
  if (root < 0) return false;
  if (wolframPeek(f).kind != WOLFRAM_TOKEN_END) return false;  // An extra ")" stops the row early
  f.root = root;
  return true;
}

// ---------------------------------------------------------------------------------------------------------
// Writing the tree back: the check that what will be shown is what was sent

struct WolframWriter {
  char* out;
  size_t capacity, length;
  bool overflow;
};

inline void wolframWrite(WolframWriter& w, const char* s, size_t n) {
  for (size_t i = 0; i < n; i++) {
    if (s[i] == ' ') continue;
    if (w.length + 1 >= w.capacity) {
      w.overflow = true;
      return;
    }
    w.out[w.length++] = s[i];
  }
}

inline void wolframWriteNode(const WolframFormula& f, int16_t i, WolframWriter& w) {
  if (i < 0) return;
  const WolframNode& n = f.nodes[i];
  switch (n.type) {
    case WOLFRAM_NODE_ROW:
      for (int16_t child = n.a; child >= 0; child = f.nodes[child].next) wolframWriteNode(f, child, w);
      break;
    case WOLFRAM_NODE_TEXT:
    case WOLFRAM_NODE_SYMBOL:
    case WOLFRAM_NODE_OP:
    case WOLFRAM_NODE_GLOSS:
      wolframWrite(w, f.text + n.start, n.len);
      break;
    case WOLFRAM_NODE_GROUP:
      wolframWrite(w, "(", 1);
      wolframWriteNode(f, n.a, w);
      wolframWrite(w, ")", 1);
      break;
    case WOLFRAM_NODE_FRAC:
      wolframWriteNode(f, n.a, w);
      wolframWrite(w, "/", 1);
      wolframWriteNode(f, n.b, w);
      break;
    case WOLFRAM_NODE_ROOT:
      wolframWrite(w, f.text + n.start, n.len);
      wolframWriteNode(f, n.a, w);
      break;
    case WOLFRAM_NODE_SCRIPT:
    case WOLFRAM_NODE_BIGOP:
      if (n.type == WOLFRAM_NODE_SCRIPT) {
        wolframWriteNode(f, n.a, w);
      } else {
        wolframWrite(w, f.text + n.start, n.len);
      }
      if (n.flags & WOLFRAM_FLAG_SUP_FIRST) {
        if (n.c >= 0) wolframWrite(w, "^", 1);
        wolframWriteNode(f, n.c, w);
        if (n.b >= 0) wolframWrite(w, "_", 1);
        wolframWriteNode(f, n.b, w);
      } else {
        if (n.b >= 0) wolframWrite(w, "_", 1);
        wolframWriteNode(f, n.b, w);
        if (n.c >= 0) wolframWrite(w, "^", 1);
        wolframWriteNode(f, n.c, w);
      }
      break;
  }
}

inline bool wolframFormulaRoundTrips(const WolframFormula& f, WolframSpan line) {
  char written[WOLFRAM_FORMULA_LINE_LIMIT + 1];
  char source[WOLFRAM_FORMULA_LINE_LIMIT + 1];
  WolframWriter w = {written, sizeof(written), 0, false};
  WolframWriter s = {source, sizeof(source), 0, false};
  wolframWriteNode(f, f.root, w);
  wolframWrite(s, f.text + line.start, line.len);
  return !w.overflow && !s.overflow && w.length == s.length && memcmp(written, source, w.length) == 0;
}
