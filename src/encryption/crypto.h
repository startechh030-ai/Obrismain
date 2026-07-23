#ifndef OBRIS_CRYPTO_H
#define OBRIS_CRYPTO_H

#include <cstdint>
#include <cstddef>

namespace obris {

/// Cryptographic utilities wrapping libsodium.
/// Provides XChaCha20-Poly1305 encryption.
class Crypto {
public:
    static bool init();
    static bool isInitialized();

    /// Encrypt: output is [nonce(24) + ciphertext].
    /// Caller must free the returned buffer with free().
    static unsigned char* encrypt(const unsigned char* key, size_t keyLen,
                                   const unsigned char* data, size_t dataLen,
                                   size_t* outLen);

    /// Decrypt: input is [nonce(24) + ciphertext].
    /// Caller must free the returned buffer with free().
    static unsigned char* decrypt(const unsigned char* key, size_t keyLen,
                                   const unsigned char* in, size_t inLen,
                                   size_t* outLen);

    static void generateKey(unsigned char* key, size_t len);
    static void randomBytes(unsigned char* buf, size_t len);

private:
    Crypto() = delete;
    static bool initialized_;
};

} // namespace obris

#endif
