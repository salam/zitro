# World clock: the time anywhere, on a sunlit globe

Kind `clock`. Code: `feature_worldclock.ino`, `clock_core.h`, `clock_place.h`, `clock_data.h` (generated), and the globe's shared drawing in `feature_globe.ino` and `globe_core.h`. Server: `webpage2/api/place.php`, `place_lib.php`, `places/` (generated). Tests: `tests/clock/run.sh`, `tests/globe/run.sh` (day and night). Data: `scripts/make_clock_data.py`, `scripts/make_place_data.py`.

## Why

Before this, "Wie spät ist es in Tokio?" went to the AI, which is never told the time, so it made one up. The clock answers with a time Zitro computes itself, and shows where the city is and where it's day or night right now.

## Requirements

- `model` = `clock:<English city name>`, or `clock:here` for the home city. The prompt rule sends time questions here ("Wie spät ist es?", "Wie spät ist es in Tokio?", "Zeitverschiebung nach Sydney"); for a country the AI names its capital. Smaller places go the same way (`clock:Knoxville`); a region the user names comes after a comma (`clock:Paris, Texas`). It must never state a time, a date or a time difference itself.
- **The written answer is always Zitro's own.** For any answer with a `clock:` model the AI's text is replaced: by the computed sentence (`clockAnswer`) when the city opens, otherwise by a fixed honest sentence: "Ich kenne die Uhrzeit gerade nicht. Ich brauche dazu WLAN." without a clock, "Die Uhrzeit von <name> kenne ich nicht. Frag mich nach einer Hauptstadt oder grossen Stadt." for a place neither the table nor the server knows, "Ich kann <name> gerade nicht nachschlagen: Der Zitro-Server antwortet nicht. Hauptstädte und grosse Städte gehen auch ohne ihn." when the server can't be reached. The AI's own words about the time never reach the screen, the voice or the conversation memory.
- Computed sentence, German: "In Tokio ist es jetzt 15:32 Uhr, Sonntag. Das ist 8 Stunden später als in Zürich." Same zone: "Das ist dieselbe Zeit wie in <home>." Half hours: "4 Stunden 30 Minuten". For the home city: "Es ist jetzt 07:32 Uhr, Sonntag, in Zürich." English likewise.
- City lookup, step 1: the whole name in the built-in table (`clockFindCity`). Case, accents and punctuation are ignored ("Zürich", "zurich", "Zuerich"); German and English names and a few hand-written aliases (Peking, Bombay, NYC...) work. A name two cities share goes to the national capital, otherwise to the bigger city.
- City lookup, step 2, only when the table says no (added on the user's request, 2026-11-09): the server. See [Places that aren't built in](#places-that-arent-built-in). The whole name goes to the table first, so "Paris, Texas" can't turn into the Paris in the table.
- Clock: needs a clock set by NTP (after 2024-01-01). The 320 built-in cities need no network in the lookup or the view, so they work offline once the clock is set, also for a shake (shakes only pick built-in cities). The view never uses the network.
- View: globe on the right (centre x 168), times on the left, all live:
  - the city's name on top (requested 2026-11-08: the footer alone was too small to tell whose time it is), then its time big in citrus with the difference to home next to it ("+8 h", "-4:30 h", "="), then short weekday and UTC offset ("So, UTC+9");
  - when the city isn't home: home name, and below it the home time;
  - the globe starts with the city in the middle, north up, pin in citrus; arrow when it's out of sight;
  - day and night on the ocean as they are now (see below), and a small sun where the Sun stands overhead, if that side faces the viewer;
  - national capitals as white dots;
  - 24 faint lines at 7.5 + 15 n degrees: the nominal time zones. Real zone borders bend round countries; the lines show the idea, every time on screen comes from the real zone.
- Turning as on the globe: with the hand at 2x, flick to spin. No idle turn, like the globe (`model_view.ino` skips both). Button B: zoom 1, 1.6, 2.4. Button C: the written answer. Button A: new question.
- Footer: city name, "Natural Earth"; for a place from the server its name and region, "GeoNames" (CC BY 4.0 asks for the credit).
- Memory: the globe's built-in coastline (no server file: no network) in PSRAM, freed in `unload()`. 1.9 KB of static RAM for the capitals' unit vectors.

### Day and night (globe and clock)

- The Sun's overhead point from the date: Astronomical Almanac low-precision formulas (declination, right ascension, Greenwich mean sidereal time), about 0.01 degrees from 1950 to 2050. Tested against the 2026 equinox and solstices (USNO) and the equation of time on 3 November and 11 February.
- Each pixel of the disc gets the sine of the Sun's height there. Night below 6 degrees under the horizon (end of civil twilight), day from 3 degrees above, 16 shades between. The rim always shows a little twilight when the Sun is behind the viewer, as it should.
- The globe for where-is questions shows day and night too. Without a clock both show all day.
- Drawing cost: one square root per disc pixel (from a 256-step table, `globeFastSqrt`: the ESP32 has no square root instruction), at most the whole 240 x 117 body when zoomed. Measured on the stick 2026-11-08, globe for Tokyo with the 15000-point server coast, zoom 0: 18.7 fps, `drawUs` 20.4 ms with the sprite in internal RAM; 16.6 fps, 25.7 ms when the sprite landed in PSRAM. Both below the 25 fps target. The serial log now has `shadeUs`: shading takes 4.8 ms of the 19 ms frame, and a frame starts every 53 ms, so most of the gap is pushing the sprite and the rest of `loop()`, not day and night. Making the shading cheaper wouldn't reach 25 fps.

### Home city and time zone

- Setting `homeCity` (string, stored as the city's English name, default Zurich). Web page: a text field with the table's names as suggestions (`/cities`); a name not in the table isn't saved and the page says so. Voice: "Stelle meinen Wohnort auf Berlin" (the prompt lists it as "englischer Name einer Stadt"); an unknown city gets "Diese Stadt kenne ich nicht.".
- It sets the device's time zone with the zone's real POSIX rule, at boot, on every NTP start (`configTzTime`) and right after a change. Before, `configTime(3600, 3600)` gave newlib the TZ `UTC-1DST` without any dates, so the library picked its own default rule instead of the EU's last Sundays of March and October. Zone names in `<...>` become plain letters for newlib (`clockLibraryRule`); they don't change any time.
- Affects everything that reads local time, such as the parental lock's 20-6 h.

### Places that aren't built in

- Asked for only after the table said no, and only with a known clock: `GET https://zitro.spot/api/place.php?q=knoxville[&r=tennessee]`, one request within the 10 s deadline, 1 KB at most. The stick sends both parts folded to a-z (`clockSplitPlaceQuery`, the text before and after the first comma), the server accepts nothing else (400).
- Reply: `{"found":true,"name":"Knoxville","region":"TN","country":"US","lat":35.961,"lon":-83.921,"zone":"America/New_York","rule":"EST5EDT,M3.2.0,M11.1.0","source":"GeoNames"}` or `{"found":false}`. `Cache-Control: max-age=86400`.
- The server says where a place is and which rule its zone follows. The time is still computed on the stick from the rule (`clockRuleTime`, `clockAnswerFor`), as for built-in cities. `clockParsePlace` refuses a reply unless the rule parses and coordinates and texts make sense.
- Which place: the biggest of that name. Parts of a city (GeoNames `PPLX`) and places under another name count a tenth of their population: Brooklyn still beats the towns called Brooklyn, Cologne is Köln and not the village in Italy, a suburb doesn't beat a town. With a hint, only places in that US state or admin region or country; a hint it doesn't know finds nothing rather than the wrong place.
- Label: "Knoxville, TN" for US places (state), "Lyon, FR" elsewhere: other countries' region codes are often just numbers. On the panel the region is left out if it doesn't fit.
- Home city stays table-only: its rule also sets the device clock at boot, without a network.

### Data

- Cities: Natural Earth `ne_10m_populated_places` (public domain). Kept: every national capital (`Admin-0 capital`, `Admin-0 capital alt`), every place with at least 3 million people, Swiss places from 50,000, German and Austrian ones from 500,000. 320 cities, sorted biggest first.
- Names: Natural Earth's English and German names, letters above U+017F reduced to their base letters to fit the fonts.
- Zones: Natural Earth's `TIMEZONE` (IANA names). Eleven cities have none; `ZONE_FIXES` in the script names the zone the city lies in (Moscow Europe/Moscow, Sydney Australia/Sydney, ...).
- Rules: the POSIX TZ string at the end of each zone's TZif file (RFC 8536), from the tz database installed where the script runs (2026e when generated). 195 zones.
- Regenerate: `python3 -I scripts/make_clock_data.py ne_10m_populated_places.geojson /usr/share/zoneinfo sketch/sketch_jan12a/clock_data.h`. The header records the input's SHA-256 and the tz version. When a country changes its rules, regenerate with a newer tz database.
- Server places: GeoNames `cities1000` (171,815 places with at least 1000 people, each with its IANA zone), `admin1CodesASCII.txt` and `countryInfo.txt`. Keys are the name and ASCII name, the German spelling without umlauts, and other Latin-script names for places from 100,000 people: 231,377 names, 392 zones, 602 small gzip files by the first two letters (5.5 MB), so a request reads one small file. Regenerate: `python3 -I scripts/make_place_data.py cities1000.txt admin1CodesASCII.txt countryInfo.txt /usr/share/zoneinfo <date> webpage2/api/places`, same input same bytes; `SOURCE.txt` records the input's SHA-256, the date and the tz version. Deploy: copy `place.php`, `place_lib.php` and `places/` to `public_html/zitro.spot/api/`; `places/.htaccess` closes the folder to the web.

### Tests

- `tests/clock/run.sh`: every zone against the host's own tz database every 3 hours from November 2026 to November 2030 (2.28 million comparisons, none differ); rule parsing including southern hemisphere, all-year daylight time, half and quarter hours and broken rules; calendar round trips; every city found by its names; sorted keys; font range; Sun positions; the answer sentences.
- Also in `tests/clock/run.sh`: all 392 zones the server can send against the host's tz database (4.58 million comparisons, none differ); the generator and lookup on real GeoNames lines (biggest wins, hints, Cologne, Brooklyn, refusals, the endpoint's 400); hostile replies on the stick's side; Python's and the stick's fold agree on every name in the data (929,314 with `GEONAMES_CITIES` set to the real file).
- `tests/globe/run.sh`: shading levels, the disc filled only where it should be, the Sun's direction on screen.
- On the stick (2026-11-08, screenshots with `shot`): Tokyo, New Delhi (+4:30 h) and Sydney (summer time, UTC+11) show the right times, the night side and the Sun's overhead point where they belong, and the panel fits. Not checked: long home names (cut with "..."), how turning feels.

## User guide

Ask for the time: "Wie spät ist es in Tokio?", "What time is it in New York?", or just "Wie spät ist es?".

Zitro shows the Earth turned to that city, with a yellow pin. The time there is big at the top left, your own time below, and how many hours apart they are. The dark side of the Earth is where it's night right now, and the little sun shows where the Sun is straight overhead.

- **Turn the stick** to turn the globe; **flick** it to spin.
- **Button B** zooms in.
- **Button C** shows the answer as text.

Zitro has every capital and the biggest cities of the world built in, 320 in all. For other places, like "Wie spät ist es in Knoxville?", it asks the Zitro server, so those need WiFi. If there are several places with that name, you get the biggest, and you can say which one you mean: "Wie spät ist es in Paris in Texas?". Set your own city on the setup page under **Home city**, or say "Stelle meinen Wohnort auf Berlin". It also sets your Zitro's time zone, summer time included.

## Release note

- Ask "Wie spät ist es in Tokio?" and see the time there on a globe that shows where it's day and night right now. Set your home city and Zitro follows its summer time.

## Sources and licences

| What | Source | Terms |
| --- | --- | --- |
| Cities, coordinates, names, zone names | Natural Earth, `ne_10m_populated_places`, `github.com/nvkelso/natural-earth-vector` | Public domain: "No permission is needed to use Natural Earth." |
| Other places: names, coordinates, zone names | GeoNames, `cities1000`, `download.geonames.org/export/dump/` | CC BY 4.0: credit "GeoNames", shown on the stick's bottom line and on zitro.spot |
| Time zone rules | IANA tz database (tzdata 2026e), via the TZif files | Public domain |
| Sun's position | Astronomical Almanac, low-precision formulas | Equations, implemented here |
| Test dates | US Naval Observatory, 2026 equinoxes and solstices | US government data |
