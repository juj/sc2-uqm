/*
 *  Print a resource ID. By Serge van den Boom (svdb@stack.nl)
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

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include <inttypes.h>

#include "../shared/util.h"

typedef uint32_t RESOURCE;
typedef uint16_t RES_PACKAGE;
typedef uint16_t RES_INSTANCE;
typedef uint16_t RES_TYPE;

#define TYPE_BITS 8
#define INSTANCE_BITS 13
#define PACKAGE_BITS 11

#define GET_TYPE(res) \
		((RES_TYPE)(res) & (RES_TYPE)((1 << TYPE_BITS) - 1))
#define GET_INSTANCE(res) \
		((RES_INSTANCE)((res) >> TYPE_BITS) & ((1 << INSTANCE_BITS) - 1))
#define GET_PACKAGE(res) \
		((RES_PACKAGE)((res) >> (TYPE_BITS + INSTANCE_BITS)) & \
		((1 << PACKAGE_BITS) - 1))
#define MAKE_RESOURCE(p,t,i) \
		(((RESOURCE)(p) << (TYPE_BITS + INSTANCE_BITS)) | \
		((RESOURCE)(i) << TYPE_BITS) | \
		((RESOURCE)(t)))

void usage(FILE *out);
int printResource(FILE *out, const char *format, RESOURCE res);

int
main(int argc, char *argv[]) {
	char ch;
	RESOURCE res = 0;
	const char *format = "RESID=0x%08R  PACKAGE=%p (0x%04P)  "
			"INSTANCE=%i (0x%04I)  TYPE=%t (0x%02T)\n";
	
	for (;;) {
		ch = getopt(argc, argv, "f:hi:p:r:t:");
		if (ch == -1)
			break;
		switch(ch) {
			case 'f':
				// format
				format = optarg;
				break;
			case 'r': {
				// resource
				uint32_t temp;
				if (parseU32(optarg, 0, &temp) == -1) {
					fprintf(stderr, "Invalid resource for -r.\n");
					exit(EXIT_FAILURE);
				}
				res = temp;
				break;
			}
			case 'p': {
				// package
				uint16_t temp;
				if (parseU16(optarg, 0, &temp) == -1) {
					fprintf(stderr, "Invalid package for -p.\n");
					exit(EXIT_FAILURE);
				}
				res = MAKE_RESOURCE(temp, GET_TYPE(res), GET_INSTANCE(res));
				break;
			}
			case 'i': {
				// instance
				uint16_t temp;
				if (parseU16(optarg, 0, &temp) == -1) {
					fprintf(stderr, "Invalid instance for -i.\n");
					exit(EXIT_FAILURE);
				}
				res = MAKE_RESOURCE(GET_PACKAGE(res), GET_TYPE(res), temp);
				break;
			}
			case 't': {
				// type
				uint16_t temp;
				if (parseU16(optarg, 0, &temp) == -1) {
					fprintf(stderr, "Invalid type for -t.\n");
					exit(EXIT_FAILURE);
				}
				res = MAKE_RESOURCE(GET_PACKAGE(res), temp, GET_INSTANCE(res));
				break;
			}
			case '?':
			case 'h':
				usage(stdout);
				return EXIT_SUCCESS;
			default:
				usage(stderr);
				exit(EXIT_FAILURE);
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 0) {
		fprintf(stderr, "Error: Extra arguments passed.\n");
		usage(stderr);
		exit(EXIT_FAILURE);
	}

	(void) printResource(stdout, format, res);

	return EXIT_SUCCESS;
}

void
usage(FILE *out) {
	fprintf(out, "Usage: resid [-f format] [-r resource] [-p package] [-i"
			"instance] [-t type]\n"
			"\t-f  Format to print the resource:\n"
			"\t    %%r - resource (decimal)\n"
			"\t    %%R - resource (hexadecimal)\n"
			"\t    %%i - instance (decimal)\n"
			"\t    %%I - instance (hexadecimal)\n"
			"\t    %%t - type (decimal)\n"
			"\t    %%T - type (hexadecimal)\n"
			"\t    %%%% - literal '%%'\n"
			"\t-r  resource to print\n"
			"\t-p  package part of the resource to print\n"
			"\t-i  instance part of the resource to print\n"
			"\t-t  type part of the resource to print\n");
}

int
printResource(FILE *out, const char *format, RESOURCE res) {
	int result = 0;

	const char *ptr = format;
	for (;;) {
		const char *lastEnd = ptr;
		ptr += strcspn(ptr, "%\\");

		if (lastEnd != ptr) {
			// Print literal text skipped since the last % pattern.
			result += fwrite(lastEnd, 1, ptr - lastEnd, out);
		}

		if (*ptr == '\0')
			break;

		if (*ptr == '\\') {
			// Escape a character.
			ptr++;
			if (*ptr == '\0')
				break;

			char ch = *ptr;
			switch (ch) {
				case 'n':
					ch = '\n';
					break;
				case 'r':
					ch = '\r';
					break;
				case 't':
					ch = '\t';
					break;
				default:
					break;
			}
			fputc(ch, out);
			result++;
		}

		// ptr == '%"
		ptr++;

		if (*ptr == '%') {
			// Literal %.
			fputc('%', out);
			ptr++;
			result++;
			continue;
		}

		// Parse the width
		long width = 0;
		char *widthEnd;
		width = strtol(ptr, &widthEnd, 10);
		ptr = widthEnd;

		switch (*ptr) {
			case 'r':
				result += fprintf(out, "%0*" PRIu32, (int) width, res);
				break;
			case 'R':
				result += fprintf(out, "%0*" PRIx32, (int) width, res);
				break;
			case 'p':
				result += fprintf(out, "%0*" PRIu16, (int) width,
						GET_PACKAGE(res));
				break;
			case 'P':
				result += fprintf(out, "%0*" PRIx16, (int) width,
						GET_PACKAGE(res));
				break;
			case 'i':
				result += fprintf(out, "%0*" PRIu16, (int) width,
						GET_INSTANCE(res));
				break;
			case 'I':
				result += fprintf(out, "%0*" PRIx16, (int) width,
						GET_INSTANCE(res));
				break;
			case 't':
				result += fprintf(out, "%0*" PRIu16, (int) width,
						GET_TYPE(res));
				break;
			case 'T':
				result += fprintf(out, "%0*" PRIx16, (int) width,
						GET_TYPE(res));
				break;
			case '\0':
				continue;
			default:
				fputc(*ptr, out);
				result++;
				break;
		}
		ptr++;
	}

	return 0;
}

