<?php
// GET https://zitro.spot/api/place.php?q=knoxville[&r=tennessee]: where a place is and which time zone rule
// it follows, for the stick's world clock (sketch/sketch_jan12a/feature_worldclock.ino) when a place isn't
// in its own table. q and r are already folded by the stick to the letters a-z.
require_once __DIR__ . '/place_lib.php';

const PLACE_DATA_DIR = __DIR__ . '/places';

ini_set('serialize_precision', '-1');
header('Content-Type: application/json; charset=utf-8');

$q = $_GET['q'] ?? '';
$r = $_GET['r'] ?? '';
if (!is_string($q) || !is_string($r) || !preg_match(PLACE_KEY_PATTERN, $q) || ($r !== '' && !preg_match(PLACE_KEY_PATTERN, $r))) {
    http_response_code(400);
    echo json_encode(['error' => 'bad name']);
    exit;
}
if (!is_file(PLACE_DATA_DIR . '/zones.tsv')) {
    http_response_code(503);
    echo json_encode(['error' => 'no place data']);
    exit;
}
header('Cache-Control: public, max-age=86400');
$place = place_find(PLACE_DATA_DIR, $q, $r);
echo json_encode($place ?? ['found' => false], JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES);
