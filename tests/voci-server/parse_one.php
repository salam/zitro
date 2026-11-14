<?php
// Reads one uploaded file in a PHP process of its own, for test_lib.php: a file that runs out of memory or
// time ends only this process, not the whole test. Prints one JSON line: {"code", "message"} for a VociError,
// or {"entries"} with the count, plus "peak" (bytes of memory taken from the system) and "seconds" (time spent
// reading the file, without PHP's start).
//   php -d memory_limit=32M parse_one.php <file>
require_once __DIR__ . '/../../webpage2/api/voci_lib.php';

$start = hrtime(true);
try {
    $out = ['entries' => count(voci_parse_file($argv[1], basename($argv[1]))['entries'])];
} catch (VociError $e) {
    $out = ['code' => $e->errorCode, 'message' => $e->getMessage()];
}
$seconds = (hrtime(true) - $start) / 1e9;
echo json_encode($out + ['peak' => memory_get_peak_usage(true), 'seconds' => round($seconds, 3)], VOCI_JSON), "\n";
