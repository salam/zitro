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
  M5.Display.setRotation(getLandscapeRotation());  // Locked until the view ends
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
  int kindB = activeFeature->buttonB ? HINT_NONE : HINT_ZOOM;
  int kindC = activeFeature->buttonC ? HINT_NONE : HINT_TEXT;
  if (activeFeature->hints) activeFeature->hints(kindB, kindC);
  drawButtonHints(c, M5.Display.getRotation(), HINT_MIC, kindB, kindC);
}

// Draws one frame and shows it
void modelViewRender(float dt) {
  if (!modelCanvas || !activeFeature || !activeFeature->frame) return;
  const bool hints = modelHintsShowing(millis(), modelHintsFrom);
  modelViewRedraw = modelHintsWereShowing && !hints;
  modelHintsWereShowing = hints;
  activeFeature->frame(*modelCanvas, dt);
  modelViewRedraw = false;
  if (hints) modelDrawHints(*modelCanvas);
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
    drawScreen("Not enough memory\nfor the 3D view.");
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
    if (magnitude >= MODEL_STILL_DPS && activeFeature->turnsWithGyro) {
      const float k = MODEL_TURN_SIGN * MODEL_TURN_GAIN * dt * (float)M_PI / 180.0f;
      mat3Rotate(modelRotation, k * modelGyroDps[0], k * modelGyroDps[1], k * modelGyroDps[2]);
    }
    if (magnitude >= MODEL_ACTIVE_DPS && now - modelWindowStart < MODEL_MOTION_CAP_MS) lastActivityTime = now;
  }
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
  static int cachedReserve = -1;
  bool ascii = true;
  for (unsigned int i = 0; i < modelLabel.length(); i++) {
    if ((uint8_t)modelLabel[i] >= 0x80) ascii = false;  // The small font only knows ASCII
  }
  if (ascii) {
    c.setFont(&fonts::DejaVu9);
  } else {
    c.setFreeFont(FontManager::getFont12pt());  // Names with umlauts or Greek letters
  }
  c.setTextSize(1);
  if (cachedLabel != modelLabel || cachedSource != modelSourceName || cachedReserve != reserveRight) {
    cachedLabel = modelLabel;
    cachedSource = modelSourceName;
    cachedReserve = reserveRight;
    const String source = modelSourceName.length() > 0 ? " - " + modelSourceName : String("");
    const int room = WIDTH - reserveRight - 6;
    String name = modelLabel;
    if (c.textWidth((name + source).c_str()) > room) {
      while (name.length() > 1 && c.textWidth((name + "..." + source).c_str()) > room) {
        // Never cut in the middle of a UTF-8 character
        int cut = name.length() - 1;
        while (cut > 0 && ((uint8_t)name[cut] & 0xC0) == 0x80) cut--;
        name.remove(cut);
      }
      name += "...";
    }
    cachedText = name + source;
  }
  c.fillRect(0, HEIGHT - EDU_FOOTER_HEIGHT, WIDTH, EDU_FOOTER_HEIGHT, TFT_BLACK);
  c.setTextDatum(textdatum_t::baseline_left);
  c.setTextColor(TFT_LIGHTGREY);
  c.drawString(cachedText, 2, HEIGHT - 4);
}

// Serial "modelgyro": how much the gyro reads while nothing moves, against the activity limit
void modelGyroNoise() {
  setGyroEnabled(true);
  delay(200);
  float peak = 0, sum = 0, ax = 0, ay = 0, az = 0;
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
    sum += m;
    samples++;
    delay(10);
  }
  Serial.printf("modelgyro samples=%d mean=%.2f peak=%.2f dps, still<%.1f active>=%.1f accel=%.2f,%.2f,%.2f\n", samples,
                sum / samples, peak, MODEL_STILL_DPS, MODEL_ACTIVE_DPS, ax, ay, az);
  updateGyroPower();
}
