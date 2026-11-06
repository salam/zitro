// Small check macros shared by the physics and solid host tests.
#pragma once
#include <cmath>
#include <cstdio>
static int failures = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { failures++; \
  fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define NEAR(a, b, e) do { checks++; if (!(fabs((double)(a) - (double)(b)) <= (e))) { failures++; \
  fprintf(stderr, "  FAIL %s:%d: %s = %g, expected %g +- %g\n", __FILE__, __LINE__, #a, (double)(a), (double)(b), (double)(e)); } } while (0)
static int finish(const char* name) {
  printf("%s: %d checks, %d failures\n", name, checks, failures);
  return failures ? 1 : 0;
}
