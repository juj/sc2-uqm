/*
 * abx to raw converter. By Serge van den Boom (svdb@stack.nl),
 * The actual conversion code is from Toys for Bob. 
 * So far, it ignores sample rates, so it will work ok as long as all
 * the frames have the same frequency. This is probably
 * enough for our purposes.
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "abx2raw.h"

void convert_abx(FILE *in, FILE *out);
uint8_t *UnCompressAudio(struct abx_header *abx, uint8_t *source);

int
main(int argc, char *argv[]) {
	FILE *in, *out;
	
	if (argc != 3) {
		fprintf(stderr, "abx2wav <infile> <outfile>\n");
		return EXIT_FAILURE;
	}

	in = fopen(argv[1], "rb");
	if (!in) {
		perror("Could not open input file");
		return EXIT_FAILURE;
	}

	out = fopen(argv[2], "wb");
	if (!out) {
		perror("Could not open output file");
		return EXIT_FAILURE;
	}

	convert_abx(in, out);

	fclose(in);
	fclose(out);
	return EXIT_SUCCESS;
}

static signed char trans[16*16] =
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

void
read_data(char *buf, size_t size, FILE *file) {
	ssize_t numread;
	
	numread = fread(buf, size, 1, file);
	if (numread == 0 && ferror(file)) {
		perror("read header");
		exit(EXIT_FAILURE);
	}
	if ((size_t) numread != 1) {
		fprintf(stderr, "Input file too small.\n");
		exit(EXIT_FAILURE);
	}
}

void
convert_abx(FILE *in, FILE *out) { 
	struct abx_header abx;
	struct frame_info *frame_info;
	uint8_t **frames;
	uint8_t **uncoded;
	int i;

	read_data((uint8_t *) &abx, sizeof (struct abx_header), in);
	fprintf(stderr, "Base sample rate: %dHz\n", abx.freq);
	frame_info = malloc(sizeof (struct frame_info) * abx.num_frames);
	read_data((uint8_t *) frame_info,
			sizeof (struct frame_info) * abx.num_frames, in);
	frames = malloc(sizeof (uint8_t *) * abx.num_frames);
	uncoded = malloc(sizeof (uint8_t *) * abx.num_frames);
	for (i = 0; i < abx.num_frames; i++) {
		frames[i] = malloc(frame_info[i].fsize);
		fseek(in, frame_info[i].addr, SEEK_SET);
		read_data(frames[i], frame_info[i].fsize, in);

#if 0
		// debug output to locate the first bad frame
		fprintf(stderr, "Now going to process frame %d.\n", i);
#endif

#if 0
		// skip some corrupt frames
		if (i >= 270 && i <= 271) {
			// fill the bad part with zeros
			uncoded[i] = malloc(frame_info[i].usize);
			memset(uncoded[i], '\0', frame_info[i].usize);
			continue;
		}
#endif

		uncoded[i] = UnCompressAudio(&abx, frames[i]);
	}

	for (i = 0; i < abx.num_frames; i++) {
		fwrite(uncoded[i], frame_info[i].usize, 1, out);
	}
	
	for (i = 0; i < abx.num_frames; i++) {
		free(frames[i]);
		free(uncoded[i]);
	}
	free(uncoded);
	free(frames);
	free(frame_info);
}

#define MAKE_WORD(byte1, byte2) ((byte2 << 8) | (byte1))

// This is used to make certain this C code is compatible when compiled on
// a 68000 based machine.  (Which it has been done and tested on.)
#define Get8086word(t)	MAKE_WORD ((t)[0], (t)[1])

// GetFreq will report the playback frequency of a particular ACOMP data
// file.
uint16_t
GetFreq(uint8_t *sound) {
	return(Get8086word(sound + 2));
}

uint8_t *
UnCompressAudio(struct abx_header *abx,
		uint8_t *source) {
	uint16_t slen, frame, freq;
	int16_t prev;
	uint8_t *result, *dest;

	slen = Get8086word(source);
	dest = result = malloc(slen * sizeof (uint8_t));
	freq = GetFreq(source);
	if (freq == 0) {
		freq = abx->freq;
	} else if (freq != abx->freq) {
		fprintf(stderr, "Frame frequency (%d) != global frequency (%d).\n",
				freq, abx->freq);
		fprintf(stderr, "This is not supported. Output will be corrupted.\n");
		abx->freq = freq;
	}
	source += 4;  // Skip length, and then frequency word.
	frame = *source++;	// Frame size.
	source += 3;  // Skip sqelch value, and maximum error allowed.
	prev = *source++;  // Get initial previous data point.
	*dest++ = prev ^ 0x80;
	slen--;						  // Decrement total sound length.
	while (slen > 0)
	{
		uint16_t bytes;
		uint8_t	sample;

		sample = *source++;  // Get sample.
		if (sample & RESYNC) // Is it a resync byte?
		{
			--slen;  // Decrement output sample length.

			prev = (sample & 0x7F) << 1; // Store resync byte.
			*dest++ = prev ^ 0x80;
		}
		else if (sample & SQLCH) // Is it a squelch byte?
		{
			bytes = sample & SQUELCHCNT;	// And off the number of squelch bytes
			slen -= bytes;	// Decrement total samples remaining count.

			memset(dest, prev ^ 0x80, bytes);
			dest += bytes;
		}
		else		// Must be a delta modulate byte!!
		{
			int8_t *base;

			slen -= frame; // Pulling one frame out.
					// Compute base address to multiplier table.
			base = trans + (sample & MULTIPLIER) * 16;
			switch (sample & DELTAMOD) // Delta mod resolution.
			{
				case ONEBIT:
				{
					int16_t up;

					up = base[8];	 // Go up 1 bit.
					for (bytes = frame / 8; bytes; bytes--)
					{
						uint8_t mask;

						sample = *source++;
						for(mask = 0x80; mask; mask >>= 1)
						{
							if ( sample & mask )
								prev += up;
							else
								prev -= up;
							if ( prev < 0 ) prev = 0;
							else if ( prev > 255 ) prev = 255;
							*dest++ = prev ^ 0x80;
						}
					}
					break;
				}
				case TWOBIT:
					base+=6; // Base address of two bit delta's.
					for (bytes = frame / 4; bytes; bytes--)
					{
						sample = *source++;

						prev += base[sample>>6];
						if ( prev < 0 ) prev = 0;
						else if ( prev > 255 ) prev = 255;
						*dest++ = prev ^ 0x80;

						prev += base[(sample>>4)&0x3];
						if ( prev < 0 ) prev = 0;
						else if ( prev > 255 ) prev = 255;
						*dest++ = prev ^ 0x80;

						prev += base[(sample>>2)&0x3];
						if ( prev < 0 ) prev = 0;
						else if ( prev > 255 ) prev = 255;
						*dest++ = prev ^ 0x80;

						prev += base[sample&0x3];
						if ( prev < 0 ) prev = 0;
						else if ( prev > 255 ) prev = 255;
						*dest++ = prev ^ 0x80;
					}
					break;
				case FOURBIT:
					for (bytes = frame / 2; bytes; bytes--)
					{
						sample = *source++;

						prev += base[sample>>4];
						if ( prev < 0 ) prev = 0;
						else if ( prev > 255 ) prev = 255;
						*dest++ = prev ^ 0x80;

						prev += base[sample&0x0F];
						if ( prev < 0 ) prev = 0;
						else if ( prev > 255 ) prev = 255;
						*dest++ = prev ^ 0x80;
					}
					break;
			}
		}
		// While still audio data to decompress....
	}
	return result;
}

