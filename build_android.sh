#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
BUILD_TYPE="${1:-Debug}"
ABI="${2:-all}"
JOBS=$(nproc 2>/dev/null || echo 4)

export ANDROID_NDK="${ANDROID_NDK:-$ANDROID_NDK_HOME}"

# Step 1: deps
bash build_scripts/setup_deps.sh

# Step 2: native
build_abi() {
    local abi="$1"
    cmake -B "build/$abi" \
        -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake" \
        -DANDROID_ABI="$abi" -DANDROID_PLATFORM=android-26 \
        -DANDROID_STL=c++_shared -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -G Ninja
    cmake --build "build/$abi" --target obris_shared --parallel "$JOBS"
    mkdir -p "android/app/src/main/jniLibs/$abi"
    cp "build/$abi/libobris_shared.so" "android/app/src/main/jniLibs/$abi/"
}
if [ "$ABI" = "all" ]; then build_abi arm64-v8a; build_abi armeabi-v7a; build_abi x86_64
else build_abi "$ABI"; fi

# Step 3: APK
cd android && ./gradlew assembleDebug 2>/dev/null || echo "APK build skipped (run: cd android && ./gradlew assembleDebug)"
echo "Build done!"
