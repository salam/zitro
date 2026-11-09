<?php
// Tests the place table and its lookup: the generator on a few real GeoNames lines, then place_find.
// Run by tests/clock/run.sh with the folder the generator wrote as the first argument.
require_once __DIR__ . '/../../webpage2/api/place_lib.php';

$dir = $argv[1];
$checks = 0;
$failures = 0;
function check(bool $ok, string $what): void {
    global $checks, $failures;
    $checks++;
    if (!$ok) { $failures++; fwrite(STDERR, "  FAIL $what\n"); }
}
function has(?array $p, string $name, string $region, string $what): void {
    check($p !== null && $p['name'] === $name && $p['region'] === $region, $what . ': ' . json_encode($p));
}

$k = place_find($dir, 'knoxville');
has($k, 'Knoxville', 'TN', 'biggest Knoxville');
check($k !== null && $k['zone'] === 'America/New_York' && $k['rule'] === 'EST5EDT,M3.2.0,M11.1.0', 'its zone and rule');
check($k !== null && abs($k['lat'] - 35.961) < 0.01 && abs($k['lon'] + 83.921) < 0.01 && $k['source'] === 'GeoNames', 'where it is');

has(place_find($dir, 'springfield'), 'Springfield', 'MO', 'a name many share goes to the biggest');
has(place_find($dir, 'springfield', 'illinois'), 'Springfield', 'IL', 'a state by name');
has(place_find($dir, 'springfield', 'il'), 'Springfield', 'IL', 'a state by code');
has(place_find($dir, 'paris'), 'Paris', 'FR', 'Paris is in France');
has(place_find($dir, 'paris', 'texas'), 'Paris', 'TX', 'unless the hint says Texas');
has(place_find($dir, 'paris', 'usa'), 'Paris', 'TX', 'a country alias');
has(place_find($dir, 'paris', 'france'), 'Paris', 'FR', 'a country by name');
has(place_find($dir, 'cologne'), 'Köln', 'DE', 'a big city under another name beats a village');
has(place_find($dir, 'brooklyn'), 'Brooklyn', 'NY', 'a very big district beats towns');
has(place_find($dir, 'gottingen'), 'Göttingen', 'DE', 'without the umlaut');
has(place_find($dir, 'goettingen'), 'Göttingen', 'DE', 'written the German way');

check(place_find($dir, 'nosuchplace') === null, 'no such place');
check(place_find($dir, 'zq') === null, 'no such file');
check(place_find($dir, 'paris', 'narnia') === null, 'an unknown region finds nothing rather than the wrong Paris');
check(place_find($dir, 'knoxville', 'france') === null, 'a region it is not in');
foreach (['../zones', 'a', '', 'PARIS', 'pa ris', str_repeat('a', 49), "paris\n"] as $bad) {
    check(place_find($dir, $bad) === null, 'refuses name ' . json_encode($bad));
}
check(place_find($dir, 'paris', '../x') === null, 'refuses a bad hint');

// The endpoint itself, in its own process: refusals and the shape of a reply
function ask(array $get): string {
    $code = '$_GET = ' . var_export($get, true) . '; include ' . var_export(__DIR__ . '/../../webpage2/api/place.php', true) . ';';
    return (string)shell_exec('php -r ' . escapeshellarg($code) . ' 2>&1');
}
$reply = json_decode(ask(['q' => 'knoxville']), true);
check(($reply['found'] ?? null) === true && $reply['region'] === 'TN' && strlen(json_encode($reply)) < 1024, 'the endpoint answers');
check(json_decode(ask(['q' => 'nosuchplace']), true) === ['found' => false], 'and says when it knows nothing');
foreach ([['q' => ''], [], ['q' => '../../etc'], ['q' => ['a']], ['q' => 'paris', 'r' => '<x>'], ['q' => 'paris', 'r' => ['a']]] as $get) {
    check((json_decode(ask($get), true)['error'] ?? '') === 'bad name', 'endpoint refuses ' . json_encode($get));
}

echo "place: $checks checks, $failures failures\n";
exit($failures ? 1 : 0);
