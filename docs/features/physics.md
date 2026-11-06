# Physics scenes and geometric solids

Two result kinds, `physics` and `solid`, computed entirely on the device. They need no server file and no network after the AI's answer.

## Requirements

### Common

- The AI only names an id from a closed list, in the answer field `model`: `physics:<id>` or `solid:<id>`. Case, spaces and underscores are ignored; anything else gives no model, and Zitro shows the normal picture.
- Motion, numbers and shapes come from textbook equations in `physics_core.h` and `solid_core.h`. No drawing is hand-animated. Where a scene isn't real time or true to scale, this document says so, and the sound scene says "Zeitlupe" on screen.
- Labels come from built-in tables, in the device language (`settings/language`, read once per lookup). Decimal mark in German is a comma.
- Footer: the scene or solid name and "berechnet" / "computed" (atoms: "PubChem").
- `keepAwake` stays unset: a running animation never holds off the normal sleep timers.
- Working memory comes from PSRAM in `begin()`, is freed in `end()`, and a failed allocation returns false. `fetch()` does no network and allocates nothing.
- Button A: new question, as everywhere.

### Physics (`physics:<id>`)

- `turnsWithGyro` false. Button B steps through the scene's settings and wraps around. Button C goes back to the text in every scene, so nobody gets stuck in one; no scene uses it for steps.
- Button B restarts the run, except the lever, states of matter and seasons, which continue from their current state so the change is visible. The setting survives Button C and back, but not a new lookup.

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
| `seasons` | Seasons | Sun's longitude from the date, declination, day length at 47 degrees north | Next season |
| `moon` | Moon phases | Phase from the date, Meeus ch. 48; side view and top view | Today, or the month running |
| `atom-<symbol>` | Atom shell model | Electron shells of hydrogen to krypton (Z 1 to 36), from PubChem's configurations. The symbol is matched whole and in any case; English names work too | Next element, label follows |

Chosen the way primary and lower-secondary school teaches physics, for up to 12-year-olds: forces first, then energy, light, sound, electricity, matter, the sky and atoms.

Known simplifications, none of them on-screen claims:

- `falling`: no air. Over 5 m and for compact balls it hardly matters.
- `magnet`: each pole is a point source (Gilbert's model), good outside the bar.
- `shadow`: a nearby classroom lamp, not the Sun, so similar triangles hold exactly.
- `sound`: particle displacement is hugely exaggerated, and time is slowed. Equal loudness gives equal pressure, so high tones push the air less far, as in reality.
- `circuit`: bulb resistance is taken as constant. Dots show the way and strength of the current, not the electrons' true speed.
- `matter`: a model substance in its usual units, not water, with gravity far stronger than real so that a liquid lies on the floor. A sample cooled from gas may freeze with defects, as real ones do. The whole sample's sideways drift is removed each step, standard practice in such simulations.
- `seasons`: day length is geometric (Sun's centre on the horizon, no refraction). A day passes in 8 seconds. Latitude is fixed at 47 degrees north, shown on screen.
- `moon`: lit side as seen from the northern hemisphere. No clock yet: the month runs from a known new moon, with no "today".
- `atom-*`: shell model, not to scale. Speeds follow Bohr's 1/n^3 ratio, slowed enormously. Neutrons aren't shown, as they differ by isotope.

### Solids (`solid:<id>`)

- Ids: `tetrahedron`, `cube`, `octahedron`, `dodecahedron`, `icosahedron`, `cuboid`, `triangular-prism`, `pentagonal-prism`, `hexagonal-prism`, `triangular-pyramid`, `square-pyramid`, `pentagonal-pyramid`, `hexagonal-pyramid`, `cylinder`, `cone`, `sphere`, `torus`.
- `turnsWithGyro` true. Button B: zoom, default. Button C: back to the text, default.
- Polyhedra get their faces from the convex hull of exact corner points. Counts come from those logical faces, never from drawn triangles, and are tested against Euler's formula V - E + F = 2 and against known values.
- Curved solids show no counts, because school books differ on them, and draw no lines between their small faces.
- Calculations such as a torus's volume stay with Wolfram: the prompt rule says so.

### Tests

`tests/physics/run.sh` builds with address and undefined-behaviour sanitizers and runs three programs:

- `test_physics.cpp`: the math, against textbook values and US Naval Observatory dates.
- `test_solid.cpp`: counts, Euler, closed surfaces, outward normals, hostile ids.
- `test_render.cpp`: the real drawing code on a stand-in canvas with the real fonts: every scene and setting in both languages. It fails on text that leaves the screen, sits over the footer, or has no glyph. With `ZITRO_RENDER_DIR=<dir>` it writes pictures. It runs only on the host: speed and look on the stick aren't covered.

## User guide

Ask how something works, and Zitro shows it moving.

- "Wie funktioniert ein Hebel?", "Warum schwimmt Holz?", "Warum gibt es Jahreszeiten?", "Warum knickt ein Stab im Wasser?", "Wie ist ein Kohlenstoffatom aufgebaut?"
- For shapes: "Zeig mir einen Torus", "Wie sieht ein Ikosaeder aus?"

Questions like "Was ist ein Magnet?" still bring a photo.

**Scenes:** Button B changes something: a heavier weight, a longer pendulum, another planet. Button C shows the written answer, and pressing it again brings the scene back. Button A asks something new.

**Shapes:** turn the stick to look around the shape. Button B zooms. Shapes with flat sides show how many faces, edges and corners they have. Button C shows the written answer.

## Release note

- Ask how something works and watch it move: levers, pendulums, magnets, light, sound, seasons, moon phases and atoms, plus 3D shapes to turn in your hand, all computed right on your Zitro.

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
| Solids | Exact geometry | No source needed |

The firmware adds no third-party code. Fonts are the project's existing ones.
