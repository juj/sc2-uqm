// Reverse engineered from the Star Control executable.

#ifndef _HUFF_H
#define _HUFF_H

typedef struct Huff_Code Huff_Code;
typedef struct Huff_Context Huff_Context;

#include "dostypes.h"
#include "../../shared/cbytesex.h"


struct Huff_Code {
	WORD field_0;
	BYTE value;
	BYTE len;
};

struct Huff_Context {
	Huff_Code codes[0x100];
	WORD codeCount;
	WORD maxCodeLen;
	bool (*lookupCodeFunc)(Huff_Context *ctx, DWORD *result);

	BitStreamContext *bsc;

	union {
		BYTE topCodeIndexLen;
				// Number of bits in the index of the top code.
		BYTE codeIndices[16];
				// Index into 'codes' to the first code with some length.
	} u;
};

Huff_Context *Huff_new(BitStreamContext *bsc, WORD codeCount);
void Huff_delete(Huff_Context *ctx);

static inline bool
Huff_getCode(Huff_Context *ctx, DWORD *result) {
	return ctx->lookupCodeFunc(ctx, result);
}

#endif  /* _HUFF_H */

