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

/* AIFF format decoder
 * By Serge van den Boom (svdb@stack.nl) 20020816,
 * modularization by Alex Volkov (codepro@usa.net)
 * 
 * Doesn't convert all .aif files in general, only 8- and 16-bit PCM and
 * AIFF-C 16-bit SDX2-compressed.
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <errno.h>
#include <assert.h>

#include "aiff.h"

static int aiff_decodePCM(aiff_File*, void *buf, uint32_t bufsize);
static int aiff_decodeSDX2(aiff_File*, void *buf, uint32_t bufsize);


static bool read_be_16 (FILE *fp, uint16_t *v)
{
	uint8_t buf[2];
	if (fread(buf, sizeof(buf), 1, fp) != 1)
		return false;
	*v = (buf[0] << 8) | buf[1];
	return true;
}

static bool read_be_32 (FILE *fp, uint32_t *v)
{
	uint8_t buf[4];
	if (fread(buf, sizeof(buf), 1, fp) != 1)
		return false;
	*v = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
	return true;
}

// Read 80-bit IEEE 754 floating point number.
// We are only interested in values that we can work with,
//   so using an sint32 here is fine.
static bool read_be_f80(FILE *fp, int32_t *v)
{
	int sign, exp;
	int shift;
	uint16_t se;
	uint32_t mant, mant_low;
	if (!read_be_16(fp, &se) ||
		!read_be_32(fp, &mant) || !read_be_32(fp, &mant_low))
		return false;

	sign = (se >> 15) & 1;        // sign is the highest bit
	exp = (se & ((1 << 15) - 1)); // exponent is next highest 15 bits
#if 0 // XXX: 80bit IEEE 754 used in AIFF uses explicit mantissa MS bit
	// mantissa has an implied leading bit which is typically 1
	mant >>= 1;
	if (exp != 0)
		mant |= 0x80000000;
#endif
	mant >>= 1;            // we also need space for sign
	exp -= (1 << 14) - 1;  // exponent is biased by (2^(e-1) - 1)
	shift = exp - 31 + 1;  // mantissa is already 31 bits before decimal pt.
	if (shift > 0)
		mant = 0x7fffffff; // already too big
	else if (shift < 0)
		mant >>= -shift;

	*v = sign ? -(int32_t)mant : (int32_t)mant;

	return true;
}

static bool aiff_readFileHeader(aiff_File *aiff, aiff_FileHeader *hdr)
{
	if (!read_be_32(aiff->fp, &hdr->chunk.id) ||
			!read_be_32(aiff->fp, &hdr->chunk.size) ||
			!read_be_32(aiff->fp, &hdr->type))
	{
		aiff->last_error = errno;
		return false;
	}
	return true;
}

static bool	aiff_readChunkHeader(aiff_File *aiff, aiff_ChunkHeader *hdr)
{
	if (!read_be_32(aiff->fp, &hdr->id) ||
			!read_be_32(aiff->fp, &hdr->size))
	{
		aiff->last_error = errno;
		return false;
	}
	return true;
}

static int aiff_readCommonChunk(aiff_File *aiff, uint32_t size, aiff_ExtCommonChunk *fmt)
{
	int bytes;

	memset(fmt, sizeof(*fmt), 0);
	if (size < AIFF_COMM_SIZE)
	{
		aiff->last_error = EIO;
		return 0;
	}

	if (!read_be_16(aiff->fp, &fmt->channels) ||
			!read_be_32(aiff->fp, &fmt->sampleFrames) ||
			!read_be_16(aiff->fp, &fmt->sampleSize) ||
			!read_be_f80(aiff->fp, &fmt->sampleRate))
	{
		aiff->last_error = errno;
		return 0;
	}
	bytes = AIFF_COMM_SIZE;

	if (size >= AIFF_EXT_COMM_SIZE)
	{
		if (!read_be_32(aiff->fp, &fmt->extTypeID))
		{
			aiff->last_error = errno;
			return 0;
		}
		bytes += sizeof(fmt->extTypeID);
	}
	
	return bytes;
}

static bool aiff_readSoundDataChunk(aiff_File *aiff, aiff_SoundDataChunk *data)
{
	if (!read_be_32(aiff->fp, &data->offset) ||
			!read_be_32(aiff->fp, &data->blockSize))
	{
		aiff->last_error = errno;
		return false;
	}
	return true;
}

bool aiff_open(aiff_File *aiff, const char *filename)
{
	aiff_FileHeader fileHdr;
	aiff_ChunkHeader chunkHdr;
	uint32_t sdata_size = 0;
	long remSize;

	aiff->fp = fopen(filename, "rb");
	if (!aiff->fp)
	{
		aiff->last_error = errno;
		return false;
	}

	aiff->data_size = 0;
	aiff->max_pcm = 0;
	aiff->data_ofs = 0;
	memset(&aiff->fmtHdr, 0, sizeof(aiff->fmtHdr));
	memset(aiff->prev_val, sizeof(aiff->prev_val), 0);

	// read the header
	if (!aiff_readFileHeader(aiff, &fileHdr))
	{
		aiff->last_error = errno;
		aiff_close(aiff);
		return false;
	}
	if (fileHdr.chunk.id != aiff_FormID)
	{
		fprintf(stderr, "aiff_open(): not an aiff file, ID 0x%08x",
				(unsigned)fileHdr.chunk.id);
		aiff_close(aiff);
		return false;
	}
	if (fileHdr.type != aiff_FormTypeAIFF && fileHdr.type != aiff_FormTypeAIFC)
	{
		fprintf(stderr, "aiff_open(): unsupported aiff file, Type 0x%08x",
				(unsigned)fileHdr.type);
		aiff_close(aiff);
		return false;
	}

	for (remSize = fileHdr.chunk.size - sizeof(aiff_ID); remSize > 0;
			remSize -= ((chunkHdr.size + 1) & ~1) + AIFF_CHUNK_HDR_SIZE)
	{
		if (!aiff_readChunkHeader(aiff, &chunkHdr))
		{
			aiff_close(aiff);
			return false;
		}

		if (chunkHdr.id == aiff_CommonID)
		{
			int read = aiff_readCommonChunk(aiff, chunkHdr.size, &aiff->fmtHdr);
			if (!read)
			{
				aiff_close(aiff);
				return false;
			}
			fseek(aiff->fp, chunkHdr.size - read, SEEK_CUR);
		}
		else if (chunkHdr.id == aiff_SoundDataID)
		{
			aiff_SoundDataChunk data;
			if (!aiff_readSoundDataChunk(aiff, &data))
			{
				aiff_close(aiff);
				return false;
			}
			sdata_size = chunkHdr.size - AIFF_SSND_SIZE - data.offset;
			aiff->data_ofs = ftell(aiff->fp) + data.offset;
			fseek(aiff->fp, chunkHdr.size - AIFF_SSND_SIZE, SEEK_CUR);
		}
		else
		{	// skip uninteresting chunk
			fseek(aiff->fp, chunkHdr.size, SEEK_CUR);
		}

		// 2-align the file ptr
		fseek(aiff->fp, chunkHdr.size & 1, SEEK_CUR);
	}

	if (aiff->fmtHdr.sampleFrames == 0)
	{
		fprintf(stderr, "aiff_open(): aiff file has no sound data");
		aiff_close(aiff);
		return false;
	}
	
	// make bits-per-sample a multiple of 8
	aiff->bits_per_sample = (aiff->fmtHdr.sampleSize + 7) & ~7;
	if (aiff->bits_per_sample == 0 || aiff->bits_per_sample > 16)
	{	// XXX: for now we do not support 24 and 32 bps
		fprintf(stderr, "aiff_open(): unsupported sample size %u",
				aiff->bits_per_sample);
		aiff_close(aiff);
		return false;
	}
	if (aiff->fmtHdr.sampleRate == 0 || aiff->fmtHdr.sampleRate > 480000)
	{
		fprintf(stderr, "aiff_open(): unsupported sampling rate %ld",
				(long)aiff->fmtHdr.sampleRate);
		aiff_close(aiff);
		return false;
	}

	aiff->block_align = aiff->bits_per_sample / 8 * aiff->fmtHdr.channels;
	aiff->file_block = aiff->block_align;
	if (!aiff->data_ofs)
	{
		fprintf(stderr, "aiff_open(): bad aiff file, no SSND chunk found");
		aiff_close(aiff);
		return false;
	}

	if (fileHdr.type == aiff_FormTypeAIFF)
	{
		if (aiff->fmtHdr.extTypeID != 0)
		{
			fprintf(stderr, "aiff_open(): unsupported extension 0x%08x",
					(unsigned)aiff->fmtHdr.extTypeID);
			aiff_close(aiff);
			return false;
		}
		aiff->compType = aifc_None;
	}
	else if (fileHdr.type == aiff_FormTypeAIFC)
	{
		if (aiff->fmtHdr.extTypeID != aiff_CompressionTypeSDX2)
		{
			fprintf(stderr, "aiff_open(): unsupported compression 0x%08x",
					(unsigned)aiff->fmtHdr.extTypeID);
			aiff_close(aiff);
			return false;
		}
		aiff->compType = aifc_Sdx2;
		aiff->file_block /= 2;

		if (aiff->fmtHdr.channels > MAX_CHANNELS)
		{
			fprintf(stderr, "aiff_open(): number of channels (%u) too large",
					(unsigned)aiff->fmtHdr.channels);
			aiff_close(aiff);
			return false;
		}
	}

	aiff->data_size = aiff->fmtHdr.sampleFrames * aiff->file_block;
	if (sdata_size < aiff->data_size)
	{
		fprintf(stderr, "aiff_open(): sound data size %u is less than "
				"computed %u\n",
				(unsigned)sdata_size, (unsigned)aiff->data_size);
		aiff->fmtHdr.sampleFrames = sdata_size / aiff->file_block;
		aiff->data_size = aiff->fmtHdr.sampleFrames * aiff->file_block;
	}

	if (aiff->compType == aifc_Sdx2 && aiff->bits_per_sample != 16)
	{
		fprintf(stderr, "aiff_open(): unsupported sample size %u for SDX2",
				(unsigned)aiff->fmtHdr.sampleSize);
		aiff_close(aiff);
		return false;
	}

	fseek(aiff->fp, aiff->data_ofs, SEEK_SET);
	aiff->max_pcm = aiff->fmtHdr.sampleFrames;
	aiff->cur_pcm = 0;
	aiff->last_error = 0;

	return true;
}

void aiff_close(aiff_File *aiff)
{
	if (aiff->fp)
	{
		fclose (aiff->fp);
		aiff->fp = NULL;
	}
}

int aiff_readData(aiff_File *aiff, void *buf, uint32_t bufsize)
{
	switch (aiff->compType)
	{
	case aifc_None:
		return aiff_decodePCM(aiff, buf, bufsize);
	case aifc_Sdx2:
		return aiff_decodeSDX2(aiff, buf, bufsize);
	default:
		assert(false && "Unknown compession type");
		return 0;
	}
}

static int aiff_decodePCM(aiff_File *aiff, void *buf, uint32_t bufsize)
{
	uint32_t dec_pcm;
	uint32_t size;

	dec_pcm = bufsize / aiff->block_align;
	if (dec_pcm > aiff->max_pcm - aiff->cur_pcm)
		dec_pcm = aiff->max_pcm - aiff->cur_pcm;

	dec_pcm = fread(buf, aiff->file_block, dec_pcm, aiff->fp);
	aiff->cur_pcm += dec_pcm;
	size = dec_pcm * aiff->block_align;

	if (aiff->bits_per_sample == 8)
	{	// AIFF files store 8-bit data as signed
		// and we need it unsigned
		uint8_t* ptr = (uint8_t*)buf;
		uint32_t left;
		for (left = size; left > 0; --left, ++ptr)
			*ptr ^= 0x80;
	}
	else if (aiff->bits_per_sample == 16)
	{	// AIFF files store 16-bit data big-endian
		// and we need it in machine order
		uint8_t* ptr = (uint8_t*)buf;
		uint32_t left;
		for (left = size / 2; left > 0; --left, ptr += 2)
			*(int16_t*)ptr = (ptr[0] << 8) | ptr[1];
	}

	return size;
}

static int aiff_decodeSDX2(aiff_File *aiff, void* buf, uint32_t bufsize)
{
	uint32_t dec_pcm;
	int8_t *src;
	int16_t *dst = buf;
	uint32_t left;
	uint32_t size;

	dec_pcm = bufsize / aiff->block_align;
	if (dec_pcm > aiff->max_pcm - aiff->cur_pcm)
		dec_pcm = aiff->max_pcm - aiff->cur_pcm;

	src = (int8_t*)buf + bufsize - (dec_pcm * aiff->file_block);
	dec_pcm = fread(src, aiff->file_block, dec_pcm, aiff->fp);
	aiff->cur_pcm += dec_pcm;
	size = dec_pcm * aiff->block_align;

	for (left = dec_pcm; left > 0; --left)
	{
		int i;
		int32_t *prev = aiff->prev_val;
		for (i = aiff->fmtHdr.channels; i > 0; --i, ++prev, ++src, ++dst)
		{
			int32_t v = (*src * abs(*src)) << 1;
			if (*src & 1)
				v += *prev;
			// Saturate the value. This is just a safety measure, as it
			// should never be necessary in SDX2.
			if (v > 32767)
				v = 32767;
			else if (v < -32768)
				v = -32768;
			*prev = v;
			*dst = v;
		}
	}

	return size;
}
