# Obris — Lightweight GLB Renderer for Android

**libs.obris.so** — A lightweight C++ library for rendering GLB models with PBR materials, HDR environments, audio playback, JSON manifest parsing, and encryption. Built on Filament + miniaudio + libsodium.

## Architecture

```
┌─────────────────────────────────────────────────┐
│  Your Kotlin App                                 │
│  (Splash → Load → Auth → Lobby → Mini-Games)    │
├─────────────────────────────────────────────────┤
│  libs.obris.so                                   │
│  ┌──────────┐ ┌──────────┐ ┌──────────────────┐ │
│  │ Filament  │ │ miniaudio│ │ libsodium        │ │
│  │ GLB PBR   │ │ Sound FX │ │ XChaCha20-Poly1305│ │
│  │ IBL/HDR   │ │ BGM loop │ │ Key generation   │ │
│  │ Camera    │ │ 3D audio │ │                  │ │
│  └──────────┘ └──────────┘ └──────────────────┘ │
│  ┌──────────┐ ┌──────────┐                      │
│  │ JSON     │ │ JNI      │                      │
│  │ Reader   │ │ Bridge   │                      │
│  └──────────┘ └──────────┘                      │
├─────────────────────────────────────────────────┤
│  Godot (sleeps until a mini-game launches)       │
└─────────────────────────────────────────────────┘
```

## API (Kotlin)

```kotlin
// Init
ObrisActivity.nativeCreate(surface, assets, width, height, iblPath)

// Camera
ObrisActivity.nativeSetCamera(x, y, z, tx, ty, tz, fov)

// Lights
ObrisActivity.nativeAddLight(type, r, g, b, intensity, dx, dy, dz)

// Models
val model = ObrisActivity.nativeLoadModel("characters/hero.glb", ...)
ObrisActivity.nativeSetModelTransform(handle, px, py, pz, rx, ry, rz, rw, sx, sy, sz)
ObrisActivity.nativeSetModelVisible(handle, true)

// Environment
ObrisActivity.nativeLoadIBL("environments/lobby.ktx")
ObrisActivity.nativeSetIBLIntensity(0.8f)

// Audio
val bgm = ObrisActivity.nativeLoadSound("audio/bgm.ogg")
ObrisActivity.nativePlaySound(bgm, 0.5f, true)

// JSON
val manifest = ObrisActivity.nativeLoadJSON("scenes/lobby.json")
val bgmPath = ObrisActivity.nativeJSONGetString(manifest, "background_music")

// Encryption
val encrypted = ObrisActivity.nativeEncrypt(key, data)
val decrypted = ObrisActivity.nativeDecrypt(key, encrypted)
```

## Building

### Prerequisites
- Android NDK 27+
- CMake 3.22+, Ninja
- Android SDK 34+

### One-command build
```bash
export ANDROID_NDK=$HOME/Android/Sdk/ndk/27.0.12077973
./build_android.sh
```

### Step by step
```bash
# 1. Download dependencies
bash build_scripts/setup_deps.sh

# 2. Build native .so
cmake -B build/arm64-v8a \
    -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26 \
    -DANDROID_STL=c++_shared -DCMAKE_BUILD_TYPE=Debug -G Ninja
cmake --build build/arm64-v8a --target obris_shared --parallel

# 3. Build APK
cd android && ./gradlew assembleDebug
```

### GitHub Actions
Push to `main` and the CI automatically:
1. Downloads Filament, miniaudio, libsodium
2. Builds `libobris_shared.so` for arm64-v8a, armeabi-v7a, x86_64
3. Packages the Android APK
4. Uploads artifacts

## Dependencies

| Library | Version | Type | Setup |
|---------|---------|------|-------|
| Filament | 1.53.2 | Prebuilt `.so` | `setup_deps.sh` |
| miniaudio | latest | Header-only | `setup_deps.sh` |
| libsodium | 1.0.20 | Prebuilt `.so` | `setup_deps.sh` |

## License
MIT
