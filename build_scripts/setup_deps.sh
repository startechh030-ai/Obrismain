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

    # ── Download all C++ headers ─────────────────────────────
    if [ ! -f "$out_dir/include/filament/Engine.h" ]; then
        info "Downloading Filament headers..."
        local gh="https://raw.githubusercontent.com/google/filament/v$version"
        local count=0

        # filament/include/filament/
        local fil_dir="$out_dir/include/filament"
        mkdir -p "$fil_dir"
        for h in Engine Renderer Scene View Camera SwapChain Skybox IndirectLight Texture Color FilamentAPI TransformManager RenderableManager Box Options Viewport Frustum ColorGrading Exposure ToneMapper; do
            curl -fsSL -o "$fil_dir/$h.h" "$gh/filament/include/filament/$h.h" 2>/dev/null && count=$((count+1))
        done
        # libs/filabridge/include/filament/ (MaterialEnums lives here, not in main filament dir)
        for h in MaterialEnums; do
            curl -fsSL -o "$fil_dir/$h.h" "$gh/libs/filabridge/include/filament/$h.h" 2>/dev/null && count=$((count+1))
        done

        # libs/utils/include/utils/ + subdirs
        local utils_dir="$out_dir/include/utils"
        mkdir -p "$utils_dir/linux" "$utils_dir/generic" "$utils_dir/android" "$utils_dir/darwin" "$utils_dir/win32"
        for h in Allocator BitmaskEnum CString CallStack Condition CountDownLatch CyclicBarrier Entity EntityInstance EntityManager FixedCapacityVector FixedCircularBuffer Hash Invocable JobSystem Log Mutex NameComponentManager Panic Path PrivateImplementation Profiler QuadTree Range RangeMap SingleInstanceComponentManager Slice Stopwatch StructureOfArrays Systrace ThermalManager ThreadUtils WorkStealingDequeue Zip2Iterator algorithm api_level architecture ashmem bitset compiler compressed_pair debug memalign ostream sstream string trap unwindows vector; do
            curl -fsSL -o "$utils_dir/$h.h" "$gh/libs/utils/include/utils/$h.h" 2>/dev/null && count=$((count+1))
        done
        # Platform-specific
        for h in Condition Mutex; do
            curl -fsSL -o "$utils_dir/linux/$h.h" "$gh/libs/utils/include/utils/linux/$h.h" 2>/dev/null && count=$((count+1))
            curl -fsSL -o "$utils_dir/generic/$h.h" "$gh/libs/utils/include/utils/generic/$h.h" 2>/dev/null && count=$((count+1))
        done
        for h in PerformanceHintManager Systrace ThermalManager; do
            curl -fsSL -o "$utils_dir/android/$h.h" "$gh/libs/utils/include/utils/android/$h.h" 2>/dev/null && count=$((count+1))
        done
        curl -fsSL -o "$utils_dir/darwin/Systrace.h" "$gh/libs/utils/include/utils/darwin/Systrace.h" 2>/dev/null && count=$((count+1))
        curl -fsSL -o "$utils_dir/win32/stdtypes.h" "$gh/libs/utils/include/utils/win32/stdtypes.h" 2>/dev/null && count=$((count+1))

        # libs/math/include/math/
        local math_dir="$out_dir/include/math"
        mkdir -p "$math_dir"
        for h in mathfwd vec2 vec3 vec4 mat4 mat3 quat geometry half TMatHelpers TVecHelpers TQuatHelpers TMat TVec TQuat norm type_traits compiler common scalar fast int2 int3 int4 uint2 uint3 uint4; do
            curl -fsSL -o "$math_dir/$h.h" "$gh/libs/math/include/math/$h.h" 2>/dev/null && count=$((count+1))
        done

        # filament/backend/include/backend/
        local backend_dir="$out_dir/include/backend"
        mkdir -p "$backend_dir"
        for h in DriverEnums Platform PresentCallable CallbackHandler PixelBufferDescriptor; do
            curl -fsSL -o "$backend_dir/$h.h" "$gh/filament/backend/include/backend/$h.h" 2>/dev/null && count=$((count+1))
        done

        # libs/gltfio/include/gltfio/
        local gltfio_dir="$out_dir/include/gltfio"
        mkdir -p "$gltfio_dir"
        for h in AssetLoader FilamentAsset ResourceLoader Animator math; do
            curl -fsSL -o "$gltfio_dir/$h.h" "$gh/libs/gltfio/include/gltfio/$h.h" 2>/dev/null && count=$((count+1))
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
