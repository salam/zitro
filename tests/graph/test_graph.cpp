// Host tests for function graphs. Build and run: tests/graph/run.sh
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "../../sketch/sketch_jan12a/graph_core.h"

static int failures = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { failures++; \
  fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static GraphProgram program;

static bool compile(const std::string& s) { return graphCompile(s.c_str(), program); }

// The value of an expression at x, compared with the same maths written in C++
static void checkValue(const char* expr, double x, double expected) {
  checks++;
  if (!graphCompile(expr, program)) {
    failures++;
    fprintf(stderr, "  FAIL: '%s' did not compile\n", expr);
    return;
  }
  const double got = graphEval(program, x);
  if (!(std::fabs(got - expected) <= 1e-12 * (1 + std::fabs(expected)))) {
    failures++;
    fprintf(stderr, "  FAIL: '%s' at %g gave %.15g, expected %.15g\n", expr, x, got, expected);
  }
}

static void checkRejected(const std::string& expr) {
  checks++;
  if (compile(expr)) {
    failures++;
    fprintf(stderr, "  FAIL: '%s' should be rejected\n", expr.substr(0, 60).c_str());
  }
}

static void checkLabel(const char* expr, const char* label) {
  checks++;
  if (!graphCompile(expr, program) || strcmp(program.label, label) != 0) {
    failures++;
    fprintf(stderr, "  FAIL: '%s' labelled '%s', expected '%s'\n", expr, program.label, label);
  }
}

static void testArithmetic() {
  checkValue("x", 3, 3);
  checkValue("x+1", 3, 4);
  checkValue("x-1", 3, 2);
  checkValue("x*2", 3, 6);
  checkValue("x/2", 3, 1.5);
  checkValue("x^2-3", 3, 6);
  checkValue("1+2*3", 0, 7);
  checkValue("(1+2)*3", 0, 9);
  checkValue("10-3-2", 0, 5);    // Left to right
  checkValue("24/4/2", 0, 3);
  checkValue("0.5x", 4, 2);
  checkValue(".5x", 4, 2);
  checkValue("2.x", 4, 8);
  checkValue(" 2 * x ", 4, 8);
}

static void testPrecedence() {
  checkValue("2x^2", 3, 18);     // Not 36
  checkValue("-x^2", 2, -4);     // Not 4
  checkValue("(-x)^2", 2, 4);
  checkValue("2^-1", 0, 0.5);
  checkValue("2^3^2", 0, 512);   // From the right
  checkValue("2^-x^2", 2, 1.0 / 16);
  checkValue("--x", 2, 2);
  checkValue("+x", 2, 2);
  checkValue("x--2", 2, 4);
  checkValue("2*-x", 2, -4);
  checkValue("-x*2", 2, -4);
  checkValue("x^2^0", 5, 5);
  checkValue("2^x*3", 2, 12);
}

static void testImplicitMultiplication() {
  checkValue("2x", 3, 6);
  checkValue("2(x+1)", 3, 8);
  checkValue("(x+1)(x-1)", 3, 8);
  checkValue("x(x+1)", 3, 12);
  checkValue("2pi", 0, 2 * M_PI);
  checkValue("2pix", 3, 6 * M_PI);
  checkValue("2sin(x)", 1, 2 * sin(1.0));
  checkValue("x sin(x)", 1, sin(1.0));
  checkValue("xsin(x)", 2, 2 * sin(2.0));
  checkValue("2e^x", 1, 2 * M_E);
  checkValue("ex", 2, 2 * M_E);       // e times x, not a name
  checkValue("xe", 2, 2 * M_E);
  checkValue("sin(x)cos(x)", 1, sin(1.0) * cos(1.0));
  checkValue("1/2x", 4, 2);           // (1/2)*x, as Wolfram reads it
  checkValue("1/(2x)", 4, 0.125);
  checkValue("2^3x", 2, 16);
}

static void testFunctions() {
  for (double x : {-2.5, -0.5, 0.25, 1.0, 7.5}) {
    checkValue("sin(x)", x, sin(x));
    checkValue("cos(x)", x, cos(x));
    checkValue("tan(x)", x, tan(x));
    checkValue("abs(x)", x, fabs(x));
    checkValue("exp(x)", x, exp(x));
    checkValue("sin(x)^2+cos(x)^2", x, 1);
  }
  checkValue("sqrt(x)", 16, 4);
  checkValue("ln(x)", M_E, 1);
  checkValue("log(x)", 1000, 3);      // Base 10, as taught in school
  checkValue("pi", 0, M_PI);
  checkValue("e", 0, M_E);
  checkValue("e^x", 2, M_E * M_E);
  checkValue("sin(cos(tan(x)))", 1, sin(cos(tan(1.0))));
  checkValue("SIN(X)", 1, sin(1.0));  // The AI's capitals aren't a reason to fail
  checkValue("Pi", 0, M_PI);
  checkValue("sin (x)", 1, sin(1.0));
  checkValue("y=x^2", 3, 9);          // The AI sometimes sends the whole equation
  checkValue("y = x^2", 3, 9);
  checkValue("f(x)=x^2", 3, 9);
  checkValue("5", 99, 5);             // A flat line is a true graph
}

static void testNoPoint() {
  const std::pair<const char*, double> cases[] = {
      {"1/x", 0}, {"1/0", 1}, {"x/x", 0}, {"0/0", 1}, {"sqrt(x)", -1}, {"ln(x)", 0}, {"ln(x)", -1},
      {"log(x)", -1}, {"x^0.5", -1}, {"(-8)^(1/3)", 0}, {"exp(x)", 1e6}, {"x^x^x^x", 1e6}};
  for (const auto& c : cases) {
    checks++;
    if (!graphCompile(c.first, program) || std::isfinite(graphEval(program, c.second))) {
      failures++;
      fprintf(stderr, "  FAIL: '%s' at %g should have no point\n", c.first, c.second);
    }
  }
  // NaN and infinity going in come out as no point, without a crash
  CHECK(compile("sin(x)+1/x"));
  CHECK(!std::isfinite(graphEval(program, NAN)));
  CHECK(!std::isfinite(graphEval(program, INFINITY)));
}

static void testInvalid() {
  const char* bad[] = {
      "", " ", "+", "-", "*", "()", "(", ")", "(x", "x)", "(x))", "((x)", ")x(", "x+", "x-", "*x", "/x", "^x",
      "x*", "x/", "x^", "x++", "x**2", "x*/2", "sin", "sin()", "sin x", "sinx", "sin(", "sin)", "cos(x", "sqrt",
      "y", "y=", "f(x)=", "x=3", "z", "x+y", "2,5", "2..5", "1.2.3", ".", "x.", "foo(x)", "sinh(x)", "asin(x)",
      "x!", "x%2", "x=2", "x;2", "x\"", "$x", "\\sin(x)", "x\n", "x\t2",
      "x2", "2 3", "(x+1)2", "1e3", "2e3", "0x1", "0x10", "inf", "nan", "infinity", "NaN",
      "x\xc2\xb2", "\xcf\x80x", "2\xc2\xb7x", "\xff\xfe", "1234567890123456"};
  for (const char* s : bad) checkRejected(s);
  CHECK(!graphCompile(nullptr, program));
}

static void testHostile() {
  checkRejected(std::string(10000, '('));
  checkRejected(std::string(10000, 'x'));
  checkRejected(std::string(100000, '1'));
  checkRejected(std::string(10000, '-') + "x");
  checkRejected(std::string(5000, '(') + "x" + std::string(5000, ')'));
  std::string powers = "x";
  for (int i = 0; i < 5000; i++) powers += "^x";
  checkRejected(powers);

  // Everything that fits the length limit but overflows a fixed stack or program
  checkRejected(std::string(40, '('));
  checkRejected(std::string(30, '(') + "x" + std::string(30, ')'));
  checkRejected(std::string(62, '-') + "x");
  checkRejected("x" + std::string(62, 'x'));
  std::string constants = "1";
  for (int i = 0; i < 20; i++) constants += "+1";
  checkRejected(constants);
  std::string rightDeep = "x";
  for (int i = 0; i < 31; i++) rightDeep += "^x";
  checkRejected(rightDeep);

  // Length limit: 63 is what fits a ModelRef
  std::string fits = "x" + std::string(62, ' ');
  CHECK(compile(fits));
  checkRejected("x" + std::string(63, ' '));

  // Real nesting of a sensible depth still works
  checkValue("sin(cos(sin(cos(sin(cos(x))))))", 1, sin(cos(sin(cos(sin(cos(1.0)))))));
  checkValue("((((((((x))))))))", 2, 2);

  // Every short combination of the alphabet: compiled programs must evaluate safely.
  // The sanitizers catch any stack or array overrun.
  const char alphabet[] = "x2.+-*/^()sine ";
  const int n = sizeof(alphabet) - 1;
  char text[6] = {0};
  int compiled = 0;
  for (int a = 0; a < n; a++) for (int b = 0; b < n; b++) for (int c = 0; c < n; c++)
    for (int d = 0; d < n; d++) for (int f = 0; f < n; f++) {
      text[0] = alphabet[a]; text[1] = alphabet[b]; text[2] = alphabet[c]; text[3] = alphabet[d]; text[4] = alphabet[f];
      if (graphCompile(text, program)) {
        compiled++;
        graphEval(program, 1.5);
      }
    }
  CHECK(compiled > 1000);

  // Random bytes, including long ones
  srand(7);
  char buffer[80];
  for (int i = 0; i < 200000; i++) {
    const int length = rand() % 79;
    for (int k = 0; k < length; k++) {
      buffer[k] = (i % 2) ? alphabet[rand() % n] : (char)(1 + rand() % 255);
    }
    buffer[length] = 0;
    if (graphCompile(buffer, program)) graphEval(program, 0.5);
  }
}

static void testLabel() {
  checkLabel("sin(x)", "sin(x)");
  checkLabel("x^2-3", "x^2-3");
  checkLabel(" 2 x ", "2x");
  checkLabel("SIN(X)", "sin(x)");
  checkLabel("y = x^2", "x^2");
  checkLabel("2(x+1)", "2(x+1)");
  checkLabel("(x+1)(x-1)", "(x+1)(x-1)");
  checkLabel("x sin(x)", "x*sin(x)");   // Not "xsin(x)"
  checkLabel("pix", "pi*x");
  checkLabel("1/2x", "1/2*x");          // Shows how it was read
  checkLabel("1/2+3x", "1/2+3x");
  checkLabel("1/x+2x", "1/x+2x");
  checkLabel("-x^2", "-x^2");
}

int main() {
  testArithmetic();
  testPrecedence();
  testImplicitMultiplication();
  testFunctions();
  testNoPoint();
  testInvalid();
  testHostile();
  testLabel();
  printf("%d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
