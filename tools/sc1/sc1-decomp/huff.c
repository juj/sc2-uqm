// Reverse engineered from the Star Control executable.

#include "huff.h"
#include "../../shared/cbytesex.h"
#include "getbit.h"

#include <errno.h>
#include <stdlib.h>

static bool Huff_compareCodes(const Huff_Code *srcCodePtr,
		const Huff_Code *destCodePtr);
static void Huff_sortAndStuff(int flag, Huff_Context *ctx);
static void Huff_sub_22334(Huff_Context *ctx);
static void Huff_reverseCodeBits(Huff_Context *ctx);
static bool Huff_readTable(Huff_Context *ctx);
static bool Huff_lookupCodeLarge(Huff_Context *ctx, DWORD *result);
static bool Huff_lookupCodeSmall(Huff_Context *ctx, DWORD *result);

Huff_Context *
Huff_new(BitStreamContext *bsc, WORD codeCount) {
	Huff_Context *result = malloc(sizeof (Huff_Context));

	result->codeCount = codeCount;
	result->bsc = bsc;
	
	if (!Huff_readTable(result))
		goto err;
	Huff_sortAndStuff(0, result);
	Huff_sub_22334(result);
	Huff_reverseCodeBits(result);
	Huff_sortAndStuff(1, result);

	return result;

err:
	free (result);
	return NULL;
}

void
Huff_delete(Huff_Context *ctx) {
	free(ctx);
}

// Returns true if we need to swap.
static bool
Huff_compareCodes(const Huff_Code *srcCodePtr, const Huff_Code *destCodePtr) {
	// 2203:016a

	if (destCodePtr->len < srcCodePtr->len)
		return true;

	if (destCodePtr->len > srcCodePtr->len)
		return false;

	if (destCodePtr->field_0 < srcCodePtr->field_0)
		return true;

	if (destCodePtr->field_0 > srcCodePtr->field_0)
		return false;

	if (destCodePtr->value < srcCodePtr->value)
		return true;

	return false;
}

static void
Huff_sortAndStuff(int flag, Huff_Context *ctx) {
	// 2203:00c6
	if (flag != 0 && ctx->maxCodeLen <= 8) {
		// ctx->codes is reordered so that
		// ctx->codes[i] == ctx->codes[i]->field_0

		// 2203:00d3
		ctx->u.topCodeIndexLen = ctx->codes[0].len;

		WORD codeI = 0;
		if (ctx->codeCount == 0)
			return;

		// 2203:00e9
		Huff_Code *codePtr = &ctx->codes[0];

		do {
			// 2203:011a
			// Put *codePtr in its place by swapping it with the Huff_Code
			// structure where it needs to go.
			// We continue doing this as long as the new value of *codePtr
			// is not yet in its place.
			while (codePtr->field_0 != codeI &&
					/* 2203:00ed */ codePtr->len != 0) {
				// 2203:00f3
				// Swap ctx->codes[codePtr->field_0] and *codePtr
				Huff_Code *otherCode = &ctx->codes[codePtr->field_0];
				Huff_Code tempCode = *codePtr;
				*codePtr = *otherCode;
				*otherCode = tempCode;
				// 2203:011a
			}
			
			// 2203:0121
			codePtr++;
			codeI++;
		} while (codeI < ctx->codeCount);

		return;
	} else {
		// Sorting the array of codes (using comb sort).
		// The codes with the smaller code length are put first.

		// 2203:0139
		WORD si = ctx->codeCount / 2;

		for (;;) {
			// 2203:0143
			bool noSwapThisRound = true;

			if (si >= ctx->codeCount)
				goto loc_22200;

			// 2203:0158
			Huff_Code *srcCodePtr = &ctx->codes[0];
			Huff_Code *destCodePtr = &ctx->codes[si];

			WORD cx = 0;
			do {
				// 2203:016a
				if (Huff_compareCodes(srcCodePtr, destCodePtr)) {
					// 2203:018e
					Huff_Code tempCode = *srcCodePtr;
					*srcCodePtr = *destCodePtr;
					*destCodePtr = tempCode;
					
					noSwapThisRound = false;
				}

				// 2203:01b3
				srcCodePtr++;
				destCodePtr++;
				cx++;
			} while (cx < ctx->codeCount - si);

loc_22200:
			// 2203:01d0
			if (!noSwapThisRound)
				continue;

			si /= 2;
			if (si <= 0)
				break;
		}

		// 2203:01e2
		if (flag == 0)
			return;
		
		si = 0;
		for (WORD cx = 0; cx < ctx->codeCount; cx++) {
			// Keep si from previous round.
			while (si < ctx->codes[cx].len) {
				// 2203:0200
				ctx->u.codeIndices[si] = cx;
				si++;
			}
		}
	}
}

// Pre: codes are sorted on code length (smallest first).
static void
Huff_sub_22334(Huff_Context *ctx) {
	// 2203:030f
	WORD dx = 0;
	WORD var_6 = 0;
	WORD prevCodeLen = 0;

	WORD codeI = ctx->codeCount;
	Huff_Code *codePtr = &ctx->codes[codeI - 1];

	while (codeI != 0) {
		// 2203:032f
		dx += var_6;
		if (codePtr->len != prevCodeLen) {
			// 2203:033d
			prevCodeLen = codePtr->len;
			var_6 = 1 << (16 - codePtr->len);
		}

		codePtr->field_0 = dx;

		codePtr--;
		codeI--;
	}
}

static void
Huff_reverseCodeBits(Huff_Context *ctx) {
	Huff_Code *codePtr = &ctx->codes[0];

	for (WORD codeI = ctx->codeCount; codeI != 0; codeI--) {
		// 2203:0376
		WORD wordToReverse = codePtr->field_0;
		WORD rightBit = 1;
		WORD leftBit = 0x8000;
		WORD reversedWord = 0;
		
		BYTE bitI = 16;
		do {
			// 2203:0390
			if (wordToReverse & rightBit)
				reversedWord |= leftBit;

			leftBit >>= 1;
			rightBit <<= 1;
			bitI--;
		} while (bitI != 0);

		codePtr->field_0 = reversedWord;
		codePtr++;
	}
}

static bool
Huff_readTable(Huff_Context *ctx) {
	// 2203:022b
	if (!haveBits(ctx->bsc, 8))
		return false;
	WORD lengthCount = getBits(ctx->bsc, 8) + 1;
			// Number of lengths for codes in this table.
	
	// 2203:025d
	WORD index = 0;
	ctx->maxCodeLen = 0;

	// 2203:026a
	while (lengthCount != 0) {
		// 2203:0271
		if (index >= 0x100 || !haveBits(ctx->bsc, 8))
			return false;
		WORD dx = getBits(ctx->bsc, 8);
		
		WORD thisCodeLenCount = highU4(dx) + 1;
				// Number of codes with this length
		WORD codeLen = lowU4(dx) + 1;

		// 2203:02af (changed order; no reason for this to be in the while
		// loop)
		if (codeLen > ctx->maxCodeLen) {
			// 2203:02b9
			ctx->maxCodeLen = codeLen;
		}

		// 2203:02ab
		// Prepare 'thisCodeLenCount' codes with the specified code length.
		while (thisCodeLenCount != 0) {
			Huff_Code *codePtr = &ctx->codes[index];
			codePtr->len = codeLen;
			codePtr->value = index;
			codePtr->field_0 = 0;

			index++;
			thisCodeLenCount--;
		};

		// 2203:02e1
		lengthCount--;
	}

	// 2203: 02e6
	if (ctx->maxCodeLen <= 8) {
		ctx->lookupCodeFunc = Huff_lookupCodeSmall;
	} else
		ctx->lookupCodeFunc = Huff_lookupCodeLarge;

	return true;
}

// Huffman decoder for codes of arbitrary length.
static bool
Huff_lookupCodeLarge(Huff_Context *ctx, DWORD *result) {
	// 2203:03f2
	Huff_Code *codePtr = &ctx->codes[0];

	// 2203:03f8
	WORD si = getBits(ctx->bsc, codePtr->len);
	BYTE ch = codePtr->len;
	BYTE cl;
	
	// 2203:0429
	while (codePtr->field_0 != si) {
		if (codePtr->field_0 < si) {
			// 2203:042f
			codePtr++;
			cl = codePtr->len - ch;
			if (cl == 0)
				continue;
		} else {
			// 2203:0459
			WORD ax = (WORD) ctx->u.codeIndices[ch];
			codePtr = &ctx->codes[ax];
			cl = codePtr->len - ch;
		}

		// 2203:043d and 2203:0479
		WORD ax = getBits(ctx->bsc, cl);

		// 2203:049cd
		si |= (ax << ch);
		ch += cl;
	}

	*result = codePtr->value;
	return true;
}

// Table-based huffman decoding.
// Code lengths are no longer than 8 bits.
// ctx->codes[]->value contains the value produced.
static bool
Huff_lookupCodeSmall(Huff_Context *ctx, DWORD *result) {
	// 2203:04b4
	WORD codeI = getBits(ctx->bsc, ctx->u.topCodeIndexLen);
	WORD haveCodeBits = ctx->u.topCodeIndexLen;

	// 2203:04f1
	Huff_Code *codePtr;
	for (;;) {
		codePtr = &ctx->codes[codeI];
		if (haveCodeBits == codePtr->len)
			break;

		WORD ax = getBit(ctx->bsc);
		
		// 2203:051c
		codeI |= (ax << haveCodeBits);
		haveCodeBits++;
	}

	*result = codePtr->value;
	return true;
}


