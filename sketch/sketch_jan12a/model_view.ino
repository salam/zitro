// 3D view: draws the current model into a full-screen sprite, one frame at a time.
// The pure math and parsing live in model_core.h.

static const float MODEL_ZOOMS[3] = {1.0f, 1.5f, 2.2f};
static const int MODEL_FOOTER_HEIGHT = 14;
static const float MODEL_CAMERA = 3.0f;  // Distance from the model's centre, in model radii

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
    if (r >= 3) c.fillCircle(x - r / 3, y - r / 3, r / 3, TFT_WHITE);
  }

  if (hasFooter) {
    c.fillRect(0, HEIGHT - MODEL_FOOTER_HEIGHT, WIDTH, MODEL_FOOTER_HEIGHT, TFT_BLACK);
    c.setTextSize(1);
    c.setTextDatum(textdatum_t::bottom_left);
    c.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    c.drawString(footer, 2, HEIGHT - 2);
  }
  c.pushSprite(0, 0);
  captureCanvas(c);
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
  currentMolecule = saved;
  free(bench);
}
