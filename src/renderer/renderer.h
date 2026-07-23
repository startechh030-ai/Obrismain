#ifndef OBRIS_RENDERER_H
#define OBRIS_RENDERER_H

#include "obris.h"
#include <vector>
#include <string>
#include <unordered_map>

namespace obris {

struct LoadedModel {
    ObrisModel id;
    std::string path;
    void* filamentAsset = nullptr;   // gltfio::FilamentAsset*
    void* entity = nullptr;          // utils::Entity
    std::string currentAnim;
    float pos[3] = {0,0,0};
    float rot[4] = {0,0,0,1};
    float scale[3] = {1,1,1};
    bool visible = true;
};

struct LightData {
    ObrisLight def;
    void* lightManager = nullptr;    // filament::LightManager::Builder result
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
    ObrisCamera getCamera() const { return camera_; }

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

    // Accessors for JNI
    void* engine() const { return engine_; }

private:
    bool initialized_ = false;

    // Filament handles (opaque)
    void* engine_ = nullptr;         // filament::Engine*
    void* renderer_ = nullptr;       // filament::Renderer*
    void* scene_ = nullptr;          // filament::Scene*
    void* view_ = nullptr;           // filament::View*
    void* swapChain_ = nullptr;      // filament::SwapChain*
    void* cameraEntity_ = nullptr;   // utils::Entity for camera
    void* skybox_ = nullptr;         // filament::Skybox*
    void* indirectLight_ = nullptr;  // filament::IndirectLight*

    // Camera state (our copy)
    ObrisCamera camera_;

    // Resources
    std::vector<LightData> lights_;
    std::unordered_map<ObrisModel, LoadedModel> models_;
    ObrisModel nextModelId_ = 1;

    // Viewport
    int width_ = 720;
    int height_ = 1280;

    // Clear color
    float clearR_ = 0.1f, clearG_ = 0.1f, clearB_ = 0.2f, clearA_ = 1.0f;

    // Init helpers
    bool initFilament(const ObrisConfig& config);
    void createSceneEntities();
    void destroySceneEntities();
    void applyCameraToFilament();
};

} // namespace obris

#endif
