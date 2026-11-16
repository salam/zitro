# ISS live: where the space station is right now

Kind `iss`, query `now`. Stick: `feature_iss.ino`, `iss_core.h`, and the globe's shared drawing in `feature_globe.ino`. Server: `webpage2/api/iss.php`, `webpage2/api/iss_lib.php`, place names in `webpage2/data/iss/` (made by `scripts/make_iss_places.py`). Tests: `tests/iss/run.sh`. Requested by the user on 2026-11-08: the zitro.spot server fetches NASA's data, the stick shows it. Place names under the ISS requested on 2026-11-08 ("über Korsika", "über Alaska", "über dem Südpazifik").

## Requirements

### Server (`https://zitro.spot/api/iss.php`)

- Source: NASA JSC Flight Operations (TOPO), ISS trajectory data, `https://nasa-public-data.s3.amazonaws.com/iss-coords/current/ISS_OEM/ISS.OEM_J2K_EPH.txt`. CCSDS OEM, EME2000 frame, UTC, one position and velocity every 4 minutes for about 15 days, published every few days.
- Cached in `api/cache/iss_oem.txt` and fetched again when the copy is older than 12 hours or covers less than the next day. One request fetches at a time (lock file); the others use the old copy. A failed fetch waits 12 hours unless the copy runs out. The cache folder is closed to the web (`.htaccess`, 403).
- Per request: the position every 60 s from 45 minutes ago to 3 hours ahead (226 points), by cubic Hermite interpolation of the two neighbouring vectors (uses their velocities; about 0.1 km over 4 minutes, tested by leaving out every second vector). Then J2000 to the mean equator of date (IAU 1976 precession), Earth's turn (Greenwich mean sidereal time, IAU 1982), and geodetic latitude, longitude and height on WGS84. Nutation and polar motion are left out: under a kilometre on the ground.
- Also: speed (from the velocity, km/h) and orbit time (vis-viva, Kepler's third law, GM 398600.4418).
- Reply JSON, about 5 KB: `source`, `created` (the NASA file's date), `t0`, `step`, `lat`, `lon`, `alt` (km), `speed_kmh`, `period_min`, `now`. HTTP 503 with `error` if there's no data for now. `Cache-Control: max-age=60`. UTF-8 as it is, no `\u` escapes.
- The only input read from the request is `places=1` (compared exactly). With it the reply also has `places` (pairs of German and English names), `place_t` (seconds after `t0`) and `place_i` (index into `places`, -1 for no name): from each `place_t` on, the ISS is over that place until the next change. Sticks from before it don't send the flag and get exactly the reply they know. If the place files are missing, the reply simply has no places.
- Places along the track: the ground point every 10 s (about 77 km), looked up in the grid; a cell is 0.25 degrees (about 28 km). A name can be up to about 50 km early or late at a border. If the track passes more than 160 names or 300 changes, the reply has no places at all: measured at most 106 and 138 over the test file, 9.1 KB, 4 ms.
- Checked 2026-11-08 against wheretheiss.at, which computes from NORAD's element sets instead: 1.7 km apart on the ground, same height (430.3 km) and speed.
- Deploy: copy `webpage2/api/iss.php`, `iss_lib.php` and `cache/.htaccess` to `public_html/zitro.spot/api/`. The script makes `cache/` itself.

### Place names (`scripts/make_iss_places.py`)

- Data: Natural Earth 5.1.2, public domain, the GeoJSON files from `https://raw.githubusercontent.com/nvkelso/natural-earth-vector/v5.1.2/geojson/`. The script checks each file's SHA-256 against the pinned one and writes `places.bin` (run-length rows of name indices, 108 KB) and `places.json` (1093 name pairs, the sources with their SHA-256, and which disputed areas are blank). Regenerate with `python3 -I scripts/make_iss_places.py <downloads> webpage2/data/iss`.
- Names are shown exactly as the source spells them. On the stick they follow a citrus arrow and in the written answer a colon ("Unter ihr liegt gerade: Korsika."), so German needs no article or case ending (the user's choice, 2026-11-16: "über Tyrrhenisches Meer" would be wrong, the source has no articles).
- Painted in this order, each over the one before:
  1. Seas and oceans (`ne_10m_geography_marine_polys`), biggest first. Natural Earth halves the Pacific and Atlantic but names both halves the same in German, so four names are made by hand from its English ones: Nordpazifik, Südpazifik, Nordatlantik, Südatlantik (English: North Pacific, South Pacific, North Atlantic, South Atlantic).
  2. Countries (`ne_10m_admin_0_countries`), as Natural Earth draws them.
  3. States, provinces and cantons, only for USA, Canada, Australia, Brazil, Russia, China, India, Mexico, Argentina (big federal countries: "Alaska" says more than "Vereinigte Staaten") and Germany, Austria, Switzerland (known here by their states and cantons). Two nameless placeholders in the source are left out.
  4. Islands and island groups (`ne_10m_geography_regions_polys`) smaller than 100 000 km² with at least 90% of their cells in one country: Korsika, Sizilien, Kreta, Hawaii. Bigger ones are often a country's heartland (Honshu, Great Britain); an island shared by countries (Ireland, Borneo) is told by the country.
  5. Big lakes (`ne_10m_lakes`, scale rank 0 and 1): Great Lakes, Victoriasee, Titicacasee. English from the full source name ("Lake Chad"), as its English field is cut short ("Chad").
  6. Disputed areas (`ne_10m_admin_0_disputed_areas`) get no name at all when they're a region inside a country: Krim, parts of Kashmir, Donezk, Luhansk, Abchasien, Südossetien, Transnistrien, Bergkarabach, the Moroccan-held part of Westsahara, Arunachal Pradesh, Essequibo, southern Belize, Sabah and smaller ones, 83 names in all (the user's choice, 2026-11-16). An area covering at least 90% of a country as Natural Earth draws it keeps that country's name: Israel, Kosovo, Republik China (Taiwan), Türkische Republik Nordzypern, Somaliland, Westsahara, Westjordanland, Falklandinseln and 5 more. The full lists are in `places.json`. Natural Earth's layer also lists claims on whole countries, which is why "all disputed areas blank" wasn't used.
- Typography: all-caps names get normal case ("TASMANIEN" becomes "Tasmanien"), typographic quotes and dashes become plain ones (the stick's font lacks them). The script fails on any other character outside the font (U+0020 to U+05FF) and on any name over 47 bytes.
- No name in a cell (water without a named sea, a blank disputed area): nothing is shown, never the nearest name.

### Stick

- The AI sets `model` = `iss:now` for "where is the ISS now" questions and must not state a place, height or speed itself. Like the clock, the written answer is always Zitro's own: computed from the track ("Die ISS fliegt gerade 437 km hoch bei 48,6° S, 16,6° O, mit 27 534 km/h. Eine Runde um die Erde dauert 93 Minuten."), or an honest "can't find it" without a clock or without an answer from the server. General orbit questions stay with `physics:satellites`.
- Lookup: one GET of `iss.php?places=1` within the 10 s deadline, 24 KB at most. `issParseTrack` refuses anything incomplete or out of range (lengths, latitudes, heights 100-2000 km, step 10-600 s, speed, orbit time, at most 400 points). The track must cover now. Place data is checked on its own (pairs of 1 to 47 bytes of UTF-8 inside the font, times rising inside the track, indices in range, at most 160 names and 300 changes): if anything is wrong, only the places are dropped.
- View: the sunlit globe (as the clock), centred under the ISS at the lookup and again every time the view opens from the text, where the station is then (requested 2026-11-08; past the end of the track the globe stays as it was), a small station with solar wings where it is this moment, the track of the last 45 minutes in grey and the next 90 minutes in citrus. Text in the corners, upright for the reader (see the clock): "ISS" top left, height and speed top right, latitude bottom left and longitude bottom right in landscape; above and below the globe in portrait. The place under the station sits over the latitude (landscape y 77; portrait y 154, over at most the globe's bottom 13 rows: lat and lon on one line would be 134 px or more at longitudes from 100 degrees, wider than the 129 px there): a citrus arrow pointing down, then the name in white. Like all text in the view it has a 1 px black rim and no box (since 2026-11-16: boxes notched the centred globe). A name wider than the panel scrolls at 30 px/s after a 1.5 s pause at its start. No name there: no line. The station moves by the clock, with the fraction of a second, so it glides. Out of sight: the citrus arrow. Turning, flick and zoom as on the globe; no idle turn.
- Past the end of the track (3 hours after the lookup) the station and numbers disappear and the panel says to ask again: no position is better than a wrong one. A wake from deep sleep looks it up again.
- Not in shakes: it needs the network.
- The written answer adds the place at the lookup when there is one: "... Unter ihr liegt gerade: Korsika." / "... Right below it: Corsica."
- Footer: "ISS live - NASA, Natural Earth" with places, "ISS live - NASA" without.

### Tests

- `tests/iss/run.sh`: server math in PHP (Meeus 12.a sidereal time, WGS84 on the equator and over the pole, precession rate, the real NASA file: parsing, interpolation, 226-point track within the inclination and heights, orbit time and speed, refusal past the data and of garbage) and the stick's side in C++ (the real server reply, interpolation, the date line, hostile replies, the answer text). Places: lookups on the real grid (Korsika, Alaska, Südpazifik, Nordatlantik, Kanton Zürich, Frankreich, Irland, Victoriasee, Kaspisches Meer, Mittelmeer, Israel; nothing over Krim, Kashmir, Donezk), the changes along the track, the reply without the flag unchanged, the largest reply over the whole test file; on the stick's side a real reply with places, `\u` escapes, 13 kinds of bad place data, no place outside the track, the answer sentence.
- On the stick (2026-11-08): lookup in 455 ms; the full voice flow sets `iss:now`; the station moves between screenshots. Not measured: frame rate.

## User guide

Ask "Wo ist die ISS gerade?" or "Where is the ISS?". Zitro shows the Earth with the space station where it is right now, as it is lit by the Sun this moment. The grey line is where it has just been, the yellow one where it will fly in the next 90 minutes. Left you see how high and how fast it flies and where it is. Watch closely: it moves! It goes round the Earth in about an hour and a half.

A yellow arrow shows what lies right below the station: a sea, a country, a state or canton, or an island. Over a few places whose border is disputed, there's no name, only the coordinates.

Zitro gets the station's path from NASA, through the Zitro server, so it needs WiFi.

## Release note

- Ask "Where is the ISS?" and watch the space station fly over the Earth live, with NASA's own data.
- See what the ISS is flying over: Korsika, Alaska, the South Pacific or Kanton Zürich, right on the screen.

## Sources and licences

| What | Source | Terms |
| --- | --- | --- |
| ISS trajectory | NASA JSC Flight Operations, ISS OEM ephemeris (public S3 bucket `nasa-public-data`) | NASA data, US government work, no copyright in the US |
| Precession, sidereal time | IAU 1976 / IAU 1982 as in Meeus, Astronomical Algorithms, ch. 12 and 21 | Equations |
| WGS84 | NGA, World Geodetic System 1984 | Published constants |
| Place names | Natural Earth 5.1.2 (naturalearthdata.com), 10m marine areas, countries, states and provinces, regions (islands), lakes, disputed areas | Public domain ("No permission is needed to use Natural Earth"); credited in the footer and here anyway |
| Cross-check (tests only) | wheretheiss.at | Read once, not shipped |
