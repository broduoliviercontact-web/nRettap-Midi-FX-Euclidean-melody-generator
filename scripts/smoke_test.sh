#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

echo "-> Running native tests..."
make test

echo "-> Verifying exported symbol..."
make check-symbols
