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
 * ABX to WAVE converter: By Serge van den Boom (svdb@stack.nl)
 * Modularized and wave output by Alex Volkov (codepro@usa.net)
 *
 * So far, it ignores sample rates, so it will work ok as long as all
 * the frames have the same frequency. This is probably
 * enough for our purposes.
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "abx.h"
#include "wav.h"

void convert_abx_to_wave(abx_File *abx, wave_File *wave);

int
main(int argc, char *argv[]) {
	wave_File wave;
	abx_File abx;

	if (argc != 3) {
		fprintf(stderr, "abx2wav <infile> <outfile>\n");
		return EXIT_FAILURE;
	}

	if (!abx_open(&abx, argv[1])) {
		perror("Could not open input file");
		return EXIT_FAILURE;
	}

	if (!wave_create(&wave, argv[2])) {
		perror("Could not open output file");
		return EXIT_FAILURE;
	}

	convert_abx_to_wave(&abx, &wave);

	wave_close(&wave);
	abx_close(&abx);
	return EXIT_SUCCESS;
}

void
convert_abx_to_wave(abx_File *abx, wave_File *wave) {

	uint32_t bufsize;
	uint8_t *buf;
	uint32_t bytes;

	wave_setFormat(wave, 1, 8, abx->freq);

	bufsize = abx_getMaxBuffer(abx);
	buf = malloc(bufsize);
	if (!buf) {
		perror("alloc buffer");
        exit(EXIT_FAILURE);
	}

	for (bytes = abx_readFrame(abx, buf, bufsize); bytes > 0; ) {
		if (wave_writeData(wave, buf, bytes) != bytes) {
			fprintf(stderr, "Cannot write wave: %s\n", strerror(wave->last_error));
			break;
		}
		bytes = abx_readFrame(abx, buf, bufsize);
	}
	if (bytes == 0 && abx->last_error != 0)
		fprintf(stderr, "Cannot read abx: %s\n", strerror(abx->last_error));

	free(buf);
}
