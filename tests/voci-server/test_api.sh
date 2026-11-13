#!/bin/bash
# Serves webpage2 with php -S and a temporary data folder, then runs test_api.php against it: the endpoint's
# requests and answers, its limits and privacy rules, and that the page is there. Called by run.sh.
# VOCI_SWEEP_ONE_IN=1 makes every request run the retention sweep, so the test can see it work.
set -e
cd "$(dirname "$0")"
TEMP="$(mktemp -d "${TMPDIR:-/tmp}/zitro_voci_api_XXXXXX")"
PORT="$(php -r '$s = stream_socket_server("tcp://127.0.0.1:0"); echo explode(":", stream_socket_get_name($s, false))[1];')"
VOCI_DATA_DIR="$TEMP/data" VOCI_SWEEP_ONE_IN=1 php -d upload_max_filesize=6M -d post_max_size=8M \
    -S "127.0.0.1:$PORT" -t ../../webpage2 >"$TEMP/server.log" 2>&1 &
SERVER=$!
trap 'status=$?; kill $SERVER 2>/dev/null; wait $SERVER 2>/dev/null || true; rm -rf "$TEMP"; exit $status' EXIT

php test_api.php "http://127.0.0.1:$PORT" "$TEMP/data" "$TEMP" || { echo "server log:"; tail -20 "$TEMP/server.log"; exit 1; }
