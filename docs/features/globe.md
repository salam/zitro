# Globe: where a place is on Earth

Kind `globe`. Code: `feature_globe.ino`, `globe_core.h`, `globe_wiki.h`, `globe_coast_data.h` (generated). Tests: `tests/globe/run.sh`. Data: `scripts/make_globe_data.py`, `webpage2/data/globe/`.

## Requirements

- `model` = `globe:<English Wikipedia article title>`, only for where-is questions about a place on Earth. "Wo ist das Matterhorn?" gives `globe:Matterhorn`. "Wie sieht das Matterhorn aus?" stays a photo.
- The AI names the article and nothing else. The pin comes from `https://en.wikipedia.org/w/api.php?action=query&prop=coordinates|langlinks&lllang=de&format=json&formatversion=2&redirects=1&titles=...`. Redirects are followed, so "Eiffel tower" finds "Eiffel Tower".
- `fetch()` is true only for exactly one page in the article namespace, not missing or invalid, with at least one coordinate, where every coordinate's `globe` is exactly `earth`, `lat` and `lon` are numbers, |lat| <= 90 and |lon| <= 180. Otherwise false, and the picture shows. Tested with real replies: Mars, Photosynthesis, Atlantis (no coordinates), Olympus Mons (`mars`), Tycho and Sea of Tranquility (`moon`). A rate-limited reply (HTTP 429) is also false.
- Label: the German article title from Wikipedia on a German device, if there is one; otherwise the English title after redirects. Cut to 100 bytes, never inside a UTF-8 character. Source on the bottom line: Wikipedia.
- Coastlines: Natural Earth, public domain. Built in: 110m, 134 lines, 5127 points (Natural Earth's 5128, less one duplicate after rounding to 1/100 degree), 20786 bytes. The whole feature adds 30632 bytes of flash. On the server: 50m thinned to 15000 points in 1379 lines, 62768 bytes, 8.6 km tolerance, at `EDU_DATA_BASE_URL "globe/coast_detail.bin"`.
- Order in `fetch()`: Wikipedia first, up to the 10 s deadline. Only with a place: the server file, at most 4 s and never past the deadline. If it's unreachable, late or doesn't pass the parser, the built-in coast is used and the answer still counts. This second request happens on every globe answer and every wake from deep sleep. No sprite, no network in the view. Everything is freed in `unload()`.
- File format, little endian: `ZGLB`, version u8 = 1, reserved u8 = 0, lines u16, points u16; points per line u16 * lines; lat, lon in 1/100 degree, i16 * points. Parser limits: 4000 lines, 20000 points, 64 KB file, at least 2 points per line, sizes must add up to the exact file size, every coordinate in range. The built-in file uses the same parser.
- View: starts with the pin in the middle, north up. Orthographic projection, so the far side is exactly z < 0; coast pieces are cut at the disc's edge. Button B: zoom 1, 1.6, 2.4 around the middle. Button C: back to the text. Going back to the view keeps the turn.
- Pin: citrus #f2b705. Behind the globe, or off the screen when zoomed in: a citrus arrow at the edge shows the way instead. Exactly behind, it points right.
- Turning: `turnsWithGyro` is true, so the shared view turns the globe with the hand at 2x. The flick is added on top from `modelGyroDps`: a peak of at least 200 deg/s no older than 0.25 s when the hand drops below 30 deg/s starts a coast at the same gain and direction, capped at 6 rad/s, slowing with a 1.2 s time constant, stopping under 0.2 rad/s. Any hand motion of 30 deg/s or more catches it. `GLOBE_TURN_GAIN` and `GLOBE_TURN_SIGN` mirror `MODEL_TURN_GAIN` and `MODEL_TURN_SIGN`: `model_view.ino` comes later in the build, so they can't be shared. Change them together.
- Lying still, the shared view idles at 0.35 rad/s about the screen's vertical axis, not the Earth's axis. This turns the pin out of sight after about 4.5 s and tilts the globe's motion. Open: the globe should skip this idle turn (see the merge note below).
- The flick's 30 deg/s catch limit is above the shared 3 deg/s still limit on purpose: a stick in the hand rarely reads under 3, so the coast would end at once. Between the two, the hand's small turn and the coast simply add up.
- Drawing speed: rotation in 14-bit fixed point, z first so hidden points skip x and y, pieces that stay on one pixel are left out, and lines go straight into the 16-bit sprite buffer. Counted on the host, worst of many views, not timed:

  | Coast | Radius | Points turned | Pieces drawn, at most | Pixels, at most |
  | --- | --- | --- | --- | --- |
  | Built in | 55 | 5127 | 1929 | 3900 |
  | Built in | 88 | 5127 | 2587 | 4690 |
  | Built in | 132 | 5127 | 3017 | 4220 |
  | Server | 55 | 15000 | 3360 | 6748 |
  | Server | 88 | 15000 | 4868 | 8419 |
  | Server | 132 | 15000 | 6402 | 8259 |

- Test on the stick: serial `model globe:Matterhorn`. Every 5 s the view logs `globe fps=... drawUs avg=... max=... gapUs max=... points=... pieces=... zoom=... sprite=...`. `fps` is capped at 25 by the shared view, so `drawUs` is the number to read: time spent in `frame()`, without pushing the sprite. `points=5127` means built in, `points=15000` the server file. Target: 25 fps with the built-in coast. Once the server file is uploaded there is no switch back to the built-in coast, so measure before uploading, then again after. If the server file is too slow, lower `DETAIL_POINTS` in the script and generate it again.
- Not measured: anything on the stick. Frame rate, flick feel and the 4 s limit for the server file have only been reasoned about.

- Merge note, shared file `model_view.ino`, in `modelViewTick`: `if (magnitude < MODEL_STILL_DPS && activeFeature->turnsWithGyro && activeFeature != &FEATURE_GLOBE)`. Not made here. Until it is, the arrow does the finding.

## User guide

Ask where a place is: "Where is the Matterhorn?" or "Wo liegt Paris?". Zitro shows the Earth with a yellow pin on that place.

- **Turn the stick** to turn the globe.
- **Flick it quickly** and the globe keeps spinning, then slows down. Move the stick again to stop it.
- **Lost the pin?** A yellow arrow shows which way it is.
- **Button B** zooms in, three steps.
- **Button C** shows the written answer.
- **Button A** asks a new question.

Zitro only shows a globe for places Wikipedia has a position for, on Earth. For anything else, such as Mars, you get the picture. "What does the Matterhorn look like?" also gets a picture.

## Release note

- Ask "Where is the Matterhorn?" and find it on a globe you spin with your hand: flick it and watch it turn!

## Sources and licences

- **Natural Earth coastlines**, 110m and 50m, from `github.com/nvkelso/natural-earth-vector`. Public domain: "No permission is needed to use Natural Earth. Crediting the authors is unnecessary." (naturalearthdata.com/about/terms-of-use, read 2026-11-06). No conditions. Thinned for the server file.
- **Wikipedia API**, en.wikipedia.org, for coordinates and article titles. Only facts and titles are shown, no article text. Wikipedia's text is CC BY-SA, and the bottom line names Wikipedia as the source. Wikimedia's API rules ask for a User-Agent that identifies the client. `eduHttpGet` sends the plain `Zitro`. While testing from a shared address, the API answered HTTP 429 with `retry-after` about 25 s, and a single request after waiting worked.
