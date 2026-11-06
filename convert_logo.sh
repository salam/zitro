#!/bin/bash
# Superseded: the boot splash is now the Zitro wordmark, generated from source.
# This script used to convert the old Firebird logo (designs/logo.png) and would overwrite it.
exec python3 "$(dirname "$0")/scripts/make_zitro_brand.py" "$@"
