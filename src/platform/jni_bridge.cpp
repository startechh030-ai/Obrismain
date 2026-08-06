#include "obris.h"
#include "renderer/renderer.h"
#include "audio/audio_engine.h"
#include "json/json_reader.h"
#include "encryption/crypto.h"
#include <jni.h>
#include <android/asset_manager_jni.h>
#include <android/log.h>
#include <cstring>

#define LOG_TAG "ObrisJNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

using namespace obris;

static struct {
    Renderer* renderer = nullptr;
    AudioEngine* audio = nullptr;
} gEngine;

extern "C" {

JNIEXPORT jlong JNICALL
Java_com_obris_ObrisActivity_nativeCreate(
    JNIEnv* env, jobject /*thiz*/,
    jobject assetManager) {

    if (assetManager) {
        AAssetManager* aam = AAssetManager_fromJava(env, assetManager);
        setAssetManager((void*)aam);
    }

    Crypto::init();

    gEngine.renderer = new Renderer();
    gEngine.audio = new AudioEngine();
    gEngine.audio->init();

    LOGI("Obris native library created");
    return (jlong)(uintptr_t)&gEngine;
}

JNIEXPORT void JNICALL
Java_com_obris_ObrisActivity_nativeDestroy(
    JNIEnv* /*env*/, jobject /*thiz*/) {

    if (gEngine.audio)   { gEngine.audio->shutdown(); delete gEngine.audio; gEngine.audio = nullptr; }
    if (gEngine.renderer) { gEngine.renderer->shutdown(); delete gEngine.renderer; gEngine.renderer = nullptr; }
    LOGI("Obris native library destroyed");
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
