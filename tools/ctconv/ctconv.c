/**
 * 3DO SC2 colortab files converter (.ct)
 * By Alex Volkov (codepro@usa.net), 20050202
 *
 * GPL applies
 *
**/

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <malloc.h>
#ifdef WIN32
#  include <getopt.h>
#else
#  include <unistd.h>
#endif
#include "ctres.h"

int opt_verbose = 0;
char* opt_inname = 0;
char* opt_outname = 0;
char* opt_actfile = 0;

typedef struct
{
	uint32_t size;
	uint32_t ccluts;
	clut_tab_t* data;
} clut_entry_t;

typedef struct
{
	uint32_t size;
	pal_tab_t* data;
} pal_entry_t;

int ctabs = 0;
clut_entry_t* clut_entries = 0;
pal_entry_t* pal_entries = 0;

// internals
void verbose(const char* fmt, ...);

void verbose(const char* fmt, ...)
{
	va_list args;
	
	if (!opt_verbose)
		return;
	
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
}

uint8_t fread_8(FILE *f)
{
	return (uint8_t) fgetc(f);
}

uint16_t fread_16_BE(FILE *f)
{
	return (fread_8(f) << 8) | fread_8(f);
}

uint32_t fread_32_BE(FILE *f)
{
	return (fread_16_BE(f) << 16) | fread_16_BE(f);
}

void fwrite_16_BE(FILE *f, uint16_t val)
{
	fputc((val >> 8) & 0xff, f);
	fputc(val & 0xff, f);
}

void fwrite_32_BE(FILE *f, uint32_t val)
{
	fwrite_16_BE(f, (uint16_t) ((val >> 16) & 0xffff));
	fwrite_16_BE(f, (uint16_t) (val & 0xffff));
}

int read_head(FILE* f)
{
	uint32_t magic;
	int v;
	int i;

	magic = fread_32_BE(f);
	if (magic != CTTAB_MAGIC)
	{
		verbose("Invalid magic found: 0x%08x\n", magic);
		return -10;
	}

	v = fread_16_BE(f);
	if (v != 0)
	{
		verbose("Do not know what to do with value 0x%04x in the header\n", v);
		return -11;
	}

	ctabs = fread_16_BE(f);
	verbose("CLUT tables: %u\n", ctabs);

	v = fread_32_BE(f);
	if (v != 0)
	{
		verbose("Do not know what to do with offset 0x%08x in the header\n", v);
		return -11;
	}

	clut_entries = malloc(sizeof(clut_entries[0]) * ctabs);
	if (!clut_entries)
	{
		verbose("Not enough memory to store entries\n");
		return -12;
	}
	memset(clut_entries, 0, sizeof(clut_entries[0]) * ctabs);

	for (i = 0; i < ctabs; ++i)
	{
		clut_entries[i].size = fread_32_BE(f);
	}

	return 0;
}

int write_head(FILE* f)
{
	int i;

	fwrite_32_BE(f, CTTAB_MAGIC); // magic
	fwrite_16_BE(f, 0);      // unknown
	fwrite_16_BE(f, (uint16_t)ctabs); // number of tabs
	fwrite_32_BE(f, 0); // offset

	for (i = 0; i < ctabs; ++i)
	{
		fwrite_32_BE(f, pal_entries[i].size); // offset
	}

	return 0;
}

int read_cluts(FILE* f)
{
	int i;

	verbose("Reading %d entries...\n", ctabs);

	for (i = 0; i < ctabs; ++i)
	{
		uint32_t size = clut_entries[i].size;
		clut_tab_t* p;
		uint32_t j;

		if (!size)
			continue;

		p = malloc(size);
		if (!p)
		{
			verbose("Not enough memory to store entry %d, needed %u bytes\n", i, clut_entries[i].size);
			return -20;
		}

		p->start_clut = fread_8(f);
		if (size > 1)
			p->end_clut = fread_8(f);
		else
			p->end_clut = p->start_clut;
		
		clut_entries[i].ccluts = (size - 2) / sizeof(clut_t);
		if (size > 1 && p->end_clut - p->start_clut + 1 != (int)clut_entries[i].ccluts)
		{
			verbose("Unsupported colortab format or already in new format, entry %d\n", i);
			return -20;
		}

		for (j = 0; j < clut_entries[i].ccluts; ++j)
		{
			int k;
			for (k = 0; k < CLUT_COLORS; ++k)
				p->cluts[j][k] = fread_16_BE(f);
		}

		clut_entries[i].data = p;
	}

	return 0;
}

int write_pals(FILE *f)
{
	int i;

	verbose("Writing %d entries...\n", ctabs);

	for (i = 0; i < ctabs; ++i)
	{
		if (!pal_entries[i].size)
			continue;

		if (1 != fwrite(pal_entries[i].data, pal_entries[i].size, 1, f))
		{
			verbose("Could not write pal entry %d\n", i);
			return -30;
		}
	}
	
	return 0;
}

int convert_clut(pal_t dst, clut_t src)
{
	int i, j, k;
	clut_color_t cval;
	pal_channel_t r, g, b, rtot, gtot, btot;

	for (i = 0; i < CLUT_COLORS; ++i)
	{
		cval = src[i];

		rtot = r = (pal_channel_t)((cval >> 10) & 0x1f); // bits 10-14
		gtot = g = (pal_channel_t)((cval >>  5) & 0x1f); // bits 5-9
		btot = b = (pal_channel_t)((cval      ) & 0x1f); // bits 0-4

		for (j = 0; j < CLUT_STEPS; ++j)
		{
			k = ((j << 5) + i);
			dst[k].chan.r = rtot;
			rtot += r;
			dst[k].chan.g = gtot;
			gtot += g;
			dst[k].chan.b = btot;
			btot += b;
		}
	}
	return 0;
}

int convert_cluts(pal_t* dst, clut_t* src, int cnt)
{
	int i;

	for (i = 0; i < cnt; ++i)
		convert_clut(dst[i], src[i]);

	return 0;
}

int convert_clut_tabs()
{
	int i;

	pal_entries = malloc(sizeof(pal_entries[0]) * ctabs);
	if (!pal_entries)
	{
		verbose("Not enough memory to store entries\n");
		return -40;
	}
	memset(pal_entries, 0, sizeof(pal_entries[0]) * ctabs);

	for (i = 0; i < ctabs; ++i)
	{
		clut_tab_t* pc;
		pal_tab_t* pp;
		uint32_t size;
		uint32_t c;

		pc = clut_entries[i].data;
		size = clut_entries[i].size;
		c = clut_entries[i].ccluts;
		if (!size || !pc)
			continue;
		
		if (c > 0)
		{	// convert colors
			pal_entries[i].size = 2 + sizeof(pal_t) * c;
			pp = malloc(pal_entries[i].size);
			if (!pp)
			{
				verbose("Not enough memory to store entry %d\n", i);
				return -41;
			}
			convert_cluts(pp->pals, pc->cluts, c);
		}
		else
		{	// just space for start and end indices
			pp = malloc(size);
			if (!pp)
			{
				verbose("Not enough memory to store entry %d\n", i);
				return -41;
			}
		}
		pp->start_clut = pc->start_clut;
		if (size > 1)
			pp->end_clut = pc->end_clut;

		pal_entries[i].data = pp;
	}

	return 0;
}

void free_data()
{
	int i;

	for (i = 0; i < ctabs; ++i)
	{
		if (clut_entries && clut_entries[i].data)
			free(clut_entries[i].data);

		if (pal_entries && pal_entries[i].data)
			free(pal_entries[i].data);
	}

	if (clut_entries)
		free(clut_entries);

	if (pal_entries)
		free(pal_entries);
}

void usage()
{
	fprintf(stderr, "usage: ctconv [-v] [-o out-file] <ct-file>\n");
	fprintf(stderr, "\tconverts 3DO CLUT strtab file to RRGGBB palette strtab\n");
	fprintf(stderr, "options:\n");
	fprintf(stderr, "  -v\t\t : be verbose, explains things no human should know\n");
	fprintf(stderr, "  -o out-file\t : specifies output file [default: same as input]\n");
}

void parse_arguments(int argc, char *argv[])
{
	char ch;
	
	while ((ch = getopt(argc, argv, "?hvo:")) != -1)
	{
		switch(ch)
		{
		case 'o':
			opt_outname = optarg;
			break;
		case 'v':
			opt_verbose = 1;
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
	if (argc != 1)
	{
		usage();
		exit(EXIT_FAILURE);
	}
	
	opt_inname = argv[0];
}

int main(int argc, char* argv[])
{
	FILE *f;
	int ret = 0;

	parse_arguments(argc, argv);

	if (!opt_outname)
		opt_outname = opt_inname;
	verbose("Output name: %s\n", opt_outname);

	f = fopen(opt_inname, "rb");
	if (!f)
	{
		fprintf(stderr, "Cannot open %s\n", opt_inname);
		return -1;
	}

	verbose("Reading file %s\n", opt_inname);

	ret = read_head(f);
	if (!ret)
		ret = read_cluts(f);
	fclose(f);

	if (!ret)
		ret = convert_clut_tabs();

	if (!ret)
	{
		f = fopen(opt_outname, "wb");
		if (!f)
		{
			fprintf(stderr, "Cannot open %s\n", opt_outname);
			return -2;
		}

		verbose("Writing file %s\n", opt_outname);
	}

	if (!ret)
		ret = write_head(f);
	if (!ret)
		ret = write_pals(f);

	fclose(f);
	free_data();

	return ret;
}
