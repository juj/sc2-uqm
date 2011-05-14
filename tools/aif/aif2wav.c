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
 * AIFF to WAVE converter.
 * By Serge van den Boom (svdb@stack.nl) 20020816
 * AIFF decoder modularization and WAVE ouput by Alex Volkov (codepro@usa.net)
 *
 * Doesn't convert all .aif files in general, only AIFF-C, 16 bits
 * SDX2-compressed, and AIFF 8-/16-bit PCM.
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aiff.h"
#include "wav.h"

void convert_aiff_to_wave(aiff_File *aiff, wave_File *wave);

int
main(int argc, char *argv[]) {
	wave_File wave;
	aiff_File aiff;

	if (argc != 3) {
		fprintf(stderr, "aif2wav <infile> <outfile>\n");
		return EXIT_FAILURE;
	}

	if (!aiff_open(&aiff, argv[1])) {
		perror("Could not open input file");
		return EXIT_FAILURE;
	}

	if (!wave_create(&wave, argv[2])) {
		perror("Could not open output file");
		return EXIT_FAILURE;
	}

	convert_aiff_to_wave(&aiff, &wave);

	wave_close(&wave);
	aiff_close(&aiff);
	return EXIT_SUCCESS;
}

void
convert_aiff_to_wave(aiff_File *aiff, wave_File *wave) {

	uint32_t bufsize;
	uint8_t *buf;
	uint32_t bytes;

	wave_setFormat(wave, aiff->fmtHdr.channels, aiff->fmtHdr.sampleSize,
			aiff->fmtHdr.sampleRate);

	bufsize = 0x10000;
	buf = malloc(bufsize);
	if (!buf) {
		perror("alloc buffer");
        exit(EXIT_FAILURE);
	}

	fprintf(stderr, "Number of channels:        %u\n",
			(unsigned)aiff->fmtHdr.channels);
	fprintf(stderr, "Number of sample frames:   %u\n",
			(unsigned)aiff->fmtHdr.sampleFrames);
	fprintf(stderr, "Number of bits per sample: %u\n",
			(unsigned)aiff->fmtHdr.sampleSize);
	fprintf(stderr, "Sample rate:               %d Hz\n",
			(int)aiff->fmtHdr.sampleRate);

	for (bytes = aiff_readData(aiff, buf, bufsize); bytes > 0; ) {
		if (wave_writeData(wave, buf, bytes) != bytes) {
			fprintf(stderr, "Cannot write wave: %s\n", strerror(wave->last_error));
			break;
		}
		bytes = aiff_readData(aiff, buf, bufsize);
	}
	if (bytes == 0 && aiff->last_error != 0)
		fprintf(stderr, "Cannot read aiff: %s\n", strerror(aiff->last_error));

	free(buf);
}

