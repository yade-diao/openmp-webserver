#!/usr/bin/env bash
set -u -o pipefail

BASE_URL="${1:-http://127.0.0.1:8080}"
UPLOAD_CLIENTS="${2:-100}"
DOWNLOAD_CLIENTS="${3:-200}"
SECONDS="${4:-20}"
UPLOAD_FILE="bench_upload.bin"
UPLOAD_CONTENT="hello_from_webbench"

if ! command -v webbench >/dev/null 2>&1; then
  echo "webbench not found. install it first."
  exit 1
fi

# 先准备一个下载目标文件（上传一次）
curl -fsS "${BASE_URL}/upload?name=${UPLOAD_FILE}&content=${UPLOAD_CONTENT}" >/dev/null

echo "[upload] using GET /upload for webbench compatibility"
set +e
webbench -c "${UPLOAD_CLIENTS}" -t "${SECONDS}" "${BASE_URL}/upload?name=${UPLOAD_FILE}&content=${UPLOAD_CONTENT}"
UPLOAD_RC=$?
set -e
echo "[upload] webbench_exit_code=${UPLOAD_RC}"

echo "[download] concurrent public download"
set +e
webbench -c "${DOWNLOAD_CLIENTS}" -t "${SECONDS}" "${BASE_URL}/files/${UPLOAD_FILE}"
DOWNLOAD_RC=$?
set -e
echo "[download] webbench_exit_code=${DOWNLOAD_RC}"

exit 0
