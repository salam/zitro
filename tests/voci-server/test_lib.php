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
// The VociError's message for a file, or '' if it was read
function message(string $name, string $bytes): string {
    try {
        voci_parse_file(fixture($name, $bytes), $name);
        return '';
    } catch (VociError $e) {
        return $e->getMessage();
    }
}
// A file read by parse_one.php in a PHP process with 32 MB of memory (or $memory): its JSON line as an array,
// or what the process printed instead (a fatal error when it ran out of memory). A process still reading after
// 10 seconds is stopped, so a slow parser fails the check instead of holding up the test.
function parseApart(string $name, string $bytes, string $memory = '32M'): array|string {
    return parsePathApart(fixture($name, $bytes), $memory);
}
function parsePathApart(string $path, string $memory = '32M', int $deadline = 10): array|string {
    $cmd = [PHP_BINARY, '-d', "memory_limit=$memory", __DIR__ . '/parse_one.php', $path];
    $proc = proc_open($cmd, [0 => ['file', '/dev/null', 'r'], 1 => ['pipe', 'w'], 2 => ['redirect', 1]], $pipes);
    if (!is_resource($proc)) return 'could not start php';
    $out = '';
    $end = microtime(true) + $deadline;
    while (!feof($pipes[1])) {
        $left = $end - microtime(true);
        $read = [$pipes[1]];
        $none = null;
        if ($left <= 0 || !stream_select($read, $none, $none, (int)$left, (int)(fmod($left, 1) * 1e6))) {
            proc_terminate($proc, 9);
            fclose($pipes[1]);
            proc_close($proc);
            return "still reading after $deadline s";
        }
        $out .= (string)fread($pipes[1], 65536);
    }
    fclose($pipes[1]);
    proc_close($proc);
    $out = trim($out);
    $r = json_decode($out, true);
    return is_array($r) ? $r : substr($out, 0, 300);
}
// Whether a file read apart stayed within 32 MB of memory and 2 seconds, with what it took
function cheap(array|string $r): bool {
    return is_array($r) && $r['peak'] < 32 * 1048576 && $r['seconds'] < 2;
}
function cost(array|string $r): string {
    return is_array($r) ? sprintf('%s, %.1f MB, %.2f s', $r['code'] ?? ($r['entries'] . ' entries'), $r['peak'] / 1048576, $r['seconds']) : $r;
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

// Reading stops after 500 filled rows and a header: the rest of a big file is never split up
$gappy = ['English,Deutsch'];
foreach (array_slice($rows, 0, 500) as $i => $row) {
    $gappy[] = $row;
    if ($i % 50 === 0) array_push($gappy, '', ',,', ' , ', "\t");
}
$r = parse('gappy.csv', implode("\n", $gappy) . "\n\n\n");
check(is_array($r) && count($r['entries']) === 500 && $r['entries'][499] === ['t' => 'word500', 'b' => 'Wort500'], 'a header, 500 entries and blank rows between them are fine: ' . (is_array($r) ? count($r['entries']) : $r));
$rows600 = [];
for ($i = 1; $i <= 600; $i++) $rows600[] = "word$i,Wort$i";
check(str_contains(message('600.csv', implode("\n", $rows600)), 'mehr als 500'), '600 rows: "mehr als 500": ' . message('600.csv', implode("\n", $rows600)));
voci_lang('en');
check(str_contains(message('600.csv', implode("\n", $rows600)), 'more than 500'), 'and "more than 500" in English: ' . message('600.csv', implode("\n", $rows600)));
voci_lang('de');
check(str_contains(message('big.csv', implode("\n", $rows)), '501 Einträge'), '501 rows still get their count: ' . message('big.csv', implode("\n", $rows)));
$half = array_slice($rows, 0, 495);
for ($i = 1; $i <= 10; $i++) $half[] = "only$i,";
$r = parse('half.csv', implode("\n", $half));
check($r === 'too_many', 'rows with one side filled count too: 495 pairs and 10 half rows are refused: ' . (is_array($r) ? count($r['entries']) : $r));
$cards = [];
for ($i = 1; $i <= 502; $i++) $cards[] = "word$i,Wort$i";
check(str_contains(message('quizlet-502.txt', implode(';', $cards)), 'mehr als 500'), '502 Quizlet cards on one line: "mehr als 500"');
$r = parse('quizlet-500.txt', implode(';', array_slice($cards, 0, 500)) . ';');
check(is_array($r) && count($r['entries']) === 500, '500 Quizlet cards are fine: ' . (is_array($r) ? count($r['entries']) : $r));
$r = parse('long-semi.csv', implode(';', array_map(fn($i) => "w$i", range(1, 1000))));
check(pairs($r) === ['w1|w2'], 'a line of 1000 parts without commas is no Quizlet export and no list too long: ' . show(pairs($r)));

// Hostile files of almost 5 MB, each in a process with 32 MB of memory
$r = parseApart('short-rows.csv', str_repeat("a,b\n", 1300000));
check(($r['code'] ?? null) === 'too_many' && str_contains($r['message'], 'mehr als 500'), '1.3 million short rows: too_many, not out of memory: ' . show($r));
$r = parseApart('quizlet-huge.txt', str_repeat("a,b;", 1300000));
check(($r['code'] ?? null) === 'too_many', '1.3 million Quizlet cards on one line: too_many: ' . show($r));
$r = parseApart('semicolons.txt', str_repeat(";;;;\n", 1000000));
check(($r['code'] ?? null) === 'no_entries', 'a million lines of ";;;;": no_entries: ' . show($r));
$r = parseApart('blank-rows.csv', str_repeat(",\n", 2500000));
check(($r['code'] ?? null) === 'no_entries', '2.5 million rows of ",": no_entries: ' . show($r));
$r = parseApart('blank-lines.csv', str_repeat("\n", 2500000) . str_repeat(" \n", 1000000) . "dog,Hund\ncat,Katze\n");
check(($r['entries'] ?? null) === 2, 'millions of blank lines before two pairs: ' . show($r));

// One very wide row: only the first 100 cells of a row are kept, the rest is skipped without being split up.
// These run with 128 MB, so a parser that needs more than 32 MB shows its numbers instead of just failing.
$r = parseApart('commas.csv', str_repeat(',', 5000000), '128M');
check(($r['code'] ?? null) === 'no_entries' && cheap($r), '5 MB of commas on one line: no_entries within 32 MB and 2 s: ' . cost($r));
$r = parseApart('wide-comma.csv', str_repeat('a,', 2600000), '128M');
check(($r['entries'] ?? null) === 1 && cheap($r), '2.6 million cells "a," on one line: one pair within 32 MB and 2 s: ' . cost($r));
$r = parseApart('wide-tab.txt', str_repeat("a\t", 2600000), '128M');
check(($r['entries'] ?? null) === 1 && cheap($r), '2.6 million tab-separated cells on one line: ' . cost($r));
$r = parseApart('wide-semi.txt', str_repeat('a;', 2600000), '128M');
check(($r['entries'] ?? null) === 1 && cheap($r), '2.6 million semicolon-separated cells on one line: ' . cost($r));
$path = apkg('wide.apkg', ['collection.anki2' => [[str_repeat("a\x1f", 2600000) . 'a']]]);
$r = parsePathApart($path, '128M');
check(($r['entries'] ?? null) === 1 && cheap($r), 'an apkg note with 2.6 million fields: ' . cost($r));
// Anki notes: reading stops at the 502nd note with anything in it, like rows of text (voci_add_row)
$path = apkg('many-notes.apkg', ['collection.anki2' => (function () {
    for ($i = 0; $i < 20000; $i++) yield array_fill(0, 100, 'ab');
})()]);
$r = parsePathApart($path, '128M');
check(($r['code'] ?? null) === 'too_many' && cheap($r), '20,000 notes of 100 fields: too_many within 32 MB and 2 s: ' . cost($r));
$apkgMessage = function (string $path): string {
    try {
        voci_parse_file($path, basename($path));
        return '';
    } catch (VociError $e) {
        return $e->getMessage();
    }
};
$notes = [['English', 'Deutsch']];
for ($i = 1; $i <= 500; $i++) {
    $notes[] = ["word$i", "Wort$i"];
    if ($i % 100 === 0) $notes[] = ['', ' '];
}
$r = parseApkg(apkg('notes-500.apkg', ['collection.anki2' => $notes]));
check(is_array($r) && count($r['entries']) === 500 && $r['entries'][499] === ['t' => 'word500', 'b' => 'Wort500'], 'a header note, 500 notes and blank notes between them are fine: ' . (is_array($r) ? count($r['entries']) : $r));
$notes = array_map(fn($i) => ["word$i", "Wort$i"], range(1, 502));
$m = $apkgMessage(apkg('notes-501.apkg', ['collection.anki2' => array_slice($notes, 0, 501)]));
check(str_contains($m, '501 Einträge'), '501 notes still get their count: ' . $m);
$m = $apkgMessage(apkg('notes-502.apkg', ['collection.anki2' => $notes]));
check(str_contains($m, 'mehr als 500'), '502 notes: "mehr als 500": ' . $m);
$r = parseApkg(apkg('blank-first.apkg', ['collection.anki2' => [['', ''], ['dog', 'Hund'], ['cat', '']]]));
check(pairs($r) === ['dog|Hund'] && count($r['warnings']) === 1 && str_contains($r['warnings'][0], 'Karte 3'), 'card numbers count blank notes: ' . show($r));

// One long Anki field: only its first 4 KB are read, and like any side over 120 characters it is left out
$path = apkg('long-field.apkg', ['collection.anki2' => [['dog', str_repeat('Hund ', 4000000)], ['cat', 'Katze']]]);
$r = parsePathApart($path, '128M');
check(($r['entries'] ?? null) === 1 && cheap($r), 'an apkg field of 20 MB: left out, the other card read within 32 MB and 2 s: ' . cost($r));
$r = parseApkg(apkg('long-field-100k.apkg', ['collection.anki2' => [['dog', str_repeat('Hund ', 20000)], ['cat', 'Katze']]]));
check(pairs($r) === ['cat|Katze'] && count($r['warnings']) === 1 && str_contains($r['warnings'][0], '120') && str_contains($r['warnings'][0], 'Karte 1'), 'a 100 KB field is left out as too long: ' . show($r['warnings'] ?? $r));
$r = parseApkg(apkg('styled.apkg', ['collection.anki2' => [['<span style="' . str_repeat('x', 3000) . '">dog</span>', 'Hund']]]));
check(pairs($r) === ['dog|Hund'], 'a word in 3 KB of markup is still read: ' . show(pairs($r)));
$r = parseApkg(apkg('long-first.apkg', ['collection.anki2' => [[str_repeat('a', 6000), 'Hund'], ['cat', 'Katze']]]));
check(pairs($r) === ['cat|Katze'] && count($r['warnings']) === 1 && str_contains($r['warnings'][0], '120'), 'the field after a 6 KB field is still read, so the card is too long, not one-sided: ' . show($r['warnings'] ?? $r));

// The most the limits still let through: 500 rows of 100 cells of 100 characters, about 5 MB
$wide = [];
for ($row = 1; $row <= 500; $row++) {
    $cells = [];
    for ($cell = 1; $cell <= 100; $cell++) $cells[] = str_pad("w{$row}x{$cell}", 100, 'a');
    $wide[] = implode(',', $cells);
}
$r = parseApart('wide-rows.csv', implode("\n", $wide), '128M');
check(($r['entries'] ?? null) === 500 && cheap($r), '500 rows of 100 cells, 5 MB: within 32 MB and 2 s: ' . cost($r));

// One very long cell: only its start is looked at for German signals
$r = parseApart('long-cell.csv', "dog,Hund\ncat," . str_repeat('x ', 2500000), '128M');
check(($r['entries'] ?? null) === 1 && cheap($r), 'a 5 MB cell: left out, the other pair read within 32 MB and 2 s: ' . cost($r));

// Quotes inside a long cell: whether one opens a quoted field is checked without copying the cell each time
$r = parseApart('quotes.csv', 'a, ' . str_repeat('x', 2500000) . str_repeat('"', 2500000), '128M');
check(($r['code'] ?? null) === 'no_entries' && cheap($r), 'a 2.5 MB cell followed by 2.5 million quotes: within 32 MB and 2 s: ' . cost($r));

// Rows wider than 100 cells still read like before: empty cells at the end (a spreadsheet's used range), and
// a quoted line break after the 100th cell still belongs to the row
$r = parse('trailing.csv', "en,de" . str_repeat(',', 200) . "\ndog,Hund" . str_repeat(',', 200) . "\ncat,Katze" . str_repeat(',', 150) . "\n");
check(pairs($r) === ['dog|Hund', 'cat|Katze'] && $r['warnings'] === [], 'rows with 200 empty cells at the end: ' . show($r));
$r = parse('wide-quoted.csv', "dog,Hund" . str_repeat(',', 150) . "\"x\ny\"\ncat,\nbird,Vogel\n");
check(pairs($r) === ['dog|Hund', 'bird|Vogel'] && count($r['warnings']) === 1 && str_contains($r['warnings'][0], 'Zeile 3'), 'a quoted line break after the 100th cell keeps the line numbers: ' . show($r));

// Anki headers are read without copying the text for each one, and at most 20 of them
$r = parseApart('headers.txt', str_repeat("#html:true\n", 470000), '128M');
check(isset($r['code']) && cheap($r), '5 MB of "#html:true" lines: an error within 32 MB and 2 s: ' . cost($r));
$r = parse('anki-20.txt', str_repeat("#separator:comma\n", 20) . "dog,Hund\ncat,Katze\n");
check(pairs($r) === ['dog|Hund', 'cat|Katze'] && $r['warnings'] === [], '20 Anki header lines: ' . show($r));
$r = parse('anki-21.txt', str_repeat("#separator:comma\n", 21) . "dog,Hund\ncat,Katze\n");
check(pairs($r) === ['dog|Hund', 'cat|Katze'] && count($r['warnings']) === 1 && str_contains($r['warnings'][0], 'Zeile 21'), 'a 21st header line is read as a row, like any other line starting with #: ' . show($r));
$r = parse('anki-lines.txt', "#separator:tab\n#html:true\n#deck column:3\ndog\tHund\tDeck\ncat\t\tDeck\n");
check(pairs($r) === ['dog|Hund'] && count($r['warnings']) === 1 && str_contains($r['warnings'][0], 'Zeile 5'), 'line numbers count the Anki headers: ' . show($r));

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
