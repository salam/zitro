// The grid of the guided tour: 16 small pictures and the sentence that opens each feature.

static uint16_t guideRgb(M5Canvas& c, uint32_t hex) {
  return c.color565((hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF);
}

// Each picture fits a 26x26 box with its top left corner at x, y
void guideDrawIcon(M5Canvas& c, const char* id, int x, int y) {
  const uint16_t white = TFT_WHITE, black = TFT_BLACK;
  const uint16_t citrus = guideRgb(c, 0xF2B705), red = guideRgb(c, 0xE53935), blue = guideRgb(c, 0x3B82F6);
  const uint16_t green = guideRgb(c, 0x4F9A3C), grey = guideRgb(c, 0x8F8F8F), light = guideRgb(c, 0xDDDDDD);
  const String name = id;
  if (name == "print") {
    c.fillRect(x + 6, y, 14, 9, white);
    c.fillRoundRect(x + 1, y + 8, 24, 11, 3, guideRgb(c, 0x7D8A99));
    c.fillRect(x + 6, y + 15, 14, 11, white);
    c.drawFastHLine(x + 9, y + 19, 8, grey);
    c.drawFastHLine(x + 9, y + 22, 6, grey);
    c.fillCircle(x + 21, y + 12, 1, citrus);
  } else if (name == "time") {  // A rocket: the example is spaceflight
    c.fillTriangle(x + 13, y, x + 19, y + 9, x + 7, y + 9, red);
    c.fillRect(x + 8, y + 9, 10, 11, light);
    c.fillCircle(x + 13, y + 13, 2, blue);
    c.fillTriangle(x + 8, y + 14, x + 3, y + 22, x + 8, y + 20, red);
    c.fillTriangle(x + 18, y + 14, x + 23, y + 22, x + 18, y + 20, red);
    c.fillTriangle(x + 10, y + 20, x + 16, y + 20, x + 13, y + 26, citrus);
  } else if (name == "mol") {
    c.drawWideLine(x + 13, y + 13, x + 5, y + 6, 1.0f, grey);
    c.drawWideLine(x + 13, y + 13, x + 22, y + 7, 1.0f, grey);
    c.drawWideLine(x + 13, y + 13, x + 12, y + 23, 1.0f, grey);
    c.fillCircle(x + 13, y + 13, 5, grey);
    c.fillCircle(x + 5, y + 6, 4, red);
    c.fillCircle(x + 22, y + 7, 4, blue);
    c.fillCircle(x + 12, y + 23, 3, white);
  } else if (name == "body") {
    const uint16_t heart = guideRgb(c, 0xE0364A);
    c.fillCircle(x + 8, y + 9, 6, heart);
    c.fillCircle(x + 18, y + 9, 6, heart);
    c.fillTriangle(x + 2, y + 12, x + 24, y + 12, x + 13, y + 25, heart);
  } else if (name == "globe") {
    c.fillCircle(x + 13, y + 14, 11, guideRgb(c, 0x2F6FD0));
    c.fillTriangle(x + 6, y + 7, x + 14, y + 5, x + 9, y + 16, green);
    c.fillTriangle(x + 14, y + 5, x + 15, y + 13, x + 9, y + 16, green);
    c.fillTriangle(x + 15, y + 17, x + 22, y + 15, x + 19, y + 24, green);
    c.fillCircle(x + 13, y + 5, 3, citrus);
    c.fillTriangle(x + 10, y + 6, x + 16, y + 6, x + 13, y + 12, citrus);
  } else if (name == "iss") {
    c.fillRect(x + 10, y + 11, 6, 6, light);
    c.fillRect(x + 2, y + 13, 22, 2, light);
    const uint16_t panel = guideRgb(c, 0x3066C9);
    const int offsets[4] = {1, 5, 18, 22};
    for (int i = 0; i < 4; i++) {
      c.fillRect(x + offsets[i], y + 3, 3, 8, panel);
      c.fillRect(x + offsets[i], y + 17, 3, 8, panel);
    }
  } else if (name == "physics") {
    c.fillTriangle(x + 13, y + 17, x + 8, y + 25, x + 18, y + 25, grey);
    c.drawWideLine(x + 1, y + 11, x + 25, y + 21, 1.2f, citrus);
    c.fillRect(x + 2, y + 2, 9, 9, red);
    c.fillCircle(x + 21, y + 14, 4, blue);
  } else if (name == "graph") {
    c.drawFastHLine(x + 2, y + 14, 23, grey);
    c.drawFastVLine(x + 13, y + 1, 25, grey);
    int lastY = y + 14;
    for (int i = 1; i <= 23; i++) {
      const int py = y + 14 - (int)roundf(sinf(i / 23.0f * 6.283f) * 9.0f);
      c.drawLine(x + 1 + i, lastY, x + 2 + i, py, citrus);
      lastY = py;
    }
  } else if (name == "math") {
    c.fillRoundRect(x + 1, y + 2, 24, 23, 4, guideRgb(c, 0x2A3A55));
    c.setFreeFont(FontManager::getFont12pt());
    c.setTextColor(white);
    c.setTextDatum(MC_DATUM);
    c.drawString("x", x + 11, y + 14);
    c.setFont(&fonts::DejaVu9);
    c.setTextSize(1);
    c.drawString("2", x + 19, y + 9);
  } else if (name == "timer") {
    c.fillRect(x + 10, y, 6, 3, light);
    c.fillCircle(x + 13, y + 15, 10, light);
    c.fillCircle(x + 13, y + 15, 8, guideRgb(c, 0x101010));
    c.drawWideLine(x + 13, y + 15, x + 18, y + 10, 0.9f, citrus);
  } else if (name == "recipe") {
    c.fillEllipse(x + 11, y + 13, 10, 6, guideRgb(c, 0x555555));
    c.fillEllipse(x + 11, y + 12, 7, 4, guideRgb(c, 0xE9B65C));
    c.drawWideLine(x + 20, y + 14, x + 26, y + 18, 1.5f, grey);
    c.drawFastVLine(x + 7, y + 3, 3, light);
    c.drawFastVLine(x + 12, y + 2, 3, light);
  } else if (name == "clock") {
    c.fillCircle(x + 13, y + 14, 11, white);
    c.drawWideLine(x + 13, y + 14, x + 13, y + 7, 0.9f, black);
    c.drawWideLine(x + 13, y + 14, x + 19, y + 16, 0.8f, red);
    c.fillCircle(x + 13, y + 14, 1, black);
  } else if (name == "speak") {
    c.fillRect(x + 2, y + 10, 6, 7, white);
    c.fillTriangle(x + 7, y + 13, x + 14, y + 4, x + 14, y + 23, white);
    c.fillArc(x + 15, y + 13, 5, 4, 308, 52, citrus);
    c.fillArc(x + 15, y + 13, 9, 8, 308, 52, grey);
  } else if (name == "shake") {
    c.fillRoundRect(x + 8, y + 3, 11, 22, 2, light);
    c.fillRect(x + 10, y + 5, 7, 15, black);
    c.fillArc(x + 13, y + 14, 12, 11, 150, 210, citrus);
    c.fillArc(x + 13, y + 14, 12, 11, 330, 30, citrus);
  } else if (name == "paint") {
    c.fillEllipse(x + 13, y + 14, 12, 10, guideRgb(c, 0xC79A64));
    c.fillCircle(x + 7, y + 12, 2, red);
    c.fillCircle(x + 12, y + 8, 2, citrus);
    c.fillCircle(x + 19, y + 11, 2, blue);
    c.fillCircle(x + 18, y + 18, 2, green);
    c.fillCircle(x + 10, y + 19, 2, guideRgb(c, 0x101010));
  } else if (name == "camera") {
    c.fillRoundRect(x + 1, y + 7, 24, 16, 3, light);
    c.fillRect(x + 9, y + 3, 8, 5, light);
    c.fillCircle(x + 13, y + 15, 5, guideRgb(c, 0x101010));
    c.fillCircle(x + 13, y + 15, 3, blue);
  }
}

static void guideDrawTile(M5Canvas& c, const GuideTile& t, int x, int y, bool german) {
  c.fillRoundRect(x, y, 116, 50, 5, guideRgb(c, 0x1B1B1B));
  c.drawRoundRect(x, y, 116, 50, 5, guideRgb(c, 0x3A3A3A));
  guideDrawIcon(c, t.icon, x + 2, y + 2);
  c.setFreeFont(FontManager::getFont12pt());
  c.setTextColor(TFT_WHITE);
  c.setTextDatum(ML_DATUM);
  c.drawString(german ? t.titleDe : t.titleEn, x + 29, y + 15);
  c.setFont(&fonts::DejaVu9);
  c.setTextSize(1);
  c.setTextColor(guideRgb(c, 0xD7D7D7));
  c.setTextDatum(textdatum_t::top_left);
  String lines[2];
  const int n = wrapToLines(c, german ? t.sayDe : t.sayEn, 109, lines, 2);
  for (int i = 0; i < n; i++) c.drawString(lines[i], x + 4, y + 29 + i * 10);
}

// Draws whichever pages are on screen at this slide position, then the page dots and the counter
void guideDrawGrid(M5Canvas& c, float pos, int rotation) {
  const bool german = deviceLanguageIsGerman();
  for (int page = 0; page < GUIDE_PAGES; page++) {
    const int offset = (int)roundf((page - pos) * WIDTH);
    if (offset <= -WIDTH || offset >= WIDTH) continue;
    for (int i = 0; i < GUIDE_TILES_PER_PAGE; i++) {
      const GuideTile& tile = GUIDE_TILES[page * GUIDE_TILES_PER_PAGE + i];
      guideDrawTile(c, tile, offset + 3 + (i % 2) * 118, 16 + (i / 2) * 53, german);
    }
  }
  const int current = (int)roundf(pos);
  const uint16_t citrus = guideRgb(c, 0xF2B705), dim = guideRgb(c, 0x8A8A8A);
  // At the top right, clear of Button B's tab when the stick is turned around
  for (int page = 0; page < GUIDE_PAGES; page++) {
    c.fillCircle(176 + page * 16, 6, page == current ? 3 : 2, page == current ? citrus : dim);
  }
  c.setFont(&fonts::DejaVu9);
  c.setTextSize(1);
  c.setTextColor(TFT_LIGHTGREY);
  char counter[6];
  snprintf(counter, sizeof(counter), "%d/%d", current + 1, GUIDE_PAGES);
  // On the side away from Button C's tab
  if (rotation == 1) {
    c.setTextDatum(textdatum_t::baseline_left);
    c.drawString(counter, 4, HEIGHT - 4);
  } else {
    c.setTextDatum(textdatum_t::baseline_right);
    c.drawString(counter, WIDTH - 4, HEIGHT - 4);
  }
}
