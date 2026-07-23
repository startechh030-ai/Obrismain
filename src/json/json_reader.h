#ifndef OBRIS_JSON_READER_H
#define OBRIS_JSON_READER_H

#include <string>
#include <cstdlib>

namespace obris {

/// Minimal JSON reader for asset manifests.
/// Reads JSON files from Android assets and does simple flat key lookups.
class JsonReader {
public:
    /// Load a JSON file from Android assets. Returns allocated string.
    /// Caller must free with free().
    static char* loadFromAssets(const char* path);

    /// Simple flat key-value parsing. Works for: { "key": "value" }
    /// Returns allocated string, caller must free with free().
    static char* getString(const char* json, const char* key);

    /// Get float value from flat JSON.
    static float getFloat(const char* json, const char* key);

    /// Get int value from flat JSON.
    static int getInt(const char* json, const char* key);

    /// Get a string from nested path like "characters.player.model"
    /// Returns allocated string, caller must free with free().
    static char* getStringPath(const char* json, const char* path);
};

} // namespace obris

#endif
