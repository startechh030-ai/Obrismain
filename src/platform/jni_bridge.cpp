#include "obris.h"
#include "renderer/renderer.h"
#include "audio/audio_engine.h"
#include "json/json_reader.h"
#include "encryption/crypto.h"
#include <jni.h>
#include <android/native_window_jni.h>
#include <android/asset_manager_jni.h>
#include <android/log.h>
#include <cstring>

#define LOG_TAG "ObrisJNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

using namespace obris;

// ── Engine instance ──────────────────────────────────────────
static struct {
    Renderer* renderer = nullptr;
    AudioEngine* audio = nullptr;
    int width = 720;
    int height = 1280;
} gEngine;

// ══════════════════════════════════════════════════════════════
//  JNI: com.obris.ObrisActivity
// ══════════════════════════════════════════════════════════════

extern "C" {

JNIEXPORT jlong JNICALL
Java_com_obris_ObrisActivity_nativeCreate(
    JNIEnv* env, jobject /*thiz*/,
    jobject surface, jobject assetManager,
    jint width, jint height,
    jstring iblPath) {

    // Set global asset manager (for JSON reader and asset loading)
    AAssetManager* aam = AAssetManager_fromJava(env, assetManager);
    setAssetManager((void*)aam);

    // Init crypto
    Crypto::init();

    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);

    ObrisConfig config;
    config.width = width;
    config.height = height;
    config.nativeWindow = window;
    config.useVulkan = 0;  // Use OpenGL ES 3 (EGL) for universal Android compatibility
    config.iblPath = iblPath ? env->GetStringUTFChars(iblPath, nullptr) : nullptr;

    // Create renderer
    gEngine.renderer = new Renderer();
    if (!gEngine.renderer->init(config)) {
        LOGE("Failed to init renderer");
        delete gEngine.renderer;
        gEngine.renderer = nullptr;
        return 0;
    }
    gEngine.width = width;
    gEngine.height = height;

    // Create audio
    gEngine.audio = new AudioEngine();
    gEngine.audio->init();

    if (config.iblPath) env->ReleaseStringUTFChars(iblPath, config.iblPath);

    LOGI("Obris engine created (%dx%d)", width, height);
    return (jlong)(uintptr_t)&gEngine;
}

JNIEXPORT void JNICALL
Java_com_obris_ObrisActivity_nativeDestroy(
    JNIEnv* /*env*/, jobject /*thiz*/) {

    if (gEngine.audio)   { gEngine.audio->shutdown(); delete gEngine.audio; gEngine.audio = nullptr; }
    if (gEngine.renderer) { gEngine.renderer->shutdown(); delete gEngine.renderer; gEngine.renderer = nullptr; }
    LOGI("Obris engine destroyed");
}

JNIEXPORT void JNICALL
Java_com_obris_ObrisActivity_nativeRenderFrame(
    JNIEnv* /*env*/, jobject /*thiz*/) {
    if (gEngine.renderer) gEngine.renderer->renderFrame();
}

JNIEXPORT void JNICALL
Java_com_obris_ObrisActivity_nativeResize(
    JNIEnv* /*env*/, jobject /*thiz*/, jint w, jint h) {
    if (gEngine.renderer) gEngine.renderer->resize(w, h);
    gEngine.width = w;
    gEngine.height = h;
}

// ── Camera ──────────────────────────────────────────────────

JNIEXPORT void JNICALL
Java_com_obris_ObrisActivity_nativeSetCamera(
    JNIEnv* /*env*/, jobject /*thiz*/,
    jfloat x, jfloat y, jfloat z,
    jfloat tx, jfloat ty, jfloat tz,
    jfloat fov) {

    if (!gEngine.renderer) return;
    ObrisCamera cam;
    cam.x = x; cam.y = y; cam.z = z;
    cam.tx = tx; cam.ty = ty; cam.tz = tz;
    cam.fov = fov;
    cam.near = 0.1f; cam.far = 1000.0f;
    cam.isPerspective = 1;
    gEngine.renderer->setCamera(cam);
}

// ── Lights ──────────────────────────────────────────────────

JNIEXPORT jint JNICALL
Java_com_obris_ObrisActivity_nativeAddLight(
    JNIEnv* /*env*/, jobject /*thiz*/,
    jint type,
    jfloat r, jfloat g, jfloat b,
    jfloat intensity,
    jfloat dx, jfloat dy, jfloat dz) {

    if (!gEngine.renderer) return -1;
    ObrisLight light;
    light.type = type;
    light.color[0] = r; light.color[1] = g; light.color[2] = b;
    light.intensity = intensity;
    light.direction[0] = dx; light.direction[1] = dy; light.direction[2] = dz;
    light.position[0] = 0; light.position[1] = 0; light.position[2] = 0;
    return (jint)gEngine.renderer->addLight(light);
}

// ── Models ──────────────────────────────────────────────────

JNIEXPORT jint JNICALL
Java_com_obris_ObrisActivity_nativeLoadModel(
    JNIEnv* env, jobject /*thiz*/,
    jstring path,
    jfloat px, jfloat py, jfloat pz,
    jfloat rx, jfloat ry, jfloat rz, jfloat rw,
    jfloat sx, jfloat sy, jfloat sz) {

    if (!gEngine.renderer) return 0;

    ObrisModelInfo info;
    info.path = env->GetStringUTFChars(path, nullptr);
    float pos[3]  = {px, py, pz};
    float rot[4]  = {rx, ry, rz, rw};
    float scale[3]= {sx, sy, sz};
    info.pos = pos;
    info.rot = rot;
    info.scale = scale;
    info.animName = nullptr;

    ObrisModel id = gEngine.renderer->loadModel(info);
    env->ReleaseStringUTFChars(path, info.path);
    return (jint)id;
}

JNIEXPORT void JNICALL
Java_com_obris_ObrisActivity_nativeUnloadModel(
    JNIEnv* /*env*/, jobject /*thiz*/, jint handle) {
    if (gEngine.renderer) gEngine.renderer->unloadModel((ObrisModel)handle);
}

JNIEXPORT void JNICALL
Java_com_obris_ObrisActivity_nativeSetModelTransform(
    JNIEnv* /*env*/, jobject /*thiz*/,
    jint handle,
    jfloat px, jfloat py, jfloat pz,
    jfloat rx, jfloat ry, jfloat rz, jfloat rw,
    jfloat sx, jfloat sy, jfloat sz) {
    if (!gEngine.renderer) return;
    float pos[3] = {px, py, pz};
    float rot[4] = {rx, ry, rz, rw};
    float scale[3]= {sx, sy, sz};
    gEngine.renderer->setModelTransform((ObrisModel)handle, pos, rot, scale);
}

JNIEXPORT void JNICALL
Java_com_obris_ObrisActivity_nativeSetModelVisible(
    JNIEnv* /*env*/, jobject /*thiz*/, jint handle, jboolean visible) {
    if (gEngine.renderer) gEngine.renderer->setModelVisible((ObrisModel)handle, visible);
}

// ── IBL / HDR ───────────────────────────────────────────────

JNIEXPORT jboolean JNICALL
Java_com_obris_ObrisActivity_nativeLoadIBL(
    JNIEnv* env, jobject /*thiz*/, jstring path) {
    if (!gEngine.renderer) return JNI_FALSE;
    const char* cpath = env->GetStringUTFChars(path, nullptr);
    bool ok = gEngine.renderer->loadIBL(cpath);
    env->ReleaseStringUTFChars(path, cpath);
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_obris_ObrisActivity_nativeSetIBLIntensity(
    JNIEnv* /*env*/, jobject /*thiz*/, jfloat intensity) {
    if (gEngine.renderer) gEngine.renderer->setIBLIntensity(intensity);
}

JNIEXPORT void JNICALL
Java_com_obris_ObrisActivity_nativeSetIBLRotation(
    JNIEnv* /*env*/, jobject /*thiz*/, jfloat degrees) {
    if (gEngine.renderer) gEngine.renderer->setIBLRotation(degrees);
}

// ── Audio ───────────────────────────────────────────────────

JNIEXPORT jint JNICALL
Java_com_obris_ObrisActivity_nativeLoadSound(
    JNIEnv* env, jobject /*thiz*/, jstring path) {
    if (!gEngine.audio) return 0;
    const char* cpath = env->GetStringUTFChars(path, nullptr);
    ObrisSound h = gEngine.audio->loadSound(cpath);
    env->ReleaseStringUTFChars(path, cpath);
    return (jint)h;
}

JNIEXPORT jint JNICALL
Java_com_obris_ObrisActivity_nativePlaySound(
    JNIEnv* /*env*/, jobject /*thiz*/,
    jint sound, jfloat volume, jboolean looping) {
    if (!gEngine.audio) return 0;
    return (jint)gEngine.audio->play((ObrisSound)sound, volume, looping);
}

JNIEXPORT void JNICALL
Java_com_obris_ObrisActivity_nativeStopSound(
    JNIEnv* /*env*/, jobject /*thiz*/, jint playback) {
    if (gEngine.audio) gEngine.audio->stop((ObrisPlayback)playback);
}

JNIEXPORT void JNICALL
Java_com_obris_ObrisActivity_nativeSetMasterVolume(
    JNIEnv* /*env*/, jobject /*thiz*/, jfloat volume) {
    if (gEngine.audio) gEngine.audio->setMasterVolume(volume);
}

// ── JSON ────────────────────────────────────────────────────

JNIEXPORT jstring JNICALL
Java_com_obris_ObrisActivity_nativeLoadJSON(
    JNIEnv* env, jobject /*thiz*/, jstring path) {
    const char* cpath = env->GetStringUTFChars(path, nullptr);
    char* json = JsonReader::loadFromAssets(cpath);
    env->ReleaseStringUTFChars(path, cpath);
    if (!json) return nullptr;
    jstring result = env->NewStringUTF(json);
    free(json);
    return result;
}

JNIEXPORT jstring JNICALL
Java_com_obris_ObrisActivity_nativeJSONGetString(
    JNIEnv* env, jobject /*thiz*/, jstring jsonStr, jstring key) {
    const char* json = env->GetStringUTFChars(jsonStr, nullptr);
    const char* k = env->GetStringUTFChars(key, nullptr);
    char* val = JsonReader::getString(json, k);
    env->ReleaseStringUTFChars(jsonStr, json);
    env->ReleaseStringUTFChars(key, k);
    if (!val) return nullptr;
    jstring result = env->NewStringUTF(val);
    free(val);
    return result;
}

JNIEXPORT jfloat JNICALL
Java_com_obris_ObrisActivity_nativeJSONGetFloat(
    JNIEnv* env, jobject /*thiz*/, jstring jsonStr, jstring key) {
    const char* json = env->GetStringUTFChars(jsonStr, nullptr);
    const char* k = env->GetStringUTFChars(key, nullptr);
    float val = JsonReader::getFloat(json, k);
    env->ReleaseStringUTFChars(jsonStr, json);
    env->ReleaseStringUTFChars(key, k);
    return val;
}

JNIEXPORT jint JNICALL
Java_com_obris_ObrisActivity_nativeJSONGetInt(
    JNIEnv* env, jobject /*thiz*/, jstring jsonStr, jstring key) {
    const char* json = env->GetStringUTFChars(jsonStr, nullptr);
    const char* k = env->GetStringUTFChars(key, nullptr);
    int val = JsonReader::getInt(json, k);
    env->ReleaseStringUTFChars(jsonStr, json);
    env->ReleaseStringUTFChars(key, k);
    return val;
}

// ── Encryption ──────────────────────────────────────────────

JNIEXPORT jbyteArray JNICALL
Java_com_obris_ObrisActivity_nativeEncrypt(
    JNIEnv* env, jobject /*thiz*/,
    jbyteArray keyArr, jbyteArray dataArr) {

    jbyte* key  = env->GetByteArrayElements(keyArr, nullptr);
    jsize  keyLen = env->GetArrayLength(keyArr);
    jbyte* data = env->GetByteArrayElements(dataArr, nullptr);
    jsize  dataLen = env->GetArrayLength(dataArr);

    size_t outLen = 0;
    unsigned char* out = Crypto::encrypt(
        (const unsigned char*)key, keyLen,
        (const unsigned char*)data, dataLen, &outLen);

    env->ReleaseByteArrayElements(keyArr, key, JNI_ABORT);
    env->ReleaseByteArrayElements(dataArr, data, JNI_ABORT);

    if (!out) return nullptr;

    jbyteArray result = env->NewByteArray(outLen);
    env->SetByteArrayRegion(result, 0, outLen, (const jbyte*)out);
    free(out);
    return result;
}

JNIEXPORT jbyteArray JNICALL
Java_com_obris_ObrisActivity_nativeDecrypt(
    JNIEnv* env, jobject /*thiz*/,
    jbyteArray keyArr, jbyteArray dataArr) {

    jbyte* key  = env->GetByteArrayElements(keyArr, nullptr);
    jsize  keyLen = env->GetArrayLength(keyArr);
    jbyte* data = env->GetByteArrayElements(dataArr, nullptr);
    jsize  dataLen = env->GetArrayLength(dataArr);

    size_t outLen = 0;
    unsigned char* out = Crypto::decrypt(
        (const unsigned char*)key, keyLen,
        (const unsigned char*)data, dataLen, &outLen);

    env->ReleaseByteArrayElements(keyArr, key, JNI_ABORT);
    env->ReleaseByteArrayElements(dataArr, data, JNI_ABORT);

    if (!out) return nullptr;

    jbyteArray result = env->NewByteArray(outLen);
    env->SetByteArrayRegion(result, 0, outLen, (const jbyte*)out);
    free(out);
    return result;
}

} // extern "C"
