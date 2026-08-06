#include "renderer.h"
#include <android/log.h>

#define LOG_TAG "ObrisRender"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

namespace obris {

Renderer::Renderer() { LOGI("Native Renderer created"); }
Renderer::~Renderer() { shutdown(); }

bool Renderer::init(const ObrisConfig& config) {
    if (initialized_) return true;
    width_ = config.width;
    height_ = config.height;
    initialized_ = true;
    LOGI("Native Renderer initialized (%dx%d)", width_, height_);
    return true;
}

void Renderer::shutdown() {
    if (!initialized_) return;
    initialized_ = false;
    LOGI("Native Renderer shut down");
}

void Renderer::renderFrame() {
    // Render delegate
}

void Renderer::resize(int w, int h) {
    width_ = w > 0 ? w : 720;
    height_ = h > 0 ? h : 1280;
}

} // namespace obris
