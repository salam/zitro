# Dinosaur fact sheets and the history of life

Status: design, agreed with the user in conversation from 2026-11-08 to 2026-11-12. Not built.

Two pieces of work that share sources and tooling:

1. **Dinosaur fact sheet**: ask Zitro about a dinosaur by name and it shows pages with a picture, the facts, a size comparison and a fun fact.
2. **Timeline "History of life"**: a new timeline topic from the first cells to modern humans.

Base: `origin/main` in its own worktree and branch. Never local `master`, which carries a leaked key in its history (see memory "leaked-openai-key-in-history").

## Why

Zitro is becoming an educational device for children from early readers to teenagers (see `docs/features/*`, and the user's direction of 2026-11-05). Dinosaurs are one of the topics children ask about most. Today, naming a dinosaur gives only the normal Wikipedia picture and the AI's written answer. The user wants a fact sheet that children can page through, and a timeline that puts dinosaurs into the whole history of life.

The rule that keeps Zitro trustworthy still applies (`edu_feature.h`): **the AI only names what to look up. Every fact shown comes from a real source.** Gaps stay gaps and are never guessed.

## Goals

- Any dinosaur name the AI recognises leads to a fact sheet, including rare ones. **The user accepted gaps** ("mit Lücken ist okay").
- Facts come from Wikidata and Wikipedia only.
- The fact sheet has these pages: **picture, facts, size comparison, fun fact**. The order is open (the user said "Abfolge ist egal"). Default order: picture, facts, size, fun fact.
- Facts asked for by the user: picture, size, name, era, egg size, running speed, "and other facts", plus a fun fact.
- The new timeline runs from the first cells to *Homo sapiens*, with at most 24 events.

## Non-goals and anti-goals

- No AI-written facts, numbers or fun facts **on the fact sheet**. The AI on the stick names the animal for the lookup; the sheet's content comes only from the prepared data. (The written answer is the AI's, as today: see "Written answer".)
- No invented body shapes. The size comparison uses plain bars, not a silhouette.
- No quiz mode (rejected by the user on 2026-11-05).
- The existing `dinosaurs` and `earth_history` timelines are not changed.
- No live Wikidata or Wikipedia calls from the stick. Entity JSON is far too large, and the data must not depend on Wikipedia being reachable.
- **Anti-goal:** a number that looks exact but isn't. Every estimate says it is one, and values that disagree are shown as a range.

## Changes to documented behaviour (approved by the user)

1. **Dinosaur names get a fact sheet** instead of only the Wikipedia picture.
2. **Timeline dates may come from Wikipedia.** `docs/features/timeline.md` says every date comes from Wikidata. For the history-of-life topic, an event Wikidata has no date for (for example *Homo sapiens*, the last universal common ancestor, Tiktaalik) may use a date from a Wikipedia sentence. The generator stores that sentence, the article and the revision ID. Approved on 2026-11-09 ("ja").
3. **Fun facts and speeds are chosen, translated and shortened by the AI that builds the data**, from Wikipedia sentences, and **no person reviews them first**. Approved on 2026-11-12 ("ja"). The spec must say this plainly, as `timeline.md` already does for the pictures.

## Two tiers of species

| | Curated (about 50) | Automatic (all other non-avian dinosaur genera) |
| --- | --- | --- |
| Picture | Chosen by hand, pinned by SHA-1, as in the timeline | Wikidata P18, checked as described in "Pictures of the automatic tier", otherwise no picture |
| Length, height, mass | Wikidata, plus Wikipedia sentences read by the builder | Wikidata only (P2043, P2048, P2067) |
| Era, time range | Wikidata (P580/P582, P523/P524), Wikipedia where Wikidata lacks it | Wikidata only |
| Year named | Wikidata (qualifier P574 on P225) or the Wikipedia infobox | Wikidata only |
| Speed, egg size, name meaning, diet, where found | From Wikipedia sentences, read by the builder | None |
| Fun fact | One per language, from a Wikipedia sentence | None |

Why two tiers: our sample of 12 well-known genera (2026-11-09 and 2026-11-11) found the following.

- **Wikidata coverage is good for famous genera and poor for the rest.** Of 5860 genera under Dinosauria, 4042 have a picture, 522 a date, 326 a length and 73 a mass. Birds are probably included in these counts.
- **Speed is never in Wikidata** and appears in Wikipedia for about half of the famous genera. German and English articles often disagree. Stegosaurus: de "6–7 km/h", en "15.3–17.9 km/h". T. rex: de "etwa 20–30 km/h".
- **Egg size appeared in none of the 12 articles.** The one example found was Hypselosaurus (en: "around 1 ft in length").
- **Pulling numbers out of free text automatically is unreliable.** A walking speed reads like a running speed, and a juvenile's size reads like an adult's.

Choosing the curated 50 is the builder's call. Suggested: the most-viewed dinosaur articles on de and en Wikipedia, plus every dinosaur in the existing timelines, plus the best-known pterosaurs and marine reptiles (Pteranodon, Plesiosaurus, Mosasaurus).

### Pictures of the automatic tier (decided by the user on 2026-11-12)

The timeline never uses Wikidata's picture automatically. Here it is used, because there are more than 1000 genera, but only if it passes every check below:

1. **Free licence and a named author**, with the same licence rules as `timeline.md`: no NC, ND, fair use, or GFDL alone.
2. **Category check.** The file's Commons categories are checked against a word list (carcass, prey, kill, blood, wound, fighting and so on), extended from the timeline's list. This is a second net, not the real check.
3. **The building AI looks at every picture** on captioned contact sheets at its prepared size, and removes predation, carcasses, gore and anything else unsuitable for young children. **No person looks**, and the feature spec must say so plainly, as `timeline.md` does.
4. **Pinned by SHA-1** after looking. A changed Commons file is refused until it has been looked at again.

A genus whose picture fails a check gets no picture. Its sheet starts with the facts, subject to the minimum below.

## Data rules

- **Where facts come from.** Every fact keeps its source: the Wikidata property, or the Wikipedia language, article and revision ID plus the sentence it came from. The generator refuses a curated number that does not appear in its stored sentence.
- **Ranges.** Values that disagree, between Wikidata statements or between de and en Wikipedia, are shown as min–max ("12–13 m"). No averages.
- **Estimates.** Speed is always marked "geschätzt" / "estimated". Eggs are shown only when the source ties them to the genus.
- **Units.** Converted exactly from the stated unit: feet, pounds and short tons appear in Wikidata (Spinosaurus height 18 ft, mass 38000 lb). Rounding is sensible and stays within the source's precision.
- **Fun facts.**
  - Taken from one Wikipedia article, from at most two sentences, translated into the other language where needed, and shortened for children.
  - No new claim may appear, and numbers stay unchanged. Suitable for young children: no gore.
  - The bottom line names the source: "Wikipedia (EN): Maiasaura".
- **Device languages.** DE and EN. Every text exists in both. Fun facts are translated at build time, never on the device.
- **Which taxa count.** Genera (P105 = genus), excluding modern birds (Neornithes, the crown group; the builder resolves the QID). Species names such as "Tyrannosaurus rex" are aliases of their genus. The user decided on 2026-11-12 that these groups also get a sheet:
  - **Non-avian dinosaurs**, under Dinosauria (Q430).
  - **Early birds**, the extinct Mesozoic avialans outside the crown group, such as Archaeopteryx. They are dinosaurs, so the sheet notes "Urvogel" / "Early bird" and does not say "kein Dinosaurier".
  - **Pterosaurs.** The sheet says "Flugsaurier – kein Dinosaurier" / "Pterosaur – not a dinosaur".
  - **Marine reptiles of the dinosaur age**: plesiosaurs, ichthyosaurs, mosasaurs. The sheet says "Meeresreptil – kein Dinosaurier" / "Marine reptile – not a dinosaur".
  - The note comes from the taxon's Wikidata lineage (P171), never from the AI. The builder resolves the exact Wikidata QIDs of these clades and records them in the generator. Wikidata's general "bird" item may contain Archaeopteryx in its lineage, so the builder checks that Archaeopteryx and the other early birds pass the exclusion that is implemented.
- **Names.** The lookup index lists the genus name, the de and en labels and the Wikidata aliases ("T-Rex"), normalised as in `protein` (lower case, umlauts spelled out).

## On the stick

- **Kind and prompt rule.** Kind `dino`, query = the name the user said. The prompt rule sets `model` to `dino:<name>` whenever the user asks about a specific dinosaur, pterosaur, marine reptile of the dinosaur age or early bird by name. It leaves `model` empty for general questions ("Warum starben die Dinosaurier aus?").
- **Written answer (decided by the user on 2026-11-12).** The user's words were "Beides, KI-Text beantwortet Frage, und das Factsheet enthält die Zahlen": the AI's written answer answers the question, and the fact sheet carries the numbers.
  - The prompt rule tells the AI to answer the question as usual, but to leave measurements, weights, speeds and dates to the fact sheet, for example "Wie schnell er war, siehst du gleich im Steckbrief.". So no unsourced number appears next to a sourced one. This is my literal reading of the answer, and the user confirmed it on 2026-11-12.
  - The main flow does **not** replace the AI's text the way `clock:` and `iss:` do.
  - Known limit: the AI decides before the lookup. If the sheet has no speed row, "Wie schnell war X?" gets no number. That is honest: no source has one.
- **Lookup.**
  - The list decides first: the AI's text never becomes a file name.
  - The index is split by first letter (`dino/index-t.txt`), so the stick downloads a few KB, not a list of about 1500 names. **My call.** Another design is fine if it keeps both properties.
  - A name not in the index means `fetch()` returns false, and the normal picture shows as today.
  - **Minimum content (my call, after the builder check).** A sheet is shown only if it has its own picture, or at least two fact rows. Otherwise `fetch()` returns false and the normal Wikipedia picture shows as today, so a one-row sheet never replaces the picture. A found sheet skips `handleImageRetrieval()`, as the other features do.
  - **The note for non-dinosaurs** ("Flugsaurier – kein Dinosaurier", "Urvogel") stands under the name on the picture page and the facts page.
- **Pages.** Pages without content are skipped. The page number counts only the pages shown.
  - *Picture.* Full width, the name on a dark band, the picture credit on the bottom line.
  - *Facts.* Name in citrus, then label/value rows: length, height, weight, speed (estimated), egg, era and "lived … million years ago", named (year), name meaning, diet, where found. Rows without a value are left out. If the rows don't fit on one page, the builder splits them across two.
  - *Size.* Drawn to scale next to a person of 1.70 m. **The 1.70 m is my call**, stated in the spec and in the user guide. Length and height are drawn as a citrus bar. A range shows as a darker end piece. If the height is unknown, there is only a thin line, never a guessed height. If the length is unknown, the page is skipped. The scale fits the larger of the two, so the person is tiny next to Brachiosaurus and large next to Velociraptor.
  - *Fun fact.* "Wusstest du?" / "Did you know?" and the sentence in the same font as the written answers (Roboto 10 pt, user decision 2026-11-12), with the source on the bottom line.
- **Buttons and gestures, as in the timeline.** C: next page; after the last page, the written answer. B: previous page. A: new question. Tilt: as in the timeline. **My call.**
- **Fonts.** The small footer font has no umlauts (see `docs/features/protein.md`). Fact and fun-fact text must render with the fonts on the device. The generator refuses characters a font lacks, and the tests wrap every text with the real font widths.
- **Limits.** Like the timeline: picture size 240x123 at most, baseline JPEG, FNV-1a hash in the file, everything in PSRAM. One sheet file plus one picture, both inside `EDU_LOOKUP_DEADLINE_MS`.

Mockup of the agreed look, with real values and their sources: `docs/superpowers/mocks/2026-11-12-dino-factsheet-mock.html` (standalone, the picture is inlined).

## Timeline "History of life"

- New topic `life_history`. Title "Geschichte des Lebens" / "History of life". It goes in the closed topic list, the prompt rule and the user guide.
- **Events.** From the first cells to *Homo sapiens*, at most 24, chosen by the builder. Candidates from the 2026-11-09 sample:
  - With Wikidata dates: eukaryotes (−2.7 billion years, precision 100 Ma), Great Oxidation Event (−2.45 billion), Cambrian explosion (−543 Ma), Archaeopteryx (−150.8 Ma), Purgatorius (−66 Ma), *Australopithecus afarensis* (−3.9 Ma), *Homo erectus* (−1.9 Ma).
  - Without Wikidata dates, needing Wikipedia: the last universal common ancestor, stromatolites, Ediacaran biota, Tiktaalik, Morganucodon, *Homo sapiens*.
- **Precision.** For this topic the generator accepts Wikidata precisions down to 100 million years. A coarse date is shown with "ca.".
- **Deep time needs code changes.**
  - Event years are `int32_t` in `timeline_core.h` and the file format, and the parser refuses anything below `INT32_MIN + 1`. The first cells (about 3.5–4 billion years ago) do not fit.
  - The builder widens the year, or changes the deep-time unit, in the format, the parser, the layout and the tests, without breaking the 25 existing files. A version bump of `ZTL1` is acceptable if all files are regenerated.
  - `timelineFormatMillions` needs "Mrd. Jahre her" / "billion years ago" from 1000 million years on.
- **Pictures.** Child-safety rules exactly as in `timeline.md`: chosen by hand, pinned, checked on contact sheets. Human-evolution pictures need particular care: reconstructions, skulls and the Lucy skeleton are fine, nudity is not.

## Tooling, tests, deployment

- `scripts/make_dino_data.py`, next to `make_timeline_data.py`, reusing its Wikidata, Commons, licence and picture code where that is sensible. It writes `webpage2/data/dino/`: the index files, one sheet per genus, the pictures and `credits.html`.
- Rate limits: Wikidata and Wikipedia answered HTTP 429 to only a few requests per second. The generator caches, batches (`wbgetentities` takes 50 IDs) and honours `Retry-After`.
- `tests/dino/run.sh`, in the style of `tests/timeline/`: parser with broken and hostile input, the page logic for every combination of gaps, the size-comparison scale, text wrapping with the real fonts, and every generated file. The timeline tests are extended for deep time.
- `./verify.sh` must compile the sketch.
- Docs: `docs/features/dino.md`, an update of `docs/features/timeline.md`, `REQUIREMENTS.md`, `USERGUIDE.md` and `RELEASE_NOTES.md`, following `CLAUDE.md`.
- **Upload.** `webpage2/data/dino/` and the new timeline files go to `public_html/zitro.spot/data/` on cyon (see memory "website-deploy-target"). Uploading is outward-facing and needs the user's go-ahead at that moment.
- **Stick tests.** The stick is shared with other sessions (see memory "shared-stick-between-sessions"). Coordinate before flashing.

## Left to the builder

- Which 50 species, which 24 events, and the pictures for both, within the rules above.
- File formats, index splitting and the exact layout of the facts page.
- How the year is widened for deep time.
- Font sizes and line breaks, within the device's fonts.

## Decisions from the builder check (2026-11-12)

| Question | Decided | By |
| --- | --- | --- |
| Pterosaurs, marine reptiles, early birds | They get a sheet, with an honest note | User |
| Pictures of the automatic tier | Licence, word list, AI looks at all, pinned. No person looks | User |
| Written answer | The AI answers the question; the numbers stay in the sheet ("Beides") | User; reading confirmed 2026-11-12 |
| A sheet with no picture and one row | Not shown: own picture or at least two rows, else today's picture | Me |
| Picture credits for the automatic tier | As in the timeline: on the bottom line where it fits, always in `credits.html` | Me |
