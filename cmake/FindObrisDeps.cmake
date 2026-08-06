# =============================================================================
# Obris - Dependency Discovery
# =============================================================================

find_library(ANDROID_LOG_LIBRARY log REQUIRED)
find_library(ANDROID_VULKAN_LIBRARY vulkan)
find_library(ANDROID_NATIVE_WINDOW_LIBRARY nativewindow)
find_library(ANDROID_OPENGLES_LIBRARY GLESv3)
find_library(ANDROID_EGL_LIBRARY EGL)

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
