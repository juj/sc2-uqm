// Reverse engineered from the Star Control executable.

#include "lztfb.h"

#include "../../shared/cbytesex.h"
#include "../../shared/util.h"
#include "getbit.h"

#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool LZTFB_processMore(LZTFB *lztfb);


LZTFB *
LZTFB_new(const char *buf, size_t len) {
	LZTFB *result = NULL;

	if (len < 6)
		goto err;

	if (buf[0] != 0x06) {
		// Not a compressed file.
		goto err;
	}
	
	result = malloc(sizeof (LZTFB));
	result->huffTable[0] = NULL;
	result->huffTable[1] = NULL;
	result->huffTable[2] = NULL;

	result->state = 0;
	result->stateData.offset = 0;
	result->stateData.count = 0;

	result->compressedSize = len;
	result->uncompressedSize = getU32BE(buf + 2);

	if ((buf[1] & 2) != 0) {
		result->table2Shift = 7;
	} else
		result->table2Shift = 6;

	BSC_init(&result->bsc, buf + 6, len);

	if ((buf[1] & 4) != 0) {
		// Literal bytes are huffman encoded.
		result->lengthBias = 3;
		result->literalsEncoded = true;
	} else {
		// Literal bytes are read directly from the input.
		result->lengthBias = 2;
		result->literalsEncoded = false;
		result->huffTable[0] = NULL;
	}

	memset(result->buf, '\0', LZTFB_BUF_SIZE);
	result->bufPtr = result->buf;
	result->bufFill = 0;
	result->lastBufFill = 0;
	
	return result;
err:
	if (result != NULL)
		LZTFB_delete(result);
	return NULL;
}

void
LZTFB_delete(LZTFB *lztfb) {
	for (int tableI = 0; tableI < 3; tableI++) {
		if (lztfb->huffTable[0] != NULL)
			Huff_delete(lztfb->huffTable[0]);
	}

	free(lztfb);
}

int
LZTFB_output(LZTFB *lztfb, FILE *out) {
	// 21e0:00aa
	size_t toOutput = lztfb->uncompressedSize;

	while (toOutput > 0) {
		lztfb->bufPtr = lztfb->buf;
		lztfb->bufFill = 0;

		if (!LZTFB_processMore(lztfb)) {
			logError(false, "LZTFB_processMore failed.\n");
			errno = EIO;
			return -1;
		}
		lztfb->lastBufFill = lztfb->bufFill;

		size_t toWrite = lztfb->bufFill;
		if (toWrite > toOutput)
			toWrite = toOutput;
		size_t written = fwrite(lztfb->buf, 1, toWrite, out);
		if (written != toWrite) {
			assert(ferror(out));
			logError(true, "fwrite() failed.\n");
			return -1;
		}
		toOutput -= written;
	}

	return 0;
}

// Memcopy from left to right. (memcpy doesn't guarantee this, and memmove
// does it differently)
static inline void
memcpyLtr(void *dest, void *src, size_t size) {
	while (size--) {
		*((uint8_t *) dest) = *((uint8_t *) src);
		src = ((uint8_t *) src) + 1;
		dest = ((uint8_t *) dest) + 1;
	}
}

static bool
LZTFB_processMore(LZTFB *lztfb) {
	// 2203:0555
	int32_t offset = lztfb->stateData.offset;
	uint32_t count = lztfb->stateData.count;
	lztfb->stateData.count = 0;
	uint8_t table2Shift = lztfb->table2Shift;
	uint8_t lengthBias = lztfb->lengthBias;

	if (lztfb->state == 0) {
		if (lztfb->literalsEncoded) {
			lztfb->huffTable[0] = Huff_new(&lztfb->bsc, 0x100);
			if (lztfb->huffTable[0] == NULL)
				goto err;
		}

		lztfb->huffTable[1] = Huff_new(&lztfb->bsc, 0x40);
		if (lztfb->huffTable[1] == NULL)
			goto err;

		lztfb->huffTable[2] = Huff_new(&lztfb->bsc, 0x40);
		if (lztfb->huffTable[2] == NULL)
			goto err;

		goto state1;
	}

	if (lztfb->state == 1) {
state1:
		// State 1: read more data and decide what to do.

		// The original code flagged EOF when it needed to read a new
		// byte, and this wasn't possible. But while EOF it would
		// still return undefined bytes.
		// Because there may be a couple of bits left at the end of the
		// stream (as the stream contains bytes) we can't know whether
		// the end of the stream has been reached as long as we still
		// have a couple of bits left. So we handle EOF the same way.
		if (lztfb->bsc.eof)
			return true;

		int bit = getBit(&lztfb->bsc);
		if (bit != 0) {
			// Literal byte.
			if (lztfb->huffTable[0] == NULL) {
				// Literal byte is read directly from the input.
				*lztfb->bufPtr = getBits(&lztfb->bsc, 8);
			} else {
				// Literal byte is Huffman-encoded.
				uint32_t dummy;
				if (!Huff_getCode(lztfb->huffTable[0], &dummy))
					goto err;
				*lztfb->bufPtr = (uint8_t) dummy;
			}

			lztfb->bufPtr++;
			lztfb->bufFill++;
			if (lztfb->bufFill == LZTFB_BUF_SIZE) {
				lztfb->state = 1;
				return true;
			}
			goto state1;
		}
		
		offset = getBits(&lztfb->bsc, table2Shift);
		
		uint32_t ax;
		if (!Huff_getCode(lztfb->huffTable[2], &ax))
			goto err;
		ax = (ax << table2Shift) | offset;
		offset = lztfb->lastBufFill + lztfb->bufFill - ax;
				// This looks like a bug; if lztfb->bufFill !=
				// LZTFB_BUF_SIZE, the offset is wrong.

		if (!Huff_getCode(lztfb->huffTable[1], &ax))
			goto err;
		ax += lengthBias;
		count = ax;
		
		if (ax == lengthBias + 0x3fU) {
			// 6 bits was not enough; read 8 more.
			count += getBits(&lztfb->bsc, 8);
		}

		offset--;
		if (offset < 0) {
			goto state2;
		} else
			goto state3;
	}

	if (lztfb->state == 2) {
state2:
		// State 2: repeat '\0' a number of times.
		// offset == -repeatCount

		// 2203:0588
		// Never more than count bytes.
		if (offset < (int32_t) -count)
			offset = -count;
					// This means count will become 0 in the next line.
		
		count += offset;
				// offset is negative; count gets smaller

		do {
			uint32_t repeatCount = -offset;
			if (repeatCount + lztfb->bufFill <= LZTFB_BUF_SIZE) {
				// Enough room for repeatCount more characters.
				memset(lztfb->bufPtr, '\0', repeatCount);
				lztfb->bufFill += repeatCount;
				lztfb->bufPtr += repeatCount;
				offset = 0;
				break;
			}
		
			lztfb->stateData.count = repeatCount -
					(LZTFB_BUF_SIZE - lztfb->bufFill);
			offset += (LZTFB_BUF_SIZE - lztfb->bufFill);
					// offset was negative; it now contains the negation of
					// the number of bytes that still fit in the buffer.
		} while (offset != 0);

		if (lztfb->bufFill != LZTFB_BUF_SIZE) {
			if (count == 0)
				goto state1;

			// count bytes will be copied from the start of the buffer.
			// I don't get it.
			goto state3;
		}
		// Buffer is full

		if (count != 0 || lztfb->stateData.count != 0) {
			lztfb->stateData.offset = -lztfb->stateData.count;
			lztfb->stateData.count += count;
			lztfb->state = 2;
					// Next time, continue where we left off.
			return true;
		}

		lztfb->state = 1;
				// Next time, start by reading more data.
		return true;
	}

state3:
	// State 3: Backreference to an earlier piece of data
	// offset is the offset of the earlier data
	// count is the length.

	offset &= LZTFB_BUF_SIZE - 1;
			// offset %= LZTFB_BUF_SIZE

	for (;;) {
		if (count + lztfb->bufFill > LZTFB_BUF_SIZE) {
			// There's no room in the buffer for this many bytes. Adjust
			// the size, and save the rest for later.

			lztfb->stateData.count =
					count - (LZTFB_BUF_SIZE - lztfb->bufFill);
			count = LZTFB_BUF_SIZE - lztfb->bufFill;
			if (count == 0) {
				// Buffer was completely full.
				break;
			}
		}

		if (count + offset <= LZTFB_BUF_SIZE) {
			// All the data that is referenced comes from the previous
			// call to this function.
			// Copy lztfb[offset..(offset + count)] to bufPtr.
			memcpyLtr(lztfb->bufPtr, &lztfb->buf[offset], count);
			lztfb->bufPtr += count;
			lztfb->bufFill += count;
			break;
		}
		
		// Copy lztfb[offset..] to bufPtr. The rest of the data to be copied
		// comes from the data we wrote this call.
		memcpyLtr(lztfb->bufPtr, &lztfb->buf[offset],
				LZTFB_BUF_SIZE - offset);
		lztfb->bufPtr += LZTFB_BUF_SIZE - offset;
		lztfb->bufFill += LZTFB_BUF_SIZE - offset;
		count -= LZTFB_BUF_SIZE - offset;
		offset = 0;
	}
	// count contains the number of bytes written since offset was last
	// adjusted.

	if (lztfb->bufFill != LZTFB_BUF_SIZE)
		goto state1;

	if (lztfb->stateData.count == 0) {
		lztfb->state = 1;
				// Next time, start by reading new data.
		return true;
	} else {
		lztfb->stateData.offset = offset + count;
				// offset was not yet adjusted with count after the last write
		lztfb->state = 3;
				// Next time, continue where we left off.
		return true;
	}

err:
	return false;
}




