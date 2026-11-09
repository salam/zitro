<?php
// Finding a place in the table made by scripts/make_place_data.py (GeoNames, CC BY 4.0). Pure functions
// over the files in one folder: place.php answers the stick, tests/clock/test_place.php checks this.

const PLACE_KEY_PATTERN = '/^[a-z]{2,48}$/D';

// What a folded region or country name can mean: ['US.TX', ...]. Empty if nothing.
function place_region_scopes(string $dir, string $hint): array {
    $scopes = [];
    $file = @fopen($dir . '/regions.tsv', 'r');
    if (!$file) return [];
    while (($line = fgets($file)) !== false) {
        [$key, $scope] = explode("\t", rtrim($line, "\n"));
        if ($key === $hint) $scopes[$scope] = true;
    }
    fclose($file);
    return $scopes;
}

// The biggest place of that (folded) name, in that region if a hint is given. Null if none. The keys are
// checked here too: they become part of a file name.
function place_find(string $dir, string $name, string $hint = ''): ?array {
    if (!preg_match(PLACE_KEY_PATTERN, $name) || ($hint !== '' && !preg_match(PLACE_KEY_PATTERN, $hint))) return null;
    $scopes = null;
    if ($hint !== '') {
        $scopes = place_region_scopes($dir, $hint);
        if (!$scopes) return null;
    }
    $path = $dir . '/' . substr($name, 0, 2) . '.tsv.gz';
    if (!is_file($path)) return null;
    $file = gzopen($path, 'rb');
    if (!$file) return null;
    $hit = null;
    while (($line = gzgets($file)) !== false) {
        $c = explode("\t", rtrim($line, "\n"));
        if (count($c) < 7) continue;
        $order = strcmp($c[0], $name);
        if ($order > 0) break;  // Sorted: it won't come any more
        if ($order < 0) continue;
        if ($scopes === null || isset($scopes[$c[2]]) || isset($scopes[$c[2] . '.' . $c[3]])) {
            $hit = $c;
            break;  // Biggest first
        }
    }
    gzclose($file);
    if ($hit === null) return null;
    $zones = file($dir . '/zones.tsv', FILE_IGNORE_NEW_LINES);
    $zone = $zones === false ? null : ($zones[(int)$hit[6]] ?? null);
    if ($zone === null) return null;
    [$iana, $rule] = explode("\t", $zone);
    return [
        'found' => true,
        'name' => $hit[1],
        'region' => $hit[2] === 'US' ? $hit[3] : $hit[2],  // Only US state codes say something everywhere
        'country' => $hit[2],
        'lat' => (float)$hit[4],
        'lon' => (float)$hit[5],
        'zone' => $iana,
        'rule' => $rule,
        'source' => 'GeoNames',
    ];
}
