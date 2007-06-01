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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <sys/mman.h>
#include <getopt.h>
#include <unistd.h>

#include <png.h>

#include "unianm.h"
#include "../../shared/cbytesex.h"
#include "../../shared/util.h"

struct options {
	const char *inFile;
	const char *outDir;
	FILE *out;
	int resType;
	bool verbose;
	const char *paletteFile;
	int paletteNr;
};

void usage(FILE *out);
void parse_arguments(int argc, char *argv[], struct options *opts);
void verbose(const struct options *opts, const char *format, ...)
		__attribute__((format(printf, 2, 3)));
bool processFile(const struct options *opts, uint8_t *buf, size_t size,
		Anim **retAnim);
MasterPaletteColor *readPalette(struct options *opts);
bool writePngFile(const struct options *opts, const char *fileName,
		const Anim *anim, const Frame *frame,
		const MasterPaletteColor *palette);
int getDigitCount(uint32_t num);
bool outputFrames(struct options *opts, Anim *anim,
		const MasterPaletteColor *masterPalette);


int
main(int argc, char *argv[]) {
	void *buf = NULL;
	size_t size = 0;
	char *outDirBuf = NULL;
	uint8_t *paletteBuf = NULL;

	struct options opts;
	parse_arguments(argc, argv, &opts);

	if (opts.outDir != NULL && opts.paletteFile == NULL) {
		logError(false, "Need to specify a palette file.\n");
		goto err;
	}

	if (opts.outDir == NULL && opts.paletteFile != NULL) {
		fprintf(stderr, "Warning: -p is meaningless without -o.\n");
		goto err;
	}

	if (mmapOpen(opts.inFile, O_RDONLY, &buf, &size) == -1)
		fatal(true, "mmapOpen() failed.\n");

	if (size < 8 || memcmp(buf, "IANM", 4) != 0) {
		logError(false, "File '%s' is not an ianm file.\n",
				opts.inFile);
		goto err;
	}

	if (opts.outDir != NULL) {
		size_t outDirLen = strlen(opts.outDir);
		if (outDirLen == 0) {
			logError(false, "Invalid output directory.\n");
			goto err;
		}

		// Add a terminating '/', if necessary.
		if (opts.outDir[outDirLen - 1] != '/') {
			outDirBuf = malloc(outDirLen + 2);
			memcpy(outDirBuf, opts.outDir, outDirLen);
			outDirBuf[outDirLen] = '/';
			outDirBuf[outDirLen + 1] = '\0';
			opts.outDir = outDirBuf;
		}

		if (access(opts.outDir, W_OK) != 0) {
			logError(true, "Cannot write to output path.\n");
			goto err;
		}
	}

	Anim *anim;
	if (!processFile(&opts, (uint8_t *) buf, size, &anim))
		goto err;

	if (opts.outDir != NULL) {
		MasterPaletteColor *palette = readPalette(&opts);
		if (palette == NULL) {
			logError(false, "Failed to read palette.\n");
			goto err;
		}

		outputFrames(&opts, anim, palette);
	}
	
	munmap(buf, size);

	(void) argc;
	(void) argv;
	return EXIT_SUCCESS;

err:
	if (paletteBuf != NULL)
		free(paletteBuf);
	if (outDirBuf != NULL)
		free(outDirBuf);
	if (buf != NULL)
		munmap(buf, size);
	return EXIT_FAILURE;
}

void
usage(FILE *out) {
	fprintf(out, "Syntax:\t"
			"unianm [-v] [-c <n>] -p <palette> -t <n> -o <outdir> <infile>\n"
			"unianm [-v] [-c <n>] -t <n> <infile>\n"
			"\t-c  specifies the palette to use (0, 1, or 2 (default))\n"
			"\t-p  specifies the file to load the palette from\n"
			"\t-t  specifies the resource type (2, 3, 4, or 5)\n"
			"\t    (ignored for now)\n"
			"\t-v  verbose\n");
}

void
parse_arguments(int argc, char *argv[], struct options *opts) {
	char ch;
	
	memset(opts, '\0', sizeof (struct options));
	opts->out = stdout;
	opts->resType = 2;
	opts->paletteNr = 2;
	while (1) {
		ch = getopt(argc, argv, "c:ho:p:t:v");
		if (ch == -1)
			break;
		switch(ch) {
			case 'o':
				opts->outDir = optarg;
				break;
			case '?':
			case 'h':
				usage(stdout);
				exit(EXIT_SUCCESS);
			case 'c':
				if (optarg[0] < '0' || optarg[0] > 2 || optarg[1] != '\0') {
					logError(false, "Invalid argument to '-p'\n");
					exit(EXIT_FAILURE);
				}
				opts->paletteNr = optarg[0] - '\0';
				break;
			case 'p':
				opts->paletteFile = optarg;
				break;
			case 't':
				opts->resType = atoi(optarg);
				if (opts->resType < 2 || opts->resType > 5) {
					logError(false, "Invalid argument to '-t'\n");
					exit(EXIT_FAILURE);
				}
				break;
			case 'v':
				opts->verbose = true;
				break;
			default:
				usage(stderr);
				exit(EXIT_FAILURE);
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 1) {
		usage(stderr);
		exit(EXIT_FAILURE);
	}
	opts->inFile = argv[0];
}

void
verbose(const struct options *opts, const char *format, ...) {
	if (!opts->verbose)
		return;

	va_list args;
	va_start(args, format);
	vfprintf(opts->out, format, args);
	va_end(args);
}

// Pre: size >= 8
bool
processFile(const struct options *opts, uint8_t *buf, size_t bufLen,
		Anim **retAnim) {
	assert(bufLen >= 8);
	uint8_t *end = buf + bufLen;
	uint8_t *bufPtr = buf + 8;
	
	Anim *anim = malloc(sizeof (Anim));
	verbose(opts, "0x00000000  \"IANM\"\n");

	anim->frameCount = getU16BE(buf + 4);
	verbose(opts, "0x00000004  Number of frames: %d\n", anim->frameCount);
	anim->bpp = getU16BE(buf + 6) & 0xff;
	verbose(opts, "0x00000006  Bits per pixel:   %d\n", anim->bpp);

	anim->frames = malloc(anim->frameCount * sizeof (Frame));

	for (uint16_t frameI = 0; frameI < anim->frameCount; frameI++) {
		if (bufPtr + 4 > end)
			goto fileTooSmall;

		Frame *frame = &anim->frames[frameI];
		frame->width = getU16BE(bufPtr);
		frame->height = getU16BE(bufPtr + 2);
		verbose(opts, "0x%08x  Frame %2d: %3d x %3d pixels\n",
				bufPtr - buf, frameI, frame->width, frame->height);
		bufPtr += 4;
	}
	
	for (uint16_t frameI = 0; frameI < anim->frameCount; frameI++) {
		Frame *frame = &anim->frames[frameI];
		
		if (bufPtr + 6 > end)
			goto fileTooSmall;

		verbose(opts, "0x%08x  Frame %d:\n",
				bufPtr - buf, frameI);
		frame->hotX = getS16BE(bufPtr);
		frame->hotY = getS16BE(bufPtr + 2);
		verbose(opts, "0x%08x    Hotspot: (%3d, %3d)\n", bufPtr - buf,
				frame->hotX, frame->hotY);
		bufPtr += 4;
		frame->hasPalette = getU8(bufPtr) != 0;
		verbose(opts, "0x%08x    %s (0x%02x)\n", bufPtr - buf,
				frame->hasPalette ? "paletted" : "not paletted",
				getU8(bufPtr));
		bufPtr++;
		frame->transIndex = getU8(bufPtr) & 0x0f;
		verbose(opts, "0x%08x    Transparent palette index: 0x%02x\n",
				bufPtr - buf, frame->transIndex);
		bufPtr++;

		if (frame->hasPalette) {
			if (bufPtr + 0x30 > end)
				goto fileTooSmall;
			for (int paletteI = 0; paletteI < 3; paletteI++) {
				frame->palettes[paletteI] = bufPtr;
				verbose(opts, "0x%08x    Palette %d:", bufPtr - buf,
						paletteI);
				for (int i = 0; i < 0x10; i++)
					verbose(opts, " %02x", bufPtr[i]);
				verbose(opts, "\n");
				bufPtr += 0x10;
			}
		} else {
			// No palette with this frame. Copy from the previous frame,
			// if there is one.
			if (frameI == 0) {
				for (int paletteI = 0; paletteI < 3; paletteI++)
					frame->palettes[paletteI] = NULL;
			} else {
				Frame *lastFrame = &anim->frames[frameI - 1];
				for (int paletteI = 0; paletteI < 3; paletteI++)
					frame->palettes[paletteI] =
							lastFrame->palettes[paletteI];
				frame->hasPalette = lastFrame->hasPalette;
			}
		}

		uint8_t bitDepth = 8 / anim->bpp;
		frame->bytesPerLine = ((frame->width + (bitDepth - 1)) / bitDepth);
		frame->pixelDataSize = frame->bytesPerLine * frame->height;
		frame->pixelData = bufPtr;

		if (bufPtr + frame->pixelDataSize > end)
			goto fileTooSmall;
		verbose(opts, "0x%08x    Pixel data: %d bytes\n", bufPtr - buf,
				frame->pixelDataSize);
		bufPtr += frame->pixelDataSize;
	}
	
	if ((size_t) (bufPtr - buf) != bufLen) {
		fprintf(stderr, "\nWarning: Extra data at the end of the file:\n"
				"\tdata length: %d bytes\n\tfile length: %d bytes\n",
				bufPtr - buf, bufLen);
	}

	*retAnim = anim;
	return true;

fileTooSmall:
	logError(false, "\nInput file is too small.\n");
	free(anim->frames);
	free(anim);
	return false;
}

MasterPaletteColor *
readPalette(struct options *opts) {
	FILE *file = NULL;
	uint32_t *buf = malloc(0x300);

	file = fopen(opts->paletteFile, "rb");
	if (file == NULL) {
		logError(true, "Could not open palette file.\n");
		goto err;
	}

	if (fread(buf, 1, 0x300, file) != 0x300) {
		logError(ferror(file), "Could not read palette file.\n");
		goto err;
	}

	fclose(file);
	return (MasterPaletteColor *) buf;

err:
	if (file != NULL)
		fclose(file);
	return NULL;
}

bool
writePngFile(const struct options *opts, const char *fileName,
		const Anim *anim, const Frame *frame,
		const MasterPaletteColor *masterPalette) {
	FILE *file = NULL;

	file = fopen(fileName, "wb");
	if (file == NULL) {
		logError(true, "Could not open file '%s' for writing.\n", fileName);
		return false;
	}

	png_structp png_ptr;
	png_infop info_ptr;
	
	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
			(png_voidp) NULL /* user_error_ptr */,
			NULL /* user_error_fn */, NULL /* user_warning_fn */);
	if (!png_ptr) {
		fprintf(stderr, "png_create_write_struct failed.\n");
		goto err;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		png_destroy_write_struct(&png_ptr, (png_infopp) NULL);
		fprintf(stderr, "png_create_info_struct failed.\n");
		goto err;
	}

	if (setjmp(png_jmpbuf(png_ptr))) {
		fprintf(stderr, "png error.\n");
		png_destroy_write_struct(&png_ptr, &info_ptr);
		goto err;
	}

	png_init_io(png_ptr, file);
	png_set_IHDR(png_ptr, info_ptr, frame->width, frame->height,
			anim->bpp /* bit_depth per channel */, PNG_COLOR_TYPE_PALETTE,
			PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
			PNG_FILTER_TYPE_DEFAULT);

	png_set_compression_level(png_ptr, 9);

	png_set_oFFs(png_ptr, info_ptr, -frame->hotX, -frame->hotY,
			PNG_OFFSET_PIXEL);

	png_color_8 sig_bit;
	sig_bit.red = 8;
	sig_bit.green = 8;
	sig_bit.blue = 8;
	png_set_sBIT(png_ptr, info_ptr, &sig_bit);

	png_color pngPalette[16];
	if (frame->hasPalette) {
		uint8_t *palette = frame->palettes[opts->paletteNr];
		for (int i = 0; i < 16; i++) {
			uint8_t entry = palette[i];
			pngPalette[i].red   = masterPalette[entry].red;
			pngPalette[i].green = masterPalette[entry].green;
			pngPalette[i].blue  = masterPalette[entry].blue;
		}
	} else {
		for (int i = 0; i < 16; i++) {
			pngPalette[i].red   = masterPalette[i].red;
			pngPalette[i].green = masterPalette[i].green;
			pngPalette[i].blue  = masterPalette[i].blue;
		}
	}
	png_set_PLTE(png_ptr, info_ptr, pngPalette, 16);

	{
		// Generate transparency chunk
		// for indexed PNGs, tRNS chunk contains an array of alpha values
		// corresponding to palette entries
		png_byte trans[0x100];
		// set all palette alpha values to 0xff (fully opaque) initially
		memset(trans, 0xff, 0x100 * sizeof (png_byte));
		if (frame->transIndex < 0x10)
			trans[frame->transIndex] = 0;
					// the only one
		// only need to write out upto and including transparentPixel
		png_set_tRNS(png_ptr, info_ptr, trans, frame->transIndex + 1, NULL);
	}

	png_write_info(png_ptr, info_ptr);

	{
		uint8_t *linePtr = frame->pixelData;
		for (uint32_t lineI = 0; lineI < frame->height; lineI++) {
			png_write_row(png_ptr, (png_byte *) linePtr);
			linePtr += frame->bytesPerLine;
		}
	}

	png_write_end(png_ptr, info_ptr);
	png_destroy_write_struct(&png_ptr, &info_ptr);

	fclose(file);
	return true;

err:
	if (file != NULL)
		fclose(file);
	return false;
}

int
getDigitCount(uint32_t num) {
	if (num == 0)
		return 1;

	int result = 0;
	while (num > 0) {
		num /= 10;
		result++;
	}
	return result;
}

// Pre: opts.outDir already has a terminating '/'.
bool
outputFrames(struct options *opts, Anim *anim,
		const MasterPaletteColor *palette) {
	if (anim->frameCount == 0) {
		fprintf(stderr, "Warning: no frames in file '%s'.\n",
				opts->inFile);
		return true;
	}

	// Get just the last component of opts->inFile.
	const char *inFileName = strrchr(opts->inFile, '/');
	if (inFileName == NULL) {
		inFileName = opts->inFile;
	} else
		inFileName++;

	size_t outDirLen = strlen(opts->outDir);
	size_t inFileLen = strlen(inFileName);
	size_t maxPathLen = outDirLen + inFileLen + sizeof ".65535.png";
	char path[maxPathLen];
	
	char *pathPtr = path +
			sprintf(path, "%s%s.", opts->outDir, inFileName);
	int digitCount = getDigitCount(anim->frameCount - 1);

	for (uint16_t frameI = 0; frameI < anim->frameCount; frameI++) {
		Frame *frame = &anim->frames[frameI];

		sprintf(pathPtr, "%0*d.png", digitCount, frameI);

		if (!writePngFile(opts, path, anim, frame, palette))
			logError(false, "Writing file '%s' failed.\n", path);
	}

	return true;
}

	
