// Result view: one sprite, gyro turning and sleep rules shared by all features (edu_feature.h).

static const unsigned long MODEL_FRAME_MS = 40;  // 25 frames per second is smooth and saves battery
static const float MODEL_TURN_GAIN = 2.0f;  // A small wrist turn shows the far side, screen stays readable
static const float MODEL_TURN_SIGN = -1.0f;  // The model stays put while the stick turns around it
static const float MODEL_STILL_DPS = 3.0f;  // Below this the hand isn't turning: sensor noise, idle turn
static const float MODEL_ACTIVE_DPS = 15.0f;  // Above this, motion counts as activity for the sleep timers
static const unsigned long MODEL_MOTION_CAP_MS = 5UL * 60UL * 1000UL;  // ...until no button for this long
static const float MODEL_IDLE_RAD_PER_S = 0.35f;

bool modelResetRotateToggle = false;
const EduFeature* activeFeature = nullptr;
float modelGyroDps[3] = {0, 0, 0};
bool modelViewRedraw = false;
bool modelIdleTurning = true;
static unsigned long modelWindowStart = 0;
static unsigned long modelHintsFrom = 0;  // When the button tabs last appeared
static bool modelHintsWereShowing = false;
static unsigned long modelHandTurnedAt = 0;
static M5Canvas* modelCanvas = nullptr;
static bool modelCanvasInPsram = false;
// Views that level (activeFeature->levels): the stick's roll and the way the text reads
static LevelState modelLevel;
int modelReadingTurn = 0;
float modelForcedRollDeg = NAN;  // Serial "roll <degrees>": the roll without turning the stick, for screenshots

static bool modelLevels() {
  return activeFeature && activeFeature->levels;
}

void modelLevelMatrix(float out[9]) {
  levelMatrix(modelRotation.m, modelLevels() ? modelLevel.roll : 0.0f, out);
}

void modelToReading(M5Canvas& c, int bx, int by, int& rx, int& ry) {
  float x, y;
  levelToReading(modelReadingTurn, (float)bx, (float)by, WIDTH, HEIGHT, x, y);
  rx = (int)lroundf(x);
  ry = (int)lroundf(y);
}

// Gravity right now, so a view opened with the stick upright reads upright from its first frame
static void modelLevelRead() {
  levelReset(modelLevel);
  M5.Imu.update();
  const auto imu = M5.Imu.getImuData();
  float up[3];
  gyroToScreen(M5.Display.getRotation(), imu.accel.x, imu.accel.y, imu.accel.z, up);
  levelStep(modelLevel, up);
}

static void modelLevelApplyForced() {
  if (isnan(modelForcedRollDeg)) return;
  modelLevel.roll = levelWrap(modelForcedRollDeg * (float)M_PI / 180.0f);
  modelLevel.known = true;
  levelSnap(modelLevel, true);
}

bool modelViewSpriteInPsram() {
  return modelCanvasInPsram;
}

// Small cube at the bottom right of the text screen: this answer has a 3D model
void drawModelHint(M5Canvas& c) {
  const int x = WIDTH - 21, y = HEIGHT - 19, s = 11, d = 5;
  const uint16_t color = c.color565(0xF2, 0xB7, 0x05);
  c.drawRect(x, y + d, s, s, color);
  c.drawRect(x + d, y, s, s, color);
  c.drawLine(x, y + d, x + d, y, color);
  c.drawLine(x + s - 1, y + d, x + s - 1 + d, y, color);
  c.drawLine(x, y + d + s - 1, x + d, y + s - 1, color);
  c.drawLine(x + s - 1, y + d + s - 1, x + s - 1 + d, y + s - 1, color);
}

void modelViewEnd() {
  viewMenuStop();  // Its rows belong to the view
  if (modelCanvas) {
    if (activeFeature && activeFeature->end) activeFeature->end();
    modelCanvas->deleteSprite();
    delete modelCanvas;
    modelCanvas = nullptr;
  }
}

// Allocates the sprite and lets the feature set up. False if there's no memory: the caller stays on the text.
bool modelViewBegin() {
  if (modelForceNoMemory || !activeFeature) return false;
  modelViewEnd();
  // Internal RAM is much faster to draw into than PSRAM; PSRAM is the fallback
  modelCanvas = new M5Canvas(&M5.Display);
  modelCanvas->setPsram(false);
  modelCanvasInPsram = false;
  if (!modelCanvas->createSprite(WIDTH, HEIGHT)) {
    modelCanvas->setPsram(true);
    modelCanvasInPsram = true;
    if (!modelCanvas->createSprite(WIDTH, HEIGHT)) {
      delete modelCanvas;
      modelCanvas = nullptr;
      return false;
    }
  }
  if (activeFeature->begin && !activeFeature->begin(*modelCanvas)) {
    modelCanvas->deleteSprite();
    delete modelCanvas;
    modelCanvas = nullptr;
    return false;
  }
  M5.Display.setRotation(getLandscapeRotation());  // Locked until the view ends; views that level turn within it
  modelLevelRead();
  modelLevelApplyForced();
  modelReadingTurn = modelLevels() ? modelLevel.quarter : 0;
  return true;
}

bool modelViewActive() {
  return modelCanvas != nullptr;
}

// Shows the button tabs again for a moment
static void modelShowHints() {
  modelHintsFrom = millis();
}

// A tab next to every button: A always asks something new; B and C as the feature says
static void modelDrawHints(M5Canvas& c) {
  if (guideActive()) {
    guideDrawViewHints(c);
    return;
  }
  int kindB = activeFeature->buttonB ? HINT_NONE : HINT_ZOOM;
  int kindC = activeFeature->buttonC ? HINT_NONE : HINT_TEXT;
  if (activeFeature->hints) activeFeature->hints(kindB, kindC);
  // Views that level: the tabs in the reading frame, still next to the buttons
  c.setRotation(modelReadingTurn);
  drawButtonHints(c, (M5.Display.getRotation() + modelReadingTurn) & 3, HINT_MIC, kindB, kindC);
  c.setRotation(0);
}

// Draws one frame and shows it
void modelViewRender(float dt) {
  if (!modelCanvas || !activeFeature || !activeFeature->frame) return;
  const bool hints = guideActive() || modelHintsShowing(millis(), modelHintsFrom);
  modelViewRedraw = modelHintsWereShowing && !hints;
  modelHintsWereShowing = hints;
  // The tour's frame and tabs are drawn for the landscape: the text stays that way while it runs
  modelReadingTurn = modelLevels() && !guideActive() ? modelLevel.quarter : 0;
  modelCanvas->setRotation(0);
  activeFeature->frame(*modelCanvas, dt);
  modelCanvas->setRotation(0);  // Raw buffer access and screenshots expect the buffer's own axes
  modelViewRedraw = false;
  if (viewMenuActive()) {
    viewMenuDraw(*modelCanvas);  // Over the running view, with its own tabs
  } else if (hints) {
    modelDrawHints(*modelCanvas);
  }
  modelCanvas->pushSprite(0, 0);
  captureCanvas(*modelCanvas);
}

// Opens the view. Out of memory: back to the text, with a short note.
bool modelViewShow() {
  if (timerAlertActive) return false;  // The alert stays on top; dismissing it brings the model back
  if (!hasModel || !activeFeature) {
    currentDisplayMode = SHOW_TEXT;
    return false;
  }
  if (!modelViewActive() && !modelViewBegin()) {
    currentDisplayMode = SHOW_TEXT;
    modelResetRotateToggle = true;
    Serial.println("3D view: no memory for the sprite");
    drawScreen(uiText("Zu wenig Speicher\nfür die 3D-Ansicht.", "Not enough memory\nfor the 3D view."));
    delay(1500);
    drawScreen(response);
    return false;
  }
  currentDisplayMode = SHOW_MODEL;
  // Every way here is a button press or a wake-up, which restarts the 5 minutes of motion
  modelWindowStart = millis();
  modelShowHints();
  modelHandTurnedAt = millis() - MODEL_IDLE_AFTER_HAND_MS;  // Lying still, it turns by itself at once
  updateGyroPower();
  modelViewRender(0.0f);
  return true;
}

void leaveModelView() {
  modelViewEnd();
  currentDisplayMode = SHOW_TEXT;
  modelResetRotateToggle = true;
  updateGyroPower();
}

// Button B in the view: the feature's own action, or zoom
void modelButtonB() {
  modelWindowStart = millis();
  modelShowHints();
  if (activeFeature && activeFeature->buttonB) {
    activeFeature->buttonB();
  } else {
    modelZoomStep = (modelZoomStep + 1) % 3;
  }
}

// Button B double click in the view: the feature's second action, if it has one
void modelButtonBDouble() {
  modelWindowStart = millis();
  modelShowHints();
  if (activeFeature && activeFeature->buttonBDouble) activeFeature->buttonBDouble();
}

// Button C in the view. True if the feature used the press; otherwise it means back to the text.
bool modelButtonC() {
  modelWindowStart = millis();
  modelShowHints();
  // A view with its own menu rows (the wave lab): C opens that menu, until the button module takes C away
  if (viewMenuOpen()) return true;
  return activeFeature && activeFeature->buttonC && activeFeature->buttonC();
}

// Holding Button C: back to the text. Music only: its click sets the volume, so it needs another
// way back. The other features keep theirs.
bool modelButtonCHold() {
  if (activeFeature != &FEATURE_MUSIC) return false;
  leaveModelView();
  drawScreen(response);
  return true;
}

// One frame of the view, at most 25 per second. Runs from loop() and never blocks.
void modelViewTick() {
  static unsigned long lastFrame = 0;
  const unsigned long now = millis();
  if (now - lastFrame < MODEL_FRAME_MS) return;
  const float dt = min((now - lastFrame) / 1000.0f, 0.1f);
  lastFrame = now;
  if (!modelViewActive() && !modelViewShow()) return;

  float magnitude = 0;
  modelGyroDps[0] = modelGyroDps[1] = modelGyroDps[2] = 0;
  if (gyroReady()) {
    M5.Imu.update();
    const auto imu = M5.Imu.getImuData();
    gyroToScreen(M5.Display.getRotation(), imu.gyro.x, imu.gyro.y, imu.gyro.z, modelGyroDps);
    magnitude = sqrtf(imu.gyro.x * imu.gyro.x + imu.gyro.y * imu.gyro.y + imu.gyro.z * imu.gyro.z);
    if (modelLevels()) {
      // The roll turns the picture exactly (levelMatrix); tilt and swivel turn the globe as the upright
      // picture sees them
      float up[3];
      gyroToScreen(M5.Display.getRotation(), imu.accel.x, imu.accel.y, imu.accel.z, up);
      levelStep(modelLevel, up);
      modelLevelApplyForced();
      float upright[3];
      levelGyro(modelGyroDps, modelLevel.roll, upright);
      memcpy(modelGyroDps, upright, sizeof(upright));
    }
    if (magnitude >= MODEL_STILL_DPS && activeFeature->turnsWithGyro) {
      const float k = MODEL_TURN_SIGN * MODEL_TURN_GAIN * dt * (float)M_PI / 180.0f;
      mat3Rotate(modelRotation, k * modelGyroDps[0], k * modelGyroDps[1], k * modelGyroDps[2]);
    }
    if (magnitude >= MODEL_ACTIVE_DPS && now - modelWindowStart < MODEL_MOTION_CAP_MS) lastActivityTime = now;
  }
  if (modelLevels() && !gyroReady()) modelLevelApplyForced();
  if (magnitude >= MODEL_STILL_DPS) modelHandTurnedAt = now;
  modelIdleTurning = modelIdleTurnAllowed(now, modelHandTurnedAt);
  if (activeFeature->keepAwake && activeFeature->keepAwake()) lastActivityTime = now;
  // The globe holds still so the pin stays in view. After the hand has turned the model, it waits a few seconds
  // before it turns by itself, so what the hand turned to stays in view.
  if (modelIdleTurning && activeFeature->turnsWithGyro && activeFeature != &FEATURE_GLOBE &&
      activeFeature != &FEATURE_CLOCK && activeFeature != &FEATURE_ISS) {
    mat3Rotate(modelRotation, 0, MODEL_IDLE_RAD_PER_S * dt, 0);
  }
  modelViewRender(dt);
}

// Name and source on the left of the bottom line. reserveRight keeps room for a legend.
void eduDrawFooter(M5Canvas& c, int reserveRight) {
  static String cachedLabel, cachedSource, cachedText;
  static int cachedReserve = -1, cachedWidth = -1;
  static bool cachedSmall = true;
  const int width = c.width(), height = c.height();  // The reading frame: views that level turn the sprite
  // The small font only knows ASCII: names with umlauts or Greek letters get the bigger one
  auto useFont = [&c](bool small) {
    if (small) c.setFont(&fonts::DejaVu9);
    else c.setFreeFont(FontManager::getFont12pt());
    c.setTextSize(1);
  };
  if (cachedLabel != modelLabel || cachedSource != modelSourceName || cachedReserve != reserveRight ||
      cachedWidth != width) {
    cachedLabel = modelLabel;
    cachedSource = modelSourceName;
    cachedReserve = reserveRight;
    cachedWidth = width;
    const bool labelAscii = eduIsAscii(modelLabel.c_str());
    useFont(labelAscii);
    char text[160];
    const bool withLabel = eduFooterFit(modelLabel.c_str(), modelSourceName.c_str(), width - reserveRight - 6,
                                        [&c](const char* t) { return (int)c.textWidth(t); }, text, sizeof(text));
    cachedText = text;
    cachedSmall = withLabel ? labelAscii : eduIsAscii(text);
  }
  useFont(cachedSmall);
  c.fillRect(0, height - EDU_FOOTER_HEIGHT, width, EDU_FOOTER_HEIGHT, TFT_BLACK);
  c.setTextDatum(textdatum_t::baseline_left);
  c.setTextColor(TFT_LIGHTGREY);
  c.drawString(cachedText, 2, height - 4);
}

// Serial "modelgyro": how much the gyro reads while nothing moves, against the activity limit
void modelGyroNoise() {
  setGyroEnabled(true);
  delay(200);
  float peak = 0, sum = 0, ax = 0, ay = 0, az = 0;
  float axis[3] = {0, 0, 0};
  int samples = 0;
  const unsigned long start = millis();
  while (millis() - start < 3000) {
    M5.Imu.update();
    const auto imu = M5.Imu.getImuData();
    const float m = sqrtf(imu.gyro.x * imu.gyro.x + imu.gyro.y * imu.gyro.y + imu.gyro.z * imu.gyro.z);
    peak = max(peak, m);
    ax = imu.accel.x;
    ay = imu.accel.y;
    az = imu.accel.z;
    axis[0] += imu.gyro.x;
    axis[1] += imu.gyro.y;
    axis[2] += imu.gyro.z;
    sum += m;
    samples++;
    delay(10);
  }
  Serial.printf("modelgyro samples=%d mean=%.2f peak=%.2f dps, still<%.1f active>=%.1f accel=%.2f,%.2f,%.2f\n", samples,
                sum / samples, peak, MODEL_STILL_DPS, MODEL_ACTIVE_DPS, ax, ay, az);
  // Per axis, and what the library still subtracts: 0 since imuForgetStartZero()
  Serial.printf("modelgyro axes=%.2f,%.2f,%.2f dps libOffset=%ld,%ld,%ld\n", axis[0] / samples, axis[1] / samples,
                axis[2] / samples, (long)M5.Imu.getOffsetData(3), (long)M5.Imu.getOffsetData(4), (long)M5.Imu.getOffsetData(5));
  updateGyroPower();
}
