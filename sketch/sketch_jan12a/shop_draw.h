// Drawing of the shop game: the places, the guests and sellers, the goods, the speech bubble and the shopping list.
// Everything is made of circles, triangles and rectangles, so every guest can look different and nothing needs a
// picture file. Templates over the canvas: the stick draws on an M5Canvas, tests/shop/render.cpp on a host canvas
// with the same calls, which makes the PNGs in screenshots/shop. Requirements: docs/features/shop.md
#pragma once

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "shop_core.h"

static inline constexpr uint16_t shopRgb(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

struct ShopFonts {
  const GFXfont* small;  // 10 pt
  const GFXfont* big;    // 14 pt
};
static ShopFonts shopFonts = {nullptr, nullptr};

static const int SHOP_W = 240, SHOP_H = 135;
static const int SHOP_COUNTER_Y = 96;  // the top of the counter
static const uint16_t SHOP_INK = shopRgb(40, 32, 30);
static const uint16_t SHOP_WHITE = shopRgb(255, 255, 255);
static const uint16_t SHOP_OK = shopRgb(47, 163, 90);
static const uint16_t SHOP_BAD = shopRgb(224, 69, 58);
static const uint16_t SHOP_CITRUS = shopRgb(255, 214, 10);

enum ShopMood : uint8_t { SHOP_MOOD_HAPPY, SHOP_MOOD_TALK, SHOP_MOOD_NEUTRAL, SHOP_MOOD_SAD };
enum ShopBubble : uint8_t { SHOP_BUBBLE_NONE, SHOP_BUBBLE_DOTS, SHOP_BUBBLE_PICTURES, SHOP_BUBBLE_HEART };
enum ShopMark : uint8_t { SHOP_MARK_NONE, SHOP_MARK_OK, SHOP_MARK_BAD };

// One screen of the game
struct ShopScene {
  ShopPlace place = SHOP_BAR;
  ShopRole role = SHOP_SELLER;
  ShopLook person{};            // the guest (child sells) or the seller (child shops)
  int personX = 92;             // the head's centre, moves while a guest walks in
  ShopMood mood = SHOP_MOOD_HAPPY;
  ShopBubble bubble = SHOP_BUBBLE_NONE;
  std::vector<ShopLine> order;  // the guest's order (bubble pictures) or the child's shopping list
  std::vector<uint8_t> marks;   // per line of the shopping list: ShopMark
  std::vector<ShopLine> counter;  // what lies on the counter
  std::vector<uint8_t> stars;     // one per guest or list of the round: 0 to come, 1 happy, 2 not
  int recording = -1;             // 0..100: the microphone is on
  const char* note = nullptr;     // one small line on the counter
};

// ---------- Small helpers ----------

struct ShopPen {
  int cx, cy;
  float f;
  int X(float v) const { return cx + (int)lroundf(v * f); }
  int Y(float v) const { return cy + (int)lroundf(v * f); }
  int S(float v) const {
    const int s = (int)lroundf(v * f);
    return s < 1 ? 1 : s;
  }
};

template <class C>
static void shopQuad(C& c, int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3, uint16_t col) {
  c.fillTriangle(x0, y0, x1, y1, x2, y2, col);
  c.fillTriangle(x0, y0, x2, y2, x3, y3, col);
}

// A line w pixels wide with round ends
template <class C>
static void shopThick(C& c, float x0, float y0, float x1, float y1, float w, uint16_t col) {
  const float dx = x1 - x0, dy = y1 - y0, len = sqrtf(dx * dx + dy * dy);
  const int r = (int)lroundf(w / 2) < 1 ? 1 : (int)lroundf(w / 2);
  if (len > 0.5f) {
    const float nx = -dy / len * w / 2, ny = dx / len * w / 2;
    shopQuad(c, (int)lroundf(x0 + nx), (int)lroundf(y0 + ny), (int)lroundf(x1 + nx), (int)lroundf(y1 + ny),
             (int)lroundf(x1 - nx), (int)lroundf(y1 - ny), (int)lroundf(x0 - nx), (int)lroundf(y0 - ny), col);
  }
  c.fillCircle((int)lroundf(x0), (int)lroundf(y0), r, col);
  c.fillCircle((int)lroundf(x1), (int)lroundf(y1), r, col);
}

template <class C>
static void shopCheck(C& c, int x, int y, int s, uint16_t col) {
  shopThick(c, x - s, y, x - s / 3, y + s * 2 / 3, s * 0.45f, col);
  shopThick(c, x - s / 3, y + s * 2 / 3, x + s, y - s * 2 / 3, s * 0.45f, col);
}

template <class C>
static void shopCross(C& c, int x, int y, int s, uint16_t col) {
  shopThick(c, x - s, y - s, x + s, y + s, s * 0.45f, col);
  shopThick(c, x - s, y + s, x + s, y - s, s * 0.45f, col);
}

template <class C>
static void shopHeart(C& c, int x, int y, int s, uint16_t col) {
  c.fillCircle(x - s / 2, y - s / 4, s / 2 + 1, col);
  c.fillCircle(x + s / 2, y - s / 4, s / 2 + 1, col);
  c.fillTriangle(x - s - 1, y - s / 6, x + s + 1, y - s / 6, x, y + s, col);
}

// The text cut to maxW pixels with "..." (in the font set last)
template <class C>
static std::string shopFit(C& c, const char* text, int maxW) {
  std::string s = text ? text : "";
  if (c.textWidth(s.c_str()) <= maxW) return s;
  while (!s.empty()) {
    do s.pop_back();
    while (!s.empty() && ((unsigned char)s.back() & 0xC0) == 0x80);
    if (!s.empty() && ((unsigned char)s.back() & 0xC0) == 0xC0) s.pop_back();
    if (c.textWidth((s + "...").c_str()) <= maxW) return s + "...";
  }
  return s;
}

template <class C>
static void shopText(C& c, const char* text, int x, int y, uint8_t datum, uint16_t col, bool big) {
  c.setFreeFont(big ? shopFonts.big : shopFonts.small);
  c.setTextDatum(datum);
  c.setTextColor(col);
  c.drawString(text, x, y);
}

// ---------- Goods ----------

// A glass, filled with a drink
template <class C>
static void shopGlass(C& c, const ShopPen& p, uint16_t liquid) {
  const uint16_t glass = shopRgb(226, 244, 255), edge = shopRgb(120, 175, 210);
  shopQuad(c, p.X(-8), p.Y(-10), p.X(8), p.Y(-10), p.X(6), p.Y(11), p.X(-6), p.Y(11), glass);
  shopQuad(c, p.X(-7.2f), p.Y(-4), p.X(7.2f), p.Y(-4), p.X(5.6f), p.Y(10), p.X(-5.6f), p.Y(10), liquid);
  shopThick(c, p.X(-8), p.Y(-10), p.X(-6), p.Y(11), p.f * 1.2f, edge);
  shopThick(c, p.X(8), p.Y(-10), p.X(6), p.Y(11), p.f * 1.2f, edge);
  shopThick(c, p.X(-6), p.Y(11), p.X(6), p.Y(11), p.f * 1.2f, edge);
  c.fillRect(p.X(-4), p.Y(-1), p.S(2), p.S(7), SHOP_WHITE);  // shine
}

// A cup or a mug with a handle
template <class C>
static void shopCup(C& c, const ShopPen& p, uint16_t cup, uint16_t drink, bool cream) {
  c.fillArc(p.X(8), p.Y(3), p.S(2.5f), p.S(5.5f), 0, 360, cup);
  c.fillRoundRect(p.X(-9), p.Y(-4), p.S(17), p.S(15), p.S(4), cup);
  c.fillEllipse(p.X(-0.5f), p.Y(-4), p.S(8.5f), p.S(2.5f), drink);
  if (cream) {
    c.fillCircle(p.X(-5), p.Y(-6), p.S(3.5f), SHOP_WHITE);
    c.fillCircle(p.X(0), p.Y(-8), p.S(4.5f), SHOP_WHITE);
    c.fillCircle(p.X(4), p.Y(-6), p.S(3.5f), SHOP_WHITE);
  } else {
    shopThick(c, p.X(-3), p.Y(-9), p.X(-1), p.Y(-13), p.f * 1.3f, shopRgb(200, 205, 210));
    shopThick(c, p.X(2), p.Y(-9), p.X(4), p.Y(-13), p.f * 1.3f, shopRgb(200, 205, 210));
  }
}

template <class C>
static void shopDrawItem(C& c, ShopItem item, int cx, int cy, int size) {
  const ShopPen p{cx, cy, size / 24.0f};
  switch (item) {
    case ShopItem::Lemonade:
      shopGlass(c, p, shopRgb(255, 230, 90));
      shopThick(c, p.X(2), p.Y(-4), p.X(7), p.Y(-14), p.f * 2.2f, shopRgb(255, 90, 95));
      c.fillCircle(p.X(-7), p.Y(-9), p.S(4.5f), shopRgb(255, 212, 59));
      c.fillCircle(p.X(-7), p.Y(-9), p.S(2.5f), shopRgb(255, 240, 160));
      break;
    case ShopItem::OrangeJuice:
      shopGlass(c, p, shopRgb(255, 159, 28));
      c.fillCircle(p.X(7), p.Y(-9), p.S(4.5f), shopRgb(255, 140, 0));
      c.fillCircle(p.X(7), p.Y(-9), p.S(2.5f), shopRgb(255, 200, 120));
      break;
    case ShopItem::AppleJuice:
      shopGlass(c, p, shopRgb(222, 160, 60));
      c.fillCircle(p.X(8), p.Y(-9), p.S(4.5f), shopRgb(120, 200, 80));
      c.fillEllipse(p.X(9), p.Y(-14), p.S(2.5f), p.S(1.5f), shopRgb(60, 140, 50));
      break;
    case ShopItem::Water:
      shopGlass(c, p, shopRgb(150, 210, 255));
      c.fillCircle(p.X(2), p.Y(2), p.S(1.3f), SHOP_WHITE);
      c.fillCircle(p.X(-1), p.Y(6), p.S(1), SHOP_WHITE);
      c.fillCircle(p.X(3), p.Y(7), p.S(1), SHOP_WHITE);
      break;
    case ShopItem::Tea:
      c.fillEllipse(p.X(0), p.Y(11), p.S(12), p.S(2.5f), shopRgb(200, 205, 215));
      shopCup(c, p, shopRgb(78, 205, 196), shopRgb(170, 95, 40), false);
      break;
    case ShopItem::HotChocolate:
      shopCup(c, p, shopRgb(231, 111, 81), shopRgb(110, 60, 30), true);
      break;
    case ShopItem::IceCream:
      c.fillTriangle(p.X(-7), p.Y(-1), p.X(7), p.Y(-1), p.X(0), p.Y(12), shopRgb(233, 184, 114));
      shopThick(c, p.X(-4), p.Y(1), p.X(2), p.Y(8), p.f, shopRgb(190, 135, 70));
      shopThick(c, p.X(4), p.Y(1), p.X(-2), p.Y(8), p.f, shopRgb(190, 135, 70));
      c.fillCircle(p.X(0), p.Y(-5), p.S(7.5f), shopRgb(255, 143, 171));
      c.fillCircle(p.X(-4), p.Y(-1), p.S(3.5f), shopRgb(255, 143, 171));
      c.fillCircle(p.X(4), p.Y(-1), p.S(3.5f), shopRgb(255, 143, 171));
      c.fillCircle(p.X(1), p.Y(-13), p.S(2.5f), SHOP_BAD);
      break;
    case ShopItem::Cookie:
      c.fillCircle(p.X(0), p.Y(0), p.S(11), shopRgb(190, 130, 70));
      c.fillCircle(p.X(0), p.Y(0), p.S(9.5f), shopRgb(222, 165, 95));
      c.fillCircle(p.X(-4), p.Y(-4), p.S(1.8f), shopRgb(80, 45, 25));
      c.fillCircle(p.X(4), p.Y(-3), p.S(1.8f), shopRgb(80, 45, 25));
      c.fillCircle(p.X(-1), p.Y(3), p.S(1.8f), shopRgb(80, 45, 25));
      c.fillCircle(p.X(5), p.Y(5), p.S(1.8f), shopRgb(80, 45, 25));
      c.fillCircle(p.X(-5), p.Y(5), p.S(1.5f), shopRgb(80, 45, 25));
      break;
    case ShopItem::Bread:
      c.fillEllipse(p.X(0), p.Y(3), p.S(12), p.S(8), shopRgb(176, 106, 50));
      c.fillEllipse(p.X(0), p.Y(1), p.S(11), p.S(6), shopRgb(214, 146, 74));
      for (int k = -1; k <= 1; k++)
        shopThick(c, p.X(k * 6 - 2), p.Y(4), p.X(k * 6 + 2), p.Y(-2), p.f * 1.5f, shopRgb(245, 205, 140));
      break;
    case ShopItem::Roll:
      c.fillCircle(p.X(0), p.Y(1), p.S(10), shopRgb(186, 112, 52));
      c.fillCircle(p.X(-1), p.Y(0), p.S(8.5f), shopRgb(220, 150, 76));
      shopThick(c, p.X(-6), p.Y(0), p.X(5), p.Y(0), p.f * 1.5f, shopRgb(245, 205, 140));
      shopThick(c, p.X(-1), p.Y(-6), p.X(-1), p.Y(6), p.f * 1.5f, shopRgb(245, 205, 140));
      break;
    case ShopItem::Croissant:
      c.fillArc(p.X(0), p.Y(8), p.S(5), p.S(13), 195, 345, shopRgb(196, 120, 40));
      c.fillArc(p.X(0), p.Y(8), p.S(6), p.S(12), 198, 342, shopRgb(242, 176, 79));
      for (int k = 0; k < 4; k++) {
        const float a = (225 + k * 30) * 3.14159f / 180;
        shopThick(c, p.X(cosf(a) * 6), p.Y(8 + sinf(a) * 6), p.X(cosf(a) * 12), p.Y(8 + sinf(a) * 12), p.f,
                  shopRgb(196, 120, 40));
      }
      break;
    case ShopItem::Pretzel: {
      // A rope: the loop on top, both ends cross in the middle and rest on the loop's sides
      const uint16_t b = shopRgb(176, 98, 40);
      c.fillArc(p.X(0), p.Y(1), p.S(7.5f), p.S(11), 145, 395, b);
      shopThick(c, p.X(-9), p.Y(7), p.X(7), p.Y(-6), p.f * 3.4f, b);
      shopThick(c, p.X(9), p.Y(7), p.X(-7), p.Y(-6), p.f * 3.4f, b);
      c.fillCircle(p.X(-6), p.Y(-6), p.S(0.9f), SHOP_WHITE);
      c.fillCircle(p.X(5), p.Y(-8), p.S(0.9f), SHOP_WHITE);
      c.fillCircle(p.X(0), p.Y(1), p.S(0.9f), SHOP_WHITE);
      c.fillCircle(p.X(-9), p.Y(1), p.S(0.9f), SHOP_WHITE);
      break;
    }
    case ShopItem::Cake:
      c.fillTriangle(p.X(-11), p.Y(-1), p.X(11), p.Y(-1), p.X(11), p.Y(-8), shopRgb(255, 250, 240));
      c.fillRect(p.X(-11), p.Y(-1), p.S(22), p.S(12), shopRgb(247, 208, 138));
      c.fillRect(p.X(-11), p.Y(3), p.S(22), p.S(3), shopRgb(255, 120, 150));
      c.fillRect(p.X(-11), p.Y(-1), p.S(22), p.S(2), SHOP_WHITE);
      c.fillCircle(p.X(7), p.Y(-9), p.S(3), SHOP_BAD);
      break;
    case ShopItem::Muffin:
      shopQuad(c, p.X(-8), p.Y(0), p.X(8), p.Y(0), p.X(6), p.Y(11), p.X(-6), p.Y(11), shopRgb(77, 150, 255));
      for (int k = -1; k <= 1; k++) shopThick(c, p.X(k * 4), p.Y(1), p.X(k * 3.5f), p.Y(10), p.f, shopRgb(160, 200, 255));
      c.fillEllipse(p.X(0), p.Y(-2), p.S(10), p.S(8), shopRgb(140, 90, 45));
      c.fillCircle(p.X(-4), p.Y(-4), p.S(1.6f), shopRgb(60, 40, 100));
      c.fillCircle(p.X(3), p.Y(-6), p.S(1.6f), shopRgb(60, 40, 100));
      c.fillCircle(p.X(4), p.Y(-1), p.S(1.6f), shopRgb(60, 40, 100));
      break;
    case ShopItem::Doughnut: {
      c.fillArc(p.X(0), p.Y(0), p.S(4), p.S(11), 0, 360, shopRgb(214, 150, 80));
      c.fillArc(p.X(0), p.Y(-1), p.S(4.5f), p.S(9.5f), 0, 360, shopRgb(255, 143, 171));
      const uint16_t sprinkle[] = {SHOP_WHITE, shopRgb(77, 150, 255), SHOP_CITRUS, shopRgb(120, 220, 120)};
      for (int k = 0; k < 8; k++) {
        const float a = k * 45 * 3.14159f / 180;
        c.fillRect(p.X(cosf(a) * 7), p.Y(-1 + sinf(a) * 7), p.S(1.6f), p.S(1.6f), sprinkle[k % 4]);
      }
      break;
    }
    case ShopItem::Sandwich:
      c.fillRoundRect(p.X(-11), p.Y(4), p.S(22), p.S(6), p.S(2), shopRgb(222, 165, 95));
      c.fillRect(p.X(-11), p.Y(1), p.S(22), p.S(3), shopRgb(230, 70, 60));
      c.fillTriangle(p.X(-10), p.Y(1), p.X(10), p.Y(1), p.X(6), p.Y(5), shopRgb(255, 212, 59));
      for (int k = 0; k < 5; k++) c.fillCircle(p.X(-9 + k * 4.5f), p.Y(-1), p.S(2.2f), shopRgb(110, 190, 70));
      c.fillRoundRect(p.X(-11), p.Y(-9), p.S(22), p.S(8), p.S(4), shopRgb(222, 165, 95));
      c.fillRoundRect(p.X(-10), p.Y(-8), p.S(20), p.S(5), p.S(3), shopRgb(245, 205, 140));
      break;
    case ShopItem::Apple:
      c.fillCircle(p.X(-3.5f), p.Y(2), p.S(8.5f), shopRgb(230, 57, 70));
      c.fillCircle(p.X(3.5f), p.Y(2), p.S(8.5f), shopRgb(230, 57, 70));
      shopThick(c, p.X(0), p.Y(-6), p.X(1), p.Y(-11), p.f * 1.6f, shopRgb(110, 70, 40));
      c.fillEllipse(p.X(5), p.Y(-9), p.S(4), p.S(2), shopRgb(90, 170, 70));
      c.fillCircle(p.X(-5), p.Y(-1), p.S(2), shopRgb(255, 170, 170));
      break;
    case ShopItem::Banana:
      c.fillArc(p.X(-2), p.Y(-10), p.S(13), p.S(19), 25, 145, shopRgb(255, 220, 60));
      c.fillArc(p.X(-2), p.Y(-10), p.S(13), p.S(15), 30, 140, shopRgb(240, 190, 30));
      c.fillCircle(p.X(-2 + 16 * cosf(0.44f)), p.Y(-10 + 16 * sinf(0.44f)), p.S(2), shopRgb(110, 80, 40));
      break;
    case ShopItem::Orange:
      c.fillCircle(p.X(0), p.Y(2), p.S(10), shopRgb(255, 150, 20));
      c.fillCircle(p.X(-3), p.Y(-1), p.S(2.5f), shopRgb(255, 190, 90));
      c.fillEllipse(p.X(4), p.Y(-9), p.S(4), p.S(2), shopRgb(90, 170, 70));
      c.fillCircle(p.X(0), p.Y(-8), p.S(1.3f), shopRgb(110, 80, 40));
      break;
    case ShopItem::Carrot:
      c.fillTriangle(p.X(-6), p.Y(-5), p.X(6), p.Y(-5), p.X(0), p.Y(12), shopRgb(247, 127, 0));
      shopThick(c, p.X(-3), p.Y(0), p.X(0), p.Y(0), p.f, shopRgb(200, 90, 0));
      shopThick(c, p.X(1), p.Y(5), p.X(3), p.Y(5), p.f, shopRgb(200, 90, 0));
      c.fillTriangle(p.X(-5), p.Y(-5), p.X(-1), p.Y(-5), p.X(-6), p.Y(-13), shopRgb(80, 170, 70));
      c.fillTriangle(p.X(-2), p.Y(-5), p.X(2), p.Y(-5), p.X(0), p.Y(-14), shopRgb(100, 190, 80));
      c.fillTriangle(p.X(1), p.Y(-5), p.X(5), p.Y(-5), p.X(6), p.Y(-13), shopRgb(80, 170, 70));
      break;
    case ShopItem::Tomato:
      c.fillEllipse(p.X(0), p.Y(2), p.S(11), p.S(9), shopRgb(225, 50, 45));
      c.fillCircle(p.X(-4), p.Y(-1), p.S(2), shopRgb(255, 140, 130));
      for (int k = 0; k < 5; k++) {
        const float a = (k * 72 - 90) * 3.14159f / 180;
        shopThick(c, p.X(0), p.Y(-6), p.X(cosf(a) * 5), p.Y(-6 + sinf(a) * 3), p.f * 1.8f, shopRgb(70, 160, 60));
      }
      break;
    case ShopItem::Egg:
      c.fillEllipse(p.X(0), p.Y(1), p.S(8.5f), p.S(11), shopRgb(200, 170, 130));
      c.fillEllipse(p.X(0), p.Y(1), p.S(7.5f), p.S(10), shopRgb(255, 243, 214));
      c.fillEllipse(p.X(-3), p.Y(-3), p.S(1.5f), p.S(3), SHOP_WHITE);
      break;
    case ShopItem::Milk: {
      const uint16_t edge = shopRgb(90, 140, 200);
      c.fillTriangle(p.X(-8), p.Y(-4), p.X(8), p.Y(-4), p.X(0), p.Y(-11), edge);
      c.fillTriangle(p.X(-6), p.Y(-5), p.X(6), p.Y(-5), p.X(0), p.Y(-9), SHOP_WHITE);
      c.fillRect(p.X(-2), p.Y(-14), p.S(4), p.S(4), edge);
      c.fillRect(p.X(-8), p.Y(-4), p.S(16), p.S(16), edge);
      c.fillRect(p.X(-7), p.Y(-3), p.S(14), p.S(14), SHOP_WHITE);
      c.fillRect(p.X(-7), p.Y(3), p.S(14), p.S(5), shopRgb(77, 150, 255));
      break;
    }
    case ShopItem::Cheese:
      c.fillTriangle(p.X(-11), p.Y(-1), p.X(11), p.Y(-1), p.X(11), p.Y(-9), shopRgb(255, 235, 120));
      c.fillRect(p.X(-11), p.Y(-1), p.S(22), p.S(11), shopRgb(255, 205, 50));
      c.fillCircle(p.X(-5), p.Y(4), p.S(2.2f), shopRgb(230, 170, 30));
      c.fillCircle(p.X(3), p.Y(2), p.S(1.6f), shopRgb(230, 170, 30));
      c.fillCircle(p.X(6), p.Y(7), p.S(2), shopRgb(230, 170, 30));
      break;
    default: break;
  }
}

// ---------- People ----------

static const uint16_t SHOP_SKIN[SHOP_SKINS] = {shopRgb(253, 220, 196), shopRgb(245, 198, 160), shopRgb(224, 164, 122),
                                              shopRgb(198, 134, 84),  shopRgb(141, 90, 59),   shopRgb(98, 62, 40)};
static const uint16_t SHOP_SKIN_SHADE[SHOP_SKINS] = {shopRgb(240, 180, 150), shopRgb(225, 165, 125),
                                                    shopRgb(200, 135, 95),  shopRgb(170, 105, 60),
                                                    shopRgb(115, 70, 45),   shopRgb(75, 45, 30)};
static const uint16_t SHOP_HAIR[SHOP_HAIRS] = {shopRgb(43, 27, 18),   shopRgb(95, 58, 35),  shopRgb(170, 85, 40),
                                              shopRgb(235, 195, 110), shopRgb(120, 120, 125), shopRgb(244, 143, 177),
                                              shopRgb(25, 25, 30)};
static const uint16_t SHOP_SHIRT[SHOP_SHIRTS] = {shopRgb(126, 217, 87), shopRgb(255, 201, 60),  shopRgb(255, 107, 107),
                                                shopRgb(77, 150, 255),  shopRgb(155, 93, 229),  shopRgb(0, 187, 249),
                                                shopRgb(241, 91, 181),  shopRgb(255, 159, 28)};

// A person behind the counter: head centre (cx, cy), radius 20 at f = 1. The counter hides the body below.
template <class C>
static void shopDrawPerson(C& c, const ShopLook& look, int cx, int cy, uint8_t hat, ShopMood mood, bool apron,
                           float f = 1.0f) {
  const ShopPen p{cx, cy, f};
  const uint16_t skin = SHOP_SKIN[look.skin % SHOP_SKINS];
  const uint16_t hair = SHOP_HAIR[look.hair % SHOP_HAIRS];
  const uint16_t shirt = SHOP_SHIRT[look.shirt % SHOP_SHIRTS];
  const uint16_t dark = shopRgb(40, 28, 24);
  const uint16_t hood = hat == SHOP_HAT_FROG ? shopRgb(110, 200, 70) : hat == SHOP_HAT_BEAR ? shopRgb(150, 100, 60) : shirt;

  // Body, neck
  c.fillRoundRect(p.X(-27), p.Y(17), p.S(54), p.S(50), p.S(15), shirt);
  if (apron) {
    c.fillRoundRect(p.X(-15), p.Y(27), p.S(30), p.S(40), p.S(5), SHOP_WHITE);
    shopThick(c, p.X(-13), p.Y(28), p.X(-17), p.Y(19), f * 2.5f, SHOP_WHITE);
    shopThick(c, p.X(13), p.Y(28), p.X(17), p.Y(19), f * 2.5f, SHOP_WHITE);
    c.fillRoundRect(p.X(-7), p.Y(38), p.S(14), p.S(9), p.S(2), shopRgb(225, 230, 235));
  }
  c.fillRect(p.X(-6), p.Y(12), p.S(12), p.S(9), skin);
  c.fillTriangle(p.X(-7), p.Y(17), p.X(7), p.Y(17), p.X(0), p.Y(24), skin);

  // Behind the head: hood, long hair, pigtails, bun
  if (hat == SHOP_HAT_FROG || hat == SHOP_HAT_BEAR) c.fillCircle(p.X(0), p.Y(-1), p.S(26), hood);
  if (hat == SHOP_HAT_NONE || hat == SHOP_HAT_CAT) {
    if (look.style == 1) c.fillRoundRect(p.X(-23), p.Y(-18), p.S(46), p.S(40), p.S(14), hair);
    if (look.style == 4) {
      c.fillCircle(p.X(-25), p.Y(6), p.S(8), hair);
      c.fillCircle(p.X(25), p.Y(6), p.S(8), hair);
    }
    if (look.style == 2) c.fillCircle(p.X(0), p.Y(-23), p.S(9), hair);
  }
  // Ears, head
  c.fillCircle(p.X(-20), p.Y(3), p.S(5), skin);
  c.fillCircle(p.X(20), p.Y(3), p.S(5), skin);
  c.fillCircle(p.X(0), p.Y(0), p.S(20), skin);

  // Hair on top (hats cover it)
  const bool hairOnTop = hat == SHOP_HAT_NONE || hat == SHOP_HAT_CAT || hat == SHOP_HAT_VISOR;
  if (hairOnTop) {
    c.fillArc(p.X(0), p.Y(0), 0, p.S(21), 180, 360, hair);
    c.fillEllipse(p.X(-6), p.Y(-1), p.S(13), p.S(4), hair);
    if (look.style == 3)
      for (int k = 0; k < 7; k++) {
        const float a = (190 + k * 27) * 3.14159f / 180;
        c.fillCircle(p.X(cosf(a) * 20), p.Y(sinf(a) * 20), p.S(6.5f), hair);
      }
    if (look.style == 5)
      for (int k = -2; k <= 2; k++)
        c.fillTriangle(p.X(k * 8 - 5), p.Y(-16 + abs(k) * 2), p.X(k * 8 + 5), p.Y(-16 + abs(k) * 2),
                       p.X(k * 9), p.Y(-27 + abs(k) * 3), hair);
    if (look.style == 1) {
      c.fillRect(p.X(-21), p.Y(-4), p.S(5), p.S(22), hair);
      c.fillRect(p.X(16), p.Y(-4), p.S(5), p.S(22), hair);
    }
  }
  // Hats
  switch (hat) {
    case SHOP_HAT_CAP:
    case SHOP_HAT_VISOR: {
      const uint16_t capCol = SHOP_SHIRT[(look.shirt + 3) % SHOP_SHIRTS];
      if (hat == SHOP_HAT_CAP) c.fillArc(p.X(0), p.Y(-2), 0, p.S(21), 180, 360, capCol);
      else c.fillRect(p.X(-20), p.Y(-9), p.S(40), p.S(6), capCol);
      c.fillRoundRect(p.X(-4), p.Y(-6), p.S(32), p.S(5), p.S(2), capCol);
      if (hat == SHOP_HAT_CAP) c.fillCircle(p.X(0), p.Y(-22), p.S(2.5f), capCol);
      break;
    }
    case SHOP_HAT_BEANIE: {
      const uint16_t col = SHOP_SHIRT[(look.shirt + 5) % SHOP_SHIRTS];
      c.fillArc(p.X(0), p.Y(-2), 0, p.S(21), 180, 360, col);
      c.fillRoundRect(p.X(-22), p.Y(-7), p.S(44), p.S(7), p.S(3), SHOP_WHITE);
      c.fillCircle(p.X(0), p.Y(-24), p.S(5), SHOP_WHITE);
      break;
    }
    case SHOP_HAT_FROG:
      c.fillArc(p.X(0), p.Y(-1), p.S(18), p.S(26), 180, 360, hood);
      for (int s = -1; s <= 1; s += 2) {
        c.fillCircle(p.X(s * 11), p.Y(-24), p.S(7), hood);
        c.fillCircle(p.X(s * 11), p.Y(-24), p.S(4.5f), SHOP_WHITE);
        c.fillCircle(p.X(s * 11), p.Y(-24), p.S(2.2f), dark);
      }
      break;
    case SHOP_HAT_BEAR:
      c.fillArc(p.X(0), p.Y(-1), p.S(18), p.S(26), 180, 360, hood);
      for (int s = -1; s <= 1; s += 2) {
        c.fillCircle(p.X(s * 19), p.Y(-21), p.S(7), hood);
        c.fillCircle(p.X(s * 19), p.Y(-21), p.S(3.5f), shopRgb(230, 180, 140));
      }
      break;
    case SHOP_HAT_CAT:
      for (int s = -1; s <= 1; s += 2) {
        c.fillTriangle(p.X(s * 6), p.Y(-19), p.X(s * 20), p.Y(-9), p.X(s * 19), p.Y(-31), hair);
        c.fillTriangle(p.X(s * 10), p.Y(-18), p.X(s * 17), p.Y(-13), p.X(s * 17), p.Y(-25), shopRgb(255, 170, 190));
      }
      break;
    case SHOP_HAT_BAKER:
      c.fillRect(p.X(-17), p.Y(-26), p.S(34), p.S(16), SHOP_WHITE);
      c.fillCircle(p.X(-12), p.Y(-29), p.S(9), SHOP_WHITE);
      c.fillCircle(p.X(0), p.Y(-33), p.S(10), SHOP_WHITE);
      c.fillCircle(p.X(12), p.Y(-29), p.S(9), SHOP_WHITE);
      c.fillRect(p.X(-18), p.Y(-13), p.S(36), p.S(7), shopRgb(235, 238, 242));
      break;
    default: break;
  }
  // Face
  const uint16_t cheek = shopRgb(255, 150, 140);
  c.fillCircle(p.X(-12), p.Y(9), p.S(3), cheek);
  c.fillCircle(p.X(12), p.Y(9), p.S(3), cheek);
  for (int s = -1; s <= 1; s += 2) {
    c.fillCircle(p.X(s * 7), p.Y(3), p.S(3), dark);
    c.fillCircle(p.X(s * 7 + 1), p.Y(2), p.S(1), SHOP_WHITE);
  }
  if (look.glasses) {
    const uint16_t frame = look.shirt % 2 ? shopRgb(200, 40, 60) : shopRgb(50, 50, 60);
    for (int s = -1; s <= 1; s += 2) c.fillArc(p.X(s * 7), p.Y(3), p.S(5.5f), p.S(6.8f), 0, 360, frame);
    c.fillRect(p.X(-2), p.Y(2), p.S(4), p.S(1.2f), frame);
  }
  c.fillCircle(p.X(0), p.Y(8), p.S(2), SHOP_SKIN_SHADE[look.skin % SHOP_SKINS]);
  switch (mood) {
    case SHOP_MOOD_HAPPY:
      c.fillArc(p.X(0), p.Y(10), 0, p.S(8), 0, 180, dark);
      c.fillRect(p.X(-5), p.Y(10), p.S(10), p.S(2), SHOP_WHITE);
      c.fillEllipse(p.X(0), p.Y(16), p.S(3.5f), p.S(1.5f), shopRgb(240, 100, 110));
      break;
    case SHOP_MOOD_TALK:
      c.fillEllipse(p.X(0), p.Y(13), p.S(3.5f), p.S(4), dark);
      c.fillEllipse(p.X(0), p.Y(15), p.S(2.2f), p.S(1.5f), shopRgb(240, 100, 110));
      break;
    case SHOP_MOOD_NEUTRAL:
      shopThick(c, p.X(-4), p.Y(13), p.X(4), p.Y(13), f * 2, dark);
      shopThick(c, p.X(-10), p.Y(-5), p.X(-4), p.Y(-6), f * 1.6f, dark);  // one eyebrow up
      break;
    case SHOP_MOOD_SAD:
      c.fillArc(p.X(0), p.Y(17), p.S(4), p.S(6), 205, 335, dark);
      break;
  }
}

// ---------- Places ----------

struct ShopColors {
  uint16_t wall, wallLight, top, edge, front, inset;
};

static inline ShopColors shopColors(ShopPlace place) {
  switch (place) {
    case SHOP_BAKERY:
      return {shopRgb(243, 214, 170), shopRgb(250, 232, 200), shopRgb(232, 176, 122), shopRgb(245, 205, 160),
              shopRgb(176, 100, 40), shopRgb(150, 82, 30)};
    case SHOP_MARKET:
      return {shopRgb(170, 222, 190), shopRgb(205, 238, 215), shopRgb(214, 222, 228), shopRgb(240, 244, 247),
              shopRgb(70, 125, 170), shopRgb(55, 105, 145)};
    default:
      return {shopRgb(70, 160, 225), shopRgb(140, 205, 245), shopRgb(255, 214, 60), shopRgb(255, 236, 140),
              shopRgb(250, 170, 30), shopRgb(40, 110, 220)};
  }
}

// The wall behind the counter. busy: shelves and windows (not behind the menu board and the shopping list).
template <class C>
static void shopDrawWall(C& c, ShopPlace place, bool busy) {
  const ShopColors col = shopColors(place);
  c.fillRect(0, 0, SHOP_W, SHOP_COUNTER_Y, col.wall);
  if (place == SHOP_BAR) {
    for (int k = 0; k < 4; k++) {
      const int x = 8 + k * 60;
      c.fillRoundRect(x, 10, 44, 100, 22, col.wallLight);
      if (busy) {
        c.fillRect(x + 21, 10, 2, 90, col.wall);
        c.fillRect(x, 50, 44, 2, col.wall);
      }
    }
    c.fillRect(57, 0, 2, 14, shopRgb(30, 60, 110));
    c.fillArc(58, 22, 0, 10, 180, 360, SHOP_CITRUS);
    c.fillRect(48, 22, 21, 2, shopRgb(250, 190, 0));
  } else if (place == SHOP_BAKERY) {
    for (int y = 70; y < SHOP_COUNTER_Y; y += 8)
      for (int x = ((y / 8) % 2) * 8; x < SHOP_W; x += 16) c.fillRect(x, y, 8, 8, col.wallLight);
    if (busy) {
      const uint16_t shelf = shopRgb(150, 90, 40);
      for (int s = 0; s < 2; s++) {
        const int x0 = s == 0 ? 4 : 160;
        for (int r = 0; r < 2; r++) {
          const int y = 30 + r * 30;
          c.fillRect(x0, y, 74, 4, shelf);
          for (int k = 0; k < 3; k++)
            shopDrawItem(c, r == 0 ? ShopItem::Bread : (k % 2 ? ShopItem::Croissant : ShopItem::Pretzel),
                         x0 + 12 + k * 25, y - 8, 18);
        }
      }
    }
  } else {
    if (busy) {
      const uint16_t shelf = shopRgb(120, 130, 140);
      const uint16_t box[] = {shopRgb(255, 107, 107), shopRgb(77, 150, 255), shopRgb(255, 201, 60),
                              shopRgb(126, 217, 87), shopRgb(155, 93, 229), shopRgb(255, 159, 28)};
      for (int s = 0; s < 2; s++) {
        const int x0 = s == 0 ? 4 : 156;
        for (int r = 0; r < 3; r++) {
          const int y = 26 + r * 26;
          c.fillRect(x0, y, 80, 3, shelf);
          for (int k = 0; k < 6; k++) {
            const int h = 10 + ((k * 7 + r * 3 + s) % 4) * 3;
            c.fillRoundRect(x0 + 3 + k * 13, y - h, 10, h, 2, box[(k + r * 2 + s) % 6]);
          }
        }
      }
    }
  }
}

template <class C>
static void shopDrawCounter(C& c, ShopPlace place) {
  const ShopColors col = shopColors(place);
  c.fillRect(0, SHOP_COUNTER_Y, SHOP_W, 8, col.top);
  c.fillRect(0, SHOP_COUNTER_Y, SHOP_W, 2, col.edge);
  c.fillRect(0, SHOP_COUNTER_Y + 8, SHOP_W, SHOP_H - SHOP_COUNTER_Y - 8, col.front);
  c.fillRoundRect(14, SHOP_COUNTER_Y + 13, SHOP_W - 28, 40, 8, col.inset);
  if (place == SHOP_MARKET) {  // the checkout: a belt and the till
    c.fillRect(150, SHOP_COUNTER_Y + 1, 90, 6, shopRgb(50, 55, 60));
    for (int x = 154; x < SHOP_W; x += 10) c.fillRect(x, SHOP_COUNTER_Y + 2, 2, 4, shopRgb(80, 85, 90));
  }
}

template <class C>
static void shopDrawTill(C& c) {
  const int x = 8, y = 70;
  c.fillRoundRect(x, y + 6, 40, 22, 4, shopRgb(255, 107, 107));
  c.fillTriangle(x + 4, y + 6, x + 36, y + 6, x + 32, y - 2, shopRgb(230, 80, 85));
  c.fillRect(x + 12, y - 4, 16, 6, shopRgb(60, 70, 80));
  c.fillRect(x + 14, y - 3, 12, 4, shopRgb(120, 230, 160));
  for (int r = 0; r < 2; r++)
    for (int k = 0; k < 4; k++) c.fillRect(x + 6 + k * 8, y + 10 + r * 6, 5, 4, SHOP_WHITE);
}

// The goods on the counter: each piece drawn, several of a kind overlapping; "3x" when they wouldn't fit
template <class C>
static void shopDrawCounterGoods(C& c, const std::vector<ShopLine>& goods, int centerX) {
  const int size = 22, step = 11, gap = 8;
  int width = 0;
  for (size_t k = 0; k < goods.size(); k++) width += size + (goods[k].qty > 1 ? goods[k].qty - 1 : 0) * step + (k ? gap : 0);
  const bool compact = width > 200;
  if (compact) {
    width = 0;
    for (size_t k = 0; k < goods.size(); k++) width += size + 22 + (k ? gap : 0);
  }
  int x = centerX - width / 2;
  if (x < 6) x = 6;
  const int cy = SHOP_COUNTER_Y + 3 - size / 2;
  for (size_t k = 0; k < goods.size(); k++) {
    const int n = goods[k].qty < 1 ? 1 : goods[k].qty;
    if (compact) {
      char label[8];
      snprintf(label, sizeof label, "%dx", n);
      shopText(c, label, x, cy + 2, ML_DATUM, SHOP_INK, false);
      shopDrawItem(c, goods[k].item, x + 22 + size / 2, cy, size);
      x += size + 22 + gap;
    } else {
      for (int i = 0; i < n; i++) shopDrawItem(c, goods[k].item, x + size / 2 + i * step, cy, size);
      x += size + (n - 1) * step + gap;
    }
  }
}

// Rows of "2x [picture]" from top
template <class C>
static void shopDrawLines(C& c, const std::vector<ShopLine>& lines, const std::vector<uint8_t>* marks, int x, int y,
                          int rowH, int icon) {
  for (size_t k = 0; k < lines.size(); k++) {
    const int cy = y + (int)k * rowH + rowH / 2;
    char label[8];
    snprintf(label, sizeof label, "%dx", lines[k].qty < 1 ? 1 : lines[k].qty);
    shopText(c, label, x, cy, ML_DATUM, SHOP_INK, true);
    shopDrawItem(c, lines[k].item, x + 40, cy, icon);
    const uint8_t m = marks && k < marks->size() ? (*marks)[k] : SHOP_MARK_NONE;
    if (m == SHOP_MARK_OK) shopCheck(c, x + 58, cy, 5, SHOP_OK);
    if (m == SHOP_MARK_BAD) shopCross(c, x + 58, cy, 4, SHOP_BAD);
  }
}

template <class C>
static void shopDrawBubble(C& c, const ShopScene& s) {
  const int rows = s.bubble == SHOP_BUBBLE_PICTURES ? (int)s.order.size() : 0;
  const int x = 140, y = 8, w = 84;
  const int h = rows ? rows * 26 + 8 : 40;
  const int mouthX = s.personX + 12, mouthY = 64;
  c.fillTriangle(x + 2, y + h - 22, x + 2, y + h - 8, mouthX + 6, mouthY, SHOP_WHITE);
  c.fillRoundRect(x, y, w, h, 10, SHOP_WHITE);
  if (s.bubble == SHOP_BUBBLE_DOTS) {
    for (int k = -1; k <= 1; k++) c.fillCircle(x + w / 2 + k * 14, y + h / 2, 4, shopRgb(150, 155, 165));
  } else if (s.bubble == SHOP_BUBBLE_HEART) {
    shopHeart(c, x + w / 2, y + h / 2, 11, SHOP_BAD);
  } else {
    shopDrawLines(c, s.order, nullptr, x + 10, y + 4, 26, 24);
  }
}

// The paper with what the child should buy
template <class C>
static void shopDrawList(C& c, const ShopScene& s) {
  const int x = 160, y = 4, w = 66;
  const int h = (int)s.order.size() * 24 + 14;
  c.fillRect(x + 3, y + 3, w, h, shopRgb(60, 60, 60));
  c.fillRect(x, y, w, h, shopRgb(255, 253, 240));
  c.fillRect(x, y + 7, w, 2, shopRgb(240, 110, 110));
  for (int k = 0; k < (int)s.order.size(); k++) c.fillRect(x, y + 34 + k * 24, w, 1, shopRgb(190, 215, 240));
  shopDrawLines(c, s.order, &s.marks, x + 6, y + 11, 24, 21);
}

// The board with everything the place sells
template <class C>
static void shopDrawBoard(C& c, ShopPlace place) {
  const int x = 12, y = 8, w = 82, h = 64;
  c.fillRoundRect(x, y, w, h, 4, shopRgb(140, 90, 45));
  c.fillRect(x + 3, y + 3, w - 6, h - 6, shopRgb(46, 94, 78));
  for (int k = 0; k < SHOP_GOODS_PER_PLACE; k++)
    shopDrawItem(c, shopPlaceInfo(place).goods[k], x + 13 + (k % 4) * 19, y + 18 + (k / 4) * 28, 17);
}

template <class C>
static void shopDrawStars(C& c, const std::vector<uint8_t>& stars) {
  for (size_t k = 0; k < stars.size(); k++) {
    const int x = 24 + (int)k * 11, y = SHOP_H - 10;
    if (stars[k] == 1) shopHeart(c, x, y, 4, SHOP_CITRUS);
    else c.fillCircle(x, y, stars[k] == 2 ? 3 : 2, stars[k] == 2 ? SHOP_WHITE : shopRgb(170, 170, 180));
  }
}

template <class C>
static void shopDrawMic(C& c, int percent) {
  const int cx = 146, y = SHOP_COUNTER_Y + 17;
  c.fillRoundRect(cx - 62, y - 4, 124, 26, 13, shopRgb(30, 30, 35));
  c.fillRoundRect(cx - 54, y + 2, 7, 11, 3, SHOP_WHITE);
  c.fillArc(cx - 51, y + 8, 6, 7, 0, 180, SHOP_WHITE);
  c.fillRect(cx - 52, y + 14, 2, 4, SHOP_WHITE);
  c.fillCircle(cx + 50, y + 9, 4, SHOP_BAD);
  c.fillRoundRect(cx - 38, y + 5, 80, 8, 4, shopRgb(80, 80, 90));
  const int w = percent * 80 / 100;
  if (w > 0) c.fillRoundRect(cx - 38, y + 5, w < 8 ? 8 : w, 8, 4, SHOP_CITRUS);
}

// ---------- Screens ----------

template <class C>
static void shopDrawScene(C& c, const ShopScene& s) {
  const bool selling = s.role == SHOP_SELLER;
  shopDrawWall(c, s.place, selling);
  if (!selling) shopDrawBoard(c, s.place);
  const uint8_t hat = selling ? s.person.hat
                              : s.place == SHOP_BAKERY ? SHOP_HAT_BAKER
                              : s.place == SHOP_MARKET ? SHOP_HAT_CAP
                                                       : SHOP_HAT_VISOR;
  shopDrawPerson(c, s.person, s.personX, 50, hat, s.mood, !selling);
  if (!selling) shopDrawList(c, s);
  shopDrawCounter(c, s.place);
  if (s.place == SHOP_MARKET) shopDrawTill(c);
  shopDrawCounterGoods(c, s.counter, selling ? s.personX + 20 : 108);
  if (selling && s.bubble != SHOP_BUBBLE_NONE) shopDrawBubble(c, s);
  shopDrawStars(c, s.stars);
  if (s.recording >= 0) {
    shopDrawMic(c, s.recording);
  } else if (s.note && s.note[0]) {
    c.setFreeFont(shopFonts.small);
    const std::string note = shopFit(c, s.note, SHOP_W - 100);
    c.fillRoundRect(74, SHOP_COUNTER_Y + 9, SHOP_W - 82, 18, 8, shopRgb(30, 30, 35));
    shopText(c, note.c_str(), (74 + SHOP_W - 8) / 2, SHOP_COUNTER_Y + 18, MC_DATUM, SHOP_WHITE, false);
  }
}

// The start: the place with its seller or a guest, a card with the place, the role and four of its goods
template <class C>
static void shopDrawStart(C& c, ShopPlace place, ShopRole role, const ShopLook& person, const char* title,
                          const char* roleLine) {
  shopDrawWall(c, place, false);
  const uint8_t hat = role == SHOP_GUEST ? (place == SHOP_BAKERY ? SHOP_HAT_BAKER : place == SHOP_MARKET ? SHOP_HAT_CAP : SHOP_HAT_VISOR)
                                         : person.hat;
  shopDrawPerson(c, person, 56, 54, hat, SHOP_MOOD_HAPPY, role == SHOP_GUEST);
  shopDrawCounter(c, place);
  c.fillRoundRect(104, 10, 126, 78, 10, SHOP_WHITE);
  c.setFreeFont(shopFonts.big);
  const bool fits = c.textWidth(title) <= 108;
  shopText(c, title, 167, 26, MC_DATUM, SHOP_INK, fits);
  shopText(c, roleLine, 167, 48, MC_DATUM, shopRgb(110, 110, 120), false);
  for (int k = 0; k < 4; k++) shopDrawItem(c, shopPlaceInfo(place).goods[k], 122 + k * 30, 72, 20);
}

// The end of a round: how many guests left happy
template <class C>
static void shopDrawEnd(C& c, ShopPlace place, const std::vector<uint8_t>& stars, const char* line1, const char* line2) {
  shopDrawWall(c, place, false);
  shopDrawCounter(c, place);
  c.fillRoundRect(16, 10, 208, 76, 10, SHOP_WHITE);
  shopText(c, line1, SHOP_W / 2, 30, MC_DATUM, SHOP_INK, true);
  shopText(c, line2, SHOP_W / 2, 54, MC_DATUM, shopRgb(110, 110, 120), false);
  const int n = (int)stars.size();
  for (int k = 0; k < n; k++) {
    const int x = SHOP_W / 2 + (k * 2 - (n - 1)) * 12, y = 74;
    if (stars[k] == 1) shopHeart(c, x, y, 7, SHOP_BAD);
    else c.fillCircle(x, y, 5, shopRgb(210, 210, 215));
  }
}
