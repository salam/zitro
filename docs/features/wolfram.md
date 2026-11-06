# Wolfram results as text and formulas

Sharp text and real formulas in place of one tall picture shrunk to 240 px. Kind: `wolfram`, used by `is_wolfram`, not by the AI's `model` field.

## Requirements

### Flow

- `is_wolfram` answers take this path. The AI prompt is unchanged: the feature has no `promptRule`.
- The question is translated by the existing `aiChat` step, once. Then `GET https://www.wolframalpha.com/api/v1/llm-api?input=<urlencoded>&maxchars=900&units=metric`, header `Authorization: Bearer WOLFRAM_APPID`, inside `EDU_LOOKUP_DEADLINE_MS`. The key is not in the address and not logged. `maxchars` stays at 900: at 150 the API answered 501 for a question it can do.
- The result opens straight away. `response` (the AI's short text) stays available behind it.
- The Simple API picture (`fetchWolframSimpleImage`) is the fallback, with the question already translated, when: the HTTP answer isn't a complete 200 (501, 403, timeout, overflow over 6144 bytes), the body isn't valid UTF-8 text, nothing readable is in it, the first section holds a character neither a font nor a drawing can show, or the English question has 64 characters or more (it wouldn't fit `ModelRef.query`, so it couldn't come back after deep sleep). Only if that fails too: "Nicht verstanden." as before.
- Deep sleep saves `wolfram:<question>` and asks Wolfram again on wake. Nothing from a text result is stored.
- Serial `model wolfram:<English question>` runs it without voice or AI.

### Sections (`wolfram_core.h`)

- A section header is a line ending in `:`, after a blank line or another header, at most 100 bytes. Sections with nothing left in them are not pages.
- Dropped: `Query`, `Assumption(s)`, the website link, `Manipulatives illustration`, `Typical human computation ...`, `Interpretation(s)`, `image:` lines, `Wolfram Language code:` lines, lines with `assumption=`, a trailing empty table cell, sections holding only a remark in parentheses.
- Order: the answer; then `Input` and `Input interpretation`; then the rest in Wolfram's order. The answer is the first section titled `Result`, `Equation`, `Solution(s)`, `Statement`, `Value`, `Sum`, `Product`, `Definition`, `Derivative...`, `Indefinite integral...`, `Definite integral...`, `Limit...`, `Infinite sum...`, or ending in `result(s)` or `solution(s)`. "First" is deliberate: for `formula for kinetic energy` the `Equation` comes before a `Result` computed from values nobody gave. Without any: the first section that isn't input.
- Assumptions stay visible. `Assuming ...` lines from `Assumption(s)` (at most 4) and `(assuming ...)` lines are pinned to the bottom of every page of their section, unless they'd take over half its room: then they follow in the normal flow. Other remarks like `(Taylor series)` stay below their line, smaller.
- Limits: 16 sections, 64 lines. Beyond that a section is dropped whole, never as half a list.
- Bottom line: `<Input interpretation or Input or Query> - Wolfram|Alpha`, the name cut if it has to be, the source never. If the name holds a character only drawn elsewhere: `Wolfram|Alpha` alone.
- German device: titles from a table; a title not in it stays English. Wolfram's own text is never translated.

### Formulas (`wolfram_formula.h`, `wolfram_layout.h`)

- Only `/ ^ _` and function calls bind; everything else stays a flat row in Wolfram's order. Wolfram's reading: a space is a product, left to right. `1/4 (b - 5)^2` is a quarter times the square. `1/2 e^(-i x)` has the exponential outside the bar. `d/dx(f)` has `f` outside. `(m v^2)/2` loses the parentheses to the bar.
- Typeset: stacked fractions (`a/b` inline in scripts, with its parentheses back), powers, subscripts, `sqrt`, `cuberoot`, `integral` with limits (only with a `dx`-like word on the line), `sum_`, `product_`, `lim_`, parentheses drawn tall around tall content, words like `(cubic centimeters)` in grey.
- A line is a formula only if read fully **and** the tree written back equals the source apart from spaces. Otherwise it's plain text. That includes `0.583^_`, `[1; 2^_]`, `(2, 5]`, `7/12 | 14/24`, tables, sentences (two words of 4 letters or more), and anything with two readings: `1/2x`, `x^2y`, `1/x(x + 1)`, `a/b/c`, `x^y^z`. Words of 3 letters or more never go on a fraction bar, so `and/or` isn't a fraction and `m/s` is. Nesting is limited to 4 levels of parentheses, 120 nodes, 220 bytes.
- Plain text wraps at spaces. A longer word wraps at any character, so a 66-digit number continues on the next row, with every digit there. `^` followed by plain digits is raised (`10^-22`, `a^2`).
- Every text piece on screen is a slice of Wolfram's response: numbers can't change on the way. `tests/wolfram` pins this on all fixtures and on fuzzed input: the digits on screen are the digits in the source, in order; plus hand-pinned numbers.
- Fonts end at U+05FF. Drawn as lines, in formulas and in plain text: `≈ ≠ ≤ ≥ ∞ √ ∫ ∑ ∏ → − ∓ … ⋅`, dashes and curly quotes. Anything else above U+05FF gets a box in later sections, never a gap, and makes the picture the answer if it's on the first section.
- Sizes: 14 pt with scripts in 10 pt if the whole section fits without wrapping; otherwise 10 pt with scripts in DejaVu 9 (Roboto 10 for non-ASCII). Too wide after breaking at `= + -` or before words, then between factors, or too tall: plain text. One layout is built when the view opens, in PSRAM; `frame()` only draws it, once per page change.

### Plot

- The first `image:` directly followed by `Wolfram Language code: Plot[` is the plot, if it's `https`, on `*.wolframalpha.com`, a plain `.png` path. Number lines, pie charts and so on aren't loaded.
- Fetched in `fetch()` if 2.5 s of the deadline remain. At most 64 KB, a real PNG, at most 700000 pixels. Decoded into a temporary PSRAM sprite (about 800 KB for 903x446), shrunk by averaging to fit 240x117 so thin lines stay lines, kept as about 55 KB. Any trouble: no plot page, the answer stands.
- It's a page of its own at the place of its section. `graph:` stays what handles "draw this function".

### Buttons

| Input | In the result |
| --- | --- |
| Button A | New question |
| Button C | Next page. After the last page: the text answer. From the text: the first page again |
| Button B | Previous page. Nothing on the first |
| Hold Button B | Loads the Simple API picture (one more API call), which replaces the text result for this answer, and opens the printer list. If it can't be loaded, the text result stays |

Footer shows `page/pages` at the right when there are several. `turnsWithGyro` is false.

### Printing

The printer path is unchanged: it needs a picture in `SHOW_IMAGE`. The fallback picture prints as before. A text result has no picture, so holding Button B fetches Wolfram's picture first and prints that: the page shows Wolfram's result page, not the formulas on the screen. `printtest wolfram` still works.

### Limits

Flash: 26.8 KB. PSRAM: about 7 KB per answer, 17 KB more while the view is open, plus the plot. A plot adds up to about 3 s to the answer (estimate, not measured on the stick).

Not done: Wolfram's "You could instead try" suggestion from a 501 is only skipped, not shown.

Seen but unchanged: the fallback's old `Serial.println("Wolfram URL: " + url)` prints the AppID on the serial port. Its address also carries the key, where the new path uses a header. Like every call in this firmware, both use `setInsecure()`.

### Tests

`tests/wolfram/run.sh`, with sanitizers. 25 real API answers as fixtures, with no AppID in them. Checks the first section of each, noise, assumptions, order, German titles, plots, 47 hand-written formula shapes, 42 lines that must stay plain text, numbers, symbols, layout bounds, plot helpers, broken input (cut UTF-8, NUL, CRLF, 6000-byte lines, 3000-deep parentheses, carets, too many sections and lines) and 23000 fuzzed answers and formulas. With a `.bmp` path as argument it also draws every page with the real fonts, to look at. The stick itself hasn't run any of this.

## User guide

Ask Zitro to calculate something: "Was ist die Ableitung von x^2 sin(x)?", "Löse x^2 + x - 1 = 0", "Wie gross ist das Volumen eines Torus mit Radius 5 cm?"

The result opens right away, with real fractions, powers and roots. The small grey lines say what Wolfram assumed, like "assuming inner radius b": they matter, so read them.

- **Button C**: next page. After the last page you see Zitro's text answer. Press again to go back to the start.
- **Button B**: one page back.
- **Hold Button B**: to print. Zitro fetches Wolfram's full picture of the result and opens the printer list. The print shows that picture, not the formula on the screen, and the picture stays on screen afterwards.
- **Button A**: new question.

The numbers come straight from Wolfram|Alpha, unchanged, and a long number carries on in the next line. The bottom line shows what Wolfram understood. If Wolfram's answer can't be shown as text, you get the Wolfram picture as before. Some answers have a plot too: it comes as a page of its own.

## Release note

- **Sharp math from Wolfram**: calculations now open as crisp fractions, powers and roots made for your Zitro's screen. Press the button to step through the result, its plot and more!

## Merge notes

- `USERGUIDE.md` "Calculations are answered with Wolfram Alpha" gets the section above. The deep-sleep line stays true for the fallback picture; text results are asked for again on wake, which needs WiFi.
- `REQUIREMENTS.md`: add the requirements above. The rule "`create_image` and calculations (`is_wolfram`) win over `model`" stays: `is_wolfram` now uses the `wolfram` kind itself.
- A change from today: a Wolfram answer used to show the text first, with the picture a press away. It now opens on the result. You asked for that.

## Sources and licences

Wolfram|Alpha LLM API, read 6 Nov 2026: [documentation](https://products.wolframalpha.com/llm-api/documentation), [API terms](https://products.wolframalpha.com/api/termsofuse), [commercial terms](https://products.wolframalpha.com/api/commercial-termsofuse). The documentation says use is "subject to the API Terms of Use and Commercial Terms of Use", and offers a "free non-commercial development account" next to paid commercial plans.

**For you to decide. The free account is non-commercial and the stick may be sold.** As far as these terms go (not legal advice), a sold stick calling Wolfram needs a commercial licence, with monthly fees by number of queries. Points that apply:

- **Selling**: no selling or passing on Wolfram data or API access without written permission. An AppID may not be used by others: one key built into every sold stick looks like that. This is true of the Simple API today as well.
- **Link**: the terms require a "conspicuous hyperlink" to the result page on every page with results. A 240x135 screen has no link. The bottom line names Wolfram|Alpha, which isn't the same thing. Asking Wolfram, or a QR code, are ways out.
- **Commercial terms**: "Computed by Wolfram|Alpha" with a link, a display area agreed with Wolfram, and showing Input Interpretation. The bottom line and the second page show the interpretation; the rest isn't met.
- **Caching**: not allowed. Text results aren't stored, but the picture saved to flash before deep sleep (`REQUIREMENTS.md`, `/lastimage`, in place before this change) may count as caching.
- **Disclaimer**: Zitro's own terms have to link Wolfram's terms and carry the sentence "Results and information from this site are not a certified or definitive source of information that can be relied on for legal, financial, medical, life-safety or any other critical purposes."
- **Quota**: on your plan. An answer takes one LLM call, a fallback one more, printing one more. The plot image isn't an API call, as far as it can be told from the outside.
