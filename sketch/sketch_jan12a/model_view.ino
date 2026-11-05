// 3D view: draws the current model into a full-screen sprite, one frame at a time.
// The pure math and parsing live in model_core.h.

static const float MODEL_ZOOMS[3] = {1.0f, 1.5f, 2.2f};
static const int MODEL_FOOTER_HEIGHT = 18;
static const float MODEL_CAMERA = 3.0f;  // Distance from the model's centre, in model radii
static const unsigned long MODEL_FRAME_MS = 40;  // 25 frames per second is smooth and saves battery
static const float MODEL_TURN_GAIN = 2.0f;  // A small wrist turn shows the far side, screen stays readable
static const float MODEL_TURN_SIGN = -1.0f;  // The model stays put while the stick turns around it
static const float MODEL_STILL_DPS = 3.0f;  // Below this the hand isn't turning: sensor noise, idle turn
static const float MODEL_ACTIVE_DPS = 15.0f;  // Above this, motion counts as activity for the sleep timers
static const unsigned long MODEL_MOTION_CAP_MS = 5UL * 60UL * 1000UL;  // ...until no button for this long
static const float MODEL_IDLE_RAD_PER_S = 0.35f;
static const int MODEL_LEGEND_MAX = 8;

bool modelResetRotateToggle = false;
static unsigned long modelWindowStart = 0;
static uint8_t modelLegend[MODEL_LEGEND_MAX];
static uint8_t modelLegendCount = 0;
static String modelFooterText;
static bool modelFooterSmallFont = true;

static void modelSetFooterFont(M5Canvas& c) {
  if (modelFooterSmallFont) {
    c.setFont(&fonts::DejaVu9);
  } else {
    c.setFreeFont(FontManager::getFont12pt());  // Names with umlauts or Greek letters
  }
  c.setTextSize(1);
}

static M5Canvas* modelCanvas = nullptr;
static bool modelCanvasInPsram = false;
static float* modelZ = nullptr;
static int16_t* modelSX = nullptr;
static int16_t* modelSY = nullptr;
static uint8_t* modelR = nullptr;
static uint8_t* modelOrder = nullptr;
static uint16_t* modelAdjStart = nullptr;  // Bonds per atom: adjacency in compact form
static uint8_t* modelAdj = nullptr;
static uint8_t* modelAdjOrder = nullptr;

static void modelFreeBuffers() {
  free(modelZ); modelZ = nullptr;
  free(modelSX); modelSX = nullptr;
  free(modelSY); modelSY = nullptr;
  free(modelR); modelR = nullptr;
  free(modelOrder); modelOrder = nullptr;
  free(modelAdjStart); modelAdjStart = nullptr;
  free(modelAdj); modelAdj = nullptr;
  free(modelAdjOrder); modelAdjOrder = nullptr;
}

static void modelBuildAdjacency() {
  const ModelMolecule& m = *currentMolecule;
  memset(modelAdjStart, 0, sizeof(uint16_t) * (m.atomCount + 1));
  for (uint16_t i = 0; i < m.bondCount; i++) {
    modelAdjStart[m.bonds[i].a + 1]++;
    modelAdjStart[m.bonds[i].b + 1]++;
  }
  for (uint16_t i = 0; i < m.atomCount; i++) modelAdjStart[i + 1] += modelAdjStart[i];
  uint16_t* fill = (uint16_t*)malloc(sizeof(uint16_t) * m.atomCount);
  if (!fill) return;
  memcpy(fill, modelAdjStart, sizeof(uint16_t) * m.atomCount);
  for (uint16_t i = 0; i < m.bondCount; i++) {
    const ModelBond& b = m.bonds[i];
    modelAdj[fill[b.a]] = b.b;
    modelAdjOrder[fill[b.a]++] = b.order;
    modelAdj[fill[b.b]] = b.a;
    modelAdjOrder[fill[b.b]++] = b.order;
  }
  free(fill);
}

void modelViewEnd() {
  if (modelCanvas) {
    modelCanvas->deleteSprite();
    delete modelCanvas;
    modelCanvas = nullptr;
  }
  modelFreeBuffers();
}

static void modelLegendAdd(uint8_t element) {
  for (uint8_t i = 0; i < modelLegendCount; i++) {
    if (modelLegend[i] == element) return;
  }
  if (modelLegendCount < MODEL_LEGEND_MAX) modelLegend[modelLegendCount++] = element;
}

// Name and source on the left, the elements' letters in their colours on the right
static void modelBuildFooter() {
  const ModelMolecule& m = *currentMolecule;
  modelLegendCount = 0;
  // Carbon and hydrogen first, as chemists write formulas; then in order of appearance
  for (uint16_t i = 0; i < m.atomCount; i++) {
    if (m.atoms[i].element == 6) { modelLegendAdd(6); break; }
  }
  for (uint16_t i = 0; i < m.atomCount; i++) {
    if (m.atoms[i].element == 1) { modelLegendAdd(1); break; }
  }
  for (uint16_t i = 0; i < m.atomCount; i++) modelLegendAdd(m.atoms[i].element);

  // The small font only knows ASCII
  modelFooterSmallFont = true;
  for (unsigned int i = 0; i < modelLabel.length(); i++) {
    if ((uint8_t)modelLabel[i] >= 0x80) modelFooterSmallFont = false;
  }
  M5Canvas& c = *modelCanvas;
  modelSetFooterFont(c);
  int legendWidth = 0;
  for (uint8_t i = 0; i < modelLegendCount; i++) legendWidth += c.textWidth(elementSymbol(modelLegend[i])) + 3;
  const String source = " - PubChem";
  const int room = WIDTH - legendWidth - 6;
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
  modelFooterText = name + source;
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

// Allocates the sprite and depth buffers. False if there's no memory: the caller stays on the text.
bool modelViewBegin() {
  if (modelForceNoMemory || !currentMolecule) return false;
  modelViewEnd();
  const uint16_t atoms = currentMolecule->atomCount;
  const uint16_t half = currentMolecule->bondCount * 2;
  modelZ = (float*)malloc(sizeof(float) * atoms);
  modelSX = (int16_t*)malloc(sizeof(int16_t) * atoms);
  modelSY = (int16_t*)malloc(sizeof(int16_t) * atoms);
  modelR = (uint8_t*)malloc(atoms);
  modelOrder = (uint8_t*)malloc(atoms);
  modelAdjStart = (uint16_t*)malloc(sizeof(uint16_t) * (atoms + 1));
  modelAdj = (uint8_t*)malloc(half + 1);
  modelAdjOrder = (uint8_t*)malloc(half + 1);
  if (!modelZ || !modelSX || !modelSY || !modelR || !modelOrder || !modelAdjStart || !modelAdj || !modelAdjOrder) {
    modelViewEnd();
    return false;
  }
  for (uint16_t i = 0; i < atoms; i++) modelOrder[i] = (uint8_t)i;
  modelBuildAdjacency();

  // Internal RAM is much faster to draw into than PSRAM; PSRAM is the fallback
  modelCanvas = new M5Canvas(&M5.Display);
  modelCanvas->setPsram(false);
  modelCanvasInPsram = false;
  if (!modelCanvas->createSprite(WIDTH, HEIGHT)) {
    modelCanvas->setPsram(true);
    modelCanvasInPsram = true;
    if (!modelCanvas->createSprite(WIDTH, HEIGHT)) {
      modelViewEnd();
      return false;
    }
  }
  M5.Display.setRotation(getLandscapeRotation());  // Locked until the view ends
  modelBuildFooter();
  return true;
}

bool modelViewActive() {
  return modelCanvas != nullptr;
}

static uint8_t modelAtomRadius(uint8_t element) {
  if (element == 1) return 3;   // Hydrogen
  if (element <= 10) return 5;  // C, N, O ...
  return 7;                     // Larger atoms
}

// One half of a bond, from the atom to the middle, in that atom's colour
static void modelDrawHalfBond(int x0, int y0, int x1, int y1, uint8_t order, uint16_t color) {
  M5Canvas& c = *modelCanvas;
  int dx = x1 - x0, dy = y1 - y0;
  // Step sideways along the larger axis so parallel lines stay parallel on the pixel grid
  bool steep = abs(dy) > abs(dx);
  int sx = steep ? 1 : 0, sy = steep ? 0 : 1;
  if (order >= 2) {
    c.drawLine(x0 - 2 * sx, y0 - 2 * sy, x1 - 2 * sx, y1 - 2 * sy, color);
    c.drawLine(x0 - sx, y0 - sy, x1 - sx, y1 - sy, color);
    c.drawLine(x0 + 2 * sx, y0 + 2 * sy, x1 + 2 * sx, y1 + 2 * sy, color);
    c.drawLine(x0 + sx, y0 + sy, x1 + sx, y1 + sy, color);
    if (order >= 3) c.drawLine(x0, y0, x1, y1, color);
  } else {
    c.drawLine(x0, y0, x1, y1, color);
    c.drawLine(x0 + sx, y0 + sy, x1 + sx, y1 + sy, color);
  }
}

// Draws one frame and shows it, with an optional footer
void modelViewFrame(const char* footer) {
  if (!modelCanvas || !currentMolecule) return;
  const ModelMolecule& m = *currentMolecule;
  M5Canvas& c = *modelCanvas;
  c.fillSprite(TFT_BLACK);

  const bool hasFooter = footer && footer[0];
  const int cx = WIDTH / 2;
  const int cy = (HEIGHT - (hasFooter ? MODEL_FOOTER_HEIGHT : 0)) / 2;
  const float scale = (HEIGHT - MODEL_FOOTER_HEIGHT) * 0.42f * MODEL_ZOOMS[modelZoomStep];

  float p[3];
  for (uint16_t i = 0; i < m.atomCount; i++) {
    const ModelAtom& a = m.atoms[i];
    rotatePoint(modelRotation, a.x, a.y, a.z, p);
    const float perspective = MODEL_CAMERA / (MODEL_CAMERA - p[2]);
    modelSX[i] = (int16_t)(cx + p[0] * scale * perspective);
    modelSY[i] = (int16_t)(cy - p[1] * scale * perspective);
    modelZ[i] = p[2];
    int r = (int)(modelAtomRadius(a.element) * MODEL_ZOOMS[modelZoomStep] * perspective + 0.5f);
    modelR[i] = (uint8_t)(r < 1 ? 1 : r);
  }
  sortByDepth(modelZ, modelOrder, m.atomCount);

  for (uint16_t n = 0; n < m.atomCount; n++) {
    const uint8_t i = modelOrder[n];
    const uint16_t color = elementColor565(m.atoms[i].element);
    const int x = modelSX[i], y = modelSY[i], r = modelR[i];
    for (uint16_t k = modelAdjStart[i]; k < modelAdjStart[i + 1]; k++) {
      const uint8_t other = modelAdj[k];
      modelDrawHalfBond(x, y, (x + modelSX[other]) / 2, (y + modelSY[other]) / 2, modelAdjOrder[k], color);
    }
    // A dark rim keeps overlapping atoms apart, which is what makes the depth readable
    c.fillCircle(x, y, r + 1, TFT_BLACK);
    c.fillCircle(x, y, r, color);
    // Only on larger atoms: a highlight on a small one reads as a + sign, which means charge
    if (r >= 6) c.fillRect(x - r / 3 - 1, y - r / 3 - 1, 2, 2, TFT_WHITE);
  }

  if (hasFooter) {
    c.fillRect(0, HEIGHT - MODEL_FOOTER_HEIGHT, WIDTH, MODEL_FOOTER_HEIGHT, TFT_BLACK);
    modelSetFooterFont(c);
    c.setTextDatum(textdatum_t::baseline_left);
    c.setTextColor(TFT_LIGHTGREY);
    c.drawString(footer, 2, HEIGHT - 4);
    int x = WIDTH - 2;
    for (int i = modelLegendCount - 1; i >= 0; i--) {
      const char* symbol = elementSymbol(modelLegend[i]);
      x -= c.textWidth(symbol);
      c.setTextColor(elementColor565(modelLegend[i]));
      c.drawString(symbol, x, HEIGHT - 4);
      x -= 3;
    }
  }
  c.pushSprite(0, 0);
  captureCanvas(c);
}

// Opens the 3D view. Out of memory: back to the text, with a short note.
bool modelViewShow() {
  if (!hasModel || !currentMolecule) {
    currentDisplayMode = SHOW_TEXT;
    return false;
  }
  if (!modelViewActive() && !modelViewBegin()) {
    currentDisplayMode = SHOW_TEXT;
    Serial.println("3D view: no memory for the sprite");
    drawScreen("Not enough memory\nfor the 3D view.");
    delay(1500);
    drawScreen(response);
    return false;
  }
  currentDisplayMode = SHOW_MODEL;
  updateGyroPower();
  modelViewFrame(modelFooterText.c_str());
  return true;
}

void leaveModelView() {
  modelViewEnd();
  currentDisplayMode = SHOW_TEXT;
  modelResetRotateToggle = true;
  updateGyroPower();
}

void modelCycleZoom() {
  modelZoomStep = (modelZoomStep + 1) % 3;
  modelWindowStart = millis();
}

// One frame of the 3D view, at most 25 per second. Runs from loop() and never blocks.
void modelViewTick() {
  static unsigned long lastFrame = 0;
  static unsigned long lastTick = 0;
  const unsigned long now = millis();
  // First pass after entering the view or waking up: that was a button press
  if (now - lastTick > 2000) modelWindowStart = now;
  lastTick = now;
  if (now - lastFrame < MODEL_FRAME_MS) return;
  const float dt = min((now - lastFrame) / 1000.0f, 0.1f);
  lastFrame = now;
  if (!modelViewActive() && !modelViewShow()) return;

  float magnitude = 0;
  if (gyroReady()) {
    M5.Imu.update();
    const auto imu = M5.Imu.getImuData();
    const float gx = imu.gyro.x, gy = imu.gyro.y, gz = imu.gyro.z;
    magnitude = sqrtf(gx * gx + gy * gy + gz * gz);
    if (magnitude >= MODEL_STILL_DPS) {
      // Rotation 1 and 3 are the same landscape view turned by half a circle
      const float side = M5.Display.getRotation() == 3 ? -1.0f : 1.0f;
      const float k = MODEL_TURN_SIGN * MODEL_TURN_GAIN * dt * (float)M_PI / 180.0f;
      mat3Rotate(modelRotation, k * side * gx, k * side * gy, k * gz);
    }
    if (magnitude >= MODEL_ACTIVE_DPS && now - modelWindowStart < MODEL_MOTION_CAP_MS) lastActivityTime = now;
  }
  if (magnitude < MODEL_STILL_DPS) mat3Rotate(modelRotation, 0, MODEL_IDLE_RAD_PER_S * dt, 0);
  modelViewFrame(modelFooterText.c_str());
}

// Serial "modelgyro": how much the gyro reads while nothing moves, against the activity limit
void modelGyroNoise() {
  setGyroEnabled(true);
  delay(200);
  float peak = 0, sum = 0;
  int samples = 0;
  const unsigned long start = millis();
  while (millis() - start < 3000) {
    M5.Imu.update();
    const auto imu = M5.Imu.getImuData();
    const float m = sqrtf(imu.gyro.x * imu.gyro.x + imu.gyro.y * imu.gyro.y + imu.gyro.z * imu.gyro.z);
    peak = max(peak, m);
    sum += m;
    samples++;
    delay(10);
  }
  Serial.printf("modelgyro samples=%d mean=%.2f peak=%.2f dps, still<%.1f active>=%.1f\n", samples, sum / samples, peak,
                MODEL_STILL_DPS, MODEL_ACTIVE_DPS);
  updateGyroPower();
}

// Serial "modelbench <atoms>": frames per second for a made-up test molecule.
// Only measures drawing speed; this shape is never shown as a real model.
void modelBench(int atoms) {
  if (atoms < 2) atoms = 2;
  if (atoms > MODEL_MAX_ATOMS) atoms = MODEL_MAX_ATOMS;
  ModelMolecule* bench = (ModelMolecule*)malloc(sizeof(ModelMolecule));
  if (!bench) {
    Serial.println("modelbench: no memory");
    return;
  }
  bench->atomCount = (uint16_t)atoms;
  bench->bondCount = (uint16_t)min(atoms - 1 + atoms / 4, MODEL_MAX_BONDS);
  const uint8_t elements[4] = {6, 1, 8, 7};
  for (int i = 0; i < atoms; i++) {
    const float t = (float)i / (atoms - 1);
    const float angle = t * 40.0f;
    const float radius = sqrtf(1.0f - (2 * t - 1) * (2 * t - 1));
    bench->atoms[i] = {radius * cosf(angle), 2 * t - 1, radius * sinf(angle), elements[i % 4]};
  }
  for (int i = 0; i < bench->bondCount; i++) {
    const int a = i < atoms - 1 ? i : (i - (atoms - 1)) * 3 % atoms;
    const int b = i < atoms - 1 ? i + 1 : (a + 2) % atoms;
    bench->bonds[i] = {(uint8_t)a, (uint8_t)b, (uint8_t)(i % 5 == 0 ? 2 : 1)};
  }
  ModelMolecule* saved = currentMolecule;
  currentMolecule = bench;
  const String savedLabel = modelLabel;
  modelLabel = "Benchmark";
  if (modelViewBegin()) {
    mat3Identity(modelRotation);
    for (uint8_t zoom = 0; zoom < 3; zoom++) {
      modelZoomStep = zoom;
      const unsigned long start = millis();
      int frames = 0;
      while (millis() - start < 3000) {
        mat3Rotate(modelRotation, 0.01f, 0.03f, 0.0f);
        modelViewFrame("Benchmark");
        frames++;
        yield();
      }
      Serial.printf("modelbench atoms=%d bonds=%d zoom=%d fps=%.1f sprite=%s freeHeap=%u\n", atoms, bench->bondCount,
                    zoom, frames * 1000.0f / (millis() - start), modelCanvasInPsram ? "psram" : "internal",
                    (unsigned)ESP.getFreeHeap());
    }
    modelZoomStep = 0;
    modelViewEnd();
  } else {
    Serial.println("modelbench: no memory for the sprite");
  }
  modelLabel = savedLabel;
  currentMolecule = saved;
  free(bench);
}
