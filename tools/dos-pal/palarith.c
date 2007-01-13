/*
 * UQM palette arithmetic
 * By Alex Volkov (codepro@usa.net), 20051222
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

// Palette types
typedef uint8_t channel_t;

typedef struct
{
	channel_t r, g, b;
} color_t;

#define PAL_COLORS  0x100 

typedef union
{
	color_t colors[PAL_COLORS];
	channel_t chans[PAL_COLORS * 3];
} uqm_palette_t;

typedef struct
{
	float y, u, v;
} yuv_t;

#if defined(_MSC_VER)
#	pragma pack(pop)
#endif

struct options
{
	char *infile;
	char *outfile;
	int from, to; // inclusive bounds
	int luma;
	int add;
	int sub;
	int blend;
	color_t color;
	int color_set;
	int prorate;
	int verbose;
};


int verbose_level = 0;
void verbose(int level, const char* fmt, ...);
void parse_arguments(int argc, char* argv[], struct options* opts);
int readPalette(uqm_palette_t*, const char* filename);
int writePalette(uqm_palette_t*, const char* filename);

void setColor(uqm_palette_t*, int from, int to, color_t* clr);
void adjustLuma(uqm_palette_t*, int from, int to, int ratio);
void setColorLuma(uqm_palette_t*, int from, int to, int ratio, color_t* toclr);
void addColor(uqm_palette_t*, int from, int to, color_t* clr, int prorate);
void subColor(uqm_palette_t*, int from, int to, color_t* clr, int prorate);
void blendToColor(uqm_palette_t*, int from, int to, int ratio, color_t* toclr, int prorate);

int main(int argc, char* argv[])
{
	struct options opts;
	int ret;
	uqm_palette_t pal;

	parse_arguments(argc, argv, &opts);
	verbose_level = opts.verbose;

	verbose(2, "Processing '%s', index %d-%d\n", opts.infile, opts.from, opts.to);

	memset(&pal, 0, sizeof(pal));

	ret = readPalette(&pal, opts.infile);
	if (ret != 0)
		return ret;

	if (opts.luma < 0 && !opts.add && !opts.sub && !opts.blend)
		setColor(&pal, opts.from, opts.to, &opts.color);

	if (opts.luma >= 0)
	{
		if (opts.color_set && !opts.add && !opts.sub && !opts.blend)
			setColorLuma(&pal, opts.from, opts.to, opts.luma, &opts.color);
		else
			adjustLuma(&pal, opts.from, opts.to, opts.luma);
	}

	if (opts.add)
		addColor(&pal, opts.from, opts.to, &opts.color, opts.prorate);
	else if (opts.sub)
		subColor(&pal, opts.from, opts.to, &opts.color, opts.prorate);
	else if (opts.blend)
		blendToColor(&pal, opts.from, opts.to, opts.blend, &opts.color,
				opts.prorate);

	ret = writePalette(&pal, opts.outfile);

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
			"palarith [-r range] [-l level] [-a|-s|-b ratio] [-c RRGGBB] [-p]\n"
			"         [-o <outfile>] <infile>\n"
			"Options:\n"
			"\t-r  color index range within palette to work on (form: XXX-YYY)\n"
			"\t-l  adjust luminosity to level (in percent)\n"
			"\t-a  add color (requires -c; exclusive with -s and -b)\n"
			"\t-s  subtract color (requires -c; exclusive with -a and -b)\n"
			"\t-b  blend with color, ratio is percent of color (requires -c)\n"
			"\t-c  target color for ops that use it (hex; 00ff00 for green)\n"
			"\t-p  pro-rate the added, subed or blended color to current luma\n"
			"\t-o  output palette filename\n"
			"\t-v  increase verbosity level (use more than once)\n"
			);
}

void parse_arguments(int argc, char* argv[], struct options* opts)
{
	char ch;
	
	memset(opts, 0, sizeof (struct options));
	opts->to = PAL_COLORS - 1;
	opts->luma = -1;

	while (-1 != (ch = getopt(argc, argv, "h?o:r:l:asb:c:pv")))
	{
		switch (ch)
		{
		case 'r':
		{
			char* pto = strchr(optarg, ':');
			if (pto)
			{
				*pto = '\0';
				++pto;
			}
			if (!*optarg && (!pto || !*pto))
			{
				fprintf(stderr, "Invalid argument to -r\n");
				exit(EXIT_FAILURE);
			}
			if (*optarg)
				opts->from = atoi(optarg);
			if (pto && *pto)
				opts->to = atoi(pto);

			if (opts->to < opts->from
				|| opts->from < 0 || opts->from >= PAL_COLORS
				|| opts->to < 0 || opts->to >= PAL_COLORS)
			{
				fprintf(stderr, "Out of range argument to -r\n");
				exit(EXIT_FAILURE);
			}
			break;
		}
		case 'c':
		{
			int r, g, b;
			if (3 != sscanf(optarg, "%2x%2x%2x", &r, &g, &b))
			{
				fprintf(stderr, "Bad -c RRGGBB option '%s'\n", optarg);
				exit(EXIT_FAILURE);
			}
			opts->color.r = r;
			opts->color.g = g;
			opts->color.b = b;
			opts->color_set = 1;
			break;
		}
		case 'l':
			opts->luma = atoi(optarg);
			if (opts->luma < 0 || opts->luma > 255 * 100)
			{
				fprintf(stderr, "Out of range -i option '%s'\n", optarg);
				exit(EXIT_FAILURE);
			}
			break;
		case 'a':
			opts->add = 1;
			break;
		case 's':
			opts->sub = 1;
			break;
		case 'b':
			opts->blend = atoi(optarg);
			if (opts->blend < 0 || opts->blend > 100)
			{
				fprintf(stderr, "Out of range -b option '%s'\n", optarg);
				exit(EXIT_FAILURE);
			}
			break;
		case 'p':
			opts->prorate = 1;
			break;
		case 'o':
			opts->outfile = optarg;
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

	if (opts->luma < 0 && !opts->color_set && !opts->add && !opts->sub && !opts->blend)
	{
		fprintf(stderr, "Please specify one or more of -i, -c, -a, -s or -b\n");
		exit(EXIT_FAILURE);
	}

	if ((opts->add || opts->sub || opts->blend) && !opts->color_set)
	{
		fprintf(stderr, "Options -a, -s and -b require -c\n");
		exit(EXIT_FAILURE);
	}
}

int readPalette(uqm_palette_t* pal, const char* filename)
{
	FILE* in;
	size_t inlen;

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

	if (inlen < sizeof(*pal))
	{
		verbose(1, "Warning: palette '%s' is incomplete\n", filename);
	}
	else if (inlen > sizeof(*pal))
	{
		verbose(1, "Warning: palette '%s' appears too large\n", filename);
		inlen = sizeof(*pal);
	}

	if (1 != fread(pal, inlen, 1, in))
	{
		verbose(1, "Cannot read file '%s'\n", filename);
		fclose(in);
		return EXIT_FAILURE;
	}
	
	fclose(in);

	return 0;
}

int writePalette(uqm_palette_t* pal, const char* filename)
{
	FILE* f;

	f = fopen(filename, "wb");
	if (!f)
	{
		verbose(1, "Error: Could not open file %s: %s\n", filename,
				strerror(errno));
		return EXIT_FAILURE;
	}
	if (1 != fwrite(pal, sizeof(*pal), 1, f))
	{
		verbose(1, "Cannot write file '%s'\n", filename);
		fclose(f);
		return EXIT_FAILURE;
	}

	fclose(f);

	return EXIT_SUCCESS;
}

int calc_y(color_t rgb)
{
	int y;

	y = 0.299f * rgb.r + 0.587f * rgb.g + 0.114f * rgb.b;

	return y;
}

inline int clamp_chan(int c)
{
	if (c > 255)
		c = 255;
	if (c < 0)
		c = 0;
	return c;
}

int get_luma(color_t rgb)
{
	if (rgb.r >= rgb.g && rgb.r >= rgb.b)
		return rgb.r;
	else if (rgb.g >= rgb.r && rgb.g >= rgb.b)
		return rgb.g;
	else
		return rgb.b;
}

void adjustLuma(uqm_palette_t* pal, int from, int to, int ratio)
{
	int i;
	color_t* sclr;
	float fact = 0.01f * ratio;
	int minl = 255, maxl = 0;
	float boost = 0;

	// get some statistics first
	for (i = from, sclr = pal->colors + from; i <= to; ++i, ++sclr)
	{
		int l = get_luma(*sclr);
		if (l < minl)
			minl = l;
		if (l > maxl)
			maxl = l;
	}

	if (maxl * fact > 255)
	{
		boost = (maxl * fact - 255) / (255.0 * 255.0 / 100.0);
	}

	if (ratio > 100)
		fact = 1.0f + 0.0025f * (ratio - 100);

	for (i = from, sclr = pal->colors + from; i <= to; ++i, ++sclr)
	{
		color_t clr = *sclr;

		sclr->r = clamp_chan(fact * clr.r + boost * (maxl - clr.r));
		sclr->g = clamp_chan(fact * clr.g + boost * (maxl - clr.g));
		sclr->b = clamp_chan(fact * clr.b + boost * (maxl - clr.b));
	}
}

void setColorLuma(uqm_palette_t* pal, int from, int to,
		int ratio, color_t* toclr)
{
	int i;
	color_t* sclr;
	float fact = 0.01f * ratio;
	int minl = 255, maxl = 0;
	float boost = 0;

	// get some statistics first
	for (i = from, sclr = pal->colors + from; i <= to; ++i, ++sclr)
	{
		int l = get_luma(*sclr);
		if (l < minl)
			minl = l;
		if (l > maxl)
			maxl = l;
	}

	if (maxl * fact > 255)
	{
		boost = (maxl * fact - 255) / (255.0 * 255.0 / 100.0);
	}

	if (ratio > 100)
		fact = 1.0f + 0.0025f * (ratio - 100);

	for (i = from, sclr = pal->colors + from; i <= to; ++i, ++sclr)
	{
		color_t clr;
		int l = get_luma(*sclr);

		clr.r = toclr->r * l / 255;
		clr.g = toclr->g * l / 255;
		clr.b = toclr->b * l / 255;

		sclr->r = clamp_chan(fact * clr.r + boost * (maxl - clr.r));
		sclr->g = clamp_chan(fact * clr.g + boost * (maxl - clr.g));
		sclr->b = clamp_chan(fact * clr.b + boost * (maxl - clr.b));
	}
}

void setColor(uqm_palette_t* pal, int from, int to, color_t* clr)
{
	int i;
	color_t* dclr;

	for (i = from, dclr = pal->colors + from; i <= to; ++i, ++dclr)
	{
		*dclr = *clr;
	}
}

void addColor(uqm_palette_t* pal, int from, int to, color_t* clr, int prorate)
{
	int i;
	color_t* sclr;

	for (i = from, sclr = pal->colors + from; i <= to; ++i, ++sclr)
	{
		color_t dclr = *clr;
		if (prorate)
		{
			int l = get_luma(*sclr);
			dclr.r = dclr.r * l / 255;
			dclr.g = dclr.g * l / 255;
			dclr.b = dclr.b * l / 255;
		}
		sclr->r = clamp_chan(sclr->r + dclr.r);
		sclr->g = clamp_chan(sclr->g + dclr.g);
		sclr->b = clamp_chan(sclr->b + dclr.b);
	}
}

void subColor(uqm_palette_t* pal, int from, int to, color_t* clr, int prorate)
{
	int i;
	color_t* sclr;

	for (i = from, sclr = pal->colors + from; i <= to; ++i, ++sclr)
	{
		color_t dclr = *clr;
		if (prorate)
		{
			int l = get_luma(*sclr);
			dclr.r = dclr.r * l / 255;
			dclr.g = dclr.g * l / 255;
			dclr.b = dclr.b * l / 255;
		}
		sclr->r = clamp_chan(sclr->r - dclr.r);
		sclr->g = clamp_chan(sclr->g - dclr.g);
		sclr->b = clamp_chan(sclr->b - dclr.b);
	}
}

void blendToColor(uqm_palette_t* pal, int from, int to,
		int ratio, color_t* toclr, int prorate)
{
	int i;
	color_t* sclr;
	float dfact = 0.01f * ratio;
	float sfact = 0.01f * (100 - ratio);

	for (i = from, sclr = pal->colors + from; i <= to; ++i, ++sclr)
	{
		color_t dclr = *toclr;
		if (prorate)
		{
			int l = get_luma(*sclr);
			dclr.r = dclr.r * l / 255;
			dclr.g = dclr.g * l / 255;
			dclr.b = dclr.b * l / 255;
		}
		sclr->r = clamp_chan(sclr->r * sfact + dclr.r * dfact);
		sclr->g = clamp_chan(sclr->g * sfact + dclr.g * dfact);
		sclr->b = clamp_chan(sclr->b * sfact + dclr.b * dfact);
	}
}
