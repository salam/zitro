# Function graphs

Sharp graphs drawn for this screen, replacing tiny shrunken Wolfram pictures for plain function plots. Kind: `graph`.

## Requirements

### Answer field

- `model` = `graph:<expression in x>`, for example `graph:sin(x)` or `graph:x^2-3`. The AI supplies only the expression. Every point is computed on the device.
- Calculations (integrals, derivatives, solving, numeric results) stay with Wolfram.
- Invalid input, or nothing to draw near the origin (`sqrt(-1)`, `1/0`, `sqrt(x-1000)`): `fetch` returns false and Zitro shows the normal picture.

### Expression language

| Part | Accepted |
| --- | --- |
| Operators | `+ - * / ^`, unary minus and plus, parentheses |
| Functions | `sin cos tan sqrt abs exp ln log`. Parentheses required: `sin x` is rejected. Angles in radians. |
| `log` | Base 10, as taught in school. `ln` is the natural logarithm. Wolfram reads `log` as natural: the prompt rule states the base. |
| Constants | `pi`, `e` |
| Numbers | Digits with at most one point, at most 15 digits. No exponent form: `1e3` is rejected rather than read as 1 times e times 3 or as 1000. |
| Implicit multiplication | `2x`, `2sin(x)`, `2(x+1)`, `(x+1)(x-1)`, `x sin(x)`, `2pi`. A number after a value is rejected (`x2`, `2 3`, `(x+1)2`). |
| Prefix | A leading `y=` or `f(x)=` is ignored. Other equations are rejected. |
| Case and spaces | Upper case is read as lower case. Spaces are ignored, except inside names and numbers. |

Reading rules: `^` binds tighter than a sign and groups from the right (`-x^2` is `-(x^2)`, `2^3^2` is 512). Implicit multiplication has the same rank as `*` and `/`, left to right, so `1/2x` is `(1/2)*x`, as Wolfram reads it. The label shows that as `1/2*x`.

A constant such as `graph:3` is valid: a flat line is a true graph.

### Robustness

- The parser is iterative with fixed-size stacks and no allocation. Limits: 63 characters, 48 program steps, 16 constants, 16 values on the stack, 24 open operators and parentheses. Over a limit means invalid.
- Non-finite results (division by zero, `sqrt` or `ln` out of domain, overflow) mean no point in that column. Values beyond 1e30 count as no point.
- Negative base with a fractional power has no point: `x^(1/3)` is empty for negative x.

### Drawing

- 240 columns, one sample per column, `double` evaluation, stored as `float`. A plot of 117 rows above the footer.
- Axes at x = 0 and y = 0, or pinned to the plot edge when 0 is out of sight. Dim grid, tick marks and numbers at round steps of 1, 2 or 5 times a power of ten (at most about 5 on x, 4 on y). Curve in citrus (#f2b705), 2 px wide.
- Discontinuities: a segment jumping more than 6 px is checked by bisection (12 halvings towards the larger change). On a continuous curve the change shrinks with the interval, across a jump it stays, across a pole it grows. Broken segments are not drawn, so `tan(x)`, `1/x` and `x/abs(x)` draw no line across the screen. The result is kept per segment and doesn't depend on the y range.
- Default range: x from -5 to 5. If under 60 % of columns have a point there, the view centres on the part that does (`sqrt(x)`, `ln(x)`). If there are none, it looks in +-20 and +-100.
- y fits all of the visible curve with an 8 % margin. When the full range is over 8 times the middle 80 % of values, as with poles, the top and bottom 3 % go off screen, so `tan(x)` isn't flattened. Flat lines get a range of their own. The range follows smoothly and ignores changes below 15 %.
- A white dot marks the curve at the middle of the screen. Its exact values are shown at the top as `x=... y=...`, with `y=--` where there's no point.
- Footer: the expression as understood (no spaces, lower case, `*` written out where the reading isn't obvious) and source "Zitro". All ASCII: `sqrt(x)`, `x^2`.

### Moving and buttons

| Input | In the graph |
| --- | --- |
| Tilt the stick and hold (`modelGyroDps[1]`, or `[2]` when larger; below 4 degrees per second ignored) | Slides along the curve under the dot, for as long as it's held. Beyond 6 degrees: 5 columns per second per degree, at most 240. Back to level stops it. Stops at x = +-1000. `turnsWithGyro` is false. |
| Button A | New question |
| Button B click | Zoom (default handler, `modelZoomStep`): x +-5, then +-1, then +-20, then back to +-5, which also returns to the starting place |
| Button C click | Back to the text answer, and from there back to the same place in the graph (default handler) |

Button use: the first plan had Button C zoom in and Button B zoom out, but Button C is the only way back to the text in this view, and the interface offers no hold on it. So Button B does all the zooming and Button C stays as in every other view.

The gyro measures turning speed, not tilt angle, so the tilt is added up from how the stick was held when the view opened. Mapping turning directly to position was rejected: turning back would undo the movement, so you could only peek, never travel. A held tilt fades (8 s time constant, up to 60 degrees), so a return too slow to measure can't slide forever: a long hold slows down and ends. The way back also takes off what has faded (forgotten over about 2 minutes), so going level after a normal hold doesn't slide backwards. After a return too slow to measure, the next tilt the other way starts late by up to the faded amount. The direction constant `GRAPH_TILT_SIGN` was worked out from the molecule view's turning sign and is not yet tried on a stick.

### Frame time

`frame()` never blocks on computing: samples get at most 15 ms per frame, then the rest waits for the next frame, growing from the middle outwards. Panning keeps what's still on screen and computes only new columns. Break checks share the same budget and run once per segment. A single evaluation is not interruptible; the worst case is a chain of about 30 powers in one expression.

### Memory and flash

No network. Sample buffer of about 2.2 KB in PSRAM between `begin` and `end`. About 370 bytes of static data. Flash: 23 KB more than the build without this feature.

### Prompt

The feature's `promptRule` assumes plain function plots go to `graph` with `is_wolfram` false. The main prompt needs two changes at merge:

1. Replace `Berechnungen und Kurven ('Molmasse von Koffein', 'Visualisiere sin(x)') bleiben is_wolfram mit leerem model.` with `Berechnungen ('Molmasse von Koffein', 'Integral von sin(x)') bleiben is_wolfram mit leerem model.`
2. In the `create_image` clause, replace `'erstelle ein Bild')` with `'erstelle ein Bild'; nicht bei Funktionskurven wie 'Zeichne x^2', siehe GRAPHEN)`. Otherwise "Zeichne x^2" becomes a painting, because `create_image` wins over `model`.

### Not tested on hardware

Host tests cover the logic, and `verify.sh` covers compiling. Direction and feel of tilting, frame rate, and the look on the real screen need a stick.

## User guide

Ask Zitro to show a curve, for example "Visualisiere sin(x)", "Zeichne x^2 minus 3" or "Wie sieht die Kurve von 1 durch x aus?". Zitro draws the graph itself and calculates every point exactly.

- **Tilt the stick** left or right and hold it to slide along the curve. Hold it level again to stop. The top line shows x and y at the white dot.
- **Button B** zooms: closer, then far away, then back to where you started.
- **Button C** shows the text answer. Press it again to return to the graph.
- **Button A** asks a new question.

Where a curve has no value, the line stops. That's correct: `1/x` has no value at 0, so there's a gap.

Questions like "Was ist das Integral von sin(x)?" or "Loese x^2=4" still go to Wolfram Alpha.

## Release note

- Ask for a curve like sin(x) or x^2 and watch Zitro draw it sharp on its own screen, then tilt the stick to travel along it and read the exact values.

## Sources and licences

No external data. Everything is computed on the device by code in this repository (`graph_core.h`, `feature_graph.ino`) with the C standard maths library that comes with the ESP32 toolchain. No licence conditions to record. Nothing to upload.
