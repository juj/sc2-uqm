/*
 * .str file extractor for Star Control 1 files
 *
 * By Serge van den Boom (svdb@stack.nl), 2007-05-31
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "../../shared/cbytesex.h"
#include "../../shared/util.h"


struct options {
	char *inFile;
	char *outFile;
	bool cStyle;
};


void usage(void);
void parse_arguments(int argc, char *argv[], struct options *opts);
bool outputString(const struct options *opts, uint8_t *str, size_t len,
		FILE *out);
bool outputStrings(const struct options *opts, uint8_t *buf, size_t size,
		FILE *out);

int
main(int argc, char *argv[]) {
	struct options opts;
	FILE *out = NULL;
	void *buf = NULL;
	size_t size;

	parse_arguments(argc, argv, &opts);

	if (mmapOpen(opts.inFile, O_RDONLY, &buf, &size) == -1)
		fatal(true, "mmapOpen() failed.\n");

	if (opts.outFile == NULL) {
		out = stdout;
	} else {
		out = fopen(opts.outFile, "wb");
		if (out == NULL) {
			logError(true, "Could not open output file '%s'.\n",
					opts.outFile);
			goto err;
		}
	}

	if (!outputStrings(&opts, buf, size, out))
		goto err;

	if (opts.outFile != NULL)
		fclose(out);
	if (buf != NULL)
		munmap(buf, size);
	return EXIT_SUCCESS;

err:
	if (opts.outFile != NULL && out != NULL)
		fclose(out);
	if (buf != NULL)
		munmap(buf, size);
	return EXIT_FAILURE;
}

void
usage(void) {
	fprintf(stderr, "unstr [-c] [-o <outfile>] <infile>\n"
			"\t-c  outout C-style strings\n");
}

void
parse_arguments(int argc, char *argv[], struct options *opts) {
	char ch;
	
	memset(opts, '\0', sizeof (struct options));
	while (1) {
		ch = getopt(argc, argv, "cho:");
		if (ch == -1)
			break;
		switch(ch) {
			case 'c':
				opts->cStyle = true;
			case 'o':
				opts->outFile = optarg;
				break;
			case '?':
			case 'h':
			default:
				usage();
				exit(EXIT_FAILURE);
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 1) {
		usage();
		exit(EXIT_FAILURE);
	}
	opts->inFile = argv[0];
}

bool
outputString(const struct options *opts, uint8_t *str, size_t len,
		FILE *out) {
	if (!opts->cStyle) {
		size_t writeResult = fwrite(str, 1, len, out);
		if (writeResult != len) {
			logError(true, "fwrite() failed\n");
			return false;
		}
		fputc('\n', out);
		return true;
	}

	fputs("\t\"", out);
	uint8_t *end = str + len;
	for (; str < end; str++) {
		switch (*str) {
			case '\\':
				fputs("\\\\", out);
				break;
			case '"':
				fputs("\\\"", out);
				break;
			case '\n':
				fputs("\\n", out);
				break;
			case '\r':
				fputs("\\r", out);
				break;
			case '\t':
				fputs("\\t", out);
				break;
			default:
				if (*str >= 0x20 && *str < 0x7f) {
					// Print literal character.
					fputc(*str, out);
				} else {
					// Print character as hex.
					fprintf(out, "\\x%02x", *str);
				}
				break;
		}
	}
	fputs("\",\n", out);

	return true;
}

bool
outputStrings(const struct options *opts, uint8_t *buf, size_t size,
		FILE *out) {
	uint8_t *lenPtr = buf;
	uint8_t *dataPtr = buf;

	if ((size_t) (lenPtr - buf + 4) > size)
		goto fileTooSmall;
	
	size_t numStrings = getU16BE(lenPtr);
	// buf[2] and buf[3] are always 0
	lenPtr += 4;
	dataPtr = lenPtr + 2 * numStrings;

	if (opts->cStyle)
		fprintf(out, "(char *[%d]) {\n", numStrings);

	for (size_t strI = 0; strI < numStrings; strI++) {
		if ((size_t) (lenPtr - buf + 2) > size)
			goto fileTooSmall;
		size_t len = getU16BE(lenPtr);
		if (dataPtr - buf + len > size)
			goto fileTooSmall;

		(void) outputString(opts, dataPtr, len, out);

		lenPtr += 2;
		dataPtr += len;
	}
	
	if (opts->cStyle)
		fputs("}\n", out);

	return true;

fileTooSmall:
	logError(false, "\nInput file is too small.\n");
	return false;
}



