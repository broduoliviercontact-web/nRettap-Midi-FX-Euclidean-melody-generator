#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

MODULE_ID="$(python3 -c 'import json; print(json.load(open("src/module.json"))["id"])')"
VERSION="$(python3 -c 'import json; print(json.load(open("src/module.json"))["version"])')"
DSO="build/aarch64/dsp.so"
DIST="dist/${MODULE_ID}"

echo "-> Packaging ${MODULE_ID} v${VERSION}..."

if [ ! -f "$DSO" ]; then
    echo "Missing $DSO — running build first..."
    ./scripts/build.sh
fi

rm -rf "dist/${MODULE_ID}" "dist/${MODULE_ID}-module.tar.gz"
mkdir -p "${DIST}"

cp src/module.json "${DIST}/module.json"
cp "${DSO}" "${DIST}/dsp.so"
[ -f src/ui.js ] && cp src/ui.js "${DIST}/ui.js"
[ -f src/ui_chain.js ] && cp src/ui_chain.js "${DIST}/ui_chain.js"

tar -C dist -czf "dist/${MODULE_ID}-module.tar.gz" "${MODULE_ID}"

echo "OK: dist/${MODULE_ID}-module.tar.gz"
echo ""
tar -tzf "dist/${MODULE_ID}-module.tar.gz"
