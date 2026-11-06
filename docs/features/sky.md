# Sky: a star map you turn with your hand

Kind: `sky`. All data is built into the firmware, about 33 KB with its code (see "Flash"). No server files, no network.

## Requirements

- `sky:<constellation>` opens the star map centred on that constellation, with its lines in citrus, its name big at the top left and on the bottom line, and the names of up to 3 of its brightest IAU-named stars next to those stars. 29 of the 88 constellations have no star with an IAU name above the catalogue limit, so they show no star names.
- `sky:all` opens the whole dome, all lines the same, with the names of up to 3 constellations nearest the middle of the picture.
- Queries are a closed list from `sky_data.h`: the 88 IAU names, their English and German names and a few aliases (Great Bear, Big Dipper, Southern Cross, Scorpio, Grosser Wagen...), folded to lower-case letters. "the" and "constellation" are ignored. Anything else, including stars such as `sky:Sirius` or any character outside letters, space, `'`, `.`, `-`, makes `fetch()` return false and Zitro shows the picture.
- The script fails if a key would mean two constellations. d3-celestial calls both Circinus and Pyxis "Compass": neither accepts plain "Compass".
- Label: `DE` shows the German name, `EN` the IAU name. It comes from the table, so a wrong lookup is visible.
- Stars: every Bright Star Catalogue star down to magnitude 5.0 (1630), plus the 35 dimmer ones that constellation lines end on: 1665 in all, 743 line segments. Dot size and brightness by magnitude in 6 steps, colour from B-V in 6 bands kept well towards white. Stars without a B-V value (9 of those down to magnitude 5) are white.
- **The map doesn't point at the real sky, and must never suggest it does.** The stick has no compass and no location. It starts from an arbitrary direction (the constellation asked for; Orion for `sky:all`). No horizon, no ground, no compass marks. For the first 5 seconds after a lookup the screen says it's a star map that doesn't point at the real sky.
- It shows the celestial sphere itself in right ascension and declination (J2000), not the sky at a date and place. Planets, the Moon and the Sun aren't on it.
- Geometry: seen from inside the sphere, with celestial north up at the start, so east is on the left as in the real sky. `tests/sky` checks Orion and Ursa Major aren't mirrored. Gnomonic projection, so the straight lines are correct.
- Turning: `turnsWithGyro` is **false**, on purpose. The shared turning also spins by itself when the hand is still (20 degrees per second), which would carry a constellation away. `skyTurnWithHand` integrates `modelGyroDps` the same way, with the same gain and sign, and stops when the hand stops. Gyro power and the sleep rules don't depend on the flag. The accelerometer isn't used: nothing here needs a true "up".
- If the constellation's centre leaves the picture, a citrus arrow at the edge points the shortest way back.
- Button B: zoom, the default 3 steps (90, 53, 28 degrees across). A lookup starts at the largest step at which the constellation fits, so Crux starts close and Hydra wide. Button C: back to the text. Button A: new question.
- Drawing target 25 frames per second, no allocation, nothing on the heap. Line in the serial log every 5 seconds while the view is open, opened with serial `model sky:all` or `model sky:Orion`. The format, values not measured:
  `skybench fps=<n> drawAvgUs=<n> drawMaxUs=<n> stars=<drawn>/1665 lines=<drawn>/743 zoom=<0-2> sprite=<internal|psram> freeHeap=<n>`
  `fps` includes sending the picture and is capped at 25 by the view. `drawAvgUs`/`drawMaxUs` are the feature's own frame. If `fps` stays below 25, lower `MAG_LIMIT` in the script: the table is sorted brightest first.
- Data is regenerated with `python3 -I scripts/make_sky_data.py <downloads> sketch/sketch_jan12a/sky_data.h`. The header records the SHA-256 of each input. Line points must match a catalogue star within 0.01 degrees or the script fails (largest found: 0.0025).

## User guide

Ask about a constellation or the stars: "Where is Orion?", "What does the Great Bear look like?", "Show me the night sky."

Zitro opens a star map. Turn the stick and look around. The sky stays still while you turn, and stops when you stop.

The constellation you asked for is drawn in yellow. If you lose it, follow the yellow arrow.

- **Button A:** new question
- **Button B:** zoom in, three steps, then back
- **Button C:** back to the written answer

This is a map of all the stars, not a window. The stick has no compass and doesn't know where you are, so it can't point at the real stars above you. It shows the stars as they sit on the whole sky, all year round, without planets or the Moon.

## Release note

- Look around a star map with your hand: ask for Orion or the Great Bear, or the whole night sky, and turn your Zitro to explore 1,600 real stars.

## Sources and licences

| Data | Source | Licence or terms | What we must do |
| --- | --- | --- | --- |
| Star positions, magnitudes, B-V | Bright Star Catalogue, 5th Revised Ed. (Preliminary), Hoffleit & Warren 1991, HEASARC table BSC5P: <https://heasarc.gsfc.nasa.gov/W3Browse/star-catalog/bsc5p.html> | HEASARC's data policy says "HEASARC materials are all available freely for your use" (<https://heasarc.gsfc.nasa.gov/docs/heasarc/data_policy.html>). The table is marked public. No separate licence for the catalogue itself was found, here or in the CDS ReadMe for V/50. | Credit: footer "Yale BSC", and this file. The policy's acknowledgement wording is for research publications. |
| Constellation lines, German and English names | d3-celestial by Olaf Frohn: <https://github.com/ofrohn/d3-celestial>, files `constellations.lines.json`, `constellations.json` | BSD 3-Clause, below. The project says the lines come from the IAU constellation page with some changes by the author, and the translated names from Wikipedia. | Keep the notice below with the firmware's documentation. See note. |
| Star names | IAU Catalog of Star Names (IAU-CSN), WGSN: <https://www.pas.rochester.edu/~emamajek/WGSN/IAU-CSN.txt>, official version <https://www.iau.org/public/themes/naming_stars/> | The file's header: IAU products are released under Creative Commons Attribution, "free to use... as long as the source is mentioned". | Credit: footer "IAU", and this file. |

Not used: HYG. It's CC BY-SA 4.0 (<https://codeberg.org/astronexus/hyg>), and share-alike would have reached further than this needs.

Note: the German names are ordinary single words taken from a list d3-celestial compiled from Wikipedia, whose text is CC BY-SA. A list of 88 names is unlikely to carry that, but we haven't had it checked. They could be replaced by a list written by hand. Corrections to d3's names are in `ENGLISH_NAME` and `GERMAN_NAME` in the script, and all aliases in `ALIASES` were written by hand.

The IAU's page and its copyright statement for the constellation lines couldn't be read at their old address (it now redirects to the IAU's main site), so their terms rest on what d3-celestial states.

### d3-celestial licence

Copyright (c) 2015, Olaf Frohn
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

## Flash

27 KB by symbol size in the built image (tables 22 KB, code 5 KB), plus about 6 KB of name strings: about 33 KB in all. The tables sit in flash (addresses 0x3F4...), not in internal RAM. Static RAM: 152 bytes for the palette and label boxes, plus a few variables.

## Not tested

Nothing has run on the stick: drawing speed, how it feels to turn, whether the sign of each axis is right (`SKY_TURN_SIGN`), how dim stars and lines look on the real screen (`SKY_COLOR_LINE`, `skyStarColor565`), and whether labels fit (font widths). Host tests cover data, lookup, geometry and appearance values; previews of Orion, Ursa Major and Crux were rendered on the host without text.
