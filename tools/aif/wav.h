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

#ifndef WAV_H_INCL
#define WAV_H_INCL

#include <stdint.h>
#include "port.h"

#define wave_MAKE_ID(x1, x2, x3, x4) \
		(((x4) << 24) | ((x3) << 16) | ((x2) << 8) | (x1))

#define wave_RiffID   wave_MAKE_ID('R', 'I', 'F', 'F')
#define wave_WaveID   wave_MAKE_ID('W', 'A', 'V', 'E')
#define wave_FmtID    wave_MAKE_ID('f', 'm', 't', ' ')
#define wave_DataID   wave_MAKE_ID('d', 'a', 't', 'a')

typedef struct
{
	uint32_t id;
	uint32_t size;
	uint32_t type;
} wave_FileHeader;

typedef struct
{
	uint16_t format;
	uint16_t channels;
	uint32_t samplesPerSec;
	uint32_t bytesPerSec;
	uint16_t blockAlign;
	uint16_t bitsPerSample;
} wave_FormatHeader;

#define WAVE_FORMAT_PCM  1

typedef struct
{
	uint32_t id;
	uint32_t size;
} wave_ChunkHeader;

typedef struct
{
	// read-only
	wave_FormatHeader fmtHdr;
	int last_error;

	// internal
	bool writing;
	FILE *fp;
	unsigned bytesPerSample;
	uint32_t data_ofs;
	uint32_t data_size;
	uint32_t max_pcm;
	uint32_t cur_pcm;
} wave_File;

bool wave_open(wave_File *wave, const char *filename);
bool wave_create(wave_File *wave, const char *filename);
void wave_close(wave_File *wave);
bool wave_setFormat(wave_File *wave, uint16_t chans, uint16_t bitsPerSample,
		uint32_t freq);
uint32_t wave_readData(wave_File *wave, void *buf, uint32_t bufsize);
uint32_t wave_writeData(wave_File *wave, void *buf, uint32_t bufsize);

#endif /* WAV_H_INCL */
