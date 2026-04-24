#!/usr/bin/env bash
# download-pdfium.sh
#
# Downloads the pre-built PDFium DLL for Windows x64 from the
# bblanchon/pdfium-binaries GitHub releases and places it at:
#   src-tauri/binaries/pdfium.dll
#
# Source: https://github.com/bblanchon/pdfium-binaries
# The pdfium-render crate (https://crates.io/crates/pdfium-render) documents
# that it binds against the latest release from this repository.
#
# Usage:
#   bash src-tauri/scripts/download-pdfium.sh
#
# Requirements (Windows / Git Bash):
#   - curl  (included in Git for Windows)
#   - tar   (included in Git for Windows >= 2.23 / Windows 10 1803+)
#
# The script is safe to re-run: it skips the download if pdfium.dll
# already exists in the target directory.

set -euo pipefail

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

# Resolve this script's location so it works regardless of cwd
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BINARIES_DIR="${REPO_ROOT}/src-tauri/binaries"
TARGET_DLL="${BINARIES_DIR}/pdfium.dll"

# Latest confirmed release tag from bblanchon/pdfium-binaries.
# Check https://github.com/bblanchon/pdfium-binaries/releases for a newer
# version and update PDFIUM_VERSION accordingly.
PDFIUM_VERSION="chromium/6958"
PDFIUM_TAG="chromium%2F6958"   # URL-encoded tag for GitHub API

# Archive name for Windows x64
ARCHIVE_NAME="pdfium-win-x64.tgz"

# Download URL: GitHub release asset
DOWNLOAD_URL="https://github.com/bblanchon/pdfium-binaries/releases/download/${PDFIUM_TAG}/${ARCHIVE_NAME}"

# ---------------------------------------------------------------------------
# Guard: skip if DLL already present
# ---------------------------------------------------------------------------

if [[ -f "${TARGET_DLL}" ]]; then
    echo "[pdfium] pdfium.dll already exists at ${TARGET_DLL} — skipping download."
    exit 0
fi

# ---------------------------------------------------------------------------
# Download & extract
# ---------------------------------------------------------------------------

echo "[pdfium] Downloading PDFium ${PDFIUM_VERSION} for Windows x64..."
echo "[pdfium] URL: ${DOWNLOAD_URL}"

mkdir -p "${BINARIES_DIR}"

TMP_DIR="$(mktemp -d)"
TMP_ARCHIVE="${TMP_DIR}/${ARCHIVE_NAME}"

cleanup() {
    rm -rf "${TMP_DIR}"
}
trap cleanup EXIT

# Download archive
curl --fail --location --progress-bar \
     --output "${TMP_ARCHIVE}" \
     "${DOWNLOAD_URL}"

# Extract only the DLL from the archive
# The archive layout from bblanchon/pdfium-binaries is:
#   lib/pdfium.dll   (Windows)
echo "[pdfium] Extracting pdfium.dll..."
tar -xzf "${TMP_ARCHIVE}" -C "${TMP_DIR}" --wildcards "*/pdfium.dll" 2>/dev/null \
    || tar -xzf "${TMP_ARCHIVE}" -C "${TMP_DIR}"

# Locate the extracted DLL (may be in a subdirectory)
EXTRACTED_DLL="$(find "${TMP_DIR}" -name "pdfium.dll" | head -1)"

if [[ -z "${EXTRACTED_DLL}" ]]; then
    echo "[pdfium] ERROR: pdfium.dll not found in the downloaded archive." >&2
    echo "[pdfium] Archive contents:" >&2
    tar -tzf "${TMP_ARCHIVE}" >&2
    exit 1
fi

cp "${EXTRACTED_DLL}" "${TARGET_DLL}"

echo "[pdfium] Done. pdfium.dll installed at: ${TARGET_DLL}"
