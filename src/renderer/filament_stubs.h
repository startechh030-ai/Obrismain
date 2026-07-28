#ifndef OBRIS_FILAMENT_STUBS_H
#define OBRIS_FILAMENT_STUBS_H

// ── Minimal type declarations for Filament C++ API ────────────
// These match the symbols exported by libfilament-jni.so v1.53.2.
// No headers needed — we declare only what we use.

#include <cstdint>
#include <cstddef>

// ── math types (used inline by Filament headers, so we define equivalents) ──
namespace filament::math {
template<typename T> struct TVec2 { T x, y; };
template<typename T> struct TVec3 {
    T x, y, z;
    TVec3() : x(0), y(0), z(0) {}
    TVec3(T v) : x(v), y(v), z(v) {}
    TVec3(T x, T y, T z) : x(x), y(y), z(z) {}
};
template<typename T> struct TVec4 {
    T x, y, z, w;
    TVec4() : x(0), y(0), z(0), w(0) {}
    TVec4(T x, T y, T z, T w) : x(x), y(y), z(z), w(w) {}
};
template<typename T> struct TMat33 { TVec3<T> v[3]; };
template<typename T> struct TMat44 {
    TVec4<T> v[4];
    static TMat44 translation(TVec3<T> t) {
        TMat44 r;
        r.v[0] = TVec4<T>(1,0,0,0);
        r.v[1] = TVec4<T>(0,1,0,0);
        r.v[2] = TVec4<T>(0,0,1,0);
        r.v[3] = TVec4<T>(t.x, t.y, t.z, 1);
        return r;
    }
    static TMat44 scaling(TVec3<T> s) {
        TMat44 r;
        r.v[0] = TVec4<T>(s.x,0,0,0);
        r.v[1] = TVec4<T>(0,s.y,0,0);
        r.v[2] = TVec4<T>(0,0,s.z,0);
        r.v[3] = TVec4<T>(0,0,0,1);
        return r;
    }
    static TMat44 rotation(T angle, TVec3<T> axis) {
        // Simplified — for production use proper Rodrigues
        TMat44 r;
        r.v[0] = TVec4<T>(1,0,0,0);
        r.v[1] = TVec4<T>(0,1,0,0);
        r.v[2] = TVec4<T>(0,0,1,0);
        r.v[3] = TVec4<T>(0,0,0,1);
        return r;
    }
};
using float2 = TVec2<float>;
using float3 = TVec3<float>;
using float4 = TVec4<float>;
using double3 = TVec3<double>;
using mat3f = TMat33<float>;
using mat4f = TMat44<float>;
}

// ── utils types ───────────────────────────────────────────────
namespace utils {
class Entity {
    uint32_t id_ = 0;
public:
    Entity() = default;
    explicit Entity(uint32_t id) : id_(id) {}
    uint32_t getId() const { return id_; }
    bool isNull() const { return id_ == 0; }
    bool operator==(Entity e) const { return e.id_ == id_; }
};

template<typename T, bool B> class EntityInstance {};

class EntityManager {
public:
    static EntityManager& get();
    Entity create();
    void destroy(Entity e);
    void destroy(size_t n, Entity* e);
    size_t getEntityCount() const;
};

template<typename T> class Invocable {};
}

// ── filament types (forward declarations of classes) ──────────
// These classes exist in libfilament-jni.so and are used via pointers.
namespace filament {
namespace backend { struct DriverApiForward; }

class Engine {
public:
    enum Backend : uint8_t { OPENGL = 0, VULKAN = 1, METAL = 2 };
    static Engine* create(Backend backend);
    void destroy(Engine* self);
    Renderer* createRenderer();
    Scene* createScene();
    Camera* createCamera(utils::Entity entity);
    View* createView();
    SwapChain* createSwapChain(void* nativeWindow, uint64_t flags = 0);
    SwapChain* createSwapChain(uint32_t w, uint32_t h, uint64_t flags = 0);
    void destroy(SwapChain const*);
    void destroy(Renderer const*);
    void destroy(Scene const*);
    void destroy(View const*);
    void destroy(Skybox const*);
    void destroy(IndirectLight const*);
    void destroy(Texture const*);
    void destroy(Material const*);
    void destroy(MaterialInstance const*);
    void destroy(VertexBuffer const*);
    void destroy(IndexBuffer const*);
    void destroy(Fence const*);
    void destroy(Stream const*);
    void destroy(ColorGrading const*);
    void destroy(RenderTarget const*);
    void destroy(SkinningBuffer const*);
    void destroy(MorphTargetBuffer const*);
    void destroy(InstanceBuffer const*);
    void destroy(BufferObject const*);
    void destroy(utils::Entity);
    void destroyCameraComponent(utils::Entity);
    void destroy(Engine* engine);
    RenderableManager& getRenderableManager();
    TransformManager& getTransformManager();
    LightManager& getLightManager();
    void enableAccurateTranslations();
};

class Renderer {
public:
    struct ClearOptions { math::float4 clearColor; bool clear; };
    bool beginFrame(SwapChain* chain, uint64_t flags = 0);
    void render(View const* view);
    void endFrame();
    void setClearOptions(ClearOptions const& options);
};

class Scene {
public:
    void addEntity(utils::Entity entity);
    void removeEntity(utils::Entity entity);
    void setIndirectLight(IndirectLight* ibl);
    void setSkybox(Skybox* skybox);
};

class View {
public:
    void setScene(Scene* scene);
    void setCamera(Camera* camera);
    void setViewport(class Viewport const& vp);
    void setPostProcessingEnabled(bool enabled);
    void setShadowingEnabled(bool enabled);
    void setFrustumCullingEnabled(bool enabled);
    void setVisibleLayers(uint8_t select, uint8_t values);
    void setScreenSpaceRefractionEnabled(bool enabled);
    void setStencilBufferEnabled(bool enabled);
};

class Camera {
public:
    enum Projection { PERSPECTIVE, ORTHO };
    enum Fov { VERTICAL, HORIZONTAL };
    void setProjection(double fovInDegrees, double aspect, double near, double far, Fov fov = Fov::VERTICAL);
    void setProjection(Projection proj, double left, double right, double bottom, double top, double near, double far);
    void lookAt(math::double3 const& eye, math::double3 const& target, math::double3 const& up);
};

struct Viewport {
    int32_t left, bottom;
    uint32_t width, height;
    Viewport() : left(0), bottom(0), width(1), height(1) {}
    Viewport(int32_t l, int32_t b, uint32_t w, uint32_t h) : left(l), bottom(b), width(w), height(h) {}
};

class SwapChain {
public:
    static const uint64_t CONFIG_HAS_STENCIL_BITS = 1;
    static const uint64_t CONFIG_TRANSPARENT = 2;
    static const uint64_t CONFIG_READABLE = 4;
    static const uint64_t CONFIG_PROTECTED_CONTENT = 8;
    static const uint64_t CONFIG_PROTECTED_CONTENT_OPTIONAL = 16;
    static bool isSRGBSwapChainSupported(Engine&);
    static bool isProtectedContentSupported(Engine&);
};

class LightManager {
public:
    enum class Type : uint8_t { DIRECTIONAL, POINT, FOCUSED_SPOT, SPOT };
    struct ShadowOptions {};
    class Builder {
    public:
        Builder(Type type);
        Builder(Type type) /* actually just Builder(type) */;
        Builder& color(math::float3 const& c);
        Builder& intensity(float i);
        Builder& intensity(float i, float sunAngle);
        Builder& intensityCandela(float i);
        Builder& direction(math::float3 const& d);
        Builder& position(math::float3 const& p);
        Builder& falloff(float f);
        Builder& castShadows(bool s);
        Builder& castLight(bool l);
        Builder& shadowOptions(ShadowOptions const& o);
        Builder& spotLightCone(float inner, float outer);
        Builder& sunAngularRadius(float r);
        Builder& sunHaloSize(float s);
        Builder& sunHaloFalloff(float f);
        Builder& lightChannel(unsigned int c, bool v);
        void build(Engine& engine, utils::Entity entity);
    };
};

class TransformManager {
public:
    struct Instance;
    Instance getInstance(utils::Entity e) const;
    void setTransform(Instance instance, math::mat4f const& transform);
    void setTransform(Instance instance, math::TMat44<double> const& transform);
    void create(utils::Entity e, Instance parent);
    void create(utils::Entity e, Instance parent, math::mat4f const& transform);
    void create(utils::Entity e, Instance parent, math::TMat44<double> const& transform);
    void destroy(utils::Entity e);
    bool hasComponent(utils::Entity e) const;
    void openLocalTransformTransaction();
    void commitLocalTransformTransaction();
    void setAccurateTranslationsEnabled(bool e);
    size_t getEntityCount() const;
    utils::Entity const* getEntities() const;
};

class RenderableManager {
public:
    struct Instance;
    Instance getInstance(utils::Entity e) const;
    void setLayerMask(Instance instance, uint8_t select, uint8_t values);
    void setFogEnabled(Instance instance, bool enabled);
};

class Color {
public:
    static math::float3 toLinear(math::float3 const& color);
};

class Skybox {
public:
    class Builder {
    public:
        Builder();
        Builder& environment(Texture* tex);
        Builder& color(math::float4 c);
        Builder& intensity(float i);
        Builder& showSun(bool s);
        Skybox* build(Engine& engine);
    };
    void setLayerMask(uint8_t select, uint8_t values);
};

class IndirectLight {
public:
    class Builder {
    public:
        Builder();
        Builder& reflections(Texture const* tex);
        Builder& irradiance(Texture const* tex);
        Builder& irradiance(uint8_t bands, math::float3 const* data);
        Builder& radiance(uint8_t bands, math::float3 const* data);
        Builder& intensity(float i);
        Builder& rotation(math::mat3f const& r);
        IndirectLight* build(Engine& engine);
    };
    void setIntensity(float intensity);
    void setRotation(math::mat3f const& rotation);
};

class Texture { public: enum class Sampler : uint8_t { SAMPLER_2D, SAMPLER_CUBEMAP, SAMPLER_3D, SAMPLER_EXTERNAL }; };
class Material { public: bool isColorWriteEnabled() const; bool isDepthWriteEnabled() const; bool isAlphaToCoverageEnabled() const; };
class MaterialInstance { public: bool isColorWriteEnabled() const; bool isDepthWriteEnabled() const; bool isDepthCullingEnabled() const; bool isStencilWriteEnabled() const; };
class VertexBuffer { public: class Builder { public: Builder& enableBufferObjects(bool); }; };
class IndexBuffer { public: class Builder {}; };
class Fence {};
class Stream { public: class Builder {}; };
class ColorGrading {};
class RenderTarget { public: class Builder {}; };
class SkinningBuffer { public: class Builder {}; };
class MorphTargetBuffer { public: class Builder {}; };
class InstanceBuffer {};
class BufferObject {};
class Frustum {
public:
    Frustum(math::mat4f const& p);
    void setProjection(math::mat4f const& p);
    bool intersects(class Box const& b) const;
    bool intersects(math::float4 const& p) const;
    bool contains(math::float3 p) const;
    struct Plane {};
    Plane getNormalizedPlane(size_t i) const;
    void getNormalizedPlanes(Plane* planes) const;
};
class Culler {
public:
    struct Test {
        static void intersects(uint8_t* out, Frustum const& f, math::float3 const* pos, math::float3 const* bounds, size_t count);
        static void intersects(uint8_t* out, Frustum const& f, math::float4 const* bounds, size_t count);
    };
};
class Box {};

} // namespace filament

#endif // OBRIS_FILAMENT_STUBS_H
