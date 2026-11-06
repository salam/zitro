// Screenshots over USB serial.
// The LCD can't be read back, so full-screen draws pass their canvas to captureCanvas()
// before pushing it. When a capture is armed, the canvas is streamed as run-length encoded
// RGB565 between "@@SHOT <name> <w> <h>" and "@@END" lines (decoded by
// scripts/capture_screens.py).
//
// Serial commands (115200 baud, newline terminated):
//   tour  draw every main screen of the app once and capture each one
//   shot  capture whatever is on screen now

static int captureArmed = 0;       // >0: capture the next draw(s)
static int captureSkipFrames = 0;  // skip this many frames first (animated screens)
static String captureName;

static void dumpCanvas(M5Canvas &canvas, const String &name) {
  int w = canvas.width();
  int h = canvas.height();
  Serial.printf("\n@@SHOT %s %d %d\n", name.c_str(), w, h);
  uint16_t color = 0;
  uint32_t run = 0;
  int tokens = 0;
  auto emit = [&]() {
    Serial.printf("%04X%04X", color, (unsigned)run);
    if (++tokens % 16 == 0) Serial.print('\n');
  };
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      uint16_t px = canvas.readPixel(x, y);
      if (run > 0 && px == color && run < 0xFFFF) {
        run++;
      } else {
        if (run > 0) emit();
        color = px;
        run = 1;
      }
    }
  }
  if (run > 0) emit();
  Serial.println("\n@@END");
  Serial.flush();
}

void captureCanvas(M5Canvas &canvas) {
  gifAnimOnScreenDraw();  // every full-screen draw comes through here
  if (captureArmed <= 0) return;
  if (captureSkipFrames > 0) {
    captureSkipFrames--;
    return;
  }
  captureArmed = 0;
  dumpCanvas(canvas, captureName);
}

static void armCapture(const String &name, int skipFrames) {
  captureName = name;
  captureSkipFrames = skipFrames;
  captureArmed = 1;
}

static void tourScreen(const String &name, const String &text) {
  armCapture(name, 0);
  drawScreen(text);
  delay(300);
}

// Timer alert and list with made-up timers; the real timers are restored afterwards
static void tourTimerScreens() {
  TimerEntry savedTimers[MAX_TIMERS];
  for (int i = 0; i < MAX_TIMERS; i++) savedTimers[i] = timers[i];
  int savedCount = timerCount;

  time_t now = time(NULL);
  for (int i = 0; i < MAX_TIMERS; i++) timers[i].active = false;
  timers[0] = {true, now + 552, "Pasta"};
  timers[1] = {true, now + 3 * 3600 + 20 * 60, "Mama anrufen"};
  timerCount = 2;

  // Landscape first (held rotation forced so the stick's posture doesn't matter)
  printUiForcedRotation = getLandscapeRotation();
  armCapture("18-timer-alert", 0);
  drawTimerAlert(0, true);
  delay(300);
  armCapture("18b-timer-alert-blink", 0);
  drawTimerAlert(0, false);
  delay(300);
  timers[2] = {false, now, "Waesche aus der Maschine nehmen und aufhaengen"};
  armCapture("18c-timer-alert-long", 0);
  drawTimerAlert(2, true);
  delay(300);

  armCapture("19-timer-list", 0);
  drawTimerListEntry(0);
  delay(300);
  armCapture("19b-timer-list-second", 0);
  drawTimerListEntry(1);
  delay(300);

  // Portrait, Button A at the bottom
  printUiForcedRotation = 0;
  armCapture("17c-timer-set-portrait", 0);
  drawTimerSet("Pasta", formatTimerDuration(10));
  delay(300);
  armCapture("18d-timer-alert-portrait", 0);
  drawTimerAlert(0, true);
  delay(300);
  armCapture("18e-timer-alert-portrait-blink", 0);
  drawTimerAlert(0, false);
  delay(300);
  armCapture("19c-timer-list-portrait", 0);
  drawTimerListEntry(0);
  delay(300);
  printUiForcedRotation = -1;
  M5.Display.setRotation(getLandscapeRotation());

  for (int i = 0; i < MAX_TIMERS; i++) timers[i] = savedTimers[i];
  timerCount = savedCount;
}

// Print flow with a made-up printer list; the real list is restored afterwards
static void tourPrintScreens() {
  PrinterInfo savedPrinters[MAX_PRINTERS];
  for (int i = 0; i < MAX_PRINTERS; i++) savedPrinters[i] = discoveredPrinters[i];
  int savedCount = printerCount;
  int savedSelected = selectedPrinterIndex;

  const char* names[] = {"Canon_TS5100_series.local", "HP-LaserJet-M110we.local",
                         "Brother_HL_L2350DW_Office_Upstairs.local", "EPSON-ET-2850.local",
                         "Kitchen-Printer.local"};

  armCapture("23-print-searching", 0);
  drawPrintStatus("Searching...", PRINT_BADGE_NONE);
  delay(300);

  printerCount = 1;
  discoveredPrinters[0].name = names[0];
  selectedPrinterIndex = 0;
  armCapture("24-print-select-one", 0);
  displayPrinterSelection();
  delay(300);

  printerCount = 5;
  for (int i = 0; i < 5; i++) discoveredPrinters[i].name = names[i];
  selectedPrinterIndex = 2;
  armCapture("25-print-select-many", 0);
  displayPrinterSelection();
  delay(300);

  // Size menu after picking a printer
  int savedLayout = printLayout, savedZoom = printZoomIndex;
  printMenuStep = PRINT_STEP_SIZE;
  printLayout = PRINT_LAYOUT_FIT;
  armCapture("36-print-size-fit", 0);
  displayPrinterSelection();
  delay(300);
  printLayout = PRINT_LAYOUT_HALF_TEXT;
  armCapture("37-print-size-half-text", 0);
  displayPrinterSelection();
  delay(300);
  printLayout = PRINT_LAYOUT_ZOOM;
  printZoomIndex = 3;
  armCapture("38-print-size-zoom", 0);
  displayPrinterSelection();
  delay(300);
  printMenuStep = PRINT_STEP_ZOOM;
  armCapture("39-print-zoom-picker", 0);
  displayPrinterSelection();
  delay(300);
  printMenuStep = PRINT_STEP_PRINTER;

  armCapture("26-print-printing", 0);
  drawPrintStatus("Printing...", PRINT_BADGE_NONE);
  delay(300);
  armCapture("27-print-sent", 0);
  drawPrintStatus("Sent!", PRINT_BADGE_OK);
  delay(300);
  armCapture("28-print-failed", 0);
  drawPrintStatus("Print\nfailed", PRINT_BADGE_FAIL);
  delay(300);
  armCapture("29-print-none", 0);
  drawPrintStatus("No printer\nfound", PRINT_BADGE_FAIL);
  delay(300);

  // Portrait, Button A at the bottom
  printUiForcedRotation = 0;
  armCapture("30-print-portrait-select-many", 0);
  displayPrinterSelection();
  delay(300);
  printerCount = 1;
  selectedPrinterIndex = 0;
  discoveredPrinters[0].name = "Canon TS5100 series";
  armCapture("31-print-portrait-select-one", 0);
  displayPrinterSelection();
  delay(300);
  armCapture("32-print-portrait-searching", 0);
  drawPrintStatus("Searching...", PRINT_BADGE_NONE);
  delay(300);
  armCapture("33-print-portrait-sent", 0);
  drawPrintStatus("Sent!", PRINT_BADGE_OK);
  delay(300);
  armCapture("34-print-portrait-none", 0);
  drawPrintStatus("No printer\nfound", PRINT_BADGE_FAIL);
  delay(300);

  // Portrait, Button A on top
  printUiForcedRotation = 2;
  printerCount = 5;
  for (int i = 0; i < 5; i++) discoveredPrinters[i].name = names[i];
  selectedPrinterIndex = 3;
  armCapture("35-print-portrait-flipped-select", 0);
  displayPrinterSelection();
  delay(300);

  printUiForcedRotation = 0;
  printMenuStep = PRINT_STEP_SIZE;
  printLayout = PRINT_LAYOUT_HALF_TEXT;
  armCapture("40-print-portrait-size", 0);
  displayPrinterSelection();
  delay(300);
  printLayout = PRINT_LAYOUT_ZOOM;
  printMenuStep = PRINT_STEP_ZOOM;
  armCapture("41-print-portrait-zoom", 0);
  displayPrinterSelection();
  delay(300);
  printMenuStep = PRINT_STEP_PRINTER;
  printLayout = savedLayout;
  printZoomIndex = savedZoom;
  printUiForcedRotation = -1;
  M5.Display.setRotation(getLandscapeRotation());

  for (int i = 0; i < MAX_PRINTERS; i++) discoveredPrinters[i] = savedPrinters[i];
  printerCount = savedCount;
  selectedPrinterIndex = savedSelected;
}

static void runScreenshotTour() {
  Serial.println("@@TOUR BEGIN");

  // Keep the real state; the tour only draws
  String savedResponse = response;
  modelViewEnd();
  bool savedHasImage = hasImage;
  bool savedHasModel = hasModel;
  hasModel = false;
  DisplayMode savedMode = currentDisplayMode;
  hasImage = false;
  currentDisplayMode = SHOW_TEXT;

  {
    M5Canvas splash(&M5.Display);
    splash.createSprite(WIDTH, HEIGHT);
    splash.drawPng(logo_png, logo_png_len, 0, 0);
    dumpCanvas(splash, "01-splash");
    splash.pushSprite(0, 0);
    splash.deleteSprite();
    delay(300);
  }
  tourScreen("02-home", MAIN_QUESTION);
  tourScreen("03-welcome-no-key", "Welcome to Zitro!\n\nAdd an AI key on\nthe setup page.");
  armCapture("04-setup-mode", 0);
  drawSetupScreen();
  delay(300);
  tourScreen("05-waking-up", "Waking up...");
  tourScreen("06-reconnecting", "Reconnecting to\nHome WiFi...");

  armCapture("07-recording", 0);
  drawProgress(activeRecordSeconds > 2 ? activeRecordSeconds - 2 : 1);
  delay(300);
  tourScreen("08-recording-restart", "Restarting...");

  armCapture("09-transcribing", 1);  // Middle frame of the pulse is the brightest
  drawScreenWithIcon("Transcribing...", transcribeIcon, TRANSCRIBEICON_WIDTH, TRANSCRIBEICON_HEIGHT);
  armCapture("10-thinking", 1);
  drawScreenWithIcon("Thinking...", thinkingIcon, THINKINGICON_WIDTH, THINKINGICON_HEIGHT, 2);

  tourScreen("11-answer", wordWrap("Der Mond ist im Schnitt 384'400 km von der Erde entfernt. Licht braucht gut eine Sekunde.", 25));
  tourScreen("12-couldnt-hear", "Couldn't hear.\nTry again.");
  tourScreen("13-error-key-rejected", "API key rejected.\nTriple-press the\nside button for\nsettings.");
  tourScreen("14-hint-no-key", "No AI key set.\nTriple-press the\nside button for\nsettings.");
  tourScreen("15-hint-voice-key", "Voice needs an\nOpenAI or Zitro\nkey. Triple-press\nside button.");
  armCapture("16-setting-changed", 0);
  drawIconTextScreen("Lautstaerke auf 80 gesetzt.", volumeIcon);
  delay(300);
  printUiForcedRotation = getLandscapeRotation();
  armCapture("17-timer-set", 0);
  drawTimerSet("Pasta", formatTimerDuration(10));
  delay(300);
  armCapture("17b-timer-set-long", 0);
  drawTimerSet("Waesche aus der Maschine nehmen", formatTimerDuration(90));
  delay(300);
  printUiForcedRotation = -1;
  tourTimerScreens();

  armCapture("20-night-lock", 1);
  drawScreenWithIcon("Ich schlafe z z z", nightIcon, NIGHTICON_WIDTH, NIGHTICON_HEIGHT);
  armCapture("21-opening-settings", 0);
  drawSetupScreen(true);
  delay(300);
  armCapture("22-mic-error", 1);
  drawScreenWithIcon("Mic error", micBrokenIcon, MICBROKENICON_WIDTH, MICBROKENICON_HEIGHT, 2);
  tourPrintScreens();

  captureArmed = 0;
  response = savedResponse;
  hasImage = savedHasImage;
  hasModel = savedHasModel;
  if (hasModel) {  // Only with a model loaded: the tour itself doesn't use the network
    armCapture("50-3d-model", 0);
    modelViewShow();
    modelViewEnd();
  }
  currentDisplayMode = savedMode;
  redrawPreviousResult();
  resetToActiveState();
  Serial.println("@@TOUR END");
}

void handleSerialCommands() {
  static String line;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c != '\n') {
      if (line.length() < 200) line += c;
      continue;
    }
    line.trim();
    if (line == "tour") {
      runScreenshotTour();
    } else if (line == "shot") {
      armCapture("current", 0);
      redrawPreviousResult();
    } else if (line.startsWith("modelhost ")) {
      modelPubChemHost = line.substring(10);
      Serial.println("modelhost " + modelPubChemHost);
    } else if (line.startsWith("model ")) {
      runModelCommand(line.substring(6));
    } else if (line.startsWith("ask ")) {
      serialTestQuestion = line.substring(4);
      handleVoiceQuery(false);
    } else if (line == "sleeptest") {
      // What deep sleep does to the state, without the sleep: Button A would be needed to wake
      if (hasImage) currentDisplayMode = SHOW_IMAGE;
      saveStateToPreferences();
      free(imageBuffer);
      imageBuffer = nullptr;
      imageBufferSize = 0;
      hasImage = false;
      currentImageUrl = "";
      handleDeepSleepWake();
      Serial.printf("sleeptest hasImage=%d buffer=%d bytes mode=%d\n", hasImage, imageBufferSize, currentDisplayMode);
    } else if (line == "modelgyro") {
      modelGyroNoise();
    } else if (line.startsWith("modelbench ")) {
      modelBench(line.substring(11).toInt());
      redrawPreviousResult();
    } else if (line.startsWith("modelnomem ")) {
      modelForceNoMemory = line.substring(11).toInt() != 0;
      Serial.printf("modelnomem %d\n", modelForceNoMemory);
    } else if (line.startsWith("printimg ") || line.startsWith("printtest ") || line.startsWith("printtext ")) {
      runPrintTestCommand(line);
    }
    line = "";
  }
}
