// A small software stand-in for M5Canvas, so the real drawing code runs on the host: it renders
// to pictures anyone can look at, and reports text that leaves the screen or has no glyph.
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#define PROGMEM
struct GFXglyph {
  uint32_t bitmapOffset;
  uint8_t width, height, xAdvance;
  int8_t xOffset, yOffset;
};
struct GFXfont {
  uint8_t* bitmap;
  GFXglyph* glyph;
  uint16_t first, last;
  uint8_t yAdvance;
};
#include "../../sketch/sketch_jan12a/fonts/PrimaryFont10pt.h"
#include "../../sketch/sketch_jan12a/fonts/PrimaryFont14pt.h"

enum class textdatum_t { baseline_left, baseline_center, baseline_right };

class M5Canvas {
 public:
  static const int W = 240, H = 135;
  uint16_t px[H][W];
  int textProblems = 0;
  std::string context;

  void fillSprite(uint16_t c) {
    for (auto& row : px) for (auto& p : row) p = c;
    drawnCount = 0;
  }
  void drawPixel(int x, int y, uint16_t c) { if (x >= 0 && y >= 0 && x < W && y < H) px[y][x] = c; }
  void fillRect(int x, int y, int w, int h, uint16_t c) {
    for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) drawPixel(i, j, c);
  }
  void drawRect(int x, int y, int w, int h, uint16_t c) {
    drawFastHLine(x, y, w, c); drawFastHLine(x, y + h - 1, w, c);
    drawFastVLine(x, y, h, c); drawFastVLine(x + w - 1, y, h, c);
  }
  void drawFastHLine(int x, int y, int w, uint16_t c) { for (int i = 0; i < w; i++) drawPixel(x + i, y, c); }
  void drawFastVLine(int x, int y, int h, uint16_t c) { for (int i = 0; i < h; i++) drawPixel(x, y + i, c); }
  void drawLine(int x0, int y0, int x1, int y1, uint16_t c) {
    int dx = abs(x1 - x0), dy = -abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, e = dx + dy;
    for (int guard = 0; guard < 100000; guard++) {
      drawPixel(x0, y0, c);
      if (x0 == x1 && y0 == y1) break;
      int e2 = 2 * e;
      if (e2 >= dy) { e += dy; x0 += sx; }
      if (e2 <= dx) { e += dx; y0 += sy; }
    }
  }
  void fillCircle(int x, int y, int r, uint16_t c) {
    for (int j = -r; j <= r; j++) for (int i = -r; i <= r; i++) if (i * i + j * j <= r * r + r) drawPixel(x + i, y + j, c);
  }
  void drawCircle(int x, int y, int r, uint16_t c) {
    for (int j = -r; j <= r; j++) for (int i = -r; i <= r; i++) {
      const int d = i * i + j * j;
      if (d <= r * r + r && d >= r * r - r) drawPixel(x + i, y + j, c);
    }
  }
  void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t c) {
    int minY = std::max(0, std::min({y0, y1, y2})), maxY = std::min(H - 1, std::max({y0, y1, y2}));
    int minX = std::max(0, std::min({x0, x1, x2})), maxX = std::min(W - 1, std::max({x0, x1, x2}));
    for (int y = minY; y <= maxY; y++) for (int x = minX; x <= maxX; x++) {
      const int a = (x1 - x0) * (y - y0) - (y1 - y0) * (x - x0);
      const int b = (x2 - x1) * (y - y1) - (y2 - y1) * (x - x1);
      const int d = (x0 - x2) * (y - y2) - (y0 - y2) * (x - x2);
      if ((a >= 0 && b >= 0 && d >= 0) || (a <= 0 && b <= 0 && d <= 0)) drawPixel(x, y, c);
    }
  }
  void setFreeFont(const GFXfont* f) { font = f; }
  void setTextSize(int) {}
  void setTextDatum(textdatum_t d) { datum = d; }
  void setTextColor(uint16_t c) { textColor = c; }
  int textWidth(const char* s) {
    if (!font) { problem("no font set", s); return 0; }
    int w = 0;
    forEachChar(s, [&](uint32_t u) {
      if (u < font->first || u > font->last) { problem("no glyph", s); return; }
      w += font->glyph[u - font->first].xAdvance;
    });
    return w;
  }
  void drawString(const char* s, int x, int y) {
    const int w = textWidth(s);
    if (datum == textdatum_t::baseline_center) x -= w / 2;
    if (datum == textdatum_t::baseline_right) x -= w;
    if (x < 0 || x + w > W) problem("text runs off the side", s);
    if (!inFooter) checkTextPlace(s, x, y, w);
    forEachChar(s, [&](uint32_t u) {
      if (!font || u < font->first || u > font->last) return;
      const GFXglyph& g = font->glyph[u - font->first];
      const uint8_t* bits = font->bitmap + g.bitmapOffset;
      int bit = 0;
      for (int j = 0; j < g.height; j++) for (int i = 0; i < g.width; i++, bit++) {
        if (bits[bit >> 3] & (0x80 >> (bit & 7))) drawPixel(x + g.xOffset + i, y + g.yOffset + j, textColor);
      }
      x += g.xAdvance;
    });
  }
  // Footer as the firmware draws it, close enough to judge the layout
  void footerStandIn(const std::string& label) {
    fillRect(0, H - 18, W, 18, 0);
    const GFXfont* saved = font;
    font = &Roboto_Regular10pt8b;
    datum = textdatum_t::baseline_left;
    textColor = 0xC618;
    inFooter = true;
    drawString(label.c_str(), 2, H - 4);
    inFooter = false;
    font = saved;
  }
  void writePpm(const std::string& path, int scale) const {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", W * scale, H * scale);
    for (int y = 0; y < H * scale; y++) for (int x = 0; x < W * scale; x++) {
      const uint16_t p = px[y / scale][x / scale];
      const uint8_t rgb[3] = {(uint8_t)((p >> 11) * 255 / 31), (uint8_t)(((p >> 5) & 63) * 255 / 63), (uint8_t)((p & 31) * 255 / 31)};
      fwrite(rgb, 1, 3, f);
    }
    fclose(f);
  }

 private:
  const GFXfont* font = nullptr;
  textdatum_t datum = textdatum_t::baseline_left;
  uint16_t textColor = 0xFFFF;
  bool inFooter = false;
  struct Box { int x0, y0, x1, y1; };
  Box drawn[32];
  int drawnCount = 0;
  // Real glyph extents, so a descender cut off by the footer counts, and so does text on text
  void checkTextPlace(const char* s, int x, int y, int w) {
    if (!font) return;
    int top = y, bottom = y;
    forEachChar(s, [&](uint32_t u) {
      if (u < font->first || u > font->last) return;
      const GFXglyph& g = font->glyph[u - font->first];
      if (g.width == 0 || u == ' ') return;
      top = std::min(top, y + g.yOffset);
      bottom = std::max(bottom, y + g.yOffset + g.height);
    });
    if (top < 0) problem("text above the screen", s);
    if (bottom > H - 18) problem("text cut off by the footer", s);
    const Box box = {x, top, x + w, bottom};
    for (int i = 0; i < drawnCount; i++) {
      const Box& o = drawn[i];
      if (box.x0 < o.x1 && o.x0 < box.x1 && box.y0 < o.y1 && o.y0 < box.y1) problem("text over other text", s);
    }
    if (drawnCount < 32) drawn[drawnCount++] = box;
  }
  void problem(const char* what, const char* s) {
    textProblems++;
    fprintf(stderr, "  TEXT %s: %s: '%s'\n", context.c_str(), what, s);
  }
  template <class F> static void forEachChar(const char* s, F f) {
    for (const uint8_t* p = (const uint8_t*)s; *p;) {
      uint32_t u = *p;
      int extra = 0;
      if (u >= 0xE0) { u &= 0x0F; extra = 2; }
      else if (u >= 0xC0) { u &= 0x1F; extra = 1; }
      p++;
      while (extra-- > 0 && *p) u = (u << 6) | (*p++ & 0x3F);
      f(u);
    }
  }
};
