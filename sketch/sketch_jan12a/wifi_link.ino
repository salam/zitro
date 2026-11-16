// The WiFi connection, found in the background (order and timings in wifi_core.h). Nothing here
// waits except wifiWaitForLink(): the screens and recording go on while Zitro searches, and a corner
// icon pulses grey to white on the home and recording screens until the connection stands. Then it
// turns into a WiFi symbol with a green tick for a moment, which fades out. What can't go on without
// WiFi (a recorded question, the setup screen's retry) waits on the WiFi page.
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

// The corner icon: searching, then the tick once connected
static bool wifiIconScreenOk = false;  // the screen shown has room for it (home, recording, waiting)
static WifiCorner wifiIconShown = WIFI_CORNER_NONE;
static unsigned long wifiIconLastDraw = 0;
static int wifiIconX = 0, wifiIconY = 0;      // the searching icon
static int wifiCornerX = 0, wifiCornerY = 0;  // the tick's box, which covers the searching icon too
uint8_t wifiIconTourLevel = 0;  // the screenshot tour shows the searching icon at this brightness (0: off)
uint8_t wifiDoneTourLevel = 0;  // the same for the tick
static const int WIFI_ICON_W = 17;
static const int WIFI_ICON_H = 14;
static bool wifiDoneOn = false;  // the tick plays: the connection came while the searching icon showed
static unsigned long wifiDoneAt = 0;
static bool wifiInWait = false;  // in wifiWaitForLink(): its caller draws what comes next at once

// The WiFi page
static bool wifiPageUp = false;      // the searching page is on screen: its frames may be drawn
static bool wifiPageGerman = true;   // read once per wait: the setting lives in flash
static String wifiPageDetail;        // the line under the title, as drawn
static uint16_t wifiPageBar = 0;     // the bar, as drawn
static unsigned long wifiPageLastFrame = 0;

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
  // The searching icon turns into the tick. Not in a wait: what comes next (a download) would stop
  // the fade halfway.
  wifiDoneAt = millis();
  wifiDoneOn = wifiIconScreenOk && wifiIconShown == WIFI_CORNER_SEARCHING && !displaySleeping && !wifiInWait;
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
  wifiIconShown = WIFI_CORNER_NONE;
  wifiDoneOn = false;
  WiFi.scanDelete();
}

bool wifiLinkUp() {
  return wifiMachine.phase == WIFI_UP && WiFi.status() == WL_CONNECTED;
}

// =============== THE CORNER ICON ===============

// Searching while the search runs (and grey once it found nothing), then the tick until it has faded
static WifiCorner wifiCornerWanted() {
  if (wifiIconTourLevel > 0) return WIFI_CORNER_SEARCHING;
  if (wifiDoneTourLevel > 0) return WIFI_CORNER_CONNECTED;
  if (apMode || wifiMachine.phase == WIFI_IDLE) return WIFI_CORNER_NONE;
  if (wifiMachine.phase != WIFI_UP) return WIFI_CORNER_SEARCHING;
  if (wifiDoneOn && wifiDoneLevel(millis() - wifiDoneAt) > 0) return WIFI_CORNER_CONNECTED;
  return WIFI_CORNER_NONE;
}

static uint8_t wifiDoneIconLevel() {
  if (wifiDoneTourLevel > 0) return wifiDoneTourLevel;
  return wifiDoneLevel(millis() - wifiDoneAt);
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

// The tick (wifi_done_icon.h) at x, y, scaled to level. Its black pixels are left out: it lies on black.
static void drawWifiDoneIcon(M5Canvas& c, int x, int y, uint8_t level) {
  for (int j = 0; j < WIFI_DONE_H; j++) {
    for (int i = 0; i < WIFI_DONE_W; i++) {
      const uint8_t* px = &wifiDoneIcon[(j * WIFI_DONE_W + i) * 3];
      const uint8_t r = pgm_read_byte(px), g = pgm_read_byte(px + 1), b = pgm_read_byte(px + 2);
      if ((r | g | b) == 0) continue;
      c.drawPixel(x + i, y + j, c.color565(r * level / 255, g * level / 255, b * level / 255));
    }
  }
}

// Top right; left of the battery when that shows (below 35 %, see drawBatteryIndicator). The tick is
// larger: 2 px further right and 1 px higher than the searching icon, its badge hanging below it.
static void wifiIconPlace() {
  const bool battery = M5.Power.getBatteryLevel() < 35;
  const int right = battery ? WIDTH - 25 - 6 : WIDTH - 4;  // where the searching icon ends
  wifiIconX = right - WIFI_ICON_W;
  wifiIconY = 3;
  wifiCornerX = right + 2 - WIFI_DONE_W;
  wifiCornerY = 2;
}

// The corner's icon into c, with the tick's box at x, y
static void wifiCornerDraw(M5Canvas& c, int x, int y, WifiCorner look) {
  if (look == WIFI_CORNER_SEARCHING) {
    drawWifiSymbol(c, x + wifiIconX - wifiCornerX, y + wifiIconY - wifiCornerY, wifiIconLevel());
  } else if (look == WIFI_CORNER_CONNECTED) {
    drawWifiDoneIcon(c, x, y, wifiDoneIconLevel());
  }
}

// Called by the home and recording screens before they push their canvas
void wifiIconDrawInto(M5Canvas& canvas) {
  wifiIconPlace();
  wifiIconShown = wifiCornerWanted();
  wifiCornerDraw(canvas, wifiCornerX, wifiCornerY, wifiIconShown);
  wifiIconLastDraw = millis();
}

// Called right after captureCanvas() by the screens that have room for the icon
void wifiIconScreenAllows() {
  wifiIconScreenOk = true;
}

// Every full-screen draw passes here (captureCanvas()); the home and recording screens then hand the
// corner back with wifiIconScreenAllows(), the WiFi page takes the screen back after its own draw
void wifiIconOnScreenDraw() {
  wifiIconScreenOk = false;
  wifiPageUp = false;
}

// One frame of the corner, in the tick's box, so a change of icon leaves nothing of the other behind
static void wifiIconTick() {
  if (!wifiIconScreenOk || displaySleeping) return;
  const WifiCorner wanted = wifiCornerWanted();
  if (wanted == WIFI_CORNER_NONE && wifiIconShown == WIFI_CORNER_NONE) return;
  if (wanted == wifiIconShown && millis() - wifiIconLastDraw < 40) return;
  wifiIconLastDraw = millis();
  M5Canvas c(&M5.Display);
  c.createSprite(WIFI_DONE_W, WIFI_DONE_H);
  c.fillSprite(TFT_BLACK);
  wifiCornerDraw(c, 0, 0, wanted);  // nothing: black, the tick has faded
  c.pushSprite(wifiCornerX, wifiCornerY);
  c.deleteSprite();
  wifiIconShown = wanted;
}

// The search and the icon, from loop() and from anything that waits a while (recording)
void wifiBackgroundTick() {
  wifiLinkTick();
  wifiIconTick();
}

// =============== THE WIFI PAGE ===============
// Full screen while something can't go on without WiFi: a large symbol whose arcs pulse grey to white
// outwards, what is being tried, and a bar that moves on in steps (wifiProgress()). Drawn whole when
// its text changes; in between only the symbol and the bar, so no full-screen canvas per frame.

static const int WIFI_PAGE_CX = WIDTH / 2;
static const int WIFI_PAGE_CY = 48;  // centre of the symbol's dot
static const int WIFI_PAGE_SYM_W = 56, WIFI_PAGE_SYM_H = 40;  // the box redrawn for the pulse
static const int WIFI_PAGE_SYM_X = WIFI_PAGE_CX - WIFI_PAGE_SYM_W / 2;
static const int WIFI_PAGE_SYM_Y = WIFI_PAGE_CY - 35;
static const int WIFI_PAGE_TITLE_Y = 76;
static const int WIFI_PAGE_DETAIL_Y = 98;
static const int WIFI_BAR_X = 50, WIFI_BAR_Y = 117, WIFI_BAR_W = 140, WIFI_BAR_H = 6;
static const uint32_t WIFI_PAGE_AFTER_MS = 300;  // a connection this quick doesn't flash the page
static const uint32_t WIFI_PAGE_FRAME_MS = 40;

static uint16_t wifiGrey(M5Canvas& c, uint8_t level) {
  return c.color565(level, level, level);
}

// Brightness of the dot with the inner arc, the middle arc and the outer arc
static void wifiPageLevels(WifiPageLook look, uint32_t now, uint8_t* level) {
  for (int i = 0; i < 3; i++) {
    if (look == WIFI_PAGE_CONNECTED) {
      level[i] = 255;
    } else if (look == WIFI_PAGE_NONE) {
      level[i] = WIFI_ICON_GREY;
    } else {
      level[i] = wifiPulseLevel(now - i * WIFI_WAVE_MS);  // the outer arcs follow the inner ones
    }
  }
}

// The large symbol, the same shape as the corner icon: a dot and three arcs, the dot's centre at cx, cy
static void drawWifiPageSymbol(M5Canvas& c, int cx, int cy, const uint8_t* level) {
  c.fillCircle(cx, cy, 4, wifiGrey(c, level[0]));
  c.fillArc(cx, cy, 10, 14, 225, 315, wifiGrey(c, level[0]));
  c.fillArc(cx, cy, 19, 23, 225, 315, wifiGrey(c, level[1]));
  c.fillArc(cx, cy, 28, 32, 225, 315, wifiGrey(c, level[2]));
}

static void drawWifiPageBar(M5Canvas& c, int x, int y, uint16_t progress) {
  c.fillRoundRect(x, y, WIFI_BAR_W, WIFI_BAR_H, WIFI_BAR_H / 2, wifiGrey(c, 55));
  int w = (int)((uint32_t)WIFI_BAR_W * progress / WIFI_BAR_FULL);
  if (w > 0 && w < WIFI_BAR_H) w = WIFI_BAR_H;  // a sliver still has round ends
  if (w > 0) c.fillRoundRect(x, y, w, WIFI_BAR_H, WIFI_BAR_H / 2, CITRUS);
}

// text, cut short with "..." to fit maxW pixels, whole characters only
static String wifiFitWidth(M5Canvas& c, const String& text, int maxW) {
  if (c.textWidth(text) <= maxW) return text;
  String s = text;
  while (s.length() > 0 && c.textWidth(s + "...") > maxW) {
    int end = s.length() - 1;
    while (end > 0 && ((uint8_t)s[end] & 0xC0) == 0x80) end--;  // back to the character's first byte
    s.remove(end);
  }
  return s + "...";
}

static String wifiPageTitle(WifiPageLook look) {
  if (look == WIFI_PAGE_CONNECTED) return wifiPageGerman ? "Verbunden!" : "Connected!";
  if (look == WIFI_PAGE_NONE) return wifiPageGerman ? "Kein bekanntes WLAN" : "No known WiFi found";
  return wifiPageGerman ? "Suche WLAN..." : "Looking for WiFi...";
}

// The line under the title while searching: the network being joined, with "2/3" when the scan found
// several, or the scan itself. A tab separates the count, which stays when the name is cut short.
static String wifiPageSearchText(const String& ssid, int next, int count) {
  if (ssid.length() == 0) return wifiPageGerman ? "Netze in der Nähe" : "Networks nearby";
  if (count < 2) return ssid;
  return ssid + "\t" + String(next + 1) + "/" + String(count);
}

static String wifiPageNoneText() {
  return wifiPageGerman ? "Öffne die Einrichtung..." : "Opening setup...";
}

static String wifiPageSearchDetail() {
  if (wifiMachine.phase == WIFI_SCAN) return wifiPageSearchText("", 0, 0);
  if (wifiJoining < 0 || wifiJoining >= wifiNetworkCount) return "";
  const int count = wifiMachine.phase == WIFI_JOIN ? wifiMachine.candidates : 0;
  return wifiPageSearchText(wifiNetworks[wifiJoining].ssid, wifiMachine.next, count);
}

// The whole page. now: where the pulse is; progress: the bar (not drawn when nothing was found)
static void drawWifiPage(WifiPageLook look, const String& detail, uint16_t progress, uint32_t now) {
  M5.Display.setRotation(getLandscapeRotation());
  M5Canvas canvas(&M5.Display);
  canvas.createSprite(WIDTH, HEIGHT);
  canvas.fillSprite(TFT_BLACK);
  uint8_t level[3];
  wifiPageLevels(look, now, level);
  drawWifiPageSymbol(canvas, WIFI_PAGE_CX, WIFI_PAGE_CY, level);

  canvas.setFreeFont(FontManager::getFont12pt());
  canvas.setTextDatum(MC_DATUM);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawString(wifiPageTitle(look), WIDTH / 2, WIFI_PAGE_TITLE_Y);

  String line = detail;
  const int tab = detail.indexOf('\t');
  const int maxW = WIDTH - 24;
  if (tab >= 0) {
    const String count = "  " + detail.substring(tab + 1);
    line = wifiFitWidth(canvas, detail.substring(0, tab), maxW - canvas.textWidth(count)) + count;
  } else {
    line = wifiFitWidth(canvas, detail, maxW);
  }
  canvas.setTextColor(wifiGrey(canvas, 160));
  canvas.drawString(line, WIDTH / 2, WIFI_PAGE_DETAIL_Y);

  if (look != WIFI_PAGE_NONE) drawWifiPageBar(canvas, WIFI_BAR_X, WIFI_BAR_Y, progress);

  drawBatteryIndicator(canvas);
  captureCanvas(canvas);
  wifiPageUp = look == WIFI_PAGE_SEARCHING;
  canvas.pushSprite(0, 0);
  canvas.deleteSprite();
  wifiPageDetail = detail;
  wifiPageBar = progress;
  wifiPageLastFrame = millis();
}

// One frame of the searching page: the pulse and the bar; all of it when the text changed
static void wifiPageFrame() {
  const unsigned long now = millis();
  if (wifiPageUp && now - wifiPageLastFrame < WIFI_PAGE_FRAME_MS) return;
  const uint16_t bar = wifiProgress(wifiMachine, now, wifiPageBar);
  const String detail = wifiPageSearchDetail();
  if (!wifiPageUp || detail != wifiPageDetail) {
    drawWifiPage(WIFI_PAGE_SEARCHING, detail, bar, now);
    return;
  }
  wifiPageLastFrame = now;
  uint8_t level[3];
  wifiPageLevels(WIFI_PAGE_SEARCHING, now, level);
  M5Canvas c(&M5.Display);
  c.createSprite(WIFI_PAGE_SYM_W, WIFI_PAGE_SYM_H);
  c.fillSprite(TFT_BLACK);
  drawWifiPageSymbol(c, WIFI_PAGE_CX - WIFI_PAGE_SYM_X, WIFI_PAGE_CY - WIFI_PAGE_SYM_Y, level);
  c.pushSprite(WIFI_PAGE_SYM_X, WIFI_PAGE_SYM_Y);
  c.deleteSprite();
  if (bar != wifiPageBar) {
    c.createSprite(WIFI_BAR_W, WIFI_BAR_H);
    c.fillSprite(TFT_BLACK);
    drawWifiPageBar(c, 0, 0, bar);
    c.pushSprite(WIFI_BAR_X, WIFI_BAR_Y);
    c.deleteSprite();
    wifiPageBar = bar;
  }
}

// The page now: searching (before a wait, so a button press shows at once), connected with the
// network's name, or nothing found before the setup screen opens
void wifiPageShow(WifiPageLook look) {
  wifiPageGerman = deviceLanguageIsGerman();
  String detail;
  if (look == WIFI_PAGE_SEARCHING) {
    detail = wifiPageSearchDetail();
  } else if (look == WIFI_PAGE_CONNECTED) {
    detail = WiFi.SSID();
  } else {
    detail = wifiPageNoneText();
  }
  drawWifiPage(look, detail, look == WIFI_PAGE_SEARCHING ? 0 : WIFI_BAR_FULL, millis());
}

// For the screenshot tour: the page at one moment of a search, without searching, with a made-up
// network name (never the real one). ssid empty while searching: the scan.
void wifiPageTourShot(WifiPageLook look, const String& ssid, int next, int count, uint16_t progress,
                      uint32_t pulseAt) {
  wifiPageGerman = deviceLanguageIsGerman();
  String detail = ssid;
  if (look == WIFI_PAGE_SEARCHING) detail = wifiPageSearchText(ssid, next, count);
  if (look == WIFI_PAGE_NONE) detail = wifiPageNoneText();
  drawWifiPage(look, detail, progress, pulseAt);
  wifiPageUp = false;  // not a real search: no frames follow
}

// =============== WAITING FOR THE CONNECTION ===============

static bool wifiWaitLoop(uint32_t maxMs, WifiWaitShow show) {
  if (show == WIFI_SHOW_ICON) {
    wifiIconPlace();
    wifiIconShown = WIFI_CORNER_NONE;
    wifiIconScreenOk = true;
  }
  if (show == WIFI_SHOW_PAGE) {
    wifiPageGerman = deviceLanguageIsGerman();
    if (!wifiPageUp) wifiPageBar = 0;
  }
  const unsigned long start = millis();
  while (millis() - start < maxMs) {
    wifiBackgroundTick();
    if (wifiMachine.phase == WIFI_UP) {
      wifiIconTick();  // takes the icon away (no tick in a wait, see wifiOnUp)
      return true;
    }
    if (wifiMachine.phase == WIFI_NONE) return false;
    // The page right away when it is already up (the caller showed it), else after a short while
    if (show == WIFI_SHOW_PAGE && (wifiPageUp || millis() - start >= WIFI_PAGE_AFTER_MS)) wifiPageFrame();
    delay(10);
  }
  Serial.println("WiFi: gave up waiting");
  return WiFi.status() == WL_CONNECTED;
}

// Wait until connected, searching at once if no search is under way. False after maxMs, or once a
// whole search found no known network. show: what the screen shows meanwhile.
bool wifiWaitForLink(uint32_t maxMs, WifiWaitShow show) {
  if (apMode) return false;
  if (wifiNetworkCount == 0) loadWiFiCredentials();
  if (wifiNetworkCount == 0) return false;
  if (wifiMachine.phase == WIFI_IDLE) wifiRadioOn();
  wifiInWait = true;
  if (wifiMachine.phase == WIFI_UP && WiFi.status() != WL_CONNECTED) {
    wifiRun(wifiStart(wifiMachine, millis(), true));  // lost and not noticed yet
  } else {
    wifiRun(wifiKick(wifiMachine, millis(), wifiCanFast()));
  }
  const bool up = wifiMachine.phase == WIFI_UP || wifiWaitLoop(maxMs, show);
  wifiInWait = false;
  wifiPageUp = false;  // no more frames: the caller draws what comes next
  return up;
}

// Blocking connect for the setup screen's retry, on the WiFi page
bool connectToWiFi() {
  loadWiFiCredentials();
  listStoredNetworks();
  if (wifiNetworkCount == 0) return false;
  wifiLinkStart();
  return wifiWaitForLink(WIFI_CONNECT_WAIT_MS, WIFI_SHOW_PAGE);
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
  return wifiWaitForLink(WIFI_CONNECT_WAIT_MS, WIFI_SHOW_NOTHING);
}
