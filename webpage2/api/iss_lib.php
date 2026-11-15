<?php
// The ISS's track over the Earth, from NASA's published ephemeris. Pure functions, no network:
// iss.php fetches and caches the file, tests/iss/test_iss.php checks the math.
//
// Source: NASA JSC Flight Operations (TOPO), ISS trajectory data, CCSDS OEM, EME2000 frame, UTC,
// one state vector every 4 minutes for about 15 days:
// https://nasa-public-data.s3.amazonaws.com/iss-coords/current/ISS_OEM/ISS.OEM_J2K_EPH.txt

const ISS_MU = 398600.4418;          // Earth GM, km^3/s^2 (IERS 2010)
const ISS_WGS84_A = 6378.137;        // km
const ISS_WGS84_F = 1 / 298.257223563;
const ISS_PLACE_STEP = 10;          // Seconds between place lookups along the track: about 77 km of ground
const ISS_PLACE_MAX_NAMES = 160;      // Measured at most 106 in 3 h 45 (tests/iss); more, or more changes, and the reply has no places
const ISS_PLACE_MAX_CHANGES = 300;    // Measured at most 138

// Reads the state vectors and the header. Returns ['created' => string, 'start' => unix, 'stop' => unix,
// 'states' => [[t, x, y, z, vx, vy, vz], ...]] sorted by time, or null if the file isn't usable.
function iss_parse_oem(string $text): ?array {
    $states = [];
    $created = '';
    $start = null;
    $stop = null;
    foreach (preg_split('/\r?\n/', $text) as $line) {
        if (preg_match('/^CREATION_DATE\s*=\s*(\S+)/', $line, $m)) $created = $m[1];
        if (preg_match('/^USEABLE_START_TIME\s*=\s*(\S+)/', $line, $m)) $start = iss_utc($m[1]);
        if (preg_match('/^USEABLE_STOP_TIME\s*=\s*(\S+)/', $line, $m)) $stop = iss_utc($m[1]);
        if (!preg_match('/^(\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d(?:\.\d+)?)\s+(.+)$/', $line, $m)) continue;
        $values = preg_split('/\s+/', trim($m[2]));
        if (count($values) < 6) continue;
        $t = iss_utc($m[1]);
        if ($t === null) continue;
        $row = [$t];
        for ($k = 0; $k < 6; $k++) {
            if (!is_numeric($values[$k])) continue 2;
            $row[] = (float)$values[$k];
        }
        $states[] = $row;
    }
    if (count($states) < 2 || $start === null || $stop === null) return null;
    usort($states, fn($a, $b) => $a[0] <=> $b[0]);
    return ['created' => $created, 'start' => $start, 'stop' => $stop, 'states' => $states];
}

// "2026-11-06T12:00:00.000" (UTC) to Unix seconds, with fractions
function iss_utc(string $s): ?float {
    if (!preg_match('/^(\d{4})-(\d\d)-(\d\d)T(\d\d):(\d\d):(\d\d)(\.\d+)?$/', $s, $m)) return null;
    $whole = gmmktime((int)$m[4], (int)$m[5], (int)$m[6], (int)$m[2], (int)$m[3], (int)$m[1]);
    return $whole + (isset($m[7]) ? (float)$m[7] : 0.0);
}

// Position and velocity at time t by cubic Hermite interpolation between the two neighbouring vectors,
// which uses their velocities too: far better than linear over 4 minutes. Null outside the data.
function iss_state_at(array $states, float $t): ?array {
    $n = count($states);
    if ($t < $states[0][0] || $t > $states[$n - 1][0]) return null;
    $lo = 0;
    $hi = $n - 1;
    while ($hi - $lo > 1) {
        $mid = intdiv($lo + $hi, 2);
        if ($states[$mid][0] <= $t) $lo = $mid; else $hi = $mid;
    }
    $a = $states[$lo];
    $b = $states[$hi];
    $h = $b[0] - $a[0];
    if ($h <= 0) return [$a[1], $a[2], $a[3], $a[4], $a[5], $a[6]];
    $s = ($t - $a[0]) / $h;
    $h00 = 2 * $s ** 3 - 3 * $s ** 2 + 1;
    $h10 = $s ** 3 - 2 * $s ** 2 + $s;
    $h01 = -2 * $s ** 3 + 3 * $s ** 2;
    $h11 = $s ** 3 - $s ** 2;
    $d00 = (6 * $s ** 2 - 6 * $s) / $h;
    $d10 = 3 * $s ** 2 - 4 * $s + 1;
    $d01 = (-6 * $s ** 2 + 6 * $s) / $h;
    $d11 = 3 * $s ** 2 - 2 * $s;
    $out = [];
    for ($k = 0; $k < 3; $k++) {
        $out[] = $h00 * $a[1 + $k] + $h10 * $h * $a[4 + $k] + $h01 * $b[1 + $k] + $h11 * $h * $b[4 + $k];
    }
    for ($k = 0; $k < 3; $k++) {
        $out[] = $d00 * $a[1 + $k] + $d10 * $a[4 + $k] + $d01 * $b[1 + $k] + $d11 * $b[4 + $k];
    }
    return $out;
}

function iss_julian_day(float $unix): float { return $unix / 86400.0 + 2440587.5; }

// Greenwich mean sidereal time in radians (IAU 1982, Meeus 12.4)
function iss_gmst(float $unix): float {
    $d = iss_julian_day($unix) - 2451545.0;
    $T = $d / 36525.0;
    $deg = 280.46061837 + 360.98564736629 * $d + 0.000387933 * $T * $T - $T ** 3 / 38710000.0;
    $deg = fmod($deg, 360.0);
    if ($deg < 0) $deg += 360.0;
    return deg2rad($deg);
}

// J2000 (EME2000) to the mean equator and equinox of the date: IAU 1976 precession (Meeus 21.2-21.4).
// Nutation (under 20 arcseconds, about half a kilometre on the ground) is left out.
function iss_precess(array $r, float $unix): array {
    $T = (iss_julian_day($unix) - 2451545.0) / 36525.0;
    $as = M_PI / 180 / 3600;
    $zeta = (2306.2181 * $T + 0.30188 * $T * $T + 0.017998 * $T ** 3) * $as;
    $z = (2306.2181 * $T + 1.09468 * $T * $T + 0.018203 * $T ** 3) * $as;
    $theta = (2004.3109 * $T - 0.42665 * $T * $T - 0.041833 * $T ** 3) * $as;
    $cz = cos($zeta); $sz = sin($zeta);
    $cZ = cos($z); $sZ = sin($z);
    $ct = cos($theta); $st = sin($theta);
    $m = [
        [$cZ * $ct * $cz - $sZ * $sz, -$cZ * $ct * $sz - $sZ * $cz, -$cZ * $st],
        [$sZ * $ct * $cz + $cZ * $sz, -$sZ * $ct * $sz + $cZ * $cz, -$sZ * $st],
        [$st * $cz, -$st * $sz, $ct],
    ];
    return [
        $m[0][0] * $r[0] + $m[0][1] * $r[1] + $m[0][2] * $r[2],
        $m[1][0] * $r[0] + $m[1][1] * $r[1] + $m[1][2] * $r[2],
        $m[2][0] * $r[0] + $m[2][1] * $r[1] + $m[2][2] * $r[2],
    ];
}

// Earth-fixed position (km) to geodetic latitude, longitude (degrees) and height (km) on WGS84
function iss_geodetic(array $e): array {
    $e2 = ISS_WGS84_F * (2 - ISS_WGS84_F);
    $p = hypot($e[0], $e[1]);
    $lon = atan2($e[1], $e[0]);
    $lat = atan2($e[2], $p * (1 - $e2));
    for ($i = 0; $i < 8; $i++) {
        $sin = sin($lat);
        $N = ISS_WGS84_A / sqrt(1 - $e2 * $sin * $sin);
        $lat = atan2($e[2] + $e2 * $N * $sin, $p);  // Stable at the poles too
    }
    $sin = sin($lat);
    $h = $p * cos($lat) + $e[2] * $sin - ISS_WGS84_A * sqrt(1 - $e2 * $sin * $sin);
    return [rad2deg($lat), rad2deg($lon), $h];
}

// Where on Earth the ISS is at time t: [lat, lon, height km], or null outside the data
function iss_ground_point(array $states, float $t): ?array {
    $s = iss_state_at($states, $t);
    if ($s === null) return null;
    $r = iss_precess([$s[0], $s[1], $s[2]], $t);
    $g = iss_gmst($t);
    $earth = [cos($g) * $r[0] + sin($g) * $r[1], -sin($g) * $r[0] + cos($g) * $r[1], $r[2]];
    return iss_geodetic($earth);
}

// The reply for the stick: the track from $before seconds ago to $after seconds ahead, every $step seconds,
// plus speed and orbit time now. Null if the data doesn't cover that time. With $places (iss_places_open),
// also the place under the ISS along the track (iss_track_places).
function iss_track(array $oem, float $now, int $before = 2700, int $after = 10800, int $step = 60, ?array &$places = null): ?array {
    $t0 = floor(($now - $before) / $step) * $step;
    $t1 = $now + $after;
    if ($t0 < $oem['start'] || $t1 > $oem['stop']) return null;
    $lat = $lon = $alt = [];
    for ($t = $t0; $t <= $t1; $t += $step) {
        $p = iss_ground_point($oem['states'], $t);
        if ($p === null) return null;
        $lat[] = round($p[0], 3);
        $lon[] = round($p[1], 3);
        $alt[] = round($p[2], 1);
    }
    $s = iss_state_at($oem['states'], $now);
    $r = sqrt($s[0] ** 2 + $s[1] ** 2 + $s[2] ** 2);
    $v = sqrt($s[3] ** 2 + $s[4] ** 2 + $s[5] ** 2);
    $a = 1 / (2 / $r - $v * $v / ISS_MU);  // Vis-viva: the orbit's size from where and how fast
    $out = [
        'source' => 'NASA JSC ISS trajectory data (OEM)',
        'created' => $oem['created'],
        't0' => (int)$t0,
        'step' => $step,
        'lat' => $lat,
        'lon' => $lon,
        'alt' => $alt,
        'speed_kmh' => (int)round($v * 3600),
        'period_min' => round(2 * M_PI * sqrt($a ** 3 / ISS_MU) / 60, 1),
    ];
    if ($places !== null) $out += iss_track_places($oem, $places, $t0, $t0 + (count($lat) - 1) * $step);
    return $out;
}

// The place grid from scripts/make_iss_places.py (webpage2/data/iss): names, and for each row of cells
// runs of (count, name index). Null if the files are missing or don't fit together.
function iss_places_open(string $dir): ?array {
    $json = @file_get_contents($dir . '/places.json');
    $bin = @file_get_contents($dir . '/places.bin');
    $meta = is_string($json) ? json_decode($json, true) : null;
    if (!is_array($meta) || !is_array($meta['names'] ?? null) || !is_string($bin) || strlen($bin) < 12 || substr($bin, 0, 4) !== 'ISSP') return null;
    $h = unpack('vversion/vcols/vrows', $bin, 4);
    if ($h['version'] !== 1 || $h['cols'] < 1 || $h['rows'] < 1 || strlen($bin) < 12 + 4 * $h['rows']) return null;
    return ['names' => $meta['names'], 'bin' => $bin, 'cols' => $h['cols'], 'rows' => $h['rows'],
            'offsets' => array_values(unpack('V' . $h['rows'], $bin, 12)), 'decoded' => []];
}

// The name index under a point, 0 for none. Rows are decoded once and kept in $places.
function iss_place_at(array &$places, float $lat, float $lon): int {
    $rows = $places['rows'];
    $cols = $places['cols'];
    $r = min($rows - 1, max(0, (int)floor((90 - $lat) / 180 * $rows)));
    $c = min($cols - 1, max(0, (int)floor(fmod($lon + 540, 360) / 360 * $cols)));
    if (!isset($places['decoded'][$r])) {
        $runs = [];
        $pos = $places['offsets'][$r];
        $end = $r + 1 < $rows ? $places['offsets'][$r + 1] : strlen($places['bin']);
        $x = 0;
        while ($pos + 4 <= $end && $x < $cols) {
            $run = unpack('vn/vid', $places['bin'], $pos);
            $pos += 4;
            $x += $run['n'];
            $runs[] = [$x, $run['id']];
        }
        $places['decoded'][$r] = $runs;
    }
    foreach ($places['decoded'][$r] as [$upTo, $id]) {
        if ($c < $upTo) return $id < count($places['names']) ? $id : 0;
    }
    return 0;
}

// The place under the ISS from $t0 to $t1, looked up every ISS_PLACE_STEP seconds and sent as changes:
// 'place_t' seconds after $t0, 'place_i' an index into 'places' ([German, English]) or -1 for no place.
// Nothing if the ISS passes more places than the stick keeps: no place is better than a cut-off list.
function iss_track_places(array $oem, array &$places, float $t0, float $t1): array {
    $used = [];
    $times = $indices = [];
    $last = -1;
    for ($t = $t0; $t <= $t1; $t += ISS_PLACE_STEP) {
        $p = iss_ground_point($oem['states'], $t);
        $id = $p === null ? 0 : iss_place_at($places, $p[0], $p[1]);
        if ($id === $last) continue;
        $last = $id;
        if ($id !== 0 && !isset($used[$id])) $used[$id] = count($used);
        $times[] = (int)round($t - $t0);
        $indices[] = $id === 0 ? -1 : $used[$id];
    }
    if (count($used) > ISS_PLACE_MAX_NAMES || count($times) > ISS_PLACE_MAX_CHANGES) return [];
    $names = [];
    foreach ($used as $id => $_) $names[] = $places['names'][$id];
    return ['places' => $names, 'place_t' => $times, 'place_i' => $indices];
}
