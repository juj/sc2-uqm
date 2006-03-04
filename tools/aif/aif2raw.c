/*
 * aif to raw converter. By Serge van den Boom (svdb@stack.nl),
 * 20020816
 * Doesn't convert all .aif files in general, only AIFF-C, 16 bits
 * SDX2-compressed.
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aif2raw.h"

void convert_aif(FILE *in, FILE *out);
void write_data(const uint8_t *data, ssize_t size, int numChannels,
		FILE *out);

int
main(int argc, char *argv[]) {
	FILE *in, *out;
	
	if (argc != 3) {
		fprintf(stderr, "aif2wav <infile> <outfile>\n");
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

	convert_aif(in, out);

	fclose(in);
	fclose(out);
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

#if 0
/* local to file byte order */
inline void
ltofld(char *dest, long double val) {
}
#endif

/* file to local byte order */
inline long double
ftolld(const char *val) {
	long double result;
	((char *) &result)[0] = val[9];
	((char *) &result)[1] = val[8];
	((char *) &result)[2] = val[7];
	((char *) &result)[3] = val[6];
	((char *) &result)[4] = val[5];
	((char *) &result)[5] = val[4];
	((char *) &result)[6] = val[3];
	((char *) &result)[7] = val[2];
	((char *) &result)[8] = val[1];
	((char *) &result)[9] = val[0];
	((char *) &result)[10] = 0x00;
	((char *) &result)[11] = 0x00;
	return result;
}

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

struct aifc_Chunk *
read_chunk(FILE *in) {
	struct aifc_Chunk *result;
	size_t numread;

	result = malloc(sizeof (struct aifc_ChunkHeader));
	numread = fread(result, sizeof (struct aifc_ChunkHeader), 1, in);
	if (numread == 0) {
		free(result);
		if (ferror(in))
			fprintf(stderr, "Fread failed.\n");
		return NULL;
	}
	result = realloc(result, sizeof (struct aifc_ChunkHeader) +
			ftol32(result->ckSize));
	numread = fread(&result->ckData, ftol32(result->ckSize), 1, in);
	if (numread == 0) {
		free(result);
		if (ferror(in))
			fprintf(stderr, "Fread failed.\n");
		return NULL;
	}
	return result;
}

/* number to increase number of sections by if all preallocated
   sections are full */
#define CHUNK_NUM_INC  5

struct aifc_Chunk **
read_chunks(FILE *in) {
	struct aifc_Chunk **chunks;
	struct aifc_ContainerChunk *form;
	int maxchunks;
	int numchunks;

	/* The FORM chunk contains all the other chunks. */
	form = malloc(sizeof (struct aifc_ContainerChunk));
	read_data((char *) form, sizeof (struct aifc_ContainerChunk), in);
	if (form->formType != aifc_FormTypeAIFC) {
		fprintf(stderr, "File is not an AIFF-C file.\n");
		free(form);
		exit(EXIT_FAILURE);
	}
	free(form);

	numchunks = 0;
	maxchunks = 0;
	chunks = NULL;
	while (1) {
		if (numchunks >= maxchunks) {
			maxchunks += CHUNK_NUM_INC;
			chunks = realloc(chunks,
					(maxchunks + 1) * sizeof (struct aifc_Chunk *));
		}
		chunks[numchunks] = read_chunk(in);
		if (chunks[numchunks] == NULL)
			break;
		numchunks++;
	}
	chunks = realloc(chunks,
			(numchunks + 1) * sizeof (struct aifc_Chunk *));
	return chunks;
}

void
free_chunks(struct aifc_Chunk **chunks) {
	int i;
	
	for (i = 0; chunks[i] != NULL; i++)
		free(chunks[i]);
	free(chunks);
}

void
convert_aif(FILE *in, FILE *out) { 
	struct aifc_Chunk **chunks;
	struct aifc_FormatVersionChunk *fverChunk;
	struct aifc_ExtCommonChunk *commChunk;
	struct aifc_SoundDataChunk *ssndChunk;
	int i;

	fverChunk = NULL;
	commChunk = NULL;
	ssndChunk = NULL;
	chunks = read_chunks(in);
	for (i = 0; chunks[i] != NULL; i++) {
		if (chunks[i]->ckID == aifc_FormVersionID && fverChunk == NULL) {
			fverChunk = (struct aifc_FormatVersionChunk *) chunks[i];
		} else if (chunks[i]->ckID == aifc_CommonID && commChunk == NULL) {
			commChunk = (struct aifc_ExtCommonChunk *) chunks[i];
		} else if (chunks[i]->ckID == aifc_SoundDataID && ssndChunk == NULL) {
			ssndChunk = (struct aifc_SoundDataChunk *) chunks[i];
		}
	}
	if (fverChunk == NULL) {
		fprintf(stderr, "No format version chunk found.\n");
		exit(EXIT_FAILURE);
	}
	if (commChunk == NULL) {
		fprintf(stderr, "No common chunk found.\n");
		exit(EXIT_FAILURE);
	}
	if (ssndChunk == NULL) {
		fprintf(stderr, "No sound data chunk found.\n");
		exit(EXIT_FAILURE);
	}
	fprintf(stderr, "Number of channels:        %d\n",
			ftol16(commChunk->numChannels));
	fprintf(stderr, "Number of sample frames:   %d\n",
			ftol32(commChunk->numSampleFrames));
	fprintf(stderr, "Number of bits per sample: %d\n",
			ftol16(commChunk->sampleSize));
	fprintf(stderr, "Sample rate:               %.2LfHz\n",
			ftolld((char *) &commChunk->sampleRate));
	if (ftol16(commChunk->sampleSize) != 16) {
		fprintf(stderr, "Only 16-bits files are supported.\n");
		exit(EXIT_FAILURE);
	}
	if (commChunk->compressionType != aifc_CompressionTypeSDX2) {
		fprintf(stderr, "Only SDX2 compression is supported.\n");
		exit(EXIT_FAILURE);
	}
	write_data(ssndChunk->data + ftol32(ssndChunk->offset),
			ftol32(ssndChunk->ckSize) - ftol32(ssndChunk->offset),
			ftol16(commChunk->numChannels), out);
	free_chunks(chunks);
}

void
write_data(const uint8_t *data, ssize_t size, int numChannels, FILE *out) {
	const uint8_t *srcptr, *endptr;
	uint16_t *buffer, *dstptr;
	int i;
	int16_t *last;

	endptr = data + size;
	srcptr = data;
	buffer = malloc(size * sizeof (uint16_t));
	dstptr = buffer;
	last = malloc(numChannels * sizeof(int16_t));
	memset(last, '\0', numChannels * sizeof(uint16_t));
	while (srcptr < endptr) {
		for (i = 0; i < numChannels; i++) {
			if (*srcptr & 1) {
				*dstptr = last[i] +
						((*(int8_t *)srcptr * abs(*(int8_t *)srcptr)) << 1);
			} else
				*dstptr = (*(int8_t *)srcptr * abs(*(int8_t *)srcptr)) << 1;
			last[i] = *dstptr;
			dstptr++;
			srcptr++;
		}
	}
	fwrite(buffer, size * sizeof (uint16_t), 1, out);

	free(last);
	free(buffer);
}


