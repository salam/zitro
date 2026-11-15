// The background WiFi search (sketch/sketch_jan12a/wifi_core.h): join order, fallbacks, retries, pulse.
#include <cstdio>

#include "../../sketch/sketch_jan12a/wifi_core.h"

static int failures = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { failures++; \
  fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static WifiInputs at(uint32_t now) {
  WifiInputs in;
  in.now = now;
  in.connected = false;
  in.joinFailed = false;
  in.scanResult = -1;
  in.awake = true;
  return in;
}

static void testRanking() {
  const char* stored[] = {"Home", "Office", "Grandma", "Phone"};
  // Strongest known network first, whatever the stored order
  {
    WifiSeen seen[] = {{"Neighbour", -40}, {"Office", -70}, {"Home", -60}, {"Grandma", -80}};
    int s[5], p[5];
    int n = wifiRankCandidates(stored, 4, seen, 4, s, p);
    CHECK(n == 3);
    CHECK(s[0] == 0 && p[0] == 2);  // Home -60
    CHECK(s[1] == 1 && p[1] == 1);  // Office -70
    CHECK(s[2] == 2 && p[2] == 3);  // Grandma -80
  }
  // A network seen through two routers counts once, with its stronger router
  {
    WifiSeen seen[] = {{"Home", -85}, {"Office", -70}, {"Home", -50}};
    int s[5], p[5];
    int n = wifiRankCandidates(stored, 4, seen, 3, s, p);
    CHECK(n == 2);
    CHECK(s[0] == 0 && p[0] == 2);
    CHECK(s[1] == 1 && p[1] == 1);
  }
  // Equal strength: stored order decides
  {
    WifiSeen seen[] = {{"Phone", -60}, {"Office", -60}};
    int s[5], p[5];
    int n = wifiRankCandidates(stored, 4, seen, 2, s, p);
    CHECK(n == 2);
    CHECK(s[0] == 1 && s[1] == 3);
  }
  // Nothing known around: no candidates, so no time is spent on networks that aren't there
  {
    WifiSeen seen[] = {{"Neighbour", -40}, {"Cafe", -60}};
    int s[5], p[5];
    CHECK(wifiRankCandidates(stored, 4, seen, 2, s, p) == 0);
  }
  // A hidden network around: the stored networks the scan didn't see are tried by name, after the seen ones
  {
    WifiSeen seen[] = {{"", -55}, {"Office", -70}};
    int s[5], p[5];
    int n = wifiRankCandidates(stored, 4, seen, 2, s, p);
    CHECK(n == 4);
    CHECK(s[0] == 1 && p[0] == 1);
    CHECK(s[1] == 0 && p[1] == -1);
    CHECK(s[2] == 2 && p[2] == -1);
    CHECK(s[3] == 3 && p[3] == -1);
  }
  // Empty stored names never match a hidden network's empty name
  {
    const char* withEmpty[] = {"", "Home"};
    WifiSeen seen[] = {{"", -40}};
    int s[5], p[5];
    int n = wifiRankCandidates(withEmpty, 2, seen, 1, s, p);
    CHECK(n == 1);
    CHECK(s[0] == 1 && p[0] == -1);
  }
}

static void testFastJoin() {
  WifiMachine m;
  CHECK(wifiStart(m, 0, true) == WIFI_CMD_FAST);
  CHECK(wifiSearching(m));
  WifiInputs in = at(800);
  in.connected = true;
  CHECK(wifiTick(m, in) == WIFI_CMD_UP);
  CHECK(m.phase == WIFI_UP);
  CHECK(!wifiSearching(m));
}

static void testFastFailsThenScan() {
  WifiMachine m;
  wifiStart(m, 0, true);
  // A "failed" status right after starting may be left over from before: ignored
  WifiInputs in = at(200);
  in.joinFailed = true;
  CHECK(wifiTick(m, in) == WIFI_CMD_NONE);
  CHECK(m.phase == WIFI_FAST);
  // Later it counts, and the scan starts without waiting out the fast join
  in.now = 1200;
  CHECK(wifiTick(m, in) == WIFI_CMD_SCAN);
  CHECK(m.phase == WIFI_SCAN);
}

static void testFastTimesOut() {
  WifiMachine m;
  wifiStart(m, 0, true);
  CHECK(wifiTick(m, at(WIFI_FAST_MS - 1)) == WIFI_CMD_NONE);
  CHECK(wifiTick(m, at(WIFI_FAST_MS)) == WIFI_CMD_SCAN);
}

static void testNoRememberedNetworkScansAtOnce() {
  WifiMachine m;
  CHECK(wifiStart(m, 0, false) == WIFI_CMD_SCAN);
  CHECK(m.phase == WIFI_SCAN);
}

static void testScanThenJoinInOrder() {
  WifiMachine m;
  wifiStart(m, 0, false);
  WifiInputs in = at(1500);
  in.scanResult = 3;
  CHECK(wifiTick(m, in) == WIFI_CMD_JOIN);
  CHECK(m.phase == WIFI_JOIN && m.next == 0 && m.candidates == 3);
  // First candidate refuses (wrong password): next one at once
  in = at(3000);
  in.joinFailed = true;
  CHECK(wifiTick(m, in) == WIFI_CMD_JOIN);
  CHECK(m.next == 1);
  // Second candidate times out
  CHECK(wifiTick(m, at(3000 + WIFI_JOIN_MS - 1)) == WIFI_CMD_NONE);
  CHECK(wifiTick(m, at(3000 + WIFI_JOIN_MS)) == WIFI_CMD_JOIN);
  CHECK(m.next == 2);
  // Third one connects
  in = at(3000 + WIFI_JOIN_MS + 2000);
  in.connected = true;
  CHECK(wifiTick(m, in) == WIFI_CMD_UP);
  CHECK(m.phase == WIFI_UP);
  CHECK(m.canFast);  // remembered for the next wake
}

static void testAllCandidatesFail() {
  WifiMachine m;
  wifiStart(m, 0, false);
  WifiInputs in = at(1500);
  in.scanResult = 1;
  wifiTick(m, in);
  CHECK(wifiTick(m, at(1500 + WIFI_JOIN_MS)) == WIFI_CMD_NONE_FOUND);
  CHECK(m.phase == WIFI_NONE);
  CHECK(!wifiSearching(m));
}

static void testScanFindsNothingKnown() {
  WifiMachine m;
  wifiStart(m, 0, false);
  WifiInputs in = at(1600);
  in.scanResult = 0;
  CHECK(wifiTick(m, in) == WIFI_CMD_NONE_FOUND);
  CHECK(m.phase == WIFI_NONE);
}

static void testScanFailsOrHangs() {
  WifiMachine m;
  wifiStart(m, 0, false);
  WifiInputs in = at(100);
  in.scanResult = -2;
  CHECK(wifiTick(m, in) == WIFI_CMD_NONE_FOUND);

  wifiStart(m, 0, false);
  CHECK(wifiTick(m, at(WIFI_SCAN_MS - 1)) == WIFI_CMD_NONE);
  CHECK(wifiTick(m, at(WIFI_SCAN_MS)) == WIFI_CMD_NONE_FOUND);
}

static void testLateJoinDuringScan() {
  WifiMachine m;
  wifiStart(m, 0, true);
  CHECK(wifiTick(m, at(WIFI_FAST_MS)) == WIFI_CMD_SCAN);
  WifiInputs in = at(WIFI_FAST_MS + 300);
  in.connected = true;
  CHECK(wifiTick(m, in) == WIFI_CMD_UP);
  CHECK(m.phase == WIFI_UP);
}

static void testRetryAfterNothingFound() {
  WifiMachine m;
  wifiStart(m, 0, false);
  WifiInputs in = at(1000);
  in.scanResult = 0;
  wifiTick(m, in);  // NONE since 1000
  CHECK(wifiTick(m, at(1000 + WIFI_RETRY_MS - 1)) == WIFI_CMD_NONE);
  // Not while the screen is off
  in = at(1000 + WIFI_RETRY_MS);
  in.awake = false;
  CHECK(wifiTick(m, in) == WIFI_CMD_NONE);
  // A retry scans: the remembered network was just tried
  CHECK(wifiTick(m, at(1000 + WIFI_RETRY_MS)) == WIFI_CMD_SCAN);
}

static void testKick() {
  WifiMachine m;
  wifiStart(m, 0, false);
  WifiInputs in = at(1000);
  in.scanResult = 0;
  wifiTick(m, in);
  CHECK(m.phase == WIFI_NONE);
  // A recorded question doesn't wait for the 30 s retry
  CHECK(wifiKick(m, 2000, true) == WIFI_CMD_FAST);
  CHECK(m.phase == WIFI_FAST);
  // A search under way just goes on
  CHECK(wifiKick(m, 2100, true) == WIFI_CMD_NONE);
  CHECK(m.phase == WIFI_FAST && m.since == 2000);
  // From idle (radio was off)
  wifiStop(m);
  CHECK(wifiKick(m, 3000, false) == WIFI_CMD_SCAN);
}

static void testLostConnection() {
  WifiMachine m;
  wifiStart(m, 0, false);
  WifiInputs in = at(1000);
  in.scanResult = 1;
  wifiTick(m, in);
  in = at(2000);
  in.connected = true;
  wifiTick(m, in);
  CHECK(m.phase == WIFI_UP);
  // Still connected: nothing to do
  CHECK(wifiTick(m, in) == WIFI_CMD_NONE);
  // Lost while the screen is off: left alone
  in = at(5000);
  in.awake = false;
  CHECK(wifiTick(m, in) == WIFI_CMD_NONE);
  CHECK(m.phase == WIFI_UP);
  // Lost while awake: the same network first
  CHECK(wifiTick(m, at(6000)) == WIFI_CMD_FAST);
  CHECK(wifiSearching(m));
}

static void testStop() {
  WifiMachine m;
  wifiStart(m, 0, true);
  wifiStop(m);
  CHECK(m.phase == WIFI_IDLE);
  CHECK(!wifiSearching(m));
  CHECK(wifiTick(m, at(100000)) == WIFI_CMD_NONE);
}

static void testPulse() {
  CHECK(wifiPulseLevel(0) == WIFI_ICON_GREY);
  CHECK(wifiPulseLevel(WIFI_PULSE_MS / 2) == 255);
  CHECK(wifiPulseLevel(WIFI_PULSE_MS) == WIFI_ICON_GREY);
  uint8_t low = 255, high = 0;
  for (uint32_t t = 0; t < 3 * WIFI_PULSE_MS; t += 7) {
    uint8_t v = wifiPulseLevel(t);
    if (v < low) low = v;
    if (v > high) high = v;
  }
  CHECK(low == WIFI_ICON_GREY);
  CHECK(high == 255);
  // Rises in the first half, falls in the second
  CHECK(wifiPulseLevel(200) < wifiPulseLevel(400));
  CHECK(wifiPulseLevel(1000) > wifiPulseLevel(1200));
  // millis() wraps after 49 days: still in range
  CHECK(wifiPulseLevel(0xFFFFFFFFu) >= WIFI_ICON_GREY);
}

// The WiFi page's bar: steps for the remembered network, the scan and each network found, creeping
// within a step but never reaching its end, never going back, full once connected
static void testProgressSteps() {
  WifiMachine m;
  wifiStart(m, 0, true);
  uint16_t p = wifiProgress(m, 0, 0);
  CHECK(p == 0);
  uint16_t q = wifiProgress(m, 500, p);
  CHECK(q > p && q < WIFI_BAR_SCAN_AFTER_FAST);
  p = q;
  // Creeps, but stays inside the step however long it takes
  q = wifiProgress(m, WIFI_FAST_MS - 1, p);
  CHECK(q > p && q < WIFI_BAR_SCAN_AFTER_FAST);
  p = q;
  // Fast join timed out: the scan's step
  wifiTick(m, at(WIFI_FAST_MS));
  CHECK(m.phase == WIFI_SCAN);
  q = wifiProgress(m, WIFI_FAST_MS, p);
  CHECK(q >= WIFI_BAR_SCAN_AFTER_FAST && q > p);
  p = q;
  q = wifiProgress(m, WIFI_FAST_MS + WIFI_SCAN_MS - 1, p);
  CHECK(q > p && q < WIFI_BAR_JOIN_AFTER_FAST);
  p = q;
  // Two networks found: half of what's left for each
  WifiInputs in = at(WIFI_FAST_MS + 1600);
  in.scanResult = 2;
  wifiTick(m, in);
  CHECK(m.phase == WIFI_JOIN);
  const uint32_t joinAt = WIFI_FAST_MS + 1600;
  const uint16_t half = WIFI_BAR_JOIN_AFTER_FAST + (WIFI_BAR_FULL - WIFI_BAR_JOIN_AFTER_FAST) / 2;
  q = wifiProgress(m, joinAt, p);
  CHECK(q >= WIFI_BAR_JOIN_AFTER_FAST && q > p);
  p = q;
  q = wifiProgress(m, joinAt + WIFI_JOIN_MS - 1, p);
  CHECK(q > p && q < half);
  p = q;
  // The first one timed out: the second one's slice
  wifiTick(m, at(joinAt + WIFI_JOIN_MS));
  CHECK(m.next == 1);
  q = wifiProgress(m, joinAt + WIFI_JOIN_MS, p);
  CHECK(q >= half && q < WIFI_BAR_FULL);
  p = q;
  q = wifiProgress(m, joinAt + 2 * WIFI_JOIN_MS - 1, p);
  CHECK(q > p && q < WIFI_BAR_FULL);
  // Connected: full
  in = at(joinAt + WIFI_JOIN_MS + 2000);
  in.connected = true;
  wifiTick(m, in);
  CHECK(wifiProgress(m, joinAt + WIFI_JOIN_MS + 2000, q) == WIFI_BAR_FULL);
}

static void testProgressWithoutFastJoin() {
  // Nothing remembered: the scan starts at the left end and gets more room
  WifiMachine m;
  wifiStart(m, 0, false);
  CHECK(wifiProgress(m, 0, 0) == 0);
  uint16_t p = wifiProgress(m, WIFI_SCAN_MS - 1, 0);
  CHECK(p > 0 && p < WIFI_BAR_JOIN);
  CHECK(p > WIFI_BAR_JOIN_AFTER_FAST - WIFI_BAR_SCAN_AFTER_FAST);
  WifiInputs in = at(1500);
  in.scanResult = 1;
  wifiTick(m, in);
  uint16_t q = wifiProgress(m, 1500, 0);
  CHECK(q == WIFI_BAR_JOIN);
  // One network: all that's left is its slice
  q = wifiProgress(m, 1500 + WIFI_JOIN_MS - 1, q);
  CHECK(q > WIFI_BAR_JOIN && q < WIFI_BAR_FULL);
}

static void testProgressHoldsAndNeverGoesBack() {
  // Nothing found: the bar stays where it got to
  WifiMachine m;
  wifiStart(m, 0, false);
  uint16_t p = wifiProgress(m, 1000, 0);
  WifiInputs in = at(1600);
  in.scanResult = 0;
  wifiTick(m, in);
  CHECK(m.phase == WIFI_NONE);
  CHECK(wifiProgress(m, 1600, p) == p);
  CHECK(wifiProgress(m, 60000, p) == p);
  // A search started again (a retry) doesn't pull a bar back that got further before
  wifiStart(m, 70000, false);
  CHECK(wifiProgress(m, 70000, 400) == 400);
  // Idle: nothing to show, nothing taken away
  wifiStop(m);
  CHECK(wifiProgress(m, 80000, 123) == 123);
  // millis() wrapping during a step: still inside it
  wifiStart(m, 0xFFFFFF00u, true);
  uint16_t w = wifiProgress(m, 0x100u, 0);
  CHECK(w > 0 && w < WIFI_BAR_SCAN_AFTER_FAST);
}

int main() {
  testRanking();
  testFastJoin();
  testFastFailsThenScan();
  testFastTimesOut();
  testNoRememberedNetworkScansAtOnce();
  testScanThenJoinInOrder();
  testAllCandidatesFail();
  testScanFindsNothingKnown();
  testScanFailsOrHangs();
  testLateJoinDuringScan();
  testRetryAfterNothingFound();
  testKick();
  testLostConnection();
  testStop();
  testPulse();
  testProgressSteps();
  testProgressWithoutFastJoin();
  testProgressHoldsAndNeverGoesBack();
  printf("wifi: %d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
