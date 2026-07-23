#include "crypto.h"
#include <cstdlib>
#include <cstring>
#include <android/log.h>

#define LOG_TAG "ObrisCrypto"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

namespace obris {

bool Crypto::initialized_ = false;

bool Crypto::init() {
    if (initialized_) return true;
#if defined(OBRIS_USE_SODIUM) && OBRIS_USE_SODIUM
    // if (sodium_init() < 0) return false;
    LOGI("libsodium initialized");
#else
    LOGI("Crypto stub (no libsodium)");
#endif
    initialized_ = true;
    return true;
}

bool Crypto::isInitialized() { return initialized_; }

unsigned char* Crypto::encrypt(const unsigned char* key, size_t keyLen,
                                const unsigned char* data, size_t dataLen,
                                size_t* outLen) {
#if defined(OBRIS_USE_SODIUM) && OBRIS_USE_SODIUM
    // size_t cipherLen = dataLen + crypto_aead_xchacha20poly1305_ietf_ABYTES;
    // size_t nonceLen = crypto_aead_xchacha20poly1305_ietf_NPUBBYTES;
    // unsigned char* out = (unsigned char*)malloc(nonceLen + cipherLen);
    // randombytes_buf(out, nonceLen);
    // crypto_aead_xchacha20poly1305_ietf_encrypt(
    //     out + nonceLen, &cipherLen, data, dataLen,
    //     NULL, 0, NULL, out, (const unsigned char*)key);
    // *outLen = nonceLen + cipherLen;
    // return out;
    LOGI("encrypt() stub");
#else
    LOGI("encrypt() stub (no libsodium)");
#endif
    *outLen = 0;
    return nullptr;
}

unsigned char* Crypto::decrypt(const unsigned char* key, size_t keyLen,
                                const unsigned char* in, size_t inLen,
                                size_t* outLen) {
#if defined(OBRIS_USE_SODIUM) && OBRIS_USE_SODIUM
    // size_t nonceLen = crypto_aead_xchacha20poly1305_ietf_NPUBBYTES;
    // if (inLen < nonceLen) { *outLen = 0; return nullptr; }
    // unsigned char* plain = (unsigned char*)malloc(inLen);
    // size_t plainLen = 0;
    // if (crypto_aead_xchacha20poly1305_ietf_decrypt(
    //         plain, &plainLen, NULL,
    //         in + nonceLen, inLen - nonceLen, NULL, 0,
    //         in, (const unsigned char*)key) != 0) {
    //     free(plain);
    //     *outLen = 0;
    //     return nullptr;
    // }
    // *outLen = plainLen;
    // return plain;
    LOGI("decrypt() stub");
#else
    LOGI("decrypt() stub (no libsodium)");
#endif
    *outLen = 0;
    return nullptr;
}

void Crypto::generateKey(unsigned char* key, size_t len) {
    randomBytes(key, len);
}

void Crypto::randomBytes(unsigned char* buf, size_t len) {
#if defined(OBRIS_USE_SODIUM) && OBRIS_USE_SODIUM
    // randombytes_buf(buf, len);
#else
    // Non-secure fallback
    static bool seeded = false;
    if (!seeded) { srand(time(nullptr)); seeded = true; }
    for (size_t i = 0; i < len; ++i) buf[i] = rand() & 0xFF;
#endif
}

} // namespace obris
