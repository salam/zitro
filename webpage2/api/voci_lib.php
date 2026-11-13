<?php
// Word lists for the vocabulary trainer (docs/superpowers/specs/2026-11-13-vocab-trainer-design.md, 6.2):
// reading what parents upload (CSV, TSV, Quizlet and Anki exports, Anki packages) into clean pairs, and
// keeping them per stick in a folder named after the SHA-256 of the stick's code. voci.php answers the page
// zitro.spot/voci and the stick; tests/voci-server/test_lib.php checks this. PHP 8.3 with zip, sqlite3 and
// mbstring (no zstd, no iconv).

const VOCI_MAX_UPLOAD = 5242880;      // Bytes in one uploaded file
const VOCI_MAX_ENTRIES = 500;         // Pairs in one list
const VOCI_MAX_SIDE = 120;            // Characters on one side of a pair
const VOCI_MAX_TITLE = 60;            // Characters in a book or list name
const VOCI_MAX_LISTS = 100;           // Lists per stick
const VOCI_MAX_BYTES = 1048576;       // Bytes of list files per stick
const VOCI_MAX_UNZIPPED = 67108864;   // Bytes of the collection inside an Anki package
const VOCI_MAX_NOTES = 20000;         // Notes read from an Anki package before giving up
const VOCI_NEW_DIRS_PER_DAY = 20;     // New sticks per IP hash and day
const VOCI_WRITES_PER_HOUR = 120;     // Changes per stick and hour
const VOCI_KEEP_DAYS = 365;           // Lists go after this long without a fetch by the stick or a change
const VOCI_CODE_PATTERN = '/^[A-Z2-7]{26}$/D';
const VOCI_ID_PATTERN = '/^[0-9a-f]{8}$/D';
const VOCI_JSON = JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES | JSON_INVALID_UTF8_SUBSTITUTE;

// Header labels and what they say about their column: a language, or just "the word" / "its meaning". A
// first row is a header only if its two labels say different things, so "word,Wort" stays an entry.
const VOCI_HEADER_LABELS = [
    'en' => 'en', 'english' => 'en', 'englisch' => 'en',
    'de' => 'de', 'deutsch' => 'de', 'german' => 'de',
    'term' => 'term', 'word' => 'term', 'wort' => 'term', 'front' => 'term', 'vorderseite' => 'term',
    'definition' => 'meaning', 'back' => 'meaning', 'rückseite' => 'meaning',
];
const VOCI_GERMAN_WORDS = ['der', 'die', 'das', 'den', 'dem', 'des', 'und', 'ist', 'ein', 'eine', 'einen', 'einer',
    'nicht', 'mit', 'ich', 'du', 'wir', 'sich', 'auf', 'zu', 'von', 'im'];
const VOCI_ENGLISH_WORDS = ['the', 'to', 'a', 'of', 'and', 'is', 'are', 'you', 'my', 'your', 'it', 'this', 'that', 'what', 'with'];
const VOCI_ANKI_SEPARATORS = ['tab' => "\t", 'comma' => ',', 'semicolon' => ';', 'pipe' => '|', 'space' => ' ', 'colon' => ':'];
const VOCI_ANKI_HEADERS = ['separator', 'html', 'tags', 'columns', 'notetype', 'deck', 'notetype column', 'deck column',
    'tags column', 'guid column', 'if matches'];

const VOCI_TEXTS = [
    'de' => [
        'bad_code' => 'Diese Seite kennt deinen Stick nicht. Sag zu Zitro «Wörterlisten hochladen» und scanne den QR-Code auf dem Stick nochmal.',
        'bad_request' => 'Das hat nicht geklappt. Lade die Seite neu und versuch es nochmal.',
        'server' => 'Auf dem Server ist etwas schiefgegangen. Versuch es in ein paar Minuten nochmal.',
        'no_file' => 'Es ist keine Datei angekommen. Wähle die Datei nochmal aus.',
        'too_big' => 'Die Datei ist grösser als 5 MB. Eine Wörterliste ist viel kleiner: Speichere nur die Liste als CSV-Datei und lade diese hoch.',
        'too_many' => 'Die Liste hat %d Einträge, erlaubt sind höchstens 500. Teile sie auf, zum Beispiel in eine Liste pro Unit.',
        'anki_new_format' => 'Diese Anki-Datei hat das neue Format, das wir nicht lesen können. Exportiere den Stapel in Anki nochmal und setze dabei das Häkchen bei «Ältere Anki-Versionen unterstützen» (englisch: «Support older Anki versions»).',
        'anki_broken' => 'In dieser Anki-Datei finden wir keine Karten. Exportiere den Stapel in Anki nochmal als Stapelpaket (.apkg) und setze dabei das Häkchen bei «Ältere Anki-Versionen unterstützen».',
        'spreadsheet' => 'Excel- und Numbers-Dateien können wir nicht direkt lesen. Speichere die Tabelle mit «Speichern unter» oder «Exportieren» als «CSV UTF-8» und lade diese Datei hoch.',
        'pdf' => 'PDF-Dateien können wir nicht lesen. Schreib oder kopiere die Wörter in eine Tabelle mit zwei Spalten (Englisch und Deutsch) und speichere sie als CSV-Datei.',
        'unreadable' => 'Diese Datei können wir nicht lesen. Speichere die Liste als CSV- oder Textdatei mit zwei Spalten: Englisch und Deutsch.',
        'one_column' => 'Wir finden nur eine Spalte. Jede Zeile braucht zwei Teile, getrennt durch Komma, Semikolon oder Tabulator: das englische Wort und die Übersetzung.',
        'no_entries' => 'In der Datei stehen keine Wortpaare. Jede Zeile braucht das englische Wort und die Übersetzung.',
        'bad_name' => 'Buch und Name dürfen nicht leer sein und höchstens 60 Zeichen haben.',
        'not_found' => 'Diese Liste gibt es nicht mehr. Lade die Seite neu.',
        'book_not_found' => 'Dieses Buch gibt es nicht mehr. Lade die Seite neu.',
        'rate_new' => 'Von diesem Internetanschluss aus wurden heute schon viele Sticks eingerichtet. Versuch es morgen wieder.',
        'rate_writes' => 'Das waren sehr viele Änderungen in kurzer Zeit. Warte eine Stunde und versuch es dann nochmal.',
        'quota_lists' => 'Auf diesem Stick sind schon 100 Listen. Lösche Listen, die ihr nicht mehr braucht, und speichere die neue dann nochmal.',
        'quota_bytes' => 'Der Platz für diesen Stick ist voll. Lösche Listen, die ihr nicht mehr braucht, und speichere die neue dann nochmal.',
        'warn_empty' => 'Weggelassen, weil eine Seite leer ist: %s.',
        'warn_long' => 'Weggelassen, weil eine Seite länger als 120 Zeichen ist: %s.',
        'warn_joined' => 'Mehr als zwei Spalten: Alles nach dem ersten Komma steht jetzt bei der Übersetzung. Bitte prüfen: %s.',
        'line' => 'Zeile %s', 'lines' => 'Zeilen %s', 'card' => 'Karte %s', 'cards' => 'Karten %s',
        'default_book' => 'Englisch',
        'default_name' => 'Liste',
    ],
    'en' => [
        'bad_code' => 'This page doesn\'t know your stick. Say “upload vocabulary” to Zitro and scan the QR code on the stick again.',
        'bad_request' => 'That didn\'t work. Reload the page and try again.',
        'server' => 'Something went wrong on the server. Try again in a few minutes.',
        'no_file' => 'No file arrived. Please choose the file again.',
        'too_big' => 'The file is larger than 5 MB. A word list is much smaller: save just the list as a CSV file and upload that.',
        'too_many' => 'The list has %d entries, at most 500 are allowed. Split it up, for example into one list per unit.',
        'anki_new_format' => 'This Anki file uses the new format, which we can\'t read. Export the deck again in Anki and tick “Support older Anki versions”.',
        'anki_broken' => 'We can\'t find any cards in this Anki file. Export the deck again in Anki as a deck package (.apkg) and tick “Support older Anki versions”.',
        'spreadsheet' => 'We can\'t read Excel or Numbers files directly. Use “Save as” or “Export” to save the table as “CSV UTF-8” and upload that file.',
        'pdf' => 'We can\'t read PDF files. Type or copy the words into a table with two columns (English and German) and save it as a CSV file.',
        'unreadable' => 'We can\'t read this file. Save the list as a CSV or text file with two columns: English and German.',
        'one_column' => 'We only found one column. Each line needs two parts, separated by a comma, semicolon or tab: the English word and its translation.',
        'no_entries' => 'There are no word pairs in this file. Each line needs the English word and its translation.',
        'bad_name' => 'Book and name can\'t be empty and can have at most 60 characters.',
        'not_found' => 'This list doesn\'t exist any more. Reload the page.',
        'book_not_found' => 'This book doesn\'t exist any more. Reload the page.',
        'rate_new' => 'Many sticks were already set up from this internet connection today. Please try again tomorrow.',
        'rate_writes' => 'That was a lot of changes in a short time. Wait an hour, then try again.',
        'quota_lists' => 'This stick already has 100 lists. Delete lists you don\'t need any more, then save the new one again.',
        'quota_bytes' => 'The space for this stick is full. Delete lists you don\'t need any more, then save the new one again.',
        'warn_empty' => 'Left out because one side is empty: %s.',
        'warn_long' => 'Left out because one side is longer than 120 characters: %s.',
        'warn_joined' => 'More than two columns: everything after the first comma is now part of the translation. Please check %s.',
        'line' => 'line %s', 'lines' => 'lines %s', 'card' => 'card %s', 'cards' => 'cards %s',
        'default_book' => 'English',
        'default_name' => 'List',
    ],
];

class VociError extends Exception {
    public string $errorCode;

    public function __construct(string $errorCode, string $message) {
        parent::__construct($message);
        $this->errorCode = $errorCode;
    }
}

// The language of messages and default names: 'de' (default) or 'en'. Sets it when given one.
function voci_lang(?string $set = null): string {
    static $lang = 'de';
    if ($set !== null) $lang = $set === 'en' ? 'en' : 'de';
    return $lang;
}

function voci_t(string $key, string|int ...$args): string {
    $text = VOCI_TEXTS[voci_lang()][$key] ?? VOCI_TEXTS['de'][$key];
    return $args ? vsprintf($text, $args) : $text;
}

function voci_fail(string $code, ?string $key = null, string|int ...$args): never {
    throw new VociError($code, voci_t($key ?? $code, ...$args));
}

// ---------- Reading an upload ----------

// An uploaded file as ['entries' => [['t', 'b'], ...], 'book', 'name', 'swapped', 'warnings'], t being the
// trained language (English) and b German. Throws VociError with a message for parents.
function voci_parse_file(string $path, string $filename): array {
    $size = @filesize($path);
    if ($size === false) voci_fail('no_file');
    if ($size > VOCI_MAX_UPLOAD) voci_fail('too_big');
    $bytes = (string)file_get_contents($path);
    if (str_starts_with($bytes, "PK\x03\x04")) {
        $table = voci_read_zip($path);
    } elseif (str_starts_with($bytes, '%PDF')) {
        voci_fail('unreadable', 'pdf');
    } elseif (str_starts_with($bytes, "\xD0\xCF\x11\xE0")) {
        voci_fail('unreadable', 'spreadsheet');  // Old Excel (.xls)
    } else {
        $text = voci_decode($bytes);
        if (str_contains($text, "\0")) voci_fail('unreadable');
        $table = voci_read_text($text);
    }
    [$pairs, $swapped, $warnings] = voci_table_pairs($table);
    [$entries, $more] = voci_validate_entries($pairs, $table['unit']);
    $names = voci_guess_names($filename);
    return [
        'entries' => $entries,
        'book' => $names['book'],
        'name' => $names['name'],
        'swapped' => $swapped,
        'warnings' => array_merge($warnings, $more),
    ];
}

// Text in UTF-8: UTF-8 with or without BOM, UTF-16 with BOM, anything else is taken as Windows-1252 (Excel).
function voci_decode(string $bytes): string {
    if (str_starts_with($bytes, "\xFF\xFE")) return mb_convert_encoding(substr($bytes, 2), 'UTF-8', 'UTF-16LE');
    if (str_starts_with($bytes, "\xFE\xFF")) return mb_convert_encoding(substr($bytes, 2), 'UTF-8', 'UTF-16BE');
    if (str_starts_with($bytes, "\xEF\xBB\xBF")) return mb_scrub(substr($bytes, 3), 'UTF-8');
    if (mb_check_encoding($bytes, 'UTF-8')) return $bytes;
    return mb_convert_encoding($bytes, 'UTF-8', 'Windows-1252');
}

// A text export as a table: ['rows' => [['cells', 'line', 'quoted'], ...], 'html', 'skip' (columns to
// ignore), 'join' (unquoted extra commas belong to the second column), 'unit' (what a row is called)].
function voci_read_text(string $text): array {
    $text = str_replace(["\r\n", "\r"], "\n", $text);
    $sep = null;
    $html = false;
    $skip = [];
    $line = 1;
    // Anki's text export starts with lines like "#separator:tab", "#html:true", "#deck column:3"
    while (preg_match('/^#([a-z ]+):([^\n]*)(?:\n|$)/i', $text, $m) && in_array(strtolower(trim($m[1])), VOCI_ANKI_HEADERS, true)) {
        $key = strtolower(trim($m[1]));
        $value = trim($m[2]);
        if ($key === 'separator') {
            $sep = VOCI_ANKI_SEPARATORS[strtolower($value)] ?? (strlen($value) === 1 ? $value : null);
        } elseif ($key === 'html') {
            $html = strtolower($value) === 'true';
        } elseif (str_ends_with($key, ' column') && ctype_digit($value) && (int)$value > 0) {
            $skip[] = (int)$value - 1;
        }
        $text = substr($text, strlen($m[0]));
        $line++;
    }
    if ($sep === null) {
        $cards = voci_quizlet_cards($text);
        if ($cards !== null) return ['rows' => $cards, 'html' => $html, 'skip' => [], 'join' => false, 'unit' => 'card'];
        if (trim($text) === '') return ['rows' => [], 'html' => $html, 'skip' => [], 'join' => false, 'unit' => 'line'];
        $sep = voci_guess_separator($text);
        if ($sep === null) voci_fail('unreadable', 'one_column');
    }
    return ['rows' => voci_split_rows($text, $sep, $line), 'html' => $html, 'skip' => $skip, 'join' => $sep === ',', 'unit' => 'line'];
}

// Quizlet can put all cards on one line with semicolons between them: "dog,Hund;cat,Katze;" or with tabs
// inside. The cards, or null if the text doesn't look like that.
function voci_quizlet_cards(string $text): ?array {
    if (!str_contains($text, ';')) return null;
    $lines = count(array_filter(explode("\n", $text), fn($l) => trim($l) !== ''));
    $parts = array_values(array_filter(array_map('trim', explode(';', $text)), fn($p) => $p !== ''));
    if (count($parts) < 2 || ($lines > 1 && count($parts) < 3 * $lines)) return null;
    foreach (["\t", ','] as $inner) {
        $every = true;
        foreach ($parts as $part) {
            if (!str_contains($part, $inner)) { $every = false; break; }
        }
        if (!$every) continue;
        $rows = [];
        foreach ($parts as $i => $part) {
            $rows[] = ['cells' => explode($inner, $part, 2), 'line' => $i + 1, 'quoted' => true];
        }
        return $rows;
    }
    return null;
}

// Tab, semicolon or comma: the one on the most of the first lines (outside quotes), in that order on a tie.
// Null if none of them is there.
function voci_guess_separator(string $text): ?string {
    $sample = [];
    foreach (explode("\n", $text) as $line) {
        if (trim($line) === '') continue;
        $sample[] = $line;
        if (count($sample) === 6) break;
    }
    $best = null;
    $bestLines = 0;
    foreach (["\t", ';', ','] as $sep) {
        $lines = 0;
        foreach ($sample as $line) {
            if (voci_count_outside_quotes($line, $sep) > 0) $lines++;
        }
        if ($lines > $bestLines) {
            $best = $sep;
            $bestLines = $lines;
        }
    }
    return $best;
}

function voci_count_outside_quotes(string $line, string $char): int {
    $count = 0;
    $quoted = false;
    for ($i = 0, $n = strlen($line); $i < $n; $i++) {
        if ($line[$i] === '"') $quoted = !$quoted;
        elseif ($line[$i] === $char && !$quoted) $count++;
    }
    return $count;
}

// Rows of delimited text per RFC 4180: a field in quotes may hold the separator, line breaks and "" for a
// quote. Spaces before an opening quote are allowed. Each row: ['cells', 'line' (where it starts), 'quoted'].
function voci_split_rows(string $text, string $sep, int $line = 1): array {
    $rows = [];
    $cells = [];
    $cell = '';
    $cellQuoted = false;
    $rowQuoted = false;
    $rowLine = $line;
    $stops = $sep . "\"\n";
    $n = strlen($text);
    $i = 0;
    while ($i < $n) {
        $run = strcspn($text, $stops, $i);
        if ($run > 0) {
            $cell .= substr($text, $i, $run);
            $i += $run;
            continue;
        }
        $c = $text[$i];
        if ($c === '"' && !$cellQuoted && trim($cell) === '') {
            $cellQuoted = $rowQuoted = true;
            $cell = '';
            $i++;
            while ($i < $n) {
                $q = strpos($text, '"', $i);
                $chunk = substr($text, $i, $q === false ? null : $q - $i);
                $cell .= $chunk;
                $line += substr_count($chunk, "\n");
                if ($q === false) {
                    $i = $n;
                } elseif ($q + 1 < $n && $text[$q + 1] === '"') {
                    $cell .= '"';
                    $i = $q + 2;
                    continue;
                } else {
                    $i = $q + 1;
                }
                break;
            }
        } elseif ($c === '"') {
            $cell .= '"';
            $i++;
        } elseif ($c === $sep) {
            $cells[] = $cell;
            $cell = '';
            $cellQuoted = false;
            $i++;
        } else {  // End of the row
            $cells[] = $cell;
            $rows[] = ['cells' => $cells, 'line' => $rowLine, 'quoted' => $rowQuoted];
            $cells = [];
            $cell = '';
            $cellQuoted = $rowQuoted = false;
            $rowLine = ++$line;
            $i++;
        }
    }
    if ($cell !== '' || $cells) {
        $cells[] = $cell;
        $rows[] = ['cells' => $cells, 'line' => $rowLine, 'quoted' => $rowQuoted];
    }
    return $rows;
}

// An Anki package (.apkg, .colpkg): the notes' fields as a table. Spreadsheets are zips too and get their
// own message.
function voci_read_zip(string $path): array {
    $zip = new ZipArchive();
    if ($zip->open($path, ZipArchive::RDONLY) !== true) voci_fail('unreadable');
    $tmp = null;
    try {
        $sizes = [];
        for ($i = 0; $i < $zip->numFiles; $i++) {
            $stat = $zip->statIndex($i);
            if ($stat) $sizes[$stat['name']] = $stat['size'];
        }
        // New exports keep the collection zstd-compressed in collection.anki21b and add a collection.anki2
        // that only says "please update Anki". Only collection.anki21 or a lone anki2 has the real cards.
        if (isset($sizes['collection.anki21'])) {
            $pick = 'collection.anki21';
        } elseif (isset($sizes['collection.anki21b'])) {
            voci_fail('anki_new_format');
        } elseif (isset($sizes['collection.anki2'])) {
            $pick = 'collection.anki2';
        } else {
            $mime = $zip->getFromName('mimetype');
            foreach (array_keys($sizes) as $name) {
                if (str_starts_with($name, 'xl/') || str_starts_with($name, 'Index/') || (is_string($mime) && str_contains($mime, 'spreadsheet'))) {
                    voci_fail('unreadable', 'spreadsheet');
                }
            }
            voci_fail('unreadable');
        }
        if ($sizes[$pick] > VOCI_MAX_UNZIPPED) voci_fail('unreadable', 'anki_broken');
        $tmp = tempnam(sys_get_temp_dir(), 'voci');
        $in = $zip->getStream($pick);
        $out = fopen($tmp, 'wb');
        $copied = ($in && $out) ? stream_copy_to_stream($in, $out, VOCI_MAX_UNZIPPED + 1) : false;
        if ($in) fclose($in);
        if ($out) fclose($out);
        if ($copied === false || $copied > VOCI_MAX_UNZIPPED) voci_fail('unreadable', 'anki_broken');
        return ['rows' => voci_read_anki_notes($tmp), 'html' => true, 'skip' => [], 'join' => false, 'unit' => 'card'];
    } finally {
        $zip->close();
        if ($tmp !== null) @unlink($tmp);
    }
}

// The fields of every note in an Anki collection (SQLite), oldest note first.
function voci_read_anki_notes(string $file): array {
    $rows = [];
    try {
        $db = new SQLite3($file, SQLITE3_OPEN_READONLY);
        $db->enableExceptions(true);
        try {
            $db->exec('PRAGMA trusted_schema = OFF');
        } catch (Exception $e) {
            // Older SQLite: nothing to switch off
        }
        // An uploaded file: "notes" must be an ordinary table, read in storage order with the cap inside SQLite,
        // so nothing in the file computes while it is read: no view (any query), no virtual table, no generated
        // column (an expression per row)
        $schema = $db->querySingle("SELECT sql FROM sqlite_master WHERE name = 'notes' AND type = 'table'");
        if (!is_string($schema) || stripos(ltrim($schema), 'CREATE TABLE') !== 0) voci_fail('unreadable', 'anki_broken');
        $columns = $db->query('PRAGMA table_xinfo(notes)');
        while (($column = $columns->fetchArray(SQLITE3_ASSOC)) !== false) {
            if ((int)($column['hidden'] ?? 0) !== 0) voci_fail('unreadable', 'anki_broken');
        }
        $result = $db->query('SELECT flds FROM notes ORDER BY rowid LIMIT ' . (VOCI_MAX_NOTES + 1));
        while (($note = $result->fetchArray(SQLITE3_NUM)) !== false) {
            $rows[] = ['cells' => explode("\x1f", (string)$note[0]), 'line' => count($rows) + 1, 'quoted' => true];
        }
        $db->close();
    } catch (Exception $e) {
        voci_fail('unreadable', 'anki_broken');
    }
    if (count($rows) > VOCI_MAX_NOTES) voci_fail('too_many', null, count($rows));
    return $rows;
}

// One cell as plain text: [sound:...] and tags removed, entities decoded in HTML exports (a line break
// becomes ", "), all kinds of spaces folded to one, zero-width characters dropped.
function voci_clean(string $s, bool $html = false): string {
    if (!mb_check_encoding($s, 'UTF-8')) $s = mb_scrub($s, 'UTF-8');
    $s = preg_replace('/\[sound:[^\]]*\]/', '', $s);
    if ($html) $s = preg_replace('~<br\s*/?>|</(?:div|p|li)\s*>~i', ', ', $s);
    $s = preg_replace('~<[a-zA-Z/!][^<>]*>~', '', $s);
    if ($html) $s = html_entity_decode($s, ENT_QUOTES | ENT_HTML5, 'UTF-8');
    $s = preg_replace('/[\x{200B}-\x{200D}\x{2060}\x{FEFF}\x{00AD}]/u', '', $s);
    $s = preg_replace('/[\s\x{00A0}\x{1680}\x{2000}-\x{200A}\x{2028}\x{2029}\x{202F}\x{205F}\x{3000}\p{Cc}]+/u', ' ', $s);
    $s = trim($s);
    if ($html) $s = trim(preg_replace('/\s*,(?:\s*,)+/', ',', $s), ' ,');
    return $s;
}

// The pairs in a table, before the limits: the first two columns that have anything in them, a header row
// dropped, the German column second. Returns [[['t', 'b', 'line'], ...], swapped, warnings].
function voci_table_pairs(array $table): array {
    $rows = [];
    foreach ($table['rows'] as $row) {
        $cells = [];
        foreach ($row['cells'] as $i => $cell) {
            if (!in_array($i, $table['skip'], true)) $cells[] = voci_clean($cell, $table['html']);
        }
        while ($cells && end($cells) === '') array_pop($cells);
        if (!$cells) continue;  // A blank line, or one like ",,"
        $rows[] = ['cells' => $cells, 'line' => $row['line'], 'quoted' => $row['quoted']];
    }
    if (!$rows) voci_fail('no_entries');

    $used = [];
    foreach ($rows as $row) {
        foreach ($row['cells'] as $i => $cell) {
            if ($cell !== '') $used[$i] = true;
        }
    }
    ksort($used);
    $columns = array_slice(array_keys($used), 0, 2);
    if (count($columns) < 2) voci_fail('unreadable', 'one_column');
    [$a, $b] = $columns;

    $swapped = null;
    $la = VOCI_HEADER_LABELS[mb_strtolower($rows[0]['cells'][$a] ?? '')] ?? null;
    $lb = VOCI_HEADER_LABELS[mb_strtolower($rows[0]['cells'][$b] ?? '')] ?? null;
    if ($la !== null && $lb !== null && $la !== $lb) {
        array_shift($rows);
        if ($la === 'de' || $lb === 'en') $swapped = true;
        elseif ($la === 'en' || $lb === 'de') $swapped = false;
    }

    // Quizlet's comma export doesn't quote: "rabbit,Kaninchen, Hase" has a comma too many. When most rows
    // end with the second column, the rest of a longer unquoted row belongs to it.
    $warnings = [];
    if ($table['join'] && $rows) {
        $counts = array_count_values(array_map(fn($row) => count($row['cells']), $rows));
        arsort($counts);
        if (array_key_first($counts) === $b + 1) {
            $joined = [];
            foreach ($rows as &$row) {
                if ($row['quoted'] || count($row['cells']) <= $b + 1) continue;
                $rest = array_filter(array_slice($row['cells'], $b), fn($cell) => $cell !== '');
                $row['cells'][$b] = implode(', ', $rest);
                $joined[] = $row['line'];
            }
            unset($row);
            if ($joined) $warnings[] = voci_t('warn_joined', voci_where($joined, $table['unit']));
        }
    }

    if ($swapped === null) {
        $scoreA = $scoreB = 0;
        foreach ($rows as $row) {
            $scoreA += voci_german_score($row['cells'][$a] ?? '');
            $scoreB += voci_german_score($row['cells'][$b] ?? '');
        }
        $swapped = $scoreA > $scoreB;
    }
    $pairs = [];
    foreach ($rows as $row) {
        $x = $row['cells'][$a] ?? '';
        $y = $row['cells'][$b] ?? '';
        $pairs[] = $swapped ? ['t' => $y, 'b' => $x, 'line' => $row['line']] : ['t' => $x, 'b' => $y, 'line' => $row['line']];
    }
    return [$pairs, $swapped, $warnings];
}

// How German a cell looks: umlauts and ß +2, a capitalised single word +1, a German function word +1, an
// English function word -1.
function voci_german_score(string $s): int {
    if ($s === '') return 0;
    $score = 0;
    if (preg_match('/[äöüßÄÖÜ]/u', $s)) $score += 2;
    if (!str_contains($s, ' ') && preg_match('/^\p{Lu}/u', $s)) $score += 1;
    $words = preg_split('/[^\p{L}]+/u', mb_strtolower($s), -1, PREG_SPLIT_NO_EMPTY) ?: [];
    if (array_intersect($words, VOCI_GERMAN_WORDS)) $score += 1;
    if (array_intersect($words, VOCI_ENGLISH_WORDS)) $score -= 1;
    return $score;
}

// "Zeile 4" / "Zeilen 2, 3" for a warning, at most ten numbers.
function voci_where(array $lines, string $unit): string {
    $text = implode(', ', array_slice($lines, 0, 10)) . (count($lines) > 10 ? ' …' : '');
    return voci_t(count($lines) === 1 ? $unit : $unit . 's', $text);
}

// Pairs as stored: each side cleaned, pairs with an empty side or a side over 120 characters left out with
// a warning, fully empty ones silently. Takes what the parser or the page sends: [['t', 'b'(, 'line')], ...].
// Returns [entries, warnings]; throws when nothing is left or more than 500 are.
function voci_validate_entries(array $entries, string $unit = 'line'): array {
    $clean = [];
    $empty = [];
    $long = [];
    foreach (array_values($entries) as $i => $entry) {
        if (!is_array($entry)) voci_fail('bad_request');
        $sides = [];
        foreach (['t', 'b'] as $key) {
            $value = $entry[$key] ?? '';
            if (is_int($value) || is_float($value)) $value = (string)$value;
            if (!is_string($value)) voci_fail('bad_request');
            $sides[$key] = voci_clean($value);
        }
        $where = is_int($entry['line'] ?? null) ? $entry['line'] : $i + 1;
        if ($sides['t'] === '' && $sides['b'] === '') continue;
        if ($sides['t'] === '' || $sides['b'] === '') {
            $empty[] = $where;
        } elseif (mb_strlen($sides['t']) > VOCI_MAX_SIDE || mb_strlen($sides['b']) > VOCI_MAX_SIDE) {
            $long[] = $where;
        } else {
            $clean[] = $sides;
        }
    }
    if (count($clean) > VOCI_MAX_ENTRIES) voci_fail('too_many', null, count($clean));
    if (!$clean) voci_fail('no_entries');
    $warnings = [];
    if ($empty) $warnings[] = voci_t('warn_empty', voci_where($empty, $unit));
    if ($long) $warnings[] = voci_t('warn_long', voci_where($long, $unit));
    return [$clean, $warnings];
}

// Book and list name from a file name: "young-world-3-unit-1.csv" is Young World 3 / Unit 1. The last unit
// word with its number is the name, the rest the book; without one the whole name is the book.
function voci_guess_names(string $filename): array {
    $base = basename(str_replace('\\', '/', $filename));
    $base = preg_replace('/\.[A-Za-z0-9]{1,6}$/', '', $base);
    $base = preg_replace('/\s*\(\d+\)$/', '', $base);  // "list (1).csv", a second download
    $base = trim(preg_replace('/[\s_.+-]+/u', ' ', $base));
    $book = $base;
    $name = voci_t('default_name');
    if (preg_match_all('/\b(unit|lektion|lesson|kapitel|chapter)\s*(\d+)\b/i', $base, $m, PREG_SET_ORDER | PREG_OFFSET_CAPTURE)) {
        $last = end($m);
        $name = ucfirst(strtolower($last[1][0])) . ' ' . (int)$last[2][0];
        $book = trim(substr($base, 0, $last[0][1]) . ' ' . substr($base, $last[0][1] + strlen($last[0][0])));
    }
    if ($book === '') $book = voci_t('default_book');
    return ['book' => voci_title(mb_substr($book, 0, VOCI_MAX_TITLE)), 'name' => $name];
}

// Each word's first letter upper case, the rest as it is ("YW3" stays).
function voci_title(string $s): string {
    return preg_replace_callback('/(^|\s)(\p{Ll})/u', fn($m) => $m[1] . mb_strtoupper($m[2]), $s);
}

// A book or list name as stored, or a VociError if it's empty or too long.
function voci_check_title(mixed $value): string {
    $title = is_string($value) ? voci_clean($value) : '';
    if ($title === '' || mb_strlen($title) > VOCI_MAX_TITLE) voci_fail('bad_name');
    return $title;
}

// ---------- Storage, one folder per stick ----------

function voci_valid_code(string $code): bool {
    return preg_match(VOCI_CODE_PATTERN, $code) === 1;
}

function voci_code_dir(string $data, string $code): string {
    return $data . '/' . hash('sha256', $code);
}

// The data folder, closed to the web even if its .htaccess didn't make it onto the server.
function voci_prepare(string $data): void {
    if (!is_dir($data)) @mkdir($data, 0700, true);
    if (!is_file($data . '/.htaccess')) @file_put_contents($data . '/.htaccess', "Require all denied\n");
}

// A stick's lists and current book: ['current' => '', 'lists' => []] if it has none.
function voci_load(string $dir): array {
    $text = @file_get_contents($dir . '/index.json');
    $lists = is_string($text) ? json_decode($text, true) : null;
    $current = @file_get_contents($dir . '/current');
    return ['current' => is_string($current) ? $current : '', 'lists' => is_array($lists) ? $lists : []];
}

function voci_write(string $path, string $bytes): void {
    $tmp = $path . '.tmp';
    if (file_put_contents($tmp, $bytes) !== strlen($bytes) || !rename($tmp, $path)) {
        @unlink($tmp);
        throw new RuntimeException('cannot write a list file');
    }
}

function voci_store_index(string $dir, array &$index): void {
    usort($index['lists'], fn($x, $y) => strnatcasecmp($x['book'], $y['book']) ?: strnatcasecmp($x['name'], $y['name']));
    voci_write($dir . '/index.json', json_encode($index['lists'], VOCI_JSON));
    voci_write($dir . '/current', $index['current']);
}

function voci_find(array $lists, mixed $id): ?int {
    if (!is_string($id) || !preg_match(VOCI_ID_PATTERN, $id)) return null;
    foreach ($lists as $i => $list) {
        if ($list['id'] === $id) return $i;
    }
    return null;
}

function voci_has_book(array $lists, string $book): bool {
    foreach ($lists as $list) {
        if ($list['book'] === $book) return true;
    }
    return false;
}

// The spelling a book already has on this stick ("young world 3" joins "Young World 3"), or the new one.
function voci_same_book(array $lists, string $book): string {
    $key = mb_strtolower($book);
    foreach ($lists as $list) {
        if (mb_strtolower($list['book']) === $key) return $list['book'];
    }
    return $book;
}

// Runs one change on a stick's lists under its lock and returns the new index plus what $change returns.
// Only a save may create the stick's folder; that counts against the IP's new folders of the day. Every
// change counts against the stick's changes of the hour.
function voci_change(string $data, string $code, string $ip, bool $create, int $now, callable $change): array {
    $dir = voci_code_dir($data, $code);
    if (!is_dir($dir)) {
        if (!$create) voci_fail('not_found');
        voci_count_new_dir($data, $ip, $now);
        if (!@mkdir($dir, 0700) && !is_dir($dir)) throw new RuntimeException('cannot make a list folder');
    }
    $lock = fopen($dir . '/lock', 'c');
    if (!$lock || !flock($lock, LOCK_EX)) throw new RuntimeException('cannot lock a list folder');
    try {
        if (!is_dir($dir)) voci_fail('not_found');  // Wiped while waiting
        voci_count_write($dir, $now);
        $index = voci_load($dir);
        $extra = $change($index, $dir);
        voci_store_index($dir, $index);
        return ['current' => $index['current'], 'lists' => $index['lists']] + $extra;
    } finally {
        flock($lock, LOCK_UN);
        fclose($lock);
    }
}

function voci_count_write(string $dir, int $now): void {
    $hour = intdiv($now, 3600);
    $writes = json_decode((string)@file_get_contents($dir . '/writes'), true);
    $count = is_array($writes) && ($writes['h'] ?? null) === $hour ? (int)($writes['n'] ?? 0) : 0;
    if ($count >= VOCI_WRITES_PER_HOUR) voci_fail('rate_limited', 'rate_writes');
    file_put_contents($dir . '/writes', json_encode(['h' => $hour, 'n' => $count + 1]));
}

// New stick folders per day and IP. The IP is kept only as a salted hash, files older than two days go.
function voci_count_new_dir(string $data, string $ip, int $now): void {
    $rate = $data . '/_rate';
    if (!is_dir($rate)) @mkdir($rate, 0700, true);
    foreach (glob($rate . '/*') ?: [] as $file) {
        if (@filemtime($file) < $now - 2 * 86400) @unlink($file);
    }
    $file = $rate . '/' . gmdate('Ymd', $now) . '-' . substr(hash_hmac('sha256', $ip, voci_salt($data)), 0, 32);
    $handle = fopen($file, 'c+');
    if (!$handle || !flock($handle, LOCK_EX)) throw new RuntimeException('cannot count new folders');
    try {
        $count = (int)stream_get_contents($handle);
        if ($count >= VOCI_NEW_DIRS_PER_DAY) voci_fail('rate_limited', 'rate_new');
        ftruncate($handle, 0);
        rewind($handle);
        fwrite($handle, (string)($count + 1));
    } finally {
        flock($handle, LOCK_UN);
        fclose($handle);
    }
}

function voci_salt(string $data): string {
    $file = $data . '/_salt';
    $salt = @file_get_contents($file);
    if (is_string($salt) && strlen($salt) >= 32) return $salt;
    $new = bin2hex(random_bytes(32));
    $handle = @fopen($file, 'x');
    if ($handle) {
        fwrite($handle, $new);
        fclose($handle);
        return $new;
    }
    $salt = @file_get_contents($file);  // Another request made it first
    return is_string($salt) && $salt !== '' ? $salt : $new;
}

// Saves a list, new or replacing the one with the request's id. Returns ['id', 'warnings'].
function voci_op_save(array &$index, string $dir, array $req, int $now): array {
    $book = voci_check_title($req['book'] ?? null);
    $name = voci_check_title($req['name'] ?? null);
    if (!is_array($req['entries'] ?? null)) voci_fail('bad_request');
    [$entries, $warnings] = voci_validate_entries($req['entries']);
    $id = $req['id'] ?? null;
    $at = null;
    if ($id !== null) {
        $at = voci_find($index['lists'], $id);
        if ($at === null) voci_fail('not_found');
    } else {
        if (count($index['lists']) >= VOCI_MAX_LISTS) voci_fail('quota', 'quota_lists');
        do {
            $id = bin2hex(random_bytes(4));
        } while (voci_find($index['lists'], $id) !== null);
    }
    $book = voci_same_book($index['lists'], $book);
    $file = json_encode(['id' => $id, 'book' => $book, 'name' => $name, 'lang' => 'en', 'base' => 'de', 'entries' => $entries], VOCI_JSON);
    $others = 0;
    foreach ($index['lists'] as $list) {
        if ($list['id'] !== $id) $others += (int)@filesize($dir . '/' . $list['id'] . '.json');
    }
    if ($others + strlen($file) > VOCI_MAX_BYTES) voci_fail('quota', 'quota_bytes');
    voci_write($dir . '/' . $id . '.json', $file);
    $entry = ['id' => $id, 'book' => $book, 'name' => $name, 'lang' => 'en', 'base' => 'de', 'count' => count($entries), 'updated' => $now];
    if ($at === null) $index['lists'][] = $entry;
    else $index['lists'][$at] = $entry;
    if (!voci_has_book($index['lists'], $index['current'])) $index['current'] = $book;
    return ['id' => $id, 'warnings' => $warnings];
}

// Renames a list and/or moves it to another book. The current book follows when its last list moves.
function voci_op_rename(array &$index, string $dir, array $req, int $now): array {
    $at = voci_find($index['lists'], $req['id'] ?? null);
    if ($at === null) voci_fail('not_found');
    $list = $index['lists'][$at];
    $book = voci_check_title(array_key_exists('book', $req) ? $req['book'] : $list['book']);
    $name = voci_check_title(array_key_exists('name', $req) ? $req['name'] : $list['name']);
    $others = $index['lists'];
    unset($others[$at]);
    $book = voci_same_book($others, $book);
    $path = $dir . '/' . $list['id'] . '.json';
    $file = json_decode((string)@file_get_contents($path), true);
    if (!is_array($file)) voci_fail('not_found');
    $file['book'] = $book;
    $file['name'] = $name;
    voci_write($path, json_encode($file, VOCI_JSON));
    $index['lists'][$at] = array_merge($list, ['book' => $book, 'name' => $name, 'updated' => $now]);
    if ($index['current'] === $list['book'] && !voci_has_book($index['lists'], $list['book'])) $index['current'] = $book;
    return [];
}

// Deletes a list. If that was the current book's last list, the only book left (if just one) is current.
function voci_op_delete(array &$index, string $dir, array $req): array {
    $at = voci_find($index['lists'], $req['id'] ?? null);
    if ($at === null) voci_fail('not_found');
    @unlink($dir . '/' . $index['lists'][$at]['id'] . '.json');
    array_splice($index['lists'], $at, 1);
    if (!voci_has_book($index['lists'], $index['current'])) {
        $books = array_values(array_unique(array_column($index['lists'], 'book')));
        $index['current'] = count($books) === 1 ? $books[0] : '';
    }
    return [];
}

// Makes a book the current one ("" for none).
function voci_op_current(array &$index, array $req): array {
    $book = $req['book'] ?? null;
    if (!is_string($book)) voci_fail('bad_request');
    $book = voci_clean($book);
    if ($book !== '') {
        $book = voci_same_book($index['lists'], $book);
        if (!voci_has_book($index['lists'], $book)) voci_fail('not_found', 'book_not_found');
    }
    $index['current'] = $book;
    return [];
}

// Everything of one stick, gone.
function voci_wipe(string $data, string $code): array {
    $dir = voci_code_dir($data, $code);
    if (is_dir($dir)) {
        $lock = fopen($dir . '/lock', 'c');
        if ($lock) flock($lock, LOCK_EX);
        voci_remove_dir($dir);
        if ($lock) {
            flock($lock, LOCK_UN);
            fclose($lock);
        }
    }
    return ['current' => '', 'lists' => []];
}

function voci_remove_dir(string $dir): void {
    foreach (scandir($dir) ?: [] as $name) {
        if ($name !== '.' && $name !== '..') @unlink($dir . '/' . $name);
    }
    @rmdir($dir);
}

// The stick fetched its index: its lists stay for another year.
function voci_mark_seen(string $dir, int $now): void {
    if (is_dir($dir)) @file_put_contents($dir . '/seen', (string)$now, LOCK_EX);
}

// Deletes the lists of sticks that neither fetched nor changed them for a year.
function voci_sweep(string $data, int $now): void {
    $limit = $now - VOCI_KEEP_DAYS * 86400;
    clearstatcache();
    foreach (scandir($data) ?: [] as $name) {
        if (!preg_match('/^[0-9a-f]{64}$/D', $name)) continue;
        $dir = $data . '/' . $name;
        if (!is_dir($dir)) continue;
        $last = max((int)@file_get_contents($dir . '/seen'), (int)@filemtime($dir . '/index.json'), (int)@filemtime($dir));
        if ($last < $limit) voci_remove_dir($dir);
    }
}
