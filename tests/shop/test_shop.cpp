// Host tests for the shop game's pure logic (sketch/sketch_jan12a/shop_core.h).
// Run with tests/shop/run.sh; no hardware needed.
#include <cstdio>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "../../sketch/sketch_jan12a/shop_core.h"

static int failures = 0, checks = 0;

#define CHECK(cond)                                               \
  do {                                                            \
    checks++;                                                     \
    if (!(cond)) {                                                \
      failures++;                                                 \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
    }                                                             \
  } while (0)

#define CHECK_EQ(a, b)                                                                                    \
  do {                                                                                                    \
    checks++;                                                                                             \
    auto va = (a);                                                                                        \
    auto vb = (b);                                                                                        \
    if (!(va == vb)) {                                                                                    \
      failures++;                                                                                         \
      std::ostringstream sa, sb;                                                                          \
      sa << va;                                                                                           \
      sb << vb;                                                                                           \
      std::printf("FAIL %s:%d  %s == %s  (got '%s', want '%s')\n", __FILE__, __LINE__, #a, #b,           \
                  sa.str().c_str(), sb.str().c_str());                                                     \
    }                                                                                                     \
  } while (0)

using I = ShopItem;

// "2 lemonade, 1 cookie": the heard lines as text, in the order they were said; "?" for a plural without a number
static std::string heardText(const char* said) {
  ShopHeard h = shopHear(said);
  std::string out;
  for (const ShopLine& l : h.lines) {
    if (!out.empty()) out += ", ";
    out += l.qty < 0 ? std::string("?") : std::to_string(l.qty);
    out += " ";
    out += shopItemInfo(l.item).word;
  }
  return out;
}

static std::vector<ShopLine> order(std::initializer_list<ShopLine> lines) { return std::vector<ShopLine>(lines); }

static void testCatalog() {
  // Every place has eight goods, none appears twice, every item has all its texts
  std::set<int> seen;
  for (int p = 0; p < SHOP_PLACES; p++) {
    const ShopPlaceInfo& info = shopPlaceInfo((ShopPlace)p);
    CHECK(info.german[0] && info.english[0]);
    for (int k = 0; k < SHOP_GOODS_PER_PLACE; k++) {
      const I item = info.goods[k];
      CHECK(seen.insert((int)item).second);
      const ShopItemInfo& it = shopItemInfo(item);
      CHECK(it.single[0] && it.many[0] && it.word[0] && it.kind[0] && it.names[0]);
      CHECK(shopPlaceOf(item) == (ShopPlace)p);
      // The order phrases are heard back as what they say
      for (int n = 1; n <= 3; n++) {
        ShopHeard h = shopHear(shopPhrase(item, n).c_str());
        CHECK(h.lines.size() == 1 && h.lines[0].item == item && h.lines[0].qty == n);
        if (h.lines.size() != 1 || h.lines[0].item != item || h.lines[0].qty != n)
          std::printf("    phrase '%s'\n", shopPhrase(item, n).c_str());
      }
    }
  }
  CHECK_EQ((int)seen.size(), (int)I::COUNT);
}

static void testPhrases() {
  CHECK_EQ(shopPhrase(I::Lemonade, 1), std::string("a lemonade"));
  CHECK_EQ(shopPhrase(I::IceCream, 1), std::string("an ice cream"));
  CHECK_EQ(shopPhrase(I::Egg, 1), std::string("an egg"));
  CHECK_EQ(shopPhrase(I::Water, 2), std::string("two glasses of water"));
  CHECK_EQ(shopPhrase(I::Tea, 3), std::string("three cups of tea"));
  CHECK_EQ(shopPhrase(I::Bread, 1), std::string("a loaf of bread"));
  CHECK_EQ(shopPhrase(I::Tomato, 2), std::string("two tomatoes"));
  CHECK_EQ(shopPhrase(I::Sandwich, 2), std::string("two sandwiches"));
  CHECK_EQ(shopListPhrase(order({{I::Lemonade, 2}})), std::string("two lemonades"));
  CHECK_EQ(shopListPhrase(order({{I::Lemonade, 2}, {I::Cookie, 1}})), std::string("two lemonades and a cookie"));
  CHECK_EQ(shopListPhrase(order({{I::Water, 1}, {I::Cookie, 2}, {I::IceCream, 1}})),
           std::string("a glass of water, two cookies and an ice cream"));
}

static void testHearing() {
  CHECK_EQ(heardText("Two lemonades and a cookie, please."), std::string("2 lemonade, 1 cookie"));
  CHECK_EQ(heardText("2 lemonades"), std::string("2 lemonade"));
  CHECK_EQ(heardText("I'd like a glass of water and two cups of tea."), std::string("1 water, 2 tea"));
  CHECK_EQ(heardText("Can I have an ice cream?"), std::string("1 ice cream"));
  CHECK_EQ(heardText("ice-cream"), std::string("1 ice cream"));
  // The longest name wins: juice, not the fruit; a roll, not bread; a cake, not a drink
  CHECK_EQ(heardText("three apple juices"), std::string("3 apple juice"));
  CHECK_EQ(heardText("an orange juice and an orange"), std::string("1 orange juice, 1 orange"));
  CHECK_EQ(heardText("two bread rolls"), std::string("2 bread roll"));
  CHECK_EQ(heardText("a piece of chocolate cake"), std::string("1 cake"));
  CHECK_EQ(heardText("a hot chocolate"), std::string("1 hot chocolate"));
  CHECK_EQ(heardText("a loaf of bread"), std::string("1 bread"));
  // British and American words, other spellings
  CHECK_EQ(heardText("two biscuits"), std::string("2 cookie"));
  CHECK_EQ(heardText("three donuts"), std::string("3 doughnut"));
  CHECK_EQ(heardText("two tomatos"), std::string("2 tomato"));
  // A number with a singular still counts: children say "two lemonade"
  CHECK_EQ(heardText("two lemonade please"), std::string("2 lemonade"));
  // A plural without a number: how many?
  CHECK_EQ(heardText("lemonades please"), std::string("? lemonade"));
  CHECK_EQ(heardText("some apples"), std::string("? apple"));
  // Words between the number and the thing don't matter, the latest number does
  CHECK_EQ(heardText("two big cold lemonades"), std::string("2 lemonade"));
  CHECK_EQ(heardText("two, no, three lemonades"), std::string("3 lemonade"));
  CHECK_EQ(heardText("TEN EGGS"), std::string("10 egg"));
  CHECK_EQ(heardText("twelve eggs"), std::string("12 egg"));
  CHECK_EQ(heardText("twenty eggs"), std::string("20 egg"));
  // Said twice: the later one counts, "another" adds one
  CHECK_EQ(heardText("a lemonade and another lemonade"), std::string("2 lemonade"));
  CHECK_EQ(heardText("two cookies, sorry, three cookies"), std::string("3 cookie"));
  // A number belongs to the next thing only
  CHECK_EQ(heardText("two lemonades and cookies"), std::string("2 lemonade, ? cookie"));
  // "to", "too" and "for" are not numbers
  CHECK_EQ(heardText("I want to have a cookie too"), std::string("1 cookie"));
  // Nothing heard
  CHECK_EQ(heardText("Hallo, zwei Limonaden bitte"), std::string(""));
  CHECK_EQ(heardText(""), std::string(""));
  CHECK(shopHear("Two lemonades, please").please);
  CHECK(!shopHear("Two lemonades").please);
  // Transcripts from the speech test (2026-11-14, gpt-transcribe, language=en, computer voices): German stays German
  // and is not heard as goods, a German voice reading English can lose a word
  CHECK_EQ(heardText("Zwei Limonaden und ein Keks, bitte."), std::string(""));
  CHECK_EQ(heardText("Ich hätte gern ein Glas und einen Tee."), std::string(""));
  CHECK_EQ(heardText("Drei Orangensaft."), std::string(""));
  CHECK_EQ(heardText("Zwei Brezeln und ein Gipfeli, bitte."), std::string(""));
  CHECK_EQ(heardText("Drei Äpfel und eine Flasche Milch, bitte."), std::string(""));
  CHECK_EQ(heardText("Here you are, three orange juices."), std::string("3 orange juice"));
  CHECK_EQ(heardText("Two lemon, addes and a cookie, please."), std::string("1 cookie"));
  CHECK_EQ(heardText("We would like two bananas and four eggs."), std::string("2 banana, 4 egg"));
  CHECK_EQ(heardText("A hot chocolate and two glasses of water, please."), std::string("1 hot chocolate, 2 water"));
  // A bare number answers "How many?"
  CHECK_EQ(shopBareNumber("Two."), 2);
  CHECK_EQ(shopBareNumber("three please"), 3);
  CHECK_EQ(shopBareNumber("two lemonades"), 0);
  CHECK_EQ(shopBareNumber("hello"), 0);
}

static void testCompare() {
  const auto want = order({{I::Lemonade, 2}, {I::Cookie, 1}});
  std::vector<ShopDiff> d;
  CHECK_EQ((int)shopCompare(want, shopHear("Two lemonades and a cookie"), SHOP_BAR, d), (int)SHOP_MATCH_EXACT);
  CHECK(d.empty());
  // Order doesn't matter
  CHECK_EQ((int)shopCompare(want, shopHear("a cookie and two lemonades"), SHOP_BAR, d), (int)SHOP_MATCH_EXACT);
  CHECK_EQ((int)shopCompare(want, shopHear("Hallo"), SHOP_BAR, d), (int)SHOP_MATCH_NOTHING);
  CHECK_EQ((int)shopCompare(want, shopHear("three lemonades and a cookie"), SHOP_BAR, d), (int)SHOP_MATCH_WRONG);
  CHECK(d.size() == 1 && d[0].item == I::Lemonade && d[0].want == 2 && d[0].got == 3);
  CHECK_EQ((int)shopCompare(want, shopHear("two lemonades"), SHOP_BAR, d), (int)SHOP_MATCH_WRONG);
  CHECK(d.size() == 1 && d[0].item == I::Cookie && d[0].want == 1 && d[0].got == 0);
  CHECK_EQ((int)shopCompare(want, shopHear("two lemonades, a cookie and a muffin"), SHOP_BAR, d), (int)SHOP_MATCH_WRONG);
  CHECK(d.size() == 1 && d[0].item == I::Muffin && d[0].want == 0 && d[0].got == 1);
  CHECK_EQ((int)shopCompare(want, shopHear("lemonades and a cookie"), SHOP_BAR, d), (int)SHOP_MATCH_WRONG);
  CHECK(d.size() == 1 && d[0].got == -1);
  // Only things this place doesn't sell
  CHECK_EQ((int)shopCompare(want, shopHear("two bananas"), SHOP_BAR, d), (int)SHOP_MATCH_NOT_HERE);
  CHECK(d.size() == 1 && d[0].item == I::Banana);
  // Wanted things first, in the order's order, then the extra ones
  CHECK_EQ((int)shopCompare(want, shopHear("a muffin and a lemonade"), SHOP_BAR, d), (int)SHOP_MATCH_WRONG);
  CHECK(d.size() == 3 && d[0].item == I::Lemonade && d[1].item == I::Cookie && d[2].item == I::Muffin);
}

static void testReplies() {
  const auto want = order({{I::Lemonade, 2}, {I::Cookie, 1}});
  std::vector<ShopDiff> d;
  // The guest's complaint names the first difference
  shopCompare(want, shopHear("three lemonades and a cookie"), SHOP_BAR, d);
  CHECK_EQ(shopComplaint(d), std::string("Just two lemonades, please."));
  shopCompare(order({{I::Water, 1}}), shopHear("two glasses of water"), SHOP_BAR, d);
  CHECK_EQ(shopComplaint(d), std::string("Just one glass of water, please."));
  shopCompare(want, shopHear("a lemonade and a cookie"), SHOP_BAR, d);
  CHECK_EQ(shopComplaint(d), std::string("I'd like two lemonades, please."));
  shopCompare(want, shopHear("two lemonades"), SHOP_BAR, d);
  CHECK_EQ(shopComplaint(d), std::string("And a cookie, please!"));
  shopCompare(want, shopHear("lemonades and a cookie"), SHOP_BAR, d);
  CHECK_EQ(shopComplaint(d), std::string("Two lemonades, please."));
  shopCompare(want, shopHear("two lemonades, a cookie and two muffins"), SHOP_BAR, d);
  CHECK_EQ(shopComplaint(d), std::string("I didn't order a muffin."));

  // The seller's answer when the child orders
  ShopHeard h = shopHear("Two lemonades and a cookie, please");
  CHECK_EQ(shopSellerReply(SHOP_MATCH_EXACT, h, SHOP_BAR), std::string("Of course! Here you are: two lemonades and a cookie."));
  h = shopHear("two lemonades and a cookie");
  CHECK_EQ(shopSellerReply(SHOP_MATCH_EXACT, h, SHOP_BAR), std::string("Here you are: two lemonades and a cookie."));
  h = shopHear("hallo");
  CHECK_EQ(shopSellerReply(SHOP_MATCH_NOTHING, h, SHOP_BAR), std::string("Sorry? What would you like?"));
  h = shopHear("two bananas");
  shopCompare(want, h, SHOP_BAR, d);
  CHECK_EQ(shopSellerReply(SHOP_MATCH_NOT_HERE, h, SHOP_BAR), std::string("Sorry, we don't have bananas."));
  h = shopHear("a lemonade and an ice cream");
  shopCompare(want, h, SHOP_BAR, d);
  CHECK_EQ(shopSellerReply(SHOP_MATCH_WRONG, h, SHOP_BAR), std::string("A lemonade and an ice cream?"));
  // A muffin is the bakery's
  h = shopHear("a lemonade and a muffin");
  shopCompare(want, h, SHOP_BAR, d);
  CHECK_EQ(shopSellerReply(SHOP_MATCH_WRONG, h, SHOP_BAR), std::string("Sorry, we don't have muffins. A lemonade?"));
  h = shopHear("lemonades and a cookie");
  shopCompare(want, h, SHOP_BAR, d);
  CHECK_EQ(shopSellerReply(SHOP_MATCH_WRONG, h, SHOP_BAR), std::string("How many lemonades?"));
  CHECK_EQ(shopAskedItem(h, SHOP_BAR), (int)I::Lemonade);
  // The seller only asks about what the place sells: the number goes to the rolls, not the bananas
  h = shopHear("bananas and rolls, please");
  shopCompare(order({{I::Roll, 3}}), h, SHOP_BAKERY, d);
  CHECK_EQ(shopSellerReply(SHOP_MATCH_WRONG, h, SHOP_BAKERY), std::string("Sorry, we don't have bananas. How many bread rolls?"));
  CHECK_EQ(shopAskedItem(h, SHOP_BAKERY), (int)I::Roll);
  CHECK_EQ(shopAskedItem(shopHear("bananas"), SHOP_BAKERY), -1);
  // Part of the order, all of it right: "Anything else?", and the next answer adds to it
  h = shopHear("a cookie, please");
  CHECK_EQ((int)shopCompare(want, h, SHOP_BAR, d), (int)SHOP_MATCH_WRONG);
  CHECK(shopOnlyMissing(d));
  CHECK_EQ(shopSellerReply(SHOP_MATCH_WRONG, h, SHOP_BAR, shopOnlyMissing(d)), std::string("A cookie. Anything else?"));
  ShopHeard both = shopMerge(h, shopHear("and two lemonades"));
  CHECK_EQ((int)shopCompare(want, both, SHOP_BAR, d), (int)SHOP_MATCH_EXACT);
  CHECK(both.please);
  // A later number replaces the earlier one
  both = shopMerge(shopHear("a lemonade"), shopHear("two lemonades"));
  CHECK(both.lines.size() == 1 && both.lines[0].qty == 2);
  // Something wrong in it: not only missing
  shopCompare(want, shopHear("three lemonades"), SHOP_BAR, d);
  CHECK(!shopOnlyMissing(d));
  shopCompare(want, shopHear("a muffin"), SHOP_BAR, d);
  CHECK(!shopOnlyMissing(d));
  // Something this place doesn't sell next to something it does: the seller says what's missing first
  h = shopHear("a lemonade and a banana");
  shopCompare(want, h, SHOP_BAR, d);
  CHECK_EQ(shopSellerReply(SHOP_MATCH_WRONG, h, SHOP_BAR), std::string("Sorry, we don't have bananas. A lemonade?"));
  CHECK_EQ(shopModelAnswer(want), std::string("Ah, two lemonades and a cookie. Here you are!"));
}

static void testOrders() {
  // Orders grow over the round, stay in the place, never name a thing twice
  for (int p = 0; p < SHOP_PLACES; p++) {
    uint32_t rng = 12345 + p;
    for (int round = 0; round < 50; round++) {
      for (int level = 0; level < SHOP_GUESTS_PER_ROUND; level++) {
        std::vector<ShopLine> o = shopMakeOrder((ShopPlace)p, level, rng);
        const size_t wantLines = level < 2 ? 1 : level < 4 ? 2 : 3;
        CHECK_EQ(o.size(), wantLines);
        std::set<int> items;
        for (const ShopLine& l : o) {
          CHECK(shopPlaceOf(l.item) == (ShopPlace)p);
          CHECK(l.qty >= 1 && l.qty <= (level == 0 ? 2 : 3));
          CHECK(items.insert((int)l.item).second);
        }
        // What the guest says is heard back as the order
        const std::string said = shopOrderSentence(o, rng);
        std::vector<ShopDiff> d;
        CHECK_EQ((int)shopCompare(o, shopHear(said.c_str()), (ShopPlace)p, d), (int)SHOP_MATCH_EXACT);
      }
    }
  }
  uint32_t rng = 7;
  const std::string s = shopOrderSentence(order({{I::Lemonade, 2}}), rng);
  CHECK(s.find("two lemonades") != std::string::npos);
  CHECK(s.find("please") != std::string::npos);
}

static void testLooksAndRequest() {
  uint32_t rng = 99;
  std::set<std::string> seen;
  for (int k = 0; k < 40; k++) {
    ShopLook l = shopMakeLook(rng);
    CHECK(l.skin < SHOP_SKINS && l.hair < SHOP_HAIRS && l.style < SHOP_STYLES && l.hat < SHOP_HATS && l.shirt < SHOP_SHIRTS);
    CHECK(l.voice < SHOP_VOICES);
    seen.insert(std::to_string(l.skin) + "-" + std::to_string(l.style) + "-" + std::to_string(l.hat) + "-" +
                std::to_string(l.shirt));
  }
  CHECK(seen.size() > 30);  // guests look different
  CHECK_EQ(std::string(shopVoiceName(0)), std::string("alloy"));

  ShopRequest r;
  CHECK(shopParseRequest("play|lemonade|seller", r));
  CHECK(r.hasPlace && r.place == SHOP_BAR && r.hasRole && r.role == SHOP_SELLER);
  CHECK(shopParseRequest("play|Bäckerei|Gast", r));
  CHECK(r.hasPlace && r.place == SHOP_BAKERY && r.hasRole && r.role == SHOP_GUEST);
  CHECK(shopParseRequest("play|market|guest", r));
  CHECK(r.place == SHOP_MARKET && r.role == SHOP_GUEST);
  CHECK(shopParseRequest("play||", r));
  CHECK(!r.hasPlace && !r.hasRole);
  CHECK(shopParseRequest("play", r));
  CHECK(shopParseRequest("play|||", r));  // as the chat model wrote it for "Let's play shop" (2026-11-14)
  CHECK(!r.hasPlace && !r.hasRole);
  CHECK(shopParseRequest("play|shop|", r));
  CHECK(r.hasPlace && r.place == SHOP_MARKET && !r.hasRole);
  CHECK(shopParseRequest("play|Laden an der Kasse|Verkäufer", r));
  CHECK(r.hasPlace && r.place == SHOP_MARKET && r.role == SHOP_SELLER);
  CHECK(!shopParseRequest("", r));
  CHECK(!shopParseRequest("practice|x|y", r));
}

int main() {
  testCatalog();
  testPhrases();
  testHearing();
  testCompare();
  testReplies();
  testOrders();
  testLooksAndRequest();
  std::printf("%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
