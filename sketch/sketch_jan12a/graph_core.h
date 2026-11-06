// Pure logic of function graphs: parser and evaluator, sampling, gaps, y fit and tick numbers.
// Plain C++ with no Arduino includes, tested on the host: tests/graph/run.sh
//
// The parser is iterative with fixed-size stacks, so hostile input can't exhaust memory or the
// call stack. It compiles to a short postfix program; every point on screen comes from
// evaluating that program on the device.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

static const size_t GRAPH_MAX_EXPR = 63;   // Same as ModelRef.query
static const int GRAPH_MAX_OPS = 48;       // Postfix program length
static const int GRAPH_MAX_CONSTS = 16;    // Numbers, pi and e in one expression
static const int GRAPH_MAX_STACK = 16;     // Values while evaluating
static const int GRAPH_MAX_PENDING = 24;   // Operators and open parentheses while parsing
static const int GRAPH_MAX_DIGITS = 15;    // Digits in one number: a double holds these exactly
static const size_t GRAPH_LABEL_SIZE = 96;  // The expression plus any * made visible

enum GraphOp : uint8_t {
  GRAPH_OP_X, GRAPH_OP_ADD, GRAPH_OP_SUB, GRAPH_OP_MUL, GRAPH_OP_DIV, GRAPH_OP_POW, GRAPH_OP_NEG,
  GRAPH_OP_SIN, GRAPH_OP_COS, GRAPH_OP_TAN, GRAPH_OP_SQRT, GRAPH_OP_ABS, GRAPH_OP_EXP, GRAPH_OP_LN, GRAPH_OP_LOG,
  GRAPH_OP_OPEN,          // Only while parsing: an open parenthesis
  GRAPH_OP_CONST = 128    // GRAPH_OP_CONST + i pushes constants[i]
};

struct GraphProgram {
  uint8_t count;
  uint8_t constCount;
  uint8_t ops[GRAPH_MAX_OPS];
  double constants[GRAPH_MAX_CONSTS];
  char label[GRAPH_LABEL_SIZE];  // What was understood: no spaces, lower case, ambiguous * written out
};

struct GraphParser {
  GraphProgram& out;
  uint8_t pending[GRAPH_MAX_PENDING];
  int pendingCount;
  int depth;  // Values on the stack when the program runs at this point
  size_t labelLength;
  bool slashInTerm;

  explicit GraphParser(GraphProgram& program) : out(program), pendingCount(0), depth(0), labelLength(0), slashInTerm(false) {}

  bool echo(const char* s, size_t n) {
    if (labelLength + n >= GRAPH_LABEL_SIZE) return false;
    memcpy(out.label + labelLength, s, n);
    labelLength += n;
    out.label[labelLength] = 0;
    return true;
  }

  // Tracks the stack the program will use, so evaluating needs no checks
  bool emit(uint8_t op) {
    if (out.count >= GRAPH_MAX_OPS) return false;
    if (op == GRAPH_OP_X || op >= GRAPH_OP_CONST) {
      if (++depth > GRAPH_MAX_STACK) return false;
    } else if (op >= GRAPH_OP_ADD && op <= GRAPH_OP_POW) {
      if (depth < 2) return false;
      depth--;
    } else if (depth < 1) {
      return false;
    }
    out.ops[out.count++] = op;
    return true;
  }

  bool emitConstant(double value) {
    if (out.constCount >= GRAPH_MAX_CONSTS) return false;
    out.constants[out.constCount] = value;
    return emit((uint8_t)(GRAPH_OP_CONST + out.constCount++));
  }

  bool push(uint8_t op) {
    if (pendingCount >= GRAPH_MAX_PENDING) return false;
    pending[pendingCount++] = op;
    return true;
  }

  static int precedence(uint8_t op) {
    switch (op) {
      case GRAPH_OP_ADD: case GRAPH_OP_SUB: return 1;
      case GRAPH_OP_MUL: case GRAPH_OP_DIV: return 2;
      case GRAPH_OP_NEG: return 3;  // -x^2 is -(x^2), 2*-x works
      case GRAPH_OP_POW: return 4;
      default: return 0;            // Parenthesis or function: a binary operator stops here
    }
  }

  bool binary(uint8_t op) {
    const int p = precedence(op);
    while (pendingCount > 0) {
      const int top = precedence(pending[pendingCount - 1]);
      // ^ groups from the right: 2^3^2 is 2^(3^2)
      if (top > p || (top == p && op != GRAPH_OP_POW)) {
        if (!emit(pending[--pendingCount])) return false;
      } else {
        break;
      }
    }
    return push(op);
  }

  // 2x, (x+1)(x-1). Written out where the reading isn't obvious: 1/2x means (1/2)*x, not 1/(2x)
  bool implicitMultiply(char previous, char next) {
    const bool bothLetters = isLetter(previous) && isLetter(next);
    if ((slashInTerm || bothLetters) && !echo("*", 1)) return false;
    return binary(GRAPH_OP_MUL);
  }

  static bool isLetter(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
};

inline char graphLower(char c) {
  return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
}

inline bool graphMatches(const char* text, const char* name) {
  for (; *name; text++, name++) {
    if (graphLower(*text) != *name) return false;
  }
  return true;
}

struct GraphName {
  const char* name;
  uint8_t length;
  uint8_t op;      // A function, or GRAPH_OP_X, or GRAPH_OP_CONST for the constants below
  double value;
};

// Longest first where one name starts another: exp before e
static const GraphName GRAPH_NAMES[] = {
    {"sqrt", 4, GRAPH_OP_SQRT, 0},  {"sin", 3, GRAPH_OP_SIN, 0}, {"cos", 3, GRAPH_OP_COS, 0},
    {"tan", 3, GRAPH_OP_TAN, 0},    {"abs", 3, GRAPH_OP_ABS, 0}, {"exp", 3, GRAPH_OP_EXP, 0},
    {"log", 3, GRAPH_OP_LOG, 0},    {"ln", 2, GRAPH_OP_LN, 0},
    {"pi", 2, GRAPH_OP_CONST, 3.14159265358979323846},
    {"e", 1, GRAPH_OP_CONST, 2.71828182845904523536},
    {"x", 1, GRAPH_OP_X, 0},
};

// Skips "y=" or "f(x)=" in front: the expression is what counts
inline const char* graphSkipPrefix(const char* p) {
  const char* prefixes[] = {"y", "f(x)"};
  for (const char* prefix : prefixes) {
    const char* q = p;
    while (*q == ' ') q++;
    if (!graphMatches(q, prefix)) continue;
    q += strlen(prefix);
    while (*q == ' ') q++;
    if (*q == '=') return q + 1;
  }
  return p;
}

// Compiles an expression in x. False on anything unexpected; then the program must not be used.
inline bool graphCompile(const char* text, GraphProgram& out) {
  out.count = 0;
  out.constCount = 0;
  out.label[0] = 0;
  if (!text) return false;
  for (size_t i = 0; i <= GRAPH_MAX_EXPR; i++) {
    if (!text[i]) break;
    if (i == GRAPH_MAX_EXPR) return false;
  }

  GraphParser parser(out);
  bool expectOperand = true;
  char previous = 0;
  const char* p = graphSkipPrefix(text);
  while (*p) {
    const char c = *p;
    if (c == ' ') {
      p++;
    } else if ((c >= '0' && c <= '9') || c == '.') {
      // Written by hand: strtod would also read 0x1, 2e3, inf and nan
      if (!expectOperand) return false;  // "x2" and "2 3" are too likely a mistake to multiply
      const char* start = p;
      double mantissa = 0, divisor = 1;
      int digits = 0;
      bool point = false;
      for (; (*p >= '0' && *p <= '9') || *p == '.'; p++) {
        if (*p == '.') {
          if (point) return false;
          point = true;
        } else {
          if (++digits > GRAPH_MAX_DIGITS) return false;
          mantissa = mantissa * 10 + (*p - '0');
          if (point) divisor *= 10;
        }
      }
      if (digits == 0) return false;
      if (!parser.echo(start, p - start) || !parser.emitConstant(mantissa / divisor)) return false;
      expectOperand = false;
      previous = p[-1];
    } else if (GraphParser::isLetter(c)) {
      const GraphName* found = nullptr;
      for (const GraphName& name : GRAPH_NAMES) {
        if (graphMatches(p, name.name)) {
          found = &name;
          break;
        }
      }
      if (!found) return false;
      if (!expectOperand && !parser.implicitMultiply(previous, c)) return false;
      if (!parser.echo(found->name, found->length)) return false;
      p += found->length;
      if (found->op == GRAPH_OP_X) {
        if (!parser.emit(GRAPH_OP_X)) return false;
        expectOperand = false;
      } else if (found->op == GRAPH_OP_CONST) {
        if (!parser.emitConstant(found->value)) return false;
        expectOperand = false;
      } else {
        // A function needs its parentheses: "sin x" is not accepted
        while (*p == ' ') p++;
        if (*p != '(' || !parser.push(found->op)) return false;
        expectOperand = true;
      }
      previous = p[-1];
    } else {
      p++;
      switch (c) {
        case '(':
          if (!expectOperand && !parser.implicitMultiply(previous, c)) return false;
          if (!parser.push(GRAPH_OP_OPEN) || !parser.echo(&c, 1)) return false;
          expectOperand = true;
          break;
        case ')': {
          if (expectOperand) return false;
          bool closed = false;
          while (parser.pendingCount > 0 && !closed) {
            const uint8_t op = parser.pending[--parser.pendingCount];
            if (op == GRAPH_OP_OPEN) {
              closed = true;
            } else if (!parser.emit(op)) {
              return false;
            }
          }
          if (!closed || !parser.echo(&c, 1)) return false;
          if (parser.pendingCount > 0) {
            const uint8_t top = parser.pending[parser.pendingCount - 1];
            if (top >= GRAPH_OP_SIN && top <= GRAPH_OP_LOG) {
              parser.pendingCount--;
              if (!parser.emit(top)) return false;
            }
          }
          break;
        }
        case '+':
        case '-':
          if (expectOperand) {
            // A sign binds to what follows, so nothing is taken off the stack
            if (c == '-' && !parser.push(GRAPH_OP_NEG)) return false;
          } else {
            if (!parser.binary(c == '+' ? GRAPH_OP_ADD : GRAPH_OP_SUB)) return false;
            parser.slashInTerm = false;
            expectOperand = true;
          }
          if (!parser.echo(&c, 1)) return false;
          break;
        case '*':
        case '/':
        case '^':
          if (expectOperand) return false;
          if (!parser.binary(c == '*' ? GRAPH_OP_MUL : c == '/' ? GRAPH_OP_DIV : GRAPH_OP_POW)) return false;
          if (c == '/') parser.slashInTerm = true;
          if (!parser.echo(&c, 1)) return false;
          expectOperand = true;
          break;
        default:
          return false;
      }
      previous = c;
    }
  }
  if (expectOperand) return false;
  while (parser.pendingCount > 0) {
    const uint8_t op = parser.pending[--parser.pendingCount];
    if (op == GRAPH_OP_OPEN || !parser.emit(op)) return false;
  }
  return parser.depth == 1;
}

// The value at x. Not finite (NaN or infinity) means there is no point here.
inline double graphEval(const GraphProgram& program, double x) {
  double stack[GRAPH_MAX_STACK];
  int top = -1;
  for (int i = 0; i < program.count; i++) {
    const uint8_t op = program.ops[i];
    if (op >= GRAPH_OP_CONST) {
      stack[++top] = program.constants[op - GRAPH_OP_CONST];
      continue;
    }
    switch (op) {
      case GRAPH_OP_X: stack[++top] = x; break;
      case GRAPH_OP_ADD: top--; stack[top] += stack[top + 1]; break;
      case GRAPH_OP_SUB: top--; stack[top] -= stack[top + 1]; break;
      case GRAPH_OP_MUL: top--; stack[top] *= stack[top + 1]; break;
      case GRAPH_OP_DIV: top--; stack[top] /= stack[top + 1]; break;
      case GRAPH_OP_POW:
        top--;
        // x^2 is by far the most common power, and pow() is slow without a double-precision FPU
        stack[top] = stack[top + 1] == 2.0 ? stack[top] * stack[top] : std::pow(stack[top], stack[top + 1]);
        break;
      case GRAPH_OP_NEG: stack[top] = -stack[top]; break;
      case GRAPH_OP_SIN: stack[top] = std::sin(stack[top]); break;
      case GRAPH_OP_COS: stack[top] = std::cos(stack[top]); break;
      case GRAPH_OP_TAN: stack[top] = std::tan(stack[top]); break;
      case GRAPH_OP_SQRT: stack[top] = std::sqrt(stack[top]); break;
      case GRAPH_OP_ABS: stack[top] = std::fabs(stack[top]); break;
      case GRAPH_OP_EXP: stack[top] = std::exp(stack[top]); break;
      case GRAPH_OP_LN: stack[top] = std::log(stack[top]); break;
      case GRAPH_OP_LOG: stack[top] = std::log10(stack[top]); break;
      default: return NAN;
    }
  }
  return top == 0 ? stack[0] : NAN;
}

// ----- Sampling: one value per pixel column, kept so that panning computes only new columns -----

static const int GRAPH_COLUMNS = 240;
static const double GRAPH_MAX_CENTER = 1000;   // Panning stops here: far enough, and x keeps its precision
static const float GRAPH_LARGEST_SAMPLE = 1e30f;  // Beyond this a value is treated as no point
static const int GRAPH_JOIN_PIXELS = 6;        // Up to this jump two neighbours are always joined
static const int GRAPH_BISECT_STEPS = 12;
static const int GRAPH_FAR_PIXELS = 1000;      // Pixel values are clamped to this far outside the plot

enum GraphSegment : uint8_t { GRAPH_SEGMENT_UNKNOWN, GRAPH_SEGMENT_JOINED, GRAPH_SEGMENT_BROKEN };

struct GraphSamples {
  float y[GRAPH_COLUMNS];         // NaN: no point in this column
  uint8_t segment[GRAPH_COLUMNS];  // Between column i and i + 1. Independent of the y range, so it can be kept
  float sorted[GRAPH_COLUMNS];    // Working space for the y fit
  int lo, hi;                     // Columns lo .. hi - 1 are computed
  long offset;                    // Column GRAPH_COLUMNS / 2 is at x = offset * unitsPerColumn
  double unitsPerColumn;
};

inline double graphColumnX(const GraphSamples& s, int column) {
  // A multiple of the step, never a running sum: x = 0 is exactly 0, and no error builds up while panning
  return (double)(column - GRAPH_COLUMNS / 2 + s.offset) * s.unitsPerColumn;
}

inline double graphCenterX(const GraphSamples& s) {
  return (double)s.offset * s.unitsPerColumn;
}

// Shows centerX +- halfWidth. Everything gets computed again, from the middle outwards.
inline void graphSetWindow(GraphSamples& s, double centerX, double halfWidth) {
  s.unitsPerColumn = halfWidth / (GRAPH_COLUMNS / 2);
  s.offset = std::lround(centerX / s.unitsPerColumn);
  s.lo = s.hi = GRAPH_COLUMNS / 2;
}

// Moves the window by whole columns, to the right if positive. What's still on screen is kept.
inline void graphPan(GraphSamples& s, long columns) {
  const long limit = std::lround(GRAPH_MAX_CENTER / s.unitsPerColumn);
  const long target = std::max(-limit, std::min(limit, s.offset + columns));
  const long move = target - s.offset;
  if (move == 0) return;
  s.offset = target;
  if (move >= GRAPH_COLUMNS || move <= -GRAPH_COLUMNS) {
    s.lo = s.hi = GRAPH_COLUMNS / 2;
    return;
  }
  const int d = (int)(move > 0 ? move : -move);
  const int kept = GRAPH_COLUMNS - d;
  if (move > 0) {
    memmove(s.y, s.y + d, kept * sizeof(float));
    memmove(s.segment, s.segment + d, kept);
    s.lo = std::max(0, s.lo - d);
    s.hi -= d;
  } else {
    memmove(s.y + d, s.y, kept * sizeof(float));
    memmove(s.segment + d, s.segment, kept);
    s.lo += d;
    s.hi = std::min(GRAPH_COLUMNS, s.hi + d);
  }
  if (s.hi <= s.lo) s.lo = s.hi = GRAPH_COLUMNS / 2;
}

inline float graphToSample(double value) {
  if (!std::isfinite(value) || std::fabs(value) > (double)GRAPH_LARGEST_SAMPLE) return NAN;
  return (float)value;
}

// Computes one more column. False when all are done. The caller decides how many fit in a frame.
inline bool graphComputeNext(const GraphProgram& program, GraphSamples& s) {
  if (s.lo <= 0 && s.hi >= GRAPH_COLUMNS) return false;
  const bool right = s.lo <= 0 || (s.hi < GRAPH_COLUMNS && s.hi - GRAPH_COLUMNS / 2 <= GRAPH_COLUMNS / 2 - s.lo);
  const int column = right ? s.hi : s.lo - 1;
  s.y[column] = graphToSample(graphEval(program, graphColumnX(s, column)));
  s.segment[column] = GRAPH_SEGMENT_UNKNOWN;
  if (column > 0) s.segment[column - 1] = GRAPH_SEGMENT_UNKNOWN;
  if (right) {
    s.hi++;
  } else {
    s.lo--;
  }
  return true;
}

// True if the curve really breaks between x0 and x1: a pole like tan(x) and 1/x, or a jump like x/abs(x).
// Halving the interval towards the larger change: on a continuous curve the change shrinks with the
// interval, across a jump it stays, and across a pole it grows.
inline bool graphIsBreak(const GraphProgram& program, double x0, double x1) {
  double y0 = graphEval(program, x0), y1 = graphEval(program, x1);
  if (!std::isfinite(y0) || !std::isfinite(y1)) return true;
  const double initial = std::fabs(y1 - y0);
  for (int i = 0; i < GRAPH_BISECT_STEPS; i++) {
    const double xm = (x0 + x1) / 2;
    if (xm == x0 || xm == x1) break;
    const double ym = graphEval(program, xm);
    if (!std::isfinite(ym)) return true;
    if (std::fabs(ym - y0) >= std::fabs(y1 - ym)) {
      x1 = xm;
      y1 = ym;
    } else {
      x0 = xm;
      y0 = ym;
    }
  }
  return std::fabs(y1 - y0) > 0.25 * initial;
}

// Pixel row of a value in a plot of the given height: ymax on row 0, clamped so a huge value stays a safe int
inline int graphPixelY(double y, double ymin, double ymax, int height) {
  const double row = (ymax - y) * (height - 1) / (ymax - ymin);
  if (!(row > -GRAPH_FAR_PIXELS)) return -GRAPH_FAR_PIXELS;
  if (row > height + GRAPH_FAR_PIXELS) return height + GRAPH_FAR_PIXELS;
  return (int)std::floor(row + 0.5);
}

// Whether to draw from column i to i + 1, and where. Checking for a break costs evaluations, so it's
// only done for a large jump, only when mayClassify says there's time, and only once per segment.
// Not yet known: not drawn in this frame.
inline bool graphSegmentRows(const GraphProgram& program, GraphSamples& s, int i, double ymin, double ymax, int height,
                             bool mayClassify, int& row0, int& row1) {
  if (i < s.lo || i + 1 >= s.hi) return false;
  const float y0 = s.y[i], y1 = s.y[i + 1];
  if (!std::isfinite(y0) || !std::isfinite(y1)) return false;
  row0 = graphPixelY(y0, ymin, ymax, height);
  row1 = graphPixelY(y1, ymin, ymax, height);
  if ((row0 < 0 && row1 < 0) || (row0 >= height && row1 >= height)) return false;
  const int jump = row1 > row0 ? row1 - row0 : row0 - row1;
  if (jump <= GRAPH_JOIN_PIXELS) return true;
  if (s.segment[i] == GRAPH_SEGMENT_UNKNOWN) {
    if (!mayClassify) return false;
    s.segment[i] = graphIsBreak(program, graphColumnX(s, i), graphColumnX(s, i + 1)) ? GRAPH_SEGMENT_BROKEN
                                                                                    : GRAPH_SEGMENT_JOINED;
  }
  return s.segment[i] == GRAPH_SEGMENT_JOINED;
}

// ----- Fitting y to what's visible -----

// The range to show. Normally all of the curve; with poles the extreme values would flatten everything
// else, so the most extreme few percent go off screen. False if nothing is computed yet or there are no points.
inline bool graphFitY(GraphSamples& s, double& ymin, double& ymax) {
  int n = 0;
  for (int i = s.lo; i < s.hi; i++) {
    if (std::isfinite(s.y[i])) s.sorted[n++] = s.y[i];
  }
  if (n == 0) return false;
  std::sort(s.sorted, s.sorted + n);
  double low = s.sorted[0], high = s.sorted[n - 1];
  const double middle = (double)s.sorted[n - 1 - n / 10] - (double)s.sorted[n / 10];
  if (high - low > 8 * middle) {
    const int cut = n * 3 / 100;
    if (s.sorted[n - 1 - cut] > s.sorted[cut]) {
      low = s.sorted[cut];
      high = s.sorted[n - 1 - cut];
    }
  }
  const double size = std::max(std::fabs(low), std::fabs(high));
  if (high - low <= 1e-6 * size || high - low == 0) {
    // A flat line: float samples can't tell it from one with invisible wiggles
    const double pad = std::max(1.0, size * 0.1);
    ymin = low - pad;
    ymax = high + pad;
  } else {
    const double margin = (high - low) * 0.08;
    ymin = low - margin;
    ymax = high + margin;
  }
  return true;
}

// The y range on screen follows the fit smoothly and ignores small changes, so tick numbers don't flicker
struct GraphYView {
  double min, max, targetMin, targetMax;
  bool ready;
};

inline void graphYViewFollow(GraphYView& view, double fitMin, double fitMax) {
  const double tolerance = 0.15 * (view.targetMax - view.targetMin);
  if (!view.ready) {
    view.min = view.targetMin = fitMin;
    view.max = view.targetMax = fitMax;
    view.ready = true;
  } else if (std::fabs(fitMin - view.targetMin) > tolerance || std::fabs(fitMax - view.targetMax) > tolerance) {
    view.targetMin = fitMin;
    view.targetMax = fitMax;
  }
}

inline void graphYViewEase(GraphYView& view, float dt) {
  const double k = std::min(1.0, 8.0 * (double)dt);
  view.min += (view.targetMin - view.min) * k;
  view.max += (view.targetMax - view.max) * k;
}

// ----- Tick numbers -----

// A round step (1, 2 or 5 times a power of ten) for at most about maxTicks ticks in this range
inline double graphNiceStep(double range, int maxTicks) {
  if (!std::isfinite(range) || range <= 0 || maxTicks < 1) return 1;
  const double rough = range / maxTicks;
  const double magnitude = std::pow(10.0, std::floor(std::log10(rough)));
  const double n = rough / magnitude;
  return (n <= 1 ? 1 : n <= 2 ? 2 : n <= 5 ? 5 : 10) * magnitude;
}

// Writes 2e+06 as 2e6: the screen is narrow
inline void graphShortenExponent(char* text) {
  char* e = strchr(text, 'e');
  if (!e) return;
  char* from = e + 1;
  char* to = e + 1;
  if (*from == '+') {
    from++;
  } else if (*from == '-') {
    *to++ = *from++;
  }
  while (*from == '0' && from[1]) from++;
  while (*from) *to++ = *from++;
  *to = 0;
}

// The number at tick k: built from the step, so no 0.30000001 and no -0
inline void graphFormatTick(long k, double step, char* out, size_t size) {
  if (k == 0) {
    snprintf(out, size, "0");
    return;
  }
  const double value = (double)k * step;
  if (std::fabs(value) >= 1e6 || step < 1e-4) {
    snprintf(out, size, "%.2g", value);
    graphShortenExponent(out);
  } else {
    int decimals = 0;
    for (double scaled = step; decimals < 5 && std::fabs(scaled - std::floor(scaled + 0.5)) > 1e-6 * scaled; scaled *= 10) {
      decimals++;
    }
    snprintf(out, size, "%.*f", decimals, value);
  }
}

// A value at the dot. scale is the size of what's on screen: much smaller than that is rounding, so 0.
inline void graphFormatValue(double value, double scale, char* out, size_t size) {
  if (!std::isfinite(value)) {
    snprintf(out, size, "--");
  } else if (value == 0 || std::fabs(value) < scale * 1e-4) {
    snprintf(out, size, "0");
  } else if (std::fabs(value) >= 1000 && std::fabs(value) < 1e6) {
    snprintf(out, size, "%.0f", value);
  } else {
    snprintf(out, size, "%.3g", value);
    graphShortenExponent(out);
  }
}

// ----- Moving by turning the stick -----

static const float GRAPH_STILL_DPS = 4.0f;  // Below this the hand isn't turning: sensor noise

// Turning speed along the curve from the gyro, in degrees per second, positive towards larger x.
// Both ways of turning count, whichever is stronger: rocking the stick like a steering wheel, clockwise
// (screen axis z, gz negative), and swinging it so the screen faces right (screen axis y, gy positive).
inline float graphTurnRate(float gy, float gz) {
  const float rate = std::fabs(gy) >= std::fabs(gz) ? gy : -gz;
  return std::fabs(rate) >= GRAPH_STILL_DPS ? rate : 0.0f;
}

// ----- Where to start -----

// The window to open with: around 0 if the curve is there, otherwise where it is.
// False if there's nothing to draw anywhere near: sqrt(-1), 1/0, sqrt(x-1000).
inline bool graphFindStart(const GraphProgram& program, double defaultHalfWidth, double& centerX) {
  const double scans[] = {defaultHalfWidth, 20, 100};
  for (const double halfWidth : scans) {
    const double step = halfWidth / (GRAPH_COLUMNS / 2);
    int count = 0;
    double first = 0, last = 0;
    for (int i = 0; i < GRAPH_COLUMNS; i++) {
      const double x = (i - GRAPH_COLUMNS / 2) * step;
      if (!std::isfinite(graphToSample(graphEval(program, x)))) continue;
      if (count++ == 0) first = x;
      last = x;
    }
    if (count >= 2) {
      // Two points at least: a single one, like sqrt(-x^2) at 0, is not a graph
      const bool mostlyThere = count >= GRAPH_COLUMNS * 6 / 10;
      centerX = mostlyThere ? 0 : (first + last) / 2;
      return true;
    }
  }
  return false;
}
