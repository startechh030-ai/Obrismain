#ifndef OBRIS_RENDERER_H
#define OBRIS_RENDERER_H

#include "obris.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <cstdint>

// Forward declarations for Filament / Utils types
namespace utils {
class Entity;
}

namespace obris {

struct LoadedModel {
    ObrisModel id = 0;
    std::string path;
    void* filamentAsset = nullptr;   // gltfio::FilamentAsset*
    uint32_t entityCount = 0;
    void* entities = nullptr;        // Allocated utils::Entity[] array (stored as opaque void*)
    std::string currentAnim;
    float pos[3] = {0,0,0};
    float rot[4] = {0,0,0,1};
    float scale[3] = {1,1,1};
    bool visible = true;
};

struct LightEntry {
    ObrisLight def;
    uint32_t entityId = 0;           // utils::Entity mIdentity integer
    bool active = false;
};

class Renderer {
public:
    Renderer();
    ~Renderer();

    bool init(const ObrisConfig& config);
    void shutdown();
    void renderFrame();
    void resize(int w, int h);

    // Camera
    void setCamera(const ObrisCamera& cam);
    ObrisCamera getCamera() const { return cameraState_; }

    // Lights
    int addLight(const ObrisLight& light);
    void updateLight(int idx, const ObrisLight& light);
    void removeLight(int idx);

    // Models
    ObrisModel loadModel(const ObrisModelInfo& info);
    void unloadModel(ObrisModel model);
    void setModelVisible(ObrisModel model, bool visible);
    void setModelTransform(ObrisModel model, const float pos[3],
                           const float rot[4], const float scale[3]);
    void playAnimation(ObrisModel model, const char* name);
    void stopAnimation(ObrisModel model);

    // IBL
    bool loadIBL(const char* path);
    void setIBLIntensity(float intensity);
    void setIBLRotation(float degrees);

    // Accessors
    void* engine() const { return engine_; }
    void* assetLoader() const { return assetLoader_; }

private:
    bool initialized_ = false;

    // Filament handles (opaque pointers)
    void* engine_ = nullptr;         // filament::Engine*
    void* renderer_ = nullptr;       // filament::Renderer*
    void* scene_ = nullptr;          // filament::Scene*
    void* view_ = nullptr;           // filament::View*
    void* swapChain_ = nullptr;      // filament::SwapChain*
    void* filamentCamera_ = nullptr; // filament::Camera*
    uint32_t cameraEntity_ = 0;      // utils::Entity ID for camera
    void* skybox_ = nullptr;         // filament::Skybox*
    void* indirectLight_ = nullptr;  // filament::IndirectLight*
    void* assetLoader_ = nullptr;    // gltfio::AssetLoader*

    // Camera state
    ObrisCamera cameraState_;

    // Resources
    std::vector<LightEntry> lights_;
    std::unordered_map<ObrisModel, LoadedModel> models_;
    ObrisModel nextModelId_ = 1;

    // Viewport
    int width_ = 720;
    int height_ = 1280;

    // Clear color
    float clearR_ = 0.22f, clearG_ = 0.23f, clearB_ = 0.25f, clearA_ = 1.0f;

    // Internal helpers
    bool initFilament(const ObrisConfig& config);
    void applyCameraToFilament();
    void applyModelTransform(LoadedModel& model);
};

} // namespace obris

#endif
