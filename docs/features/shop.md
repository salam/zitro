# Shop game ("Verkäuferlis")

Part 3 of the English practice (issue #21, this part #25). Logic in `sketch/sketch_jan12a/shop_core.h` (tests:
`tests/shop/run.sh`), drawing in `shop_draw.h` (screens on the computer: `tests/shop/render.sh`, saved in
`screenshots/shop/`), the mode in `shop.ino`. Recording is shared with the vocabulary trainer (`record_speech.ino`).

## Requirements

### What the user asked for (2026-11-14)

- A simple first-person scene at the counter: the lemonade bar, the bakery, the till of a shop. From different sides:
  as the guest, as the seller. New guests keep coming and ask for something. Reference: a picture of a café counter
  with guests whose speech bubbles show "2x [cup]", "3x [lemonade]" (not in the repository: someone else's art).
- "ja loslegen" to the proposals: the goods are built in (not taken from the parents' lists), the child sells or
  shops. Changed while building, see "Serving by voice" below.

### Places and goods

- Three places with eight goods each, English names, drawn from shapes (no picture files):
  - Limonadenbar: lemonade, orange juice, apple juice, water (a glass of), tea (a cup of), hot chocolate, ice cream,
    cookie.
  - Bäckerei: bread (a loaf of), bread roll, croissant, pretzel, cake (a piece of), muffin, doughnut, sandwich.
  - Laden (at the till): apple, banana, orange, carrot, tomato, egg, milk (a bottle of), cheese (a piece of).
- These are Zitro's own everyday words, not a textbook list: "Zitro ships no word lists" still holds for school lists.

### Starting

- New answer field `shop`: `play|<place>|<role>`, place `lemonade`, `bakery`, `shop` or empty, role `seller` (the
  child sells) or `guest` (the child shops) or empty. German words work too (Bäckerei, Laden, Kasse, Verkäufer,
  Kunde). Missing parts keep the last choice (first time: lemonade bar, child sells).
- Start screen: the place, "Du verkaufst" or "Du kaufst ein", four of its goods. A starts, B swaps the role, C goes to
  the next place.
- A takeover mode like the vocabulary trainer: `loop()` hands over to `shopTick()` while `shopActive()`. Holding B
  doesn't reset WiFi, three presses of C don't open the settings. Deep sleep ends the game.

### The child sells (listening)

- A guest walks in (random skin, hair, hat or animal hood, glasses, shirt, voice) and orders in British English:
  "Hi! Can I have two lemonades and a cookie, please?" The bubble shows "...".
- **Serving by voice** (Claude's call while building, instead of the proposed buttons: B is the menu and C is "?", so
  only A was left to choose, add and hand over): A, then the child says what it hands over ("Two lemonades and a
  cookie", "Here you are, two lemonades and a cookie").
- What was said lands on the counter. Right: a heart, "Thank you! Bye!". Wrong: the guest says the first difference
  ("Just two lemonades, please.", "And a cookie, please!", "I didn't order a muffin.") and the bubble shows the order
  in pictures. After the third wrong try the guest takes the order anyway ("Never mind. Thank you! Bye!").
- "?" (C): the guest says the order again, slowly; the second time the bubble shows it in pictures too.
- No sound (volume 0, no key, no network): the bubble shows the pictures right away.

### The child shops (saying the words)

- The seller (with an apron and a hat of the place) greets: "Hello! What would you like?". A board shows the place's
  goods, a paper the shopping list in pictures ("3x [apple]").
- A, then the child orders. Right: the seller serves ("Of course! Here you are: three apples." with "please", else
  "Here you are: ..."), the list gets checks.
- Not right: the seller asks back with what they understood ("Three apples and a piece of cheese?"), puts that on the
  counter and the list shows a check or a cross per line. "How many apples?" when a plural came without a number:
  then a number alone answers it ("Two."). Things the place doesn't sell: "Sorry, we don't have bananas." After the
  third try the seller says the order right and serves it ("Ah, three apples and a bottle of milk. Here you are!").
- "?" (C): Zitro (not the seller) says the list's words slowly; the second time the whole order.

### Hearing an order

- Words are folded to lower-case letters and digits; "I'd" is "id", hyphens split ("ice-cream").
- The longest name wins ("apple juice" before "apple", "bread roll" before "bread"). Plurals, British and American
  words and spellings: biscuit/cookie, donut/doughnut, tomatos, bun/roll, chocolate/cocoa for hot chocolate.
- Numbers: one to ten, digits up to 20, "a", "an". The latest number before a thing counts ("two, no, three
  lemonades"), words in between don't matter ("two big cold lemonades"), "to", "too" and "for" aren't numbers. A thing
  said again replaces its number, "another" adds one. A singular with a number counts ("two lemonade"); a plural
  without a number is "how many?". German words aren't heard as goods.
- Recording: 16 kHz, `language=en`, a prompt naming the place's goods, up to 8 s, ends 1.2 s after the speech stops.

### Round

- 5 guests or lists. One thing for the first two, two for the next two, three for the last; one or two of each for
  the first, up to three later. A heart for every one right the first time without the pictures; the end shows
  "4 von 5 · auf Anhieb richtig".
- Menu (B): Weiter, Selber einkaufen / Selber verkaufen, Zur Bäckerei (the next place), Beenden. Role and place go
  back to the start screen.

### Voices

- `speakTextVoiced()` takes a voice: guests and sellers get one of the 13 voices of `gpt-4o-mini-tts` (alloy, ash,
  ballad, coral, echo, fable, nova, onyx, sage, shimmer, verse, marin, cedar; checked 2026-11-14), with the
  instruction to speak British English clearly and a little slowly. "?" asks for very slow speech.

### Serial commands

`shop play|<place>|<role>`, `shopkey a|b|c`, `shoptype <text>` (stands in for the next recording).

## User guide

**Playing shop.** Say "Lass uns Verkäuferlis spielen", "Ich will Bäckerei spielen" or "Ich kaufe ein". Pick the place
with the upper side button and the side (sell or shop) with the lower one, then press M5.

**You sell.** A guest comes and orders in English. Press M5 and say what you give: "Two lemonades and a cookie". Didn't
understand? Press the upper side button (?): the guest says it again slowly, the second time you see it in pictures.

**You shop.** Your shopping list shows pictures. Press M5 and order in English: "Three apples and a bottle of milk,
please". If the seller understood something else, they ask back. Need help? The upper side button (?) says the words.

## Release note

- **Play shop in English**: say "Lass uns Verkäuferlis spielen". Guests come to your lemonade bar, bakery or shop and
  order in English. Listen and serve them, or swap sides and do the shopping yourself.

## Not yet

- Money and change, prices on the board.
- Goods from the parents' lists (they have no pictures).
- A child test at the stick and real photos of the screens.
