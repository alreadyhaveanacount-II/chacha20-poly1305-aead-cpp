#pragma once
#include <poly1305.hpp>
#include <chacha20.hpp>
#include <cstdint>

namespace ChaCha20_Poly1305 {
    // Poly Helper Function

    inline void poly_pad16(Poly1305& p, size_t len) {
        size_t rem = len & 15;
        if (rem) {
            static const uint8_t zero[16] = { 0 };
            p.update(zero, 16 - rem);
        }
    }

    inline void encrypt(
        ChaCha20& c,
        const uint8_t* plaintext, size_t plaintext_len,
        const uint8_t* aad, size_t aad_len,
        uint8_t* output,
        uint8_t* tag)
    {
        // 1. Poly1305 one-time key (counter = 0)
        uint8_t key_block[64] = { 0 };
        c.set_counter(0);
        c.process(key_block, key_block, 64);

        Poly1305 p(key_block);

        // 2. AAD
        if (aad_len) {
            p.update(aad, aad_len);
            poly_pad16(p, aad_len);
        }

        // 3. Encrypt plaintext (counter = 1)
        c.set_counter(1);
        c.process(plaintext, output, plaintext_len);

        // 4. Ciphertext
        if (plaintext_len) {
            p.update(output, plaintext_len);
            poly_pad16(p, plaintext_len);
        }

        // 5. Lengths (LE64)
        uint64_t aad_len_le = aad_len;
        uint64_t ct_len_le = plaintext_len;

        p.update(reinterpret_cast<uint8_t*>(&aad_len_le), 8);
        p.update(reinterpret_cast<uint8_t*>(&ct_len_le), 8);

        // 6. Final tag
        p.final_(tag);
    }

    inline bool constant_time_compare(const uint8_t* a, const uint8_t* b, size_t len) {
        uint8_t diff = 0;
        for (size_t i = 0; i < len; i++) {
            diff |= a[i] ^ b[i];
        }

        return diff == 0;
    }

    inline bool decrypt(
        ChaCha20& c,
        const uint8_t* ciphertext, size_t ciphertext_len,
        const uint8_t* aad, size_t aad_len,
        const uint8_t* received_tag,
        uint8_t* output)
    {
        // 1. Poly1305 key (counter = 0)
        uint8_t key_block[64] = { 0 };
        c.set_counter(0);
        c.process(key_block, key_block, 64);

        Poly1305 p(key_block);

        // 2. AAD
        if (aad_len) {
            p.update(aad, aad_len);
            poly_pad16(p, aad_len);
        }

        // 3. Ciphertext
        if (ciphertext_len) {
            p.update(ciphertext, ciphertext_len);
            poly_pad16(p, ciphertext_len);
        }

        // 4. Lengths
        uint64_t aad_len_le = aad_len;
        uint64_t ct_len_le = ciphertext_len;

        p.update(reinterpret_cast<uint8_t*>(&aad_len_le), 8);
        p.update(reinterpret_cast<uint8_t*>(&ct_len_le), 8);

        // 5. Verify tag (constant time)
        uint8_t calc_tag[16];
        p.final_(calc_tag);

        if(!constant_time_compare(calc_tag, received_tag, 16)) {
            return false; // Authentication failed
		}

        // 6. Decrypt only after authentication
        c.set_counter(1);
        c.process(ciphertext, output, ciphertext_len);

        return true;
    }
}