<?php
// GET https://zitro.spot/api/locate.php: roughly where the asking stick is, from the IPv4 address it asks from,
// as a GeoNames place with its time zone rule: the same answer as place.php. The stick only asks when a parent
// switched on "Wohnort über zitro.spot bestimmen" (setting homeAuto). This script keeps nothing: the address is
// looked up and forgotten. Data: DB-IP Lite (CC BY 4.0), see scripts/make_ip_data.py.
require_once __DIR__ . '/locate_lib.php';

const LOCATE_DATA_DIR = __DIR__ . '/ipdata';
const LOCATE_ZONES = __DIR__ . '/places/zones.tsv';

ini_set('serialize_precision', '-1');
header('Content-Type: application/json; charset=utf-8');
header('Cache-Control: private, no-store');

if (!is_file(LOCATE_DATA_DIR . '/ip4.bin') || !is_file(LOCATE_ZONES)) {
    http_response_code(503);
    echo json_encode(['error' => 'no location data']);
    exit;
}
$place = locate_ipv4(LOCATE_DATA_DIR, LOCATE_ZONES, (string)($_SERVER['REMOTE_ADDR'] ?? ''));
echo json_encode($place ?? ['found' => false], JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES);
