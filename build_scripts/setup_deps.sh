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

# ═══════════════════════════════════════════════════
# 1. miniaudio (single header)
# ═══════════════════════════════════════════════════
setup_miniaudio() {
    local dir="$THIRD_DIR/miniaudio"
    local header="$dir/miniaudio.h"
    mkdir -p "$dir"
    [ -f "$header" ] && [ "$(wc -c < "$header")" -gt 100000 ] && { ok "miniaudio already present"; return; }
    info "Downloading miniaudio..."
    curl -fsSL "https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h" -o "$header"
    [ -f "$header" ] && [ "$(wc -c < "$header")" -gt 100000 ] && ok "miniaudio — $(wc -c < "$header") bytes" || warn "miniaudio download failed"
}

# ═══════════════════════════════════════════════════
# 2. Filament
# ═══════════════════════════════════════════════════
setup_filament() {
    local version="1.53.2"
    local out_dir="$THIRD_DIR/filament"
    local any=false

    # Check already done
    local all_done=true
    for abi in arm64-v8a armeabi-v7a x86_64; do
        [ ! -f "$out_dir/lib/$abi/libfilament-jni.so" ] && all_done=false
    done
    [ ! -f "$out_dir/include/filament/Engine.h" ] && all_done=false
    if [ "$all_done" = true ]; then ok "Filament already set up"; return; fi

    mkdir -p "$out_dir"

    # ── Download AARs and extract .so files ────────────────────
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

    # ── Download headers via Python ────────────────────────────
    if [ ! -f "$out_dir/include/filament/Engine.h" ]; then
        info "Downloading Filament headers via python..."
        if python3 -c "
import os, subprocess
base = 'https://raw.githubusercontent.com/google/filament/v1.53.2'
inc = '$out_dir/include'
paths = [
    ('filament/include/filament/Engine.h', 'filament/Engine.h'),
    ('filament/include/filament/Renderer.h', 'filament/Renderer.h'),
    ('filament/include/filament/Scene.h', 'filament/Scene.h'),
    ('filament/include/filament/View.h', 'filament/View.h'),
    ('filament/include/filament/Camera.h', 'filament/Camera.h'),
    ('filament/include/filament/SwapChain.h', 'filament/SwapChain.h'),
    ('filament/include/filament/LightManager.h', 'filament/LightManager.h'),
    ('filament/include/filament/Skybox.h', 'filament/Skybox.h'),
    ('filament/include/filament/IndirectLight.h', 'filament/IndirectLight.h'),
    ('filament/include/filament/Texture.h', 'filament/Texture.h'),
    ('filament/include/filament/Color.h', 'filament/Color.h'),
    ('filament/include/filament/FilamentAPI.h', 'filament/FilamentAPI.h'),
    ('filament/include/filament/TransformManager.h', 'filament/TransformManager.h'),
    ('filament/include/filament/RenderableManager.h', 'filament/RenderableManager.h'),
    ('libs/utils/include/utils/Entity.h', 'utils/Entity.h'),
    ('libs/utils/include/utils/EntityInstance.h', 'utils/EntityInstance.h'),
    ('libs/utils/include/utils/EntityManager.h', 'utils/EntityManager.h'),
    ('libs/utils/include/utils/Allocator.h', 'utils/Allocator.h'),
    ('libs/utils/include/utils/Log.h', 'utils/Log.h'),
    ('libs/math/include/math/mathfwd.h', 'math/mathfwd.h'),
    ('libs/math/include/math/vec2.h', 'math/vec2.h'),
    ('libs/math/include/math/vec3.h', 'math/vec3.h'),
    ('libs/math/include/math/vec4.h', 'math/vec4.h'),
    ('libs/math/include/math/mat4.h', 'math/mat4.h'),
    ('libs/math/include/math/mat3.h', 'math/mat3.h'),
    ('libs/math/include/math/quat.h', 'math/quat.h'),
    ('libs/math/include/math/geometry.h', 'math/geometry.h'),
    ('libs/math/include/math/half.h', 'math/half.h'),
    ('libs/math/include/math/TMatHelpers.h', 'math/TMatHelpers.h'),
    ('libs/math/include/math/TVecHelpers.h', 'math/TVecHelpers.h'),
    ('libs/math/include/math/TQuatHelpers.h', 'math/TQuatHelpers.h'),
    ('libs/math/include/math/TMat.h', 'math/TMat.h'),
    ('libs/math/include/math/TVec.h', 'math/TVec.h'),
    ('libs/math/include/math/TQuat.h', 'math/TQuat.h'),
    ('libs/gltfio/include/gltfio/AssetLoader.h', 'gltfio/AssetLoader.h'),
    ('libs/gltfio/include/gltfio/FilamentAsset.h', 'gltfio/FilamentAsset.h'),
    ('libs/gltfio/include/gltfio/ResourceLoader.h', 'gltfio/ResourceLoader.h'),
    ('libs/gltfio/include/gltfio/Animator.h', 'gltfio/Animator.h'),
]
count = 0
for rp, lp in paths:
    target = os.path.join(inc, lp)
    os.makedirs(os.path.dirname(target), exist_ok=True)
    r = subprocess.run(['curl', '-fsSL', '-o', target, base + '/' + rp], capture_output=True)
    if r.returncode == 0 and os.path.getsize(target) > 10:
        count += 1
print(f'{count}/{len(paths)}')
exit(0 if count > 25 else 1)
" 2>&1; then
            ok "Filament headers downloaded"
            any=true
        else
            warn "Header download partial — continuing with stubs"
        fi
    fi

    # ── Create stubs for missing headers ───────────────────────
    if [ ! -f "$out_dir/include/filament/Engine.h" ]; then
        warn "No headers at all — creating minimal stubs for compilation"
        mkdir -p "$out_dir/include/filament" "$out_dir/include/utils" "$out_dir/include/math" "$out_dir/include/gltfio"
        cat > "$out_dir/include/filament/Engine.h" << 'STUB'
#pragma once
#include <cstdint>
namespace filament {
    class Engine { public: enum Backend : uint8_t { OPENGL = 0, VULKAN = 1, METAL = 2 }; };
    class Renderer {};
    class Scene {};
    class View {};
    class Camera {};
}
STUB
    fi

    # ── Verify ─────────────────────────────────────────────────
    for ABI in arm64-v8a armeabi-v7a x86_64; do
        local sz_file="$out_dir/lib/$ABI/libfilament-jni.so"
        [ -f "$sz_file" ] && ok "Filament $ABI — $(wc -c < "$sz_file") bytes"
    done
    [ -f "$out_dir/include/filament/Engine.h" ] && ok "Filament headers present"
    [ "$any" = false ] && warn "Filament — NOT AVAILABLE (stub renderer)"
}

# ═══════════════════════════════════════════════════
# 3. libsodium
# ═══════════════════════════════════════════════════
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
    [ "$any" = false ] && warn "libsodium — NOT AVAILABLE (stub encryption)" || ok "libsodium done"
}

# ═══════════════════════════════════════════════════
# Main
# ═══════════════════════════════════════════════════
echo ""; echo "═══ Obris — Third-Party Setup ═══"; echo ""
setup_miniaudio; setup_filament; setup_sodium
echo ""; echo "═══ Done ═══"
for lib in miniaudio filament libsodium; do
    so=$(find "$THIRD_DIR/$lib" -name '*.so' 2>/dev/null | wc -l)
    h=$(find "$THIRD_DIR/$lib" \( -name '*.h' -o -name '*.hpp' \) 2>/dev/null | wc -l)
    [ "$so" -gt 0 ] || [ "$h" -gt 0 ] && echo "  ✅ $lib: $so .so, $h headers" || echo "  ⬜ $lib: stub"
done
