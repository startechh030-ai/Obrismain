#include "json_reader.h"
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/log.h>
#include <cstring>
#include <cstdio>

#define LOG_TAG "ObrisJSON"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Global asset manager set from JNI
static AAssetManager* gAssetManager = nullptr;

namespace obris {

void setAssetManager(void* aam) {
    gAssetManager = static_cast<AAssetManager*>(aam);
}

void* getAssetManager() {
    return static_cast<void*>(gAssetManager);
}

char* JsonReader::loadFromAssets(const char* path) {
    if (!gAssetManager) {
        LOGE("AssetManager not set! Call setAssetManager() from JNI first");
        return nullptr;
    }

    AAsset* asset = AAssetManager_open(gAssetManager, path, AASSET_MODE_BUFFER);
    if (!asset) {
        LOGE("Failed to open asset: %s", path);
        return nullptr;
    }

    size_t size = AAsset_getLength(asset);
    char* buffer = static_cast<char*>(malloc(size + 1));
    if (!buffer) {
        AAsset_close(asset);
        return nullptr;
    }

    memcpy(buffer, AAsset_getBuffer(asset), size);
    buffer[size] = '\0';
    AAsset_close(asset);
    return buffer;
}

char* JsonReader::getString(const char* json, const char* key) {
    if (!json || !key) return nullptr;

    // Find `"key": "value"` pattern
    std::string search = "\"";
    search += key;
    search += "\": \"";

    const char* pos = strstr(json, search.c_str());
    if (!pos) {
        // Try with no space after colon
        search = "\"";
        search += key;
        search += "\":\"";
        pos = strstr(json, search.c_str());
    }
    if (!pos) return nullptr;

    pos += search.length();
    const char* end = strchr(pos, '"');
    if (!end) return nullptr;

    size_t len = end - pos;
    char* result = static_cast<char*>(malloc(len + 1));
    memcpy(result, pos, len);
    result[len] = '\0';

    // Unescape
    for (char* p = result; *p; ++p) {
        if (*p == '\\' && *(p+1)) memmove(p, p+1, strlen(p));
    }

    return result;
}

float JsonReader::getFloat(const char* json, const char* key) {
    if (!json || !key) return 0.0f;

    std::string search = "\"";
    search += key;
    search += "\": ";

    const char* pos = strstr(json, search.c_str());
    if (!pos) {
        search = "\"";
        search += key;
        search += "\":";
        pos = strstr(json, search.c_str());
    }
    if (!pos) return 0.0f;

    pos += search.length();
    return static_cast<float>(atof(pos));
}

int JsonReader::getInt(const char* json, const char* key) {
    if (!json || !key) return 0;

    std::string search = "\"";
    search += key;
    search += "\": ";

    const char* pos = strstr(json, search.c_str());
    if (!pos) {
        search = "\"";
        search += key;
        search += "\":";
        pos = strstr(json, search.c_str());
    }
    if (!pos) return 0;

    pos += search.length();
    return atoi(pos);
}

char* JsonReader::getStringPath(const char* json, const char* path) {
    if (!json || !path) return nullptr;

    // Simple dot-path traversal: "characters.player.model"
    // Navigates nested JSON objects
    const char* current = json;
    char* pathDup = strdup(path);
    char* token = strtok(pathDup, ".");

    while (token) {
        std::string search = "\"";
        search += token;
        search += "\":";

        const char* pos = strstr(current, search.c_str());
        if (!pos) {
            free(pathDup);
            return nullptr;
        }

        pos += search.length();

        // Skip whitespace
        while (*pos == ' ' || *pos == '\t' || *pos == '\n') ++pos;

        token = strtok(nullptr, ".");
        if (token) {
            // Expect an object
            if (*pos != '{') { free(pathDup); return nullptr; }
            current = pos + 1;
        } else {
            // Last token — extract value
            if (*pos == '"') {
                ++pos;
                const char* end = strchr(pos, '"');
                if (!end) { free(pathDup); return nullptr; }
                size_t len = end - pos;
                char* result = static_cast<char*>(malloc(len + 1));
                memcpy(result, pos, len);
                result[len] = '\0';
                free(pathDup);
                return result;
            }
            return nullptr;
        }
    }

    free(pathDup);
    return nullptr;
}

} // namespace obris
