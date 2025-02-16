#!/usr/bin/env bash
set -u -o pipefail

BASE_URL="${1:-http://127.0.0.1:8080}"
CLIENTS="${2:-200}"
SECONDS="${3:-30}"

if ! command -v webbench >/dev/null 2>&1; then
  echo "webbench not found. install it first."
  exit 1
fi

echo "[access] base_url=${BASE_URL} clients=${CLIENTS} seconds=${SECONDS}"
set +e
webbench -c "${CLIENTS}" -t "${SECONDS}" "${BASE_URL}/files/not_found.bin"
WB_RC=$?
set -e

echo "[access] webbench_exit_code=${WB_RC}"
exit 0
