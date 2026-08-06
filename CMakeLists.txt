cmake_minimum_required(VERSION 3.22)
project(obris
    VERSION 0.1.0
    DESCRIPTION "Obris — High-Performance Game Engine Utility Library"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE OBRIS_SOURCES
    src/*.cpp src/*.h
)

add_library(obris_shared SHARED ${OBRIS_SOURCES})

target_include_directories(obris_shared PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer
    ${CMAKE_CURRENT_SOURCE_DIR}/src/audio
    ${CMAKE_CURRENT_SOURCE_DIR}/src/json
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encryption
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform
)

include(cmake/FindObrisDeps.cmake)

target_link_libraries(obris_shared
    ${ANDROID_LOG_LIBRARY}
    ${ANDROID_VULKAN_LIBRARY}
    ${ANDROID_NATIVE_WINDOW_LIBRARY}
    ${ANDROID_OPENGLES_LIBRARY}
    ${ANDROID_EGL_LIBRARY}
    android
    nativewindow
)

if(TARGET filament::filament)
    target_link_libraries(obris_shared filament::filament)
    target_compile_definitions(obris_shared PRIVATE OBRIS_USE_FILAMENT=1)
endif()
if(TARGET filament::gltfio)
    target_link_libraries(obris_shared filament::gltfio)
    target_compile_definitions(obris_shared PRIVATE OBRIS_USE_GLTFIO=1)
endif()
if(TARGET miniaudio)
    target_link_libraries(obris_shared miniaudio)
    target_compile_definitions(obris_shared PRIVATE OBRIS_USE_MINIAUDIO=1)
endif()
if(TARGET sodium::sodium)
    target_link_libraries(obris_shared sodium::sodium)
    target_compile_definitions(obris_shared PRIVATE OBRIS_USE_SODIUM=1)
endif()
