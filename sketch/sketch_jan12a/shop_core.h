// Pure logic of the shop game ("Verkäuferlis"): the goods of each place, the orders guests make, what the stick
// hears in a child's English order, and what guest and seller answer. Everything is decided here, on the stick:
// no chat model takes part, so answers come fast and nothing is made up.
// No Arduino includes, tested on the host: tests/shop/run.sh. Requirements: docs/features/shop.md
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

enum ShopPlace : uint8_t { SHOP_BAR, SHOP_BAKERY, SHOP_MARKET, SHOP_PLACES };
enum ShopRole : uint8_t { SHOP_SELLER, SHOP_GUEST };

static const int SHOP_GOODS_PER_PLACE = 8;
static const int SHOP_GUESTS_PER_ROUND = 5;
static const int SHOP_MAX_QTY = 20;

enum class ShopItem : uint8_t {
  Lemonade, OrangeJuice, AppleJuice, Water, Tea, HotChocolate, IceCream, Cookie,  // lemonade bar
  Bread, Roll, Croissant, Pretzel, Cake, Muffin, Doughnut, Sandwich,              // bakery
  Apple, Banana, Orange, Carrot, Tomato, Egg, Milk, Cheese,                       // shop
  COUNT
};

struct ShopItemInfo {
  const char* single;  // after "a"/"an" or "one": "lemonade", "glass of water"
  const char* many;    // after a number: "lemonades", "glasses of water"
  bool an;             // "an ice cream"
  const char* word;    // the name alone, for the hint: "lemonade"
  const char* kind;    // "we don't have ...": "lemonade", "bananas"
  const char* names;   // what the stick hears: singular forms, then after ';' the plural ones, '|' between forms
};

static const ShopItemInfo SHOP_ITEM_INFO[(int)ShopItem::COUNT] = {
    {"lemonade", "lemonades", false, "lemonade", "lemonade", "lemonade;lemonades"},
    {"orange juice", "orange juices", true, "orange juice", "orange juice", "orange juice;orange juices"},
    {"apple juice", "apple juices", true, "apple juice", "apple juice", "apple juice;apple juices"},
    {"glass of water", "glasses of water", false, "water", "water", "water;waters"},
    {"cup of tea", "cups of tea", false, "tea", "tea", "tea;teas"},
    {"hot chocolate", "hot chocolates", false, "hot chocolate", "hot chocolate",
     "hot chocolate|chocolate|cocoa;hot chocolates|chocolates"},
    {"ice cream", "ice creams", true, "ice cream", "ice cream", "ice cream|icecream;ice creams|icecreams"},
    {"cookie", "cookies", false, "cookie", "cookies", "cookie|biscuit;cookies|biscuits"},
    {"loaf of bread", "loaves of bread", false, "bread", "bread", "bread;breads"},
    {"bread roll", "bread rolls", false, "bread roll", "bread rolls", "bread roll|roll|bun;bread rolls|rolls|buns"},
    {"croissant", "croissants", false, "croissant", "croissants", "croissant;croissants"},
    {"pretzel", "pretzels", false, "pretzel", "pretzels", "pretzel;pretzels"},
    {"piece of cake", "pieces of cake", false, "cake", "cake", "cake|chocolate cake;cakes|chocolate cakes"},
    {"muffin", "muffins", false, "muffin", "muffins", "muffin;muffins"},
    {"doughnut", "doughnuts", false, "doughnut", "doughnuts", "doughnut|donut;doughnuts|donuts"},
    {"sandwich", "sandwiches", false, "sandwich", "sandwiches", "sandwich;sandwiches"},
    {"apple", "apples", true, "apple", "apples", "apple;apples"},
    {"banana", "bananas", false, "banana", "bananas", "banana;bananas"},
    {"orange", "oranges", true, "orange", "oranges", "orange;oranges"},
    {"carrot", "carrots", false, "carrot", "carrots", "carrot;carrots"},
    {"tomato", "tomatoes", false, "tomato", "tomatoes", "tomato;tomatoes|tomatos"},
    {"egg", "eggs", true, "egg", "eggs", "egg;eggs"},
    {"bottle of milk", "bottles of milk", false, "milk", "milk", "milk;milks"},
    {"piece of cheese", "pieces of cheese", false, "cheese", "cheese", "cheese;cheeses"},
};

struct ShopPlaceInfo {
  const char* german;
  const char* english;
  ShopItem goods[SHOP_GOODS_PER_PLACE];
};

static const ShopPlaceInfo SHOP_PLACE_INFO[SHOP_PLACES] = {
    {"Limonadenbar", "Lemonade bar",
     {ShopItem::Lemonade, ShopItem::OrangeJuice, ShopItem::AppleJuice, ShopItem::Water, ShopItem::Tea,
      ShopItem::HotChocolate, ShopItem::IceCream, ShopItem::Cookie}},
    {"Bäckerei", "Bakery",
     {ShopItem::Bread, ShopItem::Roll, ShopItem::Croissant, ShopItem::Pretzel, ShopItem::Cake, ShopItem::Muffin,
      ShopItem::Doughnut, ShopItem::Sandwich}},
    {"Laden", "Shop",
     {ShopItem::Apple, ShopItem::Banana, ShopItem::Orange, ShopItem::Carrot, ShopItem::Tomato, ShopItem::Egg,
      ShopItem::Milk, ShopItem::Cheese}},
};

static inline const ShopItemInfo& shopItemInfo(ShopItem item) { return SHOP_ITEM_INFO[(int)item]; }
static inline const ShopPlaceInfo& shopPlaceInfo(ShopPlace place) { return SHOP_PLACE_INFO[place]; }
static inline ShopPlace shopPlaceOf(ShopItem item) { return (ShopPlace)((int)item / SHOP_GOODS_PER_PLACE); }

// ---------- Orders ----------

struct ShopLine {
  ShopItem item;
  int8_t qty;  // -1: a plural without a number ("lemonades, please")
};

struct ShopHeard {
  std::vector<ShopLine> lines;  // in the order the child said them
  bool please = false;
};

static inline std::string shopNumberWord(int n) {
  static const char* const words[] = {"zero",   "one",     "two",      "three",    "four",    "five",    "six",
                                      "seven",  "eight",   "nine",     "ten",      "eleven",  "twelve",  "thirteen",
                                      "fourteen", "fifteen", "sixteen", "seventeen", "eighteen", "nineteen", "twenty"};
  return n >= 0 && n <= 20 ? words[n] : std::to_string(n);
}

static inline std::string shopCapitalized(std::string s) {
  if (!s.empty() && s[0] >= 'a' && s[0] <= 'z') s[0] = (char)(s[0] - 'a' + 'A');
  return s;
}

// "a lemonade", "an ice cream", "two glasses of water"
static inline std::string shopPhrase(ShopItem item, int qty) {
  const ShopItemInfo& it = shopItemInfo(item);
  if (qty == 1) return std::string(it.an ? "an " : "a ") + it.single;
  if (qty < 1) return it.many;
  return shopNumberWord(qty) + " " + it.many;
}

// "two lemonades, a cookie and an ice cream"
static inline std::string shopListPhrase(const std::vector<ShopLine>& lines) {
  std::string out;
  for (size_t k = 0; k < lines.size(); k++) {
    if (k > 0) out += k + 1 == lines.size() ? " and " : ", ";
    out += shopPhrase(lines[k].item, lines[k].qty);
  }
  return out;
}

// ---------- Hearing an order ----------

// Lower-case ASCII words; an apostrophe joins ("I'd" is "id"), anything else that isn't a letter or digit splits
static inline std::vector<std::string> shopWords(const char* text) {
  std::vector<std::string> words;
  std::string cur;
  for (const char* p = text ? text : ""; *p; p++) {
    unsigned char c = (unsigned char)*p;
    if (c >= 'A' && c <= 'Z') c = (unsigned char)(c - 'A' + 'a');
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
      cur += (char)c;
    } else if (c == '\'') {
      continue;
    } else if (!cur.empty()) {
      words.push_back(cur);
      cur.clear();
    }
  }
  if (!cur.empty()) words.push_back(cur);
  return words;
}

struct ShopName {
  std::vector<std::string> words;
  ShopItem item;
  bool plural;
};

static inline const std::vector<ShopName>& shopNames() {
  static std::vector<ShopName> names;
  if (!names.empty()) return names;
  for (int i = 0; i < (int)ShopItem::COUNT; i++) {
    bool plural = false;
    std::string form;
    for (const char* p = SHOP_ITEM_INFO[i].names;; p++) {
      if (*p == '|' || *p == ';' || *p == 0) {
        if (!form.empty()) names.push_back({shopWords(form.c_str()), (ShopItem)i, plural});
        form.clear();
        if (*p == ';') plural = true;
        if (*p == 0) break;
      } else {
        form += *p;
      }
    }
  }
  return names;
}

// The longest name of a thing at word i; its length in words, 0 for none
static inline size_t shopNameAt(const std::vector<std::string>& words, size_t i, ShopItem& item, bool& plural) {
  size_t best = 0;
  for (const ShopName& n : shopNames()) {
    const size_t len = n.words.size();
    if (len <= best || i + len > words.size()) continue;
    bool same = true;
    for (size_t k = 0; k < len && same; k++) same = words[i + k] == n.words[k];
    if (!same) continue;
    best = len;
    item = n.item;
    plural = n.plural;
  }
  return best;
}

// 1 to SHOP_MAX_QTY for a number word or digits, "a", "an" and "another" are one; 0 for anything else
static inline int shopNumberValue(const std::string& w) {
  if (w == "a" || w == "an" || w == "another") return 1;
  for (int n = 1; n <= SHOP_MAX_QTY; n++)
    if (w == shopNumberWord(n)) return n;
  if (w.empty() || w.size() > 2) return 0;
  for (char c : w)
    if (c < '0' || c > '9') return 0;
  const int n = atoi(w.c_str());
  return n >= 1 && n <= SHOP_MAX_QTY ? n : 0;
}

// What the child ordered or served. The latest number before a thing counts ("two, no, three lemonades"); words in
// between don't matter ("two big lemonades"). A thing said again replaces the earlier number, "another" adds one.
static inline ShopHeard shopHear(const char* text) {
  ShopHeard heard;
  const std::vector<std::string> words = shopWords(text);
  int pending = 0;
  bool adds = false;
  for (size_t i = 0; i < words.size();) {
    ShopItem item = ShopItem::COUNT;
    bool plural = false;
    const size_t len = shopNameAt(words, i, item, plural);
    if (len > 0) {
      const int qty = pending ? pending : plural ? -1 : 1;
      ShopLine* found = nullptr;
      for (ShopLine& l : heard.lines)
        if (l.item == item) found = &l;
      if (!found) heard.lines.push_back({item, (int8_t)qty});
      else if (adds && found->qty > 0) found->qty = (int8_t)(found->qty + 1 > SHOP_MAX_QTY ? SHOP_MAX_QTY : found->qty + 1);
      else found->qty = (int8_t)qty;
      pending = 0;
      adds = false;
      i += len;
      continue;
    }
    const int n = shopNumberValue(words[i]);
    if (n > 0) {
      pending = n;
      adds = words[i] == "another";
    }
    if (words[i] == "please") heard.please = true;
    i++;
  }
  return heard;
}

// The answer to "How many?": a number and no thing, else 0 ("a" and "an" alone are no answer)
static inline int shopBareNumber(const char* text) {
  if (!shopHear(text).lines.empty()) return 0;
  int n = 0;
  for (const std::string& w : shopWords(text))
    if (w != "a" && w != "an" && w != "another" && shopNumberValue(w) > 0) n = shopNumberValue(w);
  return n;
}

// ---------- Comparing ----------

enum ShopMatch : uint8_t { SHOP_MATCH_EXACT, SHOP_MATCH_NOTHING, SHOP_MATCH_WRONG, SHOP_MATCH_NOT_HERE };

struct ShopDiff {
  ShopItem item;
  int8_t want;  // 0: not ordered
  int8_t got;   // 0: not said, -1: said without a number
};

static inline int shopQtyOf(const std::vector<ShopLine>& lines, ShopItem item) {
  for (const ShopLine& l : lines)
    if (l.item == item) return l.qty;
  return 0;
}

// The differences: first the ordered things in the order's order, then the extra ones. NOT_HERE: only things
// this place doesn't sell were heard.
static inline ShopMatch shopCompare(const std::vector<ShopLine>& want, const ShopHeard& heard, ShopPlace place,
                                    std::vector<ShopDiff>& diffs) {
  diffs.clear();
  if (heard.lines.empty()) return SHOP_MATCH_NOTHING;
  bool anyHere = false;
  for (const ShopLine& h : heard.lines) anyHere = anyHere || shopPlaceOf(h.item) == place;
  if (!anyHere) {
    for (const ShopLine& h : heard.lines) diffs.push_back({h.item, 0, h.qty});
    return SHOP_MATCH_NOT_HERE;
  }
  for (const ShopLine& w : want) {
    const int got = shopQtyOf(heard.lines, w.item);
    if (got != w.qty) diffs.push_back({w.item, w.qty, (int8_t)got});
  }
  for (const ShopLine& h : heard.lines)
    if (shopQtyOf(want, h.item) == 0) diffs.push_back({h.item, 0, h.qty});
  return diffs.empty() ? SHOP_MATCH_EXACT : SHOP_MATCH_WRONG;
}

// ---------- What guests and sellers say ----------

// The guest, when the child served something else: about the first difference only
static inline std::string shopComplaint(const std::vector<ShopDiff>& diffs) {
  if (diffs.empty()) return "";
  const ShopDiff& d = diffs[0];
  const ShopItemInfo& it = shopItemInfo(d.item);
  if (d.want == 0) return "I didn't order " + shopPhrase(d.item, 1) + ".";
  if (d.got == 0) return "And " + shopPhrase(d.item, d.want) + ", please!";
  if (d.got < 0) return shopCapitalized(shopPhrase(d.item, d.want)) + ", please.";
  if (d.got > d.want) return "Just " + shopNumberWord(d.want) + " " + (d.want == 1 ? it.single : it.many) + ", please.";
  return "I'd like " + shopPhrase(d.item, d.want) + ", please.";
}

// The thing of this place whose number the seller asks for ("How many lemonades?"), or -1
static inline int shopAskedItem(const ShopHeard& heard, ShopPlace place) {
  for (const ShopLine& l : heard.lines)
    if (l.qty < 0 && shopPlaceOf(l.item) == place) return (int)l.item;
  return -1;
}

// Only ordered things are missing, everything said was right: a part of the order ("a cookie" for "two lemonades
// and a cookie"). The next answer adds to it.
static inline bool shopOnlyMissing(const std::vector<ShopDiff>& diffs) {
  if (diffs.empty()) return false;
  for (const ShopDiff& d : diffs)
    if (d.want == 0 || d.got != 0) return false;
  return true;
}

// The earlier answer with the next one: things said again take the new number, "please" stays
static inline ShopHeard shopMerge(const ShopHeard& before, const ShopHeard& next) {
  ShopHeard out = before;
  out.please = before.please || next.please;
  for (const ShopLine& n : next.lines) {
    bool found = false;
    for (ShopLine& l : out.lines)
      if (l.item == n.item) {
        l.qty = n.qty;
        found = true;
      }
    if (!found) out.lines.push_back(n);
  }
  return out;
}

// The seller, when the child ordered. partOfOrder: what was heard is a right part of the list (shopOnlyMissing).
static inline std::string shopSellerReply(ShopMatch match, const ShopHeard& heard, ShopPlace place,
                                          bool partOfOrder = false) {
  if (match == SHOP_MATCH_EXACT)
    return std::string(heard.please ? "Of course! " : "") + "Here you are: " + shopListPhrase(heard.lines) + ".";
  if (match == SHOP_MATCH_NOTHING || heard.lines.empty()) return "Sorry? What would you like?";
  std::string out;
  std::vector<ShopLine> here;
  for (const ShopLine& l : heard.lines) {
    if (shopPlaceOf(l.item) == place) here.push_back(l);
    else if (out.empty()) out = std::string("Sorry, we don't have ") + shopItemInfo(l.item).kind + ".";
  }
  if (here.empty()) return out;
  if (!out.empty()) out += " ";
  for (const ShopLine& l : here)
    if (l.qty < 0) return out + "How many " + shopItemInfo(l.item).many + "?";
  if (out.empty() && match == SHOP_MATCH_WRONG && partOfOrder) return shopCapitalized(shopListPhrase(here)) + ". Anything else?";
  return out + shopCapitalized(shopListPhrase(here)) + "?";
}

// The seller after the third try: says the order right and serves it
static inline std::string shopModelAnswer(const std::vector<ShopLine>& want) {
  return "Ah, " + shopListPhrase(want) + ". Here you are!";
}

// ---------- Randomness ----------

static inline uint32_t shopRandom(uint32_t& state) {
  if (state == 0) state = 0x9E3779B9u;
  state ^= state << 13;
  state ^= state >> 17;
  state ^= state << 5;
  return state;
}

template <size_t N>
static inline const char* shopPick(const char* const (&options)[N], uint32_t& rng) {
  return options[shopRandom(rng) % N];
}

// A guest's order: one thing for the first two guests, two for the next two, three for the last; one or two of
// each for the first guest, up to three later
static inline std::vector<ShopLine> shopMakeOrder(ShopPlace place, int level, uint32_t& rng) {
  const int lines = level < 2 ? 1 : level < 4 ? 2 : 3;
  const int maxQty = level == 0 ? 2 : 3;
  int idx[SHOP_GOODS_PER_PLACE];
  for (int k = 0; k < SHOP_GOODS_PER_PLACE; k++) idx[k] = k;
  std::vector<ShopLine> order;
  for (int k = 0; k < lines; k++) {
    const int j = k + (int)(shopRandom(rng) % (uint32_t)(SHOP_GOODS_PER_PLACE - k));
    const int t = idx[k];
    idx[k] = idx[j];
    idx[j] = t;
    order.push_back({shopPlaceInfo(place).goods[idx[k]], (int8_t)(1 + shopRandom(rng) % (uint32_t)maxQty)});
  }
  return order;
}

static inline std::string shopOrderSentence(const std::vector<ShopLine>& order, uint32_t& rng) {
  static const char* const greetings[] = {"Hello! ", "Hi! ", "Good morning! ", ""};
  const std::string list = shopListPhrase(order);
  std::string out = shopPick(greetings, rng);
  switch (shopRandom(rng) % 4) {
    case 0: return out + shopCapitalized(list) + ", please.";
    case 1: return out + "Can I have " + list + ", please?";
    case 2: return out + "I'd like " + list + ", please.";
    default: return out + "Could I have " + list + ", please?";
  }
}

static inline std::string shopThanks(uint32_t& rng) {
  static const char* const thanks[] = {"Thank you! Bye!", "Thanks a lot! Goodbye!", "Yummy! Thank you!",
                                       "Great, thank you! Bye!"};
  return shopPick(thanks, rng);
}

// The seller greets the child: the first time, then for each new shopping list
static inline std::string shopSellerGreeting(int list, uint32_t& rng) {
  static const char* const next[] = {"Hi again! What would you like?", "Next, please! What would you like?",
                                     "Hello! What can I get you?"};
  return list == 0 ? "Hello! What would you like?" : shopPick(next, rng);
}

// ---------- Guests ----------

static const int SHOP_SKINS = 6, SHOP_HAIRS = 7, SHOP_STYLES = 6, SHOP_HATS = 6, SHOP_SHIRTS = 8, SHOP_VOICES = 13;
static const uint8_t SHOP_HAT_NONE = 0, SHOP_HAT_CAP = 1, SHOP_HAT_BEANIE = 2, SHOP_HAT_FROG = 3, SHOP_HAT_CAT = 4,
                     SHOP_HAT_BEAR = 5, SHOP_HAT_BAKER = 6, SHOP_HAT_VISOR = 7;  // the last two only for sellers

struct ShopLook {
  uint8_t skin, hair, style, hat, glasses, shirt, voice;
};

static inline const char* shopVoiceName(int voice) {
  // gpt-4o-mini-tts voices (developers.openai.com/api/docs/guides/text-to-speech, checked 2026-11-14)
  static const char* const voices[SHOP_VOICES] = {"alloy", "ash",   "ballad",  "coral", "echo",  "fable", "nova",
                                                  "onyx",  "sage",  "shimmer", "verse", "marin", "cedar"};
  return voices[(voice % SHOP_VOICES + SHOP_VOICES) % SHOP_VOICES];
}

static inline ShopLook shopMakeLook(uint32_t& rng) {
  ShopLook l;
  l.skin = (uint8_t)(shopRandom(rng) % SHOP_SKINS);
  l.hair = (uint8_t)(shopRandom(rng) % SHOP_HAIRS);
  l.style = (uint8_t)(shopRandom(rng) % SHOP_STYLES);
  l.hat = shopRandom(rng) % 5 < 2 ? (uint8_t)(1 + shopRandom(rng) % (SHOP_HATS - 1)) : SHOP_HAT_NONE;
  l.glasses = shopRandom(rng) % 10 < 3;
  l.shirt = (uint8_t)(shopRandom(rng) % SHOP_SHIRTS);
  l.voice = (uint8_t)(shopRandom(rng) % SHOP_VOICES);
  return l;
}

// ---------- Starting ----------

struct ShopRequest {
  bool hasPlace = false, hasRole = false;
  ShopPlace place = SHOP_BAR;
  ShopRole role = SHOP_SELLER;
};

static inline bool shopHas(const std::string& s, const char* part) { return s.find(part) != std::string::npos; }

// The answer field: "play|<place>|<role>", place and role as the child said them or empty
static inline bool shopParseRequest(const char* value, ShopRequest& req) {
  req = ShopRequest();
  std::string s;
  for (const char* p = value ? value : ""; *p; p++) s += (*p >= 'A' && *p <= 'Z') ? (char)(*p - 'A' + 'a') : *p;
  if (s.compare(0, 4, "play") != 0 || (s.size() > 4 && s[4] != '|')) return false;
  const size_t bar1 = s.find('|');
  const size_t bar2 = bar1 == std::string::npos ? std::string::npos : s.find('|', bar1 + 1);
  const std::string place = bar1 == std::string::npos ? "" : s.substr(bar1 + 1, bar2 == std::string::npos ? std::string::npos : bar2 - bar1 - 1);
  const std::string role = bar2 == std::string::npos ? "" : s.substr(bar2 + 1);
  if (shopHas(place, "bake") || shopHas(place, "ckerei") || shopHas(place, "beck") || shopHas(place, "brot")) {
    req.hasPlace = true;
    req.place = SHOP_BAKERY;
  } else if (shopHas(place, "lemon") || shopHas(place, "limo") || shopHas(place, "bar") || shopHas(place, "caf") ||
             shopHas(place, "glace") || shopHas(place, "ice")) {
    req.hasPlace = true;
    req.place = SHOP_BAR;
  } else if (shopHas(place, "market") || shopHas(place, "markt") || shopHas(place, "laden") || shopHas(place, "shop") ||
             shopHas(place, "kasse") || shopHas(place, "store") || shopHas(place, "kiosk")) {
    req.hasPlace = true;
    req.place = SHOP_MARKET;
  }
  if (shopHas(role, "sell") || shopHas(role, "verk") || shopHas(role, "kassier")) {
    req.hasRole = true;
    req.role = SHOP_SELLER;
  } else if (shopHas(role, "guest") || shopHas(role, "gast") || shopHas(role, "kund") || shopHas(role, "custom") ||
             shopHas(role, "einkauf") || shopHas(role, "buy")) {
    req.hasRole = true;
    req.role = SHOP_GUEST;
  }
  return true;
}
