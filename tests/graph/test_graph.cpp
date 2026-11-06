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

// ----- Sampling -----

static const int PLOT_HEIGHT = 117;
static GraphSamples samples;

static void computeAll() {
  while (graphComputeNext(program, samples)) {}
}

static void openWindow(const char* expr, double center, double halfWidth) {
  CHECK(graphCompile(expr, program));
  graphSetWindow(samples, center, halfWidth);
  computeAll();
}

static void testWindow() {
  openWindow("x", 0, 5);
  CHECK(samples.lo == 0 && samples.hi == GRAPH_COLUMNS);
  CHECK(graphColumnX(samples, GRAPH_COLUMNS / 2) == 0.0);  // Exactly 0, so 1/x has its gap there
  CHECK(fabs(graphColumnX(samples, 0) + 5) < 1e-12);
  CHECK(fabs(graphColumnX(samples, 239) - (5 - 5.0 / 120)) < 1e-12);
  for (int i = 0; i < GRAPH_COLUMNS; i++) CHECK(fabs(samples.y[i] - graphColumnX(samples, i)) < 1e-5);
}

// After any pan, every column must hold the value at its own x
static void checkSamplesMatch(const char* where) {
  for (int i = samples.lo; i < samples.hi; i++) {
    const double expected = graphEval(program, graphColumnX(samples, i));
    checks++;
    if (fabs(samples.y[i] - expected) > 1e-4 * (1 + fabs(expected))) {
      failures++;
      fprintf(stderr, "  FAIL %s: column %d at x=%g has %g, expected %g\n", where, i, graphColumnX(samples, i), samples.y[i], expected);
      return;
    }
  }
}

static void testPanningKeepsAndComputes() {
  openWindow("sin(x)+x/3", 0, 5);
  // Panning computes only what's new
  graphPan(samples, 10);
  CHECK(samples.lo == 0 && samples.hi == GRAPH_COLUMNS - 10);
  checkSamplesMatch("right");
  int computed = 0;
  while (graphComputeNext(program, samples)) computed++;
  CHECK(computed == 10);
  checkSamplesMatch("right, done");

  graphPan(samples, -25);
  CHECK(samples.lo == 25 && samples.hi == GRAPH_COLUMNS);
  checkSamplesMatch("left");
  computed = 0;
  while (graphComputeNext(program, samples)) computed++;
  CHECK(computed == 25);
  checkSamplesMatch("left, done");

  // Further than the screen is wide
  graphPan(samples, 1000);
  CHECK(samples.hi - samples.lo == 0);
  computeAll();
  checkSamplesMatch("far");
  graphPan(samples, -1000);
  computeAll();
  checkSamplesMatch("far back");

  // Panning while the previous pan isn't computed yet
  srand(3);
  for (int i = 0; i < 3000; i++) {
    graphPan(samples, rand() % 601 - 300);
    for (int k = rand() % 40; k > 0; k--) graphComputeNext(program, samples);
    CHECK(samples.lo >= 0 && samples.hi <= GRAPH_COLUMNS && samples.lo <= samples.hi);
    checkSamplesMatch("random");
  }
  computeAll();
  CHECK(samples.lo == 0 && samples.hi == GRAPH_COLUMNS);
  checkSamplesMatch("random, done");

  // Stops at the limit and stays usable
  openWindow("x", 0, 5);
  for (int i = 0; i < 2000; i++) graphPan(samples, 5000);
  CHECK(fabs(graphCenterX(samples) - GRAPH_MAX_CENTER) < 0.05);
  for (int i = 0; i < 2000; i++) graphPan(samples, -5000);
  CHECK(fabs(graphCenterX(samples) + GRAPH_MAX_CENTER) < 0.05);

  // Pan by a huge amount without overflow in the window arithmetic
  openWindow("x", 0, 1);
  graphPan(samples, 2000000000L);
  CHECK(fabs(graphCenterX(samples) - GRAPH_MAX_CENTER) < 0.05);
  computeAll();
  checkSamplesMatch("huge pan");
}

// Largest drawn jump in pixels, and number of drawn segments
struct Drawn {
  int largestJump;
  int segments;
};

static Drawn drawAll(double ymin, double ymax) {
  Drawn d = {0, 0};
  for (int i = 0; i + 1 < GRAPH_COLUMNS; i++) {
    int r0 = 0, r1 = 0;
    if (graphSegmentRows(program, samples, i, ymin, ymax, PLOT_HEIGHT, true, r0, r1)) {
      d.segments++;
      // What shows on screen: a steep segment leaving the plot is only visible up to its edge
      const int top = std::min(PLOT_HEIGHT - 1, std::max(0, r0));
      const int bottom = std::min(PLOT_HEIGHT - 1, std::max(0, r1));
      d.largestJump = std::max(d.largestJump, std::abs(bottom - top));
    }
  }
  return d;
}

static Drawn drawFitted(const char* expr, double center, double halfWidth) {
  openWindow(expr, center, halfWidth);
  double ymin = 0, ymax = 0;
  CHECK(graphFitY(samples, ymin, ymax));
  return drawAll(ymin, ymax);
}

static void checkNoLineAcross(const char* expr) {
  for (double halfWidth : {1.0, 5.0, 20.0}) {
    // Off-centre as well, so a pole isn't always in the same place within a column
    for (double center : {0.0, 0.123, 1.5707963, -3.3, 7.77}) {
      const Drawn d = drawFitted(expr, center, halfWidth);
      checks++;
      // A continuous branch can still be steep near its pole, but it can't span the plot in one column
      if (d.largestJump > PLOT_HEIGHT * 3 / 4) {
        failures++;
        fprintf(stderr, "  FAIL: '%s' +-%g around %g draws a %d px line\n", expr, halfWidth, center, d.largestJump);
      }
      CHECK(d.segments > 0);
    }
  }
}

// Ground truth: no drawn segment may contain a pole of tan, at (k + 1/2) pi
static void checkNoSegmentAcrossTanPoles(const char* expr) {
  for (double halfWidth : {1.0, 5.0, 20.0}) {
    for (double center : {0.0, 0.123, 1.5707963, -3.3, 7.77}) {
      openWindow(expr, center, halfWidth);
      double ymin = 0, ymax = 0;
      CHECK(graphFitY(samples, ymin, ymax));
      int drawn = 0;
      for (int i = 0; i + 1 < GRAPH_COLUMNS; i++) {
        int r0 = 0, r1 = 0;
        if (!graphSegmentRows(program, samples, i, ymin, ymax, PLOT_HEIGHT, true, r0, r1)) continue;
        drawn++;
        const double a = graphColumnX(samples, i) / M_PI - 0.5, b = graphColumnX(samples, i + 1) / M_PI - 0.5;
        checks++;
        if (floor(a) != floor(b)) {
          failures++;
          fprintf(stderr, "  FAIL: '%s' +-%g around %g draws across a pole at column %d\n", expr, halfWidth, center, i);
        }
      }
      CHECK(drawn > 0);
    }
  }
}

static void testDiscontinuities() {
  checkNoLineAcross("tan(x)");
  checkNoLineAcross("1/x");
  checkNoLineAcross("1/(x-1)");
  checkNoLineAcross("x/abs(x)");     // A jump, not a pole
  checkNoLineAcross("1/x^2");
  checkNoLineAcross("abs(x)/x+x/4");
  checkNoSegmentAcrossTanPoles("tan(x)");
  checkNoSegmentAcrossTanPoles("tan(x)+x");
  checkNoSegmentAcrossTanPoles("2tan(x)-3");

  // The break itself is found
  CHECK(compile("tan(x)"));
  CHECK(graphIsBreak(program, 1.5, 1.6));
  CHECK(graphIsBreak(program, 1.57, 1.5708));
  CHECK(!graphIsBreak(program, 1.0, 1.1));
  CHECK(compile("1/x"));
  CHECK(graphIsBreak(program, -0.01, 0.02));
  CHECK(!graphIsBreak(program, 0.01, 0.02));   // Very steep, but the same branch
  CHECK(compile("x/abs(x)"));
  CHECK(graphIsBreak(program, -0.01, 0.02));
  CHECK(!graphIsBreak(program, 0.01, 0.02));
  CHECK(compile("sqrt(x)"));
  CHECK(graphIsBreak(program, -1, 1));         // No point on one side

  // Steep and continuous stays joined: no gaps in x^9 or exp(x)
  const char* smooth[] = {"x^9", "x^2-3", "exp(x)", "100x", "x^3", "sin(x)", "sqrt(x+50)", "1000x^3"};
  for (const char* expr : smooth) {
    CHECK(compile(expr));
    for (double x : {-4.9, -1.0, 0.0, 0.5, 3.0, 4.9}) {
      checks++;
      if (graphIsBreak(program, x, x + 5.0 / 120)) {
        failures++;
        fprintf(stderr, "  FAIL: '%s' reported a break at %g\n", expr, x);
      }
    }
  }
  CHECK(compile("sin(50x)"));
  CHECK(!graphIsBreak(program, 0.3, 0.3 + 5.0 / 120));

  // Removable hole and a gap: the curve stops, it doesn't draw across
  const Drawn hole = drawFitted("x/x", 0, 5);
  CHECK(hole.segments == GRAPH_COLUMNS - 3);
  CHECK(hole.largestJump == 0);
  const Drawn half = drawFitted("sqrt(x)", 0, 5);
  CHECK(half.segments == GRAPH_COLUMNS / 2 - 1);

  // A jump is left out when there's time to check, and not drawn while there isn't
  // Off the column grid, or it would be a gap: x = 0 is always exactly on a column
  openWindow("(x-0.01)/abs(x-0.01)", 0, 5);
  int r0 = 0, r1 = 0, breaks = 0;
  for (int i = 0; i + 1 < GRAPH_COLUMNS; i++) {
    if (graphSegmentRows(program, samples, i, -1.2, 1.2, PLOT_HEIGHT, false, r0, r1)) CHECK(abs(r1 - r0) < 2);
    if (samples.segment[i] == GRAPH_SEGMENT_UNKNOWN &&
        !graphSegmentRows(program, samples, i, -1.2, 1.2, PLOT_HEIGHT, true, r0, r1) &&
        samples.segment[i] == GRAPH_SEGMENT_BROKEN) {
      breaks++;
    }
  }
  CHECK(breaks == 1);

  // Both ends far off screen on the same side: nothing to draw, and no classifying
  openWindow("x", 0, 5);
  CHECK(!graphSegmentRows(program, samples, 10, 100, 200, PLOT_HEIGHT, true, r0, r1));
  CHECK(!graphSegmentRows(program, samples, 10, -200, -100, PLOT_HEIGHT, true, r0, r1));
  // Outside what's computed
  graphSetWindow(samples, 0, 5);
  CHECK(!graphSegmentRows(program, samples, 10, -5, 5, PLOT_HEIGHT, true, r0, r1));
}

static void testPixels() {
  CHECK(graphPixelY(1, -1, 1, 117) == 0);
  CHECK(graphPixelY(-1, -1, 1, 117) == 116);
  CHECK(graphPixelY(0, -1, 1, 117) == 58);
  CHECK(graphPixelY(1e30, -1, 1, 117) == -GRAPH_FAR_PIXELS);
  CHECK(graphPixelY(-1e30, -1, 1, 117) == 117 + GRAPH_FAR_PIXELS);
  CHECK(graphPixelY(NAN, -1, 1, 117) == -GRAPH_FAR_PIXELS);  // Safe, even though NaN is never drawn
}

static void fit(const char* expr, double center, double halfWidth, double& ymin, double& ymax) {
  openWindow(expr, center, halfWidth);
  CHECK(graphFitY(samples, ymin, ymax));
  CHECK(ymax > ymin);
}

static void testFit() {
  double a, b;
  fit("sin(x)", 0, 5, a, b);
  CHECK(a < -0.99 && a > -1.3 && b > 0.99 && b < 1.3);
  // All of a smooth curve stays visible
  fit("x^2-3", 0, 5, a, b);
  CHECK(a < -2.99 && a > -6 && b > 21.9 && b < 25);
  fit("x", 0, 5, a, b);
  CHECK(a <= -5 && b >= 4.95);
  // Poles don't flatten everything else
  fit("tan(x)", 0, 5, a, b);
  CHECK(b > 1.5 && b < 80 && a < -1.5 && a > -80);
  fit("1/x", 0, 5, a, b);
  CHECK(b > 0.5 && b < 15 && a < -0.5 && a > -15);
  // Flat lines get a range to sit in
  fit("3", 0, 5, a, b);
  CHECK(a < 3 && b > 3 && b - a >= 1);
  fit("0", 0, 5, a, b);
  CHECK(a < 0 && b > 0);
  fit("x-x", 0, 5, a, b);
  CHECK(a < 0 && b > 0);
  fit("1000000", 0, 5, a, b);
  CHECK(a < 1000000 && b > 1000000);
  // Huge and tiny ranges work
  fit("x*10^20", 0, 5, a, b);
  CHECK(b > 4e20);
  fit("x/10^10", 0, 5, a, b);
  CHECK(b > 4e-10 && b < 1e-9);
  // Nothing to fit
  CHECK(compile("sqrt(-1)"));
  graphSetWindow(samples, 0, 5);
  computeAll();
  CHECK(!graphFitY(samples, a, b));
  // Nothing computed yet
  CHECK(compile("x"));
  graphSetWindow(samples, 0, 5);
  CHECK(!graphFitY(samples, a, b));

  // The range on screen moves smoothly and ignores small changes
  GraphYView view = {0, 0, 0, 0, false};
  graphYViewFollow(view, -1, 1);
  CHECK(view.ready && view.min == -1 && view.max == 1);
  graphYViewFollow(view, -1.1, 1.1);
  CHECK(view.targetMin == -1);
  graphYViewEase(view, 0.04f);
  CHECK(view.min == -1);
  graphYViewFollow(view, -10, 10);
  CHECK(view.targetMax == 10);
  graphYViewEase(view, 0.04f);
  CHECK(view.max > 1 && view.max < 10);
  for (int i = 0; i < 100; i++) graphYViewEase(view, 0.04f);
  CHECK(fabs(view.max - 10) < 1e-3);
  graphYViewEase(view, 10.0f);
  CHECK(view.max == 10);  // A long frame doesn't overshoot
}

static void checkTick(long k, double step, const char* expected) {
  char text[24];
  graphFormatTick(k, step, text, sizeof text);
  checks++;
  if (strcmp(text, expected) != 0) {
    failures++;
    fprintf(stderr, "  FAIL: tick %ld * %g gave '%s', expected '%s'\n", k, step, text, expected);
  }
}

static void checkValueText(double value, double scale, const char* expected) {
  char text[24];
  graphFormatValue(value, scale, text, sizeof text);
  checks++;
  if (strcmp(text, expected) != 0) {
    failures++;
    fprintf(stderr, "  FAIL: value %g gave '%s', expected '%s'\n", value, text, expected);
  }
}

static void testTicks() {
  CHECK(graphNiceStep(10, 5) == 2);
  CHECK(graphNiceStep(10, 6) == 2);
  CHECK(graphNiceStep(2, 5) == 0.5);
  CHECK(graphNiceStep(40, 5) == 10);
  CHECK(graphNiceStep(100, 5) == 20);
  CHECK(fabs(graphNiceStep(0.003, 3) - 0.001) < 1e-12);
  CHECK(graphNiceStep(0, 5) == 1);
  CHECK(graphNiceStep(-1, 5) == 1);
  CHECK(graphNiceStep(NAN, 5) == 1);
  CHECK(graphNiceStep(INFINITY, 5) == 1);
  CHECK(graphNiceStep(1, 0) == 1);
  for (double range : {1e-20, 3e-5, 0.7, 1.0, 9.99, 117.0, 5e8, 1e25}) {
    const double step = graphNiceStep(range, 5);
    CHECK(range / step <= 5.0001);
    CHECK(range / step >= 1);
  }

  checkTick(0, 2, "0");
  checkTick(0, 0.1, "0");
  checkTick(3, 2, "6");
  checkTick(-3, 2, "-6");
  checkTick(3, 0.1, "0.3");         // Not 0.30000000000000004
  checkTick(-3, 0.1, "-0.3");
  checkTick(7, 0.2, "1.4");
  checkTick(3, 0.5, "1.5");
  checkTick(2, 0.5, "1.0");         // Same number of decimals along the axis
  checkTick(3, 0.01, "0.03");
  checkTick(5, 1000, "5000");
  checkTick(2, 1000000, "2e6");
  checkTick(-3, 1e20, "-3e20");
  checkTick(3, 1e-5, "3e-5");

  checkValueText(0, 1, "0");
  checkValueText(-0.0, 1, "0");
  checkValueText(1.5, 2, "1.5");
  checkValueText(3.14159, 2, "3.14");
  checkValueText(-0.841, 2, "-0.841");
  checkValueText(1234.5, 2000, "1234");
  checkValueText(999.5, 2000, "1e3");
  checkValueText(2.5e7, 1e8, "2.5e7");
  checkValueText(NAN, 1, "--");
  checkValueText(INFINITY, 1, "--");
  checkValueText(1e-17, 2, "0");     // sin(pi) is 0 on this screen
  checkValueText(1e-17, 1e-16, "1e-17");  // ...but not if that's the scale of the graph
}

static void testStart() {
  double center = 99;
  CHECK(compile("sin(x)"));
  CHECK(graphFindStart(program, 5, center));
  CHECK(center == 0);
  CHECK(compile("1/x"));
  CHECK(graphFindStart(program, 5, center));
  CHECK(center == 0);
  CHECK(compile("sqrt(x)"));   // Half empty around 0: shift to where it is
  CHECK(graphFindStart(program, 5, center));
  CHECK(center > 1 && center < 4);
  CHECK(compile("ln(x)"));
  CHECK(graphFindStart(program, 5, center));
  CHECK(center > 1 && center < 4);
  CHECK(compile("sqrt(-x)"));
  CHECK(graphFindStart(program, 5, center));
  CHECK(center < -1 && center > -4);
  CHECK(compile("sqrt(x-10)")); // Found by looking wider
  CHECK(graphFindStart(program, 5, center));
  CHECK(center > 10 && center < 20);
  CHECK(compile("sqrt(x-50)"));
  CHECK(graphFindStart(program, 5, center));
  CHECK(center > 50 && center < 100);
  // Nothing to draw
  const char* nothing[] = {"sqrt(-1)", "1/0", "0/0", "ln(0)", "sqrt(x-1000)", "sqrt(-x^2)", "sqrt(-x^2-1)", "ln(-x^2)", "exp(1000)", "10^100"};
  for (const char* expr : nothing) {
    CHECK(compile(expr));
    checks++;
    if (graphFindStart(program, 5, center)) {
      failures++;
      fprintf(stderr, "  FAIL: '%s' should have nothing to draw\n", expr);
    }
  }
}

// Hostile programs run through everything a frame does
static void testHostileThroughTheView() {
  const char* expressions[] = {"x^x^x^x^x^x^x^x^x^x^x", "1/(x-x)", "tan(tan(tan(tan(x))))", "exp(exp(exp(x)))", "sin(1/x)",
                               "ln(ln(ln(x)))", "x^-x^-x", "1/sin(1000x)", "(-x)^x", "0^x", "tan(1000000x)", "10^100*x"};
  for (const char* expr : expressions) {
    CHECK(compile(expr));
    for (double halfWidth : {1.0, 5.0, 20.0}) {
      graphSetWindow(samples, 0, halfWidth);
      computeAll();
      double a = -1, b = 1;
      graphFitY(samples, a, b);
      CHECK(std::isfinite(a) && std::isfinite(b) && b > a);
      char text[24];
      for (int i = 0; i + 1 < GRAPH_COLUMNS; i++) {
        int r0 = 0, r1 = 0;
        if (graphSegmentRows(program, samples, i, a, b, PLOT_HEIGHT, true, r0, r1)) {
          CHECK(abs(r0) <= PLOT_HEIGHT + GRAPH_FAR_PIXELS && abs(r1) <= PLOT_HEIGHT + GRAPH_FAR_PIXELS);
        }
        graphFormatValue(samples.y[i], b - a, text, sizeof text);
        CHECK(strlen(text) < sizeof text);
      }
      const double step = graphNiceStep(b - a, 4);
      CHECK(std::isfinite(step) && step > 0);
      const long first = std::lround(std::ceil(a / step));
      graphFormatTick(first, step, text, sizeof text);
      CHECK(strlen(text) > 0);
    }
  }
}

int main() {
  testWindow();
  testPanningKeepsAndComputes();
  testDiscontinuities();
  testPixels();
  testFit();
  testTicks();
  testStart();
  testHostileThroughTheView();
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
