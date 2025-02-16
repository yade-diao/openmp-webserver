#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PORT_BASE="${1:-18080}"
UPLOAD_CLIENTS="${2:-60}"
DOWNLOAD_CLIENTS="${3:-100}"
SECONDS="${4:-15}"
PROCESS_ROUNDS="${5:-128}"

if ! command -v webbench >/dev/null 2>&1; then
  echo "webbench not found."
  exit 1
fi

if [ ! -x "${REPO_ROOT}/build-linux/openmp_webserver" ]; then
  echo "binary not found: ${REPO_ROOT}/build-linux/openmp_webserver"
  echo "please build first: cmake -S . -B build-linux && cmake --build build-linux -j"
  exit 1
fi

extract_speed() {
  local file="$1"
  grep -m1 "Speed=" "$file" | sed -E 's/.*Speed=([0-9]+) pages\/min.*/\1/'
}

run_case() {
  local mode="$1"
  local port="$2"
  local out_file
  out_file="$(mktemp)"

  echo "\n=== CASE: ${mode} (port=${port}, rounds=${PROCESS_ROUNDS}) ==="

  "${REPO_ROOT}/build-linux/openmp_webserver" "${port}" 8 et reactor \
    "${REPO_ROOT}/static" "${REPO_ROOT}/webserver.db" "${REPO_ROOT}/server.log" async "${mode}" "${PROCESS_ROUNDS}" \
    >/tmp/openmp_webserver_${mode}.log 2>&1 &
  local server_pid=$!

  sleep 1

  bash "${REPO_ROOT}/tests/webbench_upload_download.sh" "http://127.0.0.1:${port}" "${UPLOAD_CLIENTS}" "${DOWNLOAD_CLIENTS}" "${SECONDS}" | tee "${out_file}"

  kill "${server_pid}" >/dev/null 2>&1 || true
  wait "${server_pid}" 2>/dev/null || true

  local upload_speed
  local download_speed
  upload_speed="$(grep -n "\[upload\]" -n "${out_file}" >/dev/null 2>&1; grep "Speed=" "${out_file}" | sed -n '1p' | sed -E 's/.*Speed=([0-9]+) pages\/min.*/\1/')"
  download_speed="$(grep "Speed=" "${out_file}" | sed -n '2p' | sed -E 's/.*Speed=([0-9]+) pages\/min.*/\1/')"

  echo "upload_speed_pages_per_min=${upload_speed}"
  echo "download_speed_pages_per_min=${download_speed}"

  rm -f "${out_file}"
}

run_case noomp "${PORT_BASE}"
run_case omp "$((PORT_BASE + 1))"
