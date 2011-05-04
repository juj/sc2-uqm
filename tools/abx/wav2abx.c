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
 * WAVE to ABX: By Serge van den Boom (svdb@stack.nl) and 
 *    Alex Volkov (codepro@usa.net)
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "abx.h"
#include "wav.h"

#define BUFFER_SIZE 0x1000

void convert_wave_to_abx(wave_File *wave, abx_File *abx);

int
main(int argc, char *argv[]) {
	wave_File wave;
	abx_File abx;

	if (argc != 3) {
		fprintf(stderr, "wav2abx <infile> <outfile>\n");
		return EXIT_FAILURE;
	}

	if (!wave_open(&wave, argv[1])) {
		perror("Could not open input file");
		return EXIT_FAILURE;
	}
	if (wave.fmtHdr.channels != 1) {
		fprintf(stderr, "Unsupported number of channels %u\n", 
				(unsigned)wave.fmtHdr.channels);
		return EXIT_FAILURE;
	}
	if (wave.fmtHdr.bitsPerSample != 8 && wave.fmtHdr.bitsPerSample != 16) {
		fprintf(stderr, "Unsupported bits per sample %u\n", 
				(unsigned)wave.fmtHdr.bitsPerSample);
		return EXIT_FAILURE;
	}

	if (!abx_create(&abx, argv[2])) {
		perror("Could not open output file");
		return EXIT_FAILURE;
	}

	convert_wave_to_abx(&wave, &abx);

	abx_close(&abx);
	wave_close(&wave);
	return EXIT_SUCCESS;
}

void
convert_16(void *buf, uint32_t bufsize) {
	uint8_t *src = buf;
	uint8_t *dst = buf;

	for ( ; bufsize > 1; src += 2, ++dst, bufsize -= 2) {
		int v = (int16_t)((src[1] << 8) | src[0]);
		// with error correction
		v += 0x80;
		if (v > 0x7fff)
			v = 0x7fff;
		*dst = ((v >> 8) & 0xff) ^ 0x80;
	}
}

void
convert_wave_to_abx(wave_File *wave, abx_File *abx) {

	uint32_t bufsize;
	uint8_t buf[BUFFER_SIZE * 2];
	uint32_t bytes;
	unsigned numFrames;
	unsigned bytesPerSample;

	abx_setSamplingRate(abx, wave->fmtHdr.samplesPerSec);
	// Calculate number of frames as close as we can so that
	// we do not waste file bytes
	bytesPerSample = wave->fmtHdr.bitsPerSample / 8;
	numFrames = (wave->data_size / bytesPerSample + BUFFER_SIZE - 1)
			/ BUFFER_SIZE;
	abx_setMaxFrames(abx, numFrames);

	// We support 16 bit samples
	bufsize = BUFFER_SIZE;
	if (bytesPerSample == 2)
		bufsize *= 2;

	for (bytes = wave_readData(wave, buf, bufsize); bytes > 0; ) {
		if (bytesPerSample == 2)
		{
			convert_16(buf, bytes);
			bytes /= 2;
		}

		if (abx_writeFrame(abx, buf, bytes) == 0 && abx->last_error != 0) {
			fprintf(stderr, "Cannot write abx: %s\n", strerror(abx->last_error));
			break;
		}
		bytes = wave_readData(wave, buf, bufsize);
	}
	if (bytes == 0 && wave->last_error != 0)
		fprintf(stderr, "Cannot read wave: %s\n", strerror(wave->last_error));
}
