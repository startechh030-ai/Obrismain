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

    # ── Get C++ headers from GitHub raw URLs ──────────────────
    if [ ! -f "$out_dir/include/filament/Engine.h" ]; then
        info "Downloading Filament headers from GitHub raw..."
        local gh="https://raw.githubusercontent.com/google/filament/v$version"

        # Try each path variant (structure changed across versions)
        local count=0
        download_header() {
            local name="$1"  # relative path inside include dir, e.g. "filament/Engine.h"
            local dir="$out_dir/include/$(dirname "$name")"
            mkdir -p "$dir"

            # Try multiple possible repo paths
            for repo_path in \
                "filament/include/$name" \
                "libs/$(dirname "$name")/include/$name" \
                "libs/$(echo "$name" | cut -d/ -f1)/include/$name" \
                "libs/filament/include/$name" \
                "libs/gltfio/include/$name" \
                "libs/ibl/include/$name" \
                "libs/utils/include/$name" \
                "filament/backend/include/$name"; do
                if curl -fsSL -o "$dir/$(basename "$name")" "$gh/$repo_path" 2>/dev/null; then
                    count=$((count + 1))
                    return 0
                fi
            done
            warn "  missed: $name"
            return 1
        }

        download_header "filament/Engine.h"
        download_header "filament/Renderer.h"
        download_header "filament/Scene.h"
        download_header "filament/View.h"
        download_header "filament/Camera.h"
        download_header "filament/SwapChain.h"
        download_header "filament/LightManager.h"
        download_header "filament/Skybox.h"
        download_header "filament/IndirectLight.h"
        download_header "filament/Texture.h"
        download_header "filament/Color.h"
        download_header "filament/FilamentAPI.h"
        download_header "filament/TransformManager.h"
        download_header "filament/RenderableManager.h"
        download_header "filament/VertexBuffer.h"
        download_header "filament/IndexBuffer.h"
        download_header "filament/Material.h"
        download_header "filament/MaterialInstance.h"
        download_header "filament/RenderTarget.h"
        download_header "filament/Options.h"
        download_header "filament/Box.h"
        download_header "filament/Viewport.h"
        download_header "filament/Frustum.h"
        download_header "filament/ColorGrading.h"
        download_header "utils/Entity.h"
        download_header "utils/EntityInstance.h"
        download_header "utils/EntityManager.h"
        download_header "utils/Invocable.h"
        download_header "utils/Allocator.h"
        download_header "utils/Log.h"
        download_header "utils/iostream.h"
        download_header "utils/compiler.h"
        download_header "utils/Panic.h"
        download_header "math/mathfwd.h"
        download_header "math/vec2.h"
        download_header "math/vec3.h"
        download_header "math/vec4.h"
        download_header "math/mat4.h"
        download_header "math/mat3.h"
        download_header "math/quat.h"
        download_header "math/geometry.h"
        download_header "math/half.h"
        download_header "math/TMatHelpers.h"
        download_header "math/TVecHelpers.h"
        download_header "math/TQuatHelpers.h"
        download_header "math/TMat.h"
        download_header "math/TVec.h"
        download_header "math/TQuat.h"
        download_header "math/norm.h"
        download_header "math/type_traits.h"
        download_header "math/compiler.h"
        download_header "math/common.h"
        download_header "math/scalar.h"
        download_header "math/fast.h"
        download_header "gltfio/AssetLoader.h"
        download_header "gltfio/FilamentAsset.h"
        download_header "gltfio/ResourceLoader.h"
        download_header "gltfio/Animator.h"
        download_header "gltfio/math.h"
        download_header "ibl/Cubemap.h"

        # Backend headers (special path)
        mkdir -p "$out_dir/include/private/backend"
        for bp in "filament/backend/include/private/backend/DriverApiForward.h" \
                   "libs/filament/backend/include/private/backend/DriverApiForward.h"; do
            curl -fsSL -o "$out_dir/include/private/backend/DriverApiForward.h" "$gh/$bp" 2>/dev/null && { count=$((count+1)); break; }
        done

        if [ "$count" -gt 20 ]; then
            ok "Filament headers — $count files downloaded"
            any=true
        else
            warn "Filament headers — only $count downloaded (need 20+)"
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
