#!/bin/bash
# Tests the server half of the vocabulary trainer: the parsing and storage in webpage2/api/voci_lib.php on
# fixtures written by the test itself (no publisher content), then the endpoint webpage2/api/voci.php and the
# page webpage2/voci/ against php -S with a temporary data folder. Needs php (zip, sqlite3, mbstring) and curl.
set -e
cd "$(dirname "$0")"
TEMP="$(mktemp -d "${TMPDIR:-/tmp}/zitro_voci_test_XXXXXX")"
trap 'rm -rf "$TEMP"' EXIT

php test_lib.php "$TEMP/lib"
bash test_api.sh
