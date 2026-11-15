<?php
// Tests the IP locator: scripts/make_ip_data.py on a few DB-IP lines and the GeoNames fixture, then
// locate_ipv4. Run by tests/clock/run.sh with the generator's folder and the zones file as arguments.
require_once __DIR__ . '/../../webpage2/api/locate_lib.php';

[$_, $dir, $zones] = $argv;
$checks = 0;
$failures = 0;
function check(bool $ok, string $what): void {
    global $checks, $failures;
    $checks++;
    if (!$ok) { $failures++; fwrite(STDERR, "  FAIL $what\n"); }
}
function at(string $ip): ?array {
    global $dir, $zones;
    return locate_ipv4($dir, $zones, $ip);
}
function is(?array $p, string $name, string $region, string $what): void {
    check($p !== null && $p['name'] === $name && $p['region'] === $region && $p['source'] === 'DB-IP', $what . ': ' . json_encode($p));
}

$k = at('5.0.0.7');
is($k, 'Köln', 'DE', "DB-IP's English name becomes the nearest GeoNames place");
check($k !== null && $k['zone'] === 'Europe/Berlin' && $k['rule'] === 'CET-1CEST,M3.5.0,M10.5.0/3', 'with its zone and rule');
check($k !== null && abs($k['lat'] - 50.933) < 0.01 && abs($k['lon'] - 6.95) < 0.01, "GeoNames' position, not DB-IP's");
is(at('5.0.0.0'), 'Köln', 'DE', 'the first address of a range');
is(at('5.0.1.255'), 'Köln', 'DE', 'the last address of the merged range');
check(at('5.0.2.0') === null, 'right after a range: nothing');
is(at('8.8.8.8'), 'Knoxville', 'TN', 'a US place has its state as region');
is(at('9.0.1.9'), 'Paris', 'TX', 'the Paris of that name nearby, not the biggest');
check(at('20.0.0.1') === null, 'no place within 100 km: nothing');
is(at('21.0.0.1'), 'Paris', 'FR', 'a district of DB-IP is the nearest GeoNames place');
check(at('0.1.2.3') === null, 'DB-IP knows no country: nothing');
check(at('200.1.2.3') === null, 'past the last range: nothing');
check(at('192.168.1.10') === null && at('10.0.0.1') === null, 'private addresses: nothing');
check(at('255.255.255.255') === null && at('127.0.0.1') === null, 'reserved addresses: nothing');
check(at('2001:db8::1') === null && at('') === null && at('not an address') === null, 'IPv6 and junk: nothing');
check(filesize("$dir/ip4.bin") === 8 * 9, 'neighbouring ranges of one place, and of none, merge: ' . filesize("$dir/ip4.bin"));

echo "locate: $checks checks, $failures failures\n";
exit($failures ? 1 : 0);
