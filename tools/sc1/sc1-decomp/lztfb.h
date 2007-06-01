// Reverse engineered from the Star Control executable.

#ifndef _LZTFB
#define _LZTFB

#include <stdint.h>
#include <stdio.h>

typedef struct LZTFB LZTFB;

#include "../../shared/cbytesex.h"
#include "huff.h"

#define LZTFB_BUF_SIZE  0x2000

struct LZTFB {
	uint8_t buf[LZTFB_BUF_SIZE];
	uint8_t *bufPtr;
	uint32_t bufFill;
	uint32_t lastBufFill;

	BitStreamContext bsc;

	bool literalsEncoded;
	Huff_Context *huffTable[3];
	int state;
	struct {
		int32_t offset;
		uint32_t count;
	} stateData;

	size_t compressedSize;
	size_t uncompressedSize;

	uint8_t table2Shift;
	uint8_t lengthBias;
};

LZTFB *LZTFB_new(const char *buf, size_t len);
int LZTFB_output(LZTFB *lztfb, FILE *out);
void LZTFB_delete(LZTFB *lztfb);

#endif  /* _LZTFB */

