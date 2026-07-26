#!/usr/bin/env bash
# =============================================================================
# Obris — Third-Party Dependencies Setup
# Downloads Filament (AAR + headers), miniaudio, libsodium for Android
# =============================================================================
set -euo pipefail

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
    if [ -f "$header" ] && [ "$(wc -c < "$header")" -gt 100000 ]; then
        ok "miniaudio — $(wc -c < "$header") bytes"
    else
        warn "miniaudio download failed"
    fi
}

# ══════════════════════════════════════════════════════════════════════════
# 2. Filament — from AAR (for .so) + git (for headers)
# ══════════════════════════════════════════════════════════════════════════
setup_filament() {
    local version="1.53.2"
    local out_dir="$THIRD_DIR/filament"
    local any=false

    # Check if already done
    local all_done=true
    for abi in arm64-v8a armeabi-v7a x86_64; do
        [ ! -f "$out_dir/lib/$abi/libfilament-jni.so" ] && all_done=false
    done
    [ ! -f "$out_dir/include/filament/Engine.h" ] && all_done=false
    if [ "$all_done" = true ]; then ok "Filament already set up"; return; fi

    mkdir -p "$out_dir"

    # ── Download AAR and extract .so files ──────────────────────
    local aar_url="https://github.com/google/filament/releases/download/v$version/filament-v$version-android.aar"
    info "Downloading Filament AAR..."
    if curl -fsSL -o "/tmp/filament.aar" "$aar_url" 2>/dev/null && [ -s "/tmp/filament.aar" ]; then
        for ABI in arm64-v8a armeabi-v7a x86_64; do
            mkdir -p "$out_dir/lib/$ABI"
            unzip -o "/tmp/filament.aar" "jni/$ABI/*" -d "/tmp/fil-aar" 2>/dev/null || true
            if [ -f "/tmp/fil-aar/jni/$ABI/libfilament-jni.so" ]; then
                cp "/tmp/fil-aar/jni/$ABI/libfilament-jni.so" "$out_dir/lib/$ABI/"
                ok "Filament .so $ABI — extracted"
                any=true
            fi
        done
        rm -f "/tmp/filament.aar"
    else
        warn "Filament AAR download failed"
    fi

    # Also download gltfio AAR for its .so
    curl -fsSL -o "/tmp/gltfio.aar" "https://github.com/google/filament/releases/download/v$version/gltfio-v$version-android.aar" 2>/dev/null && {
        for ABI in arm64-v8a armeabi-v7a x86_64; do
            unzip -o "/tmp/gltfio.aar" "jni/$ABI/*" -d "/tmp/fil-aar" 2>/dev/null || true
            if [ -f "/tmp/fil-aar/jni/$ABI/libgltfio-jni.so" ]; then
                cp "/tmp/fil-aar/jni/$ABI/libgltfio-jni.so" "$out_dir/lib/$ABI/" 2>/dev/null || true
            fi
        done
        rm -f "/tmp/gltfio.aar"
    } || true

    # ── Get C++ headers from git repo (shallow, no checkout) ────
    local HEADERS_DIR="$out_dir/include"
    if [ ! -f "$HEADERS_DIR/filament/Engine.h" ]; then
        info "Downloading Filament C++ headers from git..."
        mkdir -p "$HEADERS_DIR"

        # Download individual header files from GitHub raw
        # Core Filament headers
        local HEADER_BASE="https://raw.githubusercontent.com/google/filament/v$version"
        local HEADER_FILES=(
            "filament/Engine.h"
            "filament/Renderer.h"
            "filament/Scene.h"
            "filament/View.h"
            "filament/Camera.h"
            "filament/SwapChain.h"
            "filament/LightManager.h"
            "filament/Skybox.h"
            "filament/IndirectLight.h"
            "filament/Texture.h"
            "filament/MaterialInstance.h"
            "filament/Color.h"
            "filament/FilamentAPI.h"
            "filament/TransformManager.h"
            "filament/EntityManager.h"
            "filament/Box.h"
            "filament/RenderableManager.h"
            "filament/VertexBuffer.h"
            "filament/IndexBuffer.h"
            "filament/RenderTarget.h"
            "filament/MorphTargetBuffer.h"
            "filament/BufferObject.h"
            "filament/Options.h"
            "filament/DebugRegistry.h"
            "math/vec3.h"
            "math/vec4.h"
            "math/mat4.h"
            "math/vec2.h"
            "math/half.h"
            "math/geometry.h"
            "math/mat3.h"
            "math/quat.h"
            "math/TMatHelpers.h"
            "math/TVecHelpers.h"
            "math/TQuatHelpers.h"
            "math/TMat.h"
            "math/TVec.h"
            "math/TQuat.h"
            "math/norm.h"
            "math/type_traits.h"
            "math/compiler.h"
            "math/common.h"
            "math/scalar.h"
            "math/uint2.h"
            "math/uint3.h"
            "math/uint4.h"
            "math/int2.h"
            "math/int3.h"
            "math/int4.h"
            "math/arch.h"
            "math/fast.h"
            "math/mathfwd.h"
            "math/half.h"
            "utils/Entity.h"
            "utils/EntityInstance.h"
            "utils/EntityManager.h"
            "utils/Allocator.h"
            "utils/Log.h"
            "utils/iostream.h"
            "gltfio/AssetLoader.h"
            "gltfio/ResourceLoader.h"
            "gltfio/FilamentAsset.h"
            "gltfio/Animator.h"
            "gltfio/FilamentInstance.h"
            "gltfio/MaterialProvider.h"
            "gltfio/math.h"
            "private/backend/DriverApiForward.h"
        )

        local OK_COUNT=0
        for hfile in "${HEADER_FILES[@]}"; do
            local target="$HEADERS_DIR/$hfile"
            mkdir -p "$(dirname "$target")"
            if curl -fsSL -o "$target" "$HEADER_BASE/libs/$hfile" 2>/dev/null || \
               curl -fsSL -o "$target" "$HEADER_BASE/filament/$hfile" 2>/dev/null || \
               curl -fsSL -o "$target" "$HEADER_BASE/$hfile" 2>/dev/null; then
                OK_COUNT=$((OK_COUNT + 1))
            fi
        done

        if [ "$OK_COUNT" -gt 10 ]; then
            ok "Filament headers — $OK_COUNT files downloaded"
            any=true
        else
            warn "Filament headers — only $OK_COUNT files downloaded (trying alternative)"
            # Fallback: shallow clone just the include dir
            rm -rf "$HEADERS_DIR"
            git clone --depth=1 --branch "v$version" \
                "https://github.com/google/filament.git" "/tmp/filament-src" 2>/dev/null || true
            if [ -d "/tmp/filament-src/libs/filament/include" ]; then
                mkdir -p "$HEADERS_DIR"
                cp -r "/tmp/filament-src/libs/filament/include/." "$HEADERS_DIR/" 2>/dev/null || true
                cp -r "/tmp/filament-src/libs/utils/include/." "$HEADERS_DIR/" 2>/dev/null || true
                cp -r "/tmp/filament-src/libs/math/include/." "$HEADERS_DIR/" 2>/dev/null || true
                cp -r "/tmp/filament-src/libs/gltfio/include/." "$HEADERS_DIR/" 2>/dev/null || true
                any=true
            fi
            rm -rf "/tmp/filament-src"
        fi
    fi

    if [ "$any" = false ]; then
        warn "Filament — NOT AVAILABLE (stub renderer)"
    fi
}

# ══════════════════════════════════════════════════════════════════════════
# 3. libsodium (prebuilt from AAR)
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
                ok "libsodium $ABI"
                any=true
            }
        done
        # Headers
        unzip -o "/tmp/libsodium.aar" "include/*" -d "/tmp/ls-h" 2>/dev/null || true
        [ -d "/tmp/ls-h/include" ] && cp -r "/tmp/ls-h/include/." "$out_dir/include/" 2>/dev/null || true
        rm -f "/tmp/libsodium.aar"
    fi

    # Download headers if missing
    if [ ! -f "$out_dir/include/sodium.h" ]; then
        mkdir -p "$out_dir/include"
        curl -fsSL "https://raw.githubusercontent.com/jedisct1/libsodium/1.0.20-stable/src/libsodium/include/sodium.h" \
            -o "$out_dir/include/sodium.h" 2>/dev/null || true
    fi

    [ "$any" = false ] && warn "libsodium — stub"
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
