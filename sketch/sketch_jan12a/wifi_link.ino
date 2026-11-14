// The WiFi connection, found in the background (order and timings in wifi_core.h). Nothing here
// waits except wifiWaitForLink(): the screens and recording go on while Zitro searches, and a corner
// icon pulses grey to white on the home and recording screens until the connection stands.
// wifiBackgroundTick() runs from loop() and from the recording loop.

static WifiMachine wifiMachine;

// The network of the last connection, kept through light and deep sleep (lost on power-off).
// Matched by name against the stored networks, as the setup page may have changed them since.
RTC_DATA_ATTR static char wifiLastSsid[33];
RTC_DATA_ATTR static uint8_t wifiLastBssid[6];
RTC_DATA_ATTR static int32_t wifiLastChannel = 0;

struct WifiCandidate {
  int stored;        // index in wifiNetworks
  int32_t channel;   // 0: join by name alone (could be a hidden network)
  uint8_t bssid[6];
};
static WifiCandidate wifiCandidates[WIFI_MAX_CANDIDATES];
static int wifiJoining = -1;  // wifiNetworks index being joined
static volatile bool wifiJoinFailed = false;
static bool wifiScanStarted = false;
static unsigned long wifiScanTriedAt = 0;

static const uint32_t WIFI_SCAN_MS_PER_CHANNEL = 120;  // 13 channels: about 1.6 s for one scan

// The searching icon
static bool wifiIconScreenOk = false;  // the screen shown has room for it (home, recording, waiting)
static bool wifiIconDrawn = false;
static unsigned long wifiIconLastDraw = 0;
static int wifiIconX = 0, wifiIconY = 0;
uint8_t wifiIconTourLevel = 0;  // the screenshot tour shows the icon at this brightness (0: off)
static const int WIFI_ICON_W = 17;
static const int WIFI_ICON_H = 14;

// A disconnect reported while joining: the network isn't there or refused. Our own disconnect
// (ASSOC_LEAVE) doesn't count. Runs in the network event task.
static void wifiOnDisconnect(arduino_event_t* event) {
  if (event->event_info.wifi_sta_disconnected.reason == WIFI_REASON_ASSOC_LEAVE) return;
  wifiJoinFailed = true;
}

static int wifiStoredIndex(const char* ssid) {
  if (!ssid || !ssid[0]) return -1;
  for (int i = 0; i < wifiNetworkCount; i++) {
    if (wifiNetworks[i].ssid == ssid) return i;
  }
  return -1;
}

static bool wifiCanFast() {
  return wifiLastChannel > 0 && wifiStoredIndex(wifiLastSsid) >= 0;
}

static void wifiRadioOn() {
  static bool handlerSet = false;
  if (!handlerSet) {
    WiFi.onEvent(wifiOnDisconnect, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    handlerSet = true;
  }
  WiFi.persistent(false);         // Networks live in Preferences, not in the WiFi driver's storage
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);   // wifiMachine decides when and where to join again
}

static void wifiBeginJoin(int stored, int32_t channel, const uint8_t* bssid) {
  WiFi.disconnect(false);  // ends a join still under way; the radio stays on
  wifiJoinFailed = false;
  wifiJoining = stored;
  Serial.printf("WiFi: joining '%s'%s\n", wifiNetworks[stored].ssid.c_str(),
                channel > 0 ? (" on channel " + String(channel)).c_str() : " by name");
  WiFi.begin(wifiNetworks[stored].ssid.c_str(), wifiNetworks[stored].password.c_str(), channel, bssid);
}

// Turns a finished scan into wifiCandidates and returns how many there are
static int wifiCollectCandidates(int found) {
  const int seenMax = 40;
  if (found > seenMax) found = seenMax;
  String names[seenMax];
  WifiSeen seen[seenMax];
  for (int i = 0; i < found; i++) {
    names[i] = WiFi.SSID(i);
    seen[i].ssid = names[i].c_str();
    seen[i].rssi = WiFi.RSSI(i);
  }
  const char* stored[MAX_WIFI_NETWORKS];
  for (int s = 0; s < wifiNetworkCount; s++) stored[s] = wifiNetworks[s].ssid.c_str();
  int storedOut[WIFI_MAX_CANDIDATES];
  int pick[WIFI_MAX_CANDIDATES];
  int count = wifiRankCandidates(stored, wifiNetworkCount, seen, found, storedOut, pick);
  for (int c = 0; c < count; c++) {
    wifiCandidates[c].stored = storedOut[c];
    if (pick[c] >= 0) {
      wifiCandidates[c].channel = WiFi.channel(pick[c]);
      memcpy(wifiCandidates[c].bssid, WiFi.BSSID(pick[c]), 6);
      Serial.printf("WiFi: found '%s' (%d dBm, channel %d)\n", seen[pick[c]].ssid, seen[pick[c]].rssi,
                    (int)wifiCandidates[c].channel);
    } else {
      wifiCandidates[c].channel = 0;
      Serial.printf("WiFi: '%s' not seen, will try it by name\n", stored[storedOut[c]]);
    }
  }
  Serial.printf("WiFi: scan saw %d networks, %d known\n", found, count);
  WiFi.scanDelete();
  return count;
}

static void wifiOnUp() {
  Serial.printf("WiFi: connected to '%s', IP %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
  strncpy(wifiLastSsid, WiFi.SSID().c_str(), sizeof(wifiLastSsid) - 1);
  wifiLastSsid[sizeof(wifiLastSsid) - 1] = '\0';
  const uint8_t* bssid = WiFi.BSSID();
  if (bssid) {
    memcpy(wifiLastBssid, bssid, 6);
    wifiLastChannel = WiFi.channel();
  } else {
    wifiLastChannel = 0;
  }
  if (wifiJoining >= 0 && wifiJoining < wifiNetworkCount) {
    savedSSID = wifiNetworks[wifiJoining].ssid;
    savedPass = wifiNetworks[wifiJoining].password;
    moveNetworkToFront(wifiJoining);  // Tried first by name next time the remembered one is gone
  }
  wifiJoining = -1;
  // NTP, in the home city's time zone with its real daylight saving rules
  startNtpWithHomeTimeZone();
}

// A join still under way keeps a scan from starting, so it is ended first. The WiFi library also
// retries the very first failed join by itself; wifiLinkTick() tries the scan again until it starts.
static void wifiStartScan() {
  WiFi.disconnect(false);
  wifiScanTriedAt = millis();
  wifiScanStarted = WiFi.scanNetworks(true, true, false, WIFI_SCAN_MS_PER_CHANNEL) != WIFI_SCAN_FAILED;
}

static void wifiRun(WifiCmd cmd) {
  switch (cmd) {
    case WIFI_CMD_FAST: {
      int stored = wifiStoredIndex(wifiLastSsid);
      if (stored < 0 || wifiLastChannel <= 0) {  // forgotten meanwhile: scan instead
        wifiRun(wifiEnter(wifiMachine, WIFI_SCAN, millis()));
        return;
      }
      wifiBeginJoin(stored, wifiLastChannel, wifiLastBssid);
      break;
    }
    case WIFI_CMD_SCAN:
      Serial.println("WiFi: scanning all channels");
      wifiStartScan();
      break;
    case WIFI_CMD_JOIN: {
      const WifiCandidate& c = wifiCandidates[wifiMachine.next];
      wifiBeginJoin(c.stored, c.channel, c.channel > 0 ? c.bssid : nullptr);
      break;
    }
    case WIFI_CMD_UP:
      wifiOnUp();
      break;
    case WIFI_CMD_NONE_FOUND:
      Serial.printf("WiFi: no known network answered, searching again in %lu s\n",
                    (unsigned long)(WIFI_RETRY_MS / 1000));
      WiFi.disconnect(false);
      wifiJoining = -1;
      break;
    default:
      break;
  }
}

static void wifiLinkTick() {
  if (apMode || wifiMachine.phase == WIFI_IDLE) return;
  WifiInputs in;
  in.now = millis();
  in.connected = WiFi.status() == WL_CONNECTED;
  in.joinFailed = wifiJoinFailed;
  in.scanResult = -1;
  if (wifiMachine.phase == WIFI_SCAN) {
    const int16_t n = wifiScanStarted ? WiFi.scanComplete() : WIFI_SCAN_FAILED;
    if (n >= 0) {
      in.scanResult = wifiCollectCandidates(n);
    } else if (n == WIFI_SCAN_FAILED && millis() - wifiScanTriedAt >= 250) {
      wifiStartScan();  // didn't start or broke off; WIFI_SCAN_MS ends the tries
    }
  }
  in.awake = !displaySleeping && currentPowerState == POWER_ACTIVE;
  wifiRun(wifiTick(wifiMachine, in));
}

// Start looking for a known network (power-on, wake): the last one directly, else one scan
void wifiLinkStart() {
  if (apMode) return;
  if (wifiNetworkCount == 0) loadWiFiCredentials();
  if (wifiNetworkCount == 0) return;
  wifiRadioOn();
  wifiRun(wifiStart(wifiMachine, millis(), wifiCanFast()));
}

// The radio goes off (sleep, setup mode): forget the search
void wifiLinkStop() {
  wifiStop(wifiMachine);
  wifiJoining = -1;
  wifiIconScreenOk = false;
  wifiIconDrawn = false;
  WiFi.scanDelete();
}

bool wifiLinkUp() {
  return wifiMachine.phase == WIFI_UP && WiFi.status() == WL_CONNECTED;
}

// =============== THE SEARCHING ICON ===============

static bool wifiIconWanted() {
  if (wifiIconTourLevel > 0) return true;
  return !apMode && wifiMachine.phase != WIFI_IDLE && wifiMachine.phase != WIFI_UP;
}

// Grey to white while searching, grey and still once a search found nothing
static uint8_t wifiIconLevel() {
  if (wifiIconTourLevel > 0) return wifiIconTourLevel;
  return wifiSearching(wifiMachine) ? wifiPulseLevel(millis()) : WIFI_ICON_GREY;
}

// Three arcs over a dot, in a WIFI_ICON_W x WIFI_ICON_H box at x, y
static void drawWifiSymbol(M5Canvas& c, int x, int y, uint8_t level) {
  const uint16_t color = c.color565(level, level, level);
  const int cx = x + WIFI_ICON_W / 2;
  const int cy = y + WIFI_ICON_H - 2;
  c.fillCircle(cx, cy, 1, color);
  c.fillArc(cx, cy, 3, 4, 225, 315, color);
  c.fillArc(cx, cy, 7, 8, 225, 315, color);
  c.fillArc(cx, cy, 11, 12, 225, 315, color);
}

// Top right; left of the battery when that shows (below 35 %, see drawBatteryIndicator)
static void wifiIconPlace() {
  const bool battery = M5.Power.getBatteryLevel() < 35;
  wifiIconX = battery ? WIDTH - 25 - 6 - WIFI_ICON_W : WIDTH - 4 - WIFI_ICON_W;
  wifiIconY = 3;
}

// Called by the home and recording screens before they push their canvas
void wifiIconDrawInto(M5Canvas& canvas) {
  wifiIconPlace();
  wifiIconDrawn = wifiIconWanted();
  if (wifiIconDrawn) drawWifiSymbol(canvas, wifiIconX, wifiIconY, wifiIconLevel());
  wifiIconLastDraw = millis();
}

// Called right after captureCanvas() by the screens that have room for the icon
void wifiIconScreenAllows() {
  wifiIconScreenOk = true;
}

// Every full-screen draw passes here (captureCanvas()); the home and recording screens then hand the
// corner back with wifiIconScreenAllows()
void wifiIconOnScreenDraw() {
  wifiIconScreenOk = false;
}

static void wifiIconTick() {
  if (!wifiIconScreenOk || displaySleeping) return;
  const bool wanted = wifiIconWanted();
  if (!wanted && !wifiIconDrawn) return;
  if (wanted && millis() - wifiIconLastDraw < 40) return;
  wifiIconLastDraw = millis();
  M5Canvas c(&M5.Display);
  c.createSprite(WIFI_ICON_W, WIFI_ICON_H);
  c.fillSprite(TFT_BLACK);
  if (wanted) drawWifiSymbol(c, 0, 0, wifiIconLevel());  // else: black, the connection stands
  c.pushSprite(wifiIconX, wifiIconY);
  c.deleteSprite();
  wifiIconDrawn = wanted;
}

// The search and the icon, from loop() and from anything that waits a while (recording)
void wifiBackgroundTick() {
  wifiLinkTick();
  wifiIconTick();
}

// Wait until connected, searching at once if no search is under way. False after maxMs, or once a
// whole search found no known network. showIcon: the screen shown has room for the corner icon.
bool wifiWaitForLink(uint32_t maxMs, bool showIcon) {
  if (apMode) return false;
  if (wifiNetworkCount == 0) loadWiFiCredentials();
  if (wifiNetworkCount == 0) return false;
  if (wifiMachine.phase == WIFI_IDLE) wifiRadioOn();
  if (wifiMachine.phase == WIFI_UP && WiFi.status() != WL_CONNECTED) {
    wifiRun(wifiStart(wifiMachine, millis(), true));  // lost and not noticed yet
  } else {
    wifiRun(wifiKick(wifiMachine, millis(), wifiCanFast()));
  }
  if (wifiMachine.phase == WIFI_UP) return true;
  if (showIcon) {
    wifiIconPlace();
    wifiIconDrawn = false;
    wifiIconScreenOk = true;
  }
  const unsigned long start = millis();
  while (millis() - start < maxMs) {
    wifiBackgroundTick();
    if (wifiMachine.phase == WIFI_UP) {
      wifiIconTick();  // takes the icon away
      return true;
    }
    if (wifiMachine.phase == WIFI_NONE) return false;
    delay(10);
  }
  Serial.println("WiFi: gave up waiting");
  return WiFi.status() == WL_CONNECTED;
}

// Blocking connect for the setup screen's retry
bool connectToWiFi() {
  loadWiFiCredentials();
  listStoredNetworks();
  if (wifiNetworkCount == 0) return false;
  wifiLinkStart();
  return wifiWaitForLink(WIFI_CONNECT_WAIT_MS, false);
}

// For a request that found no connection, or failed on one that looked fine: wait for the
// background search, or join again. The screen stays as it is.
bool reconnectWiFiNow() {
  if (apMode) return false;
  if (wifiNetworkCount == 0) loadWiFiCredentials();
  if (wifiNetworkCount == 0) {
    Serial.println("No networks stored, cannot reconnect");
    return false;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi: request failed on a connection, joining again");
    wifiRun(wifiStart(wifiMachine, millis(), wifiCanFast()));
  }
  return wifiWaitForLink(WIFI_CONNECT_WAIT_MS, false);
}
