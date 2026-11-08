# ISS live: where the space station is right now

Kind `iss`, query `now`. Stick: `feature_iss.ino`, `iss_core.h`, and the globe's shared drawing in `feature_globe.ino`. Server: `webpage2/api/iss.php`, `webpage2/api/iss_lib.php`. Tests: `tests/iss/run.sh`. Requested by the user on 2026-11-08: the zitro.spot server fetches NASA's data, the stick shows it.

## Requirements

### Server (`https://zitro.spot/api/iss.php`)

- Source: NASA JSC Flight Operations (TOPO), ISS trajectory data, `https://nasa-public-data.s3.amazonaws.com/iss-coords/current/ISS_OEM/ISS.OEM_J2K_EPH.txt`. CCSDS OEM, EME2000 frame, UTC, one position and velocity every 4 minutes for about 15 days, published every few days.
- Cached in `api/cache/iss_oem.txt` and fetched again when the copy is older than 12 hours or covers less than the next day. One request fetches at a time (lock file); the others use the old copy. A failed fetch waits 12 hours unless the copy runs out. The cache folder is closed to the web (`.htaccess`, 403).
- Per request: the position every 60 s from 45 minutes ago to 3 hours ahead (226 points), by cubic Hermite interpolation of the two neighbouring vectors (uses their velocities; about 0.1 km over 4 minutes, tested by leaving out every second vector). Then J2000 to the mean equator of date (IAU 1976 precession), Earth's turn (Greenwich mean sidereal time, IAU 1982), and geodetic latitude, longitude and height on WGS84. Nutation and polar motion are left out: under a kilometre on the ground.
- Also: speed (from the velocity, km/h) and orbit time (vis-viva, Kepler's third law, GM 398600.4418).
- Reply JSON, about 5 KB: `source`, `created` (the NASA file's date), `t0`, `step`, `lat`, `lon`, `alt` (km), `speed_kmh`, `period_min`, `now`. HTTP 503 with `error` if there's no data for now. No input is read from the request. `Cache-Control: max-age=60`.
- Checked 2026-11-08 against wheretheiss.at, which computes from NORAD's element sets instead: 1.7 km apart on the ground, same height (430.3 km) and speed.
- Deploy: copy `webpage2/api/iss.php`, `iss_lib.php` and `cache/.htaccess` to `public_html/zitro.spot/api/`. The script makes `cache/` itself.

### Stick

- The AI sets `model` = `iss:now` for "where is the ISS now" questions and must not state a place, height or speed itself. Like the clock, the written answer is always Zitro's own: computed from the track ("Die ISS fliegt gerade 437 km hoch bei 48,6° S, 16,6° O, mit 27 534 km/h. Eine Runde um die Erde dauert 93 Minuten."), or an honest "can't find it" without a clock or without an answer from the server. General orbit questions stay with `physics:satellites`.
- Lookup: one GET within the 10 s deadline, 16 KB at most. `issParseTrack` refuses anything incomplete or out of range (lengths, latitudes, heights 100-2000 km, step 10-600 s, speed, orbit time, at most 400 points). The track must cover now.
- View: the sunlit globe (as the clock), centred under the ISS at the lookup, a small station with solar wings where it is this moment, the track of the last 45 minutes in grey and the next 90 minutes in citrus. Left: "ISS", height, speed, latitude, longitude, live. The station moves by the clock, with the fraction of a second, so it glides. Out of sight: the citrus arrow. Turning, flick and zoom as on the globe; no idle turn.
- Past the end of the track (3 hours after the lookup) the station and numbers disappear and the panel says to ask again: no position is better than a wrong one. A wake from deep sleep looks it up again.
- Not in shakes: it needs the network.
- Footer: "ISS live - NASA".

### Tests

- `tests/iss/run.sh`: server math in PHP (Meeus 12.a sidereal time, WGS84 on the equator and over the pole, precession rate, the real NASA file: parsing, interpolation, 226-point track within the inclination and heights, orbit time and speed, refusal past the data and of garbage) and the stick's side in C++ (the real server reply, interpolation, the date line, hostile replies, the answer text).
- On the stick (2026-11-08): lookup in 455 ms; the full voice flow sets `iss:now`; the station moves between screenshots. Not measured: frame rate.

## User guide

Ask "Wo ist die ISS gerade?" or "Where is the ISS?". Zitro shows the Earth with the space station where it is right now, as it is lit by the Sun this moment. The grey line is where it has just been, the yellow one where it will fly in the next 90 minutes. Left you see how high and how fast it flies and where it is. Watch closely: it moves! It goes round the Earth in about an hour and a half.

Zitro gets the station's path from NASA, through the Zitro server, so it needs WiFi.

## Release note

- Ask "Where is the ISS?" and watch the space station fly over the Earth live, with NASA's own data.

## Sources and licences

| What | Source | Terms |
| --- | --- | --- |
| ISS trajectory | NASA JSC Flight Operations, ISS OEM ephemeris (public S3 bucket `nasa-public-data`) | NASA data, US government work, no copyright in the US |
| Precession, sidereal time | IAU 1976 / IAU 1982 as in Meeus, Astronomical Algorithms, ch. 12 and 21 | Equations |
| WGS84 | NGA, World Geodetic System 1984 | Published constants |
| Cross-check (tests only) | wheretheiss.at | Read once, not shipped |
