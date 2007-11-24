/*
 * duck .duk extractor.
 * By Serge van den Boom (svdb@stack.nl), 20020924
 * GPL applies
 *
 * duk audio is a ADPCM variant. Some code comes from MPlayer
 * (http://www.mplayerhq.hu/homepage/)
 *
 * For now only the audio is extracted. It's saved as 11050Hz raw audio.
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "unduck.h"

#define AUDIO
#undef VIDEO

void convert_duk(FILE *duk, FILE *frm, FILE *snd, FILE *dkm);
#ifdef AUDIO
void convert_audio(FILE *duk, FILE *snd, size_t cnt);
#endif
#ifdef VIDEO
void convert_video(FILE *duk, FILE *dkm, size_t cnt);
#endif

int
main(int argc, char *argv[]) {
	FILE *duk, *frm;
#ifdef AUDIO
	FILE *snd;
#endif
#ifdef VIDEO
	FILE *dkm;
#endif
	char *filename, *filename2;
	size_t filenamelen;
	
	if (argc != 2) {
		fprintf(stderr, "unduck <infile>\n");
		return EXIT_FAILURE;
	}

	filename = argv[1];
	filenamelen = strlen(filename);
	if (filenamelen > 4 &&
			filename[filenamelen - 4] == '.' &&
			tolower(filename[filenamelen - 3]) == 'd' &&
			tolower(filename[filenamelen - 2]) == 'u' &&
			tolower(filename[filenamelen - 1]) == 'k') {
		filename[filenamelen - 4] = '\0';
		filenamelen -= 4;
	}

	filename2 = alloca(filenamelen + 5);

	sprintf(filename2, "%s.duk", filename);
	duk = fopen(filename2, "rb");
	if (!duk) {
		sprintf(filename2, "%s.DUK", filename);
		duk = fopen(filename2, "rb");
	}
	if (!duk) {
		perror("Could not open .duk input file");
		return EXIT_FAILURE;
	}

	sprintf(filename2, "%s.frm", filename);
	frm = fopen(filename2, "rb");
	if (!frm) {
		sprintf(filename2, "%s.FRM", filename);
		frm = fopen(filename2, "rb");
	}
	if (!frm) {
		perror("Could not open .frm input file");
		return EXIT_FAILURE;
	}

#ifdef AUDIO
	sprintf(filename2, "%s.raw", filename);
	snd = fopen(filename2, "wb");
	if (!snd) {
		perror("Could not open .raw output file");
		return EXIT_FAILURE;
	}
#endif

#ifdef VIDEO
	sprintf(filename2, "%s.dkm", filename);
	dkm = fopen(filename2, "wb");
	if (!dkm) {
		perror("Could not open .dkm output file");
		return EXIT_FAILURE;
	}
#endif

	convert_duk(duk, frm,
#ifdef AUDIO
			snd,
#else
			NULL,
#endif
#ifdef VIDEO
			dkm
#else
			NULL
#endif
			);

#ifdef VIDEO
	fclose(dkm);
#endif
#ifdef AUDIO
	fclose(snd);
#endif
	fclose(frm);
	fclose(duk);
	return EXIT_SUCCESS;
}

/* local to file byte order */
inline uint32_t
ltof32(uint32_t val) {
	return (val >> 24) |
			((val & 0x00ff0000L) >> 8) |
			((val & 0x0000ff00L) << 8) |
			(val << 24);
}

/* file to local byte order */
inline uint32_t
ftol32(uint32_t val) {
	return (val >> 24) |
			((val & 0x00ff0000L) >> 8) |
			((val & 0x0000ff00L) << 8) |
			(val << 24);
}

/* local to file byte order */
inline uint16_t
ltof16(uint16_t val) {
	return (val >> 8) | (val << 8);
}

/* file to local byte order */
inline uint16_t
ftol16(uint16_t val) {
	return (val >> 8) | (val << 8);
}

int
read_data(char *buf, size_t size, FILE *file) {
	ssize_t numread;
	
	numread = fread(buf, size, 1, file);
	if (numread == 0 && ferror(file)) {
		perror("read header");
		exit(EXIT_FAILURE);
	}
	if ((size_t) numread != 1)
		return -1;  // eof
	return 0;
}

void
convert_duk(FILE *duk, FILE *frm, FILE *snd, FILE *dkm) {
	uint32_t framepos;
	frame_header header;
	
	while (1) {
		if (read_data((char *) &framepos, sizeof framepos, frm) == -1) {
			// eof
			break;
		}
		framepos = ftol32(framepos);
		
		if (fseek(duk, framepos, SEEK_SET) == -1) {
			fprintf(stderr, "Cannot seek in .duk file to pos %lu",
					(unsigned long) framepos);
			perror("");
			exit(EXIT_FAILURE);
		}
		
		if (read_data((char *) &header, sizeof (header), duk) == -1) {
			fprintf(stderr, "Input file too small.\n");
			exit(EXIT_FAILURE);
		}

#ifdef AUDIO
		convert_audio(duk, snd, ftol32(header.sndcnt));
#else
		fseek(duk, ftol32(header.sndcnt), SEEK_CUR);
#endif
		fseek(duk, 16 + 2, SEEK_CUR);
				// 16 bytes to skip AvlBsh header (whatever that is)
				// 2 bytes for ??? (maybe they should be skipped *after*
				// the audio even)
#ifdef VIDEO
		convert_video(duk, dkm, ftol32(header.vidcnt));
#else
		fseek(duk, ftol32(header.vidcnt), SEEK_CUR);
#endif
	}
#ifndef AUDIO
	(void) snd;
#endif
#ifndef VIDEO
	(void) dkm;
#endif
}

// This table is from one of the files that came with the original 3do source
// It's slightly different from the data used by MPlayer.
static int adpcm_step[89] = {
		0x7, 0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xF,
		0x10, 0x12, 0x13, 0x15, 0x17, 0x1A, 0x1C, 0x1F,
		0x22, 0x26, 0x29, 0x2E, 0x32, 0x37, 0x3D, 0x43,
		0x4A, 0x51, 0x59, 0x62, 0x6C, 0x76, 0x82, 0x8F,
		0x9E, 0xAD, 0xBF, 0xD2, 0xE7, 0xFE, 0x117, 0x133,
		0x152, 0x174, 0x199, 0x1C2, 0x1EF, 0x220, 0x256, 0x292,
		0x2D4, 0x31D, 0x36C, 0x3C4, 0x424, 0x48E, 0x503, 0x583,
		0x610, 0x6AC, 0x756, 0x812, 0x8E1, 0x9C4, 0xABE, 0xBD1,
		0xCFF, 0xE4C, 0xFBA, 0x114D, 0x1308, 0x14EF, 0x1707, 0x1954,
		0x1BDD, 0x1EA6, 0x21B7, 0x2516,
		0x28CB, 0x2CDF, 0x315C, 0x364C,
		0x3BBA, 0x41B2, 0x4844, 0x4F7E,
		0x5771, 0x6030, 0x69CE, 0x7463,
		0x7FFF
		};


#ifdef AUDIO

// *** BEGIN part copied from MPlayer ***
// (some little changes)

#if 0
// pertinent tables for IMA ADPCM
static int adpcm_step[89] = {
		7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
		19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
		50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
		130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
		337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
		876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
		2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
		5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
		15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
		};
#endif

static int adpcm_index[16] = {
		-1, -1, -1, -1, 2, 4, 6, 8,
		-1, -1, -1, -1, 2, 4, 6, 8
		};

// clamp a number between 0 and 88
#define CLAMP_0_TO_88(x)if (x < 0) x = 0; else if (x > 88) x = 88;

// clamp a number within a signed 16-bit range
#define CLAMP_S16(x) \
		if (x < -32768) \
			x = -32768; \
		else if (x > 32767) \
			x = 32767;

// sign extend a 16-bit value
#define SE_16BIT(x) \
		if (x & 0x8000) \
			x -= 0x10000;

static void
decode_nibbles(uint16_t *output,
		int output_size, int channels,
		int *predictor_l, int index_l,
		int *predictor_r, int index_r) {
	int step[2];
	int predictor[2] = {0, 0};
	int index[2];
	int diff;
	int i;
	int sign;
	int delta;
	int channel_number = 0;

	step[0] = adpcm_step[index_l];
	step[1] = adpcm_step[index_r];
	predictor[0] = *predictor_l;
	predictor[1] = *predictor_r;
	index[0] = index_l;
	index[1] = index_r;

	for (i = 0; i < output_size; i++)
	{
		delta = output[i];

		index[channel_number] += adpcm_index[delta];
		CLAMP_0_TO_88(index[channel_number]);

		sign = delta & 8;
		delta = delta & 7;

#if 0
		// fast approximation, used in most decoders
		diff = step[channel_number] >> 3;
		if (delta & 4) diff += step[channel_number];
		if (delta & 2) diff += step[channel_number] >> 1;
		if (delta & 1) diff += step[channel_number] >> 2;
#else
		// real thing
//		diff = ((signed)delta + 0.5) * step[channel_number] / 4;
		diff = (((delta << 1) + 1) * step[channel_number]) >> 3;
#endif

		if (sign)
			predictor[channel_number] -= diff;
		else
			predictor[channel_number] += diff;

		CLAMP_S16(predictor[channel_number]);
		output[i] = predictor[channel_number];
		step[channel_number] = adpcm_step[index[channel_number]];

		// toggle channel
		channel_number ^= channels - 1;
	}
	*predictor_l = predictor[0];
	*predictor_r = predictor[1];
}
// *** END part copied from MPlayer ***

void
convert_audio(FILE *duk, FILE *snd, size_t cnt) {
	static int predictorl = 0,
	           predictorr = 0;
			// Important that they are static. The predictor value
			// of the previous call should be reused.
	int32_t indexl, indexr;
	uint16_t numsamples;
	uint8_t *input, *inptr, *inend;
	audio_chunk *header;
	uint16_t *output, *outptr;
	size_t outputsize;

	input = alloca(cnt);
	if (read_data((char *) input, cnt, duk) == -1) {
		fprintf(stderr, "Input file too small.\n");
		exit(EXIT_FAILURE);
	}
	header = (audio_chunk *) input;

	if (header->magic != 0x7ff7) {
		fprintf(stderr, "Error: Magic value 0xf77f not present, expected at "
				"location %lu.\n", ftell(duk) - cnt +
				((char *) &header->magic - (char *) header));
		exit(EXIT_FAILURE);
	}
	numsamples = ftol16(header->numsamples);
	if (numsamples & 3) {
		fprintf(stderr, "Error: Number of samples not a multiple of 4.\n");
		exit(EXIT_FAILURE);
	}
	if ((ssize_t) cnt - (size_t) ((char *) &header->data - (char *) header) !=
			numsamples) {
		fprintf(stderr, "cnt doesn't match number of samples.\n"
				"Number of samples: %d\n"
				"cnt: %d\n", numsamples,
				cnt - (size_t) ((char *) &header->data - (char *) header));
		exit(EXIT_FAILURE);
	}
		
	outputsize = numsamples * 2 * sizeof (uint16_t);
	output = alloca(outputsize);
	outptr = output;
	indexl = ftol16(header->indexl);
	indexr = ftol16(header->indexr);

	inptr = &header->data;
	inend = input + cnt;
	while (inptr < inend) {
		*(outptr++) = *inptr >> 4;
		*(outptr++) = *inptr & 0x0f;
		inptr++;
	}
	decode_nibbles(output, numsamples * 2,
			2, &predictorl, indexl, &predictorr, indexr);

	fwrite(output, outputsize, 1, snd);
}

#endif  /* defined(AUDIO) */


#ifdef VIDEO

void
convert_video(FILE *duk, FILE *dkm, size_t cnt) {
	fseek(duk, cnt, SEEK_CUR);
	(void) duk;
	(void) dkm;
	(void) cnt;
}

#endif  /* defined(VIDEO) */

