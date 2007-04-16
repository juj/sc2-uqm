/*
 * .str file extractor
 * By Serge van den Boom (svdb@stack.nl), 20021027
 * The GPL applies.
 *
 * 
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <string.h>
#include <errno.h>

#include "unstr.h"


struct options {
	char *infile;
	char *outfile;
	char list;
	char print;
	char verbose;
	char sound;
};


index_header *readIndex(const uint8 *buf);
void printIndex(const index_header *h, const uint8 *buf, FILE *out);
void writeFiles(const index_header *h, const uint8 *data, const char *path,
		const char *prefix);
void parse_arguments(int argc, char *argv[], struct options *opts);
void listEntries(const index_header *h, const uint8 *buf, FILE *out);
void writeEntries(const index_header *h, const uint8 *buf, FILE *out);
void writeSoundEntries(const index_header *h, const uint8 *buf, const char* outmask);

const char *riffHdr = "RIFF";
const char *waveType = "WAVE";
const char *fmtHdr = "fmt ";
const char *dataHdr = "data";
#define FMTHDR_SIZE   0x10
#define DATAHDR_SIZE  0x08
#define WAVEHDR_SIZE  (4 + 4 + 4 + FMTHDR_SIZE + DATAHDR_SIZE)

int
main(int argc, char *argv[]) {
	int in;
	struct stat sb;
	uint8 *buf;
	index_header *h;
	struct options opts;
	char outnbuf[512];

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

	if (opts.sound && !opts.outfile) {
		char *pdot = strrchr(opts.infile, '.');
		opts.outfile = outnbuf;
		strcpy(outnbuf, opts.infile);
		if (pdot)
			outnbuf[pdot - opts.infile] = 0;
		
	}

	h = readIndex(buf);
	if (opts.print)
		printIndex(h, buf, stdout);

	if (opts.list) {
		listEntries(h, buf, stdout);
	}

	if (opts.sound && opts.outfile) {
		writeSoundEntries(h, buf, opts.outfile);

	} else if (opts.outfile) {
		FILE *out;
		out = fopen(opts.outfile, "wb+");
		if (out == NULL) {
			fprintf(stderr, "Could not open output file '%s': %s\n",
					opts.outfile, strerror(errno));
			exit(EXIT_FAILURE);
		}
		writeEntries(h, buf, out);
		fclose(out);
	}

	// freeIndex(h);
	munmap(buf, sb.st_size);
	return EXIT_SUCCESS;
}

void
usage() {
	fprintf(stderr,
			"unstr -a <infile>\n"
			"unstr -p <infile>\n"
			"unstr [-o <outfile>] <infile>\n"
			"unstr -s [-o <outfile>] <infile-sndtab>\n");
}

void
parse_arguments(int argc, char *argv[], struct options *opts) {
	char ch;
	
	memset(opts, '\0', sizeof (struct options));
	while (1) {
		ch = getopt(argc, argv, "ahlo:psv");
		if (ch == -1)
			break;
		switch(ch) {
			case 'l':
				opts->list = 1;
				break;
			case 'o':
				opts->outfile = optarg;
				break;
			case 'p':
				opts->print = 1;
				break;
			case 'v':
				opts->verbose = 1;
				break;
			case 's':
				opts->sound = 1;
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
	opts->infile = argv[0];
}

index_header *
readIndex(const uint8 *buf) {
	index_header *h;
	const uint8 *sizeptr;
	uint32 size, totsize;
	uint32 i;

	if ((uint32) MAKE_DWORD(buf[3], buf[2], buf[1], buf[0]) != 0xffffffff ||
			MAKE_WORD(buf[5], buf[4]) != 0x0000) {
		fprintf(stderr, "File is not a valid STRTAB file.\n");
		exit(EXIT_FAILURE);
	}

	h = malloc(sizeof (index_header));
	h->num_entries = MAKE_WORD(buf[7], buf[6]);
	h->entries = malloc(h->num_entries * sizeof (entry_desc));

	sizeptr = buf + 8;
	if (MAKE_DWORD(sizeptr[3], sizeptr[2], sizeptr[1], sizeptr[0])) {
		fprintf(stderr, "WARNING: Placeholder string entry hasn't got "
				"size 0, but %d.\n",
				MAKE_DWORD(sizeptr[3], sizeptr[2], sizeptr[1], sizeptr[0]));
	}
	sizeptr += 4;

	totsize = 0;
	for (i = 0; i < h->num_entries; i++) {
		size = MAKE_DWORD(sizeptr[3], sizeptr[2], sizeptr[1], sizeptr[0]);
		h->entries[i].size = size;
		h->entries[i].offset = totsize;
		sizeptr += 4;
		totsize += size;
	}
	return h;
}

void
printIndex(const index_header *h, const uint8 *buf, FILE *out) {
	fprintf(stderr, "-p option not implemented yet.\n");
	(void) h;
	(void) buf;
	(void) out;
}

void
listEntries(const index_header *h, const uint8 *buf, FILE *out) {
	fprintf(stderr, "-l option not implemented yet.\n");
	(void) h;
	(void) buf;
	(void) out;
}

void
writeEntries(const index_header *h, const uint8 *buf, FILE *out) {
	uint32 i;
	char *str;
	const uint8 *start;

	str = NULL;
	start = buf + 8 + (h->num_entries + 1) * 4;
	for (i = 0; i < h->num_entries; i++) {
		fprintf(out, "String #%d:\n", i + 1);
		str = realloc(str, h->entries[i].size + 1);
		str[h->entries[i].size] = '\0';
		strncpy(str, start + h->entries[i].offset, h->entries[i].size);
		fprintf(out, "%s\n\n", str);
	}
}

void
signSoundBuf(uint8 *buf, uint32 len)
{
	for (; len; ++buf, --len)
		*buf ^= 0x80;
}

void
writeFile_Int16_LE(FILE *f, uint16 val)
{
	fputc(val & 0xff, f);
	fputc((val >> 8) & 0xff, f);
}

void
writeFile_Int32_LE(FILE *f, uint32 val)
{
	writeFile_Int16_LE(f, val & 0xffff);
	writeFile_Int16_LE(f, (val >> 16) & 0xffff);
}

void
writeSoundEntries(const index_header *h, const uint8 *buf, const char* outmask) {
	FILE *out;
	uint32 i;
	char namebuf[512];
	const uint8 *start;
	uint8 *data;
	const uint8 *orgdata;
	uint32 datalen;
	uint32 wavelen;
	uint32 freq;

	start = buf + 8 + (h->num_entries + 1) * 4;
	data = NULL;
	for (i = 0; i < h->num_entries; i++) {
		sprintf(namebuf, "%s.%u.wav", outmask, i);
		
		out = fopen(namebuf, "wb");
		if (out == NULL) {
			fprintf(stderr, "Could not open output file '%s': %s\n",
					namebuf, strerror(errno));
			exit(EXIT_FAILURE);
		}

		datalen = h->entries[i].size - 2;
		data = realloc(data, datalen);
		orgdata = start + h->entries[i].offset;
		memcpy(data, orgdata, datalen);
		signSoundBuf(data, datalen);
                freq = MAKE_WORD(orgdata[datalen], orgdata[datalen + 1]);

		wavelen = datalen + WAVEHDR_SIZE;
		fputs(riffHdr, out);
		writeFile_Int32_LE(out, wavelen);
		fputs(waveType, out);

		fputs(fmtHdr, out);
		writeFile_Int32_LE(out, FMTHDR_SIZE);
		writeFile_Int16_LE(out, 1); // format
		writeFile_Int16_LE(out, 1); // channels
		writeFile_Int32_LE(out, freq); // samples/sec
		writeFile_Int32_LE(out, freq); // bytes/sec
		writeFile_Int16_LE(out, 1); // block align
		writeFile_Int16_LE(out, 8); // bits/sample

		fputs(dataHdr, out);
		writeFile_Int32_LE(out, datalen);

		fwrite(data, 1, datalen, out);
		fclose(out);
	}
	free(data);
}
