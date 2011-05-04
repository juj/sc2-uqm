/*
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* ABX format encoder/decoder */

#ifndef ABX_H_INCL
#define ABX_H_INCL

#include <stdint.h>
#include "port.h"

typedef struct
{
	uint16_t numFrames;  // Number of frames in this file
	uint32_t totalSize;  // Total size of uncompressed data
	uint16_t maxBufSize; // Maximum buffer needed for decoded samples
	uint16_t freq;       // Base sampling frequency
} abx_FileHeader;

typedef struct
{
	uint32_t ofs;    // File offset of frame data
	uint16_t fsize;  // Compressed frame size (in file)
	uint16_t usize;  // Uncompressed data size
} abx_FrameInfo;

typedef struct
{
	uint16_t usize;        // Uncompressed data size
	uint16_t freq;         // Sampling frequency
	uint8_t blockSize;     // Block size picked by encoder; power of 2 and minimum 8
	uint8_t minSquelch;    // INFO: Minimum repeat count for squelching
	uint16_t maxError;     // INFO: Maximum total error squared per block
} abx_FrameHeader;

#define SQLCH   0x40     // Squelch byte flag
#define RESYNC  0x80     // Resync byte flag.

#define DELTAMOD   0x30  // Delta modulation bits.
#define DELTASHIFT 4     // Delta modulation bits.

#define ONEBIT 0x10 		// One bit delta modulate
#define TWOBIT 0x20 		// Two bit delta modulate
#define FOURBIT 0x30		// four bit delta modulate

#define MULTIPLIER 0x0F  // Bottom nibble contains multiplier value.
#define SQUELCHCNT 0x3F  // Bits for squelching.

#define ABX_DEFAULT_FREQ       11025
#define ABX_DEFAULT_ERROR      32
#define ABX_DEFAULT_SQUELCH    2
#define ABX_DEFAULT_BLOCKSIZE  32

typedef struct
{
	// read-only
	int last_error;
	unsigned numFrames;
	uint16_t freq;

	// internal
	bool writing;
	FILE *fp;
	abx_FrameInfo *frames;
	unsigned nextFrame;
	unsigned maxFrames;
	uint32_t frames_ofs;
	uint32_t data_ofs;
	uint32_t totalSize;
	int maxError;
	unsigned maxBufSize;
	unsigned maxEncSize;
	uint8_t *buf;
} abx_File;

bool abx_open(abx_File *abx, const char *filename);
bool abx_create(abx_File *abx, const char *filename);
void abx_close(abx_File *abx);
bool abx_setSamplingRate(abx_File *abx, uint32_t freq);
bool abx_setMaxFrames(abx_File *abx, unsigned maxFrames);
uint32_t abx_getMaxBuffer(abx_File *abx);

uint32_t abx_readFrame(abx_File *abx, void *buf, uint32_t bufsize);
uint32_t abx_writeFrame(abx_File *abx, void *buf, uint32_t bufsize);

#endif /* ABX_H_INCL */
