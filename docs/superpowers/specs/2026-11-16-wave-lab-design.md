# Wave lab ("Wellenlabor"): play with waves and hear them

Date: 2026-11-16. Branch `feature/wave-lab`, from origin/main 5696d18.

## Intent

Matthias: "The wave visualization oscillator should have the option to toggle between more complex waves and patterns and to activate their sounds." Asked what a child should take away: "They should be able to play with waves and sounds related to it. Audio and waves, and wave lengths, etc."

So this is a toy for free play, not a lesson with steps. A child tilts the stick and hears and sees, at the same moment, that a shorter wave is a higher tone, that a taller wave is louder, and that a wave's shape is the sound's colour. Audience as for all of Zitro: children who are starting to read, up to teenagers. The wavelength and the patterns (beat, octave, fifth) are for the older ones.

## Approaches

Laid out to Matthias on 2026-11-16; he picked **B**.

- **A. More settings in the existing Sound scene.** B steps on past today's four settings to triangle, square, sawtooth and a beat, each with its wavelength. Least work, but only a fixed list to step through: little to play with.
- **B. A new scene, the wave lab, played by tilting (picked).** Tilt left or right slides the pitch, and the wave stretches or squeezes while you hear it; tilt toward or away sets the loudness; B steps through the shapes and patterns. The Sound scene stays as it is. Costs a new scene and an AI rule, and a held, sliding tone on the speaker is new to Zitro.
- **C. Turn the Sound scene into the lab.** One scene only, but the air particles and the pressure curve, which explain how sound travels, get crowded out.

## Goals

- A new physics scene `wave-lab`, "Wellenlabor" / "Wave lab", reached by asking (the AI picks it, as for every scene).
- Tilting changes the pitch smoothly; the wave on screen shows its true wavelength in air while you listen.
- Seven shapes and patterns on Button B: sine, triangle, square, sawtooth, octave, fifth, beat.
- Sound is off when the lab opens and is switched on and off by the child.
- Everything shown is computed and true, or said on screen to be slowed.

## Non-goals

- No change to the existing Sound scene (`sound`).
- No games, scores or tasks: the lab only shows and sounds (the physics rule "no goals, scores or games" stands).
- No microphone, no recording, no singing along.
- No harmonic spectrum or bar display of overtones (a later idea).
- No instrument names ("sounds like a flute"): those would be claims without a source.
- Not added to the shake surprise, nor to the home menu's question list (ai-stick-ef owns that list; can be added later).
- No change to the device volume setting.

## Anti-goals (a failure even if it works)

- A sound that goes on after the lab closes, after the screen goes dark, or that starts by itself.
- Clicks, stutter or crackle while sliding that drown the tone: the child would hear the fault, not the wave.
- A number on screen that isn't true (wrong wavelength, a "real" pace that is slowed).
- The StickS3's hum coming back after the lab was used.
- A child stuck in the lab, or a press that wipes the WiFi more easily than today.

## Rule changes (approving this document confirms them)

CLAUDE.md asks for confirmation before documented behaviour changes. These change:

1. `docs/features/physics.md:22`: "Two scenes make sound ... No tone lasts longer than 60 ms" becomes: three scenes make sound. Doppler and half-life keep their short tones. The wave lab holds a tone for as long as its sound is on, built from short queued pieces so it never outlasts its frames by more than about 0.2 s.
2. `feature_physics.ino:94` and physics.md: "Button C goes back to the text in every scene" gets one exception: in the wave lab, Button C (the Plus2's C, the StickS3's double press of B) opens the lab's menu, and the way back to the text is the menu row "Text".
3. `REQUIREMENTS.md:114` (shake): "except the two that make sound" becomes "except the three that make sound". The lab stays out of `SHAKE_MEDIA`.
4. `USERGUIDE.md:442`: "The Doppler and half-life scenes make sound" gets the wave lab added, with its buttons.
5. `tests/physics/test_render.cpp`: the check "a sound longer than two frames or from a scene that has none" stays for every other scene. For the lab it checks the new held-sound state instead (below).
6. `PHYSICS_PROMPT_RULE` gets `wave-lab` and examples that split it from `sound`. The test that every id in the rule is in `PHYSICS_SCENES` and the other way round keeps passing.
7. `docs/features/physics.md:13`: "`keepAwake` stays unset: a running animation never holds off the normal sleep timers" gets one exception: the wave lab with its sound on, within the 3D view's 5-minute cap (below).

## What the child sees and does

```
+--------------------------------------------+
| ~~~~/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\~~~~~~~~ |  the wave, moving right, true wavelength
|        |<- λ ->|                           |  one wavelength marked
| 440 Hz      λ 78 cm      ≈ A4              |
| Rechteck              Zeitlupe        [🔊] |  shape name, "slowed", sound on/off
| Wellenlabor - berechnet                    |  footer, as every scene
+--------------------------------------------+
```

- **Tilt left or right** (the stick turns in the plane of its screen): the pitch slides from 220 Hz to 880 Hz, two octaves. Held level, it's 440 Hz. The pitch changes evenly per octave: about 40 degrees of tilt to an octave, so 40 degrees left is 220 Hz and 40 degrees right is 880 Hz, clamped beyond.
- **Tilt toward or away** (the top edge comes closer or goes further): the loudness. However the child holds the stick when the lab opens is the middle; the sound switched on re-centres it. Top edge away from the child is louder, toward the child quieter, about 30 degrees either way from the middle to full or to quiet, clamped beyond. Quiet is a fifth of full, never silent. The wave's height follows, and so does the sound, within the device volume. Claude's call: a fixed middle (say, screen upright) would put a child holding the stick flat at one end, and 30 degrees keeps the screen readable.
- **Button B**: the next shape or pattern, in this order: Sinus, Dreieck, Rechteck, Sägezahn, Oktave, Quinte, Schwebung (Sine, Triangle, Square, Sawtooth, Octave, Fifth, Beat). The sound stays on or off as it was, so the child hears the shapes change. Pitch and loudness keep following the tilt.
- **The menu** (Matthias, 2026-11-17: "C is not available on S3. Instead, use the menu to toggle sound on/off, etc."): holding B opens it, on both sticks. Until the button module lands, the Plus2's C and the StickS3's double press of B (today's "C") open it too. Rows: "Ton an" or "Ton aus" (preselected), "Text" (back to the written answer), "Schliessen" (back to the lab). It looks and presses like ai-stick-ef's home menu: each press of B = next row, A = choose, holding B = close; holding B never resets the WiFi while it opens or closes the menu (`buttonBHeldForMenu`). The lab keeps running underneath: sound and tilt carry on while the menu is open, so "Ton an" is heard at once.
- **B's tab** shows ▼ (next shape). No C tab on either stick (ai-stick-0a, 2026-11-17: under the button module, a C tab kind would make one press of B open the menu).
- **Button A**: a new question, as always; the lab closes and goes quiet.

On screen, always:

- The wave across the screen, moving to the right, slowed ("Zeitlupe", as in the Sound scene). The scale is fixed and true: 60 pixels to a metre, the Sound scene's scale. So 220 Hz shows a 1.56 m wave (94 px) and 880 Hz a 39 cm one (23 px). A wavelength is marked with a bracket.
- Frequency in Hz, wavelength in cm or m (computed for air at 20 °C, 343 m/s, as the Sound scene does), and the nearest note name with "≈" and an octave number ("≈ A4"). The names are the music view's (`musicNoteName()` in music_core.h): in German H for B natural, and the black keys as sharps, Cis Dis Fis Gis Ais; in English C# D# F# G# A#. So 494 Hz reads "≈ H4" in German and "≈ B4" in English, as the music view on the same stick would say. The "≈" keeps it honest while the pitch slides between notes; the fonts have no "≈", so it is drawn.
- The shape or pattern name. For two-tone patterns, the sum is drawn bold and the two tones thin underneath. The values shown are for the lower tone, with the other on the right: "+ 880 Hz", "+ 660 Hz", and for the beat how far apart the two are, "+ 4 Hz" (the pattern's name is on the row below; changed while building, 2026-11-17).
- Sound on or off, as a speaker symbol on screen.

## The shapes and patterns

| B | Name (de / en) | Heard | Drawn |
| --- | --- | --- | --- |
| 1 | Sinus / Sine | pure tone at f | sine |
| 2 | Dreieck / Triangle | triangle at f | triangle |
| 3 | Rechteck / Square | square at f | square |
| 4 | Sägezahn / Sawtooth | sawtooth at f | sawtooth |
| 5 | Oktave / Octave | sines at f and 2f | both thin, sum bold |
| 6 | Quinte / Fifth | sines at f and 3f/2 | both thin, sum bold |
| 7 | Schwebung / Beat | sines at f and f + 4 Hz | sum bold, swelling and fading 4 times a second |

## Sound

- **Starts off** every time the lab opens, also after "Text" and back in again, and after a wake. Claude's call: a scene the AI opened must not start a held tone by itself, at school or at night.
- **A held tone made of short pieces.** Today a scene can only ask for one short tone per frame (`toneHz`, `toneMs`, at most 80 ms, checked by test_render). The lab needs a new "wanted sound" in `PhysicsState`: on or off, frequency of each of up to two tones, the wave table, and the loudness. The device (feature_physics.ino) turns that into sound. Intended mechanism, to be confirmed on the stick: queue short pieces of whole wave cycles with `M5.Speaker.tone(hz, ms, channel, false, table, len)`, about 40 to 60 ms each, keeping two queued (`isPlaying(channel) < 2`). Each piece is whole cycles of the table, so the next starts where the last ended: no click at the joins, and a pitch change takes effect at the next piece. If frames stop, the sound stops by itself within about two pieces. **Built differently (2026-11-23):** on the stick the queued pieces clicked when tilting a little, because frames come 40 to 60 ms apart and the queue ran dry between them. Each frame now sends one 200 ms piece with `stop_current_sound`, which takes over where the playing wave stands as long as the table stays the same; a new shape lets the old one finish its cycle first (see `docs/features/physics.md`). Both M5Unified 0.2.12 (Plus2) and 0.2.29 (StickS3) have this `tone()` overload and `setChannelVolume()`.
- **Wave tables** are static and constant, because the speaker reads them while it plays (it keeps the pointer). One table per shape. Two-tone patterns play each tone on its own channel: the lower tone on channel 1 (as the other physics sounds and music), the upper on channel 2 (unused today). Reading aloud and alerts stay on channel 0.
- **Loudness** by `setChannelVolume()` on channels 1 and 2. `end()` puts both back to what they were, because music uses channel 1 too.
- **The device volume** is never changed, as for the other physics sounds. If it is set to silent, the lab is silent too. The speaker symbol shows what you hear: 0 to 3 sound waves from the device volume times the loudness, none with the sound off or the volume at 0 (Matthias, 2026-11-22: "Speaker should reflect the volume"; this settles the earlier contradiction with "a lit speaker always means a sound").
- **Every way out goes quiet at once**: the menu row "Ton aus"; the view closing (`end()`: Button A, the menu row "Text", a timer alert, anything that leaves the view); the screen going dark; light sleep. `end()` stops channels 1 and 2. While the screen is dark, the lab asks for no sound, and after a wake the sound is off.
- **Staying awake**: while the sound is on and the stick was tilted in the last 30 s, `keepAwake()` holds off the sleep timers (approved 2026-11-05: playback and motion count as activity). The 3D view's hard cap applies, as for all motion: 5 minutes after the last button press, motion no longer counts (`MODEL_MOTION_CAP_MS`, from `modelWindowStart`), and the lab's sound switches itself off, as music pauses itself after 5 minutes. Then the normal sleep timers run, and the dark screen ends the view's sound in any case. So a stick put down, or left in a moving pocket, with the sound on goes quiet and sleeps.
- **The StickS3's hum**: `speakerOffWhenQuiet()` looks at `M5.Speaker.isPlaying()`, so a held tone is not cut after 3 s, and the speaker stops 3 s after the lab goes quiet, as after any sound.
- **The Plus2's buzzer** most likely makes the shapes, the two-tone patterns and maybe the loudness sound much alike; pitch comes through. Not measured. The user guide doesn't promise more than the stick does.

## Known simplifications (go into physics.md)

- The wave moves slowed on screen ("Zeitlupe"): at the Sound scene's 94 pixels a second for every tone, as sound's speed doesn't depend on pitch. The sound itself is real time.
- Wavelengths are true to scale (60 px to a metre), for air at 20 °C.
- Beat: the two tones' wavelengths differ by about 1 % at 4 Hz apart, which would take 86 m of air to show a whole swell. In a 4 m stretch, the real pressure looks like one wave whose height swells and fades with time. That is what is drawn, and its swelling keeps the real pace you hear (4 times a second), while the wave itself moves slowed. The screen says "Zeitlupe" for the moving wave only.
- Shapes are drawn as they are played: built from their first 9 harmonics, so the square shows small ripples where a textbook draws sharp corners (changed while building, 2026-11-17: what you see is what you hear). Matthias, 2026-11-22: "let me look if I like and adapt later": decided on the stick.
- Tilt in the plane of the screen sets the pitch; how the stick is held otherwise doesn't matter. The pitch is not a property of the stick's angle in physics: it is a control, like a knob.

## The AI rule

Added to `PHYSICS_PROMPT_RULE`, with examples that split it from `sound`:

- `sound`: "Was ist Schall?", "Wie kommt ein Ton zum Ohr?"
- `wave-lab`: "Was ist eine Wellenlänge?", "Warum klingen Töne verschieden?", "Ich will mit Tönen spielen", "Was ist eine Schwebung?", "Was ist eine Oktave?"

## Buttons and the coming button module

The button-ui module (ai-stick-0a, approved 2026-11-15, merges last in ai-stick-ef's queue) will make B once = forward and B twice or held = one shared menu on both sticks, with Button C on the Plus2 doing nothing. Main has no menu in views yet, only the home menu (PR #51). So the lab brings its own small menu now, built from the home menu's parts (`menuPressRead()`, `homeFirstVisible()`, the same row drawing), with its rows given by the scene. When the module lands, its shared menu takes the lab's rows ("Ton an/aus" on top, then the module's own) and the lab's menu goes. ai-stick-0a and ai-stick-ef are told before building.

Holding B for the menu is guarded like the home menu's, so it never runs on into the 3-second WiFi reset.

## Tests

Host (`tests/physics`):

- Math: tilt to frequency (level = 440 Hz, 220 and 880 at the ends, clamped, rising evenly per octave); tilt to loudness (middle at the start angle, a fifth and full at 30 degrees either way, clamped); wavelength = 343 / f; the note name for 440 (A4), 261.6 (C4), 494 (H4 / B4), 466 (Ais4 / A#4) and a value between two notes; the patterns' ratios exactly 2, 3/2 and +4 Hz; the sound switching itself off at the 5-minute cap.
- Wave tables: each has the right harmonics (square: odd only, 1/n; sawtooth: all, 1/n; triangle: odd, 1/n², alternating), no DC offset, and spans whole cycles.
- Render (`test_render.cpp`): every pattern, in both languages, at the lowest, level and highest tilt, with sound on and off: text on screen, not over the footer or other text, every glyph present. The wanted sound: off when sound is off; when on, frequencies within 220 to 1760 Hz, at most two tones, only from the lab. Every other scene: the existing short-tone checks unchanged.
- Prompt rule ↔ `PHYSICS_SCENES` test passes with the new id.

Device:

- `./verify.sh` for both boards.
- Hand test on the StickS3 and on the Plus2 (shared stick: coordinate through ai-stick-ef first): is the tilt range comfortable, does a slide sound smooth or stepped, are the shapes and patterns told apart by ear, does every way out go quiet at once, is the hum gone 3 s after, frame rate.

## Documents to update

`docs/features/physics.md` (table row, sound rule, button exception, simplifications, tests, user guide and release note sections), `REQUIREMENTS.md` (shake wording; scene count where stated), `USERGUIDE.md` (examples, the sound line, the lab's buttons), `RELEASE_NOTES.md`, the comment in `shake_core.h`.

## Decided by Matthias

- Option B: a new scene, played by tilting, the Sound scene kept (2026-11-16).
- Purpose: free play with waves, sound and wavelength (2026-11-16).
- Sound can be switched on (original request).
- Sound on/off and the way back are menu rows, not Button C: "C is not available on S3. Instead, use the menu to toggle sound on/off, etc." Tilt for pitch range and loudness: "perfect". Then: "Go go go" (2026-11-17), taken as approval of this document with that change, including the rule changes listed above.

## Answered by the project

- German note names follow the music view (H, Ais), not English letters.
- The 5-minute cap on motion as activity, approved with the 3D view on 2026-11-05, applies to the lab's sound too.

## Claude's calls (correct any)

1. The menu's rows: Ton an/aus (preselected), Text, Schliessen; it opens by holding B, and by today's C until the button module lands.
2. Sound off at every opening.
3. Range 220 to 880 Hz, 440 Hz when level, about 40 degrees of tilt to an octave. The tiny speakers are weak below about 200 Hz, and the Sound scene uses 220 and 880.
4. The seven shapes and patterns and their order; beat at +4 Hz (clear swelling, slow enough to count).
5. The note name with "≈" and an octave number.
6. Keep-awake only with tilting in the last 30 s, and the sound off by itself at the 5-minute cap.
7. Not in the shake surprise, not in the home menu list.
8. No overtone bars in this version.
9. Loudness: the start angle is the middle, top edge away is louder, 30 degrees to either end, quiet is a fifth of full and never silent.

## Left to the builder

- Table length and sample depth (aliasing of square and sawtooth at 880 Hz is the thing to listen for).
- Piece length (40 to 60 ms) and how many are queued.
- Tilt smoothing so that the pitch doesn't wobble with a steady hand; which accelerometer axes, read in screen coordinates (`gyroToScreen`, as `modelViewTick` does). Orientation, roll and shake gestures are already off in the 3D/scene view (`SHOW_MODEL`).
- Exact layout within the sketch above, as long as `test_render` passes.
- How the dark screen and light sleep reach the lab (a check in `physicsFrame`, or frames stopping, which then lets the queued pieces run out).

## Open risks

- Whether queued pieces join without a click on both speakers is unmeasured. If they don't, the fallback is to change pitch only in semitone steps and restart the tone (`stop_current_sound`) only when the step changes: that still slides, in steps like a keyboard.
- Frame rate with two curves and the sum drawn every frame, on the Plus2.
- The Plus2 buzzer may make the patterns indistinguishable.
