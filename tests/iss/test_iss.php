<?php
// Host tests for the ISS track (webpage2/api/iss_lib.php). Run: tests/iss/run.sh
require __DIR__ . '/../../webpage2/api/iss_lib.php';

$checks = 0;
$failures = 0;
function check(bool $ok, string $what): void {
    global $checks, $failures;
    $checks++;
    if (!$ok) { $failures++; fwrite(STDERR, "  FAIL $what\n"); }
}
function near(float $a, float $b, float $e, string $what): void { check(abs($a - $b) <= $e, "$what: $a vs $b"); }

// Sidereal time, Meeus example 12.a: 1987-04-10 0h UT is 13h10m46.3668s
$g = rad2deg(iss_gmst(gmmktime(0, 0, 0, 4, 10, 1987)));
near($g, (13 + 10 / 60 + 46.3668 / 3600) * 15, 0.0001, 'GMST Meeus 12.a');

// WGS84: on the equator and over the pole, 400 km up
$p = iss_geodetic([ISS_WGS84_A + 400, 0, 0]);
near($p[0], 0, 1e-9, 'equator lat'); near($p[1], 0, 1e-9, 'equator lon'); near($p[2], 400, 1e-6, 'equator height');
$b = ISS_WGS84_A * (1 - ISS_WGS84_F);
$p = iss_geodetic([1e-9, 0, $b + 400]);
near($p[0], 90, 1e-6, 'pole lat'); near($p[2], 400, 1e-3, 'pole height');
$p = iss_geodetic([0, -(ISS_WGS84_A + 10), 0]);
near($p[1], -90, 1e-9, 'west lon');

// Precession: none at J2000, and about 50.3 arcseconds a year along the ecliptic
$r = iss_precess([1, 0, 0], gmmktime(12, 0, 0, 1, 1, 2000));
near($r[0], 1, 1e-6, 'no precession at J2000'); near($r[1], 0, 1e-6, 'no precession at J2000 y');
$r = iss_precess([1, 0, 0], gmmktime(12, 0, 0, 1, 1, 2026));
// The J2000 equinox's right ascension grows about 46.1 arcseconds a year (Meeus: m = 3.075 s/yr): 26 years
near(rad2deg(atan2($r[1], $r[0])) * 3600, 46.1 * 26, 5, 'equinox moves');

// The real file: parse, header, and Hermite interpolation good to well under a kilometre
$oem = iss_parse_oem(file_get_contents(__DIR__ . '/fixtures/iss_oem_head.txt'));
check($oem !== null, 'fixture parses');
check($oem['created'] === '2026-11-06T15:13:55.303', 'creation date');
check(count($oem['states']) > 200, 'states read');
check(abs($oem['states'][1][0] - $oem['states'][0][0] - 240) < 1e-6, '4 minutes apart');
$worst = 0;
$states = $oem['states'];
for ($i = 1; $i + 1 < count($states); $i += 2) {
    $sparse = [$states[$i - 1], $states[$i + 1]];  // 8 minutes apart: interpolate the one in between
    $s = iss_state_at($sparse, $states[$i][0]);
    $d = sqrt(($s[0] - $states[$i][1]) ** 2 + ($s[1] - $states[$i][2]) ** 2 + ($s[2] - $states[$i][3]) ** 2);
    $worst = max($worst, $d);
}
// The error grows with the 4th power of the gap: 8 minutes here, 4 on the server, so 16 times less there
check($worst < 2.0, "Hermite over 8 min within 2 km (worst $worst km)");
printf("  interpolation over 8 minutes: worst %.3f km, so about %.2f km over the server's 4 minutes\n", $worst, $worst / 16);
check(iss_state_at($states, $states[0][0] - 1) === null, 'before the data');

// The track: 226 points, heights where the ISS flies, one orbit in about 93 minutes
$now = $states[20][0] + 30;
$oem['start'] = $states[0][0];
$oem['stop'] = $states[count($states) - 1][0];
$track = iss_track($oem, $now);
check($track !== null, 'track inside the data');
check(count($track['lat']) === 226, 'track length ' . count($track['lat']));
foreach ($track['alt'] as $h) check($h > 380 && $h < 450, "height $h km");
foreach ($track['lat'] as $l) check(abs($l) <= 52.0, "latitude $l within the 51.6 degree inclination");
check($track['period_min'] > 91 && $track['period_min'] < 94, 'period ' . $track['period_min']);
check($track['speed_kmh'] > 27000 && $track['speed_kmh'] < 28000, 'speed ' . $track['speed_kmh']);
check(iss_track($oem, $oem['stop']) === null, 'refuses past the data');
check(iss_parse_oem("garbage\n") === null, 'garbage refused');
check(iss_parse_oem("") === null, 'empty refused');

// Place names: the real grid from scripts/make_iss_places.py
$places = iss_places_open(__DIR__ . '/../../webpage2/data/iss');
check($places !== null, 'place grid opens');
function place(array &$places, float $lat, float $lon): string {
    $i = iss_place_at($places, $lat, $lon);
    return $i === 0 ? '' : $places['names'][$i][0] . ' / ' . $places['names'][$i][1];
}
foreach ([
    [42.15, 9.10, 'Korsika / Corsica'],             // An island of a country with more land elsewhere
    [61.20, -149.90, 'Alaska / Alaska'],            // A state of a big federal country
    [-30.0, -120.0, 'Südpazifik / South Pacific'],  // The hand-made ocean halves
    [30.0, -40.0, 'Nordatlantik / North Atlantic'],
    [47.37, 8.54, 'Kanton Zürich / Zürich'],         // A canton
    [48.86, 2.35, 'Frankreich / France'],            // A country without states shown
    [53.35, -6.26, 'Irland / Ireland'],              // An island shared by two countries: the country
    [-1.0, 33.0, 'Victoriasee / Lake Victoria'],     // A big lake
    [42.0, 50.5, 'Kaspisches Meer / Caspian Sea'],
    [34.0, 18.0, 'Mittelmeer / Mediterranean Sea'],
    [32.08, 34.78, 'Israel / Israel'],               // A disputed whole country keeps its name
    [44.95, 34.10, ''],                              // Krim: a disputed region inside a country, no name
    [34.08, 74.80, ''],                              // Kashmir
    [48.00, 37.80, ''],                              // Donezk
    [90.0, 0.0, place($places, 89.99, 0.0)],         // The edges of the grid don't fail
    [-90.0, 180.0, place($places, -89.99, 179.99)],
] as [$la, $lo, $want]) {
    $got = place($places, $la, $lo);
    check($got === $want, "place at $la, $lo: '$got', expected '$want'");
}

// Along the track: changes every 10 s at most, inside the track, indices that point into the names
$track = iss_track($oem, $now, 2700, 10800, 60, $places);
check(isset($track['places'], $track['place_t'], $track['place_i']), 'track has places');
$n = count($track['place_t']);
check($n > 5 && $n === count($track['place_i']), "place changes $n");
check($track['place_t'][0] === 0, 'first change at the start');
$tLast = ($track['step'] ?? 60) * (count($track['lat']) - 1);
for ($i = 1; $i < $n; $i++) check($track['place_t'][$i] > $track['place_t'][$i - 1] && $track['place_t'][$i] <= $tLast, "change $i in order");
foreach ($track['place_i'] as $k) check($k >= -1 && $k < count($track['places']), "index $k");
foreach ($track['places'] as $pl) check(is_string($pl[0]) && $pl[0] !== '' && is_string($pl[1]) && $pl[1] !== '' && strlen($pl[0]) <= 47 && strlen($pl[1]) <= 47, 'name pair');
// Without the flag the reply is exactly what sticks from before it know
$plain = iss_track($oem, $now);
check(!isset($plain['places']) && !isset($plain['place_t']), 'no places without the flag');
$withNow = $track; unset($withNow['places'], $withNow['place_t'], $withNow['place_i']);
check($withNow === $plain, 'the rest of the reply is the same');

// Reply size over every start the fixture allows, for the stick's buffer (ISS_REPLY_LIMIT)
$largest = 0; $mostChanges = 0; $mostNames = 0; $slowest = 0;
for ($t = $oem['start'] + 2700; $t + 10800 <= $oem['stop']; $t += 1800) {
    $start = microtime(true);
    $r = iss_track($oem, $t, 2700, 10800, 60, $places);
    $slowest = max($slowest, microtime(true) - $start);
    $r['now'] = (int)$t;
    $largest = max($largest, strlen(json_encode($r, JSON_UNESCAPED_SLASHES)));
    $mostChanges = max($mostChanges, count($r['place_t'] ?? []));
    $mostNames = max($mostNames, count($r['places'] ?? []));
    check(isset($r['places']), "places at $t");
}
printf("  replies with places: largest %d bytes, at most %d changes and %d names, slowest %.0f ms\n", $largest, $mostChanges, $mostNames, $slowest * 1000);
check($largest < 14 * 1024, "largest reply $largest bytes fits the stick's 24 KB with room");

echo "iss: $checks checks, $failures failures\n";
exit($failures ? 1 : 0);
