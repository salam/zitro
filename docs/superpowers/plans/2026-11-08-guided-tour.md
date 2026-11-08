# Guided Tour Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** After setup, Zitro walks a child through one real voice question, three special answers and a sliding 2x2 grid of everything else.

**Architecture:** A pure state machine in `guide_core.h`, tested on the host, decides what each button does. `guide.ino` draws the screens and is called from `loop()` right after the timer blocks, so while the tour runs no other button handler sees a press. Sky and music reuse the real result views through `fetchModel()`; the first question reuses `handleVoiceQuery()` with small hooks.

**Tech Stack:** Arduino C++ on M5StickC Plus2 (M5Unified, M5GFX), host tests with `c++ -std=c++17`.

**Spec:** `docs/superpowers/specs/2026-11-08-guided-tour-design.md`. Look and icons: `docs/superpowers/mocks/2026-11-08-zitro-tour-mock.html` (the `ICONS` table).

## Global Constraints

- Work on branch `feature/guided-tour`, based on `origin/main`. Never merge local `master` or `main`: they lack the features and carry a leaked key in their history.
- Code prefix is `guide`. `screenshot_tour.ino` already owns `tour*` names. Users read "Tour".
- Strings follow `deviceLanguageIsGerman()`; exact text is in the spec's tables.
- After any `.ino` change run `./verify.sh` from the repo root. It must compile.
- `DejaVu9` is ASCII only. Text with umlauts uses `FontManager::getFont12pt()` or `getFont16pt()`.
- No AI call in the tour except the first question.
- Every commit message ends with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- The stick is shared with other sessions: check the port and coordinate before flashing.

## Review Focus

1. Someone presses M5 twice quickly during a demo: the menu opens and closes on "Weiter", and the tour goes on. Test in Task 1.
2. Someone taps ▶ three times quickly: the settings must not open. Test in Task 1 (`guideBlocksSettings`), wired in Task 2.
3. The first question ends without an answer (night lock, not heard, cancelled, setup request): Retry, Ask, or a stopped tour that comes back at the next power-on. Test in Task 1.
4. The screen is dark or just woke and someone presses a button: it only wakes. Test in Task 1 (`guideAcceptsPress`), wired in Task 2.
5. The stick is left lying mid-tour: after 5 minutes it sleeps normally, also across a `millis()` wrap. Test in Task 1 (`guideKeepAwake`).

---

### Task 1: Tour logic and content tables

**Files:**
- Create: `sketch/sketch_jan12a/guide_core.h`
- Create: `tests/guide/run.sh` (same form as `tests/shake/run.sh`, with `-fsanitize=address,undefined`)
- Create: `tests/guide/test_guide.cpp`
- Copy: the mock to `docs/superpowers/mocks/2026-11-08-zitro-tour-mock.html`

**Interfaces:**
- Produces, all `inline` in `guide_core.h`, no Arduino includes:
  - `enum GuideStep : uint8_t { GUIDE_OFF, GUIDE_ASK, GUIDE_RETRY, GUIDE_ANSWER, GUIDE_PHOTO, GUIDE_SKY, GUIDE_MUSIC, GUIDE_GRID, GUIDE_DONE };`
  - `enum GuideButton : uint8_t { GUIDE_BTN_A, GUIDE_BTN_B, GUIDE_BTN_C };`
  - `enum GuideEvent : uint8_t { GUIDE_EV_NONE, GUIDE_EV_RECORD, GUIDE_EV_STEP, GUIDE_EV_PAGE, GUIDE_EV_QUOTE_SKIP, GUIDE_EV_MENU, GUIDE_EV_FINISH, GUIDE_EV_FINISH_ASK, GUIDE_EV_ABORT };` (`GUIDE_EV_STEP`: the step changed, redraw it. `GUIDE_EV_MENU`: the menu opened, closed or moved.)
  - `enum GuideVoice : uint8_t { GUIDE_VOICE_ANSWERED, GUIDE_VOICE_NOT_HEARD, GUIDE_VOICE_CANCELLED, GUIDE_VOICE_BLOCKED };`
  - `enum GuideMenuItem : uint8_t { GUIDE_MENU_GO, GUIDE_MENU_BACK, GUIDE_MENU_END };`
  - `struct GuideState { GuideStep step; uint8_t page; bool quote; bool menuOpen; uint8_t menuSel; bool answered; };` (`answered`: the first question got an answer; back from Photo goes to Answer if true, otherwise to Retry)
  - `struct GuideTile { const char* icon; const char* titleDe; const char* sayDe; const char* titleEn; const char* sayEn; };`
  - `struct GuideRotate { bool sawPortrait; };`
  - Constants: `GUIDE_PAGES = 4`, `GUIDE_TILES_PER_PAGE = 4`, `GUIDE_QUOTE_MS = 1900`, `GUIDE_HEARD_MS = 1800`, `GUIDE_KEEP_AWAKE_MS = 300000UL`, `GUIDE_SKY_REF = "sky:Orion"`, `GUIDE_MUSIC_REF = "music:twinkle-twinkle"`, `GUIDE_TILES[16]` in the spec's order, icon ids as in the mock (`print time mol body globe iss physics graph math timer recipe clock speak shake paint camera`).
  - `void guideStart(GuideState& s)`: step Ask, everything else zero.
  - `GuideEvent guidePress(GuideState& s, GuideButton b)`
  - `GuideEvent guideVoiceResult(GuideState& s, GuideVoice v)`
  - `int guideMenuRows(const GuideState& s, GuideMenuItem out[3])`, returns the row count
  - `bool guideShouldAutoStart(bool done, bool deepSleepWake, bool chatKey, bool voiceKey)`
  - `bool guideBlocksSettings(const GuideState& s)`: true unless step is Off
  - `bool guideAcceptsPress(bool displayAwake, bool inWakeCooldown)`
  - `bool guideKeepAwake(uint32_t now, uint32_t lastPress)`: `now - lastPress < GUIDE_KEEP_AWAKE_MS`
  - `bool guideRotateStep(GuideRotate& r, bool landscape)`: true on the return to landscape after portrait
  - `float guideSlideStep(float pos, int target, float dt)`: `pos + (target - pos) * (1 - exp(-12 * dt))`, snapping to `target` when closer than 0.004

`guidePress` follows the spec's Flow and Menu sections. Forward moves into Photo, Sky and Music set `quote = true`; back moves set it false. C during a quote clears it and returns `GUIDE_EV_QUOTE_SKIP`.

- [ ] **Step 1: Write the failing tests in `tests/guide/test_guide.cpp`**

Same `CHECK` macro and summary as `tests/shake/test_shake.cpp`. Include `shake`'s `sky_core.h` and `music_songs.h` for the reference checks. One function per name:

```cpp
void ask_records_on_A_and_menu_on_B();     // A -> GUIDE_EV_RECORD, step stays Ask; B -> menuOpen, C -> NONE
void ask_menu_has_no_back_row();           // guideMenuRows == 2: GO, END
void other_menus_have_three_rows();        // Answer: GO, BACK, END
void demo_A_never_acts_directly();         // Photo, quote false: A -> menuOpen, step unchanged
void double_A_continues();                 // Sky: A, A -> step Sky, menuOpen false (Review Focus 1)
void menu_C_wraps_and_B_closes();          // C, C, C on 3 rows -> menuSel 0; B -> menuOpen false
void end_row_finishes();                   // C, C, A -> GUIDE_EV_FINISH, step Off
void back_table();                         // each pair from the spec's "One step back", quote false after
void back_from_photo_after_retry_goes_to_retry(); // Retry, C -> Photo; menu back -> Retry, because answered is false
void grid_pages_forward_then_done();       // C x3 -> page 3; C -> Done
void back_on_first_grid_page_goes_to_music();
void forward_sets_quote_and_C_skips_it();  // Answer C -> Photo, quote true; C -> QUOTE_SKIP, still Photo, quote false
void retry_C_goes_to_photo();
void done_A_finishes_and_asks();           // GUIDE_EV_FINISH_ASK, step Off
void voice_results();                      // ANSWERED->Answer, NOT_HEARD->Retry, CANCELLED->stays Ask (NONE), BLOCKED->Off + GUIDE_EV_ABORT (Review Focus 3)
void auto_start_conditions();              // true only for (false,false,true,true)
void settings_blocked_while_running();     // guideBlocksSettings: Ask true, Off false (Review Focus 2)
void press_only_when_awake();              // (true,false) true; (false,false) and (true,true) false (Review Focus 4)
void keep_awake_expires();                 // (299999,0) true; (300000,0) false; (5,0xFFFFFFF0) true across the wrap (Review Focus 5)
void rotate_needs_full_cycle();            // landscape false; portrait false; landscape true; landscape false
void slide_converges_without_overshoot();  // 200 steps of 0.04 s from 0 to 1 ends at exactly 1; every step <= 1
void tiles_are_complete_and_fit();         // 16 tiles, say strings ASCII, no "Weltkrieg"/"World War"; with real font advances (see below): every title <= 74 px, every sentence wraps to <= 2 lines of 105 px
void references_open();                    // skyFindConstellation("Orion") != SKY_NOT_FOUND; musicFindBuiltIn("twinkle-twinkle") != nullptr
```

Real widths, as `tests/timeline/test_timeline.cpp:45-69` does it: titles from `sketch/sketch_jan12a/fonts/PrimaryFont10pt.h`; sentences from `$HOME/Documents/Arduino/libraries/M5GFX/src/lgfx/Fonts/Custom/DejaVu9.h`, passed to the compiler in `run.sh` through an include path.

- [ ] **Step 2: Run `tests/guide/run.sh`**

Expected: fails to compile, `guide_core.h` not found.

- [ ] **Step 3: Implement `sketch/sketch_jan12a/guide_core.h` and copy the mock into `docs/superpowers/mocks/`**

- [ ] **Step 4: Run `tests/guide/run.sh`**

Expected: all checks pass, sanitizers silent.

- [ ] **Step 5: Commit**

```bash
git add sketch/sketch_jan12a/guide_core.h tests/guide docs/superpowers
git commit -m "Decide what every button does in the tour, with host tests"
```

### Task 2: Tour screens, menu and loop wiring

**Files:**
- Create: `sketch/sketch_jan12a/guide.ino`
- Modify: `sketch/sketch_jan12a/sketch_jan12a.ino` (prototypes near line 481, `HINT_` list near 3338, `drawPrintHint`, `setup()` near 8148, `loop()` near 8992 and after the timer list block near 9165)
- Modify: `sketch/sketch_jan12a/screenshot_tour.ino:358` (serial commands)

**Interfaces:**
- Consumes: Task 1.
- Produces in `guide.ino`: `bool guideActive()`, `bool guideAutoStart()` (true if it started), `void guideStartNow()`, `void guideTick()`, `void guideStop(bool markDone)`, `extern GuideState guideState`, `void guideRenderStep()`. New symbol `HINT_MENU` = 19: three dots.

- [ ] **Step 1: Add `HINT_MENU` to the list and draw it in `drawPrintHint`**

First run `gh pr view 14 --json state`. PR #14 removes `HINT_MIC` and `drawMicMark`, and edits `modelDrawHints`. If it has merged by then, rebase on `origin/main` and keep the microphone tab for the tour's Ask, Retry and Done screens.

- [ ] **Step 2: Implement `guide.ino` for Ask, Retry, Done, the quote screen and the menu**

One 240x135 `M5Canvas` created per draw, like `drawScreen`, with `setPsram(true)` because the menu opens while the result view still holds its sprite. If `createSprite()` fails, skip the draw and log it. Pass it to `captureCanvas()` so `shot` works. Lemon and keycap: reuse the home screen's drawing, extracted from `drawHomeScreen()` into `drawLemon(canvas, x, y)` and `drawKeycapM5(canvas, x, y)`. Menu rows as in the mock: white rounded bar at x 20, width 214, height 30, gap 4, tabs `drawButtonHints(c, rotation, HINT_OK, HINT_CANCEL, HINT_NEXT)`. The other tabs: `HINT_MIC` on Ask, Retry and Done only, `HINT_MENU` at B, `HINT_NEXT` at C where C does something.

- [ ] **Step 3: Implement `guideTick()`**

In this order: if the stick was asleep on the last pass and is awake now, call `guideRenderStep()`, because the wake code in `handleLightSleepWake()` draws `response` by itself. If `!guideAcceptsPress(!displaySleeping && currentPowerState == POWER_ACTIVE && !wakeFromLightSleep, inWakeCooldown())`, any click calls `resetToActiveState()` and `guideRenderStep()`, then return. Read `M5.BtnA/BtnB/BtnPWR.wasClicked()` and feed `guidePress`. On any accepted press, store `guideLastPress = millis()`. Each pass, `if (guideKeepAwake(millis(), guideLastPress)) lastActivityTime = millis();`. End the quote after `GUIDE_QUOTE_MS`. Redraw when the display rotation changed (check every 200 ms) and after a timer alert has been dismissed. `GUIDE_EV_RECORD` is a stub until Task 3. `GUIDE_EV_FINISH` calls `guideStop(true)`.

`guideStartNow()` sets `guideLastPress = millis()`, so a tour started by voice is kept awake from its first screen.

`guideStop(bool markDone)`: writes `tourDone` when asked, closes any view, sets `hasModel = false`, `hasImage = false`, `currentImageUrl = ""`, `response = ""`, `modelRefText = ""`, `currentDisplayMode = SHOW_TEXT`, calls `updateGyroPower()`, draws `MAIN_QUESTION`, and ends with `discardButtonA(0)`. M5Unified reports `wasSingleClicked()` about 0.5 s after the click the tour already used, and without this the normal `loop()` would start a recording right after "Tour beenden" (same guard as the timer alert at line 9103).

- [ ] **Step 4: Wire it into the sketch**

At the top of `redrawPreviousResult()`: `if (guideActive()) { guideRenderStep(); return; }`. In `setup()`: replace `drawScreen(MAIN_QUESTION)` in the non-deep-sleep branch with `if (!guideAutoStart()) drawScreen(MAIN_QUESTION);`. In `loop()`: add `|| guideActive()` to `btnCIsListButton`. After the timer list block: `if (guideActive()) { guideTick(); delay(20); return; }`. Serial command `guide` starts the tour, for tests.

- [ ] **Step 5: Run `./verify.sh`**

Expected: compiles.

- [ ] **Step 6: Commit**

```bash
git add sketch/sketch_jan12a
git commit -m "Open the tour after setup and ask before anything ends it"
```

### Task 3: The real first question

**Files:**
- Modify: `sketch/sketch_jan12a/guide.ino`
- Modify: `sketch/sketch_jan12a/sketch_jan12a.ino` (`handleVoiceQuery` 8683-8953, `drawProgress` 1542, `drawScreen` 1323)

**Interfaces:**
- Consumes: `guideVoiceResult`, `GuideVoice`, `guideState`.
- Produces: global `GuideVoice lastVoiceOutcome`, `void guideOnHeard(const String& question)`, `void guideDecorateScreen(M5Canvas& c)`.

- [ ] **Step 1: Set `lastVoiceOutcome` in `handleVoiceQuery`**

`GUIDE_VOICE_ANSWERED` at the top. `GUIDE_VOICE_BLOCKED` at the missing-key return, the parental-lock return and the `isOnboarding` branch. `GUIDE_VOICE_CANCELLED` at the cancelled return. `GUIDE_VOICE_NOT_HEARD` at the mic error and "Couldn't hear" returns.

- [ ] **Step 2: Add the hooks**

After a successful transcription and the parental check: `if (guideActive()) guideOnHeard(question);`, which shows the "heard" screen for `GUIDE_HEARD_MS`. In `drawProgress`, above the boxes when `guideActive()`: the "speak" string, centered, 12 pt font. In `drawScreen`, before `captureCanvas`: `if (guideActive() && guideState.step == GUIDE_ANSWER) guideDecorateScreen(canvas);`, which draws `HINT_MENU` at B and `HINT_NEXT` at C.

- [ ] **Step 3: Handle `GUIDE_EV_RECORD` in `guide.ino`**

Call `handleVoiceQuery(false)`, then `discardButtonA(0)`, then `guideVoiceResult(guideState, lastVoiceOutcome)`. On `GUIDE_EV_ABORT` call `guideStop(false)`. If `apMode` is on after the call, also `guideStop(false)` and return. `GUIDE_EV_FINISH_ASK` calls `guideStop(true)`, then `handleVoiceQuery(false)`, then `discardButtonA(0)`. On `GUIDE_VOICE_ANSWERED`, store `guideAnswerRef = hasModel ? modelRefText : ""`. In Answer, `guideRenderStep()` shows the child's result: if `guideAnswerRef` is set and `modelRefText` differs, it runs `parseModelRef` and `fetchModel(guideAnswerRef)` again, because Sky and Music replaced it. If `hasModel`, `modelViewShow()`, otherwise `drawScreen(response)`. If a model is showing, `guideTick` calls `modelViewTick()`. If the first question opened the timer list, `inTimerListMode` takes the buttons first; `guideRenderStep()` doesn't draw while it's on, and leaving the list draws the answer with the tabs through the `drawScreen` hook.

- [ ] **Step 4: Run `./verify.sh`**

Expected: compiles.

- [ ] **Step 5: Commit**

```bash
git add sketch/sketch_jan12a
git commit -m "Let the first question of the tour be a real one"
```

### Task 4: Photo, star map and music

**Files:**
- Create: `scripts/make_guide_moon.sh`, `sketch/sketch_jan12a/guide_moon.h` (generated)
- Modify: `sketch/sketch_jan12a/guide.ino`, `sketch/sketch_jan12a/model_view.ino:85-103`

**Interfaces:**
- Consumes: `fetchModel`, `parseModelRef`, `modelViewShow`, `leaveModelView`, `eduUnloadActive`, `GUIDE_SKY_REF`, `GUIDE_MUSIC_REF`, `detectOrientation`, `guideRotateStep`.
- Produces: `GUIDE_MOON_JPG` and `GUIDE_MOON_JPG_LEN` in `guide_moon.h`; `void guideDrawViewHints(M5Canvas& c)`.

- [ ] **Step 1: Write `scripts/make_guide_moon.sh` and generate the header**

macOS tools only, because this Python has no Pillow: `curl` the Commons original of `File:Moon nearside LRO.jpg`, `sips -Z 96`, `sips --padToHeightWidth 135 240 --padColor 000000 -s format jpeg -s formatOptions 70`, then write a `PROGMEM` byte array with `xxd -i`. The header's first comment names the file, licence and author.

Check: `sips -g pixelWidth -g pixelHeight` gives 240 and 135; the JPEG has no `FFC2` progressive marker; the array is under 16000 bytes.

- [ ] **Step 2: Draw the Photo step**

`canvas.drawJpg(GUIDE_MOON_JPG, GUIDE_MOON_JPG_LEN, 0, 0)`, the hint pill at top right, footer in `DejaVu9`. Each pass in this step, feed `guideRotateStep` with `detectOrientation() == LANDSCAPE`; on true, toggle between picture and the photo text.

- [ ] **Step 3: Open and close Sky and Music**

When the quote ends: `parseModelRef`, `fetchModel`, `currentDisplayMode = SHOW_MODEL`, `modelViewShow()`. Leave `response` alone: it holds the child's answer for "one step back". If it returns false, go on to the next step. `guideTick` calls `modelViewTick()` in these steps while the menu is closed. On leaving: `leaveModelView()`, `hasModel = false`, `eduUnloadActive()`. On menu open in Music, `M5.Speaker.stop(MUSIC_SPEAKER_CHANNEL)`; playback continues by itself once frames run again. Sky keeps its own "turn me" hint.

- [ ] **Step 4: Keep the tabs up in result views**

In `model_view.ino`: `const bool hints = guideActive() || modelHintsShowing(...)`, and at the top of `modelDrawHints`: `if (guideActive()) { guideDrawViewHints(c); return; }`, drawing `HINT_MENU` at B and `HINT_NEXT` at C.

- [ ] **Step 5: Run `./verify.sh`**

Expected: compiles.

- [ ] **Step 6: Commit**

```bash
git add scripts/make_guide_moon.sh sketch/sketch_jan12a
git commit -m "Show a photo, the stars and a song in the tour"
```

### Task 5: The sliding grid

**Files:**
- Create: `sketch/sketch_jan12a/guide_tiles.ino`
- Modify: `sketch/sketch_jan12a/guide.ino`

**Interfaces:**
- Consumes: `GUIDE_TILES`, `guideSlideStep`, `timelineTiltUpdate`, `TimelineTilt`, `TIMELINE_TILT_SIGN`, `gyroToScreen`, `setGyroEnabled`, `updateGyroPower`.
- Produces: `void guideDrawIcon(M5Canvas& c, const char* id, int x, int y)`, `void guideDrawGrid(M5Canvas& c, float pos, int rotation)`.

- [ ] **Step 1: Port the 16 icons from the mock's `ICONS` to M5Canvas primitives in `guide_tiles.ino`**

- [ ] **Step 2: Implement `guideDrawGrid`**

Tile geometry from the mock: x `6 + col * 116`, y `16 + row * 53`, 112x50, radius 5, icon at +4/+3, title at +34 in the 12 pt font, sentence lines from +28 in `DejaVu9` at 10 px spacing, wrapped with `wrapToLines` to 105 px. Page dots at top right, x `176 + p * 16`, y 3, so they clear B's tab when the stick is flipped. Counter `n/4` at bottom right in rotation 3, bottom left in rotation 1, so it clears C's tab.

- [ ] **Step 3: Slide and tilt**

While the step is Grid and the menu is closed: at most 25 frames per second, only while `guideSlideStep` still moves. `setGyroEnabled(true)` on entering, `updateGyroPower()` on leaving. Read the gyro as `modelViewTick` does, and pass `TIMELINE_TILT_SIGN * dps[2]` to `timelineTiltUpdate`. The result moves the target page, stopping at both ends and never leaving the grid.

- [ ] **Step 4: Run `./verify.sh` and `tests/guide/run.sh`**

Expected: compiles, all checks pass.

- [ ] **Step 5: Commit**

```bash
git add sketch/sketch_jan12a
git commit -m "Slide through everything else Zitro can do"
```

### Task 6: Start by voice, documentation and a check on the stick

**Files:**
- Modify: `sketch/sketch_jan12a/ai_provider.h:65`, `sketch/sketch_jan12a/ai_payload.ino:146`, `sketch/sketch_jan12a/sketch_jan12a.ino:4916` and `8781`
- Modify: `USERGUIDE.md`, `REQUIREMENTS.md`, `RELEASE_NOTES.md`

**Interfaces:**
- Consumes: `guideStartNow`.
- Produces: `bool isTour` in the response struct.

- [ ] **Step 1: Add `is_tour`**

Field in `ai_provider.h`, `out.isTour = jsonFlag(doc["is_tour"]);`, the key in the JSON template of the prompt, and a prompt rule: set `is_tour` only for "Was kannst du?", "Zeig mir die Tour", "What can you do?", "Show me the tour" and close variants, never for setup or WiFi requests. In `handleVoiceQuery`, after the `isOnboarding` branch: `if (gptResponse.isTour && !guideActive()) { guideStartNow(); return; }`.

- [ ] **Step 2: Update the documents**

`USERGUIDE.md`: a section "Guided tour" (when it starts, the menu, how to start it again), and the exceptions from the spec's last section next to the button lists. `REQUIREMENTS.md`: flag `tourDone`, flow, button table, sleep rule. `RELEASE_NOTES.md`: a new section dated the day of implementation, with simple end-user bullets.

- [ ] **Step 3: Run `./verify.sh` and all of `tests/*/run.sh`**

Expected: compiles, no test fails.

- [ ] **Step 4: Check on the stick**

Check the port and that no other session is using the stick (see the shared-stick memory). Flash with `./deploy.sh -y`. Serial `guide` starts the tour; serial `ask Was kannst du?` tests the voice start. Check, and report each honestly:

- Full flow in rotation 3 and 1, tabs next to the right buttons.
- No tile title cut off; sentences readable at real size. If they aren't, report it: the spec's fallback is Matthias's call.
- M5 twice during music; three quick ▶; a dark screen press.
- Tilt moves pages the right way; music goes silent in the menu.
- Screenshots of the grid with `scripts/capture_screens.py <port> shot`.

- [ ] **Step 5: Commit**

```bash
git add sketch/sketch_jan12a USERGUIDE.md REQUIREMENTS.md RELEASE_NOTES.md
git commit -m "Start the tour by voice and describe it"
```
