# Physics scenes and geometric solids

Two result kinds, `physics` and `solid`, computed entirely on the device. They need no server file and no network after the AI's answer.

## Requirements

### Common

- The AI only names an id from a closed list, in the answer field `model`: `physics:<id>` or `solid:<id>`. Case, spaces and underscores are ignored; anything else gives no model, and Zitro shows the normal picture.
- Results, numbers and shapes come from textbook equations in `physics_core.h`, `astro_core.h` and `solid_core.h`, and from published values named under Sources. Where something only follows a set schedule, or a scene isn't real time or true to scale, this document says so, and the scene says it on screen in a word: "Zeitlupe" / "slowed", "Zeitraffer" / "time-lapse", "nicht massstabsgetreu" / "not to scale".
- Labels come from built-in tables, in the device language (`settings/language`, read once per lookup). Decimal mark in German is a comma.
- Footer: the scene or solid name and "berechnet" / "computed" (atoms: "PubChem").
- `keepAwake` stays unset: a running animation never holds off the normal sleep timers.
- Working memory comes from PSRAM in `begin()`, is freed in `end()`, and a failed allocation returns false. `fetch()` does no network and allocates nothing.
- Button A: new question, as everywhere.

### Physics (`physics:<id>`)

- `turnsWithGyro` false. Button B steps through the scene's settings and wraps around. Button C goes back to the text in every scene, so nobody gets stuck in one; no scene uses it for steps.
- Button B restarts the run, except the lever, states of matter and seasons, which continue from their current state so the change is visible, and the Sun's path, which keeps its clock. The setting survives Button C and back, but not a new lookup.
- No goals, scores or games: Button B only changes what is shown.
- Two scenes make sound, short tones the scene asks for and the device plays on the speaker's channel 1 at the device volume (never changed): the Doppler scene beeps at the pitch the listener hears, the half-life scene clicks like a Geiger counter. No tone lasts longer than 60 ms, and `end()` stops the speaker channel, so nothing sounds after the view closes.
- Home place: `PhysicsState.homeLatitude` and `homeLongitude` (radians, north and east positive) feed the seasons and the Sun's path. `physicsBegin()` sets them from the home city setting (`clockHomeCity()`, the world clock's table, default Zurich 47.4 degrees north). `PHYSICS_HOME_LATITUDE` and `PHYSICS_HOME_LONGITUDE` remain for the host tests. Both scenes show the latitude they use. South of the equator the seasons scene names the seasons the other way round, and the Sun's path faces north, east on the right.

| Id | Curriculum topic | What it computes | Button B |
| --- | --- | --- | --- |
| `falling` | Gravity and falling | s = g t^2 / 2 in real time, ghosts every 0.2 s, heavy and light ball | Earth, Moon, Mars |
| `friction` | Friction | Sliding to a stop on ice, wood, rubber: d = v^2 / (2 mu g), real time | Push 1, 1.5, 2 m/s |
| `lever` | Levers | Torque balance m1 d1 = m2 d2; beam motion from torques | Five loads against 2 kg at mark 2 |
| `pulley` | Pulleys | F = G / n, rope pulled = n x lift | 1, 2, 4 carrying ropes |
| `buoyancy` | Float or sink | Archimedes: weight, buoyancy, water drag; floats at fraction rho / rho_water | Cork, wood, ice, stone, iron |
| `pendulum` | Pendulum | Full equation theta'' = -(g/L) sin(theta), real time; T = 2 pi sqrt(L/g) | 0.25, 1, 2.25 m |
| `ramp` | Energy swapping | Ball in a valley y = x^2: height and motion energy, sum constant | No friction, with friction (rest becomes heat) |
| `magnet` | Magnets and field lines | Poles as 1/r^2 sources; lines traced from north to south; a compass follows the field | One magnet, attracting pair, repelling pair |
| `shadow` | Light and shadow | Straight light rays, similar triangles; a lamp moving around a stick | Short or tall stick |
| `mirror` | Mirrors | Law of reflection, r = d - 2 (d.n) n; focal point at half the radius | Flat, concave, convex |
| `refraction` | Refraction | Snell's law, total reflection beyond the critical angle | Air to water, glass, diamond; water to air |
| `sound` | Sound waves | Longitudinal wave, air particles and the pressure curve; slowed on screen | Low or high, quiet or loud |
| `circuit` | Simple circuit | Ohm's law, I = U / R, bulb power | Off, on, two batteries, two bulbs in a row |
| `matter` | States of matter | Lennard-Jones particles with velocity Verlet and a thermostat; states emerge from the temperature | Solid, liquid, gas |
| `seasons` | Seasons | Sun's longitude from the date, declination, day length at the home city's latitude | Next season |
| `moon` | Moon phases | Phase from the date, Meeus ch. 48; side view and top view | Today, or the month running |
| `double-pendulum` | Chaos | Full equations of motion for two 25 cm rods with equal weights, Runge-Kutta, real time. A second pendulum starts 0.001 rad further out; the gap between the lower weights is shown | Start at 30, 90, 135, 170 degrees |
| `doppler` | Doppler effect | A 440 Hz siren passing a listener. Wave crests as circles from where they set off; heard pitch f c / (c - v cos a), a taken when the crest set off, and played on the speaker as each crest arrives. Slowed on screen, with the true ratio v / c | Standing, 100 km/h, 300 km/h |
| `double-slit` | Interference | Two waves in step from two slits, added up across the field; bright and dark bands on the screen at the right from 4 cos^2(pi (r1 - r2) / lambda) | Red 650 nm, green 532 nm, blue 450 nm |
| `prism` | Colours of light | Seven colours traced through a 60-degree prism with Snell's law at both faces; the refractive index of each colour from Sellmeier's equation. Shows how far violet and red fan apart | Water, crown glass N-BK7, flint glass N-SF11 |
| `half-life` | Radioactive decay | 256 atoms, each decaying by chance with 1 - 2^(-dt / T) per frame; count and curve against the exact halving; a Geiger click per frame with decays | Barium-137m, radon-222, iodine-131, carbon-14 |
| `planets` | The solar system | Planets at their heliocentric places from JPL's Keplerian elements (Kepler's equation solved), today from the clock; the distance to the Sun of one planet at a time | Inner planets today, all planets today, inner planets running, all running |
| `retrograde` | Planetary loops | Earth and planet on their orbits (same elements), the line of sight, and the planet's path along the ecliptic among the real bright stars around its next opposition | Mars, Jupiter |
| `sunpath` | The Sun's daily path | Height and direction of the Sun over the home horizon through a day, from latitude and declination; the Sun now from the clock (sidereal time, home longitude). Day length and noon height of the chosen day | Today, June solstice, equinoxes, December solstice |
| `satellites` | Orbits | Earth and orbit true to scale; period from Kepler's third law with the Earth's GM (and the Moon's for the Moon); the satellite runs by Kepler's second law; a spot on the equator turns once a sidereal day at the same pace | ISS, GPS, Meteosat (geostationary), the Moon |
| `sizes` | How big things are | Bodies side by side at their true sizes to each other, with the exact ratio of their radii | Earth and Moon, Jupiter and Earth, Sun and Jupiter, Sun and Earth, all planets in a row |
| `atom-<symbol>` | Atom shell model | Electron shells of hydrogen to krypton (Z 1 to 36), from PubChem's configurations. The symbol is matched whole and in any case; English names work too | Next element, label follows |

Chosen the way primary and lower-secondary school teaches physics, for up to 12-year-olds: forces first, then energy, light, sound, electricity, matter, the sky and atoms. The ten scenes from `double-pendulum` on go further, for teenagers: chaos, waves, light's colours, radioactivity and the solar system.

Set by a schedule, with the result computed from it (no equation of motion for these inputs): the pulley's lift, the shadow lamp's path, the swept angles in `mirror` and `refraction`, the siren's steady speed in `doppler`, and the moment the bodies grow in after each press in `sizes` (their sizes to each other stay true while they do).

Chosen rather than looked up: the double pendulum's rods, the Doppler siren's 440 Hz and its speeds, the laser colours (typical laser pointer wavelengths), the seven prism colours, the prism's 60 degrees and the 60 degrees the light comes in at, the satellites' heights and tilts (typical values: ISS about 420 km and 51.6 degrees, GPS 20,200 km and 55 degrees, geostationary 35,786 km over the equator).

Chosen rather than measured, because a real value depends on the object: the lever beam's damping and how far its centre of mass hangs below the pivot, which set how quickly it settles, not where; the wave damping and drag of the floating cube, which set how long it bobs, not how deep it floats (tested).

Known simplifications, none of them on-screen claims:

- `falling`: no air. Over 5 m and for compact balls it hardly matters.
- `magnet`: each pole is a point source (Gilbert's model), good outside the bar.
- `shadow`: a nearby classroom lamp, not the Sun, so similar triangles hold exactly.
- `sound`: particle displacement is hugely exaggerated, and time is slowed. Equal loudness gives equal pressure, so high tones push the air less far, as in reality.
- `circuit`: bulb resistance is taken as constant. The dots are electrons, going from minus to plus: the opposite of the conventional current direction many school books draw. Their pace follows the current, and isn't their true speed.
- `matter`: a model substance in its usual units, not water, with gravity far stronger than real so that a liquid lies on the floor. A sample cooled from gas may freeze with defects, as real ones do. The whole sample's sideways drift is removed each step, standard practice in such simulations.
- `seasons`: day length is geometric (Sun's centre on the horizon, no refraction). A day passes in 8 seconds. Latitude is the home city's, shown on screen.
- `moon`: lit side as seen from the northern hemisphere. No clock yet: the month runs from a known new moon, with no "today".
- `double-pendulum`: point weights on rods without mass, no friction. Chaos means the two runs part after a few seconds from 90 degrees on; after that, the exact path also depends on the rounding of the calculation, as any computed chaotic motion does. What stays true is how fast they part and that energy is kept (tested to a tenth of a percent). Starts over after a minute.
- `doppler`: slowed on screen ("Zeitlupe"): sound runs at 160 pixels a second, and the siren at the same true fraction of the speed of sound (343 m/s). The drawn circles are crests a quarter of a second apart on screen, not every wave. The beep marks each crest's arrival, at the pitch the formula gives for it; between them the device is quiet, so it never sounds like a steady siren. The listener stands still and there is no wind.
- `double-slit`: not to scale and slowed, both said on screen. Light's waves are magnified about a thousand times (650 nm is 8 pixels, under a millimetre on the stick's screen) and the slits are only five red wavelengths apart, where real ones are hundreds. Both slits are taken as points sending equal waves; the waves don't weaken with distance. The colours' proportions are kept: blue's bands lie closer than red's. The field is worked out in 2-pixel blocks.
- `prism`: water drawn as a prism with no glass walls. Crown glass and water spread the colours only a degree or so, which is a few pixels on screen: that is the real difference to flint glass, not a fault of the drawing. Colours on screen only stand for the wavelengths.
- `half-life`: time-lapse ("Zeitraffer"): one half-life takes 6 seconds on screen, for every isotope; the time shown below is the real time that passes. Each atom decays by chance, so the count follows the curve only roughly, as real samples do. Daughter atoms aren't shown. The Geiger counter clicks once a frame at most, so at the start (about 30 decays a second) it clicks a little slower than the decays happen.
- `planets`: positions from JPL's approximate elements, good to a few arcminutes from 1800 to 2050 (tested against JPL Horizons). The inner planets are true to scale; all planets together use the square root of the distance, so Neptune and Mercury both fit: "nicht massstabsgetreu" on screen. Orbits are drawn flat on the ecliptic. Planet dots aren't to scale. The time-lapse runs 10 days (inner) or 120 days (all) a second and starts over when it reaches 2050. Without a clock, it runs as a time-lapse from 1 January 2000, with the month on screen and no "no clock" note (requested 2026-11-08): nothing on screen claims to be today.
- `retrograde`: time-lapse, 20 days a second, from 120 days (Mars) or 180 days (Jupiter) before the next opposition to as long after; an opposition under way counts as the next. Longitude and latitude on the sky strip at the same scale, so the loop has its true, flat shape; Jupiter's is nearly a line. Stars are those brighter than magnitude 4.5 from the star map's catalogue, turned to the ecliptic of J2000. Without a clock, it shows the loop of 2001 (Mars) or 2000 (Jupiter).
- `sunpath`: the Sun's centre, with no refraction: days come out a few minutes shorter than in sunrise tables, which count the Sun's upper edge lifted by the air. The dots on the chosen day's path are hours of sun time, not clock time; no clock times are shown. The view is a chart: direction across, height up, both at the same scale. Near the equator, where the Sun passes almost overhead, the paths there turn steep, as on any such chart. The Sun now moves in real time; without a clock there is no "today".
- `satellites`: the orbit's shape and period, not where the satellite really is today: there is no live data. Time-lapse ("Zeitraffer"), each at its own pace: one trip takes 6 seconds for the ISS, 10 for GPS, 12 for geostationary, 16 for the Moon. Earth and orbits are true to scale with each other; the satellites and the Moon are dots. Heights count from the Earth's equator radius. The Moon's orbit is drawn tilted by 23.4 degrees to the equator; the real tilt swings between 18.3 and 28.6 degrees over 18.6 years. The ISS's and GPS's tilts are shown, the turning of their orbits over weeks is not.
- `sizes`: sizes true to each other within each picture, distances not: the bodies are placed side by side. Mean radii, so Jupiter's and Saturn's flattening isn't drawn. Saturn's main rings run from the inner edge of the C ring to the outer edge of the A ring, drawn tilted.
- `atom-*`: shell model, not to scale. Speeds follow Bohr's 1/n^3 ratio, slowed enormously. Neutrons aren't shown, as they differ by isotope.

### Solids (`solid:<id>`)

- Ids: `tetrahedron`, `cube`, `octahedron`, `dodecahedron`, `icosahedron`, `cuboid`, `triangular-prism`, `pentagonal-prism`, `hexagonal-prism`, `triangular-pyramid`, `square-pyramid`, `pentagonal-pyramid`, `hexagonal-pyramid`, `cylinder`, `cone`, `sphere`, `torus`.
- `turnsWithGyro` true. Button B: zoom, default. Button C: back to the text, default.
- Polyhedra get their faces from the convex hull of exact corner points. Counts come from those logical faces, never from drawn triangles, and are tested against Euler's formula V - E + F = 2 and against known values.
- Curved solids show no counts, because school books differ on them, and draw no lines between their small faces.
- Calculations such as a torus's volume stay with Wolfram: the prompt rule says so.

### Tests

`tests/physics/run.sh` builds with address and undefined-behaviour sanitizers and runs three programs:

- `test_physics.cpp`: the math, against textbook values, US Naval Observatory dates and JPL Horizons positions: planets against Horizons within four times JPL's stated typical errors, Mars's 2020 and 2027 oppositions and its 2020 retrograde stretch, the Sun's height and direction for Switzerland and Sydney, Kepler periods (ISS 93 min, GPS half a sidereal day, geostationary one sidereal day, the Moon 27.3 days), SCHOTT's catalogue indices and Abbe numbers, a hand-worked prism deviation, half-life chances, double pendulum energy and its small-swing mode, and the Doppler pitch against the crests' timing.
- `test_solid.cpp`: counts, Euler, closed surfaces, outward normals, hostile ids.
- `test_render.cpp`: the real drawing code on a stand-in canvas with the real fonts: every scene and setting in both languages, and the seasons and the Sun's path also for homes in Sydney, Singapore and Tromsø. It fails on text that leaves the screen, sits over the footer or over other text, or has no glyph, and on a sound longer than two frames or from a scene that has none. With `ZITRO_RENDER_DIR=<dir>` it writes pictures. It runs only on the host: speed and look on the stick aren't covered.

Not measured on the stick yet: frame rates (the double slit, the Sun's path and the planets do the most per frame), how long the retrograde scene takes to prepare when it opens (about 600 orbit positions in double precision), and how the Doppler beeps and Geiger clicks sound on the buzzer.

## User guide

Ask how something works, and Zitro shows it moving.

- "Wie funktioniert ein Hebel?", "Warum schwimmt Holz?", "Warum gibt es Jahreszeiten?", "Warum knickt ein Stab im Wasser?", "Wie ist ein Kohlenstoffatom aufgebaut?"
- "Was ist Chaos?", "Warum klingt ein Krankenwagen anders, wenn er vorbeifährt?", "Was passiert beim Doppelspalt?", "Warum macht ein Prisma Regenbogenfarben?", "Was ist eine Halbwertszeit?"
- "Wo stehen die Planeten gerade?", "Warum läuft der Mars manchmal rückwärts?", "Warum steht die Sonne im Winter so tief?", "Wie hoch fliegt die ISS?", "Wie gross ist Jupiter im Vergleich zur Erde?"
- For shapes: "Zeig mir einen Torus", "Wie sieht ein Ikosaeder aus?" Just "Pyramide" or "Prisma" shows the square pyramid and the triangular prism.

Questions like "Was ist ein Magnet?" still bring a photo.

**Scenes:** Button B changes something: a heavier weight, a longer pendulum, another planet. Button C shows the written answer, and pressing it again brings the scene back. Button A asks something new.

The Doppler scene beeps and the half-life scene clicks, at your Zitro's volume. Scenes that run faster or slower than real life, or aren't to scale, say so on screen.

**Shapes:** turn the stick to look around the shape. Button B zooms. Shapes with flat sides show how many faces, edges and corners they have. Button C shows the written answer.

## Release note

- Ask how something works and watch it move: levers, pendulums, magnets, light, sound, seasons, moon phases and atoms, plus 3D shapes to turn in your hand, all computed right on your Zitro.
- Ten new scenes: watch a double pendulum turn chaotic, hear a siren's pitch drop as it passes, see light make bands behind a double slit and fan into colours in a prism, listen to atoms decay, find the planets where they are today, follow Mars's loop, trace the Sun's path over your sky, fly with the ISS and compare the sizes of Earth, Jupiter and the Sun.

## Sources and licences

Nothing here is downloaded by the device. Values are facts and equations, built in.

| What | Source | Terms |
| --- | --- | --- |
| Electron configurations, English element names | PubChem periodic table, `pubchem.ncbi.nlm.nih.gov/rest/pug/periodictable/JSON`; generated by `scripts/make_physics_atoms.py` | NCBI/NLM: US government information is public domain; credit asked for. Named in the footer of atom scenes |
| German element names | Checked against Wikipedia, "Liste der chemischen Elemente" | Names only, plain facts; no text copied |
| Moon phase | Meeus, Astronomical Algorithms, 2nd ed., ch. 48: published equation, implemented here | Equations aren't copyrightable; no code copied |
| Sun's position | Astronomical Almanac, low-precision formula | As above |
| Test dates for moon phases and seasons of 2026 | US Naval Observatory, `aa.usno.navy.mil/api`, fetched 2026-11-06; used in tests only | US government data |
| g on Earth, Moon, Mars | Earth 9.81 standard value; Moon 1.62 and Mars 3.73 (mean) from NASA's fact sheets, `nssdc.gsfc.nasa.gov/planetary/factsheet` | US government data |
| Friction | Typical values within published ranges: steel on ice 0.03, wood on wood 0.25-0.62, rubber on concrete 0.6-0.85 (Wikipedia "Friction", citing The Engineering Toolbox) | Facts. Real surfaces vary |
| Densities, refractive indices, speed of sound | Standard physics tables. Wood taken as pine, stone as granite, glass as crown glass | Facts. Natural materials vary |
| Planet positions | E.M. Standish, "Keplerian Elements for Approximate Positions of the Major Planets", JPL, `ssd.jpl.nasa.gov/planets/approx_pos.html`, Table 1 (1800-2050), with its formulae | Published elements; fetched 2026-11-08 |
| Test positions of the planets and the Sun, Mars and Jupiter oppositions | JPL Horizons, `ssd.jpl.nasa.gov/api/horizons.api`, fetched 2026-11-08; used in tests only | US government data |
| Calendar from a Julian day, sidereal time | Meeus, Astronomical Algorithms, 2nd ed., ch. 7 and 12 | Equations |
| Obliquity of the ecliptic (J2000, 84381.412"), mean sidereal day (86164.09054 s), Moon's GM (4902.800118 km3/s2), astronomical unit | JPL, "Astrodynamic Parameters", `ssd.jpl.nasa.gov/astro_par.html` (IAU and DE ephemeris values) | Facts |
| Earth's GM (398600.4418 km3/s2) | IERS Conventions (2010), Table 1.1 (also WGS 84) | Facts |
| Mean radii of the Sun, planets and Moon; Earth's equator radius (6378.137 km); the Moon's distance (384,400 km) and eccentricity (0.0549) | NASA planetary fact sheets, `nssdc.gsfc.nasa.gov/planetary/factsheet` | US government data |
| Saturn's rings (74,658 to 136,780 km) | NASA Saturnian rings fact sheet, `nssdc.gsfc.nasa.gov/planetary/factsheet/satringfact.html` | US government data |
| Bright stars behind the retrograde loop | The star map's own catalogue (Yale Bright Star Catalogue), see `docs/features/sky.md` | As there |
| Refractive index of N-BK7 and N-SF11 | SCHOTT optical glass catalogue (Zemax catalogue 2017-01-20b, glass data sheets), Sellmeier coefficients as given at `refractiveindex.info` | Facts; the database is CC0 |
| Refractive index of water | M. Daimon and A. Masumura, Applied Optics 46, 3811 (2007), Sellmeier fit for 20 C, via `refractiveindex.info` | Facts |
| Half-lives: barium-137m 2.552 min, radon-222 3.8222 d, iodine-131 8.0252 d, carbon-14 5700 a | IAEA Nuclear Data Section, LiveChart API `www-nds.iaea.org/relnsd/v1` (ENSDF evaluations), fetched 2026-11-08. Carbon-14's older "Cambridge" value, 5730 a, is still in many school books | Facts |
| Speed of sound | 343 m/s in air at 20 C, standard physics tables, as in `sound` | Facts |
| Solids | Exact geometry | No source needed |

The firmware adds no third-party code. Fonts are the project's existing ones.
