#pragma once
#define NOMINMAX
#include <cstdint>
#include <vector>
#include <bit>
#include "helper.hpp"
#include <assert.h>

struct ChaCha20 {
public:
    ChaCha20(const uint32_t key[8], const uint32_t nonce[3]);

    void set_counter(uint32_t counter);
    void process(const uint8_t* input, uint8_t* output, size_t length);

    static ChaCha20 genRandomParams();

    ~ChaCha20() {
        CryptoHelper::secure_zero_memory(state, sizeof(state));
        CryptoHelper::unlock_memory(this, sizeof(ChaCha20));
    }

    ChaCha20(const ChaCha20&) = delete;
    ChaCha20& operator=(const ChaCha20&) = delete;

    ChaCha20(ChaCha20&&) noexcept = default;
    ChaCha20& operator=(ChaCha20&&) = delete;
private:
    alignas(64) uint32_t state[16];

    void quarter_round(uint32_t& a, uint32_t& b, uint32_t& c, uint32_t& d);
    void blockFunction(uint8_t output[64], size_t to_copy);
};

inline ChaCha20::ChaCha20(const uint32_t key[8], const uint32_t nonce[3]) {
    if(!key || !nonce) {
        throw std::invalid_argument("Key and Nonce must not be null");
	}

    CryptoHelper::lock_memory(this, sizeof(ChaCha20));

    this->state[0] = 0x61707865; // "expa"
    this->state[1] = 0x3320646e; // "nd 3"
    this->state[2] = 0x79622d32; // "2-by"
    this->state[3] = 0x6b206574; // "te k"

    for (size_t i = 0; i < 8; ++i) {
        this->state[4 + i] = key[i];
    }

    // Set initial counter to 0
    this->state[12] = 0;

    for (size_t i = 0; i < 3; ++i) {
        this->state[13 + i] = nonce[i];
    }
}

inline void ChaCha20::quarter_round(uint32_t& a, uint32_t& b, uint32_t& c, uint32_t& d) {
    a += b; d ^= a; d = std::rotl(d, 16);
    c += d; b ^= c; b = std::rotl(b, 12);
    a += b; d ^= a; d = std::rotl(d, 8);
    c += d; b ^= c; b = std::rotl(b, 7);
}

inline void ChaCha20::set_counter(uint32_t counter) {
    state[12] = counter;
}

inline void ChaCha20::blockFunction(uint8_t output[64], size_t to_copy = 64) {
    assert(to_copy <= 64 && "Cannot copy more than 64 bytes");

    alignas(64) uint32_t working_state[16];
    std::memcpy(working_state, state, 16 * sizeof(uint32_t));

#pragma loop(ivdep)
    for (int i = 0; i < 10; ++i) {
        // Column rounds
        quarter_round(working_state[0], working_state[4], working_state[8], working_state[12]);
        quarter_round(working_state[1], working_state[5], working_state[9], working_state[13]);
        quarter_round(working_state[2], working_state[6], working_state[10], working_state[14]);
        quarter_round(working_state[3], working_state[7], working_state[11], working_state[15]);
        // Diagonal rounds
        quarter_round(working_state[0], working_state[5], working_state[10], working_state[15]);
        quarter_round(working_state[1], working_state[6], working_state[11], working_state[12]);
        quarter_round(working_state[2], working_state[7], working_state[8], working_state[13]);
        quarter_round(working_state[3], working_state[4], working_state[9], working_state[14]);
    }

#pragma loop(ivdep)
    for (int i = 0; i < 16; ++i) {
        working_state[i] += state[i];
    }

    std::memcpy(output, working_state, to_copy);
    state[12]++;
}

inline void process256_chunk(const uint8_t* input, uint8_t* output, const uint8_t* keyStream) {
    #ifdef __AVX2__
        // AVX2
        __m256i in = _mm256_loadu_si256((const __m256i*)input);
        __m256i key = _mm256_loadu_si256((const __m256i*)keyStream);
        _mm256_storeu_si256((__m256i*)output, _mm256_xor_si256(in, key));
    #else
        // SSE
        __m128i in_lo = _mm_loadu_si128((const __m128i*)input);
        __m128i in_hi = _mm_loadu_si128((const __m128i*)(input + 16));
        __m128i key_lo = _mm_loadu_si128((const __m128i*)keyStream);
        __m128i key_hi = _mm_loadu_si128((const __m128i*)(keyStream + 16));

        _mm_storeu_si128((__m128i*)output, _mm_xor_si128(in_lo, key_lo));
        _mm_storeu_si128((__m128i*)(output + 16), _mm_xor_si128(in_hi, key_hi));
    #endif
}

inline void process128_chunk(const uint8_t* input, uint8_t* output, const uint8_t* keyStream) {
    __m128i input_block = _mm_loadu_si128(reinterpret_cast<const __m128i*>(input));
    __m128i key_stream_block = _mm_loadu_si128(reinterpret_cast<const __m128i*>(keyStream));
    __m128i output_block = _mm_xor_si128(input_block, key_stream_block);

    _mm_storeu_si128(reinterpret_cast<__m128i*>(output), output_block);
}

inline void process64_chunk(const uint8_t* input, uint8_t* output, const uint8_t* keyStream) {
    uint64_t input_block;
    uint64_t key_stream_block;
    std::memcpy(&input_block, input, sizeof(uint64_t));
    std::memcpy(&key_stream_block, keyStream, sizeof(uint64_t));
    uint64_t output_block = input_block ^ key_stream_block;
    std::memcpy(output, &output_block, sizeof(uint64_t));
}

inline void process32_chunk(const uint8_t* input, uint8_t* output, const uint8_t* keyStream) {
    uint32_t input_block;
    uint32_t key_stream_block;
    std::memcpy(&input_block, input, sizeof(uint32_t));
    std::memcpy(&key_stream_block, keyStream, sizeof(uint32_t));
    uint32_t output_block = input_block ^ key_stream_block;
    std::memcpy(output, &output_block, sizeof(uint32_t));
}

inline void process_manually(const uint8_t* input, uint8_t* output, const uint8_t* keyStream, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        output[i] = input[i] ^ keyStream[i];
    }
}

inline void ChaCha20::process(const uint8_t* input, uint8_t* output, size_t length) {
    if(!input || !output || length == 0) {
		throw::std::invalid_argument("Input and Output buffers must not be null, and length must be greater than zero");
	}

    size_t offset = 0;
    alignas(64) uint8_t keystream[256];

    // 1. Process full 64-byte ChaCha blocks

    while (length - offset >= 64) {
        blockFunction(keystream); // Generates 64 bytes

        // Process the 64 bytes using TWO 256-bit (32-byte) AVX2 calls
        process256_chunk(input + offset, output + offset, keystream);
        process256_chunk(input + offset + 32, output + offset + 32, keystream + 32);

        offset += 64;
    }

    // 2. Handle the final partial block (0-63 bytes left)
    if (offset < length) {
        blockFunction(keystream); // Generate one last keystream block
        size_t remaining = length - offset;

        size_t local_off = 0;

        if (remaining - local_off >= 32) {
            process256_chunk(input + offset + local_off, output + offset + local_off, keystream + local_off);
            local_off += 32;
        }
        if (remaining - local_off >= 16) {
            process128_chunk(input + offset + local_off, output + offset + local_off, keystream + local_off);
            local_off += 16;
        }
        if (remaining - local_off >= 8) {
            process64_chunk(input + offset + local_off, output + offset + local_off, keystream + local_off);
            local_off += 8;
        }

        if (remaining - local_off > 0) {
            process_manually(input + offset + local_off, output + offset + local_off, keystream + local_off, remaining - local_off);
        }
    }
}