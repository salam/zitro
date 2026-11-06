# Recipes

## Requirements

- Kind `recipe`, query `recipe:<dish in English>`, for example `recipe:pancakes`. `turnsWithGyro` is false.
- Everything shown comes from a Wikibooks page, unchanged apart from markup and characters the font lacks. The AI only names the dish and never supplies ingredients, quantities or steps.
- Data is prepared by `scripts/make_recipe_data.py` into `webpage2/data/recipe/` and read from `EDU_DATA_BASE_URL "recipe/"`:
  - `index.txt`: one line per name, `<name>|<slug>|<languages>`. Names are looked up exactly after normalizing (lower case, letters and digits, one space between words), because a near match could show a different dish.
  - `<slug>.<de|en>.json`: `title`, `lang`, `source`, `url`, `revid`, `license`, `ingredients`, `steps`.
  - `CREDITS.txt`: every page used, with link and licence.
- No match, a failed download or any unexpected file: `fetch` returns false and the picture shows.
- Language: the device's language if the dish has it, otherwise the other language, marked on the bottom line as `Wikibooks (EN)` or `Wikibooks (DE)`. There is no AI translation. Of the 61 dishes, 18 have both languages, 21 only German and 22 only English, so a German stick shows English for those 22. German and English versions of a dish are separate recipes from separate pages, so their quantities can differ.
- Screen: ingredients first, paged, then one step at a time. Header `Schritt 3/8` or `Step 3/8`. A step too long for one screen continues on further pages, numbered `2/3` at the bottom right. Text is always the 10 pt font, 4 lines per page, and is never made smaller.
- Button C: next page. After the last page it returns false, which goes back to the text, and the next look starts at the ingredients. Button B: previous page, stays on the first.
- The label on the bottom line is the source page's title, so a wrong lookup is visible.
- "Ask an adult" marker on a step whose text matches `RECIPE_ADULT_WORDS` or `RECIPE_ADULT_PARTS` in `recipe_core.h` (hot, sharp or oven work, German and English), after blanking `RECIPE_ADULT_EXCEPTIONS` such as "baking powder". Over-marking is accepted. A step without the marker is not a promise that it's safe: the check only knows these words.
- Alcohol: a recipe matching `RECIPE_ALCOHOL_WORDS` or `RECIPE_ALCOHOL_PARTS` anywhere in title, ingredients or steps is left out by the generator (also when the German page's box says `Alkohol = Ja`) and refused again by the stick. Wine and cider vinegar count as alcohol. The generator reads both lists from `recipe_core.h`, so there is one list.
- Limits: files over 16 KB (`RECIPE_BODY_LIMIT`), more than 40 ingredients or 40 steps, more than 12 KB of text or text that doesn't fit the page tables are refused whole. A recipe is never cut, because a missing step is worse than none.
- The screen stays on for 5 minutes after the last button press, since a step takes longer than the usual 30 seconds. After that the normal sleep timers run. Waking from deep sleep loads the recipe again and starts at the ingredients.
- Memory: recipe in PSRAM from `fetch` until `unload`, layout in PSRAM while the view is open. Network only in `fetch`.
- Pages are only taken if they have a clear ingredients part and steps, no leftover markup, no character outside the font and no more than 32 screens in total. Plain text after an ingredient list ends the list. Only the first variant of a page is used. A single paragraph of steps is split into its sentences, word for word.
- To update the set: edit `DISHES` in the generator, run `python3 -I scripts/make_recipe_data.py --cache <scratch dir>`, read the "left out" lines, run `tests/recipe/run.sh`.

## User guide

Ask how to make something: "Wie mache ich Pfannkuchen?" or "How do I make pizza?". If Zitro has that recipe, it opens right away. If not, you get the usual picture.

First you see what you need, then one step at a time.

- **Button C**: next. After the last step you're back at the text answer.
- **Button B**: back.
- **Button A**: new question, as always.

"Schritt 3/8" means step 3 of 8. A small number at the bottom right, such as 2/3, means this step goes on: press Button C.

An orange **"Erwachsene fragen"** marker means the step has something hot or sharp in it, or the oven. Ask a grown-up to help. Without the marker you should still take care: Zitro only looks for certain words.

There are 61 dishes, such as pancakes, pizza, soups, cakes and cookies. 39 of them are in German and 40 in English. If a recipe only exists in the other language, Zitro shows it in that language and says so at the bottom, for example "Wikibooks (EN)". Recipes with alcohol are left out.

The recipes come from Wikibooks, written by volunteers. They aren't all written for children, so some steps are long and use cooks' words.

## Release note

- Cook with Zitro: ask "How do I make pancakes?" and follow the recipe step by step, with Button C for next and Button B for back.

## Sources and licences

**Used: Wikibooks** (Cookbook at en.wikibooks.org, Kochbuch at de.wikibooks.org). Text under CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/). Commercial use is allowed with attribution, a licence notice and a note of changes. Wikimedia's terms accept a link to the page as credit for its authors.

- Attribution: the bottom line names Wikibooks and the page title, the ingredient screen shows "CC BY-SA 4.0", and `webpage2/data/recipe/CREDITS.txt` lists every page with its link, the licence and the changes made. Each file also has its `url` and `revid`.
- Share-alike: applies to the recipe files, which stay under CC BY-SA 4.0 and are served separately from the firmware. It's not legal advice that this separation is enough; a lawyer should confirm before relying on it for the sold device.
- Images from Wikibooks are not used.

**Not used: TheMealDB.** Its API page limits the free key: "You can use the test API key "1" during development of your app or for educational use. However you must become a supporter if releasing publicly on an appstore." Its terms add "You cannot publish apps to an appstore unless you are a paid subscriber" and "You cannot resell our API in any way without specific permission". The stick is sold, so the free key isn't suitable. A paid key might be, but the terms talk about app stores and don't say how a key built into a sold device counts. The text has no stated licence either, and some of it reads like it was copied from other cooking sites, which its terms themselves restrict. Checked 2026-11-06.

**Fit for small screens, measured with the real font widths** (`tests/recipe/run.sh`): 79 files, longest 30 screens. Quantities and steps are the source's and the writing is for adults, so this suits cooking together with an adult better than a child reading alone.
