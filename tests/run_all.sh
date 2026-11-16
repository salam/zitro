#!/bin/bash
# Runs every host test suite (tests/*/run.sh, and tests/*/run.py with python3) and says which passed. No stick needed.
# The gate before a branch is merged: this, then ./verify.sh (both sticks compile), then the stick test.
# Usage: tests/run_all.sh [suite ...]   (no names: all suites). Logs go to $TMPDIR/zitro_tests/<suite>.log
cd "$(dirname "$0")"
LOGS="${TMPDIR:-/tmp}/zitro_tests"
mkdir -p "$LOGS"
if [ $# -gt 0 ]; then suites=("$@"); else suites=(); for d in */run.sh */run.py; do suites+=("${d%/run.*}"); done; fi
failed=()
for s in "${suites[@]}"; do
  start=$(date +%s)
  if [ -f "$s/run.sh" ]; then run=(bash "$s/run.sh"); else run=(python3 "$s/run.py"); fi
  if "${run[@]}" > "$LOGS/$s.log" 2>&1; then
    printf "  ok    %-12s %3ss\n" "$s" "$(( $(date +%s) - start ))"
  else
    printf "  FAIL  %-12s %3ss  (%s)\n" "$s" "$(( $(date +%s) - start ))" "$LOGS/$s.log"
    failed+=("$s")
  fi
done
if [ ${#failed[@]} -gt 0 ]; then
  echo "${#failed[@]} of ${#suites[@]} suites failed: ${failed[*]}"
  exit 1
fi
echo "All ${#suites[@]} suites passed."
