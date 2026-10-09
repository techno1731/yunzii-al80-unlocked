#!/usr/bin/env bash
# Run the device-free firmware tests.
set -euo pipefail

# macOS: fall back to the command line tools when Xcode's licence has not been accepted.
if [ -z "${DEVELOPER_DIR:-}" ] && [ -d /Library/Developer/CommandLineTools ] && ! /usr/bin/xcrun --find cc >/dev/null 2>&1; then
  export DEVELOPER_DIR=/Library/Developer/CommandLineTools
fi
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="$HERE/../build"
mkdir -p "$OUT"

build() { cc "$@" -Wall -Wextra -Werror -I "$HERE/../keyboard" "$HERE/test_logic.c" -o "$OUT/test_logic" 2>/dev/null; }
# Some macOS installs ship a default SDK the linker cannot read; fall back to the others.
build || { for sdk in /Library/Developer/CommandLineTools/SDKs/MacOSX*.sdk; do build -isysroot "$sdk" && break; done; }
"$OUT/test_logic"
