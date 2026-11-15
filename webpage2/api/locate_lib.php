<?php
// Roughly where an IPv4 address is, from the table made by scripts/make_ip_data.py (DB-IP Lite and GeoNames,
// both CC BY 4.0). Pure functions over the files in one folder: locate.php answers the stick,
// tests/clock/test_locate.php checks this.

const LOCATE_PLACE_SIZE = 96;
const LOCATE_NONE = 0xFFFFFFFF;

// The place record for an address, or null: not IPv4, not public, or no place known for it
function locate_ipv4(string $dir, string $zonesPath, string $ip): ?array {
    if (filter_var($ip, FILTER_VALIDATE_IP, FILTER_FLAG_IPV4 | FILTER_FLAG_NO_PRIV_RANGE | FILTER_FLAG_NO_RES_RANGE) === false) return null;
    $address = ip2long($ip);
    if ($address === false) return null;
    $size = @filesize($dir . '/ip4.bin');
    if (!$size || $size % 8) return null;
    $file = fopen($dir . '/ip4.bin', 'rb');
    if (!$file) return null;
    // The last record that starts at or before the address
    $low = 0;
    $high = intdiv($size, 8) - 1;
    $number = LOCATE_NONE;
    while ($low <= $high) {
        $middle = intdiv($low + $high, 2);
        fseek($file, $middle * 8);
        $record = unpack('Nstart/Nplace', fread($file, 8));
        if ($record['start'] <= $address) {
            $number = $record['place'];
            $low = $middle + 1;
        } else {
            $high = $middle - 1;
        }
    }
    fclose($file);
    if ($number === LOCATE_NONE) return null;
    return locate_place($dir, $zonesPath, $number);
}

function locate_place(string $dir, string $zonesPath, int $number): ?array {
    $file = fopen($dir . '/places.bin', 'rb');
    if (!$file) return null;
    fseek($file, $number * LOCATE_PLACE_SIZE);
    $raw = fread($file, LOCATE_PLACE_SIZE);
    fclose($file);
    if ($raw === false || strlen($raw) !== LOCATE_PLACE_SIZE) return null;
    $p = unpack('Z64name/Z8region/a2country/nzone/Glat/Glon', $raw);
    $zones = file($zonesPath, FILE_IGNORE_NEW_LINES);
    $zone = $zones === false ? null : ($zones[$p['zone']] ?? null);
    if ($zone === null || $p['name'] === '') return null;
    [$iana, $rule] = explode("\t", $zone);
    return [
        'found' => true,
        'name' => $p['name'],
        'region' => $p['region'],
        'country' => $p['country'],
        'lat' => round($p['lat'], 3),
        'lon' => round($p['lon'], 3),
        'zone' => $iana,
        'rule' => $rule,
        'source' => 'DB-IP',
    ];
}
