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
#if defined(OBRIS_USE_FILAMAT) && OBRIS_USE_FILAMAT
#include <filamat/MaterialBuilder.h>
#endif
#include <android/log.h>
#include <android/native_window.h>
#include <android/asset_manager.h>
#include <cstring>
#include <cmath>
#include <vector>
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

// Vertex structures
struct GridVertex {
    filament::math::float3 position;
    filament::math::ubyte4 color;
};

struct CubeVertex {
    filament::math::float3 position;
    filament::math::ubyte4 color;
    filament::math::short4 tangent;
};

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

    // 1. Create Engine via Builder pattern (JNI wrapper)
    jlong builder = Java_com_google_android_filament_Engine_nCreateBuilder(nullptr, nullptr);
    if (!builder) { LOGE("Engine builder creation failed"); return false; }
    
    Java_com_google_android_filament_Engine_nSetBuilderBackend(
        nullptr, nullptr, builder,
        static_cast<jint>(config.useVulkan ? 1 : 0));
    
    auto* e = reinterpret_cast<Engine*>(
        Java_com_google_android_filament_Engine_nBuilderBuild(nullptr, nullptr, builder));
    Java_com_google_android_filament_Engine_nDestroyBuilder(nullptr, nullptr, builder);
    
    engine_ = e;
    if (!e) { LOGE("Engine::create failed via JNI builder"); return false; }

    // 2. Create Renderer
    auto* r = e->createRenderer();
    renderer_ = r;
    if (!r) { LOGE("createRenderer failed"); return false; }

    // 3. Create SwapChain
    auto* sw = e->createSwapChain(
        static_cast<ANativeWindow*>(config.nativeWindow),
        SwapChain::CONFIG_HAS_STENCIL_BUFFER);
    swapChain_ = sw;
    if (!sw) { LOGE("createSwapChain failed"); return false; }

    // 4. Create Scene
    auto* s = e->createScene();
    scene_ = s;

    // 5. Create Camera
    utils::Entity camEntity = utils::EntityManager::get().create();
    cameraEntity_ = camEntity.getId();
    auto* cam = e->createCamera(camEntity);
    filamentCamera_ = cam;
    cam->setProjection(60.0, (double)width_/height_, 0.1, 1000.0);
    cam->lookAt({0, 2.5f, 5.0f}, {0, 0.75f, 0.0f}, {0, 1.0f, 0.0f});

    // 6. Create View
    auto* v = e->createView();
    view_ = v;
    v->setScene(s);
    v->setCamera(cam);
    v->setViewport({0, 0, (uint32_t)width_, (uint32_t)height_});
    v->setPostProcessingEnabled(false);

    // 7. Set clear color & Skybox background
    r->setClearOptions({
        .clearColor = { 0.08f, 0.10f, 0.16f, 1.0f },
        .clear = true
    });

    auto* skybox = Skybox::Builder()
        .color({ 0.08f, 0.10f, 0.16f, 1.0f })
        .build(*e);
    s->setSkybox(skybox);
    skybox_ = skybox;

    // 8. Create Directional Sun Light (Primary daylight)
    utils::Entity sunEntity = utils::EntityManager::get().create();
    LightManager::Builder(LightManager::Type::DIRECTIONAL)
        .color(Color::toLinear({1.0f, 0.96f, 0.88f}))
        .intensity(120000.0f)
        .direction({-0.5f, -1.0f, -0.4f})
        .castShadows(true)
        .build(*e, sunEntity);
    s->addEntity(sunEntity);

    // 9. Create Fill Light (Secondary sky fill)
    utils::Entity fillEntity = utils::EntityManager::get().create();
    LightManager::Builder(LightManager::Type::DIRECTIONAL)
        .color(Color::toLinear({0.4f, 0.6f, 1.0f}))
        .intensity(40000.0f)
        .direction({0.5f, 0.8f, 0.5f})
        .castShadows(false)
        .build(*e, fillEntity);
    s->addEntity(fillEntity);

    // Store lights
    LightEntry leSun, leFill;
    leSun.entityId = sunEntity.getId(); leSun.active = true;
    leFill.entityId = fillEntity.getId(); leFill.active = true;
    lights_.push_back(leSun);
    lights_.push_back(leFill);

    // 10. Create procedural Grid Floor, Axis Gizmo & Center 3D Cube
    createProceduralObjects();

    LOGI("Filament init complete with grid floor, gizmo & 3D cube");
    return true;
#else
    (void)config;
    return true;
#endif
}

// ══════════════════════════════════════════════════════════════
//  Create Procedural Objects (Grid, Gizmo, 3D Center Cube)
// ══════════════════════════════════════════════════════════════

void Renderer::createProceduralObjects() {
#if defined(OBRIS_USE_FILAMENT) && OBRIS_USE_FILAMENT && defined(OBRIS_USE_FILAMAT) && OBRIS_USE_FILAMAT
    using namespace filament;
    using namespace filament::math;

    auto* e = static_cast<Engine*>(engine_);
    auto* s = static_cast<Scene*>(scene_);
    if (!e || !s) return;

    // ── Build Materials via Filamat ───────────────────────────
    filamat::MaterialBuilder unlitBuilder;
    unlitBuilder.name("UnlitGridMat")
                .material("void material(inout MaterialInputs material) { prepareMaterial(material); material.baseColor = getColor(); }")
                .shading(filamat::MaterialBuilder::Shading::UNLIT)
                .require(VertexAttribute::POSITION)
                .require(VertexAttribute::COLOR)
                .targetApi(filamat::MaterialBuilder::TargetApi::ALL);

    filamat::Package unlitPkg = unlitBuilder.build(e->getJobSystem());
    if (unlitPkg.isValid()) {
        auto* mat = Material::Builder()
            .package(unlitPkg.getData(), unlitPkg.getSize())
            .build(*e);
        unlitMaterial_ = mat;
        unlitMaterialInstance_ = mat->createInstance();
    }

    filamat::MaterialBuilder litBuilder;
    litBuilder.name("LitCubeMat")
              .material("void material(inout MaterialInputs material) { prepareMaterial(material); material.baseColor = getColor(); material.roughness = 0.30; material.metallic = 0.70; }")
              .shading(filamat::MaterialBuilder::Shading::LIT)
              .require(VertexAttribute::POSITION)
              .require(VertexAttribute::COLOR)
              .require(VertexAttribute::TANGENTS)
              .targetApi(filamat::MaterialBuilder::TargetApi::ALL);

    filamat::Package litPkg = litBuilder.build(e->getJobSystem());
    if (litPkg.isValid()) {
        auto* mat = Material::Builder()
            .package(litPkg.getData(), litPkg.getSize())
            .build(*e);
        litMaterial_ = mat;
        litMaterialInstance_ = mat->createInstance();
    }

    if (!unlitMaterialInstance_ || !litMaterialInstance_) {
        LOGE("Failed to create filamat procedural materials");
        return;
    }

    // ── Build Grid Floor & Axis Gizmo Mesh (LINES) ────────────
    std::vector<GridVertex> gridVerts;
    std::vector<uint16_t> gridIndices;

    ubyte4 gridColorLine = packUnorm8(float4(0.28f, 0.38f, 0.55f, 0.8f));
    ubyte4 axisXColor    = packUnorm8(float4(1.00f, 0.20f, 0.20f, 1.0f)); // Red X
    ubyte4 axisYColor    = packUnorm8(float4(0.20f, 1.00f, 0.20f, 1.0f)); // Green Y
    ubyte4 axisZColor    = packUnorm8(float4(0.20f, 0.50f, 1.00f, 1.0f)); // Blue Z

    // Grid lines XZ plane from -10 to +10
    float extent = 10.0f;
    float step = 1.0f;

    for (float i = -extent; i <= extent; i += step) {
        // Parallel to Z
        ubyte4 colZ = (std::abs(i) < 0.01f) ? axisZColor : gridColorLine;
        gridVerts.push_back({ {i, 0.0f, -extent}, colZ });
        gridVerts.push_back({ {i, 0.0f,  extent}, colZ });

        // Parallel to X
        ubyte4 colX = (std::abs(i) < 0.01f) ? axisXColor : gridColorLine;
        gridVerts.push_back({ {-extent, 0.0f, i}, colX });
        gridVerts.push_back({ { extent, 0.0f, i}, colX });
    }

    // Center Axis Gizmo at (0, 0.75, 0)
    float3 center(0.0f, 0.75f, 0.0f);
    gridVerts.push_back({ center, axisXColor });
    gridVerts.push_back({ center + float3(1.2f, 0.0f, 0.0f), axisXColor });

    gridVerts.push_back({ center, axisYColor });
    gridVerts.push_back({ center + float3(0.0f, 1.2f, 0.0f), axisYColor });

    gridVerts.push_back({ center, axisZColor });
    gridVerts.push_back({ center + float3(0.0f, 0.0f, 1.2f), axisZColor });

    for (uint16_t i = 0; i < (uint16_t)gridVerts.size(); i++) {
        gridIndices.push_back(i);
    }

    // Create Grid Vertex & Index Buffers
    auto* vbGrid = VertexBuffer::Builder()
        .vertexCount((uint32_t)gridVerts.size())
        .bufferCount(1)
        .attribute(VertexAttribute::POSITION, 0, VertexBuffer::AttributeType::FLOAT3, offsetof(GridVertex, position), sizeof(GridVertex))
        .attribute(VertexAttribute::COLOR, 0, VertexBuffer::AttributeType::UBYTE4, offsetof(GridVertex, color), sizeof(GridVertex))
        .normalized(VertexAttribute::COLOR, true)
        .build(*e);

    auto* ibGrid = IndexBuffer::Builder()
        .indexCount((uint32_t)gridIndices.size())
        .bufferType(IndexBuffer::IndexType::USHORT)
        .build(*e);

    vbGrid->setBufferAt(*e, 0, backend::BufferDescriptor(gridVerts.data(), gridVerts.size() * sizeof(GridVertex)));
    ibGrid->setBuffer(*e, backend::BufferDescriptor(gridIndices.data(), gridIndices.size() * sizeof(uint16_t)));

    gridVb_ = vbGrid;
    gridIb_ = ibGrid;

    utils::Entity gridEntity = utils::EntityManager::get().create();
    gridEntity_ = gridEntity.getId();

    RenderableManager::Builder(1)
        .boundingBox({ { -10.0f, -0.1f, -10.0f }, { 10.0f, 2.5f, 10.0f } })
        .material(0, static_cast<MaterialInstance*>(unlitMaterialInstance_))
        .geometry(0, RenderableManager::PrimitiveType::LINES, vbGrid, ibGrid, 0, (uint32_t)gridIndices.size())
        .culling(false)
        .receiveShadows(false)
        .castShadows(false)
        .build(*e, gridEntity);

    s->addEntity(gridEntity);

    // ── Build 3D Lit Cube Mesh (TRIANGLES) ────────────────────
    std::vector<CubeVertex> cubeVerts;
    std::vector<uint16_t> cubeIndices;

    // Cube dimensions: 1.0 unit centered at Y = 0.75 (from Y=0.25 to Y=1.25)
    float minX = -0.5f, maxX = 0.5f;
    float minY =  0.25f, maxY = 1.25f;
    float minZ = -0.5f, maxZ = 0.5f;

    ubyte4 colAmber   = packUnorm8(float4(1.00f, 0.70f, 0.20f, 1.0f)); // Front (Amber)
    ubyte4 colOrange  = packUnorm8(float4(0.95f, 0.40f, 0.15f, 1.0f)); // Back (Orange)
    ubyte4 colTeal    = packUnorm8(float4(0.15f, 0.80f, 0.85f, 1.0f)); // Top (Teal)
    ubyte4 colBlue    = packUnorm8(float4(0.20f, 0.30f, 0.55f, 1.0f)); // Bottom (Slate)
    ubyte4 colPurple  = packUnorm8(float4(0.75f, 0.25f, 0.85f, 1.0f)); // Right (Purple)
    ubyte4 colGreen   = packUnorm8(float4(0.20f, 0.85f, 0.45f, 1.0f)); // Left (Green)

    short4 qFront  = packSnorm16(float4(0.7071f, 0.0f, 0.0f, 0.7071f));
    short4 qBack   = packSnorm16(float4(0.0f, 0.7071f, 0.7071f, 0.0f));
    short4 qTop    = packSnorm16(float4(0.0f, 0.0f, 0.0f, 1.0f));
    short4 qBottom = packSnorm16(float4(1.0f, 0.0f, 0.0f, 0.0f));
    short4 qRight  = packSnorm16(float4(0.0f, 0.7071f, 0.0f, 0.7071f));
    short4 qLeft   = packSnorm16(float4(0.0f, -0.7071f, 0.0f, 0.7071f));

    auto addFace = [&](float3 p0, float3 p1, float3 p2, float3 p3, ubyte4 color, short4 q) {
        uint16_t base = (uint16_t)cubeVerts.size();
        cubeVerts.push_back({ p0, color, q });
        cubeVerts.push_back({ p1, color, q });
        cubeVerts.push_back({ p2, color, q });
        cubeVerts.push_back({ p3, color, q });

        cubeIndices.push_back(base + 0);
        cubeIndices.push_back(base + 1);
        cubeIndices.push_back(base + 2);

        cubeIndices.push_back(base + 0);
        cubeIndices.push_back(base + 2);
        cubeIndices.push_back(base + 3);
    };

    // Front (+Z)
    addFace({minX, minY, maxZ}, {maxX, minY, maxZ}, {maxX, maxY, maxZ}, {minX, maxY, maxZ}, colAmber, qFront);
    // Back (-Z)
    addFace({maxX, minY, minZ}, {minX, minY, minZ}, {minX, maxY, minZ}, {maxX, maxY, minZ}, colOrange, qBack);
    // Top (+Y)
    addFace({minX, maxY, maxZ}, {maxX, maxY, maxZ}, {maxX, maxY, minZ}, {minX, maxY, minZ}, colTeal, qTop);
    // Bottom (-Y)
    addFace({minX, minY, minZ}, {maxX, minY, minZ}, {maxX, minY, maxZ}, {minX, minY, maxZ}, colBlue, qBottom);
    // Right (+X)
    addFace({maxX, minY, maxZ}, {maxX, minY, minZ}, {maxX, maxY, minZ}, {maxX, maxY, maxZ}, colPurple, qRight);
    // Left (-X)
    addFace({minX, minY, minZ}, {minX, minY, maxZ}, {minX, maxY, maxZ}, {minX, maxY, minZ}, colGreen, qLeft);

    auto* vbCube = VertexBuffer::Builder()
        .vertexCount((uint32_t)cubeVerts.size())
        .bufferCount(1)
        .attribute(VertexAttribute::POSITION, 0, VertexBuffer::AttributeType::FLOAT3, offsetof(CubeVertex, position), sizeof(CubeVertex))
        .attribute(VertexAttribute::COLOR, 0, VertexBuffer::AttributeType::UBYTE4, offsetof(CubeVertex, color), sizeof(CubeVertex))
        .attribute(VertexAttribute::TANGENTS, 0, VertexBuffer::AttributeType::SHORT4, offsetof(CubeVertex, tangent), sizeof(CubeVertex))
        .normalized(VertexAttribute::COLOR, true)
        .normalized(VertexAttribute::TANGENTS, true)
        .build(*e);

    auto* ibCube = IndexBuffer::Builder()
        .indexCount((uint32_t)cubeIndices.size())
        .bufferType(IndexBuffer::IndexType::USHORT)
        .build(*e);

    vbCube->setBufferAt(*e, 0, backend::BufferDescriptor(cubeVerts.data(), cubeVerts.size() * sizeof(CubeVertex)));
    ibCube->setBuffer(*e, backend::BufferDescriptor(cubeIndices.data(), cubeIndices.size() * sizeof(uint16_t)));

    cubeVb_ = vbCube;
    cubeIb_ = ibCube;

    utils::Entity cubeEntity = utils::EntityManager::get().create();
    cubeEntity_ = cubeEntity.getId();

    RenderableManager::Builder(1)
        .boundingBox({ { -0.75f, 0.0f, -0.75f }, { 0.75f, 1.5f, 0.75f } })
        .material(0, static_cast<MaterialInstance*>(litMaterialInstance_))
        .geometry(0, RenderableManager::PrimitiveType::TRIANGLES, vbCube, ibCube, 0, (uint32_t)cubeIndices.size())
        .culling(true)
        .receiveShadows(true)
        .castShadows(true)
        .build(*e, cubeEntity);

    s->addEntity(cubeEntity);
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

    // Destroy procedural objects
    if (gridEntity_ != 0) {
        if (s) s->remove(utils::Entity::import(gridEntity_));
        e->destroy(utils::Entity::import(gridEntity_));
        gridEntity_ = 0;
    }
    if (cubeEntity_ != 0) {
        if (s) s->remove(utils::Entity::import(cubeEntity_));
        e->destroy(utils::Entity::import(cubeEntity_));
        cubeEntity_ = 0;
    }
    if (gridVb_) { e->destroy(static_cast<VertexBuffer*>(gridVb_)); gridVb_ = nullptr; }
    if (gridIb_) { e->destroy(static_cast<IndexBuffer*>(gridIb_)); gridIb_ = nullptr; }
    if (cubeVb_) { e->destroy(static_cast<VertexBuffer*>(cubeVb_)); cubeVb_ = nullptr; }
    if (cubeIb_) { e->destroy(static_cast<IndexBuffer*>(cubeIb_)); cubeIb_ = nullptr; }

    if (unlitMaterialInstance_) { e->destroy(static_cast<MaterialInstance*>(unlitMaterialInstance_)); unlitMaterialInstance_ = nullptr; }
    if (unlitMaterial_) { e->destroy(static_cast<Material*>(unlitMaterial_)); unlitMaterial_ = nullptr; }
    if (litMaterialInstance_) { e->destroy(static_cast<MaterialInstance*>(litMaterialInstance_)); litMaterialInstance_ = nullptr; }
    if (litMaterial_) { e->destroy(static_cast<Material*>(litMaterial_)); litMaterial_ = nullptr; }

    // Unload models
    for (auto& [id, model] : models_) {
        if (model.entities && model.entityCount > 0) {
            auto* ents = static_cast<utils::Entity*>(model.entities);
            for (uint32_t i = 0; i < model.entityCount; i++) {
                if (!ents[i].isNull()) {
                    if (s) s->remove(ents[i]);
                    e->destroy(ents[i]);
                }
            }
            delete[] ents;
            model.entities = nullptr;
        }
    }
    models_.clear();

    // Destroy lights
    for (auto& l : lights_) {
        if (l.active && l.entityId != 0) {
            utils::Entity ent = utils::Entity::import(l.entityId);
            if (s) s->remove(ent);
            e->destroy(ent);
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
        e->destroy(utils::Entity::import(cameraEntity_));
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
    if (!cam) {
        LOGE("applyCameraToFilament: no camera (was init called?)");
        return;
    }

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
    utils::EntityManager& em = utils::EntityManager::get();

    utils::Entity entity = em.create();

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
        if (s) s->remove(ent);
        if (e) e->destroy(ent);
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
    applyModelTransform(model);
    LOGI("Loaded model placeholder: %s (id=%u)", model.path.c_str(), id);
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

    if (it->second.entities && it->second.entityCount > 0) {
        auto* ents = static_cast<utils::Entity*>(it->second.entities);
        for (uint32_t i = 0; i < it->second.entityCount; i++) {
            if (!ents[i].isNull()) {
                if (s) s->remove(ents[i]);
                if (e) e->destroy(ents[i]);
            }
        }
        delete[] ents;
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
