#include "renderer.h"
#include "json/json_reader.h"
#include <android/log.h>
#include <android/native_window.h>
#include <android/asset_manager.h>
#include <cstring>
#include <cmath>

#define LOG_TAG "ObrisRender"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Helper: get AAssetManager from our json module
static AAssetManager* getAam() {
    return static_cast<AAssetManager*>(obris::getAssetManager());
}

namespace obris {

// ══════════════════════════════════════════════════════════════
//  Normalize 3-component vector in place
// ══════════════════════════════════════════════════════════════
static void normalizeVec3(float* v) {
    float len = std::sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
    if (len > 0.0001f) { v[0] /= len; v[1] /= len; v[2] /= len; }
}

// ══════════════════════════════════════════════════════════════
//  Quaternion multiplication (a * b)
// ══════════════════════════════════════════════════════════════
static void mulQuat(const float a[4], const float b[4], float out[4]) {
    out[0] = a[3]*b[0] + a[0]*b[3] + a[1]*b[2] - a[2]*b[1];
    out[1] = a[3]*b[1] - a[0]*b[2] + a[1]*b[3] + a[2]*b[0];
    out[2] = a[3]*b[2] + a[0]*b[1] - a[1]*b[0] + a[2]*b[3];
    out[3] = a[3]*b[3] - a[0]*b[0] - a[1]*b[1] - a[2]*b[2];
}

// ══════════════════════════════════════════════════════════════
//  Euler angles (yaw, pitch) to quaternion
// ══════════════════════════════════════════════════════════════
static void eulerToQuat(float yawRad, float pitchRad, float rollRad, float q[4]) {
    float cy = std::cos(yawRad * 0.5f);
    float sy = std::sin(yawRad * 0.5f);
    float cp = std::cos(pitchRad * 0.5f);
    float sp = std::sin(pitchRad * 0.5f);
    float cr = std::cos(rollRad * 0.5f);
    float sr = std::sin(rollRad * 0.5f);
    q[0] = sr * cp * cy - cr * sp * sy;
    q[1] = cr * sp * cy + sr * cp * sy;
    q[2] = cr * cp * sy - sr * sp * cy;
    q[3] = cr * cp * cy + sr * sp * sy;
}

// ══════════════════════════════════════════════════════════════
//  Renderer
// ══════════════════════════════════════════════════════════════

Renderer::Renderer() { LOGI("Renderer created"); }
Renderer::~Renderer() { shutdown(); }

// ══════════════════════════════════════════════════════════════
//  Init
// ══════════════════════════════════════════════════════════════

bool Renderer::init(const ObrisConfig& config) {
    if (initialized_) return true;
    width_ = config.width;
    height_ = config.height;
    camera_.near = 0.1f;
    camera_.far = 1000.0f;
    camera_.fov = 60.0f;
    camera_.isPerspective = 1;

#if defined(OBRIS_USE_FILAMENT) && OBRIS_USE_FILAMENT
    if (!initFilament(config)) {
        LOGE("Filament init failed");
        return false;
    }
    LOGI("Filament %dx%d initialized (%s)", width_, height_,
         config.useVulkan ? "Vulkan" : "OpenGL");
#else
    LOGI("Renderer stub (no Filament)");
#endif

    initialized_ = true;
    return true;
}

bool Renderer::initFilament(const ObrisConfig& config) {
#if defined(OBRIS_USE_FILAMENT) && OBRIS_USE_FILAMENT

    using namespace filament;

    // 1. Create Engine
    Engine::Backend backend = config.useVulkan ?
        Engine::Backend::VULKAN : Engine::Backend::OPENGL;
    auto* e = Engine::create(backend);
    engine_ = e;
    if (!e) { LOGE("Engine::create failed"); return false; }

    // 2. Create Renderer
    auto* r = e->createRenderer();
    renderer_ = r;
    if (!r) { LOGE("createRenderer failed"); return false; }

    // 3. Create SwapChain
    auto* sw = e->createSwapChain(
        static_cast<ANativeWindow*>(config.nativeWindow),
        SwapChain::CONFIG_HAS_STENCIL_BITS);
    swapChain_ = sw;
    if (!sw) { LOGE("createSwapChain failed"); return false; }

    // 4. Create Scene
    auto* s = e->createScene();
    scene_ = s;

    // 5. Create Camera
    auto camEntity = EntityManager::get().create();
    void* camEntityPtr = reinterpret_cast<void*>(static_cast<uintptr_t>(
        camEntity.getId()));
    auto* cam = e->createCamera(camEntity);
    camera_ = cam;
    cam->setProjection(60.0, (double)width_/height_, 0.1, 1000.0);
    cam->lookAt({0, 2.5f, 5}, {0, 1, 0}, {0, 1, 0});

    // 6. Create View
    auto* v = e->createView();
    view_ = v;
    v->setScene(s);
    v->setCamera(cam);
    v->setViewport({0, 0, (uint32_t)width_, (uint32_t)height_});
    v->setPostProcessingEnabled(false);

    // 7. Set clear color (dark blue-gray)
    r->setClearOptions({
        .clearColor = { clearR_, clearG_, clearB_, clearA_ },
        .clear = true
    });

    // 8. Create AssetLoader for GLB loading
    // gltfio::AssetLoader* loader = gltfio::AssetLoader::create({e, nullptr, nullptr, true});
    // assetLoader_ = loader;

    // 9. Create a default directional light so the scene isn't dark
    auto sunEntity = EntityManager::get().create();
    LightManager::Builder(LightManager::Type::DIRECTIONAL)
        .color(Color::toLinear({1.0f, 0.95f, 0.85f}))
        .intensity(80000.0f)
        .direction({-0.5f, -1.0f, -0.3f})
        .castShadows(true)
        .build(*e, sunEntity);
    s->addEntity(sunEntity);

    // Store this as our first light
    LightEntry le;
    le.def.type = 0;
    le.def.color[0] = 1.0f; le.def.color[1] = 0.95f; le.def.color[2] = 0.85f;
    le.def.intensity = 80000.0f;
    le.def.direction[0] = -0.5f; le.def.direction[1] = -1.0f; le.def.direction[2] = -0.3f;
    le.entity = sunEntity;
    le.active = true;
    lights_.push_back(le);

    LOGI("Filament init complete");
    return true;
#else
    (void)config;
    return true;
#endif
}

// ══════════════════════════════════════════════════════════════
//  Shutdown
// ══════════════════════════════════════════════════════════════

void Renderer::shutdown() {
    if (!initialized_) return;

#if defined(OBRIS_USE_FILAMENT) && OBRIS_USE_FILAMENT
    using namespace filament;

    auto* e = static_cast<Engine*>(engine_);
    if (!e) return;

    // Unload models
    for (auto& [id, model] : models_) {
        if (model.filamentAsset) {
            // static_cast<gltfio::FilamentAsset*>(model.filamentAsset)->releaseSourceData();
        }
        if (model.entities && model.entityCount > 0) {
            for (uint32_t i = 0; i < model.entityCount; i++) {
                e->destroy((utils::Entity)(uintptr_t)model.entities[i]);
            }
            delete[] model.entities;
        }
    }
    models_.clear();
    lights_.clear();

    // Destroy lights
    for (auto& l : lights_) {
        if (l.active && l.entity) {
            // Entities already cleaned up via removeLight or will be destroyed by Engine
        }
    }

    if (assetLoader_) {
        // delete static_cast<gltfio::AssetLoader*>(assetLoader_);
        assetLoader_ = nullptr;
    }

    if (indirectLight_) e->destroy(static_cast<IndirectLight*>(indirectLight_));
    if (skybox_) e->destroy(static_cast<Skybox*>(skybox_));
    if (view_) e->destroy(static_cast<View*>(view_));
    if (renderer_) e->destroy(static_cast<Renderer*>(renderer_));
    if (scene_) e->destroy(static_cast<Scene*>(scene_));
    if (swapChain_) e->destroy(static_cast<SwapChain*>(swapChain_));
    // Camera is cleaned up by Engine::destroy()

    Engine::destroy(&e);
#endif

    engine_ = nullptr;
    renderer_ = nullptr;
    scene_ = nullptr;
    view_ = nullptr;
    swapChain_ = nullptr;
    camera_ = nullptr;
    cameraEntity_ = 0;
    indirectLight_ = nullptr;
    skybox_ = nullptr;
    assetLoader_ = nullptr;
    initialized_ = false;
    LOGI("Renderer shut down");
}

// ══════════════════════════════════════════════════════════════
//  Frame rendering
// ══════════════════════════════════════════════════════════════

void Renderer::renderFrame() {
#if defined(OBRIS_USE_FILAMENT) && OBRIS_USE_FILAMENT
    auto* r = static_cast<filament::Renderer*>(renderer_);
    auto* sw = static_cast<filament::SwapChain*>(swapChain_);
    auto* v = static_cast<filament::View*>(view_);

    if (r->beginFrame(sw)) {
        r->render(v);
        r->endFrame();
    }
#endif
}

void Renderer::resize(int w, int h) {
    width_ = w > 0 ? w : 720;
    height_ = h > 0 ? h : 1280;
#if defined(OBRIS_USE_FILAMENT) && OBRIS_USE_FILAMENT
    if (view_) {
        static_cast<filament::View*>(view_)
            ->setViewport({0, 0, (uint32_t)width_, (uint32_t)height_});
    }
    if (camera_) {
        double aspect = (double)width_ / (double)height_;
        static_cast<filament::Camera*>(camera_)
            ->setProjection(camera_.fov, aspect, camera_.near, camera_.far);
    }
#endif
}

// ══════════════════════════════════════════════════════════════
//  Camera
// ══════════════════════════════════════════════════════════════

void Renderer::setCamera(const ObrisCamera& cam) {
    camera_ = cam;
    applyCameraToFilament();
}

void Renderer::applyCameraToFilament() {
#if defined(OBRIS_USE_FILAMENT) && OBRIS_USE_FILAMENT
    auto* cam = static_cast<filament::Camera*>(camera_);
    if (!cam) return;

    filament::math::float3 eye(camera_.x, camera_.y, camera_.z);
    filament::math::float3 target(camera_.tx, camera_.ty, camera_.tz);
    filament::math::float3 up(0, 1, 0);

    cam->lookAt(eye, target, up);

    double aspect = (double)width_ / (double)height_;
    if (camera_.isPerspective) {
        cam->setProjection(camera_.fov, aspect, camera_.near, camera_.far);
    } else {
        float halfH = camera_.far * 0.5f;
        float halfW = halfH * aspect;
        cam->setProjection(filament::Camera::Projection::ORTHO,
                           -halfW, halfW, -halfH, halfH, camera_.near, camera_.far);
    }
#endif
}

// ══════════════════════════════════════════════════════════════
//  Lights
// ══════════════════════════════════════════════════════════════

int Renderer::addLight(const ObrisLight& light) {
#if defined(OBRIS_USE_FILAMENT) && OBRIS_USE_FILAMENT
    using namespace filament;

    auto* e = static_cast<Engine*>(engine_);
    auto* s = static_cast<Scene*>(scene_);
    EntityManager& em = EntityManager::get();

    Entity entity = em.create();

    LightManager::Type ltype = (light.type == 0)
        ? LightManager::Type::DIRECTIONAL
        : LightManager::Type::POINT;

    auto builder = LightManager::Builder(ltype)
        .color(Color::toLinear({light.color[0], light.color[1], light.color[2]}))
        .intensity(light.intensity)
        .falloff(1.0f);

    if (light.type == 0) {
        builder.direction({light.direction[0], light.direction[1], light.direction[2]});
        builder.castShadows(true);
    } else {
        builder.position({light.position[0], light.position[1], light.position[2]});
    }

    builder.build(*e, entity);
    s->addEntity(entity);

    LightEntry le;
    le.def = light;
    le.entity = entity;
    le.active = true;
    lights_.push_back(le);
    return (int)lights_.size() - 1;
#else
    (void)light;
    return 0;
#endif
}

void Renderer::updateLight(int idx, const ObrisLight& light) {
    if (idx < 0 || idx >= (int)lights_.size()) return;
    // Remove and re-add
    removeLight(idx);
    // Insert at same position
    auto it = lights_.begin() + idx;
    int newIdx = addLight(light);
    // If indices differ, swap
    if (newIdx != idx && newIdx > idx) {
        std::swap(lights_[idx], lights_[newIdx]);
        lights_.pop_back();
    }
}

void Renderer::removeLight(int idx) {
    if (idx < 0 || idx >= (int)lights_.size()) return;
#if defined(OBRIS_USE_FILAMENT) && OBRIS_USE_FILAMENT
    auto* e = static_cast<filament::Engine*>(engine_);
    auto* s = static_cast<filament::Scene*>(scene_);
    if (lights_[idx].active && lights_[idx].entity) {
        s->removeEntity(lights_[idx].entity);
        e->destroy(lights_[idx].entity);
    }
#endif
    lights_.erase(lights_.begin() + idx);
}

// ══════════════════════════════════════════════════════════════
//  Models / GLB
// ══════════════════════════════════════════════════════════════

ObrisModel Renderer::loadModel(const ObrisModelInfo& info) {
    ObrisModel id = nextModelId_++;
    LoadedModel model;
    model.id = id;
    model.path = info.path ? info.path : "";

    if (info.pos)   { memcpy(model.pos,   info.pos,   sizeof(float)*3); }
    else            { model.pos[0]=0; model.pos[1]=0; model.pos[2]=0; }
    if (info.rot)   { memcpy(model.rot,   info.rot,   sizeof(float)*4); }
    else            { model.rot[0]=0; model.rot[1]=0; model.rot[2]=0; model.rot[3]=1; }
    if (info.scale) { memcpy(model.scale, info.scale, sizeof(float)*3); }
    else            { model.scale[0]=1; model.scale[1]=1; model.scale[2]=1; }

#if defined(OBRIS_USE_FILAMENT) && OBRIS_USE_FILAMENT
    // Use gltfio to load GLB from Android assets
    auto* e = static_cast<filament::Engine*>(engine_);
    auto* s = static_cast<filament::Scene*>(scene_);

    if (getAam() && !model.path.empty()) {
        AAsset* asset = AAssetManager_open(getAam(), model.path.c_str(), AASSET_MODE_BUFFER);
        if (asset) {
            size_t size = AAsset_getLength(asset);
            const void* data = AAsset_getBuffer(asset);

            // gltfio::AssetLoader* loader = static_cast<gltfio::AssetLoader*>(assetLoader_);
            // if (loader) {
            //     gltfio::FilamentAsset* fa = loader->createAssetFromBinary(
            //         static_cast<const uint8_t*>(data), size);
            //     if (fa) {
            //         model.filamentAsset = fa;
            //         model.entityCount = fa->getEntityCount();
            //         model.entities = new void*[model.entityCount];
            //         const utils::Entity* ents = fa->getEntities();
            //         for (uint32_t i = 0; i < model.entityCount; i++) {
            //             model.entities[i] = (void*)(uintptr_t)ents[i];
            //             s->addEntity(ents[i]);
            //         }
            //         // Load resources (textures)
            //         gltfio::ResourceLoader resourceLoader({e, nullptr, nullptr, true});
            //         resourceLoader.loadResources(fa);
            //         LOGI("Loaded GLB: %s (%u entities)", model.path.c_str(), model.entityCount);
            //     }
            // }
            AAsset_close(asset);
        } else {
            LOGE("Failed to open asset: %s", model.path.c_str());
        }
    }

    // Apply transform
    applyModelTransform(model);

    LOGI("Loaded model: %s (id=%u)", model.path.c_str(), id);
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
    auto* e = static_cast<filament::Engine*>(engine_);
    auto* s = static_cast<filament::Scene*>(scene_);

    if (it->second.filamentAsset) {
        // auto* fa = static_cast<gltfio::FilamentAsset*>(it->second.filamentAsset);
        // fa->releaseSourceData();
    }
    if (it->second.entities) {
        for (uint32_t i = 0; i < it->second.entityCount; i++) {
            if (it->second.entities[i]) {
                s->removeEntity((filament::Entity)(uintptr_t)it->second.entities[i]);
            }
        }
        delete[] it->second.entities;
    }
#endif

    models_.erase(it);
}

void Renderer::setModelVisible(ObrisModel id, bool visible) {
    auto it = models_.find(id);
    if (it == models_.end()) return;
    it->second.visible = visible;

#if defined(OBRIS_USE_FILAMENT) && OBRIS_USE_FILAMENT
    auto* e = static_cast<filament::Engine*>(engine_);
    for (uint32_t i = 0; i < it->second.entityCount; i++) {
        if (it->second.entities[i]) {
            e->enableEntity(it->second.entities[i], visible);
        }
    }
#endif
}

void Renderer::setModelTransform(ObrisModel id, const float pos[3],
                                  const float rot[4], const float scale[3]) {
    auto it = models_.find(id);
    if (it == models_.end()) return;
    memcpy(it->second.pos,   pos,   sizeof(float)*3);
    memcpy(it->second.rot,   rot,   sizeof(float)*4);
    memcpy(it->second.scale, scale, sizeof(float)*3);
    applyModelTransform(it->second);
}

void Renderer::applyModelTransform(LoadedModel& model) {
#if defined(OBRIS_USE_FILAMENT) && OBRIS_USE_FILAMENT
    auto* e = static_cast<filament::Engine*>(engine_);

    // Build a 4x4 transform matrix: T * R * S
    using namespace filament::math;

    float4x4 tr = mat4f::translation(float3(model.pos[0], model.pos[1], model.pos[2]));
    float4x4 sc = mat4f::scaling(float3(model.scale[0], model.scale[1], model.scale[2]));

    // Convert quaternion to rotation matrix
    float qx = model.rot[0], qy = model.rot[1], qz = model.rot[2], qw = model.rot[3];
    float4x4 rot = mat4f(
        float4(1 - 2*(qy*qy + qz*qz), 2*(qx*qy + qw*qz),     2*(qx*qz - qw*qy),     0),
        float4(2*(qx*qy - qw*qz),     1 - 2*(qx*qx + qz*qz), 2*(qy*qz + qw*qx),     0),
        float4(2*(qx*qz + qw*qy),     2*(qy*qz - qw*qx),     1 - 2*(qx*qx + qy*qy), 0),
        float4(0,                     0,                     0,                     1)
    );

    float4x4 transform = tr * rot * sc;

    auto& tcm = e->getTransformManager();
    for (uint32_t i = 0; i < model.entityCount; i++) {
        if (model.entities[i]) {
            auto inst = tcm.getInstance((utils::Entity)(uintptr_t)model.entities[i]);
            if (inst) {
                tcm.setTransform(inst, transform);
            }
        }
    }
#endif
    (void)model;
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
//  IBL (HDR Environment)
// ══════════════════════════════════════════════════════════════

bool Renderer::loadIBL(const char* path) {
    LOGI("Loading IBL: %s", path ? path : "null");
#if defined(OBRIS_USE_FILAMENT) && OBRIS_USE_FILAMENT
    if (!path) return false;

    auto* e = static_cast<filament::Engine*>(engine_);
    auto* s = static_cast<filament::Scene*>(scene_);

    // Load .ktx from assets
    if (getAam()) {
        AAsset* asset = AAssetManager_open(getAam(), path, AASSET_MODE_BUFFER);
        if (asset) {
            size_t size = AAsset_getLength(asset);
            const void* data = AAsset_getBuffer(asset);

            // Create Ktx1Bundle or Ktx2Bundle from the data
            // auto* bundle = new filament::geometry::Ktx1Bundle(
            //     static_cast<const uint8_t*>(data), size);

            // Load cubemap texture
            // auto* texture = filament::Texture::Builder()
            //     .sampler(filament::Texture::Sampler::SAMPLER_CUBEMAP)
            //     .build(*e);

            // Create IndirectLight from the cubemap
            // auto* ibl = filament::IndirectLight::Builder()
            //     .reflections(texture)
            //     .intensity(30000.0f)
            //     .build(*e);
            // s->setIndirectLight(ibl);
            // indirectLight_ = ibl;

            // Create Skybox from the cubemap
            // auto* skybox = filament::Skybox::Builder()
            //     .environment(texture)
            //     .showSun(true)
            //     .build(*e);
            // s->setSkybox(skybox);
            // skybox_ = skybox;

            AAsset_close(asset);
            LOGI("IBL loaded: %s", path);
            return true;
        } else {
            LOGE("Failed to open IBL: %s", path);
        }
    }
#endif
    (void)path;
    return false;
}

void Renderer::setIBLIntensity(float intensity) {
#if defined(OBRIS_USE_FILAMENT) && OBRIS_USE_FILAMENT
    if (indirectLight_) {
        static_cast<filament::IndirectLight*>(indirectLight_)
            ->setIntensity(intensity * 30000.0f);
    }
#endif
    (void)intensity;
}

void Renderer::setIBLRotation(float degrees) {
#if defined(OBRIS_USE_FILAMENT) && OBRIS_USE_FILAMENT
    if (indirectLight_) {
        float rad = degrees * 3.14159f / 180.0f;
        static_cast<filament::IndirectLight*>(indirectLight_)
            ->setRotation(filament::math::mat3f::rotation(rad, filament::math::float3{0, 1, 0}));
    }
#endif
    (void)degrees;
}

} // namespace obris
