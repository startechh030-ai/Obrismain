# =============================================================================
# Obris - Dependency Discovery
# =============================================================================

find_library(ANDROID_LOG_LIBRARY log REQUIRED)
find_library(ANDROID_VULKAN_LIBRARY vulkan)
find_library(ANDROID_NATIVE_WINDOW_LIBRARY nativewindow)
find_library(ANDROID_OPENGLES_LIBRARY GLESv3)
find_library(ANDROID_EGL_LIBRARY EGL)

# ── Filament ─────────────────────────────────────────────────
set(FILAMENT_DIR "${CMAKE_SOURCE_DIR}/third_party/filament")
if(EXISTS "${FILAMENT_DIR}")
    file(GLOB FILAMENT_SO "${FILAMENT_DIR}/lib/${ANDROID_ABI}/libfilament-jni.so")
    if(FILAMENT_SO)
        add_library(filament::filament UNKNOWN IMPORTED GLOBAL)
        set_target_properties(filament::filament PROPERTIES
            IMPORTED_LOCATION "${FILAMENT_SO}"
        )
        if(EXISTS "${FILAMENT_DIR}/include/filament/Engine.h")
            target_include_directories(filament::filament INTERFACE "${FILAMENT_DIR}/include")
            message(STATUS "Obris: ✅ Filament found (with headers)")
        else()
            message(STATUS "Obris: ✅ Filament .so found (headers missing — compile will fail)")
        endif()
        # gltfio
        file(GLOB GLTFIO_SO "${FILAMENT_DIR}/lib/${ANDROID_ABI}/libgltfio-jni.so")
        if(GLTFIO_SO)
            add_library(filament::gltfio UNKNOWN IMPORTED GLOBAL)
            set_target_properties(filament::gltfio PROPERTIES IMPORTED_LOCATION "${GLTFIO_SO}")
            message(STATUS "Obris: ✅ gltfio found")
        endif()
    else()
        message(WARNING "Obris: ❌ Filament .so not found for ${ANDROID_ABI}")
    endif()
else()
    message(WARNING "Obris: ❌ third_party/filament not found")
endif()

# ── miniaudio (header-only) ──────────────────────────────────
set(MINIAUDIO_DIR "${CMAKE_SOURCE_DIR}/third_party/miniaudio")
if(EXISTS "${MINIAUDIO_DIR}/miniaudio.h")
    add_library(miniaudio INTERFACE)
    target_include_directories(miniaudio INTERFACE "${MINIAUDIO_DIR}")
    message(STATUS "Obris: ✅ miniaudio found")
else()
    message(WARNING "Obris: ❌ miniaudio not found")
endif()

# ── libsodium ────────────────────────────────────────────────
set(SODIUM_DIR "${CMAKE_SOURCE_DIR}/third_party/libsodium")
if(EXISTS "${SODIUM_DIR}")
    file(GLOB SODIUM_SO "${SODIUM_DIR}/lib/${ANDROID_ABI}/libsodium.so")
    if(SODIUM_SO)
        add_library(sodium::sodium UNKNOWN IMPORTED GLOBAL)
        set_target_properties(sodium::sodium PROPERTIES IMPORTED_LOCATION "${SODIUM_SO}")
        target_include_directories(sodium::sodium INTERFACE "${SODIUM_DIR}/include")
        message(STATUS "Obris: ✅ libsodium found")
    else()
        message(WARNING "Obris: ❌ libsodium not found for ${ANDROID_ABI}")
    endif()
else()
    message(WARNING "Obris: ❌ third_party/libsodium not found")
endif()
