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

#include "huff.h"
#include "lztfb.h"
#include "../../shared/util.h"

#include <stdbool.h>
#include <stdlib.h>
#include <fcntl.h>
#include <getopt.h>
#include <string.h>
#include <sys/mman.h>


struct options {
	char *inFile;
	char *outFile;
	bool testOnly;
};

void usage(FILE *out);
void parse_arguments(int argc, char *argv[], struct options *opts);
bool testCompressed(const char *fileName);

int
main(int argc, char *argv[]) {
	void *buf;
	size_t size;
	bool compressed;
	FILE *out;

	struct options opts;
	parse_arguments(argc, argv, &opts);

	if (opts.testOnly) {
		return testCompressed(opts.inFile) ? EXIT_SUCCESS : EXIT_FAILURE;
	} else if (opts.outFile == NULL) {
		logError(false, "Either -o or -t needs to be specified.\n");
		exit(EXIT_FAILURE);
	}

	if (mmapOpen(opts.inFile, O_RDONLY, &buf, &size) == -1)
		fatal(true, "mmapOpen() failed.\n");

	if (((char *) buf)[0] == 6 && (((char *) buf)[1] & ~0x6) == 0) {
		compressed = true;
	} else {
		compressed = false;
		if (getU32BE((char *) buf + 2) != size - 6)
			fatal(false, "File '%s' is not in a recognised format.\n",
					opts.inFile);
	}

	out = fopen(opts.outFile, "w");
	if (out == NULL)
		fatal(true, "fopen() failed.\n");

	if (compressed) {
		LZTFB *lztfb = LZTFB_new(buf, size);
		if (lztfb == NULL)
			fatal(false, "LZTFB_new() failed.\n");

		if (LZTFB_output(lztfb, out) == -1)
			fatal(true, "LZTFB_output() failed.\n");

		LZTFB_delete(lztfb);
	} else {
		if (fwrite((char *) buf + 6, 1, size - 6, out) != size - 6)
			fatal(true, "fwrite() failed.\n");
	}

	(void) fclose(out);
	munmap(buf, size);

	(void) argc;
	(void) argv;
	return EXIT_SUCCESS;
}

void
usage(FILE *out) {
	fprintf(out, "Syntax:\n"
			"decomp -o <outfile> <infile>\n"
			"decomp -t <infile>\n"
			"\t-o  decompress to outfile\n"
			"\t-t  only test whether the file is compressed.\n"
			"\t    returns 0 if compressed, and 1 if not compressed\n");
}

void
parse_arguments(int argc, char *argv[], struct options *opts) {
	char ch;
	
	memset(opts, '\0', sizeof (struct options));
	while (1) {
		ch = getopt(argc, argv, "ho:t");
		if (ch == -1)
			break;
		switch(ch) {
			case 'o':
				opts->outFile = optarg;
				break;
			case '?':
			case 'h':
				usage(stdout);
				exit(EXIT_SUCCESS);
			case 't':
				opts->testOnly = true;
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

bool
testCompressed(const char *fileName) {
	FILE *file;

	file = fopen(fileName, "rb");
	if (file == NULL)
		return false;

	uint8_t buf[6];
	if (fread(buf, 1, 6, file) != 6) {
		fclose(file);
		return false;
	}
	fclose(file);
	
	return buf[0] == 6 && (buf[1] & ~0x6) == 0;
}


