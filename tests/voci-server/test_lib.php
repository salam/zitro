<?php
// Tests the word-list parsing and storage in webpage2/api/voci_lib.php. Every fixture is written here for
// the test (no publisher content): CSV, TSV, Quizlet and Anki exports, Anki packages built with ZipArchive
// and SQLite3. Run by tests/voci-server/run.sh with an empty temp folder as the first argument.
require_once __DIR__ . '/../../webpage2/api/voci_lib.php';
require_once __DIR__ . '/fixtures.php';

$dir = $argv[1] ?? (sys_get_temp_dir() . '/voci_lib_' . getmypid());
@mkdir($dir, 0700, true);
$checks = 0;
$failures = 0;
function check(bool $ok, string $what): void {
    global $checks, $failures;
    $checks++;
    if (!$ok) { $failures++; fwrite(STDERR, "  FAIL $what\n"); }
}
function fixture(string $name, string $bytes): string {
    global $dir;
    $path = $dir . '/' . $name;
    file_put_contents($path, $bytes);
    return $path;
}
// The parse result, or the VociError's code as a string
function parse(string $name, string $bytes): array|string {
    try {
        return voci_parse_file(fixture($name, $bytes), $name);
    } catch (VociError $e) {
        return $e->errorCode;
    }
}
function pairs(array|string $r): array {
    if (!is_array($r)) return [];
    return array_map(fn($e) => $e['t'] . '|' . $e['b'], $r['entries']);
}
function show($r): string {
    return json_encode($r, JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES);
}
function apkg(string $name, array $collections, array $files = []): string {
    global $dir;
    return fixture_apkg($dir . '/' . $name, $collections, $files);
}
function parseApkg(string $path): array|string {
    try {
        return voci_parse_file($path, basename($path));
    } catch (VociError $e) {
        return $e->errorCode;
    }
}

// ---------- Delimited text ----------

$rows = ["en,de"];
for ($i = 1; $i <= 37; $i++) $rows[] = "word$i,Wort$i";
$r = parse('young-world-3-unit-1.csv', implode("\n", $rows) . "\n");
check(is_array($r) && count($r['entries']) === 37, 'UTF-8 CSV: 37 entries, header dropped: ' . (is_array($r) ? count($r['entries']) : $r));
check(is_array($r) && $r['entries'][0] === ['t' => 'word1', 'b' => 'Wort1'] && $r['entries'][36] === ['t' => 'word37', 'b' => 'Wort37'], 'in file order');
check(is_array($r) && $r['swapped'] === false && $r['warnings'] === [], 'not swapped, no warnings: ' . show($r['warnings'] ?? null));
check(is_array($r) && $r['book'] === 'Young World 3' && $r['name'] === 'Unit 1', 'names from the file name');
check(is_array($r) && array_keys($r) === ['entries', 'book', 'name', 'swapped', 'warnings'], 'result shape');

$r = parse('animals.tsv', "dog\tHund\ncat\tKatze, Kater\nmouse\tMaus\n");
check(pairs($r) === ['dog|Hund', 'cat|Katze, Kater', 'mouse|Maus'], 'TSV, comma inside a cell: ' . show($r));

$r = parse('crlf.csv', "en,de\r\ndog,Hund\r\ncat,Katze\r\n");
check(pairs($r) === ['dog|Hund', 'cat|Katze'], 'CRLF line ends: ' . show($r));

$r = parse('excel.csv', mb_convert_encoding("Käse;cheese\nHund;dog\ndie Katze;cat\nÄpfel;apples\n", 'Windows-1252', 'UTF-8'));
check(is_array($r) && $r['entries'][0] === ['t' => 'cheese', 'b' => 'Käse'], 'Excel Windows-1252 semicolons: umlauts survive, German goes to b: ' . show($r));
check(is_array($r) && $r['swapped'] === true, 'German first column is swapped');
check(pairs($r) === ['cheese|Käse', 'dog|Hund', 'cat|die Katze', 'apples|Äpfel'], 'all rows swapped: ' . show($r));

$r = parse('one.csv', mb_convert_encoding("Käse;cheese\n", 'Windows-1252', 'UTF-8'));
check(pairs($r) === ['cheese|Käse'] && $r['swapped'] === true, 'a single German-first row: ' . show($r));

$r = parse('bom.csv', "\xEF\xBB\xBFen;de\nto go;gehen\ncheese;Käse\n");
check(pairs($r) === ['to go|gehen', 'cheese|Käse'], 'UTF-8 with BOM, header dropped: ' . show($r));

$r = parse('unicode.txt', "\xFF\xFE" . mb_convert_encoding("English\tGerman\r\ngreen\tgrün\r\nwhite\tweiss\r\n", 'UTF-16LE', 'UTF-8'));
check(pairs($r) === ['green|grün', 'white|weiss'], 'UTF-16 LE with BOM (Excel Unicode text): ' . show($r));

$r = parse('be.txt', "\xFE\xFF" . mb_convert_encoding("green\tgrün\nfive\tfünf\n", 'UTF-16BE', 'UTF-8'));
check(pairs($r) === ['green|grün', 'five|fünf'], 'UTF-16 BE with BOM: ' . show($r));

$r = parse('quoted.csv', "en,de\nrabbit,\"Kaninchen, Hase\"\n\"say \"\"hello\"\"\",\"«Hallo» sagen\"\n\"two\nlines\",zwei Zeilen\nlast,letzte\n");
check(pairs($r) === ['rabbit|Kaninchen, Hase', 'say "hello"|«Hallo» sagen', 'two lines|zwei Zeilen', 'last|letzte'], 'RFC 4180 quotes, doubled quotes, line break in a field: ' . show($r));

$r = parse('semi-in-quotes.csv', "dog,\"Hund; Köter\"\ncat,Katze\nmouse,Maus\n");
check(pairs($r) === ['dog|Hund; Köter', 'cat|Katze', 'mouse|Maus'], 'a semicolon inside quotes does not make it the delimiter: ' . show($r));

$r = parse('semi-with-commas.csv', "rabbit;Kaninchen, Hase\nbranch;Ast, Zweig\n");
check(pairs($r) === ['rabbit|Kaninchen, Hase', 'branch|Ast, Zweig'], 'semicolon before comma: ' . show($r));

$r = parse('spaces.csv', "dog, Hund\ncat, \"Katze, Kater\"\n");
check(pairs($r) === ['dog|Hund', 'cat|Katze, Kater'], 'a space before a quoted field: ' . show($r));

$r = parse('lead.csv', ";dog;Hund\n;cat;Katze\n");
check(pairs($r) === ['dog|Hund', 'cat|Katze'], 'first two non-empty columns: ' . show($r));

$r = parse('three.csv', "dog,Hund,The dog barks.\ncat,Katze,\nmouse,Maus,A small mouse.\n");
check(pairs($r) === ['dog|Hund', 'cat|Katze', 'mouse|Maus'], 'a third column is ignored: ' . show($r));

$r = parse('quizlet-comma.txt', "dog,Hund\nrabbit,Kaninchen, Hase\ncat,Katze\n");
check(pairs($r) === ['dog|Hund', 'rabbit|Kaninchen, Hase', 'cat|Katze'], 'unquoted comma in a two-column comma file joins the rest: ' . show($r));
check(is_array($r) && count($r['warnings']) === 1 && str_contains($r['warnings'][0], '2'), 'and warns with the line: ' . show($r['warnings'] ?? null));

$r = parse('white.csv', "  big \xC2\xA0 dog  ,\tgrosser   Hund \nzero\xE2\x80\x8Bwidth,Null\n");
check(pairs($r) === ['big dog|grosser Hund', 'zerowidth|Null'], 'whitespace folded, NBSP and zero width: ' . show($r));

$r = parse('html.csv', "<b>dog</b>,Hund<br>\na < b,a kleiner b\n");
check(pairs($r) === ['dog|Hund', 'a < b|a kleiner b'], 'tags stripped in any file, a lone < kept: ' . show($r));

$r = parse('marks.csv', "There is/are …,Es gibt …\ndeer (pl. deer),Hirsch\npupil,Schüler/-in\n");
check(pairs($r) === ['There is/are …|Es gibt …', 'deer (pl. deer)|Hirsch', 'pupil|Schüler/-in'], 'slashes, brackets and … stay as they are: ' . show($r));

// ---------- Headers and which column is which ----------

$r = parse('word.csv', "word,Wort\ndog,Hund\n");
check(pairs($r) === ['word|Wort', 'dog|Hund'], 'word/Wort is a real entry, not a header: ' . show($r));

$r = parse('header-en.csv', "English;German\nto run;rennen\n");
check(pairs($r) === ['to run|rennen'] && $r['swapped'] === false, 'English;German header: ' . show($r));

$r = parse('header-de.csv', "Deutsch,Englisch\nlaufen,to walk\nrennen,to run\n");
check(pairs($r) === ['to walk|laufen', 'to run|rennen'] && $r['swapped'] === true, 'Deutsch,Englisch header swaps: ' . show($r));

$r = parse('header-front.csv', "Front\tBack\nHund\tdog\nKatze\tcat\n");
check(pairs($r) === ['dog|Hund', 'cat|Katze'] && $r['swapped'] === true, 'Front/Back header, German first by content: ' . show($r));

$r = parse('header-vorder.csv', "Vorderseite;Rückseite\nbook;Buch\n");
check(pairs($r) === ['book|Buch'], 'Vorderseite;Rückseite header: ' . show($r));

$r = parse('unsure.csv', "gehen,laufen\nsingen,tanzen\n");
check(is_array($r) && $r['swapped'] === false && $r['entries'][0]['t'] === 'gehen', 'unsure stays as it is: ' . show($r));

$r = parse('verbs.csv', "gehen,to go\nsehen,to see\nmachen,to make\n");
check(is_array($r) && $r['swapped'] === true && $r['entries'][0] === ['t' => 'to go', 'b' => 'gehen'], 'lower-case German verbs first, English "to" second: ' . show($r));

$r = parse('sentences.csv', "Wie geht es dir?,How are you?\nDas ist mein Hund.,This is my dog.\n");
check(is_array($r) && $r['swapped'] === true, 'German sentences first: ' . show($r));

// ---------- Quizlet ----------

$r = parse('quizlet.txt', "dog\tHund\ncat\tKatze\ncheese\tKäse\n");
check(pairs($r) === ['dog|Hund', 'cat|Katze', 'cheese|Käse'], 'Quizlet tab and new line: ' . show($r));

$r = parse('quizlet-semi.txt', "dog,Hund;cat,Katze;cheese,Käse;");
check(pairs($r) === ['dog|Hund', 'cat|Katze', 'cheese|Käse'], 'Quizlet comma and semicolon between cards: ' . show($r));

$r = parse('quizlet-semi-tab.txt', "dog\tHund;cat\tKatze, Kater;cheese\tKäse");
check(pairs($r) === ['dog|Hund', 'cat|Katze, Kater', 'cheese|Käse'], 'Quizlet tab and semicolon between cards: ' . show($r));

// ---------- Anki text export ----------

$r = parse('anki.txt', "#separator:tab\n#html:true\n<b>dog</b>\tHund\nsalt &amp; pepper\tSalz &amp; Pfeffer\ncat [sound:cat.mp3]\tKatze<br>Kater\nnice&nbsp;day\tschöner Tag\n");
check(pairs($r) === ['dog|Hund', 'salt & pepper|Salz & Pfeffer', 'cat|Katze, Kater', 'nice day|schöner Tag'], 'Anki text export with html: ' . show($r));

$r = parse('anki-cols.txt', "#separator:Semicolon\n#html:false\n#guid column:1\n#notetype column:2\n#deck column:3\nabc123;Basic;English;dog;Hund\nabc124;Basic;English;cat;Katze\n");
check(pairs($r) === ['dog|Hund', 'cat|Katze'], 'Anki export with guid, note type and deck columns: ' . show($r));

$r = parse('anki-pipe.txt', "#separator:pipe\ndog|Hund\ncat|Katze\n");
check(pairs($r) === ['dog|Hund', 'cat|Katze'], 'Anki pipe separator: ' . show($r));

$r = parse('hash.csv', "#1,Nummer eins\ndog,Hund\n");
check(pairs($r) === ['#1|Nummer eins', 'dog|Hund'], 'a line starting with # that is not an Anki header stays: ' . show($r));

// ---------- Anki packages ----------

$r = parseApkg(apkg('old.apkg', ['collection.anki2' => [['dog', 'Hund']]]));
check(pairs($r) === ['dog|Hund'], 'apkg with collection.anki2: ' . show($r));

$r = parseApkg(apkg('fields.apkg', ['collection.anki2' => [['<div>dog</div>', 'Hund [sound:hund.mp3]', 'extra'], ['cat&nbsp;', 'Katze<br/>Kater'], ['only']]]));
check(pairs($r) === ['dog|Hund', 'cat|Katze, Kater'], 'apkg fields: html, sound, third field ignored: ' . show($r));
check(is_array($r) && count($r['warnings']) === 1, 'apkg note with one field warns: ' . show($r['warnings'] ?? null));

$r = parseApkg(apkg('both.apkg', ['collection.anki21' => [['cat', 'Katze']], 'collection.anki2' => [['Please update', 'Bitte aktualisieren']]]));
check(pairs($r) === ['cat|Katze'], 'collection.anki21 wins over anki2: ' . show($r));

$r = parseApkg(fixture_apkg_view($dir . '/view.apkg'));
check($r === 'unreadable', 'apkg whose notes is an endless view is refused: ' . show($r));

$r = parseApkg(fixture_apkg_generated($dir . '/generated.apkg'));
check($r === 'unreadable', 'apkg whose notes computes a column on read is refused: ' . show($r));

$r = parseApkg(fixture_apkg_new($dir . '/new.apkg'));
check($r === 'anki_new_format', 'apkg in the new format with its placeholder anki2: ' . show($r));

$r = parseApkg(apkg('newonly.apkg', [], ['collection.anki21b' => "\x28\xB5\x2F\xFDzstd"]));
check($r === 'anki_new_format', 'apkg with only collection.anki21b: ' . show($r));

$r = parseApkg(apkg('deck.colpkg', ['collection.anki21' => [['red', 'rot'], ['blue', 'blau']]]));
check(pairs($r) === ['red|rot', 'blue|blau'], '.colpkg: ' . show($r));

$r = parseApkg(apkg('broken.apkg', [], ['collection.anki2' => 'not a database']));
check($r === 'unreadable', 'apkg with a broken collection: ' . show($r));

// ---------- Files that aren't word lists ----------

$xlsx = $dir . '/list.xlsx';
$zip = new ZipArchive();
$zip->open($xlsx, ZipArchive::CREATE | ZipArchive::OVERWRITE);
$zip->addFromString('[Content_Types].xml', '<Types/>');
$zip->addFromString('xl/workbook.xml', '<workbook/>');
$zip->close();
try {
    voci_parse_file($xlsx, 'list.xlsx');
    check(false, 'xlsx is refused');
} catch (VociError $e) {
    check($e->errorCode === 'unreadable' && str_contains($e->getMessage(), 'CSV'), 'xlsx: unreadable, says to save as CSV: ' . $e->getMessage());
}
$r = parse('book.pdf', "%PDF-1.7\n%binary\n");
check($r === 'unreadable', 'PDF is unreadable: ' . show($r));
$r = parse('old.xls', "\xD0\xCF\x11\xE0\xA1\xB1\x1A\xE1" . str_repeat("\0", 100));
check($r === 'unreadable', 'old Excel is unreadable: ' . show($r));
$r = parse('one-column.txt', "dog\ncat\nmouse\n");
check($r === 'unreadable', 'one column only: ' . show($r));
$r = parse('empty.csv', '');
check($r === 'no_entries', 'empty file: ' . show($r));
$r = parse('header-only.csv', "en,de\n");
check($r === 'no_entries', 'header only: ' . show($r));
$r = parse('binary.bin', "dog\tHund\n\0\0\0\x01\x02");
check($r === 'unreadable', 'binary bytes: ' . show($r));

// ---------- Limits ----------

$r = parse('gaps.csv', "dog,Hund\ncat,\n,Maus\n,\n,,\n\nbird,Vogel\n");
check(pairs($r) === ['dog|Hund', 'bird|Vogel'], 'rows with an empty side skipped: ' . show($r));
check(is_array($r) && count($r['warnings']) === 1 && str_contains($r['warnings'][0], '2') && str_contains($r['warnings'][0], '3'), 'one warning naming lines 2 and 3, fully empty rows silent: ' . show($r['warnings'] ?? null));

$rows = [];
for ($i = 1; $i <= 501; $i++) $rows[] = "word$i,Wort$i";
$r = parse('big.csv', implode("\n", $rows));
check($r === 'too_many', '501 rows are too many: ' . show($r));
$r = parse('ok.csv', implode("\n", array_slice($rows, 0, 500)));
check(is_array($r) && count($r['entries']) === 500, '500 rows are fine');

$long = str_repeat('a', 130);
$umlauts = str_repeat('ä', 120);
$r = parse('long.csv', "dog,Hund\n$long,lang\ncat,$umlauts\n");
check(pairs($r) === ['dog|Hund', "cat|$umlauts"], 'a 130-character side is skipped, 120 umlauts (240 bytes) are kept: ' . count(pairs($r)));
check(is_array($r) && count($r['warnings']) === 1 && str_contains($r['warnings'][0], '120'), 'and warns: ' . show($r['warnings'] ?? null));

$big = fixture('huge.csv', str_repeat("dog,Hund\n", (int)(VOCI_MAX_UPLOAD / 9) + 2));
try {
    voci_parse_file($big, 'huge.csv');
    check(false, 'over 5 MB is refused');
} catch (VociError $e) {
    check($e->errorCode === 'too_big', 'over 5 MB: too_big');
}

// ---------- Names from the file name ----------

check(voci_guess_names('young-world-3-unit-1.csv') === ['book' => 'Young World 3', 'name' => 'Unit 1'], 'young-world-3-unit-1.csv');
check(voci_guess_names('Lektion 4 Tiere.txt') === ['book' => 'Tiere', 'name' => 'Lektion 4'], 'Lektion 4 Tiere.txt: ' . show(voci_guess_names('Lektion 4 Tiere.txt')));
check(voci_guess_names('animals.csv') === ['book' => 'Animals', 'name' => 'Liste'], 'animals.csv');
check(voci_guess_names('YW3_Unit01 (1).csv') === ['book' => 'YW3', 'name' => 'Unit 1'], 'YW3_Unit01 (1).csv: ' . show(voci_guess_names('YW3_Unit01 (1).csv')));
check(voci_guess_names('Englisch Kapitel 2 und Lesson 5.tsv') === ['book' => 'Englisch Kapitel 2 Und', 'name' => 'Lesson 5'], 'the last unit word wins: ' . show(voci_guess_names('Englisch Kapitel 2 und Lesson 5.tsv')));
check(voci_guess_names('unit 3.csv') === ['book' => 'Englisch', 'name' => 'Unit 3'], 'no book in the name: ' . show(voci_guess_names('unit 3.csv')));
check(voci_guess_names('über_tiere.csv') === ['book' => 'Über Tiere', 'name' => 'Liste'], 'umlaut at the start: ' . show(voci_guess_names('über_tiere.csv')));
check(voci_guess_names('../../etc/chapter-7.csv') === ['book' => 'Englisch', 'name' => 'Chapter 7'], 'path parts dropped');

// ---------- Validation (what save gets back from the page) ----------

[$entries, $warnings] = voci_validate_entries([['t' => '  dog ', 'b' => "Hund\n"], ['t' => '<i>cat</i>', 'b' => 'Katze'], ['t' => 'x', 'b' => ''], ['t' => 'y']]);
check($entries === [['t' => 'dog', 'b' => 'Hund'], ['t' => 'cat', 'b' => 'Katze']], 'validate trims and strips: ' . show($entries));
check(count($warnings) === 1, 'validate warns once about the empty sides: ' . show($warnings));
try {
    voci_validate_entries(array_fill(0, 501, ['t' => 'a', 'b' => 'b']));
    check(false, 'validate refuses 501');
} catch (VociError $e) {
    check($e->errorCode === 'too_many', 'validate refuses 501: ' . $e->errorCode);
}
try {
    voci_validate_entries([['t' => '', 'b' => '']]);
    check(false, 'validate refuses nothing');
} catch (VociError $e) {
    check($e->errorCode === 'no_entries', 'validate refuses nothing: ' . $e->errorCode);
}
try {
    voci_validate_entries(['dog']);
    check(false, 'validate refuses a non-pair');
} catch (VociError $e) {
    check($e->errorCode === 'bad_request', 'validate refuses a non-pair: ' . $e->errorCode);
}

// ---------- Codes and English texts ----------

check(voci_valid_code('ABCDEFGHIJKLMNOPQRSTUVWXYZ') && voci_valid_code('AAAAAAAAAAAAAAAAAAAAAAAAAA') && voci_valid_code('234567ABCDEFGHIJKLMNOPQRST'), 'valid codes');
foreach (['', 'ABCDEFGHIJKLMNOPQRSTUVWXY', 'ABCDEFGHIJKLMNOPQRSTUVWXYZA', 'abcdefghijklmnopqrstuvwxyz', 'ABCDEFGHIJKLMNOPQRSTUVWXY1', "ABCDEFGHIJKLMNOPQRSTUVWXYZ\n"] as $bad) {
    check(!voci_valid_code($bad), 'refuses code ' . json_encode($bad));
}
for ($i = 0; $i < 20; $i++) {
    if (!voci_valid_code(fixture_code())) check(false, 'the test codes look like the stick\'s');
}
voci_lang('en');
try {
    voci_parse_file($dir . '/new.apkg', 'new.apkg');
} catch (VociError $e) {
    check(str_contains($e->getMessage(), 'Support older Anki versions'), 'English Anki message: ' . $e->getMessage());
}
check(voci_guess_names('animals.csv') === ['book' => 'Animals', 'name' => 'List'], 'English default name');
voci_lang('de');
try {
    voci_parse_file($dir . '/new.apkg', 'new.apkg');
} catch (VociError $e) {
    check(str_contains($e->getMessage(), 'Ältere Anki-Versionen unterstützen'), 'German Anki message: ' . $e->getMessage());
}

// ---------- Retention ----------

$data = $dir . '/data';
@mkdir($data . '/_rate', 0700, true);
file_put_contents($data . '/_salt', 'salt');
$old = $data . '/' . hash('sha256', 'OLDOLDOLDOLDOLDOLDOLDOLDOL');
$seenOnly = $data . '/' . hash('sha256', 'SEENSEENSEENSEENSEENSEENSE');
$fresh = $data . '/' . hash('sha256', 'NEWNEWNEWNEWNEWNEWNEWNEWNE');
$now = time();
foreach ([$old, $seenOnly, $fresh] as $d) {
    @mkdir($d, 0700, true);
    file_put_contents($d . '/index.json', '[]');
    file_put_contents($d . '/abcd1234.json', '{}');
    touch($d . '/index.json', $now - 400 * 86400);
    touch($d . '/abcd1234.json', $now - 400 * 86400);
}
file_put_contents($old . '/seen', (string)($now - 400 * 86400));
file_put_contents($seenOnly . '/seen', (string)($now - 30 * 86400));
touch($fresh . '/index.json', $now - 10 * 86400);
foreach ([$old, $seenOnly, $fresh] as $d) touch($d, $now - 400 * 86400);
touch($data . '/_rate', $now - 400 * 86400);
voci_sweep($data, $now);
check(!is_dir($old), 'a list folder unused for a year is deleted');
check(is_dir($seenOnly), 'one the stick fetched a month ago stays');
check(is_dir($fresh), 'one written ten days ago stays');
check(is_dir($data . '/_rate') && is_file($data . '/_salt'), 'the rate folder and salt stay');

// ---------- Previews per IP and hour ----------

$rateData = $dir . '/rate-data';
@mkdir($rateData, 0700, true);
$parseError = function (string $ip, int $at) use ($rateData) {
    try {
        voci_count_parse($rateData, $ip, $at, 2);
        return null;
    } catch (VociError $e) {
        return $e->errorCode;
    }
};
$hour = 1794600000;
check($parseError('192.0.2.1', $hour) === null && $parseError('192.0.2.1', $hour) === null, 'two previews within the limit');
check($parseError('192.0.2.1', $hour) === 'rate_limited', 'the third preview in the hour is refused');
check($parseError('192.0.2.9', $hour) === null, 'another connection still may');
check($parseError('192.0.2.1', $hour + 3600) === null, 'the next hour starts again');
check(!str_contains(implode(' ', glob($rateData . '/_rate/*')), '192.0.2'), 'the IP is not in the counter file names');

echo "voci lib: $checks checks, $failures failed\n";
exit($failures ? 1 : 0);
