#pragma once

#if defined(_WIN32) || defined(_WIN64)
#pragma comment(lib, "bcrypt.lib")
#include <windows.h>
#include <bcrypt.h>
#include <fstream>
#include <dpapi.h>
// VirtualLock/Unlock are in windows.h (via memoryapi.h)
#elif defined(__linux__)
#include <sys/random.h> // For getrandom()
#include <sys/mman.h>   // For mlock() and munlock()
#include <sys/syscall.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#include <stdexcept>
#include <cstdint>
#include <vector>
#include <format>
#include <iostream>
#include <cstring>
#include <string_view>
#include <array>

namespace CryptoHelper {
	// Memory helpers

    inline void secure_zero_memory(void* ptr, size_t len) {
        if (!ptr || len == 0) return;
#if defined(_WIN32) || defined(_WIN64)
        SecureZeroMemory(ptr, len); // Windows-native secure wipe
#elif defined(__linux__) || defined(__GLIBC__)
        explicit_bzero(ptr, len);   // Linux-native secure wipe
#else
        // Generic secure wipe
        volatile uint8_t* p = static_cast<volatile uint8_t*>(ptr);
        while (len--) { *p++ = 0; }
#endif
    }


    inline bool lock_memory(void* ptr, size_t len) {
#if defined(_WIN32) || defined(_WIN64)
        return (VirtualLock(ptr, len) != 0); // Non-zero is success
#elif defined(__linux__)
        return (mlock(ptr, len) == 0);      // Zero is success
#else
        std::cerr << "Locking not supported" << std::endl;
        return false;
#endif
    }

    inline bool unlock_memory(void* ptr, size_t len) {
#if defined(_WIN32) || defined(_WIN64)
        return (VirtualUnlock(ptr, len) != 0); // Non-zero is success
#elif defined(__linux__)
        return (munlock(ptr, len) == 0);      // Zero is success
#else
        std::cerr << "Unlocking not supported" << std::endl;
        return false;
#endif
    }

    // Byte array bit-size conversion

    inline void _8bitarray_to32bitarray(const uint8_t* byteArray, uint32_t* _32bitArray, size_t byteLength) {
        for (size_t i = 0; i < byteLength / 4; ++i) {
            _32bitArray[i] = (uint32_t)byteArray[i * 4] |
                (uint32_t)byteArray[i * 4 + 1] << 8 |
                (uint32_t)byteArray[i * 4 + 2] << 16 |
                (uint32_t)byteArray[i * 4 + 3] << 24;
        }
    }

    inline void _32bitarray_to8bitarray(const uint32_t* _32bitArray, uint8_t* byteArray, size_t byteLength) {
        for (size_t i = 0; i < byteLength / 4; ++i) {
            uint32_t value = _32bitArray[i];
            byteArray[i * 4] = (uint8_t)(value & 0xFF);
            byteArray[i * 4 + 1] = (uint8_t)((value >> 8) & 0xFF);
            byteArray[i * 4 + 2] = (uint8_t)((value >> 16) & 0xFF);
            byteArray[i * 4 + 3] = (uint8_t)((value >> 24) & 0xFF);
        }
    }


    // Conversion helper

    inline std::vector<uint8_t> string_to_bytes(std::string s) {
        return std::vector<uint8_t>(s.begin(), s.end());
    }

    inline std::string bytes_to_string(std::vector<uint8_t> const& bytes) {
        return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }

    // Printing helper

    inline void print_bytes_hex(const uint8_t* byteArray, size_t byteLength, size_t rowLength) {
        for (size_t i = 0; i < byteLength; i++) {
            std::cout << std::format("{:02x} ", byteArray[i]);
            if ( ((i + 1) % rowLength) == 0) std::cout << std::endl;
        }
    }

	// Random byte generation

    inline void gen_secure_random_bytes(uint8_t* buffer, size_t length) {
#if defined(_WIN32) || defined(_WIN64)
        if (!BCRYPT_SUCCESS(BCryptGenRandom(NULL, buffer, static_cast<ULONG>(length), BCRYPT_USE_SYSTEM_PREFERRED_RNG))) {
            throw std::runtime_error("BCryptGenRandom failed");
        }
#elif defined(__linux__)
        size_t total_read = 0;
        while (total_read < length) {
            ssize_t result = getrandom(buffer + total_read, length - total_read, 0);

            if (result == -1) {
                if (errno == EINTR) continue;
                throw std::runtime_error("getrandom failed: " + std::to_string(errno));
            }
            total_read += result;
        }
#else
        throw std::runtime_error("Platform not supported");
#endif
    }
}
