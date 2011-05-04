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

/*
 * ABX encoder/decoder
 * By Serge van den Boom (svdb@stack.nl) and Alex Volkov (codepro@usa.net)
 * Based on ABX decoding code from Toys for Bob. 
 *
 * TODO:
 * - so far, it ignores sample rates, so it will work ok as long as all
 * the frames have the same frequency. This is probably enough for
 * our purposes.
 *
 * - add abx_setMaxError(), abx_setMinSquelch() and abx_setBlockSize() for
 * the encoder parameters, if anyone cares that is. The 3DO abx files all
 * used the same params, as far as I know.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <memory.h>
#include <errno.h>

#include "abx.h"

// This number can be increased to almost anything, as long
// as you have enough memory to store the data. It's kept
// on the low end to improve the sanity checks.
#define MAX_REASONABLE_FRAMES  100000

#define abx_FrameInfo_size    8
#define abx_FrameHeader_size  8

static uint32_t abx_decodeFrame(abx_File *abx, const abx_FrameHeader *hdr,
		int inlen, uint8_t *out);
static uint32_t abx_encodeFrame(abx_File *abx, abx_FrameHeader *hdr,
		uint8_t *in, int inlen);

// The deltas table came from TFB
static const int deltas[16 * 16] =
{
    -8,-7,-6,-5,-4,-3,-2,-1,1,2,3,4,5,6,7,8,                // Multiplier of 1
    -16,-14,-12,-10,-8,-6,-4,-2,2,4,6,8,10,12,14,16,        // Multiplier of 2
    -24,-21,-18,-15,-12,-9,-6,-3,3,6,9,12,15,18,21,24,      // Multiplier of 3
    -32,-28,-24,-20,-16,-12,-8,-4,4,8,12,16,20,24,28,32,    // Multiplier of 4
    -40,-35,-30,-25,-20,-15,-10,-5,5,10,15,20,25,30,35,40,  // Multiplier of 5
    -48,-42,-36,-30,-24,-18,-12,-6,6,12,18,24,30,36,42,48,  // Multiplier of 6
    -56,-49,-42,-35,-28,-21,-14,-7,7,14,21,28,35,42,49,56,  // Multiplier of 7
    -64,-56,-48,-40,-32,-24,-16,-8,8,16,24,32,40,48,56,64,  // Multiplier of 8
    -72,-63,-54,-45,-36,-27,-18,-9,9,18,27,36,45,54,63,72,  // Multiplier of 9
    -80,-70,-60,-50,-40,-30,-20,-10,10,20,30,40,50,60,70,80,  // Multiplier of 10
    -88,-77,-66,-55,-44,-33,-22,-11,11,22,33,44,55,66,77,88,  // Multiplier of 11
    -96,-84,-72,-60,-48,-36,-24,-12,12,24,36,48,60,72,84,96,  // Multiplier of 12
    -104,-91,-78,-65,-52,-39,-26,-13,13,26,39,52,65,78,91,104,  // Multiplier of 13
    -112,-98,-84,-70,-56,-42,-28,-14,14,28,42,56,70,84,98,112,  // Multiplier of 14
    -120,-105,-90,-75,-60,-45,-30,-15,15,30,45,60,75,90,105,120,// Multiplier of 15
    -128,-112,-96,-80,-64,-48,-32,-16,16,32,48,64,80,96,112,127,// Multiplier of 16
};

static bool read_8 (FILE *fp, uint8_t *v)
{
	return fread(v, sizeof(*v), 1, fp) == 1;
}

static bool read_le_16 (FILE *fp, uint16_t *v)
{
	uint8_t buf[2];
	if (fread(buf, sizeof(buf), 1, fp) != 1)
		return false;
	*v = (buf[1] << 8) | buf[0];
	return true;
}

static bool read_le_32 (FILE *fp, uint32_t *v)
{
	uint8_t buf[4];
	if (fread(buf, sizeof(buf), 1, fp) != 1)
		return false;
	*v = (buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | buf[0];
	return true;
}

static bool write_8 (FILE *fp, uint8_t v)
{
	return fwrite(&v, sizeof(v), 1, fp) == 1;
}

static bool write_le_16 (FILE *fp, uint16_t v)
{
	uint8_t buf[2];
	buf[0] = v;
	buf[1] = v >> 8;
	return fwrite(buf, sizeof(buf), 1, fp) == 1;
}

static bool write_le_32 (FILE *fp, uint32_t v)
{
	uint8_t buf[4];
	buf[0] = v;
	buf[1] = v >> 8;
	buf[2] = v >> 16;
	buf[3] = v >> 24;
	return fwrite(buf, sizeof(buf), 1, fp) == 1;
}

static bool abx_readFileHeader(abx_File *abx, abx_FileHeader *hdr)
{
	if (!read_le_16(abx->fp, &hdr->numFrames) ||
			!read_le_32(abx->fp, &hdr->totalSize) ||
			!read_le_16(abx->fp, &hdr->maxBufSize) ||
			!read_le_16(abx->fp, &hdr->freq))
	{
		abx->last_error = errno;
		return false;
	}
	return true;
}

static bool abx_writeFileHeader(abx_File *abx, const abx_FileHeader *hdr)
{
	if (!write_le_16(abx->fp, hdr->numFrames) ||
			!write_le_32(abx->fp, hdr->totalSize) ||
			!write_le_16(abx->fp, hdr->maxBufSize) ||
			!write_le_16(abx->fp, hdr->freq))
	{
		abx->last_error = errno;
		return false;
	}
	return true;
}

static bool abx_readFrameInfo(abx_File *abx, abx_FrameInfo *info)
{
	if (!read_le_32(abx->fp, &info->ofs) ||
			!read_le_16(abx->fp, &info->fsize) ||
			!read_le_16(abx->fp, &info->usize))
	{
		abx->last_error = errno;
		return false;
	}
	return true;
}

static bool abx_writeFrameInfo(abx_File *abx, const abx_FrameInfo *info)
{
	if (!write_le_32(abx->fp, info->ofs) ||
			!write_le_16(abx->fp, info->fsize) ||
			!write_le_16(abx->fp, info->usize))
	{
		abx->last_error = errno;
		return false;
	}
	return true;
}

static bool abx_readFrameHeader(abx_File *abx, abx_FrameHeader *hdr)
{
	if (!read_le_16(abx->fp, &hdr->usize) ||
			!read_le_16(abx->fp, &hdr->freq) ||
			!read_8(abx->fp, &hdr->blockSize) ||
			!read_8(abx->fp, &hdr->minSquelch) ||
			!read_le_16(abx->fp, &hdr->maxError))
	{
		abx->last_error = errno;
		return false;
	}
	return true;
}

static bool abx_writeFrameHeader(abx_File *abx, const abx_FrameHeader *hdr)
{
	if (!write_le_16(abx->fp, hdr->usize) ||
			!write_le_16(abx->fp, hdr->freq) ||
			!write_8(abx->fp, hdr->blockSize) ||
			!write_8(abx->fp, hdr->minSquelch) ||
			!write_le_16(abx->fp, hdr->maxError))
	{
		abx->last_error = errno;
		return false;
	}
	return true;
}

bool abx_open(abx_File *abx, const char *filename)
{
	abx_FileHeader fileHdr;
	unsigned i;
	unsigned maxCalcBuf;

	memset(abx, 0, sizeof(*abx));

	abx->fp = fopen(filename, "rb");
	if (!abx->fp)
	{
		abx->last_error = errno;
		return false;
	}

	// read abx header
	if (!abx_readFileHeader(abx, &fileHdr))
	{
		abx->last_error = errno;
		abx_close(abx);
		return false;
	}
	abx->numFrames = fileHdr.numFrames;
	abx->maxBufSize = fileHdr.maxBufSize;
	abx->freq = fileHdr.freq;
	if (abx->freq == 0)
		abx->freq = ABX_DEFAULT_FREQ;

	// Some sanity checks. ABX format does not have a magic number
	//    or anything like that, but we can do some math.
	if (abx->numFrames > MAX_REASONABLE_FRAMES)
	{
		abx->last_error = -1;
		fprintf(stderr, "abx_open(): number of frames (%u) is not reasonable\n",
				abx->numFrames);
		abx_close(abx);
		return false;
	}
	if (abx->freq != 11025 && abx->freq != 22050 && abx->freq != 44100
			&& abx->freq != 48000)
	{
		fprintf(stderr, "abx_open() Warning: sampling frequency (%u) is suspect\n",
				(unsigned)abx->freq);
	}

	abx->frames = calloc(sizeof(abx->frames[0]), abx->numFrames);
	if (!abx->frames)
	{
		abx->last_error = errno;
		fprintf(stderr, "abx_open(): could not allocate frames array\n");
		abx_close(abx);
		return false;
	}

	maxCalcBuf = 0;
	for (i = 0; i < abx->numFrames; ++i)
	{
		abx_FrameInfo *info = abx->frames + i;

		if (!abx_readFrameInfo(abx, info))
		{
			abx_close(abx);
			return false;
		}
		abx->totalSize += info->usize;
		if (info->usize > maxCalcBuf)
			maxCalcBuf = info->usize;
		if (info->fsize > abx->maxEncSize)
			abx->maxEncSize = info->fsize;
	}
	if (abx->totalSize != fileHdr.totalSize)
	{
		fprintf(stderr, "abx_open() Warning: "
				"total size in header (%u) does not match sum of frames (%u)\n",
				(unsigned)fileHdr.totalSize, (unsigned)abx->totalSize);
	}
	if (abx->maxBufSize < maxCalcBuf)
	{
		fprintf(stderr, "abx_open() Warning: "
				"max buffer size in header (%u) is less than calculated max (%u)\n",
				abx->maxBufSize, maxCalcBuf);
		abx->maxBufSize = maxCalcBuf;
	}
	abx->data_ofs = ftell(abx->fp);
	abx->maxFrames = abx->numFrames;

	// Our buffer stores encoded data during decoding. The maximum buffer
	// size needed was computed just above.
	abx->buf = malloc(abx->maxEncSize);
	if (!abx->buf)
	{
		abx->last_error = errno;
		abx_close(abx);
		return false;
	}

	return true;
}

static bool abx_writeHeaders(abx_File *abx)
{
	abx_FileHeader fileHdr;
	unsigned i;

	fileHdr.numFrames = abx->numFrames;
	fileHdr.maxBufSize = abx->maxBufSize;
	fileHdr.freq = abx->freq;
	fileHdr.totalSize = abx->totalSize;
	if (!abx_writeFileHeader(abx, &fileHdr))
		return false;

	for (i = 0; i < abx->numFrames; ++i)
	{
		abx_FrameInfo *info = abx->frames + i;

		if (!abx_writeFrameInfo(abx, info))
			return false;
	}

	return true;
}

bool abx_create(abx_File *abx, const char *filename)
{
	memset(abx, 0, sizeof(*abx));

	abx->fp = fopen(filename, "wb");
	if (!abx->fp)
	{
		abx->last_error = errno;
		return false;
	}
	abx->freq = ABX_DEFAULT_FREQ;
	abx->maxError = ABX_DEFAULT_ERROR;

	if (!abx_writeHeaders(abx))
	{
		abx_close(abx);
		return false;
	}
	abx->frames_ofs = ftell(abx->fp);
	
	abx->maxFrames = 10;
	abx->frames = calloc(sizeof(abx->frames[0]), abx->maxFrames);
	if (!abx->frames)
	{
		abx->last_error = errno;
		fprintf(stderr, "abx_create(): could not allocate frames array\n");
		abx_close(abx);
		return false;
	}

	fseek(abx->fp, abx->maxFrames * abx_FrameInfo_size, SEEK_CUR);
	abx->data_ofs = ftell(abx->fp);

	abx->writing = true;
	return true;
}

static bool abx_flushHeaders(abx_File *abx)
{
	fseek(abx->fp, 0, SEEK_SET);
	if (!abx_writeHeaders(abx))
	{
		return false;
	}
	return true;
}

void abx_close(abx_File *abx)
{
	if (abx->fp)
	{
		if (abx->writing)
			abx_flushHeaders(abx);

		fclose(abx->fp);
	}
	if (abx->frames)
		free(abx->frames);
	if (abx->buf)
		free(abx->buf);

	memset(abx, 0, sizeof(*abx));
}

bool abx_setSamplingRate(abx_File *abx, uint32_t freq)
{
	if (!abx->writing)
		return false;
	abx->freq = freq;
	return true;
}

uint32_t abx_getMaxBuffer(abx_File *abx)
{
	return abx->maxBufSize;
}

bool abx_setMaxFrames(abx_File *abx, unsigned maxFrames)
{
	abx_FrameInfo *newf;

	if (!abx->writing)
		return false;

	if (maxFrames < abx->numFrames)
		return false;

	if (abx->numFrames > 0 && maxFrames <= abx->maxFrames)
	{	// We've already written some audio data to the file.
		// Decreasing the allocated frame info space at this point involves
		// way too much work, so we'll silently ignore this.
		return true;
	}
	else if (abx->numFrames > 0 && maxFrames > abx->maxFrames)
	{	// We've already written some audio data to the file.
		// Increasing the allocated frame info space at this point involves
		// way too much work, so it is an error to attempt it.
		abx->last_error = ENOSPC;
		return false;
	}

	if (abx->frames && maxFrames > abx->maxFrames)
	{	// grow the array
		newf = realloc(abx->frames, maxFrames * sizeof(abx->frames[0]));
		if (!newf)
		{
			abx->last_error = errno;
			return false;
		}
		abx->frames = newf;
	}
	abx->maxFrames = maxFrames;

	if (abx->numFrames == 0)
	{	// We have not written any audio data yet.
		// Adjust the data offset
		fseek(abx->fp, abx->frames_ofs + abx->maxFrames * abx_FrameInfo_size, SEEK_SET);
		abx->data_ofs = ftell(abx->fp);
	}

	return true;
}

uint32_t abx_readFrame(abx_File *abx, void *buf, uint32_t bufsize)
{
	abx_FrameInfo *info;
	abx_FrameHeader hdr;
	uint32_t decSize;
	uint32_t inlen;

	if (abx->writing)
	{
		abx->last_error = EPERM;
		return 0;
	}

	if (abx->nextFrame == abx->numFrames)
	{	// EOF
		abx->last_error = 0;
		return 0;
	}

	info = abx->frames + abx->nextFrame;
	// Go get the next frame
	if (fseek(abx->fp, info->ofs, SEEK_SET) != 0)
	{
		abx->last_error = errno;
		return 0;
	}
	if (!abx_readFrameHeader(abx, &hdr))
		return 0;
	if (hdr.usize != info->usize)
	{
		fprintf(stderr, "abx_readFrame() Warning: "
				"decoded size in header (%u) does not match reported in info (%u) for frame %u\n",
				(unsigned)hdr.usize, (unsigned)info->usize, abx->nextFrame);
	}
	if (hdr.freq != 0 && hdr.freq != abx->freq)
	{
		fprintf(stderr, "abx_readFrame() Warning: "
				"frame frequency (%u) is different from file freq (%u) for frame %u\n",
				(unsigned)hdr.freq, (unsigned)abx->freq, abx->nextFrame);
		fprintf(stderr, "This is not supported. Output will be corrupted.\n");
	}
	if (bufsize < hdr.usize)
	{	// Buffer is too small to accept the entire frame
		// The caller should call abx_getMaxBuffer() to find out the size
		abx->last_error = 0;
		return 0;
	}

	inlen = info->fsize - abx_FrameHeader_size;
	if (fread(abx->buf, inlen, 1, abx->fp) != 1)
	{
		abx->last_error = errno;
		return 0;
	}

	decSize = abx_decodeFrame(abx, &hdr, inlen, buf);
	if (decSize != hdr.usize)
	{
		fprintf(stderr, "abx_readFrame() Warning: "
				"actual decoded data size (%u) does not match reported (%u) for frame %u\n",
				(unsigned)decSize, (unsigned)hdr.usize, abx->nextFrame);
	}

	++abx->nextFrame;

	return decSize;
}

uint32_t abx_writeFrame(abx_File *abx, void *buf, uint32_t bufsize)
{
	abx_FrameInfo *info;
	abx_FrameHeader hdr;
	uint32_t encSize;

	if (!abx->writing)
	{
		abx->last_error = EPERM;
		return 0;
	}

	if (abx->nextFrame >= abx->maxFrames)
	{	// No more room
		abx->last_error = EFBIG;
		return 0;
	}

	info = abx->frames + abx->nextFrame;

	// Our buffer stores encoded data during encoding, but the encoded data
	// can never be larger than the decoded one by algorithm definition.
	if (bufsize > abx->maxBufSize)
	{	// grow the buffer
		if (abx->buf)
			free(abx->buf);
		abx->buf = malloc(bufsize);
		if (!abx->buf)
		{
			abx->last_error = errno;
			return 0;
		}
		abx->maxBufSize = bufsize;
	}

	if (fseek(abx->fp, abx->data_ofs, SEEK_SET) != 0)
	{
		abx->last_error = errno;
		return 0;
	}
	hdr.blockSize = ABX_DEFAULT_BLOCKSIZE;
	hdr.minSquelch = ABX_DEFAULT_SQUELCH;
	hdr.maxError = abx->maxError;
	encSize = abx_encodeFrame(abx, &hdr, buf, bufsize);
	if (!abx_writeFrameHeader(abx, &hdr) ||
			fwrite(abx->buf, encSize, 1, abx->fp) != 1)
	{
		abx->last_error = errno;
		return 0;
	}
	encSize += abx_FrameHeader_size;
	
	info->usize = bufsize;
	info->ofs = abx->data_ofs;
	info->fsize = encSize;
	abx->data_ofs = ftell(abx->fp);

	if (encSize > abx->maxEncSize)
		abx->maxEncSize = encSize;
	abx->totalSize += bufsize;

	++abx->nextFrame;
	++abx->numFrames;

	return encSize;
}

static inline void clip_u8(int *val)
{
	if (*val < 0)
		*val= 0;
	else if (*val > 255)
		*val = 255;
}

static uint32_t abx_decodeFrame(abx_File *abx, const abx_FrameHeader *hdr,
		int inlen, uint8_t *out)
{
	uint8_t *in = abx->buf;
	int outlen = hdr->usize;
	int prev;

	// Get initial data point
	prev = *in;
	++in;
	--inlen; // one byte consumed
	*out = prev;
	++out;
	--outlen; // one sample stored

	while (outlen > 0 && inlen > 0)
	{
		unsigned bytes;
		unsigned sample;

		// Get next encoded byte
		sample = *in;
		++in;
		--inlen;

		if (sample & RESYNC) // Is it a resync byte?
		{
			prev = (sample & 0x7F) << 1; // Store resync byte.
			*out = prev;
			++out;
			--outlen; // one sample stored
		}
		else if (sample & SQLCH) // Is it a squelch byte?
		{
			bytes = sample & SQUELCHCNT;	// And off the number of squelch bytes
			memset(out, prev, bytes);
			out += bytes;
			outlen -= bytes;	// bytes samples stored
		}
		else if (sample & DELTAMOD)	// Is it delta modulate byte?
		{
			// base address to multiplier table
			const int *base = deltas + (sample & MULTIPLIER) * 16;
			unsigned sampleBits; // bits per sample
			unsigned mask;
			int samplesPerByte;

			// This is not optimized for efficiency, but rather deoptimized
			// for readability
			sampleBits = (sample & DELTAMOD) >> DELTASHIFT;
			if (sampleBits == 3) // no 3-bit delta coding
				sampleBits = 4;

			// Base address of deltas: middle of the table minus half the
			// range of the delta
			base += 8 - (1 << (sampleBits - 1));
			samplesPerByte = 8 / sampleBits;
			mask = (1 << sampleBits) - 1;

			for (bytes = hdr->blockSize / samplesPerByte; bytes > 0; --bytes)
			{
				unsigned val;
				int i;
				
				val = *in;
				++in;
				--inlen;

				for (i = samplesPerByte; i > 0; --i)
				{
					val <<= sampleBits;
					prev += base[(val >> 8) & mask];
					clip_u8(&prev);
					*out = prev;
					++out;
				}
			}

			outlen -= hdr->blockSize; // one block of samples stored
		}
		else
		{	// None of the known bit combinations. Weird.
			fprintf(stderr, "abx_decodeFrame() Warning: "
					"unknown sample 0x%02x in frame %u\n",
					(unsigned)sample, abx->nextFrame);
			// We'll just suppress the sample
		}
	}

	if (outlen != 0 || inlen != 0)
	{
		fprintf(stderr, "abx_decodeFrame() Warning: "
				"byte counts do not match at end of frame (%i, %i)\n",
				inlen, outlen);
	}

	return hdr->usize - outlen;
}

static int lookupDelta(const int *base, int cnt, int prev, int sample)
{
	int i;
	int imin = 0;
	int mindiff = 65536;

	for (i = 0; i < cnt; ++i)
	{
		int diff;
		// We want the delta that gives us a resulting sample that is
		// the closest to the original *after* any clipping occurs.
		// This is important in cases where both the previous sample
		// and the current sample are at min or max points, since there
		// is no 0 deltas in the tables.
		int cur = prev + base[i];
		clip_u8(&cur);
		diff = abs(cur - sample);
		if (diff < mindiff)
		{
			mindiff = diff;
			imin = i;
		}
	}
	return imin;
}

static uint32_t abx_encodeBlock(const abx_FrameHeader *hdr, uint8_t *in,
		uint8_t *out, int *last, unsigned sampleBits, unsigned mult,
		int *blockError)
{
	const int *base = deltas + mult * 16;
	const int samplesPerByte = 8 / sampleBits;
	const int deltaCnt = 1 << sampleBits;
	unsigned bytes;
	int prev = *last;
	int error = 0;

	// Base address of deltas: middle of the table minus half the
	// range of the delta
	base += 8 - deltaCnt / 2;

	for (bytes = hdr->blockSize / samplesPerByte; bytes > 0; --bytes)
	{
		unsigned val = 0;
		int i;
		
		for (i = samplesPerByte; i > 0; --i)
		{
			int sample = *in;
			unsigned index;
			
			++in;
			// Computing the closest delta index directly involves a ridiculous
			// amount of logic because the delta tables have no 0 deltas. It is
			// simpler to just iterate over all of them.
			index = lookupDelta(base, deltaCnt, prev, sample);
			prev += base[index];
			clip_u8(&prev);
			error += (prev - sample) * (prev - sample);
			if (error > hdr->maxError)
				return 0; // exceeded the maximum error, bail out

			val <<= sampleBits;
			val |= index;
		}

		if (out)
		{
			*out = val;
			++out;
		}
	}
	
	*last = prev;
	if (blockError)
		*blockError = error;
	return hdr->blockSize / samplesPerByte;
}

static uint32_t abx_encodeFrame(abx_File *abx, abx_FrameHeader *hdr,
		uint8_t *in, int inlen)
{
	uint8_t *out = abx->buf;
	int prev;

	hdr->usize = inlen;
	hdr->freq = abx->freq;

	// Store initial data point
	prev = *in;
	++in;
	--inlen; // one sample consumed
	*out = prev;
	++out;

	// Speed and efficiency is not an issue for us. The strategy here is
	// simply to achieve maximum compression by brute force. We try all of
	// the 48 delta coding variants and pick the one with the smallest
	// total error within the allowed limit.
	while (inlen > 0)
	{
		int cnt;

		// Try squelching first
		for (cnt = 0; cnt < inlen && cnt < SQUELCHCNT; ++cnt)
		{
			if (in[cnt] != prev)
				break;
		}
		if (cnt >= hdr->minSquelch)
		{	// Squelch sample repeats
			*out = SQLCH | cnt;
			++out;
			in += cnt;
			inlen -= cnt;
			continue;
		}

		// Now try resync + squelch
		for (cnt = 0; cnt < inlen - 1 && cnt < SQUELCHCNT; ++cnt)
		{
			if (in[cnt + 1] != in[0])
				break;
		}
		if (cnt >= hdr->minSquelch + 1)
		{	// Resync and squelch sample repeats
			prev = *in;
			out[0] = RESYNC | (prev >> 1);
			out[1] = SQLCH | cnt;
			out += 2;
			in += 1 + cnt;
			inlen -= 1 + cnt;
			continue;
		}

		// Try a delta-coding block
		if (inlen >= hdr->blockSize)
		{
			int bits, bestBits = 0;
			int mult, bestMult = 0;
			int error, bestError = hdr->maxError * 4;

			error = bestError; // for shortcutting
			for (bits = 1; bits < 4 && error != 0; ++bits)
			{
				if (bits == 3) // no 3-bit coding
					bits = 4;

				for (mult = 0; mult < 16; ++mult)
				{
					uint32_t blk;
					int last = prev;
					blk = abx_encodeBlock(hdr, in, NULL, &last,
							bits, mult, &error);
					if (blk > 0 && error < bestError)
					{	// remember the best one so far
						bestError = error;
						bestBits = bits;
						bestMult = mult;

						if (error == 0)
							break; // shortcut
					}
				}
			}
			if (bestBits > 0)
			{	// success!
				// out+1 because we need space for the DELTAMOD byte
				uint32_t blk = abx_encodeBlock(hdr, in, out + 1, &prev,
						bestBits, bestMult, NULL);
				if (bestBits == 4)
					bestBits = 3;
				*out = (bestBits << DELTASHIFT) | bestMult;
				out += 1 + blk;
				in += hdr->blockSize;
				inlen -= hdr->blockSize;
				continue;
			}
		}

		// And when everything else fails, emit a RESYNC
		prev = *in;
		++in;
		--inlen;
		*out = RESYNC | (prev >> 1);
		++out;
	}

	return out - abx->buf;
}
