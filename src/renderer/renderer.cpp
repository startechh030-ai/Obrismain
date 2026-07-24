#include "renderer.h"
#include <android/log.h>
#include <android/native_window.h>
#include <cstring>

#define LOG_TAG "ObrisRender"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace obris {

Renderer::Renderer() {
    LOGI("Renderer created");
}

Renderer::~Renderer() {
    shutdown();
}

// ══════════════════════════════════════════════════════════════
//  Init / Shutdown
// ══════════════════════════════════════════════════════════════

bool Renderer::init(const ObrisConfig& config) {
    if (initialized_) return true;
    width_ = config.width;
    height_ = config.height;

#if defined(OBRIS_USE_FILAMENT) && OBRIS_USE_FILAMENT
    if (!initFilament(config)) {
        LOGE("Filament initialization failed");
        return false;
    }
    LOGI("Filament %dx%d initialized (%s)", width_, height_,
         config.useVulkan ? "Vulkan" : "OpenGL");
#else
    LOGI("Renderer stub initialized (no Filament)");
#endif

    initialized_ = true;
    return true;
}

bool Renderer::initFilament(const ObrisConfig& config) {
#if defined(OBRIS_USE_FILAMENT) && OBRIS_USE_FILAMENT

    // 1. Create Engine
    filament::Engine::Backend backend = config.useVulkan ?
        filament::Engine::Backend::VULKAN : filament::Engine::Backend::OPENGL;
    engine_ = filament::Engine::create(backend);
    if (!engine_) { LOGE("Failed to create Engine"); return false; }
    auto* e = static_cast<filament::Engine*>(engine_);

    // 2. Create Renderer
    renderer_ = e->createRenderer();
    if (!renderer_) { LOGE("Failed to create Renderer"); return false; }
    auto* r = static_cast<filament::Renderer*>(renderer_);

    // 3. Create SwapChain
    swapChain_ = e->createSwapChain(
        static_cast<ANativeWindow*>(config.nativeWindow));
    if (!swapChain_) { LOGE("Failed to create SwapChain"); return false; }

    // 4. Create Scene
    scene_ = e->createScene();

    // 5. Create Camera
    auto camEntity = filament::EntityManager::get().create();
    cameraEntity_ = (void*)(uintptr_t)camEntity;
    auto* cam = e->createCamera(camEntity);

    // 6. Create View
    view_ = e->createView();
    if (!view_) { LOGE("Failed to create View"); return false; }
    auto* v = static_cast<filament::View*>(view_);
    v->setScene(static_cast<filament::Scene*>(scene_));
    v->setCamera(cam);
    v->setViewport({0, 0, (uint32_t)width_, (uint32_t)height_});
    v->setPostProcessingEnabled(false); // faster on mobile

    // 7. Load IBL if provided
    if (config.iblPath) {
        loadIBL(config.iblPath);
    }

    // 8. Set default clear color
    r->setClearOptions({
        .clearColor = { clearR_, clearG_, clearB_, clearA_ },
        .clear = true
    });

    LOGI("Filament init complete");
    return true;
#else
    (void)config;
    return true;
#endif
}

void Renderer::shutdown() {
    if (!initialized_) return;

    // Unload all models
    for (auto& [id, model] : models_) {
        unloadModel(id);
    }
    models_.clear();
    lights_.clear();

#if defined(OBRIS_USE_FILAMENT) && OBRIS_USE_FILAMENT

    if (engine_) {
        auto* e = static_cast<filament::Engine*>(engine_);

        if (indirectLight_) e->destroy(static_cast<filament::IndirectLight*>(indirectLight_));
        if (skybox_) e->destroy(static_cast<filament::Skybox*>(skybox_));
        if (view_) e->destroy(static_cast<filament::View*>(view_));
        if (renderer_) e->destroy(static_cast<filament::Renderer*>(renderer_));
        if (scene_) e->destroy(static_cast<filament::Scene*>(scene_));
        if (swapChain_) e->destroy(static_cast<filament::SwapChain*>(swapChain_));

        filament::Engine::destroy(&e);
        engine_ = nullptr;
    }
#endif

    engine_ = nullptr;
    renderer_ = nullptr;
    scene_ = nullptr;
    view_ = nullptr;
    swapChain_ = nullptr;
    cameraEntity_ = nullptr;
    indirectLight_ = nullptr;
    skybox_ = nullptr;
    initialized_ = false;
    LOGI("Renderer shut down");
}

// ══════════════════════════════════════════════════════════════
//  Frame
// ══════════════════════════════════════════════════════════════

void Renderer::renderFrame() {
#if defined(OBRIS_USE_FILAMENT) && OBRIS_USE_FILAMENT
    auto* r = static_cast<filament::Renderer*>(renderer_);
    auto* sw = static_cast<filament::SwapChain*>(swapChain_);

    if (r->beginFrame(sw)) {
        r->render(static_cast<filament::View*>(view_));
        r->endFrame();
    }
#endif
}

void Renderer::resize(int w, int h) {
    width_ = w;
    height_ = h;
#if defined(OBRIS_USE_FILAMENT) && OBRIS_USE_FILAMENT
    if (view_) {
        static_cast<filament::View*>(view_)->setViewport({0, 0, (uint32_t)w, (uint32_t)h});
    }
#endif
}

// ══════════════════════════════════════════════════════════════
//  Camera
// ══════════════════════════════════════════════════════════════

void Renderer::setCamera(const ObrisCamera& cam) {
    camera_ = cam;
#if defined(OBRIS_USE_FILAMENT) && OBRIS_USE_FILAMENT
    applyCameraToFilament();
#endif
}

void Renderer::applyCameraToFilament() {
#if defined(OBRIS_USE_FILAMENT) && OBRIS_USE_FILAMENT
    // TODO: Map ObrisCamera -> filament::Camera
    // float3 eye = {camera_.x, camera_.y, camera_.z};
    // float3 target = {camera_.tx, camera_.ty, camera_.tz};
    // float3 up = {0, 1, 0};
    // cam->lookAt(eye, target, up);
    // if (camera_.isPerspective)
    //     cam->setProjection(camera_.fov, aspect, camera_.near, camera_.far);
    // else
    //     cam->setProjection(Camera::Projection::ORTHO, ...);
#endif
}

// ══════════════════════════════════════════════════════════════
//  Lights
// ══════════════════════════════════════════════════════════════

int Renderer::addLight(const ObrisLight& light) {
    LightData ld;
    ld.def = light;
    lights_.push_back(ld);
    int idx = (int)lights_.size() - 1;

#if defined(OBRIS_USE_FILAMENT) && OBRIS_USE_FILAMENT
    // TODO: Create filament::LightManager light
    // filament::EntityManager::Entity entity = filament::EntityManager::get().create();
    // filament::LightManager::Builder(light.type == 0
    //     ? filament::LightManager::Type::DIRECTIONAL
    //     : filament::LightManager::Type::POINT)
    //     .color(filament::Color::toLinear({light.color[0], light.color[1], light.color[2]}))
    //     .intensity(light.intensity)
    //     .direction({light.direction[0], light.direction[1], light.direction[2]})
    //     .build(*static_cast<filament::Engine*>(engine_), entity);
    // static_cast<filament::Scene*>(scene_)->addEntity(entity);
#endif

    return idx;
}

void Renderer::updateLight(int idx, const ObrisLight& light) {
    if (idx < 0 || idx >= (int)lights_.size()) return;
    lights_[idx].def = light;
    // TODO: update filament light
}

void Renderer::removeLight(int idx) {
    if (idx < 0 || idx >= (int)lights_.size()) return;
    // TODO: remove from scene
    lights_.erase(lights_.begin() + idx);
}

// ══════════════════════════════════════════════════════════════
//  Models / GLB
// ══════════════════════════════════════════════════════════════

ObrisModel Renderer::loadModel(const ObrisModelInfo& info) {
    ObrisModel id = nextModelId_++;

    LoadedModel model;
    model.id = id;
    model.path = info.path;

    if (info.pos)    memcpy(model.pos,   info.pos,   sizeof(float)*3);
    if (info.rot)    memcpy(model.rot,   info.rot,   sizeof(float)*4);
    if (info.scale)  memcpy(model.scale, info.scale, sizeof(float)*3);

#if defined(OBRIS_USE_FILAMENT) && OBRIS_USE_FILAMENT
    // TODO: Use AssetLoader (gltfio) to load GLB from Android assets
    // AAsset* asset = AAssetManager_open(aam, info.path, AASSET_MODE_BUFFER);
    // auto* loader = gltfio::AssetLoader::create(...);
    // model.filamentAsset = loader->createAssetFromJson(data, size);
    // model.entity = model.filamentAsset->getEntities()[0];
    // scene->addEntity(entity);
    LOGI("Loading model: %s (id=%u) [stub]", info.path, id);
#else
    LOGI("Load model stub: %s (id=%u)", info.path, id);
#endif

    if (info.animName) {
        playAnimation(id, info.animName);
    }

    models_[id] = model;
    return id;
}

void Renderer::unloadModel(ObrisModel id) {
    auto it = models_.find(id);
    if (it == models_.end()) return;

#if defined(OBRIS_USE_FILAMENT) && OBRIS_USE_FILAMENT
    if (it->second.filamentAsset) {
        // TODO: destroy asset properly
        // static_cast<gltfio::FilamentAsset*>(it->second.filamentAsset)
        //     ->releaseSourceData();
    }
#endif

    models_.erase(it);
}

void Renderer::setModelVisible(ObrisModel id, bool visible) {
    auto it = models_.find(id);
    if (it == models_.end()) return;
    it->second.visible = visible;
    // TODO: EntityManager::setEnabled(entity, visible);
}

void Renderer::setModelTransform(ObrisModel id, const float pos[3],
                                  const float rot[4], const float scale[3]) {
    auto it = models_.find(id);
    if (it == models_.end()) return;
    memcpy(it->second.pos,   pos,   sizeof(float)*3);
    memcpy(it->second.rot,   rot,   sizeof(float)*4);
    memcpy(it->second.scale, scale, sizeof(float)*3);
    // TODO: apply transform to filament Entity
}

void Renderer::playAnimation(ObrisModel id, const char* name) {
    auto it = models_.find(id);
    if (it == models_.end()) return;
    it->second.currentAnim = name ? name : "";
    LOGI("Play anim '%s' on model %u [stub]", name, id);
}

void Renderer::stopAnimation(ObrisModel id) {
    auto it = models_.find(id);
    if (it == models_.end()) return;
    it->second.currentAnim.clear();
}

// ══════════════════════════════════════════════════════════════
//  IBL
// ══════════════════════════════════════════════════════════════

bool Renderer::loadIBL(const char* path) {
    LOGI("Loading IBL: %s [stub]", path);
#if defined(OBRIS_USE_FILAMENT) && OBRIS_USE_FILAMENT
    // TODO: Load .ktx from assets, create IndirectLight
    // using namespace filament;
    // auto* e = static_cast<Engine*>(engine_);
    // auto* s = static_cast<Scene*>(scene_);
    // auto* ibl = IndirectLight::Builder()
    //     .reflections(ktxTexture)
    //     .build(*e);
    // s->setIndirectLight(ibl);
    // indirectLight_ = ibl;
#endif
    (void)path;
    return true;
}

void Renderer::setIBLIntensity(float intensity) {
#if defined(OBRIS_USE_FILAMENT) && OBRIS_USE_FILAMENT
    // if (indirectLight_)
    //     static_cast<IndirectLight*>(indirectLight_)->setIntensity(intensity * 30000.0f);
#endif
    (void)intensity;
}

void Renderer::setIBLRotation(float degrees) {
#if defined(OBRIS_USE_FILAMENT) && OBRIS_USE_FILAMENT
    // if (indirectLight_)
    //     static_cast<IndirectLight*>(indirectLight_)->setRotation(...);
#endif
    (void)degrees;
}

} // namespace obris
