// Pure logic of function graphs: parser and evaluator for expressions in x.
// Plain C++ with no Arduino includes, tested on the host: tests/graph/run.sh
//
// The parser is iterative with fixed-size stacks, so hostile input can't exhaust memory or the
// call stack. It compiles to a short postfix program; every point on screen comes from
// evaluating that program on the device.
#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
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
