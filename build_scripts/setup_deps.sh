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

    # Check .so files & critical headers
    local all_done=true
    for abi in arm64-v8a armeabi-v7a x86_64; do
        [ ! -f "$out_dir/lib/$abi/libfilament-jni.so" ] && all_done=false
    done
    [ ! -f "$out_dir/include/filament/Engine.h" ] && all_done=false
    [ ! -f "$out_dir/include/backend/BufferDescriptor.h" ] && all_done=false
    [ ! -f "$out_dir/include/utils/bitset.h" ] && all_done=false
    [ ! -f "$out_dir/include/filament/MaterialEnums.h" ] && all_done=false

    local h_count=0
    if [ -d "$out_dir/include" ]; then
        h_count=$(find "$out_dir/include" \( -name '*.h' -o -name '*.hpp' \) | wc -l)
    fi
    [ "$h_count" -lt 200 ] && all_done=false

    if [ "$all_done" = true ]; then ok "Filament already fully set up ($h_count headers)"; return; fi

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

    # ── Download complete C++ header tree ───────────────────
    info "Fetching complete Filament C++ header tree..."
    python3 -c "
import urllib.request, json, os, sys

version = '$version'
out_dir = '$out_dir/include'
url = f'https://api.github.com/repos/google/filament/git/trees/v{version}?recursive=1'
headers = {'User-Agent': 'ObrisBuild/1.0'}
if 'GITHUB_TOKEN' in os.environ:
    headers['Authorization'] = f'token {os.environ[\"GITHUB_TOKEN\"]}'

req = urllib.request.Request(url, headers=headers)
try:
    with urllib.request.urlopen(req) as resp:
        data = json.loads(resp.read().decode('utf-8'))
        tree = data.get('tree', [])
except Exception as e:
    print(f'API error fetching tree: {e}', file=sys.stderr)
    sys.exit(1)

target_roots = ['filament/', 'libs/']
header_mappings = []

for item in tree:
    if item['type'] == 'blob' and (item['path'].endswith('.h') or item['path'].endswith('.hpp')):
        path = item['path']
        if any(path.startswith(r) for r in target_roots) and '/include/' in path:
            prefix, rel = path.split('/include/', 1)
            if 'third_party' not in prefix:
                header_mappings.append((path, rel))

raw_base = f'https://raw.githubusercontent.com/google/filament/v{version}/'
downloaded = 0
failed = 0

for gh_path, rel_path in header_mappings:
    dest = os.path.join(out_dir, rel_path)
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    file_url = raw_base + gh_path
    try:
        urllib.request.urlretrieve(file_url, dest)
        downloaded += 1
    except Exception as e:
        print(f'Warning: failed to fetch {gh_path}: {e}', file=sys.stderr)
        failed += 1

print(f'[OK] Downloaded {downloaded} Filament headers ({failed} failed)')
" 2>/dev/null || {
        warn "Python script failed, falling back to manual curl header fetch..."
        local gh="https://raw.githubusercontent.com/google/filament/v$version"

        # Core filament
        mkdir -p "$out_dir/include/filament"
        for h in Engine Renderer Scene View Camera SwapChain Skybox IndirectLight LightManager Texture Color FilamentAPI TransformManager RenderableManager Box Options Viewport Frustum ColorGrading Exposure ToneMapper MaterialEnums; do
            curl -fsSL -o "$out_dir/include/filament/$h.h" "$gh/filament/include/filament/$h.h" 2>/dev/null || true
            curl -fsSL -o "$out_dir/include/filament/$h.h" "$gh/libs/filabridge/include/filament/$h.h" 2>/dev/null || true
        done

        # Backend
        mkdir -p "$out_dir/include/backend"
        for h in AcquiredImage BufferDescriptor CallbackHandler DriverApiForward DriverEnums Handle PipelineState PixelBufferDescriptor Platform PresentCallable Program SamplerDescriptor TargetBufferInfo; do
            curl -fsSL -o "$out_dir/include/backend/$h.h" "$gh/filament/backend/include/backend/$h.h" 2>/dev/null || true
        done

        # Utils
        mkdir -p "$out_dir/include/utils" "$out_dir/include/utils/linux" "$out_dir/include/utils/generic" "$out_dir/include/utils/android"
        for h in Allocator BitmaskEnum CString CallStack Condition CountDownLatch CyclicBarrier Entity EntityInstance EntityManager FixedCapacityVector FixedCircularBuffer Hash Invocable JobSystem Log Mutex NameComponentManager Panic Path PrivateImplementation Profiler QuadTree Range RangeMap SingleInstanceComponentManager Slice Stopwatch StructureOfArrays Systrace ThermalManager ThreadUtils WorkStealingDequeue Zip2Iterator algorithm api_level architecture ashmem bitset compiler compressed_pair debug memalign ostream sstream string trap unwindows vector; do
            curl -fsSL -o "$out_dir/include/utils/$h.h" "$gh/libs/utils/include/utils/$h.h" 2>/dev/null || true
        done
        for h in Condition Mutex; do
            curl -fsSL -o "$out_dir/include/utils/linux/$h.h" "$gh/libs/utils/include/utils/linux/$h.h" 2>/dev/null || true
            curl -fsSL -o "$out_dir/include/utils/generic/$h.h" "$gh/libs/utils/include/utils/generic/$h.h" 2>/dev/null || true
        done

        # Math
        mkdir -p "$out_dir/include/math"
        for h in mathfwd vec2 vec3 vec4 mat4 mat3 quat geometry half TMatHelpers TVecHelpers TQuatHelpers TMat TVec TQuat norm type_traits compiler common scalar fast int2 int3 int4 uint2 uint3 uint4; do
            curl -fsSL -o "$out_dir/include/math/$h.h" "$gh/libs/math/include/math/$h.h" 2>/dev/null || true
        done

        # gltfio
        mkdir -p "$out_dir/include/gltfio"
        for h in AssetLoader FilamentAsset ResourceLoader Animator math; do
            curl -fsSL -o "$out_dir/include/gltfio/$h.h" "$gh/libs/gltfio/include/gltfio/$h.h" 2>/dev/null || true
        done
    }

    # ── Verify ─────────────────────────────────────────────
    for ABI in arm64-v8a armeabi-v7a x86_64; do
        local f="$out_dir/lib/$ABI/libfilament-jni.so"
        [ -f "$f" ] && ok "Filament $ABI — $(wc -c < "$f") bytes"
    done
    [ -f "$out_dir/include/filament/Engine.h" ] && ok "filament/Engine.h ready"
    [ -f "$out_dir/include/backend/BufferDescriptor.h" ] && ok "backend/BufferDescriptor.h ready"
    [ -f "$out_dir/include/utils/bitset.h" ] && ok "utils/bitset.h ready"
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
