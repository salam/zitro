// Draws the shop game's screens with the stick's own drawing code (shop_draw.h) on a host canvas and writes them as
// PPM files: tests/shop/render.sh turns them into PNGs. The button tabs are a simple copy of the stick's, for the
// layout only (stick held with the buttons on the left).
#include <string>
#include <vector>

#include "host_canvas.h"
#include "../../sketch/sketch_jan12a/shop_draw.h"

static std::string outDir = ".";

enum Tab { T_NONE, T_MIC, T_MENU, T_HELP, T_PLAY, T_NEXT, T_SWAP, T_OK, T_CANCEL };

static void tabSymbol(HostCanvas& c, Tab t, int cx, int cy, uint16_t ink) {
  switch (t) {
    case T_MIC:
      c.fillRoundRect(cx - 3, cy - 6, 6, 9, 3, ink);
      c.fillArc(cx, cy, 5, 6, 0, 180, ink);
      c.fillRect(cx, cy + 5, 1, 3, ink);
      break;
    case T_MENU:
      for (int k = -1; k <= 1; k++) c.fillCircle(cx + k * 6, cy, 1, ink);
      break;
    case T_HELP:
      c.setFreeFont(shopFonts.small);
      c.setTextDatum(MC_DATUM);
      c.setTextColor(ink);
      c.drawString("?", cx, cy);
      break;
    case T_PLAY: c.fillTriangle(cx - 3, cy - 5, cx - 3, cy + 5, cx + 5, cy, ink); break;
    case T_NEXT: c.fillTriangle(cx - 5, cy - 3, cx + 5, cy - 3, cx, cy + 4, ink); break;
    case T_SWAP:
      c.fillArc(cx, cy, 0, 5, 90, 270, ink);
      c.fillArc(cx, cy, 4, 5, 0, 360, ink);
      break;
    case T_OK: shopCheck(c, cx, cy, 4, ink); break;
    case T_CANCEL: shopCross(c, cx, cy, 3, ink); break;
    default: break;
  }
}

// Buttons on the left: A at the left edge, C top left, B bottom centre
static void tabs(HostCanvas& c, Tab a, Tab b, Tab cc) {
  const uint16_t grey = shopRgb(60, 60, 64), edge = shopRgb(130, 130, 136);
  if (a != T_NONE) {
    c.fillRoundRect(-8, 135 / 2 - 17, 23, 34, 6, SHOP_WHITE);
    tabSymbol(c, a, 7, 135 / 2, shopRgb(0, 0, 0));
  }
  if (b != T_NONE) {
    c.fillRoundRect(120 - 17, 135 - 15, 34, 23, 6, grey);
    c.drawRoundRect(120 - 17, 135 - 15, 34, 23, 6, edge);
    tabSymbol(c, b, 120, 135 - 8, SHOP_WHITE);
  }
  if (cc != T_NONE) {
    c.fillRoundRect(24 - 17, -8, 34, 23, 6, grey);
    c.drawRoundRect(24 - 17, -8, 34, 23, 6, edge);
    tabSymbol(c, cc, 24, 7, SHOP_WHITE);
  }
}

static void save(HostCanvas& c, const char* name) {
  c.writePpm(outDir + "/" + name + ".ppm", 1);
  std::printf("%s\n", name);
}

static ShopLook look(uint32_t seed) {
  uint32_t rng = seed;
  return shopMakeLook(rng);
}

int main(int argc, char** argv) {
  if (argc > 1) outDir = argv[1];
  shopFonts = {&Roboto_Regular10pt8b, &Roboto_Regular14pt8b};
  using I = ShopItem;

  {  // A guest comes to the lemonade bar and orders
    HostCanvas c(240, 135);
    ShopScene s;
    s.place = SHOP_BAR;
    s.person = look(3);
    s.mood = SHOP_MOOD_TALK;
    s.bubble = SHOP_BUBBLE_DOTS;
    s.order = {{I::Lemonade, 2}, {I::Cookie, 1}};
    s.stars = {1, 0, 0, 0, 0};
    shopDrawScene(c, s);
    tabs(c, T_MIC, T_MENU, T_HELP);
    save(c, "01-seller-order");

    s.bubble = SHOP_BUBBLE_PICTURES;  // after the second "?"
    shopDrawScene(c, s);
    tabs(c, T_MIC, T_MENU, T_HELP);
    save(c, "02-seller-hint");

    s.recording = 45;
    s.bubble = SHOP_BUBBLE_DOTS;
    shopDrawScene(c, s);
    save(c, "03-seller-recording");

    s.recording = -1;
    s.counter = s.order;
    s.mood = SHOP_MOOD_HAPPY;
    s.bubble = SHOP_BUBBLE_HEART;
    s.stars = {1, 1, 0, 0, 0};
    shopDrawScene(c, s);
    tabs(c, T_PLAY, T_MENU, T_NONE);
    save(c, "04-seller-served");
  }
  {  // At the bakery the child gives three croissants instead of two and a pretzel
    HostCanvas c(240, 135);
    ShopScene s;
    s.place = SHOP_BAKERY;
    s.person = look(11);
    s.person.hat = SHOP_HAT_FROG;
    s.mood = SHOP_MOOD_NEUTRAL;
    s.bubble = SHOP_BUBBLE_PICTURES;
    s.order = {{I::Croissant, 2}, {I::Pretzel, 1}};
    s.counter = {{I::Croissant, 3}};
    s.stars = {1, 2, 1, 0, 0};
    shopDrawScene(c, s);
    tabs(c, T_MIC, T_MENU, T_HELP);
    save(c, "05-seller-wrong");
  }
  {  // The child shops: the seller at the checkout, the board, the shopping list
    HostCanvas c(240, 135);
    ShopScene s;
    s.place = SHOP_MARKET;
    s.role = SHOP_GUEST;
    s.person = look(21);
    s.personX = 127;
    s.mood = SHOP_MOOD_TALK;
    s.order = {{I::Apple, 3}, {I::Milk, 1}, {I::Carrot, 2}};
    s.marks = {0, 0, 0};
    s.stars = {0, 0, 0, 0, 0};
    shopDrawScene(c, s);
    tabs(c, T_MIC, T_MENU, T_HELP);
    save(c, "06-guest-list");

    s.marks = {SHOP_MARK_OK, SHOP_MARK_BAD, SHOP_MARK_OK};
    s.counter = {{I::Apple, 3}, {I::Cheese, 1}, {I::Carrot, 2}};
    s.mood = SHOP_MOOD_TALK;
    shopDrawScene(c, s);
    tabs(c, T_MIC, T_MENU, T_HELP);
    save(c, "07-guest-asks-back");

    s.marks = {SHOP_MARK_NONE, SHOP_MARK_NONE, SHOP_MARK_NONE};
    s.counter.clear();
    s.mood = SHOP_MOOD_NEUTRAL;
    s.note = "Gehört: «drei Äpfel bitte, und noch zwei Rüebli»";
    shopDrawScene(c, s);
    tabs(c, T_MIC, T_MENU, T_HELP);
    save(c, "08-guest-not-understood");
  }
  {  // Served at the lemonade bar
    HostCanvas c(240, 135);
    ShopScene s;
    s.place = SHOP_BAR;
    s.role = SHOP_GUEST;
    s.person = look(5);
    s.personX = 127;
    s.mood = SHOP_MOOD_HAPPY;
    s.order = {{I::IceCream, 2}, {I::HotChocolate, 1}};
    s.marks = {SHOP_MARK_OK, SHOP_MARK_OK};
    s.counter = s.order;
    s.stars = {1, 1, 1, 0, 0};
    shopDrawScene(c, s);
    tabs(c, T_PLAY, T_MENU, T_NONE);
    save(c, "09-guest-served");
  }
  {  // Start and end
    HostCanvas c(240, 135);
    shopDrawStart(c, SHOP_BAKERY, SHOP_SELLER, look(8), "Bäckerei", "Du verkaufst");
    tabs(c, T_PLAY, T_SWAP, T_NEXT);
    save(c, "10-start-seller");
    shopDrawStart(c, SHOP_BAR, SHOP_GUEST, look(9), "Limonadenbar", "Du kaufst ein");
    tabs(c, T_PLAY, T_SWAP, T_NEXT);
    save(c, "11-start-guest");
    shopDrawEnd(c, SHOP_MARKET, {1, 1, 2, 1, 1}, "4 von 5", "Gäste zufrieden");
    tabs(c, T_PLAY, T_MENU, T_NONE);
    save(c, "12-end");
  }
  {  // Twelve guests: everyone looks different
    HostCanvas c(240, 135);
    c.fillScreen(shopRgb(230, 236, 242));
    for (int k = 0; k < 12; k++)
      shopDrawPerson(c, look(100 + k * 7), 20 + (k % 6) * 40, 28 + (k / 6) * 66, look(100 + k * 7).hat,
                     k % 3 ? SHOP_MOOD_HAPPY : SHOP_MOOD_TALK, false, 0.8f);
    save(c, "13-guests");
  }
  {  // All goods
    HostCanvas c(240, 135);
    c.fillScreen(SHOP_WHITE);
    for (int k = 0; k < (int)I::COUNT; k++) shopDrawItem(c, (I)k, 15 + (k % 8) * 30, 22 + (k / 8) * 44, 26);
    save(c, "14-goods");
  }
  return 0;
}
