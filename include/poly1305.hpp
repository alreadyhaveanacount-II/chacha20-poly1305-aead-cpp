#pragma once

#define NOMINMAX
#define min_(a,b) (((a) < (b)) ? (a) : (b)) // a massive workaround because MSVC is dumb and doesn't handle std::min correctly

#include <chacha20.hpp>
#include <helper.hpp>
#include <vector>
#include <algorithm>

#define mask26 0x3FFFFFF

struct Poly1305 {
private:
	alignas(16) uint64_t r[5];
	alignas(16) uint64_t s[2];
	alignas(16) uint64_t acc[5]{ 0 };
	alignas(32) uint8_t partial[16];
	size_t partial_len = 0;

	inline void bytes_to_limbs(const uint8_t* bytes, uint64_t* limbs, bool is_message_block = false, size_t l = 16) {
		uint64_t low, high;
		std::memcpy(&low, bytes, 8);
		std::memcpy(&high, bytes + 8, 8);
	
		limbs[0] = low & mask26;
		limbs[1] = (low >> 26) & mask26;
		limbs[2] = ((low >> 52) | (high << 12)) & mask26;
		limbs[3] = (high >> 14) & mask26;
		limbs[4] = (high >> 40);
	
		int bit_pos = l * 8;
		int limb_idx = bit_pos / 26;
		int bit_in_limb = bit_pos % 26;
	
		limbs[limb_idx] |= (1ULL << bit_in_limb);
	}

	void mul_mod_p(uint64_t* r, uint64_t* acc);

	inline void add_limbs(uint64_t* a, const uint64_t* b) {
		uint64_t carry = 0;

		for (size_t i = 0; i < 5; i++) {
			a[i] += b[i] + carry;
			carry = a[i] >> 26;
			a[i] &= mask26;
		}

		a[0] += carry * 5;

		carry = a[0] >> 26;
		a[0] &= mask26;
		a[1] += carry;
	}

	inline void process_block(const uint8_t block[16], bool is_message_block, size_t block_size=16) {
		uint64_t msg_limbs[5];
		bytes_to_limbs(block, msg_limbs, is_message_block, block_size);
		add_limbs(acc, msg_limbs);
		mul_mod_p(r, acc);
	}

public:
	Poly1305(uint8_t block[64]);

	void update(const uint8_t* data, size_t len) {
		size_t offset = 0;

		// Finish partial block if present
		if (partial_len > 0) {
			size_t take = min_(len, 16 - partial_len);
			std::memcpy(partial + partial_len, data, take);
			partial_len += take;
			offset += take;

			if (partial_len == 16) {
				process_block(partial, true);
				partial_len = 0;
			}
		}

		// Full blocks
		while (offset + 16 <= len) {
			process_block(data + offset, true);
			offset += 16;
		}

		// Remainder
		if (offset < len) {
			partial_len = len - offset;
			std::memcpy(partial, data + offset, partial_len);
		}
	}

	void final_(uint8_t tag[16]);

	void update_pad16(size_t len) {
		size_t rem = len % 16;
		if (rem == 0) return;

		uint8_t zero[16] = { 0 };
		process_block(zero, false);
	}

	~Poly1305() {
		CryptoHelper::secure_zero_memory(r, 5 * sizeof(uint64_t));
		CryptoHelper::secure_zero_memory(s, 2 * sizeof(uint64_t));
		CryptoHelper::secure_zero_memory(acc, 5 * sizeof(uint64_t));
		CryptoHelper::unlock_memory(this, sizeof(Poly1305));
	}
};

inline Poly1305::Poly1305(uint8_t block[64]) {
	CryptoHelper::lock_memory(this, sizeof(Poly1305));

	block[3] &= 15;
	block[7] &= 15;
	block[11] &= 15;
	block[15] &= 15;
	block[4] &= 252;
	block[8] &= 252;
	block[12] &= 252;

	uint32_t b0, b1, b2, b3;
	std::memcpy(&b0, block, 4);
	std::memcpy(&b1, block + 4, 4);
	std::memcpy(&b2, block + 8, 4);
	std::memcpy(&b3, block + 12, 4);

	// Clamping
	b0 &= 0x0FFFFFFF;
	b1 &= 0x0FFFFFFC;
	b2 &= 0x0FFFFFFC;
	b3 &= 0x0FFFFFFC;

	// 2. Convert to 26-bit limbs for r (In-register manipulation)
	r[0] = b0 & mask26;
	r[1] = ((b0 >> 26) | (b1 << 6)) & mask26;
	r[2] = ((b1 >> 20) | (b2 << 12)) & mask26;
	r[3] = ((b2 >> 14) | (b3 << 18)) & mask26;
	r[4] = (b3 >> 8) & mask26;

	// 3. Load s as two 64-bit values
	std::memcpy(&s[0], &block[16], 8);
	std::memcpy(&s[1], &block[24], 8);

	// Initialize accumulator to zero
	std::memset(acc, 0, 5 * sizeof(uint64_t));
}


inline void Poly1305::mul_mod_p(uint64_t* r, uint64_t* acc) {
	uint64_t a0 = acc[0], a1 = acc[1], a2 = acc[2], a3 = acc[3], a4 = acc[4];
	uint64_t r0 = r[0], r1 = r[1], r2 = r[2], r3 = r[3], r4 = r[4];

	uint64_t r1_5 = r1 * 5;
	uint64_t r2_5 = r2 * 5;
	uint64_t r3_5 = r3 * 5;
	uint64_t r4_5 = r4 * 5;

	// Multiply (schoolbook + modulus folding)
	uint64_t t0 = a0 * r0 + a1 * r4_5 + a2 * r3_5 + a3 * r2_5 + a4 * r1_5;
	uint64_t t1 = a0 * r1 + a1 * r0 + a2 * r4_5 + a3 * r3_5 + a4 * r2_5;
	uint64_t t2 = a0 * r2 + a1 * r1 + a2 * r0 + a3 * r4_5 + a4 * r3_5;
	uint64_t t3 = a0 * r3 + a1 * r2 + a2 * r1 + a3 * r0 + a4 * r4_5;
	uint64_t t4 = a0 * r4 + a1 * r3 + a2 * r2 + a3 * r1 + a4 * r0;

	// Carry propagation
	uint64_t c;

	c = t0 >> 26; acc[0] = t0 & mask26; t1 += c;
	c = t1 >> 26; acc[1] = t1 & mask26; t2 += c;
	c = t2 >> 26; acc[2] = t2 & mask26; t3 += c;
	c = t3 >> 26; acc[3] = t3 & mask26; t4 += c;
	c = t4 >> 26; acc[4] = t4 & mask26;

	// Final reduction: fold carry from top limb
	acc[0] += c * 5;
	c = acc[0] >> 26; acc[0] &= mask26; acc[1] += c;
}

inline void Poly1305::final_(uint8_t tag[16]) {
	if (partial_len > 0) {
		uint8_t block[16] = { 0 };
		std::memcpy(block, partial, partial_len);
		process_block(block, true, partial_len); // Pad partial blocks
	}

	// 1. Fully reduce acc mod (2^130 - 5), starting with carry propagation
	uint64_t c = 0;
	for (int i = 0; i < 5; i++) {
		acc[i] += c;
		c = acc[i] >> 26;
		acc[i] &= mask26;
	}

	// 2. Serialize acc back to 128-bit
	uint64_t low = acc[0] | (acc[1] << 26) | (acc[2] << 52);
	uint64_t high = (acc[2] >> 12) | (acc[3] << 14) | (acc[4] << 40);

	// 3. Add s (the 128-bit key part) using 64-bit carry math
	unsigned char carry = 0;
	#if defined(_MSC_VER)
		carry = _addcarry_u64(0, low, s[0], &low);
		_addcarry_u64(carry, high, s[1], &high);
	#else
		low += s[0];
		carry = (low < s[0]);
		high += s[1] + carry;
	#endif

	// 4. Fast serialization to tag
	std::memcpy(tag, &low, 8);
	std::memcpy(tag + 8, &high, 8);
}
