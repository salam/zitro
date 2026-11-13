<?php
// https://zitro.spot/api/voci.php: the word lists parents upload for their stick's vocabulary trainer
// (docs/superpowers/specs/2026-11-13-vocab-trainer-design.md, 6.2). The page zitro.spot/voci uploads and
// manages them, the stick (sketch/sketch_jan12a/vocab.ino) fetches them. The stick's code comes only in the
// X-Voci-Code header, never in the address, so it isn't in any log; lists are kept under its SHA-256.
//   GET  ?op=index             {"current": "<book>", "lists": [{"id","book","name","lang","base","count","updated"}]}
//   GET  ?op=list&id=<id>      {"id","book","name","lang","base","entries":[{"t":"<English>","b":"<German>"}]}
//   POST ?op=parse             multipart "file" -> {"entries","book","name","swapped","warnings"}, nothing stored
//   POST ?op=save              JSON {"book","name","entries"[,"id" to replace]} -> the new index + "id", "warnings"
//   POST ?op=rename            JSON {"id"[,"name"][,"book"]}          -> the new index
//   POST ?op=delete            JSON {"id"}                            -> the new index
//   POST ?op=current           JSON {"book"} ("" for none)           -> the new index
//   POST ?op=wipe              everything of this stick              -> the empty index
// Errors: 4xx with {"error": "<code>", "message": "<text for parents>"}, in English with &lang=en.
// VOCI_DATA_DIR (environment) moves the data folder, for tests; VOCI_SWEEP_ONE_IN sets how often the
// retention sweep runs (default every 50th request). PHP must accept uploads of 5 MB (upload_max_filesize,
// post_max_size); smaller limits answer too_big earlier.
require_once __DIR__ . '/voci_lib.php';

const VOCI_STATUS = [
    'bad_code' => 400, 'bad_request' => 400, 'bad_name' => 400, 'no_file' => 400, 'no_entries' => 400,
    'unreadable' => 400, 'anki_new_format' => 400, 'too_many' => 400, 'too_big' => 413, 'not_found' => 404,
    'method' => 405, 'quota' => 409, 'rate_limited' => 429,
];
const VOCI_READS = ['index', 'list'];
const VOCI_WRITES = ['parse', 'save', 'rename', 'delete', 'current', 'wipe'];
const VOCI_MAX_BODY = 2097152;

header('Content-Type: application/json; charset=utf-8');
header('Cache-Control: no-store');
header('X-Content-Type-Options: nosniff');
header('Referrer-Policy: no-referrer');

function voci_reply(int $status, array $body): never {
    http_response_code($status);
    echo json_encode($body, VOCI_JSON);
    exit;
}

// The uploaded file, parsed. A body over post_max_size arrives without any fields: that's too_big too.
function voci_parse_upload(): array {
    $length = (int)($_SERVER['CONTENT_LENGTH'] ?? 0);
    if ($length > VOCI_MAX_UPLOAD + 65536 || ($length > voci_ini_bytes('post_max_size') && !$_FILES && !$_POST)) voci_fail('too_big');
    $file = $_FILES['file'] ?? null;
    if (!is_array($file) || !is_int($file['error'] ?? null)) voci_fail('no_file');
    if ($file['error'] === UPLOAD_ERR_INI_SIZE || $file['error'] === UPLOAD_ERR_FORM_SIZE) voci_fail('too_big');
    if ($file['error'] !== UPLOAD_ERR_OK || !is_uploaded_file($file['tmp_name'])) voci_fail('no_file');
    return voci_parse_file($file['tmp_name'], (string)$file['name']);
}

function voci_ini_bytes(string $key): int {
    $value = trim((string)ini_get($key));
    $number = (int)$value;
    return match (strtolower(substr($value, -1))) {
        'g' => $number * 1073741824,
        'm' => $number * 1048576,
        'k' => $number * 1024,
        default => $number,
    } ?: PHP_INT_MAX;
}

function voci_json_body(): array {
    $raw = file_get_contents('php://input', false, null, 0, VOCI_MAX_BODY + 1);
    if (is_string($raw) && strlen($raw) > VOCI_MAX_BODY) voci_fail('too_big');
    $body = is_string($raw) ? json_decode($raw, true) : null;
    if (!is_array($body)) voci_fail('bad_request');
    return $body;
}

voci_lang(($_GET['lang'] ?? '') === 'en' ? 'en' : 'de');
$data = getenv('VOCI_DATA_DIR') ?: __DIR__ . '/voci-data';
$now = time();

try {
    // The code belongs in the header only. Refuse it in the address, even next to a right header: a client
    // that sends it there is leaking it into logs.
    $query = rawurldecode((string)($_SERVER['QUERY_STRING'] ?? ''));
    if (isset($_GET['code']) || isset($_POST['code']) || preg_match('/[A-Za-z2-7]{26}/', $query)) voci_fail('bad_code');
    $code = $_SERVER['HTTP_X_VOCI_CODE'] ?? '';
    if (!is_string($code) || !voci_valid_code($code)) voci_fail('bad_code');

    $op = $_GET['op'] ?? '';
    $method = $_SERVER['REQUEST_METHOD'] ?? 'GET';
    if (!is_string($op) || !in_array($op, [...VOCI_READS, ...VOCI_WRITES], true)) voci_fail('bad_request');
    if ($method !== (in_array($op, VOCI_READS, true) ? 'GET' : 'POST')) voci_fail('method', 'bad_request');

    voci_prepare($data);
    if (random_int(1, max(1, (int)(getenv('VOCI_SWEEP_ONE_IN') ?: 50))) === 1) voci_sweep($data, $now);
    $dir = voci_code_dir($data, $code);
    $ip = (string)($_SERVER['REMOTE_ADDR'] ?? '');

    switch ($op) {
        case 'index':
            if (str_starts_with((string)($_SERVER['HTTP_USER_AGENT'] ?? ''), 'Zitro')) voci_mark_seen($dir, $now);
            voci_reply(200, voci_load($dir));
        case 'list':
            $id = $_GET['id'] ?? null;
            if (!is_string($id) || !preg_match(VOCI_ID_PATTERN, $id)) voci_fail('bad_request');
            $list = @file_get_contents($dir . '/' . $id . '.json');
            if (!is_string($list)) voci_fail('not_found');
            echo $list;
            exit;
        case 'parse':
            voci_count_parse($data, $ip, $now);
            voci_reply(200, voci_parse_upload());
        case 'wipe':
            voci_reply(200, voci_wipe($data, $code));
    }
    $req = voci_json_body();
    $out = match ($op) {
        'save' => voci_change($data, $code, $ip, true, $now, fn(array &$index, string $dir) => voci_op_save($index, $dir, $req, $now)),
        'rename' => voci_change($data, $code, $ip, false, $now, fn(array &$index, string $dir) => voci_op_rename($index, $dir, $req, $now)),
        'delete' => voci_change($data, $code, $ip, false, $now, fn(array &$index, string $dir) => voci_op_delete($index, $dir, $req)),
        'current' => voci_change($data, $code, $ip, false, $now, fn(array &$index, string $dir) => voci_op_current($index, $req)),
    };
    voci_reply(200, $out);
} catch (VociError $e) {
    voci_reply(VOCI_STATUS[$e->errorCode] ?? 400, ['error' => $e->errorCode === 'method' ? 'bad_request' : $e->errorCode, 'message' => $e->getMessage()]);
} catch (Throwable $e) {
    error_log('voci.php: ' . get_class($e) . ' at line ' . $e->getLine());  // Never the code or the lists
    voci_reply(500, ['error' => 'server', 'message' => voci_t('server')]);
}
