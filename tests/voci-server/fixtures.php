<?php
// Fixture builders shared by test_lib.php and test_api.php. Everything here is made up for the tests.

// An Anki package at $path: a zip with SQLite collections (entry name => notes, each a list of fields) and
// plain files (entry name => bytes), like Anki's own exports.
function fixture_apkg(string $path, array $collections, array $files = []): string {
    @unlink($path);
    $zip = new ZipArchive();
    $zip->open($path, ZipArchive::CREATE | ZipArchive::OVERWRITE);
    $dbs = [];
    foreach ($collections as $entry => $notes) {
        $db = $path . '.' . $entry . '.db';
        @unlink($db);
        $sql = new SQLite3($db);
        $sql->exec('CREATE TABLE notes (id integer primary key, guid text, mid integer, mod integer, usn integer, tags text, flds text, sfld text, csum integer, flags integer, data text)');
        $id = 1500000000000;
        foreach ($notes as $fields) {
            $st = $sql->prepare("INSERT INTO notes VALUES (:id, :g, 1, 0, 0, '', :f, '', 0, 0, '')");
            $st->bindValue(':id', $id, SQLITE3_INTEGER);
            $st->bindValue(':g', 'g' . $id++, SQLITE3_TEXT);
            $st->bindValue(':f', implode("\x1f", $fields), SQLITE3_TEXT);
            $st->execute();
        }
        $sql->close();
        $zip->addFile($db, $entry);
        $dbs[] = $db;
    }
    foreach ($files as $entry => $bytes) $zip->addFromString($entry, $bytes);
    $zip->addFromString('media', '{}');
    $zip->close();
    foreach ($dbs as $db) @unlink($db);
    return $path;
}

// A new-format Anki export: zstd collection (only its magic here) and the placeholder anki2 Anki adds.
function fixture_apkg_new(string $path): string {
    return fixture_apkg($path, ['collection.anki2' => [['This file requires a newer version of Anki.', '']]], ['collection.anki21b' => "\x28\xB5\x2F\xFDzstd"]);
}

// A random stick code like the stick makes: 16 bytes in base32 (RFC 4648, no padding), 26 characters.
function fixture_code(): string {
    $alphabet = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ234567';
    $bits = '';
    foreach (str_split(random_bytes(16)) as $byte) $bits .= str_pad(decbin(ord($byte)), 8, '0', STR_PAD_LEFT);
    $code = '';
    foreach (str_split($bits, 5) as $chunk) $code .= $alphabet[bindec(str_pad($chunk, 5, '0'))];
    return $code;
}
