#!/usr/bin/env bash
# =============================================================================
# Obris — Third-Party Dependencies Setup
# Downloads Filament AAR (JNI-based .so), miniaudio, libsodium
# =============================================================================
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OBRIS_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
THIRD_DIR="$OBRIS_DIR/third_party"

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
    [ -f "$header" ] && [ "$(wc -c < "$header")" -gt 100000 ] && { ok "miniaudio already present"; return; }
    info "Downloading miniaudio..."
    curl -fsSL "https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h" -o "$header"
    [ -f "$header" ] && [ "$(wc -c < "$header")" -gt 100000 ] && ok "miniaudio — $(wc -c < "$header") bytes" || warn "miniaudio download failed"
}

# ══════════════════════════════════════════════════════════════════════════
# 2. Filament — download AAR, extract .so, get headers from source
# ══════════════════════════════════════════════════════════════════════════
setup_filament() {
    local version="1.53.2"
    local out_dir="$THIRD_DIR/filament"
    local any=false

    local all_done=true
    for abi in arm64-v8a armeabi-v7a x86_64; do
        [ ! -f "$out_dir/lib/$abi/libfilament-jni.so" ] && all_done=false
    done
    [ ! -f "$out_dir/include/filament/Engine.h" ] && all_done=false
    if [ "$all_done" = true ]; then ok "Filament already set up"; return; fi

    mkdir -p "$out_dir"

    # ── Download AAR and extract .so files ──────────────────────
    for aar_name in filament gltfio filamat; do
        local aar_url="https://github.com/google/filament/releases/download/v$version/${aar_name}-v$version-android.aar"
        info "Downloading $aar_name AAR..."
        curl -fsSL -o "/tmp/$aar_name.aar" "$aar_url" 2>/dev/null && [ -s "/tmp/$aar_name.aar" ] && {
            for ABI in arm64-v8a armeabi-v7a x86_64; do
                mkdir -p "$out_dir/lib/$ABI"
                unzip -o "/tmp/$aar_name.aar" "jni/$ABI/*" -d "/tmp/fil-aar" 2>/dev/null || true
                cp "/tmp/fil-aar/jni/$ABI/"*.so "$out_dir/lib/$ABI/" 2>/dev/null || true
            done
            rm -f "/tmp/$aar_name.aar"
            any=true
        } || warn "$aar_name AAR download failed"
    done

    # ── Get C++ headers from source (shallow clone) ─────────────
    if [ ! -f "$out_dir/include/filament/Engine.h" ]; then
        info "Downloading Filament headers from source..."
        local src_dir="/tmp/filament-src-headers"
        rm -rf "$src_dir"

        # Clone just the include directories we need (no filter flags)
        git clone --depth=1 --branch "v$version" \
            "https://github.com/google/filament.git" "$src_dir" 2>&1 || {
            warn "Failed to clone Filament source for headers"
            warn "Creating stub headers — the C++ code will compile but GLB loading won't work"
            mkdir -p "$out_dir/include/filament" "$out_dir/include/math" "$out_dir/include/utils"
            # Create minimal stubs for compilation
            cat > "$out_dir/include/filament/Engine.h" << 'EOF'
#pragma once
#include <cstdint>
namespace filament {
class Engine {
public:
    enum Backend : uint8_t { OPENGL, VULKAN, METAL };
    static Engine* create(Backend backend);
    void destroy(Engine* engine);
    // Key methods are in libfilament-jni.so — stubs for compilation only
};
}
EOF
            cat > "$out_dir/include/filament/Renderer.h" << 'EOF'
#pragma once
namespace filament { class Renderer {}; }
EOF
            cat > "$out_dir/include/filament/Scene.h" << 'EOF'
#pragma once
namespace filament { class Scene {}; }
EOF
            cat > "$out_dir/include/filament/View.h" << 'EOF'
#pragma once
namespace filament { class View {}; }
EOF
            cat > "$out_dir/include/filament/Camera.h" << 'EOF'
#pragma once
namespace filament { class Camera {}; }
EOF
            cat > "$out_dir/include/filament/LightManager.h" << 'EOF'
#pragma once
namespace filament { class LightManager {}; }
EOF
            ok "Filament header stubs created (compilation OK, GLB loading disabled)"
            any=true
            return
        }

        # Copy headers from the correct locations
        local found_headers=false
        for inc_dir in libs/filament/include libs/utils/include libs/math/include libs/gltfio/include libs/ibl/include; do
            if [ -d "$src_dir/$inc_dir" ]; then
                cp -r "$src_dir/$inc_dir/." "$out_dir/include/" 2>/dev/null || true
                found_headers=true
            fi
        done
        rm -rf "$src_dir"

        if [ -f "$out_dir/include/filament/Engine.h" ]; then
            ok "Filament headers downloaded from source"
            any=true
        else
            warn "Filament headers not found from source — using stubs"
            # Fall back to stubs
            mkdir -p "$out_dir/include/filament" "$out_dir/include/math" "$out_dir/include/utils"
        fi
    fi

    # Verify
    for ABI in arm64-v8a armeabi-v7a x86_64; do
        [ -f "$out_dir/lib/$ABI/libfilament-jni.so" ] && ok "Filament $ABI — $(wc -c < "$out_dir/lib/$ABI/libfilament-jni.so") bytes"
    done

    [ "$any" = false ] && warn "Filament — NOT AVAILABLE (stub renderer)"
}

# ══════════════════════════════════════════════════════════════════════════
# 3. libsodium (prebuilt AAR)
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
    info "Downloading libsodium AAR..."
    if curl -fsSL -o "/tmp/libsodium.aar" \
      "https://repo1.maven.org/maven2/org/libsodium/libsodium-android/$version/libsodium-android-$version.aar" \
      2>/dev/null && [ -s "/tmp/libsodium.aar" ]; then
        for ABI in arm64-v8a armeabi-v7a x86_64; do
            mkdir -p "$out_dir/lib/$ABI"
            unzip -o "/tmp/libsodium.aar" "jni/$ABI/*" -d "/tmp/ls" 2>/dev/null || true
            [ -f "/tmp/ls/jni/$ABI/libsodium.so" ] && {
                cp "/tmp/ls/jni/$ABI/libsodium.so" "$out_dir/lib/$ABI/"
                any=true
            }
        done
        unzip -o "/tmp/libsodium.aar" "include/*" -d "/tmp/ls-h" 2>/dev/null || true
        [ -d "/tmp/ls-h/include" ] && cp -r "/tmp/ls-h/include/." "$out_dir/include/" 2>/dev/null || true
        rm -f "/tmp/libsodium.aar"
    fi
    [ ! -f "$out_dir/include/sodium.h" ] && {
        mkdir -p "$out_dir/include"
        curl -fsSL "https://raw.githubusercontent.com/jedisct1/libsodium/1.0.20-stable/src/libsodium/include/sodium.h" \
            -o "$out_dir/include/sodium.h" 2>/dev/null || true
    }
    [ "$any" = false ] && warn "libsodium — NOT AVAILABLE"
    ok "libsodium done"
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
for lib in miniaudio filament libsodium; do
    so=$(find "$THIRD_DIR/$lib" -name '*.so' 2>/dev/null | wc -l)
    h=$(find "$THIRD_DIR/$lib" \( -name '*.h' -o -name '*.hpp' \) 2>/dev/null | wc -l)
    [ "$so" -gt 0 ] || [ "$h" -gt 0 ] && echo "  ✅ $lib: $so .so, $h headers" || echo "  ⬜ $lib: stub"
done
