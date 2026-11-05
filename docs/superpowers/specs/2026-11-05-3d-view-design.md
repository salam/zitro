# 3D view: real models you turn with your hand

Date: 2026-11-05. Status: draft for approval.

First of five educational projects. The others follow later with their own designs: scrolling band (music, timelines, sing-back), math (formulas, graphs, steps, Wolfram switch), sensor lab, printed worksheets.

## Why

Zitro should teach, not only answer. A molecule, a planet or an enzyme is easier to understand when you can hold it and turn it. The stick has a screen, a motion sensor and WiFi, so it can do that in a way a printed book can't.

The audience runs from kids who are starting to read up to teenagers. The 3D view has to work with almost no reading, and still hold real detail for older kids.

## Goals

- When a question asks about shape, structure or location, Zitro offers a model next to its answer (see "When a model is offered").
- Turning the stick turns the model.
- Everything shown as fact comes from a real source or from exact math, never from the AI's imagination.
- Covers chemistry, enzymes and proteins, DNA, crystals, atoms, geometric solids, the globe, the solar system, the night sky and physics scenes.

## Non-goals

- Quiz or game on the display (user rejected this).
- Cells, organs and human anatomy as 3D models: no small trusted 3D source has been tested. These show the Wikipedia picture.
- Printing a 3D view. That comes with the worksheets project. Hold Button B does nothing in 3D view until then.
- An age setting. To be decided in the math project, where it matters most.
- Pointing the stick at real stars: the motion sensor has no compass.

## Anti-goals: this fails even if it runs, when

- A model shows something the AI made up, such as guessed coordinates or a guessed record ID.
- Turning a model triggers a shake fact, zoom toggle or screen flip.
- A stick lying on a desk stays awake and drains its battery.
- A failed lookup leaves a child at an error screen, when a picture was available.

## The rule that keeps it honest

The AI only names what to look up. It never supplies coordinates, record IDs, counts or other data.

- Fetched content: the on-screen label comes from the source record, so a wrong lookup is visible.
- Built-in content: comes from checked tables or exact math in the firmware.
- Nothing found: Zitro says it has no 3D model and falls back to the Wikipedia picture.
- The written answer stays AI-written, as it is today. The rule covers the model.

## What the user sees

1. They ask "What does caffeine look like?".
2. Status screen "Finding 3D model", then the text answer, with a small 3D hint icon where the rotate icon sits today.
3. They press Button C or do the usual rotate gesture. The model appears.
4. Turning the stick turns the model. Lying still, it turns slowly by itself, so depth stays visible.
5. Button C goes back to the text.

The text comes first, as it does for pictures today.

A small footer names the thing and its source: "Caffeine · PubChem". Molecules also get a colour legend with element letters (C, H, O, N).

### Buttons and gestures in 3D view

| Input | In 3D view |
| --- | --- |
| Button A | New question, as everywhere. Double click as usual. |
| Button B click | Zoom, 3 steps, wraps around |
| Button B hold 1 s | Nothing yet (see Non-goals) |
| Button B hold 3 s | Resets WiFi, unchanged |
| Button C click | Back to text. From text, Button C goes to 3D. |
| Button C three quick presses | Opens settings, unchanged. The view flips on the way, which is harmless. |
| Shake, roll, rotate-to-toggle, auto-flip | Off. Screen rotation locks when entering the view. |

### How turning feels (builder may tune)

The model follows the stick: gyro readings are integrated into the model's rotation, amplified about 2x, so a small wrist turn shows the far side while the screen stays readable. Drift doesn't matter, because there is no fixed "correct" direction.

Globe and solar system keep spinning after a quick flick and slow down, which suits spinning a globe. Other kinds stop with the hand.

## Content kinds

| Kind | Example | Data | Label from | If not available | Phase |
| --- | --- | --- | --- | --- | --- |
| `molecule` | caffeine | PubChem 3D SDF by name, atoms and bonds | PubChem title | Wikipedia picture | 1 |
| `solid` | icosahedron | Computed. Closed list: 5 Platonic solids, cube, cuboid, prisms, pyramids, cylinder, cone, sphere, torus | Built-in name | n/a | 2 |
| `atom` | carbon | Shell model from PubChem periodic table, embedded in firmware | Built-in name | n/a | 2 |
| `globe` | Where is the Matterhorn? | Natural Earth 110m coastlines in firmware, Earth only. Pin from Wikipedia coordinates of the named article | Article title | No coordinates: Wikipedia picture, never a globe | 2 |
| `protein` | hemoglobin | RCSB PDB legacy file, streamed, backbone only | Built-in name, or PDB title | Wikipedia picture | 3 |
| `dna` | DNA | RCSB 1BNA, phosphate backbone (22 points, tested) | Built-in name | n/a | 3 |
| `crystal` | table salt | Built-in lattices made from published cell data: NaCl, diamond, ice, graphite | Built-in name | n/a | 3 |
| `solar` | solar system, or `solar:mars` with that planet highlighted | Computed on the device from JPL's published planet orbit elements and the clock | Built-in name | No clock: orbits only, no positions | 4 |
| `sky` | Orion | Bright stars and constellation lines in firmware. You turn the sky by hand | Built-in name | n/a | 4 |
| `physics` | magnetic field | Computed from textbook equations. Closed list: hydrogen orbitals (1s, 2s, 2p, 3d), magnet field lines, field of two charges, waves | Built-in name | n/a | 4 |

Polyhedra also show face, edge and vertex counts in the device language. Those come from the solid's logical shape, not from drawing triangles. Curved solids (sphere, cylinder, cone, torus) show no counts, because school books differ on them. `solar` carries a "not to scale" note, since true scale would make the planets invisible.

A single planet, moon or the Sun as its own model is out of scope: "What does Mars look like?" gives the Wikipedia picture.

### Tested on 2026-11-05

- **PubChem:** water, caffeine, aspirin, ethanol and glucose all return 3D data, about 4 KB at most, in about 0.4 s. Table salt, sodium chloride, DNA, insulin return 404: PubChem offers no 3D record for them. Nonsense names return 404.
- **PubChem title:** `/compound/name/caffeine/property/Title/JSON` returns "Caffeine".
- **RCSB files:** 1LYZ, 4HHB, 4INS, 1BNA, 1EMA, 2DHB, 1AKE, 6LU7, 1CAT and 1PPB all download; 76 to 474 KB; 102 to 574 backbone points (22 for DNA).
- **RCSB search ranks poorly:** "lysozyme" returns mutants 168L, 169L, 150L first, not 1LYZ.
- **RCSB titles are technical:** "Real-space refinement of the structure of hen egg-white lysozyme".
- **RCSB huge complexes:** 4V6X has no legacy file (404).
- **RCSB backbone-only download** (`models.rcsb.org`) is 4 to 7 times smaller, but failed for 1CAT and needs a second parser. Not used.
- **Periodic table:** PubChem JSON has electron configuration and atom colours, 44 KB. Embed only the needed columns.
- **JPL Horizons:** Mars position in 0.9 s. Too slow for 8 planets per view, so it's only used to check the math in tests.
- **Coastlines:** Natural Earth 110m has 5128 points, about 20 KB at 4 bytes per point.
- **Wikipedia coordinates:** Matterhorn and Pacific Ocean have them; Mars and Photosynthesis don't.
- **COD:** returns NaCl records (a = 5.62 Å), used as the source for the built-in lattice.
- **Not tested:** star catalogue licence (HYG or Yale Bright Star Catalog; check before bundling), drawing speed on the stick.

### Finding the right protein

A built-in table of about 30 classics with real PDB IDs and short DE/EN names: hemoglobin (4HHB), lysozyme (1LYZ), insulin (4INS), GFP (1EMA), trypsin, amylase, catalase (1CAT) and so on. Each ID is checked against its PDB title when the table is written. This also fills the insulin gap in PubChem.

Other names use RCSB search, and only a result whose title contains the search word counts. The footer then shows the PDB title, cut to fit, plus the ID.

## When a model is offered

A model replaces the picture, and only pictures can be printed for now, so a model must not take over everyday questions.

The AI fills `model` only when the question asks one of these:

- What something is made of or how it is built: "What does caffeine look like?", "How is DNA built?"
- Where something is: "Where is the Matterhorn?" gives the globe.
- For 3D or turning explicitly: "Show me a cube in 3D".

Everything else keeps today's picture: "What does the Matterhorn look like?", "What is gold?", "What is a diamond?" give the photo as before.


One new JSON field in the main prompt, `model`: `"<kind>:<query>"` or empty.

```
"model": "molecule:caffeine"
"model": "protein:hemoglobin"
"model": "solid:icosahedron"
"model": "globe:Matterhorn"
"model": "physics:orbital-2p"
```

- Query in English, like `image_query` today. Closed-list kinds must use a listed value.
- Unknown kinds or values are ignored, which means no model. `image_query` stays filled so the fallback works.
- The prompt tells the AI never to invent coordinates, record IDs or numbers.
- A stored `model` reference is also what deep sleep saves.

Priority when several apply:

1. Settings, timers, onboarding, as today.
2. `create_image`: the user explicitly asked for a drawing.
3. `is_wolfram`: the user wants something calculated or plotted.
4. `model`.
5. `image_query`.

The prompt today sends every "Visualisiere" to Wolfram, and Wolfram handles shapes like a torus. It needs new wording so "show me a torus" gives `solid:torus`, while "volume of a torus with radius 5 cm" stays Wolfram.

A result with a model doesn't also fetch a picture. That saves time and memory, and it avoids a clash: in picture view Button C scrolls, so it couldn't also switch between views.

## Firmware changes

Everything lives in new files following the existing pattern: pure logic in a host-testable `.ino`, like `ai_payload.ino`.

- **Parsers (pure, no hardware):** `model` field, SDF, PDB line parser.
- **Engine:** rotation, projection, depth sorting, drawing to a full-screen sprite.
- **Sources:** fetch per kind.
- **Tables:** coastlines, solids, stars, crystals, atom colours, protein list. Generated by scripts in `scripts/`, like the icons and fonts.

### Touch points in existing code

- `DisplayMode` gets `SHOW_MODEL`, with a new `hasModel` next to `hasImage`. Every place that restores a screen needs it: light-sleep wake (`sketch_jan12a.ino:2447`), deep-sleep wake (2505), `redrawPreviousResult()` (2675), after a timer alert (8827), the rotate toggle (9022), Button C (around 9240).
- `updateGyroPower()` (2626) also runs the gyro in `SHOW_MODEL`.
- `loop()`: in `SHOW_MODEL`, skip the orientation block, `detectRollGesture()` and `detectShakeGesture()`. Reset the rotate-toggle statics the way the printer list does, so leaving 3D can't complete a half-finished toggle.
- The view runs one frame per pass through `loop()` and never blocks, so timers, buttons and sleep keep working. A firing timer alert takes over and returns to the model when dismissed.
- `handleVoiceQuery()` (8469): model lookup before `handleImageRetrieval()`, using the same status-screen pattern.
- `GPTResponse` gets `model`; `parseAiAnswer` in `ai_payload.ino` reads it.
- `saveStateToPreferences()` and `restoreStateFromPreferences()` store the reference. Waking from deep sleep fetches again, like pictures.
- `captureCanvas()` and the `tour` serial command include the 3D view.
- A new serial command, `model molecule:caffeine`, opens a model without voice or AI, for testing.

### Sleep

- Motion above a dead-band of about 15 degrees per second counts as activity in 3D view. Sensor noise stays below it, so a stick on a desk sleeps on the normal timers.
- Hard cap: 5 minutes without a button press and normal sleep applies anyway, so a stick in a moving pocket can't drain the battery.
- Motion never wakes a sleeping display.

### Limits

- Molecule up to 200 atoms, 250 bonds. Protein up to 1500 backbone points. Above that: treat as not available.
- PDB: read line by line, keep only `ATOM` lines for CA and P, first alternate position only, stop at the first `ENDMDL`, never hold the whole file. Stop after 1.5 MB.
- Whole lookup: 10 s, then fall back to the picture.
- Fetch before allocating the drawing sprite, because TLS needs the internal heap. Free the sprite when leaving the view.
- Free flash: firmware is 1.85 MB of a 3.3 MB app slot.
- Use `WiFiClientSecure` with `setInsecure()` like all other fetches here, and a generic `User-Agent`. No keys or personal data are sent to these sources.

### Drawing

- Molecules: bonds as lines, atoms as depth-sorted filled circles in standard atom colours.
- Proteins: thick line along the backbone, coloured blue to red from start to end, one colour band per chain.
- Solids: lit faces. Globe: coastlines on a sphere, back side hidden, pin in citrus (#f2b705).
- Target: at least 15 frames per second for 100 atoms, and for the full coastline. **Unproven**: measure on the stick first. If it's missed, cut drawing detail (fewer coastline points, thinner proteins, simpler atom circles) rather than the frame rate.
- Molecules never lose real atoms or bonds, and hydrogens stay. From 100 to 200 atoms, 8 frames per second is acceptable. If that's missed too, lower the atom cap, so those molecules get the picture.

## Sources and credits

PubChem, RCSB PDB, Natural Earth, JPL, Wikipedia and the chosen star catalogue. Check each licence before shipping, and credit them in the footer where fetched, and in `USERGUIDE.md`.

## Done means

- `./verify.sh` passes.
- Host tests in `tests/model/run.sh` (tracked, unlike the gitignored `test/host/`) cover the `model` field (valid, unknown kind, empty), SDF (caffeine, water, broken file), PDB (1LYZ, 1BNA, multi-model file, over-limit file). Real downloaded files serve as fixtures. Solar positions are compared against a Horizons reading.
- On the stick: frame-rate target measured, each anti-goal checked by hand, screenshots from `tour`.
- `REQUIREMENTS.md`, `USERGUIDE.md` and `RELEASE_NOTES.md` updated. This includes the stale "equation" note at `REQUIREMENTS.md:74`: no equation view exists today.

## Phases

Each phase ships on its own.

1. Engine, groundwork and molecules. Starts by measuring drawing speed on the stick.
2. Solids, atoms, globe.
3. Proteins, DNA, crystals.
4. Solar system, sky, physics.

## Decided by the user

- Molecules in colour, working with the gyro.
- Sources: PubChem and others. AI suggests search terms, doesn't invent.
- No on-screen quiz.
- Audience: starting readers to teenagers.
- Existing gestures off in 3D view, Button C switches views; playback and motion count as activity.
- Scope includes geometry, bodies, enzymes, biology, astronomy, physics.

## Calls made by the author, open to correction

- "Bodies" read as geometric solids plus celestial bodies, not human anatomy.
- Text first, then 3D.
- Model follows the hand; spin with momentum for globe and solar system only.
- A found model replaces the picture, so such answers can't be printed until the worksheets project.
- Cells and organs show pictures rather than models.
- Curated protein table first, search second.
- Models only for shape, structure, location or explicit 3D questions; photos stay the default. **Awaiting the user's answer.**
- No coordinates means the picture, because a globe would show Earth under another name.
- Single planets show a picture; `solar` can highlight one.
- No counts on curved solids.
- Molecule frame-rate floor of 8 per second above 100 atoms; atoms are never dropped.

## Left to the builder

File names, sprite memory location, exact amplification, icon design, footer layout, shading, how far to thin the coastlines, the final protein list.
