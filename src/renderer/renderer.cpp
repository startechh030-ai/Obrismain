#include "renderer.h"
#include "json/json_reader.h"
#include <filament/Engine.h>
#include <filament/Renderer.h>
#include <filament/Scene.h>
#include <filament/View.h>
#include <filament/Camera.h>
#include <filament/SwapChain.h>
#include <filament/LightManager.h>
#include <filament/Skybox.h>
#include <filament/IndirectLight.h>
#include <filament/Texture.h>
#include <filament/Color.h>
#include <filament/TransformManager.h>
#include <filament/RenderableManager.h>
#include <filament/Viewport.h>
#include <filament/Options.h>
#include <filament/Frustum.h>
#include <filament/VertexBuffer.h>
#include <filament/IndexBuffer.h>
#include <filament/Material.h>
#include <filament/MaterialInstance.h>
#include <filament/FilamentAPI.h>
#include <utils/Entity.h>
#include <utils/EntityManager.h>
#include <math/vec3.h>
#include <math/vec4.h>
#include <math/mat4.h>
#include <math/mat3.h>
#include <math/quat.h>
#include <math/norm.h>
#include <gltfio/AssetLoader.h>
#include <gltfio/ResourceLoader.h>
#include <gltfio/FilamentAsset.h>
#include <gltfio/MaterialProvider.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/asset_manager.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <exception>
#include <jni.h>

#define LOG_TAG "ObrisRender"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Helper: get AAssetManager from our json module
static AAssetManager* getAam() {
    return static_cast<AAssetManager*>(obris::getAssetManager());
}

// ── JNI wrappers exported from libfilament-jni.so ────────────
extern "C" {
    jlong Java_com_google_android_filament_Engine_nCreateBuilder(JNIEnv*, jclass);
    void  Java_com_google_android_filament_Engine_nSetBuilderBackend(JNIEnv*, jclass, jlong builder, jint backend);
    jlong Java_com_google_android_filament_Engine_nBuilderBuild(JNIEnv*, jclass, jlong builder);
    void  Java_com_google_android_filament_Engine_nDestroyBuilder(JNIEnv*, jclass, jlong builder);
    void  Java_com_google_android_filament_Engine_nDestroyEngine(JNIEnv*, jclass, jlong engine);
}

namespace obris {

// ══════════════════════════════════════════════════════════════
//  Renderer Constructor / Destructor
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
    cameraState_.near = 0.1f;
    cameraState_.far = 1000.0f;
    cameraState_.fov = 60.0f;
    cameraState_.isPerspective = 1;

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
    using namespace filament::math;

    JNIEnv* env = static_cast<JNIEnv*>(config.env);

    // 1. Create Engine via Builder pattern (JNI wrapper)
    jlong builder = Java_com_google_android_filament_Engine_nCreateBuilder(env, nullptr);
    if (!builder) { LOGE("Engine builder creation failed"); return false; }
    
    Java_com_google_android_filament_Engine_nSetBuilderBackend(
        env, nullptr, builder,
        static_cast<jint>(config.useVulkan ? 1 : 0));
    
    auto* e = reinterpret_cast<Engine*>(
        Java_com_google_android_filament_Engine_nBuilderBuild(env, nullptr, builder));
    Java_com_google_android_filament_Engine_nDestroyBuilder(env, nullptr, builder);
    
    engine_ = e;
    if (!e) { LOGE("Engine::create failed via JNI builder"); return false; }

    // 2. Create Renderer
    auto* r = e->createRenderer();
    renderer_ = r;
    if (!r) { LOGE("createRenderer failed"); return false; }

    // 3. Create SwapChain
    if (!config.nativeWindow) { LOGE("config.nativeWindow is null"); return false; }
    auto* sw = e->createSwapChain(
        static_cast<ANativeWindow*>(config.nativeWindow),
        SwapChain::CONFIG_HAS_STENCIL_BUFFER);
    swapChain_ = sw;
    if (!sw) { LOGE("createSwapChain failed"); return false; }

    // 4. Create Scene
    auto* s = e->createScene();
    scene_ = s;
    if (!s) { LOGE("createScene failed"); return false; }

    // 5. Create Camera (Angled viewport looking at origin)
    utils::Entity camEntity = utils::EntityManager::get().create();
    if (camEntity.isNull()) { LOGE("camEntity is null"); return false; }
    cameraEntity_ = camEntity.getId();
    auto* cam = e->createCamera(camEntity);
    filamentCamera_ = cam;
    if (!cam) { LOGE("createCamera failed"); return false; }
    cam->setProjection(60.0, (double)width_/height_, 0.1, 1000.0);
    cam->lookAt({0.0f, 2.5f, 5.0f}, {0.0f, 0.5f, 0.0f}, {0.0f, 1.0f, 0.0f});

    // 6. Create View
    auto* v = e->createView();
    view_ = v;
    if (!v) { LOGE("createView failed"); return false; }
    v->setScene(s);
    v->setCamera(cam);
    v->setViewport({0, 0, (uint32_t)width_, (uint32_t)height_});
    v->setPostProcessingEnabled(false);

    // 7. Studio Gray Viewport Background
    r->setClearOptions({
        .clearColor = { 0.22f, 0.23f, 0.25f, 1.0f },
        .clear = true
    });

    auto* skybox = Skybox::Builder()
        .color({ 0.22f, 0.23f, 0.25f, 1.0f })
        .build(*e);
    if (skybox) {
        s->setSkybox(skybox);
        skybox_ = skybox;
    }

    // 8. Directional Sun Light (Daylight with shadows)
    utils::Entity sunEntity = utils::EntityManager::get().create();
    if (!sunEntity.isNull()) {
        LightManager::Builder(LightManager::Type::DIRECTIONAL)
            .color(Color::toLinear({1.0f, 0.98f, 0.94f}))
            .intensity(110000.0f)
            .direction({-0.6f, -1.0f, -0.4f})
            .castShadows(true)
            .build(*e, sunEntity);
        s->addEntity(sunEntity);

        LightEntry leSun;
        leSun.entityId = sunEntity.getId(); leSun.active = true;
        lights_.push_back(leSun);
    }

    // 9. Fill Sky Light (Soft secondary fill)
    utils::Entity fillEntity = utils::EntityManager::get().create();
    if (!fillEntity.isNull()) {
        LightManager::Builder(LightManager::Type::DIRECTIONAL)
            .color(Color::toLinear({0.5f, 0.65f, 0.85f}))
            .intensity(35000.0f)
            .direction({0.6f, 0.8f, 0.5f})
            .castShadows(false)
            .build(*e, fillEntity);
        s->addEntity(fillEntity);

        LightEntry leFill;
        leFill.entityId = fillEntity.getId(); leFill.active = true;
        lights_.push_back(leFill);
    }

    // 10. Load precompiled grid.filamat from assets if present
    AAssetManager* aam = getAam();
    if (aam) {
        AAsset* matAsset = AAssetManager_open(aam, "grid.filamat", AASSET_MODE_BUFFER);
        if (matAsset) {
            size_t size = AAsset_getLength(matAsset);
            const void* data = AAsset_getBuffer(matAsset);
            if (data && size > 0) {
                auto* mat = Material::Builder()
                    .package(data, size)
                    .build(*e);

                if (mat) {
                    unlitMaterial_ = mat;
                    unlitMaterialInstance_ = mat->createInstance();
                    LOGI("Loaded precompiled grid.filamat (%zu bytes)", size);
                } else {
                    LOGE("Failed to build Material from grid.filamat");
                }
            }
            AAsset_close(matAsset);
        } else {
            LOGI("grid.filamat not present in assets");
        }
    }

    LOGI("Filament init complete (all null checks passed)");
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

    auto* s = static_cast<Scene*>(scene_);

    if (unlitMaterialInstance_) { e->destroy(static_cast<MaterialInstance*>(unlitMaterialInstance_)); unlitMaterialInstance_ = nullptr; }
    if (unlitMaterial_) { e->destroy(static_cast<Material*>(unlitMaterial_)); unlitMaterial_ = nullptr; }

    // Unload models
    for (auto& [id, model] : models_) {
        if (model.filamentAsset && assetLoader_) {
            auto* loader = static_cast<filament::gltfio::AssetLoader*>(assetLoader_);
            auto* fa = static_cast<filament::gltfio::FilamentAsset*>(model.filamentAsset);
            if (s && fa) {
                s->removeEntities(fa->getEntities(), fa->getEntityCount());
            }
            if (loader && fa) loader->destroyAsset(fa);
            model.filamentAsset = nullptr;
        }
        if (model.entities && model.entityCount > 0) {
            delete[] static_cast<utils::Entity*>(model.entities);
            model.entities = nullptr;
        }
    }
    models_.clear();

    if (assetLoader_) {
        auto* loader = static_cast<filament::gltfio::AssetLoader*>(assetLoader_);
        filament::gltfio::AssetLoader::destroy(&loader);
        assetLoader_ = nullptr;
    }

    // Destroy lights
    for (auto& l : lights_) {
        if (l.active && l.entityId != 0) {
            utils::Entity ent = utils::Entity::import(l.entityId);
            if (s && !ent.isNull()) s->remove(ent);
            if (e && !ent.isNull()) e->destroy(ent);
        }
    }
    lights_.clear();

    if (indirectLight_) e->destroy(static_cast<IndirectLight*>(indirectLight_));
    if (skybox_) e->destroy(static_cast<Skybox*>(skybox_));
    if (view_) e->destroy(static_cast<View*>(view_));
    if (renderer_) e->destroy(static_cast<filament::Renderer*>(renderer_));
    if (scene_) e->destroy(static_cast<Scene*>(scene_));
    if (swapChain_) e->destroy(static_cast<SwapChain*>(swapChain_));

    if (cameraEntity_ != 0) {
        utils::Entity camEnt = utils::Entity::import(cameraEntity_);
        if (!camEnt.isNull()) e->destroy(camEnt);
    }

    Engine::destroy(&e);
#endif

    engine_ = nullptr;
    renderer_ = nullptr;
    scene_ = nullptr;
    view_ = nullptr;
    swapChain_ = nullptr;
    filamentCamera_ = nullptr;
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

    if (r && sw && v && r->beginFrame(sw)) {
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
    if (filamentCamera_) {
        double aspect = (double)width_ / (double)height_;
        static_cast<filament::Camera*>(filamentCamera_)
            ->setProjection(cameraState_.fov, aspect, cameraState_.near, cameraState_.far);
    }
#endif
}

// ══════════════════════════════════════════════════════════════
//  Camera
// ══════════════════════════════════════════════════════════════

void Renderer::setCamera(const ObrisCamera& cam) {
    cameraState_ = cam;
    applyCameraToFilament();
}

void Renderer::applyCameraToFilament() {
#if defined(OBRIS_USE_FILAMENT) && OBRIS_USE_FILAMENT
    auto* cam = static_cast<filament::Camera*>(filamentCamera_);
    if (!cam) return;

    filament::math::float3 eye(cameraState_.x, cameraState_.y, cameraState_.z);
    filament::math::float3 target(cameraState_.tx, cameraState_.ty, cameraState_.tz);
    filament::math::float3 up(0, 1, 0);

    cam->lookAt(eye, target, up);

    double aspect = (double)width_ / (double)height_;
    if (cameraState_.isPerspective) {
        cam->setProjection(cameraState_.fov, aspect, cameraState_.near, cameraState_.far);
    } else {
        float halfH = cameraState_.far * 0.5f;
        float halfW = halfH * aspect;
        cam->setProjection(filament::Camera::Projection::ORTHO,
                           -halfW, halfW, -halfH, halfH, cameraState_.near, cameraState_.far);
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
    if (!e || !s) return -1;

    utils::EntityManager& em = utils::EntityManager::get();
    utils::Entity entity = em.create();
    if (entity.isNull()) return -1;

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
    le.entityId = entity.getId();
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
    removeLight(idx);
    int newIdx = addLight(light);
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
    if (lights_[idx].active && lights_[idx].entityId != 0) {
        utils::Entity ent = utils::Entity::import(lights_[idx].entityId);
        if (s && !ent.isNull()) s->remove(ent);
        if (e && !ent.isNull()) e->destroy(ent);
    }
#endif
    lights_.erase(lights_.begin() + idx);
}

// ══════════════════════════════════════════════════════════════
//  Models / GLB Loading via gltfio
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
    AAssetManager* aam = getAam();
    auto* e = static_cast<filament::Engine*>(engine_);
    auto* s = static_cast<filament::Scene*>(scene_);

    if (aam && e && s && info.path && info.path[0] != '\0') {
        AAsset* glbAsset = AAssetManager_open(aam, info.path, AASSET_MODE_BUFFER);
        if (glbAsset) {
            size_t size = AAsset_getLength(glbAsset);
            const void* data = AAsset_getBuffer(glbAsset);

            if (!assetLoader_) {
                auto* materials = filament::gltfio::createUbershaderProvider(e, nullptr, 0);
                if (materials) {
                    filament::gltfio::AssetConfiguration config;
                    config.engine = e;
                    config.materials = materials;
                    assetLoader_ = filament::gltfio::AssetLoader::create(config);
                }
            }

            auto* loader = static_cast<filament::gltfio::AssetLoader*>(assetLoader_);
            if (loader && data && size > 0) {
                auto* fa = loader->createAsset(static_cast<const uint8_t*>(data), (uint32_t)size);
                if (fa) {
                    model.filamentAsset = fa;

                    filament::gltfio::ResourceConfiguration resConfig;
                    resConfig.engine = e;
                    resConfig.gltfPath = info.path;
                    resConfig.normalizeSkinningWeights = true;

                    filament::gltfio::ResourceLoader resourceLoader(resConfig);
                    resourceLoader.loadResources(fa);

                    uint32_t count = (uint32_t)fa->getEntityCount();
                    if (count > 0 && fa->getEntities()) {
                        model.entityCount = count;
                        model.entities = new utils::Entity[count];
                        memcpy(model.entities, fa->getEntities(), sizeof(utils::Entity) * count);

                        s->addEntities(fa->getEntities(), count);
                        LOGI("Loaded GLB model via gltfio: %s (%u entities)", model.path.c_str(), count);
                    }
                } else {
                    LOGE("gltfio createAsset failed for: %s", model.path.c_str());
                }
            }
            AAsset_close(glbAsset);
        } else {
            LOGE("Failed to open GLB asset file: %s", model.path.c_str());
        }
    }

    applyModelTransform(model);
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

    if (it->second.filamentAsset && assetLoader_) {
        auto* loader = static_cast<filament::gltfio::AssetLoader*>(assetLoader_);
        auto* fa = static_cast<filament::gltfio::FilamentAsset*>(it->second.filamentAsset);
        if (s && fa) {
            s->removeEntities(fa->getEntities(), fa->getEntityCount());
        }
        if (loader && fa) loader->destroyAsset(fa);
        it->second.filamentAsset = nullptr;
    }

    if (it->second.entities) {
        delete[] static_cast<utils::Entity*>(it->second.entities);
        it->second.entities = nullptr;
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
    if (!e || !it->second.entities) return;

    auto& rm = e->getRenderableManager();
    auto* ents = static_cast<utils::Entity*>(it->second.entities);
    for (uint32_t i = 0; i < it->second.entityCount; i++) {
        if (!ents[i].isNull()) {
            auto inst = rm.getInstance(ents[i]);
            if (inst) {
                rm.setLayerMask(inst, visible ? 0xFF : 0x00, 0xFF);
            }
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
    using namespace filament::math;
    auto* e = static_cast<filament::Engine*>(engine_);
    if (!e || !model.entities) return;

    mat4f tr = mat4f::translation(float3(model.pos[0], model.pos[1], model.pos[2]));
    mat4f sc = mat4f::scaling(float3(model.scale[0], model.scale[1], model.scale[2]));
    mat4f rot(quatf(model.rot[3], model.rot[0], model.rot[1], model.rot[2]));
    mat4f transform = tr * rot * sc;

    auto& tcm = e->getTransformManager();
    auto* ents = static_cast<utils::Entity*>(model.entities);
    for (uint32_t i = 0; i < model.entityCount; i++) {
        if (!ents[i].isNull()) {
            auto inst = tcm.getInstance(ents[i]);
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
    LOGI("Play anim '%s' on model %u", name ? name : "", id);
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
        float rad = degrees * 3.14159265f / 180.0f;
        static_cast<filament::IndirectLight*>(indirectLight_)
            ->setRotation(filament::math::mat3f::rotation(rad, filament::math::float3{0, 1, 0}));
    }
#endif
    (void)degrees;
}

} // namespace obris
