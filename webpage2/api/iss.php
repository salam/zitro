<?php
// GET https://zitro.spot/api/iss.php: where the ISS is now and its track around now, for the stick's live
// ISS view (sketch/sketch_jan12a/feature_iss.ino). Computed from NASA's own ephemeris, which this keeps
// in a cache next to it and refreshes at most every 12 hours. No input is read from the request.
require_once __DIR__ . '/iss_lib.php';

const ISS_SOURCE_URL = 'https://nasa-public-data.s3.amazonaws.com/iss-coords/current/ISS_OEM/ISS.OEM_J2K_EPH.txt';
const ISS_CACHE_FILE = __DIR__ . '/cache/iss_oem.txt';
const ISS_REFRESH_S = 12 * 3600;
const ISS_FILE_LIMIT = 4 * 1024 * 1024;

ini_set('serialize_precision', '-1');  // Shortest exact numbers: 425.8, not 425.80000000000001
header('Content-Type: application/json; charset=utf-8');
header('Cache-Control: public, max-age=60');

function iss_fail(int $code, string $why): void {
    http_response_code($code);
    echo json_encode(['error' => $why]);
    exit;
}

function iss_download(): ?string {
    $ch = curl_init(ISS_SOURCE_URL);
    curl_setopt_array($ch, [
        CURLOPT_RETURNTRANSFER => true,
        CURLOPT_FOLLOWLOCATION => true,
        CURLOPT_CONNECTTIMEOUT => 5,
        CURLOPT_TIMEOUT => 20,
        CURLOPT_USERAGENT => 'zitro.spot ISS track',
    ]);
    $body = curl_exec($ch);
    $code = curl_getinfo($ch, CURLINFO_RESPONSE_CODE);
    if (!is_string($body) || $code !== 200 || strlen($body) > ISS_FILE_LIMIT) return null;
    return iss_parse_oem($body) === null ? null : $body;
}

// The cached file, refreshed when it's old or no longer covers the next hours. One request refreshes at a
// time; the others keep using the old file meanwhile.
function iss_load(float $now): ?array {
    $dir = dirname(ISS_CACHE_FILE);
    if (!is_dir($dir)) @mkdir($dir, 0755, true);
    if (!is_file($dir . '/.htaccess')) @file_put_contents($dir . '/.htaccess', "Require all denied\n");
    $text = is_file(ISS_CACHE_FILE) ? file_get_contents(ISS_CACHE_FILE) : false;
    $oem = is_string($text) ? iss_parse_oem($text) : null;
    $stale = $oem === null || $now - filemtime(ISS_CACHE_FILE) > ISS_REFRESH_S || $oem['stop'] < $now + 86400;
    if ($stale) {
        $lock = @fopen(ISS_CACHE_FILE . '.lock', 'c');
        if ($lock && flock($lock, LOCK_EX | LOCK_NB)) {
            $fresh = iss_download();
            if ($fresh !== null) {
                file_put_contents(ISS_CACHE_FILE . '.tmp', $fresh);
                rename(ISS_CACHE_FILE . '.tmp', ISS_CACHE_FILE);
                $oem = iss_parse_oem($fresh);
            } else {
                @touch(ISS_CACHE_FILE);  // Don't hammer NASA while it's unreachable; try again in 12 hours
            }
            flock($lock, LOCK_UN);
        }
        if ($lock) fclose($lock);
    }
    return $oem;
}

$now = microtime(true);
$oem = iss_load($now);
if ($oem === null) iss_fail(503, 'no trajectory data');
$track = iss_track($oem, $now);
if ($track === null) iss_fail(503, 'trajectory data does not cover now');
$track['now'] = (int)$now;
echo json_encode($track, JSON_UNESCAPED_SLASHES);
