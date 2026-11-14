// A canvas for the host with the M5Canvas calls the shop drawing uses (sketch/sketch_jan12a/shop_draw.h), and the
// stick's own fonts, so tests/shop/render.cpp can draw the game's screens without a stick. Close to the stick's
// pictures, not pixel-exact: circles and text placement can differ by a pixel.
#pragma once

#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include <algorithm>
#include <string>
#include <vector>

#define PROGMEM
struct GFXglyph {
  uint16_t bitmapOffset;
  uint8_t width, height, xAdvance;
  int8_t xOffset, yOffset;
};
struct GFXfont {
  uint8_t* bitmap;
  GFXglyph* glyph;
  uint16_t first, last;
  uint8_t yAdvance;
};

enum : uint8_t {
  TL_DATUM = 0, TC_DATUM = 1, TR_DATUM = 2, ML_DATUM = 4, MC_DATUM = 5, MR_DATUM = 6,
  BL_DATUM = 8, BC_DATUM = 9, BR_DATUM = 10, L_BASELINE = 16, C_BASELINE = 17, R_BASELINE = 18,
};

#include "../../sketch/sketch_jan12a/fonts/PrimaryFont10pt.h"
#include "../../sketch/sketch_jan12a/fonts/PrimaryFont14pt.h"

class HostCanvas {
 public:
  HostCanvas(int w, int h) : w_(w), h_(h), px_((size_t)w * h, 0) {}
  int width() const { return w_; }
  int height() const { return h_; }
  uint16_t at(int x, int y) const { return px_[(size_t)y * w_ + x]; }

  void drawPixel(int x, int y, uint16_t c) {
    if (x >= 0 && y >= 0 && x < w_ && y < h_) px_[(size_t)y * w_ + x] = c;
  }
  void fillRect(int x, int y, int w, int h, uint16_t c) {
    for (int j = y; j < y + h; j++)
      for (int i = x; i < x + w; i++) drawPixel(i, j, c);
  }
  void fillScreen(uint16_t c) { fillRect(0, 0, w_, h_, c); }
  void drawFastHLine(int x, int y, int w, uint16_t c) { fillRect(x, y, w, 1, c); }
  void drawFastVLine(int x, int y, int h, uint16_t c) { fillRect(x, y, 1, h, c); }
  void drawRect(int x, int y, int w, int h, uint16_t c) {
    fillRect(x, y, w, 1, c);
    fillRect(x, y + h - 1, w, 1, c);
    fillRect(x, y, 1, h, c);
    fillRect(x + w - 1, y, 1, h, c);
  }
  void fillCircle(int x, int y, int r, uint16_t c) {
    for (int j = -r; j <= r; j++)
      for (int i = -r; i <= r; i++)
        if (i * i + j * j <= r * r + r) drawPixel(x + i, y + j, c);
  }
  void drawCircle(int x, int y, int r, uint16_t c) { fillArc(x, y, r, r, 0, 360, c); }
  void fillEllipse(int x, int y, int rx, int ry, uint16_t c) {
    if (rx < 1) rx = 1;
    if (ry < 1) ry = 1;
    for (int j = -ry; j <= ry; j++)
      for (int i = -rx; i <= rx; i++)
        if ((double)i * i / ((double)rx * rx) + (double)j * j / ((double)ry * ry) <= 1.0 + 0.5 / (rx > ry ? rx : ry))
          drawPixel(x + i, y + j, c);
  }
  void fillRoundRect(int x, int y, int w, int h, int r, uint16_t c) {
    for (int j = 0; j < h; j++)
      for (int i = 0; i < w; i++)
        if (inRound(i, j, w, h, r)) drawPixel(x + i, y + j, c);
  }
  void drawRoundRect(int x, int y, int w, int h, int r, uint16_t c) {
    for (int j = 0; j < h; j++)
      for (int i = 0; i < w; i++)
        if (inRound(i, j, w, h, r) && (!inRound(i - 1, j, w, h, r) || !inRound(i + 1, j, w, h, r) ||
                                       !inRound(i, j - 1, w, h, r) || !inRound(i, j + 1, w, h, r)))
          drawPixel(x + i, y + j, c);
  }
  void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t c) {
    const int minX = std::min(x0, std::min(x1, x2)), maxX = std::max(x0, std::max(x1, x2));
    const int minY = std::min(y0, std::min(y1, y2)), maxY = std::max(y0, std::max(y1, y2));
    const double area = (double)(x1 - x0) * (y2 - y0) - (double)(x2 - x0) * (y1 - y0);
    if (area == 0) {
      drawLine(x0, y0, x1, y1, c);
      drawLine(x1, y1, x2, y2, c);
      return;
    }
    for (int y = minY; y <= maxY; y++)
      for (int x = minX; x <= maxX; x++) {
        const double px = x, py = y;
        double e0 = (x1 - x0) * (py - y0) - (y1 - y0) * (px - x0);
        double e1 = (x2 - x1) * (py - y1) - (y2 - y1) * (px - x1);
        double e2 = (x0 - x2) * (py - y2) - (y0 - y2) * (px - x2);
        if (area < 0) {
          e0 = -e0;
          e1 = -e1;
          e2 = -e2;
        }
        // within half a pixel of each edge (e is the edge's length times the distance)
        if (e0 >= -edgeLen(x0, y0, x1, y1) * 0.5 && e1 >= -edgeLen(x1, y1, x2, y2) * 0.5 &&
            e2 >= -edgeLen(x2, y2, x0, y0) * 0.5)
          drawPixel(x, y, c);
      }
  }
  void drawLine(int x0, int y0, int x1, int y1, uint16_t c) {
    const int dx = abs(x1 - x0), dy = -abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
      drawPixel(x0, y0, c);
      if (x0 == x1 && y0 == y1) break;
      const int e2 = 2 * err;
      if (e2 >= dy) {
        err += dy;
        x0 += sx;
      }
      if (e2 <= dx) {
        err += dx;
        y0 += sy;
      }
    }
  }
  // Like LovyanGFX: between radii r0 and r1, from angle0 to angle1 in degrees, clockwise from three o'clock
  void fillArc(int x, int y, int r0, int r1, float a0, float a1, uint16_t c) {
    const int rin = std::min(r0, r1), rout = std::max(r0, r1);
    const bool full = a1 - a0 >= 360;
    for (int j = -rout; j <= rout; j++)
      for (int i = -rout; i <= rout; i++) {
        const double d2 = i * i + j * j;
        if (d2 > rout * rout + rout || (rin > 0 && d2 < rin * rin - rin)) continue;
        if (!full) {
          double a = atan2((double)j, (double)i) * 180 / M_PI;
          if (a < 0) a += 360;
          double from = fmod(a0, 360);
          if (from < 0) from += 360;
          double rel = a - from;
          if (rel < 0) rel += 360;
          if (rel > a1 - a0) continue;
        }
        drawPixel(x + i, y + j, c);
      }
  }

  void setFreeFont(const GFXfont* f) { font_ = f; }
  void setTextDatum(uint8_t d) { datum_ = d; }
  void setTextColor(uint16_t c) { ink_ = c; }
  void setTextColor(uint16_t c, uint16_t) { ink_ = c; }
  int textWidth(const char* s) const {
    int w = 0;
    for (uint32_t cp : decode(s)) {
      const GFXglyph* g = glyph(cp);
      if (g) w += g->xAdvance;
    }
    return w;
  }
  int textWidth(const std::string& s) const { return textWidth(s.c_str()); }
  void drawString(const char* s, int x, int y) {
    if (!font_) return;
    const int w = textWidth(s);
    const int h = datum_ & 3;
    if (h == 1) x -= w / 2;
    if (h == 2) x -= w;
    const GFXglyph* H = glyph('H');
    const int cap = H ? -H->yOffset : font_->yAdvance * 3 / 4;
    int base = y + cap;  // top
    const int v = datum_ & ~3;
    if (v == 4) base = y + cap / 2;
    if (v == 8) base = y - font_->yAdvance / 4;
    if (v == 16) base = y;
    for (uint32_t cp : decode(s)) {
      const GFXglyph* g = glyph(cp);
      if (!g) continue;
      int bit = 0;
      uint8_t bits = 0;
      const uint8_t* bm = font_->bitmap + g->bitmapOffset;
      for (int yy = 0; yy < g->height; yy++)
        for (int xx = 0; xx < g->width; xx++) {
          if (!(bit & 7)) bits = bm[bit >> 3];
          bit++;
          if (bits & 0x80) drawPixel(x + g->xOffset + xx, base + g->yOffset + yy, ink_);
          bits <<= 1;
        }
      x += g->xAdvance;
    }
  }
  void drawString(const std::string& s, int x, int y) { drawString(s.c_str(), x, y); }

  // A binary PPM, each pixel scale x scale
  bool writePpm(const std::string& path, int scale) const {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    fprintf(f, "P6\n%d %d\n255\n", w_ * scale, h_ * scale);
    std::vector<uint8_t> row((size_t)w_ * scale * 3);
    for (int y = 0; y < h_; y++) {
      for (int x = 0; x < w_; x++) {
        const uint16_t c = at(x, y);
        const uint8_t r = (uint8_t)(((c >> 11) & 31) * 255 / 31), g = (uint8_t)(((c >> 5) & 63) * 255 / 63),
                      b = (uint8_t)((c & 31) * 255 / 31);
        for (int k = 0; k < scale; k++) {
          row[((size_t)x * scale + k) * 3] = r;
          row[((size_t)x * scale + k) * 3 + 1] = g;
          row[((size_t)x * scale + k) * 3 + 2] = b;
        }
      }
      for (int k = 0; k < scale; k++) fwrite(row.data(), 1, row.size(), f);
    }
    fclose(f);
    return true;
  }

 private:
  static bool inRound(int i, int j, int w, int h, int r) {
    if (i < 0 || j < 0 || i >= w || j >= h) return false;
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;
    const double dx = i < r ? r - i - 0.5 : i >= w - r ? i - (w - r) + 0.5 : 0;
    const double dy = j < r ? r - j - 0.5 : j >= h - r ? j - (h - r) + 0.5 : 0;
    return !(dx > 0 && dy > 0 && dx * dx + dy * dy > (double)r * r);
  }
  static double edgeLen(int x0, int y0, int x1, int y1) { return sqrt((double)(x1 - x0) * (x1 - x0) + (double)(y1 - y0) * (y1 - y0)); }
  const GFXglyph* glyph(uint32_t cp) const {
    if (!font_ || cp < font_->first || cp > font_->last) return nullptr;
    return &font_->glyph[cp - font_->first];
  }
  static std::vector<uint32_t> decode(const char* s) {
    std::vector<uint32_t> out;
    for (const unsigned char* p = (const unsigned char*)s; *p;) {
      uint32_t cp = *p;
      int more = 0;
      if (cp >= 0xF0) cp &= 7, more = 3;
      else if (cp >= 0xE0) cp &= 15, more = 2;
      else if (cp >= 0xC0) cp &= 31, more = 1;
      p++;
      while (more-- && (*p & 0xC0) == 0x80) cp = (cp << 6) | (*p++ & 63);
      out.push_back(cp);
    }
    return out;
  }
  int w_, h_;
  std::vector<uint16_t> px_;
  const GFXfont* font_ = nullptr;
  uint8_t datum_ = TL_DATUM;
  uint16_t ink_ = 0xFFFF;
};
