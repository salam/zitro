<?php
// Reads one uploaded file in a PHP process of its own, for test_lib.php: a file that runs out of memory ends
// only this process, not the whole test. Prints one JSON line: {"code", "message"} for a VociError, or
// {"entries"} with the count, and "peak" (bytes of memory used).
//   php -d memory_limit=32M parse_one.php <file>
require_once __DIR__ . '/../../webpage2/api/voci_lib.php';

try {
    $out = ['entries' => count(voci_parse_file($argv[1], basename($argv[1]))['entries'])];
} catch (VociError $e) {
    $out = ['code' => $e->errorCode, 'message' => $e->getMessage()];
}
echo json_encode($out + ['peak' => memory_get_peak_usage()], VOCI_JSON), "\n";
