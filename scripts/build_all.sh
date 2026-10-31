#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DIST_DIR="${ROOT_DIR}/dist"
mkdir -p "${DIST_DIR}"

echo "[1/4] Generating sample documents (.docx, .doc, .rtf, .pdf)..."
python3 "${ROOT_DIR}/scripts/generate_samples.py"

echo "[2/4] Building Native Linux x86_64 Browser + Unit Tests..."
cmake -S "${ROOT_DIR}" -B "${ROOT_DIR}/build-linux" -DCMAKE_BUILD_TYPE=Release
cmake --build "${ROOT_DIR}/build-linux" -j"$(nproc)"
cp -f "${ROOT_DIR}/build-linux/litebrowser" "${DIST_DIR}/litebrowser-linux-x64"
strip -s "${DIST_DIR}/litebrowser-linux-x64"

echo "[3/4] Cross-compiling Windows 32-bit Browser (Windows XP / 7 / 8 / 10 / 11)..."
if command -v i686-w64-mingw32-g++-win32 >/dev/null 2>&1; then
    cmake -S "${ROOT_DIR}" -B "${ROOT_DIR}/build-win32-xp" \
        -DCMAKE_SYSTEM_NAME=Windows \
        -DCMAKE_CXX_COMPILER=i686-w64-mingw32-g++-win32 \
        -DTARGET_WINXP=ON \
        -DBUILD_LITEBROWSER_TESTS=OFF \
        -DCMAKE_BUILD_TYPE=Release
    cmake --build "${ROOT_DIR}/build-win32-xp" -j"$(nproc)"
    cp -f "${ROOT_DIR}/build-win32-xp/litebrowser.exe" "${DIST_DIR}/LiteBrowser-Win32-XP-7-8-10.exe"
    i686-w64-mingw32-strip -s "${DIST_DIR}/LiteBrowser-Win32-XP-7-8-10.exe"
fi

echo "[4/4] Cross-compiling Windows 64-bit Browser (Windows 7 / 8 / 10 / 11)..."
if command -v x86_64-w64-mingw32-g++-win32 >/dev/null 2>&1; then
    cmake -S "${ROOT_DIR}" -B "${ROOT_DIR}/build-win64" \
        -DCMAKE_SYSTEM_NAME=Windows \
        -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++-win32 \
        -DBUILD_LITEBROWSER_TESTS=OFF \
        -DCMAKE_BUILD_TYPE=Release
    cmake --build "${ROOT_DIR}/build-win64" -j"$(nproc)"
    cp -f "${ROOT_DIR}/build-win64/litebrowser.exe" "${DIST_DIR}/LiteBrowser-Win64-7-8-10-11.exe"
    x86_64-w64-mingw32-strip -s "${DIST_DIR}/LiteBrowser-Win64-7-8-10-11.exe"
fi

echo ""
echo "Running C++ Core & Document Reader Tests..."
"${ROOT_DIR}/build-linux/test_core" "${ROOT_DIR}/samples"

echo ""
echo "Running Linux Browser Headless Engine Check..."
"${DIST_DIR}/litebrowser-linux-x64" --headless-check

echo ""
echo "Build artifacts ready in ${DIST_DIR}:"
ls -lh "${DIST_DIR}"

