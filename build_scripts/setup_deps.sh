#!/usr/bin/env bash
# =============================================================================
# Obris — Third-Party Dependencies Setup
# Downloads Filament AAR (.so) + headers, miniaudio, libsodium
# =============================================================================
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OBRIS_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
THIRD_DIR="$OBRIS_DIR/third_party"

info()  { echo -e "\033[0;36m[INFO]\033[0m  $*"; }
ok()    { echo -e "\033[0;32m[OK]\033[0m    $*"; }
warn()  { echo -e "\033[1;33m[WARN]\033[0m  $*"; }

mkdir -p "$THIRD_DIR"

setup_miniaudio() {
    local dir="$THIRD_DIR/miniaudio"
    local header="$dir/miniaudio.h"
    mkdir -p "$dir"
    [ -f "$header" ] && [ "$(wc -c < "$header")" -gt 100000 ] && { ok "miniaudio already present"; return; }
    info "Downloading miniaudio..."
    curl -fsSL "https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h" -o "$header" || true
    [ -f "$header" ] && [ "$(wc -c < "$header")" -gt 100000 ] && ok "miniaudio — $(wc -c < "$header") bytes" || warn "miniaudio download failed"
}

setup_filament() {
    local version="1.53.2"
    local out_dir="$THIRD_DIR/filament"
    local any=false

    # Check .so files
    local all_done=true
    for abi in arm64-v8a armeabi-v7a x86_64; do
        [ ! -f "$out_dir/lib/$abi/libfilament-jni.so" ] && all_done=false
    done
    [ ! -f "$out_dir/include/filament/Engine.h" ] && all_done=false
    if [ "$all_done" = true ]; then ok "Filament already set up"; return; fi

    mkdir -p "$out_dir/lib" "$out_dir/include"

    # ── Download AARs and extract .so ─────────────────────
    for aar_name in filament gltfio filamat; do
        local aar_url="https://github.com/google/filament/releases/download/v$version/${aar_name}-v$version-android.aar"
        info "Downloading $aar_name AAR..."
        if curl -fsSL -o "/tmp/$aar_name.aar" "$aar_url" 2>/dev/null && [ -s "/tmp/$aar_name.aar" ]; then
            for ABI in arm64-v8a armeabi-v7a x86_64; do
                mkdir -p "$out_dir/lib/$ABI"
                unzip -o "/tmp/$aar_name.aar" "jni/$ABI/*" -d "/tmp/fil-aar" 2>/dev/null || true
                cp "/tmp/fil-aar/jni/$ABI/"*.so "$out_dir/lib/$ABI/" 2>/dev/null || true
            done
            rm -f "/tmp/$aar_name.aar"
            any=true
        else
            warn "$aar_name AAR download failed"
        fi
    done

    # ── Download real C++ headers ──────────────────────────
    if [ ! -f "$out_dir/include/filament/Engine.h" ]; then
        info "Downloading Filament headers..."

        local gh="https://raw.githubusercontent.com/google/filament/v$version"

        # Core Filament headers (from filament/include/filament/)
        local fil_headers="Engine Renderer Scene View Camera SwapChain Skybox IndirectLight Texture Color FilamentAPI TransformManager RenderableManager Box Options Viewport Frustum ColorGrading Exposure"
        local count=0
        for h in $fil_headers; do
            mkdir -p "$out_dir/include/filament"
            if curl -fsSL -o "$out_dir/include/filament/$h.h" "$gh/filament/include/filament/$h.h" 2>/dev/null; then
                count=$((count + 1))
            fi
        done

        # Math headers (from libs/math/include/math/)
        local math_headers="mathfwd vec2 vec3 vec4 mat4 mat3 quat geometry half TMatHelpers TVecHelpers TQuatHelpers TMat TVec TQuat norm type_traits compiler common scalar fast"
        for h in $math_headers; do
            mkdir -p "$out_dir/include/math"
            if curl -fsSL -o "$out_dir/include/math/$h.h" "$gh/libs/math/include/math/$h.h" 2>/dev/null; then
                count=$((count + 1))
            fi
        done

        # Utils headers (from libs/utils/include/utils/)
        local utils_headers="Entity EntityInstance EntityManager Invocable Allocator compiler Log PrivateImplementation BitmaskEnum unwindows Panic iostream ostream FixedCapacityVector CountDownLatch Path Systrace debug CallStack CString sstream"
        for h in $utils_headers; do
            mkdir -p "$out_dir/include/utils"
            if curl -fsSL -o "$out_dir/include/utils/$h.h" "$gh/libs/utils/include/utils/$h.h" 2>/dev/null; then
                count=$((count + 1))
            fi
        done

        # Backend headers (from filament/backend/include/backend/)
        local backend_headers="DriverEnums Platform PresentCallable"
        for h in $backend_headers; do
            mkdir -p "$out_dir/include/backend"
            if curl -fsSL -o "$out_dir/include/backend/$h.h" "$gh/filament/backend/include/backend/$h.h" 2>/dev/null; then
                count=$((count + 1))
            fi
        done

        # gltfio headers (from libs/gltfio/include/gltfio/)
        local gltfio_headers="AssetLoader FilamentAsset ResourceLoader Animator"
        for h in $gltfio_headers; do
            mkdir -p "$out_dir/include/gltfio"
            if curl -fsSL -o "$out_dir/include/gltfio/$h.h" "$gh/libs/gltfio/include/gltfio/$h.h" 2>/dev/null; then
                count=$((count + 1))
            fi
        done

        ok "Filament headers — $count files"
        any=true
    fi

    # ── Verify ─────────────────────────────────────────────
    for ABI in arm64-v8a armeabi-v7a x86_64; do
        local f="$out_dir/lib/$ABI/libfilament-jni.so"
        [ -f "$f" ] && ok "Filament $ABI — $(wc -c < "$f") bytes"
    done
    [ -f "$out_dir/include/filament/Engine.h" ] && ok "filament/Engine.h ready"
    [ "$any" = false ] && warn "Filament — NOT AVAILABLE (stub)"
}

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
            [ -f "/tmp/ls/jni/$ABI/libsodium.so" ] && { cp "/tmp/ls/jni/$ABI/libsodium.so" "$out_dir/lib/$ABI/"; any=true; }
        done
        unzip -o "/tmp/libsodium.aar" "include/*" -d "/tmp/ls-h" 2>/dev/null || true
        [ -d "/tmp/ls-h/include" ] && cp -r "/tmp/ls-h/include/." "$out_dir/include/" 2>/dev/null || true
        rm -f "/tmp/libsodium.aar"
    fi
    [ ! -f "$out_dir/include/sodium.h" ] && {
        mkdir -p "$out_dir/include"
        curl -fsSL "https://raw.githubusercontent.com/jedisct1/libsodium/1.0.20-stable/src/libsodium/include/sodium.h" -o "$out_dir/include/sodium.h" 2>/dev/null || true
    }
    [ "$any" = false ] && warn "libsodium — NOT AVAILABLE (stub)" || ok "libsodium done"
}

# ═══════════════════════════════════════════════════
echo ""; echo "═══ Obris — Third-Party Setup ═══"; echo ""
setup_miniaudio; setup_filament; setup_sodium
echo ""; echo "═══ Done ═══"
for lib in miniaudio filament libsodium; do
    so=$(find "$THIRD_DIR/$lib" -name '*.so' 2>/dev/null | wc -l)
    h=$(find "$THIRD_DIR/$lib" \( -name '*.h' -o -name '*.hpp' \) 2>/dev/null | wc -l)
    [ "$so" -gt 0 ] || [ "$h" -gt 0 ] && echo "  ✅ $lib: $so .so, $h headers" || echo "  ⬜ $lib: stub"
done
