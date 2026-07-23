#!/usr/bin/env bash
# =============================================================================
# Obris — Third-Party Dependencies Setup
# Downloads Filament + miniaudio + libsodium for Android
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OBRIS_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
THIRD_DIR="$OBRIS_DIR/third_party"
JOBS=$(nproc 2>/dev/null || echo 4)

info()  { echo -e "\033[0;36m[INFO]\033[0m  $*"; }
ok()    { echo -e "\033[0;32m[OK]\033[0m    $*"; }
warn()  { echo -e "\033[1;33m[WARN]\033[0m  $*"; }

mkdir -p "$THIRD_DIR"

# ══════════════════════════════════════════════════════════════════════════
# 1. miniaudio (single header)
# ══════════════════════════════════════════════════════════════════════════
setup_miniaudio() {
    local dir="$THIRD_DIR/miniaudio"
    local header="$dir/miniaudio.h"
    mkdir -p "$dir"
    if [ -f "$header" ] && [ "$(wc -c < "$header")" -gt 100000 ]; then
        ok "miniaudio already present"; return
    fi
    info "Downloading miniaudio..."
    curl -fsSL "https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h" -o "$header"
    ok "miniaudio — $(wc -c < "$header") bytes"
}

# ══════════════════════════════════════════════════════════════════════════
# 2. Filament (prebuilt)
# ══════════════════════════════════════════════════════════════════════════
setup_filament() {
    local version="1.53.2"
    local out_dir="$THIRD_DIR/filament"
    local any=false

    local all_done=true
    for abi in arm64-v8a armeabi-v7a x86_64; do
        [ ! -f "$out_dir/lib/$abi/libfilament.so" ] && all_done=false
    done
    if [ "$all_done" = true ]; then ok "Filament already downloaded"; return; fi

    mkdir -p "$out_dir"

    for ABI in arm64-v8a armeabi-v7a x86_64; do
        local fabi
        case "$ABI" in arm64-v8a) fabi="aarch64";; armeabi-v7a) fabi="armv7";; x86_64) fabi="x86_64";; esac

        local filename="filament-$version-android-$fabi.tar.gz"
        local url="https://github.com/google/filament/releases/download/v$version/$filename"
        local target_dir="$out_dir/lib/$ABI"
        mkdir -p "$target_dir"

        info "Downloading Filament $ABI..."
        if curl -fsSL -o "/tmp/$filename" "$url" && [ -s "/tmp/$filename" ]; then
            local tmp_dir="/tmp/filament-extract-$ABI"
            rm -rf "$tmp_dir" && mkdir -p "$tmp_dir"
            tar -xzf "/tmp/$filename" -C "$tmp_dir" 2>/dev/null || { warn "extract failed"; continue; }

            find "$tmp_dir" -name '*.so' -exec cp {} "$target_dir/" \; 2>/dev/null || true
            find "$tmp_dir" \( -name '*.h' -o -name '*.hpp' \) | while read -r h; do
                rel="${h#$tmp_dir/}"
                mkdir -p "$out_dir/include/$(dirname "$rel")"
                cp "$h" "$out_dir/include/$rel"
            done 2>/dev/null || true

            ok "Filament $ABI — done"
            any=true
            rm -f "/tmp/$filename"
        else
            warn "Filament $ABI not found"
        fi
    done

    [ "$any" = false ] && warn "Filament NOT AVAILABLE — stub renderer"
}

# ══════════════════════════════════════════════════════════════════════════
# 3. libsodium (prebuilt)
# ══════════════════════════════════════════════════════════════════════════
setup_sodium() {
    local version="1.0.20"
    local out_dir="$THIRD_DIR/libsodium"
    local any=false

    local all_done=true
    for abi in arm64-v8a armeabi-v7a x86_64; do
        [ ! -f "$out_dir/lib/$abi/libsodium.so" ] && all_done=false
    done
    if [ "$all_done" = true ]; then ok "libsodium already present"; return; fi

    mkdir -p "$out_dir"

    # Try AAR
    info "Downloading libsodium AAR..."
    local aar_url="https://repo1.maven.org/maven2/org/libsodium/libsodium-android/$version/libsodium-android-$version.aar"
    if curl -fsSL -o "/tmp/libsodium.aar" "$aar_url" 2>/dev/null && [ -s "/tmp/libsodium.aar" ]; then
        for ABI in arm64-v8a armeabi-v7a x86_64; do
            mkdir -p "$out_dir/lib/$ABI"
            unzip -o "/tmp/libsodium.aar" "jni/$ABI/*" -d "/tmp/ls" 2>/dev/null || true
            [ -f "/tmp/ls/jni/$ABI/libsodium.so" ] && {
                cp "/tmp/ls/jni/$ABI/libsodium.so" "$out_dir/lib/$ABI/"
                ok "libsodium $ABI — from AAR"
                any=true
            }
        done
        # Headers
        unzip -o "/tmp/libsodium.aar" "include/*" -d "/tmp/ls-h" 2>/dev/null || true
        [ -d "/tmp/ls-h/include" ] && cp -r "/tmp/ls-h/include/." "$out_dir/include/" 2>/dev/null || true
        rm -f "/tmp/libsodium.aar"
    fi

    # Try GitHub tarballs
    if [ "$any" = false ]; then
        for ABI in arm64-v8a armeabi-v7a x86_64; do
            local fabi
            case "$ABI" in arm64-v8a) fabi="arm64-v8a";; armeabi-v7a) fabi="armeabi-v7a";; x86_64) fabi="x86_64";; esac
            local url="https://github.com/jedisct1/libsodium/releases/download/$version-stable/libsodium-android-$fabi.tar.gz"
            if curl -fsSL -o "/tmp/ls-$ABI.tar.gz" "$url" 2>/dev/null && [ -s "/tmp/ls-$ABI.tar.gz" ]; then
                tar -xzf "/tmp/ls-$ABI.tar.gz" -C "/tmp/ls2" 2>/dev/null || true
                find "/tmp/ls2" -name "libsodium.so" -exec cp {} "$out_dir/lib/$ABI/" \; 2>/dev/null || true
                any=true
            fi
        done
    fi

    # Download headers if missing
    if [ ! -f "$out_dir/include/sodium.h" ]; then
        mkdir -p "$out_dir/include"
        curl -fsSL "https://raw.githubusercontent.com/jedisct1/libsodium/1.0.20-stable/src/libsodium/include/sodium.h" \
            -o "$out_dir/include/sodium.h" 2>/dev/null || true
    fi

    [ "$any" = false ] && warn "libsodium NOT AVAILABLE — stub encryption"
    ok "libsodium setup done"
}

# ══════════════════════════════════════════════════════════════════════════
# Main
# ══════════════════════════════════════════════════════════════════════════
echo ""
echo "═══ Obris — Third-Party Setup ═══"
echo ""

setup_miniaudio
setup_filament
setup_sodium

echo ""
echo "═══ Done ═══"
echo "Library status:"
for lib in miniaudio filament libsodium; do
    so=$(find "$THIRD_DIR/$lib" -name '*.so' 2>/dev/null | wc -l)
    h=$(find "$THIRD_DIR/$lib" \( -name '*.h' -o -name '*.hpp' \) 2>/dev/null | wc -l)
    [ "$so" -gt 0 ] || [ "$h" -gt 0 ] && echo "  ✅ $lib: $so .so, $h headers" || echo "  ⬜ $lib: stub"
done
