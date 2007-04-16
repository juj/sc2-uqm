/*
 * DOS SC2 string extractor.
 * 
 * By Serge van den Boom (svdb@stack.nl), 20070415
 *
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

// The DOS strings are contained within a dynamically loaded piece of 16
// bits x86 code. These code files also contain a table with starting
// offsets of the various strings.
// This program is very simple; it takes the filename of the resource,
// and will then search backwards from the end of the file to the start
// off the string table. It will then print the strings in a human-readable
// format.
//
// To use this program, you need to take the following steps:
// - use unpkg to extract the various conversation packages, given
//   starcon.pkg, con1.pkg, and con2.pkg. (starcon.pkg contains the index;
//   the actual data comes from con1.pkg and con2.pkg)
// - use unpkg on the extracted con packages. The con packages all have
//   resource type 6 (the last two numbers of the resource id presented as
//   a string are "06"). This produces some files; the files with resource
//   id 00200007 and 00400007 are the ones we're interested in.
// - on these files, run "lz" from the "dos-ani" directory, to decompress
//   them.
// - run this program, unstr, on the decompressed files.


#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
//#include <sys/param.h>
#include <string.h>


typedef uint8_t uint8;
typedef uint16_t uint16;
typedef int32_t int32;

#define MAKE_WORD(x1, x2) (((x2) << 8) | (x1))



struct options {
	char *infile;
	off_t startOffset;
	off_t endOffset;
	char verbose;
};


bool testStringTable(const uint8 *data, const uint8 *start,
		const uint8 *end);
void findStringTable(const uint8 *data, size_t len, off_t *tableStart,
		off_t *tableEnd);
void parse_arguments(int argc, char *argv[], struct options *opts);
void dumpStrings(const uint8 *data, off_t startOff, off_t endOff);


int
main(int argc, char *argv[]) {
	int in;
	struct stat sb;
	uint8 *buf;
	struct options opts;

	parse_arguments(argc, argv, &opts);

	in = open(opts.infile, O_RDONLY);
	if (in == -1) {
		fprintf(stderr, "Error: Could not open file %s: %s\n", opts.infile,
				strerror(errno));
		return EXIT_FAILURE;
	}
	
	if (fstat(in, &sb) == -1) {
		perror("stat() failed");
		return EXIT_FAILURE;
	}
	
	buf = mmap(NULL, sb.st_size, PROT_READ, MAP_SHARED, in, 0);
	if (buf == MAP_FAILED) {
		perror("mmap() failed");
		return EXIT_FAILURE;
	}
	close(in);

	if (opts.startOffset == (off_t) -1) {
		findStringTable(buf, sb.st_size, &opts.startOffset,
				&opts.endOffset);
		if (opts.verbose) {
			fprintf(stderr, "String table found at offset %u "
					"(0x%08x) to %u (0x%08x).\n",
					(unsigned int) opts.startOffset,
					(unsigned int) opts.startOffset,
					(unsigned int) opts.endOffset,
					(unsigned int) opts.endOffset);
		}
	} else {
		if (opts.endOffset == (off_t) -1)
			opts.endOffset = sb.st_size;

		if (opts.startOffset > opts.endOffset ||
				opts.endOffset > sb.st_size) {
			fprintf(stderr, "Invalid offset.\n");
			exit(EXIT_FAILURE);
		}
	}

	dumpStrings(buf, opts.startOffset, opts.endOffset);
	
	munmap(buf, sb.st_size);
	return EXIT_SUCCESS;
}

void
usage() {
	fprintf(stderr, "unstr [-s <startOffset> [-e <endOffset>]] <infile>\n"
			"\t-s  start of the string table. If not specified, unstr tries\n"
			"\t    to locate it itself.\n");
}

void
parse_arguments(int argc, char *argv[], struct options *opts) {
	char ch;
	
	memset(opts, '\0', sizeof (struct options));
	opts->startOffset = (off_t) -1;
	opts->endOffset = (off_t) -1;
	while (1) {
		ch = getopt(argc, argv, "s:v");
		if (ch == -1)
			break;
		switch(ch) {
			case 'v':
				opts->verbose = 1;
				break;
			case 'e': {
				char *strEnd;
				long value;

				errno = 0;
				value = strtoul(optarg, &strEnd, 0);
				if (errno != 0 || *optarg == '\0' || *strEnd != '\0') {
					fprintf(stderr, "Invalid argument to '-e'.\n");
					exit(EXIT_FAILURE);
				}
				opts->endOffset = (off_t) value;
				break;
			}
			case 's': {
				char *strEnd;
				long value;

				errno = 0;
				value = strtoul(optarg, &strEnd, 0);
				if (errno != 0 || *optarg == '\0' || *strEnd != '\0') {
					fprintf(stderr, "Invalid argument to '-s'.\n");
					exit(EXIT_FAILURE);
				}
				opts->startOffset = (off_t) value;
				break;
			}
			case '?':
			case 'h':
			default:
				usage();
				exit(EXIT_SUCCESS);
		}
	}

	if (opts->startOffset == (off_t) -1 && opts->endOffset != (off_t) -1) {
		fprintf(stderr, "'-e' is only valid in combination with '-s'.\n");
		exit(EXIT_FAILURE);
	}

	argc -= optind;
	argv += optind;
	if (argc != 1) {
		usage();
		exit(EXIT_FAILURE);
	}
	opts->infile = argv[0];
}

// Pre: start and end are aligned on 2 bytes.
bool
testStringTable(const uint8 *data, const uint8 *start, const uint8 *end) {
	const uint8 *ptr;
	off_t tableStartOff = start - data;
	int32 lastWord;

	lastWord = -1;
	for (ptr = start; ptr < end; ptr += 2) {
		uint16 word = MAKE_WORD(ptr[0], ptr[1]);

		// Check the range.
		if (word >= tableStartOff)
			return false;

		// Offsets must be increasing.
		if (word <= lastWord)
			return false;

		// XXX: I could whether the strings end where the next one starts,
		//      but this scanning is good enough.

		lastWord = word;
	}
	return true;
}

void
findStringTable(const uint8 *data, size_t len, off_t *tableStart,
		off_t *tableEnd) {
	off_t i = (off_t) len;

	// Table is aligned on 2 bytes.
	if (i % 2 == 1)
		i--;

	while (i >= 2) {
		uint16 word;
		const uint8 *ptr;
		i -= 2;
	
		// Check whether the word points to someplace before &data[i].
		word = MAKE_WORD(data[i], data[i + 1]);
		if (word >= i)
			continue;

		// There should be one string starting at &data[word], and ending
		// just before the start of the string table.
		ptr = memchr(&data[word], '\0', i - word);
		if (ptr == NULL)
			continue;
		// If the pointer to the end of the string is an even position,
		// the next byte must be '\0' too.
		if (((uintptr_t) ptr % 2) == 0) {
			ptr++;
			if (*ptr != '\0')
				continue;
		}
		ptr++;
		// Ptr now points past the end of the string. The start of the
		// string table could be here.

		if (testStringTable(data, ptr, &data[i + 2])) {
			*tableStart = ptr - data;
			*tableEnd = i + 2;
			return;
		}
	}

	fprintf(stderr, "Could not find the start of the string table.\n");
	exit(EXIT_FAILURE);
}

// Pre: startOff and endOff fall within the range of data.
void
dumpStrings(const uint8 *data, off_t startOff, off_t endOff) {
	const uint8 *start = data + startOff;
	const uint8 *end = data + endOff;
	const uint8 *maxStr = start;
			// The strings come directly before the string table.
	size_t i;

	for (i = 0; start < end; start += 2, i++) {
		uint16 strOff = MAKE_WORD(start[0], start[1]);
		size_t maxLen;
		const char *strEnd;

		if (&data[strOff] >= maxStr) {
			fprintf(stderr, "Invalid string table entry on offset %u.\n",
					(uint16) (start - data));
			continue;
		}

		maxLen = (size_t) (maxStr - &data[strOff]);
		strEnd = memchr(&data[strOff], '\0', maxLen);
		if (strEnd == NULL) {
			fprintf(stderr, "Invalid string table entry on offset %u.\n",
					(uint16) (start - data));
			continue;
		}

		printf("#%d\n%s\n", i, (const char *) &data[strOff]);
	}
}


