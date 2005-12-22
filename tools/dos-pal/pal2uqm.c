/*
 * DOS SC2 palette tab convertor (to UQM format)
 * By Alex Volkov (codepro@usa.net), 20051222
 *
 * -- Some code borrowed from 'unani' by Serge van den Boom (svdb@stack.nl)
 * -- Some more stuff borrowed from FSCP by Mudry (mudronyl@dragon.klte.hu)
 *
 * The GPL applies (to this file).
 *
 */ 

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <malloc.h>
#if defined(WIN32) && defined(_MSC_VER)
#	include <getopt.h>
#	include <direct.h>
#	define makedir(d)  _mkdir(d)
#	define inline __inline
#else
#	include <unistd.h>
#	define makedir(d)  mkdir(d, 0777)
#endif
#if defined(__GNUC__)
#	include <alloca.h>
#	define inline __inline__
#endif 

#define countof(a)	   ( sizeof(a)/sizeof(*a) )
#ifndef offsetof
#	define offsetof(s,m)  ( (size_t)(&((s*)0)->m) )
#endif

#if defined(_MSC_VER)
#	define PACKED
#	pragma pack(push, 1)
#elif defined(__GNUC__)
#	define PACKED __attribute__((packed))
#endif

// File structure types
typedef struct
{
	int32_t  PACKED compsize; // size after decompression, or -1 if already
	uint16_t PACKED recs;     // Number of records
	uint16_t PACKED cbextra;  // Size of extra data after rec sizes
} dos_header_t;

typedef uint16_t dos_recsize_t;

typedef struct
{
	int32_t  PACKED compsize; // size after decompression, or -1 if already
	uint32_t PACKED recs;     // Number of records
	uint32_t PACKED cbextra;  // Size of extra data after rec sizes
} uqm_header_t;

typedef uint32_t uqm_recsize_t;

// Palette types
typedef uint8_t channel_t;

typedef struct
{
	channel_t r, g, b;
} color_t;

typedef struct
{
	uint8_t first;
	uint8_t last;
	union
	{
		color_t colors[1];
		channel_t chans[3];
	} pal;
} colormap_t;

#define DOS_SIG_BITS 6

#define MAP_COLORS  0x100 

typedef union
{
	color_t colors[MAP_COLORS];
	channel_t chans[MAP_COLORS * 3];
} uqm_palette_t;

typedef struct
{
	uint8_t first;
	uint8_t last;
	uqm_palette_t pal;
} uqm_colormap_t;

#if defined(_MSC_VER)
#	pragma pack(pop)
#endif

struct options
{
	char *infile;
	char *outfile;
	int verbose;
	int plut;
	int num_palettefiles;
	char **palettefiles;
};


int verbose_level = 0;
void verbose(int level, const char* fmt, ...);
void parse_arguments(int argc, char* argv[], struct options* opts);
int loadPalette(uqm_palette_t* pal, const char* filename);
void setPalette(uqm_palette_t* pal, colormap_t* map);
dos_recsize_t* processHeader(dos_header_t* h);
int writeColortab(dos_header_t* h, const char* filename);

inline channel_t conv_chan(uint8_t c);

inline uint16_t get_16_be(uint16_t* val);
inline uint32_t get_32_be(uint32_t* val);
inline void put_16_be(uint16_t* into, uint16_t val);
inline void put_32_be(uint32_t* into, uint32_t val);

uqm_colormap_t basemap;
int num_maps = 0;


int main(int argc, char* argv[])
{
	struct options opts;
	FILE* in;
	size_t inlen;
	uint8_t* buf;
	dos_header_t* h;
	dos_recsize_t* sizes;
	int ret;

	parse_arguments(argc, argv, &opts);
	verbose_level = opts.verbose;

	in = fopen(opts.infile, "rb");
	if (!in)
	{
		verbose(1, "Error: Could not open file %s: %s\n",
				opts.infile, strerror(errno));
		return EXIT_FAILURE;
	}
	
	fseek(in, 0, SEEK_END);
	inlen = ftell(in);
	fseek(in, 0, SEEK_SET);

	buf = malloc(inlen);
	if (!buf)
	{
		verbose(1, "Out of memory reading file\n");
		return EXIT_FAILURE;
	}
	if (inlen != fread(buf, 1, inlen, in))
	{
		verbose(1, "Cannot read file '%s'\n", opts.infile);
		return EXIT_FAILURE;
	}
	fclose(in);

	h = (dos_header_t*)buf;
	sizes = processHeader(h);

	{
		int i;

		memset(&basemap, 0, sizeof(basemap));
		for (i = 0; i < opts.num_palettefiles; i++)
			loadPalette(&basemap.pal, opts.palettefiles[i]);
	}
	basemap.first = opts.plut;
	basemap.last = opts.plut;
	
	ret = writeColortab(h, opts.outfile);

	free(buf);
	
	return ret;
}

void verbose(int level, const char* fmt, ...)
{
	va_list args;
	
	if (verbose_level < level)
		return;
	
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
}

void usage()
{
	fprintf(stderr,
			"pal2uqm [-m <plutind>] [-c <basepal>] [-o <outfile>] <infile>\n"
			"Options:\n"
			"\t-c  load base palette in dos format (multiple -c allowed)\n"
			"\t-o  output .ct filename\n"
			"\t-v  increase verbosity level (use more than once)\n"
			);
}

void parse_arguments(int argc, char* argv[], struct options* opts)
{
	char ch;
	
	memset(opts, 0, sizeof (struct options));

	while (-1 != (ch = getopt(argc, argv, "h?c:o:m:v")))
	{
		switch (ch)
		{
		case 'c':
			opts->palettefiles = realloc(opts->palettefiles, 
					(opts->num_palettefiles + 1) * sizeof (char *));
			opts->palettefiles[opts->num_palettefiles] = optarg;
			opts->num_palettefiles++;
			break;
		case 'o':
			opts->outfile = optarg;
			break;
		case 'm':
			opts->plut = atoi(optarg);
			break;
		case 'v':
			opts->verbose++;
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
	opts->infile = argv[0];
	if (!opts->outfile)
		opts->outfile = opts->infile;
}

dos_recsize_t* processHeader(dos_header_t* h)
{
	dos_recsize_t* sizes;
	unsigned i;

	h->compsize = get_32_be(&h->compsize);
	h->recs = get_16_be(&h->recs);
	h->cbextra = get_16_be(&h->cbextra);

	sizes = (dos_recsize_t*)((uint8_t*)h + sizeof(*h));
	
	for (i = 0; i < h->recs; ++i)
		sizes[i] = get_16_be(sizes + i);

	return sizes;
}

int loadPalette(uqm_palette_t* pal, const char* filename)
{
	FILE* in;
	size_t inlen;
	uint8_t *buf;

	in = fopen(filename, "rb");
	if (!in)
	{
		verbose(1, "Error: Could not open file %s: %s\n", filename,
				strerror(errno));
		return EXIT_FAILURE;
	}

	fseek(in, 0, SEEK_END);
	inlen = ftell(in);
	fseek(in, 0, SEEK_SET);

	buf = malloc(inlen);
	if (!buf)
	{
		verbose(1, "Out of memory reading file\n");
		fclose(in);
		return EXIT_FAILURE;
	}
	if (inlen != fread(buf, 1, inlen, in))
	{
		verbose(1, "Cannot read file '%s'\n", filename);
		fclose(in);
		return EXIT_FAILURE;
	}
	fclose(in);

	verbose(2, "%s contains %d palette entries\n",
			filename, (int)buf[1] - buf[0] + 1);
	
	setPalette(pal, (colormap_t*)buf);

	free(buf);

	return 0;
}

void setPalette(uqm_palette_t* pal, colormap_t* map)
{
	int i;
	color_t* sclr;
	color_t* dclr;

	for (i = map->first, sclr = map->pal.colors, dclr = pal->colors + map->first;
			i <= (int)map->last;
			++i, ++sclr, ++dclr)
	{
		dclr->r = conv_chan(sclr->r);
		dclr->g = conv_chan(sclr->g);
		dclr->b = conv_chan(sclr->b);
	}
}

int writeColortab(dos_header_t* h, const char* filename)
{
	FILE* f;
	uqm_header_t uqmh;
	dos_recsize_t* sizes;
	uqm_recsize_t uqmsize;
	colormap_t* map;
	uqm_colormap_t uqmmap;
	int i;

	put_32_be(&uqmh.compsize, -1);
	put_32_be(&uqmh.recs, h->recs);
	put_32_be(&uqmh.cbextra, 0);
	// colortab sizes are fixed in uqm
	put_32_be(&uqmsize, sizeof(uqmmap));

	sizes = (dos_recsize_t*)((uint8_t*)h + sizeof(*h));

	f = fopen(filename, "wb");
	if (!f)
	{
		verbose(1, "Error: Could not open file %s: %s\n", filename,
				strerror(errno));
		return EXIT_FAILURE;
	}
	if (1 != fwrite(&uqmh, sizeof(uqmh), 1, f))
	{
		verbose(1, "Cannot write file '%s'\n", filename);
		fclose(f);
		return EXIT_FAILURE;
	}
	for (i = 0; i < h->recs; ++i)
		fwrite(&uqmsize, sizeof(uqmsize), 1, f);

	map = (colormap_t*)(sizes + h->recs);
	for (i = 0; i < h->recs; ++i)
	{
		memcpy(&uqmmap, &basemap, sizeof(basemap));
		setPalette(&uqmmap.pal, map);

		if (1 != fwrite(&uqmmap, sizeof(uqmmap), 1, f))
		{
			verbose(1, "Cannot write file '%s'\n", filename);
			fclose(f);
			return EXIT_FAILURE;
		}

		map = (colormap_t*)((uint8_t*)map + sizes[i]);
	}

	fclose(f);

	return EXIT_SUCCESS;
}

inline channel_t conv_chan(uint8_t c)
{	// this uniformly distributes the lower 'unknown' bits
	return (c << (8 - DOS_SIG_BITS)) | (c >> (DOS_SIG_BITS - (8 - DOS_SIG_BITS)));
}

inline uint16_t get_16_be(uint16_t* val)
{
	return (uint16_t)(((uint8_t*)val)[0] << 8) | ((uint8_t*)val)[1];
}

inline uint32_t get_32_be(uint32_t* val)
{
	return (uint32_t)(get_16_be((uint16_t*)val) << 16) | get_16_be((uint16_t*)val + 1);
}

inline void put_16_be(uint16_t* into, uint16_t val)
{
	((uint8_t*)into)[0] = val >> 8;
	((uint8_t*)into)[1] = val & 0xff;
}

inline void put_32_be(uint32_t* into, uint32_t val)
{
	put_16_be(((uint16_t*)into) + 0, val >> 16);
	put_16_be(((uint16_t*)into) + 1, val & 0xffff);
}
