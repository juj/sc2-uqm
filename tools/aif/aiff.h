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

/* AIFF format decoder */

#ifndef AIFF_H_INCL
#define AIFF_H_INCL

#include <stdint.h>
#include "port.h"

typedef uint32_t aiff_ID;

#define aiff_MAKE_ID(x1, x2, x3, x4) \
		(((x1) << 24) | ((x2) << 16) | ((x3) << 8) | (x4))

#define aiff_FormID         aiff_MAKE_ID('F', 'O', 'R', 'M')
#define aiff_FormVersionID  aiff_MAKE_ID('F', 'V', 'E', 'R')
#define aiff_CommonID       aiff_MAKE_ID('C', 'O', 'M', 'M')
#define aiff_SoundDataID    aiff_MAKE_ID('S', 'S', 'N', 'D')

#define aiff_FormTypeAIFF   aiff_MAKE_ID('A', 'I', 'F', 'F')
#define aiff_FormTypeAIFC   aiff_MAKE_ID('A', 'I', 'F', 'C')

#define aiff_CompressionTypeSDX2  aiff_MAKE_ID('S', 'D', 'X', '2')


typedef struct
{
	aiff_ID id;       /* Chunk ID */
	uint32_t size;    /* Chunk size, excluding header */
} aiff_ChunkHeader;

#define AIFF_CHUNK_HDR_SIZE (4+4)

typedef struct
{
	aiff_ChunkHeader chunk;
	aiff_ID type;
} aiff_FileHeader;

typedef struct
{
	uint32_t version;  /* format version, in Mac format */
} aiff_FormatVersionChunk;

typedef struct
{
	uint16_t channels;       /* number of channels */
	uint32_t sampleFrames;   /* number of sample frames */
	uint16_t sampleSize;     /* number of bits per sample */
	int32_t sampleRate;      /* number of frames per second */
			/* this is actually stored as IEEE-754 80bit in files */
} aiff_CommonChunk;

#define AIFF_COMM_SIZE (2+4+2+10)

typedef struct
{
	uint16_t channels;      /* number of channels */
	uint32_t sampleFrames;  /* number of sample frames */
	uint16_t sampleSize;    /* number of bits per sample */
	int32_t sampleRate;     /* number of frames per second */
	aiff_ID extTypeID;      /* compression type ID */
	char extName[32];       /* compression type name */
} aiff_ExtCommonChunk;

#define AIFF_EXT_COMM_SIZE (AIFF_COMM_SIZE+4)

typedef struct
{
	uint32_t offset;     /* offset to sound data */
	uint32_t blockSize;  /* size of alignment blocks */
} aiff_SoundDataChunk;

#define AIFF_SSND_SIZE (4+4)

typedef enum
{
	aifc_None,
	aifc_Sdx2,
} aiff_CompressionType;

#define MAX_CHANNELS 16

typedef struct
{
	// read-only
	int last_error;
	aiff_ExtCommonChunk fmtHdr;
	aiff_CompressionType compType;

	// internal
	FILE *fp;
	unsigned bits_per_sample;
	unsigned block_align;
	unsigned file_block;
	uint32_t data_ofs;
	uint32_t data_size;
	uint32_t max_pcm;
	uint32_t cur_pcm;
	int32_t prev_val[MAX_CHANNELS];
} aiff_File;

bool aiff_open(aiff_File*, const char *filename);
void aiff_close(aiff_File*);
int aiff_readData(aiff_File*, void *buf, uint32_t bufsize);

#endif /* AIFF_H_INCL */
