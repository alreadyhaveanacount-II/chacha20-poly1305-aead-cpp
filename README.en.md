# ChaCha20-Poly1305 AEAD

A mid performance ChaCha20-Poly1305 AEAD implementation made for fun and learning

---
## Building

### Windows (MSVC/PowerShell)
```
powershell
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### Linux

```
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

---

## Features

- ChaCha20 Cipher: Full implementation of the RFC 8439 standard.
- ChaCha20-Poly1305 AEAD: Authenticated Encryption with Associated Data (AEAD) construction for secure communication.
- High-Precision Benchmarking: Performance tracking using Cycles Per Byte (CPB) via RDTSCP and LFENCE serialization.
- Detailed throughput analysis (MB/s) with Average, Best, and Worst case metrics.
- Statistical analysis including Interquartile Range (IQR) to filter system noise and jitter.
- Modern C++ Architecture:
- Object-oriented design with a clean API for easy integration.
- Strict adherence to test vectors for 100% cryptographic correctness.
- Efficient memory management using std::vector and raw pointer buffers for zero-copy potential along with memory locking and zeroing for security.
- Cross-Platform Build: Native support for Windows (MSVC) and Linux (GCC/Clang) via CMake.

---

## Security notes

- Not constant-time audited
- Not side-channel hardened
- Not intended for production cryptography
- Educational / experimental

---

## Design notes

- Follows RFC 8439 state layout and quarter-round structure
- Tested against all vectors provided in appendix A of RFC 8439

---

## Simple API usage

```
ChaCha20_Poly1305::encrypt(
    key,
    nonce,
    plaintext,
    aad,
    ciphertext,
    tag
);
```

---

## Performance results

```
Warmup done

=======================================================
 ChaCha20 performance tests
=======================================================
[ THROUGHPUT ]
  Best:                         715.7812MB/s
  Worst:                        381.3228MB/s
  Average:                      643.4753MB/s
  Amplitude:                    334.4584MB/s
  IQR:                           60.7639MB/s

[ TIME (seconds) ]
  Smallest:                       0.0112s
  Biggest:                        0.0210s
  Average:                        0.0126s
  Amplitude:                      0.0098s
  IQR:                            0.0011s
[ EFFICIENCY ]
  Average CPB:                    3.7629 c/B
=======================================================

Encryption/Decryption match

=======================================================
 ChaCha20-Poly1305 encryption metrics
=======================================================
[ THROUGHPUT ]
  Best:                         471.1647MB/s
  Worst:                        323.5539MB/s
  Average:                      416.6988MB/s
  Amplitude:                    147.6108MB/s
  IQR:                           36.5842MB/s

[ TIME (seconds) ]
  Smallest:                       0.0170s
  Biggest:                        0.0247s
  Average:                        0.0193s
  Amplitude:                      0.0077s
  IQR:                            0.0017s
[ EFFICIENCY ]
  Average CPB:                    5.7568 c/B
=======================================================


=======================================================
 ChaCha20-Poly1305 decryption metrics
=======================================================
[ THROUGHPUT ]
  Best:                         468.6173MB/s
  Worst:                        352.4446MB/s
  Average:                      422.7914MB/s
  Amplitude:                    116.1726MB/s
  IQR:                           48.2270MB/s

[ TIME (seconds) ]
  Smallest:                       0.0171s
  Biggest:                        0.0227s
  Average:                        0.0190s
  Amplitude:                      0.0056s
  IQR:                            0.0022s
[ EFFICIENCY ]
  Average CPB:                    5.6612 c/B
=======================================================
```