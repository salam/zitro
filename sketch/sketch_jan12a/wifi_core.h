// Finding WiFi in the background, without hardware: which known network to join first, and what to do
// next on each tick. wifi_link.ino feeds it the radio's state and carries out its commands.
// Tested by tests/wifi/run.sh.
//
// The radio joins one network at a time, so "all networks at once" means one scan that sees every
// network around, then joining the strongest known one. Before that, a fast join with the channel and
// router remembered from the last connection skips the scan when nothing has changed.
#pragma once
#include <stdint.h>
#include <string.h>

static const uint32_t WIFI_FAST_MS = 3000;       // fast join with the remembered channel and router
static const uint32_t WIFI_SCAN_MS = 5000;       // one scan normally takes about 1.6 s (13 channels, 120 ms each)
static const uint32_t WIFI_JOIN_MS = 8000;       // joining one network, address included
static const uint32_t WIFI_FAIL_AFTER_MS = 1000; // a "failed" status earlier than this may be left over from before
static const uint32_t WIFI_RETRY_MS = 30000;     // pause before searching again when nothing was found
static const int WIFI_MAX_CANDIDATES = 5;        // MAX_WIFI_NETWORKS

enum WifiPhase : uint8_t {
  WIFI_IDLE,  // radio off (sleep, setup mode) or not started yet
  WIFI_FAST,  // joining the network of the last connection directly
  WIFI_SCAN,  // one scan for all networks around
  WIFI_JOIN,  // joining the known networks the scan found, strongest first
  WIFI_UP,    // connected
  WIFI_NONE,  // searched, no known network answered; searches again after WIFI_RETRY_MS
};

enum WifiCmd : uint8_t {
  WIFI_CMD_NONE,
  WIFI_CMD_FAST,  // join the remembered network on its channel and router
  WIFI_CMD_SCAN,  // stop any join and scan all channels
  WIFI_CMD_JOIN,  // join candidate WifiMachine::next
  WIFI_CMD_UP,    // just connected: remember the network, set the clock
  WIFI_CMD_NONE_FOUND,
};

// What the radio says on this tick
struct WifiInputs {
  uint32_t now;
  bool connected;   // joined and has an address
  bool joinFailed;  // the current join gave up (network not found, wrong password)
  int scanResult;   // -1 still scanning, -2 failed for good, otherwise the number of candidates found
  bool awake;       // screen on: only then search again after a failed search or a lost connection
};

struct WifiMachine {
  WifiPhase phase = WIFI_IDLE;
  uint32_t since = 0;    // when the current phase (or join attempt) began
  uint8_t next = 0;      // candidate being joined in WIFI_JOIN
  uint8_t candidates = 0;
  bool canFast = false;  // a remembered network to join directly
};

inline bool wifiSearching(const WifiMachine& m) {
  return m.phase == WIFI_FAST || m.phase == WIFI_SCAN || m.phase == WIFI_JOIN;
}

inline WifiCmd wifiEnter(WifiMachine& m, WifiPhase phase, uint32_t now) {
  m.phase = phase;
  m.since = now;
  switch (phase) {
    case WIFI_FAST: return WIFI_CMD_FAST;
    case WIFI_SCAN: return WIFI_CMD_SCAN;
    case WIFI_JOIN: m.next = 0; return WIFI_CMD_JOIN;
    case WIFI_UP: m.canFast = true; return WIFI_CMD_UP;  // the driver remembers this network
    case WIFI_NONE: return WIFI_CMD_NONE_FOUND;
    default: return WIFI_CMD_NONE;
  }
}

// Start a search: the remembered network first if there is one, else a scan
inline WifiCmd wifiStart(WifiMachine& m, uint32_t now, bool canFast) {
  m.canFast = canFast;
  m.candidates = 0;
  return wifiEnter(m, canFast ? WIFI_FAST : WIFI_SCAN, now);
}

// Something needs the network now (a question was recorded): search at once instead of waiting for
// the next retry. A search already under way just goes on.
inline WifiCmd wifiKick(WifiMachine& m, uint32_t now, bool canFast) {
  if (m.phase == WIFI_IDLE || m.phase == WIFI_NONE) return wifiStart(m, now, canFast);
  return WIFI_CMD_NONE;
}

inline void wifiStop(WifiMachine& m) {
  m.phase = WIFI_IDLE;
  m.candidates = 0;
}

inline WifiCmd wifiTick(WifiMachine& m, const WifiInputs& in) {
  const uint32_t elapsed = in.now - m.since;
  const bool gaveUp = in.joinFailed && elapsed >= WIFI_FAIL_AFTER_MS;
  switch (m.phase) {
    case WIFI_FAST:
      if (in.connected) return wifiEnter(m, WIFI_UP, in.now);
      if (gaveUp || elapsed >= WIFI_FAST_MS) return wifiEnter(m, WIFI_SCAN, in.now);
      return WIFI_CMD_NONE;
    case WIFI_SCAN:
      // The WiFi library retries the very first join by itself, and that may still get through
      if (in.connected) return wifiEnter(m, WIFI_UP, in.now);
      if (in.scanResult > 0) {
        m.candidates = in.scanResult > WIFI_MAX_CANDIDATES ? WIFI_MAX_CANDIDATES : (uint8_t)in.scanResult;
        return wifiEnter(m, WIFI_JOIN, in.now);
      }
      if (in.scanResult == 0 || in.scanResult == -2 || elapsed >= WIFI_SCAN_MS) return wifiEnter(m, WIFI_NONE, in.now);
      return WIFI_CMD_NONE;
    case WIFI_JOIN:
      if (in.connected) return wifiEnter(m, WIFI_UP, in.now);
      if (gaveUp || elapsed >= WIFI_JOIN_MS) {
        if (m.next + 1 < m.candidates) {
          m.next++;
          m.since = in.now;
          return WIFI_CMD_JOIN;
        }
        return wifiEnter(m, WIFI_NONE, in.now);
      }
      return WIFI_CMD_NONE;
    case WIFI_UP:
      // Lost: the same network first (it may only have been a blip), then a scan
      if (!in.connected && in.awake) return wifiStart(m, in.now, m.canFast);
      return WIFI_CMD_NONE;
    case WIFI_NONE:
      if (in.awake && elapsed >= WIFI_RETRY_MS) return wifiStart(m, in.now, false);
      return WIFI_CMD_NONE;
    default:
      return WIFI_CMD_NONE;
  }
}

// One network the scan saw
struct WifiSeen {
  const char* ssid;  // empty for a hidden network
  int rssi;
};

// Order in which to join: each stored network the scan saw, strongest first (a network seen through
// several routers counts with its strongest), stored order on a tie. A stored network the scan didn't
// see could be hidden, so it is only tried when the scan saw a hidden network, after the seen ones.
// pick[i]: the scan entry to join (its channel and router), or -1 to join by name alone.
// Returns how many candidates there are, at most WIFI_MAX_CANDIDATES.
inline int wifiRankCandidates(const char* const* stored, int storedCount, const WifiSeen* seen, int seenCount,
                              int* storedOut, int* pick) {
  if (storedCount > WIFI_MAX_CANDIDATES) storedCount = WIFI_MAX_CANDIDATES;
  int best[WIFI_MAX_CANDIDATES];
  bool hiddenAround = false;
  for (int s = 0; s < storedCount; s++) best[s] = -1;
  for (int i = 0; i < seenCount; i++) {
    if (!seen[i].ssid || !seen[i].ssid[0]) {
      hiddenAround = true;
      continue;
    }
    for (int s = 0; s < storedCount; s++) {
      if (stored[s] && stored[s][0] && strcmp(stored[s], seen[i].ssid) == 0 &&
          (best[s] < 0 || seen[i].rssi > seen[best[s]].rssi)) {
        best[s] = i;
      }
    }
  }
  int count = 0;
  bool taken[WIFI_MAX_CANDIDATES] = {false};
  for (;;) {
    int choice = -1;
    for (int s = 0; s < storedCount; s++) {
      if (taken[s] || best[s] < 0) continue;
      if (choice < 0 || seen[best[s]].rssi > seen[best[choice]].rssi) choice = s;
    }
    if (choice < 0) break;
    taken[choice] = true;
    storedOut[count] = choice;
    pick[count] = best[choice];
    count++;
  }
  if (hiddenAround) {
    for (int s = 0; s < storedCount; s++) {
      if (best[s] >= 0 || !stored[s] || !stored[s][0]) continue;
      storedOut[count] = s;
      pick[count] = -1;
      count++;
    }
  }
  return count;
}

// The searching icon: grey to white and back, once every WIFI_PULSE_MS. Grey and still once a search
// found nothing.
static const uint32_t WIFI_PULSE_MS = 1400;
static const uint8_t WIFI_ICON_GREY = 90;

inline uint8_t wifiPulseLevel(uint32_t now) {
  uint32_t t = now % WIFI_PULSE_MS;
  uint32_t half = WIFI_PULSE_MS / 2;
  uint32_t up = t < half ? t : WIFI_PULSE_MS - t;  // 0 .. half .. 0, a triangle
  return (uint8_t)(WIFI_ICON_GREY + (255 - WIFI_ICON_GREY) * up / half);
}

// How long a caller waits for the connection before giving up
static const uint32_t WIFI_CONNECT_WAIT_MS = 30000;   // a request, the setup screen's retry
static const uint32_t WIFI_QUESTION_WAIT_MS = 25000;  // a recorded question, before the setup screen opens
