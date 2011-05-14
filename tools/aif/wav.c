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

/* Wave format encoder/decoder */

#include <stdio.h>
#include <memory.h>
#include <errno.h>

#include "wav.h"


#define wave_FormatHeader_size  16
#define wave_ChunkHeader_size   8


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

static bool wave_readFileHeader(wave_File *wave, wave_FileHeader *hdr)
{
	if (!read_le_32(wave->fp, &hdr->id) ||
			!read_le_32(wave->fp, &hdr->size) ||
			!read_le_32(wave->fp, &hdr->type))
	{
		wave->last_error = errno;
		return false;
	}
	return true;
}

static bool wave_writeFileHeader(wave_File *wave, const wave_FileHeader *hdr)
{
	if (!write_le_32(wave->fp, hdr->id) ||
			!write_le_32(wave->fp, hdr->size) ||
			!write_le_32(wave->fp, hdr->type))
	{
		wave->last_error = errno;
		return false;
	}
	return true;
}

static bool wave_readChunkHeader(wave_File *wave, wave_ChunkHeader *chunk)
{
	if (!read_le_32(wave->fp, &chunk->id) ||
			!read_le_32(wave->fp, &chunk->size))
	{
		wave->last_error = errno;
		return false;
	}
	return true;
}

static bool wave_writeChunkHeader(wave_File *wave, const wave_ChunkHeader *chunk)
{
	if (!write_le_32(wave->fp, chunk->id) ||
			!write_le_32(wave->fp, chunk->size))
	{
		wave->last_error = errno;
		return false;
	}
	return true;
}

static bool wave_readFormatHeader(wave_File *wave, wave_FormatHeader *fmt)
{
	if (!read_le_16(wave->fp, &fmt->format) ||
			!read_le_16(wave->fp, &fmt->channels) ||
			!read_le_32(wave->fp, &fmt->samplesPerSec) ||
			!read_le_32(wave->fp, &fmt->bytesPerSec) ||
			!read_le_16(wave->fp, &fmt->blockAlign) ||
			!read_le_16(wave->fp, &fmt->bitsPerSample))
	{
		wave->last_error = errno;
		return false;
	}
	return true;
}

static bool wave_writeFormatHeader(wave_File *wave, const wave_FormatHeader *fmt)
{
	if (!write_le_16(wave->fp, fmt->format) ||
			!write_le_16(wave->fp, fmt->channels) ||
			!write_le_32(wave->fp, fmt->samplesPerSec) ||
			!write_le_32(wave->fp, fmt->bytesPerSec) ||
			!write_le_16(wave->fp, fmt->blockAlign) ||
			!write_le_16(wave->fp, fmt->bitsPerSample))
	{
		wave->last_error = errno;
		return false;
	}
	return true;
}

bool wave_open(wave_File *wave, const char *filename)
{
	wave_FileHeader fileHdr;
	wave_ChunkHeader chunkHdr;
	long dataLeft; 

	memset(wave, 0, sizeof(*wave));

	wave->fp = fopen(filename, "rb");
	if (!wave->fp)
	{
		wave->last_error = errno;
		return false;
	}

	// read wave header
	if (!wave_readFileHeader(wave, &fileHdr))
	{
		wave->last_error = errno;
		wave_close(wave);
		return false;
	}
	if (fileHdr.id != wave_RiffID || fileHdr.type != wave_WaveID)
	{
		fprintf(stderr, "wave_open(): "
				"not a wave file, ID 0x%08x, Type 0x%08x",
				(unsigned)fileHdr.id, (unsigned)fileHdr.type);
		wave_close(wave);
		return false;
	}

	for (dataLeft = ((fileHdr.size + 1) & ~1) - 4; dataLeft > 0;
			dataLeft -= (((chunkHdr.size + 1) & ~1) + 8))
	{
		if (!wave_readChunkHeader(wave, &chunkHdr))
		{
			wave_close(wave);
			return false;
		}

		if (chunkHdr.id == wave_FmtID)
		{
			if (!wave_readFormatHeader(wave, &wave->fmtHdr))
			{
				wave_close(wave);
				return false;
			}
			fseek(wave->fp, chunkHdr.size - 16, SEEK_CUR);
		}
		else
		{
			if (chunkHdr.id == wave_DataID)
			{
				wave->data_size = chunkHdr.size;
				wave->data_ofs = ftell(wave->fp);
			}
			fseek(wave->fp, chunkHdr.size, SEEK_CUR);
		}

		// 2-align the file ptr
		// XXX: I do not think this is necessary in WAVE files;
		//   possibly a remnant of ported AIFF reader
		fseek(wave->fp, chunkHdr.size & 1, SEEK_CUR);
	}

	if (!wave->data_size || !wave->data_ofs)
	{
		fprintf(stderr, "wave_open(): bad wave file,"
				" no DATA chunk found");
		wave_close(wave);
		return false;
	}

	if (wave->fmtHdr.format != WAVE_FORMAT_PCM)
	{	// not a PCM format
		fprintf(stderr, "wave_open(): unsupported format %x",
				wave->fmtHdr.format);
		wave_close(wave);
		return false;
	}

	if (dataLeft != 0)
	{
		fprintf(stderr, "wave_open(): bad or unsupported wave file, "
				"size in header does not match read chunks");
	}

	wave->bytesPerSample = (wave->fmtHdr.bitsPerSample + 7) >> 3;
	if (wave->bytesPerSample == 3)
	{
		fprintf(stderr, "wave_open(): 24-bit data is not fully supported\n");
	}

	fseek(wave->fp, wave->data_ofs, SEEK_SET);
	wave->max_pcm = wave->data_size / wave->fmtHdr.blockAlign;
	wave->cur_pcm = 0;
	wave->last_error = 0;

	return true;
}

static bool wave_writeHeaders(wave_File *wave)
{
	wave_FileHeader fileHdr;
	wave_ChunkHeader chunkHdr;

	fileHdr.id = wave_RiffID;
	fileHdr.size = 4 + wave_ChunkHeader_size + wave_FormatHeader_size
			+ wave_ChunkHeader_size + wave->data_size;
	fileHdr.type = wave_WaveID;
	if (!wave_writeFileHeader(wave, &fileHdr))
		return false;

	chunkHdr.id = wave_FmtID;
	chunkHdr.size = wave_FormatHeader_size;
	if (!wave_writeChunkHeader(wave, &chunkHdr) ||
			!wave_writeFormatHeader(wave, &wave->fmtHdr))
		return false;

	chunkHdr.id = wave_DataID;
	chunkHdr.size = wave->data_size;
	if (!wave_writeChunkHeader(wave, &chunkHdr))
		return false;

	return true;
}

bool wave_create(wave_File *wave, const char *filename)
{
	memset(wave, 0, sizeof(*wave));

	wave->fp = fopen(filename, "wb");
	if (!wave->fp)
	{
		wave->last_error = errno;
		return false;
	}

	wave->fmtHdr.format = WAVE_FORMAT_PCM;
	if (!wave_writeHeaders(wave))
	{
		wave->last_error = errno;
		return false;
	}

	wave->data_ofs = ftell(wave->fp); 
	wave->writing = true;
	return true;
}

static bool wave_flushHeaders(wave_File *wave)
{
	wave->data_size = wave->max_pcm * wave->fmtHdr.blockAlign;
	fseek(wave->fp, 0, SEEK_SET);
	if (!wave_writeHeaders(wave))
	{
		wave->last_error = errno;
		return false;
	}
	return true;
}

void wave_close(wave_File *wave)
{
	if (wave->fp)
	{
		if (wave->writing)
			wave_flushHeaders(wave);

		fclose(wave->fp);
	}
	memset(wave, 0, sizeof(*wave));
}

bool wave_setFormat(wave_File *wave, uint16_t chans, uint16_t bitsPerSample,
		uint32_t freq)
{
	wave->fmtHdr.format = WAVE_FORMAT_PCM;
	wave->fmtHdr.channels = chans;
	wave->fmtHdr.bitsPerSample = bitsPerSample;
	wave->bytesPerSample = (bitsPerSample + 7) >> 3;
	if (wave->bytesPerSample == 3)
	{
		fprintf(stderr, "wave_setFormat(): 24-bit data is not fully supported\n");
	}
	wave->fmtHdr.samplesPerSec = freq;
	wave->fmtHdr.blockAlign = wave->bytesPerSample * chans;
	wave->fmtHdr.bytesPerSec = wave->fmtHdr.blockAlign * freq;
	return true;
}

uint32_t wave_readData(wave_File *wave, void *buf, uint32_t bufsize)
{
	uint32_t pcm;
	uint8_t *ptr = (uint8_t*)buf;
	uint32_t size;
	uint32_t left;

	pcm = bufsize / wave->fmtHdr.blockAlign;
	if (pcm > wave->max_pcm - wave->cur_pcm)
		pcm = wave->max_pcm - wave->cur_pcm;

	pcm = fread(buf, wave->fmtHdr.blockAlign, pcm, wave->fp);
	wave->cur_pcm += pcm;
	size = pcm * wave->fmtHdr.blockAlign;

	// WAVE files store data little-endian and we need it in machine order
	// Let the compiler optimize this away if it is not necessary
	switch (wave->bytesPerSample)
	{
	case 2:
		for (left = size / 2; left > 0; --left, ptr += 2)
			*(int16_t*)ptr = (ptr[1] << 8) | ptr[0];
		break;

	case 3:
		// TODO
		break;
		
	case 4:
		for (left = size / 4; left > 0; --left, ptr += 4)
			*(int32_t*)ptr = (ptr[3] << 24) | (ptr[2] << 16) | (ptr[1] << 8) | ptr[0];
		break;
	}

	return size;
}

uint32_t wave_writeData(wave_File *wave, void *buf, uint32_t bufsize)
{
	uint32_t pcm;
	uint8_t *ptr = (uint8_t*)buf;
	uint32_t size;
	uint32_t left;

	pcm = bufsize / wave->fmtHdr.blockAlign;

	size = pcm * wave->fmtHdr.blockAlign;
	// WAVE files store data little-endian and we have it in machine order
	// Let the compiler optimize this away if it is not necessary
	switch (wave->bytesPerSample)
	{
	case 2:
		for (left = size / 2; left > 0; --left, ptr += 2)
		{
			int16_t v = *(int16_t*)ptr;
			ptr[0] = (v & 0xff);
			ptr[1] = (v >> 8);
		}
		break;

	case 3:
		// TODO
		break;
		
	case 4:
		for (left = size / 4; left > 0; --left, ptr += 4)
		{
			int32_t v = *(int32_t*)ptr;
			ptr[0] = (v & 0xff);
			ptr[1] = (v >> 8);
			ptr[2] = (v >> 16);
			ptr[3] = (v >> 24);
		}
		break;
	}

	pcm = fwrite(buf, wave->fmtHdr.blockAlign, pcm, wave->fp);
	wave->cur_pcm += pcm;
	wave->max_pcm = wave->cur_pcm;
	size = pcm * wave->fmtHdr.blockAlign;
	wave->data_size += size;

	return size;
}
