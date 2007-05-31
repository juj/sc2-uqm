/*
 *  Copyright Serge van den Boom (svdb@stack.nl)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

// Define WANT_64BITS_BYTESEX before includeing this file if you want
// functions for 64 bits integers.

#ifndef _CBYTESEX_H
#define _CBYTESEX_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include <assert.h>

static inline uint8_t
extractBit(uint32_t u32, uint8_t bitNr) {
	return (u32 >> bitNr) & 1;
}

static inline uint32_t
extractBits(uint32_t u32, uint8_t firstBit, uint8_t numBits) {
	uint32_t temp = (uint32_t) (u32 << (32 - (numBits + firstBit)));
			// Shift out the unneeded high bits.
	return temp >> (32 - numBits);
			// Shift out the unneeded low bits.
}


#ifdef WANT_64BITS_BYTESEX
static inline uint32_t
highU32(uint64_t u64) {
	return (uint32_t) (u64 >> 32);
}

static inline uint32_t
lowU32(uint64_t u64) {
	return (uint32_t) u64;
}
#endif  /* WANT_64BITS_BYTESEX */

static inline uint16_t
highU16(uint32_t u32) {
	return (uint16_t) (u32 >> 16);
}

static inline uint16_t
lowU16(uint32_t u32) {
	return (uint16_t) u32;
}

static inline uint8_t
highU8(uint16_t u16) {
	return (uint8_t) (u16 >> 8);
}

static inline uint8_t
lowU8(uint16_t u16) {
	return (uint8_t) u16;
}

static inline uint8_t
highU4(uint8_t u8) {
	return u8 >> 4;
}

static inline uint8_t
lowU4(uint8_t u8) {
	return u8 & 0x0f;
}

#ifdef WANT_64BITS_BYTESEX
static inline uint32_t
makeU64(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3,
		uint8_t b4, uint8_t b5, uint8_t b6, uint8_t b7) {
	uint16_t w0 = (b1 << 8) | b0;
	uint16_t w1 = (b3 << 8) | b2;
	uint16_t w2 = (b5 << 8) | b4;
	uint16_t w3 = (b7 << 8) | b6;

	uint32_t dw0 = (w1 << 16) | w0;
	uint32_t dw1 = (w3 << 16) | w2;

	return (dw1 << 32) | dw0;
}

static inline uint64_t
makeU64fromU32(uint32_t lo, uint32_t hi) {
	return (hi << 32) | lo;
}
#endif  /* WANT_64BITS_BYTESEX */

static inline uint32_t
makeU32(uint8_t lowest, uint8_t low, uint8_t high, uint8_t highest) {
	return (highest << 24) | (high << 16) | (low << 8) | lowest;
}

static inline uint32_t
makeU32fromU16(uint16_t lo, uint16_t hi) {
	return (hi << 16) | lo;
}

static inline uint16_t
makeU16(uint8_t lo, uint8_t hi) {
	return (hi << 8) | lo;
}


static inline uint8_t
getU8(const void *ptr) {
	return ((const uint8_t *) ptr)[0];
}

static inline uint16_t
getU16BE(const void *ptr) {
	const uint8_t *bytePtr = (const uint8_t *) ptr;
	return (bytePtr[0] << 8) | bytePtr[1];
}

static inline uint16_t
getU16LE(const void *ptr) {
	const uint8_t *bytePtr = (const uint8_t *) ptr;
	return (bytePtr[1] << 8) | bytePtr[0];
}

static inline uint32_t
getU32BE(const void *ptr) {
	const uint8_t *bytePtr = (const uint8_t *) ptr;
	return (bytePtr[0] << 24) | (bytePtr[1] << 16) |
			(bytePtr[2] << 8) | bytePtr[3];
}

static inline uint32_t
getU32LE(const void *ptr) {
	const uint8_t *bytePtr = (const uint8_t *) ptr;
	return (bytePtr[3] << 24) | (bytePtr[2] << 16) |
			(bytePtr[1] << 8) | bytePtr[0];
}

#ifdef WANT_64BITS_BYTESEX
static inline uint64_t
getU64BE(const void *ptr) {
	const uint8_t *bytePtr = (const uint8_t *) ptr;
	return (bytePtr[0] << 56) | (bytePtr[1] << 48) |
			(bytePtr[2] << 40) | (bytePtr[3] << 32);
			(bytePtr[4] << 24) | (bytePtr[5] << 16) |
			(bytePtr[6] << 8) | bytePtr[7];
}

static inline uint64_t
getU64LE(const void *ptr) {
	const uint8_t *bytePtr = (const uint8_t *) ptr;
	return (bytePtr[7] << 56) | (bytePtr[6] << 48) |
			(bytePtr[5] << 40) | (bytePtr[4] << 32);
			(bytePtr[3] << 24) | (bytePtr[2] << 16) |
			(bytePtr[1] << 8) | bytePtr[0];
}
#endif  /* WANT_64BITS_BYTESEX */


#define MAKE_SIGNED_GET_FUNC(suffix, bits) \
		static inline int##bits##_t \
		getS##suffix(const void *ptr) { \
			uint##bits##_t result = getU##suffix(ptr); \
			return *((int##bits##_t *) &result); \
		}

#define MAKE_EAT_FUNC(suffix, type, byteCount) \
		static inline type \
		eat##suffix(const void **ptr) { \
			type result = get##suffix(*ptr); \
			*ptr = (const void *) ((const uint8_t *) *ptr + byteCount); \
			return result; \
		}

// The order is important; the dependencies need to be put first for
// inlining to work (at least for GCC).
MAKE_SIGNED_GET_FUNC(8,     8)
MAKE_SIGNED_GET_FUNC(16BE, 16)
MAKE_SIGNED_GET_FUNC(16LE, 16)
MAKE_SIGNED_GET_FUNC(32BE, 32)
MAKE_SIGNED_GET_FUNC(32LE, 32)
#ifdef WANT_64BITS_BYTESEX
MAKE_SIGNED_GET_FUNC(64BE, 64)
MAKE_SIGNED_GET_FUNC(64LE, 64)
#endif  /* WANT_64BITS_BYTESEX */

MAKE_EAT_FUNC(U8,    uint8_t,  1)
MAKE_EAT_FUNC(U16BE, uint16_t, 2)
MAKE_EAT_FUNC(U16LE, uint16_t, 2)
MAKE_EAT_FUNC(U32BE, uint32_t, 4)
MAKE_EAT_FUNC(U32LE, uint32_t, 4)
#ifdef WANT_64BITS_BYTESEX
MAKE_EAT_FUNC(U64BE, uint64_t, 8)
MAKE_EAT_FUNC(U64LE, uint64_t, 8)
#endif  /* WANT_64BITS_BYTESEX */

MAKE_EAT_FUNC(S8,    int8_t,  1)
MAKE_EAT_FUNC(S16BE, int16_t, 2)
MAKE_EAT_FUNC(S16LE, int16_t, 2)
MAKE_EAT_FUNC(S32BE, int32_t, 4)
MAKE_EAT_FUNC(S32LE, int32_t, 4)
#ifdef WANT_64BITS_BYTESEX
MAKE_EAT_FUNC(S64BE, int64_t, 8)
MAKE_EAT_FUNC(S64LE, int64_t, 8)
#endif  /* WANT_64BITS_BYTESEX */

#undef MAKE_EAT_FUNC
#undef MAKE_SIGNED_GET_FUNC

typedef struct BitStreamContext BitStreamContext;
struct BitStreamContext {
	uint8_t bitBuf;
			// First entry; fastest too access (if it matters at all)
	uint8_t bitBufFill;
	const uint8_t *ptr;
	const uint8_t *end;
	bool eof;
};

static inline void
BSC_init(BitStreamContext *bsc, const void *ptr, size_t len) {
	bsc->bitBuf = 0;
	bsc->bitBufFill = 0;
	bsc->ptr = (const uint8_t *) ptr;
	bsc->end = bsc->ptr + len;
	bsc->eof = 0;
}

static inline bool
BSC_haveBit_OPAB(BitStreamContext *bsc) {
	if (bsc->ptr != bsc->end)
		return true;
	if (bsc->bitBufFill != 0)
		return true;
	return false;
}

static inline uint8_t
BSC_getBit_OPAB(BitStreamContext *bsc) {
	if (bsc->bitBufFill == 0) {
		bsc->bitBuf = *bsc->ptr;
		bsc->ptr++;
		bsc->bitBufFill = 7;
	} else {
		bsc->bitBufFill--;
	}

	uint8_t result = bsc->bitBuf & 1;
	bsc->bitBuf >>= 1;
	return result;
}

static inline bool
BSC_haveBits_OPAB(BitStreamContext *bsc, size_t numBits) {
	size_t bytesLeft = bsc->end - bsc->ptr;
	return ((bytesLeft << 3) + bsc->bitBufFill >= numBits);
}

// Bit nr in the stream:   76543210 76543210
// Bits in the stream:     ABCDEFGH IJKLMNOP
// Reading 6 bits yields CDEFGH
// Reading 4 bits after that yields OPAB
static inline uint32_t
BSC_getBits_OPAB(BitStreamContext *bsc, size_t numBits) {
	assert (numBits <= 32);

	uint32_t result = 0;
	uint8_t gotBits = 0;
	while (bsc->bitBufFill < numBits) {
		result |= bsc->bitBuf << gotBits;
		gotBits += bsc->bitBufFill;
		numBits -= bsc->bitBufFill;
		bsc->bitBufFill = 8;
		bsc->bitBuf = *bsc->ptr;
		bsc->ptr++;
	}

	result |= ((uint8_t) (bsc->bitBuf << (8 - numBits)) >> (8 - numBits)) <<
			gotBits;
	bsc->bitBuf >>= numBits;
	bsc->bitBufFill -= numBits;

	return result;
}

static inline size_t
BSC_bitsLeft_OPAB(BitStreamContext *bsc) {
	size_t bytesLeft = bsc->end - bsc->ptr;
	return (bytesLeft << 3) + bsc->bitBufFill;
}

#endif  /* _CBYTESEX_H */


