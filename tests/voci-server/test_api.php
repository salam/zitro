<?php
// Tests webpage2/api/voci.php over HTTP, against the php -S that test_api.sh started:
//   php test_api.php <base URL> <the server's VOCI_DATA_DIR> <a temp folder for fixtures>
// Requests and their JSON, the code rules (header only, stored as its hash), limits, retention, the page.
require_once __DIR__ . '/fixtures.php';

[, $base, $data, $temp] = $argv;
$checks = 0;
$failures = 0;
function check(bool $ok, string $what): void {
    global $checks, $failures;
    $checks++;
    if (!$ok) { $failures++; fwrite(STDERR, "  FAIL $what\n"); }
}
function show($r): string {
    return substr(json_encode($r, JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES), 0, 400);
}
function req(string $method, string $path, array $headers = [], $body = null): array {
    global $base;
    $ch = curl_init($base . $path);
    $lines = [];
    foreach ($headers as $k => $v) $lines[] = "$k: $v";
    curl_setopt_array($ch, [CURLOPT_CUSTOMREQUEST => $method, CURLOPT_RETURNTRANSFER => true, CURLOPT_HTTPHEADER => $lines, CURLOPT_TIMEOUT => 30]);
    if ($body !== null) curl_setopt($ch, CURLOPT_POSTFIELDS, $body);
    $text = curl_exec($ch);
    return ['status' => curl_getinfo($ch, CURLINFO_RESPONSE_CODE), 'body' => (string)$text, 'json' => json_decode((string)$text, true)];
}
// A request to voci.php: JSON for an array body, the code in its header, a browser's User-Agent unless given
function api(string $method, string $query, ?string $code, $body = null, array $headers = []): array {
    if ($code !== null) $headers['X-Voci-Code'] = $code;
    $headers += ['User-Agent' => 'Mozilla/5.0 (test)'];
    if (is_array($body) && !array_filter($body, fn($v) => $v instanceof CURLFile)) {
        $body = json_encode($body);
        $headers['Content-Type'] = 'application/json';
    }
    return req($method, '/api/voci.php?' . $query, $headers, $body);
}
function upload(string $code, string $path, string $name, string $query = 'op=parse'): array {
    return api('POST', $query, $code, ['file' => new CURLFile($path, 'application/octet-stream', $name)]);
}
function is_error(array $r, int $status, string $error): bool {
    return $r['status'] === $status && ($r['json']['error'] ?? null) === $error && is_string($r['json']['message'] ?? null) && $r['json']['message'] !== '';
}
function dir_of(string $code): string {
    global $data;
    return $data . '/' . hash('sha256', $code);
}
function save(string $code, string $book, string $name, int $n = 3, int $len = 0): array {
    $entries = [];
    for ($i = 1; $i <= $n; $i++) {
        $entries[] = $len ? ['t' => str_pad("w$i ", $len, 'x'), 'b' => str_pad("W$i ", $len, 'y')] : ['t' => "word $i", 'b' => "Wort $i"];
    }
    return api('POST', 'op=save', $code, ['book' => $book, 'name' => $name, 'entries' => $entries]);
}

for ($i = 0; $i < 100 && req('GET', '/')['status'] === 0; $i++) usleep(100000);

// ---------- The page ----------

$page = req('GET', '/voci/');
check($page['status'] === 200 && str_contains($page['body'], 'voci.js?v=1.0.0'), 'the page is served with its versioned script');
check(req('GET', '/voci/voci.js')['status'] === 200, 'and its script');

// ---------- The code: header only, 26 characters A-Z2-7 ----------

$A = fixture_code();
check(is_error(api('GET', 'op=index', null), 400, 'bad_code'), 'no code');
check(is_error(api('GET', 'op=index', substr($A, 0, 25)), 400, 'bad_code'), 'a 25-character code');
check(is_error(api('GET', 'op=index', strtolower($A)), 400, 'bad_code'), 'a code in lower case');
check(is_error(api('GET', 'op=index', substr($A, 0, 25) . '1'), 400, 'bad_code'), 'a code with a 1');
check(is_error(api('GET', 'op=index&code=' . $A, $A), 400, 'bad_code'), 'the code in the query string is refused, even with the header');
check(is_error(api('GET', 'op=index&x=' . strtolower($A), $A), 400, 'bad_code'), 'anything code-shaped in the query string is refused');
check(is_error(api('GET', 'op=whatever', $A), 400, 'bad_request'), 'unknown op');
check(is_error(api('GET', '', $A), 400, 'bad_request'), 'no op');
check(api('POST', 'op=index', $A)['status'] === 405, 'index wants GET');
check(api('GET', 'op=save', $A)['status'] === 405, 'save wants POST');
$r = api('GET', 'op=index&lang=en', 'TOOSHORT');
check(is_error($r, 400, 'bad_code') && str_contains($r['json']['message'], 'QR code'), 'English message: ' . show($r['json']));
$r = api('GET', 'op=index', 'TOOSHORT');
check(str_contains($r['json']['message'] ?? '', 'QR-Code'), 'German message by default: ' . show($r['json']));

// ---------- Index of a stick without lists ----------

$r = api('GET', 'op=index', $A);
check($r['status'] === 200 && $r['json'] === ['current' => '', 'lists' => []], 'a new stick has no lists: ' . show($r));
check(!is_dir(dir_of($A)), 'reading creates nothing');

// ---------- Parse ----------

$csv = $temp . '/young-world-3-unit-1.csv';
file_put_contents($csv, "en,de\ndog,Hund\ncat,Katze\ncheese,Käse\nto go,gehen\nrabbit,\"Kaninchen, Hase\"\nbird,\n");
$r = upload($A, $csv, 'young-world-3-unit-1.csv');
$parsed = $r['json'];
check($r['status'] === 200 && array_keys($parsed ?? []) === ['entries', 'book', 'name', 'swapped', 'warnings'], 'parse answers: ' . show($r));
check(count($parsed['entries'] ?? []) === 5 && $parsed['entries'][4] === ['t' => 'rabbit', 'b' => 'Kaninchen, Hase'], 'parse gives the entries');
check(($parsed['book'] ?? '') === 'Young World 3' && ($parsed['name'] ?? '') === 'Unit 1' && ($parsed['swapped'] ?? null) === false, 'and the guesses');
check(count($parsed['warnings'] ?? []) === 1 && str_contains($parsed['warnings'][0], 'Zeile 7'), 'and a warning for the bird: ' . show($parsed['warnings'] ?? null));
check(!is_dir(dir_of($A)) && api('GET', 'op=index', $A)['json'] === ['current' => '', 'lists' => []], 'parse stores nothing');

$r = api('POST', 'op=parse', $A, ['other' => new CURLFile($csv, 'text/csv', 'x.csv')]);
check(is_error($r, 400, 'no_file'), 'parse without a file: ' . show($r));
$r = api('POST', 'op=parse', $A, ['file' => new CURLFile($csv, 'text/csv', 'x.csv'), 'code' => $A]);
check(is_error($r, 400, 'bad_code'), 'the code as a form field is refused');
$r = upload($A, fixture_apkg_new($temp . '/new.apkg'), 'deck.apkg');
check(is_error($r, 400, 'anki_new_format') && str_contains($r['json']['message'], 'Ältere Anki-Versionen unterstützen'), 'new Anki format: ' . show($r['json']));
$r = upload($A, $temp . '/new.apkg', 'deck.apkg', 'op=parse&lang=en');
check(is_error($r, 400, 'anki_new_format') && str_contains($r['json']['message'], 'Support older Anki versions'), 'new Anki format in English: ' . show($r['json']));
$r = upload($A, fixture_apkg($temp . '/old.apkg', ['collection.anki21' => [['<b>green</b>', 'grün'], ['red', 'rot']]]), 'Farben.apkg');
check($r['status'] === 200 && ($r['json']['entries'] ?? null) === [['t' => 'green', 'b' => 'grün'], ['t' => 'red', 'b' => 'rot']] && $r['json']['book'] === 'Farben', 'an Anki package: ' . show($r['json']));
file_put_contents($temp . '/huge.csv', str_repeat('a', 5242881));
check(is_error(upload($A, $temp . '/huge.csv', 'huge.csv'), 413, 'too_big'), 'a file over 5 MB');
file_put_contents($temp . '/short-rows.csv', str_repeat("a,b\n", 1300000));
$r = upload($A, $temp . '/short-rows.csv', 'short-rows.csv');
check(is_error($r, 400, 'too_many') && str_contains($r['json']['message'], 'mehr als 500'), 'almost 5 MB of short rows: too_many, read only up to row 502: ' . show($r['json'] ?? $r['status']));
file_put_contents($temp . '/list.pdf', "%PDF-1.4\n");
check(is_error(upload($A, $temp . '/list.pdf', 'list.pdf'), 400, 'unreadable'), 'a PDF');

// ---------- Save, list, rename, current, delete ----------

$r = api('POST', 'op=save', $A, ['book' => $parsed['book'], 'name' => $parsed['name'], 'entries' => $parsed['entries']]);
$id = $r['json']['id'] ?? '';
check($r['status'] === 200 && preg_match('/^[0-9a-f]{8}$/D', $id) === 1, 'save gives an id: ' . show($r));
check(($r['json']['warnings'] ?? null) === [] && ($r['json']['current'] ?? '') === 'Young World 3', 'the first book becomes current');
$lists = $r['json']['lists'] ?? [];
check(count($lists) === 1 && array_keys($lists[0]) === ['id', 'book', 'name', 'lang', 'base', 'count', 'updated'], 'index entry shape: ' . show($lists));
check(($lists[0]['count'] ?? 0) === 5 && $lists[0]['lang'] === 'en' && $lists[0]['base'] === 'de' && abs($lists[0]['updated'] - time()) < 60, 'index entry values');
check(api('GET', 'op=index', $A)['json'] === ['current' => 'Young World 3', 'lists' => $lists], 'index after save');
check(is_dir(dir_of($A)) && is_file(dir_of($A) . "/$id.json"), 'stored under the hash of the code');

$r = api('GET', "op=list&id=$id", $A);
check($r['status'] === 200 && $r['json'] === ['id' => $id, 'book' => 'Young World 3', 'name' => 'Unit 1', 'lang' => 'en', 'base' => 'de', 'entries' => $parsed['entries']], 'list: ' . show($r));
check(str_contains($r['body'], 'Käse'), 'list JSON keeps umlauts unescaped');
check(is_error(api('GET', 'op=list&id=../../x', $A), 400, 'bad_request'), 'a bad id');
check(is_error(api('GET', 'op=list&id=0000000g', $A), 400, 'bad_request'), 'a bad id with g');
check(is_error(api('GET', 'op=list&id=00000000', $A), 404, 'not_found'), 'an unknown id');
$B = fixture_code();
check(is_error(api('GET', "op=list&id=$id", $B), 404, 'not_found'), 'another stick can\'t read the list');

$r = api('POST', 'op=save', $A, ['book' => 'Young World 3', 'name' => 'Unit 2', 'entries' => [['t' => 'one', 'b' => 'eins'], ['t' => 'two', 'b' => ''], ['t' => '', 'b' => '']]]);
check($r['status'] === 200 && count($r['json']['warnings'] ?? []) === 1 && ($r['json']['lists'][1]['count'] ?? 0) === 1, 'save skips an empty side and warns: ' . show($r['json']));
$unit2 = $r['json']['id'] ?? '';
check(is_error(api('POST', 'op=save', $A, ['book' => '', 'name' => 'x', 'entries' => [['t' => 'a', 'b' => 'b']]]), 400, 'bad_name'), 'save without a book');
check(is_error(api('POST', 'op=save', $A, ['book' => 'B', 'name' => str_repeat('n', 61), 'entries' => [['t' => 'a', 'b' => 'b']]]), 400, 'bad_name'), 'save with a long name');
check(is_error(api('POST', 'op=save', $A, ['book' => 'B', 'name' => 'N', 'entries' => 'dog']), 400, 'bad_request'), 'save without entries');
check(is_error(api('POST', 'op=save', $A, ['book' => 'B', 'name' => 'N', 'entries' => [['t' => '', 'b' => '']]]), 400, 'no_entries'), 'save with nothing in it');
check(is_error(api('POST', 'op=save', $A, ['book' => 'B', 'name' => 'N', 'entries' => array_fill(0, 501, ['t' => 'a', 'b' => 'b'])]), 400, 'too_many'), 'save with 501 entries');
check(is_error(api('POST', 'op=save', $A, ['book' => 'B', 'name' => 'N', 'id' => 'abcdef12', 'entries' => [['t' => 'a', 'b' => 'b']]]), 404, 'not_found'), 'save over an unknown id');
check(is_error(req('POST', '/api/voci.php?op=save', ['X-Voci-Code' => $A, 'Content-Type' => 'application/json'], '{nope'), 400, 'bad_request'), 'save with broken JSON');

$r = api('POST', 'op=save', $A, ['id' => $id, 'book' => 'Young World 3', 'name' => 'Unit 1', 'entries' => [['t' => 'dog', 'b' => 'Hund']]]);
check($r['status'] === 200 && $r['json']['id'] === $id && count($r['json']['lists']) === 2 && $r['json']['lists'][0]['count'] === 1, 'save with an id replaces the list: ' . show($r['json']));
check(api('GET', "op=list&id=$id", $A)['json']['entries'] === [['t' => 'dog', 'b' => 'Hund']], 'replaced entries');

$r = save($A, 'Young World 4', 'Unit 10');
$yw4 = $r['json']['id'] ?? '';
$r = save($A, 'young world 4', 'Unit 9');
$names = array_map(fn($l) => $l['book'] . '/' . $l['name'], $r['json']['lists'] ?? []);
check($names === ['Young World 3/Unit 1', 'Young World 3/Unit 2', 'Young World 4/Unit 9', 'Young World 4/Unit 10'], 'books keep their first spelling, lists sorted naturally: ' . show($names));
check($r['json']['current'] === 'Young World 3', 'a new book doesn\'t change the current one');

$r = api('POST', 'op=current', $A, ['book' => 'YOUNG WORLD 4']);
check($r['status'] === 200 && $r['json']['current'] === 'Young World 4', 'current: ' . show($r['json']));
check(trim((string)file_get_contents(dir_of($A) . '/current')) === 'Young World 4', 'current is stored in its own file');
check(is_error(api('POST', 'op=current', $A, ['book' => 'Narnia']), 404, 'not_found'), 'current with an unknown book');
check(is_error(api('POST', 'op=current', $A, ['nope' => 1]), 400, 'bad_request'), 'current without a book');

$r = api('POST', 'op=rename', $A, ['id' => $unit2, 'name' => 'Unit 2 (Teil 1)']);
check($r['status'] === 200 && in_array('Young World 3/Unit 2 (Teil 1)', array_map(fn($l) => $l['book'] . '/' . $l['name'], $r['json']['lists'] ?? []), true), 'rename: ' . show($r['json']));
check(api('GET', "op=list&id=$unit2", $A)['json']['name'] === 'Unit 2 (Teil 1)', 'the list file has the new name');
$r = api('POST', 'op=rename', $A, ['id' => $unit2, 'book' => 'Young World 4']);
check($r['status'] === 200 && api('GET', "op=list&id=$unit2", $A)['json']['book'] === 'Young World 4', 'rename moves a list to another book');
check(is_error(api('POST', 'op=rename', $A, ['id' => $unit2, 'name' => ' ']), 400, 'bad_name'), 'rename to nothing');
check(is_error(api('POST', 'op=rename', $A, ['id' => '12345678', 'name' => 'x']), 404, 'not_found'), 'rename an unknown list');

$r = api('POST', 'op=delete', $A, ['id' => $yw4]);
check($r['status'] === 200 && count($r['json']['lists']) === 3 && !is_file(dir_of($A) . "/$yw4.json"), 'delete');
check(is_error(api('GET', "op=list&id=$yw4", $A), 404, 'not_found'), 'a deleted list is gone');
check(is_error(api('POST', 'op=delete', $A, ['id' => $yw4]), 404, 'not_found'), 'delete it twice');
foreach ($r['json']['lists'] as $list) {
    if ($list['book'] === 'Young World 4') $r = api('POST', 'op=delete', $A, ['id' => $list['id']]);
}
check($r['json']['current'] === 'Young World 3', 'when the current book\'s last list goes, the only book left is current: ' . show($r['json']));

// ---------- Unknown sticks, the stick's fetch, wipe ----------

$C = fixture_code();
check(is_error(api('POST', 'op=rename', $C, ['id' => $id, 'name' => 'x']), 404, 'not_found'), 'rename on a stick without lists');
check(is_error(api('POST', 'op=delete', $C, ['id' => $id]), 404, 'not_found'), 'delete on a stick without lists');
check(is_error(api('POST', 'op=current', $C, ['book' => 'x']), 404, 'not_found'), 'current on a stick without lists');
check(!is_dir(dir_of($C)), 'none of them creates a folder');

check(!is_file(dir_of($A) . '/seen'), 'not seen yet');
api('GET', 'op=index', $A);
check(!is_file(dir_of($A) . '/seen'), 'the page reading the index is not the stick');
api('GET', 'op=index', $A, null, ['User-Agent' => 'Zitro/2.3']);
check(abs((int)@file_get_contents(dir_of($A) . '/seen') - time()) < 60, 'the stick\'s fetch is noted');

$found = [];
$it = new RecursiveIteratorIterator(new RecursiveDirectoryIterator($data, FilesystemIterator::SKIP_DOTS));
foreach ($it as $file) {
    if (str_contains($file->getPathname(), $A) || str_contains((string)file_get_contents($file->getPathname()), $A)) $found[] = $file->getPathname();
}
check($found === [], 'the code itself is nowhere in the data folder: ' . show($found));

$r = api('POST', 'op=wipe', $A);
check($r['status'] === 200 && $r['json'] === ['current' => '', 'lists' => []] && !is_dir(dir_of($A)), 'wipe removes the stick\'s folder');
check(api('GET', 'op=index', $A)['json'] === ['current' => '', 'lists' => []], 'and its lists');
check(api('POST', 'op=wipe', $C)['status'] === 200 && !is_dir(dir_of($C)), 'wipe on a stick without lists');

// ---------- Retention: a stick unseen and unchanged for a year goes (every request sweeps here) ----------

$old = $data . '/' . hash('sha256', fixture_code());
mkdir($old, 0700);
file_put_contents($old . '/index.json', '[]');
file_put_contents($old . '/seen', (string)(time() - 400 * 86400));
touch($old . '/index.json', time() - 400 * 86400);
touch($old, time() - 400 * 86400);
api('GET', 'op=index', $B);
check(!is_dir($old), 'an old stick folder is swept');

// ---------- Limits ----------

$hour = intdiv(time(), 3600);
$W = fixture_code();
save($W, 'Book', 'Unit 1');
$ok = 0;
for ($i = 0; $i < 119; $i++) {
    if (api('POST', 'op=rename', $W, ['id' => api('GET', 'op=index', $W)['json']['lists'][0]['id'], 'name' => "Unit $i"])['status'] === 200) $ok++;
}
$r = api('POST', 'op=current', $W, ['book' => 'Book']);
if (intdiv(time(), 3600) === $hour) {
    check($ok === 119 && is_error($r, 429, 'rate_limited'), "120 changes an hour, the 121st waits: $ok " . show($r['json']));
}

$rate = $data . '/_rate';
array_map('unlink', glob($rate . '/*'));
$Q = fixture_code();
$ok = 0;
for ($i = 1; $i <= 100; $i++) {
    if (save($Q, 'Book', "Unit $i", 1)['status'] === 200) $ok++;
}
$r = save($Q, 'Book', 'Unit 101', 1);
check($ok === 100 && is_error($r, 409, 'quota'), "100 lists per stick: $ok " . show($r['json']));

$S = fixture_code();
$ok = 0;
do {
    $r = save($S, 'Big', 'Unit ' . ($ok + 1), 500, 110);
    if ($r['status'] === 200) $ok++;
} while ($r['status'] === 200 && $ok < 20);
$bytes = array_sum(array_map('filesize', glob(dir_of($S) . '/*.json')));
check($ok >= 7 && $ok <= 9 && is_error($r, 409, 'quota'), "1 MB per stick: $ok big lists, then " . show($r['json']));
check($bytes < 1048576 + 200000, "list files stay near 1 MB: $bytes");

array_map('unlink', glob($rate . '/*'));
$day = gmdate('Ymd');
$ok = 0;
for ($i = 0; $i < 20; $i++) {
    if (save(fixture_code(), 'Book', 'Unit 1', 1)['status'] === 200) $ok++;
}
$r = save(fixture_code(), 'Book', 'Unit 1', 1);
if (gmdate('Ymd') === $day) {
    check($ok === 20 && is_error($r, 429, 'rate_limited'), "20 new sticks a day per IP: $ok " . show($r['json']));
}
$r = save($S, 'Big', 'Unit 1', 1);
check($r['status'] === 200 || is_error($r, 409, 'quota'), 'a stick that has a folder isn\'t held back by the new-stick limit');
$leak = false;
foreach (glob($rate . '/*') as $file) {
    if (str_contains(basename($file) . file_get_contents($file), '127.0.0.1')) $leak = true;
}
check(!$leak && is_file($data . '/_salt'), 'the IP is kept only as a salted hash');
check(is_file($data . '/.htaccess') && trim(file_get_contents($data . '/.htaccess')) === 'Require all denied', 'the data folder is closed to the web');

echo "voci api: $checks checks, $failures failed\n";
exit($failures ? 1 : 0);
