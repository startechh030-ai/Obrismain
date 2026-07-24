#ifndef OBRIS_OBRIS_H
#define OBRIS_OBRIS_H

#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

// ══════════════════════════════════════════════════════════════
//  Types
// ══════════════════════════════════════════════════════════════

typedef void* ObrisEngine;
typedef uint32_t ObrisModel;
typedef uint32_t ObrisSound;
typedef uint32_t ObrisPlayback;
typedef uint32_t ObrisTexture;

#define OBRIS_INVALID_HANDLE 0

// ── Camera ───────────────────────────────────────────────────
typedef struct {
    float x, y, z;         // position
    float tx, ty, tz;      // target/look-at
    float fov;             // degrees
    float near, far;
    int isPerspective;     // 1=perspective, 0=orthographic
} ObrisCamera;

// ── Light ────────────────────────────────────────────────────
typedef struct {
    int type;              // 0=directional, 1=point, 2=spot
    float color[3];        // RGB
    float intensity;
    float direction[3];    // for directional
    float position[3];     // for point/spot
} ObrisLight;

// ── Render Config ────────────────────────────────────────────
typedef struct {
    int width;
    int height;
    void* nativeWindow;    // ANativeWindow*
    int useVulkan;         // 1=Vulkan, 0=OpenGL ES
    const char* iblPath;   // HDR IBL .ktx path (can be NULL)
} ObrisConfig;

// ── Model Info ────────────────────────────────────────────────
typedef struct {
    const char* path;      // GLB file path
    const float* pos;      // position xyz (can be NULL for defaults)
    const float* rot;      // quaternion xyzw (can be NULL for identity)
    const float* scale;    // scale xyz (can be NULL for 1,1,1)
    const char* animName;  // animation to play (or NULL)
} ObrisModelInfo;

// ══════════════════════════════════════════════════════════════
//  Engine Lifecycle
// ══════════════════════════════════════════════════════════════

/// Create the Obris engine. Returns handle or NULL.
ObrisEngine obris_create(const ObrisConfig* config);

/// Destroy the engine and all resources.
void obris_destroy(ObrisEngine engine);

/// Resize the viewport.
void obris_resize(ObrisEngine engine, int width, int height);

/// Render a single frame. Call once per frame.
void obris_render_frame(ObrisEngine engine);

/// Set the background clear color.
void obris_set_clear_color(ObrisEngine engine, float r, float g, float b, float a);

// ══════════════════════════════════════════════════════════════
//  Camera
// ══════════════════════════════════════════════════════════════

/// Set the active camera.
void obris_set_camera(ObrisEngine engine, const ObrisCamera* camera);

/// Get the current camera.
void obris_get_camera(ObrisEngine engine, ObrisCamera* outCamera);

// ══════════════════════════════════════════════════════════════
//  Lights
// ══════════════════════════════════════════════════════════════

/// Add a light. Returns light index.
int obris_add_light(ObrisEngine engine, const ObrisLight* light);

/// Update an existing light.
void obris_update_light(ObrisEngine engine, int index, const ObrisLight* light);

/// Remove a light.
void obris_remove_light(ObrisEngine engine, int index);

// ══════════════════════════════════════════════════════════════
//  Models / GLB
// ══════════════════════════════════════════════════════════════

/// Load a GLB model from Android assets. Returns handle or 0.
ObrisModel obris_load_model(ObrisEngine engine, const ObrisModelInfo* info);

/// Unload a model.
void obris_unload_model(ObrisEngine engine, ObrisModel model);

/// Show or hide a model.
void obris_set_model_visible(ObrisEngine engine, ObrisModel model, int visible);

/// Move/rotate/scale a model.
void obris_set_model_transform(ObrisEngine engine, ObrisModel model,
                                const float pos[3], const float rot[4], const float scale[3]);

/// Play an animation on a model.
void obris_play_animation(ObrisEngine engine, ObrisModel model, const char* name);

/// Stop animation on a model.
void obris_stop_animation(ObrisEngine engine, ObrisModel model);

// ══════════════════════════════════════════════════════════════
//  IBL / HDR Environment
// ══════════════════════════════════════════════════════════════

/// Load an HDR environment map (.ktx or .hdr).
int obris_load_ibl(ObrisEngine engine, const char* path);

/// Set IBL intensity (0.0 - 1.0).
void obris_set_ibl_intensity(ObrisEngine engine, float intensity);

/// Set IBL rotation (degrees around Y axis).
void obris_set_ibl_rotation(ObrisEngine engine, float degrees);

// ══════════════════════════════════════════════════════════════
//  Audio
// ══════════════════════════════════════════════════════════════

/// Load a sound file. Returns handle or 0.
ObrisSound obris_load_sound(ObrisEngine engine, const char* path);

/// Play a sound. Returns playback handle.
ObrisPlayback obris_play_sound(ObrisEngine engine, ObrisSound sound,
                                float volume, int looping);

/// Stop a playback.
void obris_stop_playback(ObrisEngine engine, ObrisPlayback playback);

/// Set master volume (0.0 - 1.0).
void obris_set_master_volume(ObrisEngine engine, float volume);

// ══════════════════════════════════════════════════════════════
//  JSON Reader
// ══════════════════════════════════════════════════════════════

/// Parse a JSON file from assets. Returns a string of the full JSON.
/// The caller must free the returned string with obris_free_string().
char* obris_load_json(ObrisEngine engine, const char* path);

/// Get a string value from a JSON string by key (simple flat access).
/// Caller must free with obris_free_string().
char* obris_json_get_string(const char* json, const char* key);

/// Get a float value from a JSON string by key.
float obris_json_get_float(const char* json, const char* key);

/// Get an int value from a JSON string by key.
int obris_json_get_int(const char* json, const char* key);

/// Free a string returned by obris_load_json or obris_json_get_string.
void obris_free_string(char* str);

// ══════════════════════════════════════════════════════════════
//  Encryption
// ══════════════════════════════════════════════════════════════

/// Encrypt a buffer using XChaCha20-Poly1305.
/// Returns a newly allocated buffer containing [nonce(24) + ciphertext].
/// `outLen` receives the total length.
/// Caller must free with obris_free().
unsigned char* obris_encrypt(const unsigned char* key, int keyLen,
                              const unsigned char* data, int dataLen,
                              int* outLen);

/// Decrypt a buffer. `inLen` is total length (nonce + ciphertext).
/// Returns plaintext, `outLen` receives its length.
/// Caller must free with obris_free().
unsigned char* obris_decrypt(const unsigned char* key, int keyLen,
                              const unsigned char* data, int dataLen,
                              int* outLen);

/// Generate a random key (32 bytes).
void obris_generate_key(unsigned char* outKey, int keyLen);

/// Free memory allocated by obris functions.
void obris_free(void* ptr);

// ══════════════════════════════════════════════════════════════
//  Platform (JNI callbacks)
// ══════════════════════════════════════════════════════════════

/// Set the Android asset manager (call from JNI).
void obris_set_asset_manager(void* assetManager);

#ifdef __cplusplus
}
#endif

#endif // OBRIS_OBRIS_H
