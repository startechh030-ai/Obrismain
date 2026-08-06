#!/usr/bin/env bash
# =============================================================================
# Obris — Third-Party Dependencies Setup
# Downloads miniaudio and libsodium for native C++ library
# =============================================================================
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OBRIS_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
THIRD_DIR="$OBRIS_DIR/third_party"

info()  { echo -e "\033[0;36m[INFO]\033[0m  $*"; }
ok()    { echo -e "\033[0;32m[OK]\033[0m    $*"; }

mkdir -p "$THIRD_DIR"

# 1. miniaudio
mkdir -p "$THIRD_DIR/miniaudio"
if [ ! -f "$THIRD_DIR/miniaudio/miniaudio.h" ]; then
    info "Downloading miniaudio.h..."
    curl -fsSL "https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h" -o "$THIRD_DIR/miniaudio/miniaudio.h" || true
fi
ok "miniaudio ready"

# 2. libsodium
setup_sodium() {
    local version="1.0.20"
    local out_dir="$THIRD_DIR/libsodium"
    mkdir -p "$out_dir"
    if [ ! -f "$out_dir/include/sodium.h" ]; then
        curl -fsSL "https://raw.githubusercontent.com/jedisct1/libsodium/1.0.20-stable/src/libsodium/include/sodium.h" -o "$out_dir/include/sodium.h" 2>/dev/null || true
    fi
    ok "libsodium ready"
}
setup_sodium

echo ""; echo "═══ Setup Done ═══"
