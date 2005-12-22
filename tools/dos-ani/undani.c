/*
 * DOS SC2 .ani file extractor
 * By Alex Volkov (codepro@usa.net), 20050207
 
 * -- Largely adapted from 'unani' by Serge van den Boom (svdb@stack.nl)
 * -- Yet more stuff borrowed from FSCP by Mudry (mudronyl@dragon.klte.hu)
 * -- Utilizes LZSS algorithm;
 *    code found at http://www.cs.umu.se/~isak/Snippets
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
#include <png.h>
#include "lzhuf.h"

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

typedef struct
{
	uint32_t PACKED magic;  // Always ffff
	uint32_t PACKED z;      // Always zero
	uint16_t PACKED ofs;    // Offset of data 
	uint16_t PACKED recs_and_flags;   // bits 0..11: number of records; bits 12..15: flags
} header_t;

#define HEADER_START	sizeof(uint32_t)

typedef struct
{
	int16_t x;
	int16_t y;
} hotspot_t;

typedef struct
{
	uint16_t w;
	uint16_t h;
} bounds_t;

typedef struct
{
	hotspot_t PACKED hotspot;
	uint16_t PACKED zb;       // Always zero
	uint16_t PACKED zc;       // Always zero
	bounds_t PACKED bounds;
	uint16_t PACKED flags;    // Flags
	uint16_t PACKED xsize8;   // Width of 8bit image in bytes
	uint32_t PACKED size8;    // Size of 8bit image in bytes
	uint16_t PACKED zj;       // Always zero
	uint16_t PACKED mflags;   // Mask flags
	uint16_t PACKED xsize1;   // Width of 1bit image in bytes
	uint16_t PACKED size1;    // Size of 1bit image in bytes
	uint16_t PACKED zn;       // Always zero
	uint16_t PACKED zo;       // Always zero
	uint16_t PACKED index;    // frame index, only lower 12 bits used
} frame_desc_t;

#define FTYPE_SHIFT   12
#define FINDEX_MASK   ((1 << FTYPE_SHIFT) - 1)
#define FTYPE_MASK    (0xffff & ~FINDEX_MASK)

#define TYPE_GET(f)   (((f) & FTYPE_MASK) >> FTYPE_SHIFT)
#define INDEX_GET(f)  ((f) & FINDEX_MASK)  

#define GFX_DATA_CHILD      (1 << 8)
#define GFX_DATA_FLAG2      (1 << 9)
#define GFX_DATA_COMPRESSED (1 << 10)

typedef struct
{
	frame_desc_t* desc;
	uint32_t csize8;
	uint8_t* data8;
	uint32_t csize1;
	uint8_t* data1;
	int malloced;
	int sx0, sx1;
	int sy0, sy1;
} frame_info_t;

typedef struct
{
	int32_t num_frames;
	uint32_t data_ofs;
	uint32_t flags;
	frame_info_t* frames;
} index_header_t;


#define GFX_HAS_MASKS      (1 << 0)
#define GFX_HAS_IMAGES     (1 << 1)
#define GFX_HAS_COMPRESSED (1 << 2)

#define PAL_COLORS	0x100 

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

#if defined(_MSC_VER)
#	pragma pack(pop)
#endif

struct options
{
	char *infile;
	char *outdir;
	char *prefix;
	int makeani;
	int list;
	int print;
	int crop;
	int verbose;
	int plut;
	int zeropos;
	int num_palettefiles;
	char **palettefiles;
};

png_color palette[PAL_COLORS];

int verbose_level = 0;
void verbose(int level, const char* fmt, ...);

index_header_t* readIndex(uint8_t *buf);
void freeIndex(index_header_t *);
void printIndex(const index_header_t* h, const uint8_t *buf, FILE *out);
void generateAni(const index_header_t* h, const char *prefix, int zeropos, int plut, FILE *out);
int findTransparentColour(const char *name, const frame_info_t* f);
void writeFiles(const index_header_t* h, const char *path, const char *prefix, int zeropos);
void parse_arguments(int argc, char *argv[], struct options *opts);
void listFrames(const index_header_t* h, FILE *out);
int loadPalette(const char *filename);
int decompressFrames(const index_header_t *);
int cropFrames(const index_header_t *);

inline channel_t conv_chan(uint8_t c);

inline uint16_t get_16_le(uint16_t* val);
inline uint32_t get_32_le(uint32_t* val);

int main(int argc, char *argv[])
{
	FILE* in;
	size_t inlen;
	uint8_t *buf;
	index_header_t *h;
	struct options opts;
	char *prefix;

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

	{
		int i;

		memset(palette, 0, sizeof (palette));
		for (i = 0; i < opts.num_palettefiles; i++)
			loadPalette(opts.palettefiles[i]);
	}

	h = readIndex(buf);
	if (opts.print)
		printIndex(h, buf, stdout);

	if (opts.list)
		listFrames(h, stdout);
	
	if (opts.prefix == NULL)
	{
		char *start, *end;

		start = strrchr(opts.infile, '/');
		if (start == NULL) {
			start = opts.infile;
		} else
			start++;
		end = strrchr(start, '.');
		if (end == NULL)
			end = start + strlen(start);
		prefix = alloca(end - start + 1);
		memcpy(prefix, start, end - start);
		prefix[end - start] = '\0';
	} else
		prefix = opts.prefix;

	if (opts.makeani || (!opts.print && !opts.list))
	{
		if (decompressFrames(h) != 0)
		{
			verbose(1, "Cannot decompress frames\n");
			return EXIT_FAILURE;
		}

		if (opts.crop && cropFrames(h) != 0)
		{
			verbose(1, "Cannot crop frames\n");
			return EXIT_FAILURE;
		}
	}

	if (opts.makeani)
		generateAni(h, prefix, opts.zeropos, opts.plut, stdout);
	
	if (!opts.print && !opts.list && !opts.makeani)
	{
		size_t len;
		
		len = strlen(opts.outdir);
		if (opts.outdir[len - 1] == '/')
			opts.outdir[len - 1] = '\0';

		writeFiles(h, opts.outdir, prefix, opts.zeropos);
	}

	freeIndex(h);
	free(buf);
	
	return EXIT_SUCCESS;
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
			"undani -a [-m <plutind>] [-r] [-0] <infile>\n"
			"undani -p <infile>\n"
			"undani [-c <palfile>] [-r] [-0] [-o <outdir>] [-n <prefix>] <infile>\n"
			"Options:\n"
			"\t-a  output .ani file\n"
			"\t-p  print header and frame info\n"
			"\t-c  load palette in dos format (multiple -c allowed)\n"
			"\t-o  stuff pngs into <outdir>\n"
			"\t-n  name pngs <prefix>.<N>.png\n"
			"\t-r  crop the images removing edge rows and columns consisting\n"
			"\t    entirely of transparent color (affects hotspots with -a)\n"
			"\t-0  make <N> 0-prepended; use several to specify width\n"
			"\t-v  increase verbosity level (use more than once)\n"
			);
}

void parse_arguments(int argc, char *argv[], struct options *opts)
{
	char ch;
	
	memset(opts, 0, sizeof (struct options));
	opts->plut = -1;

	while (-1 != (ch = getopt(argc, argv, "ac:h?ln:o:m:pr0v")))
	{
		switch (ch)
		{
		case 'a':
			opts->makeani = 1;
			break;
		case 'c':
			opts->palettefiles = realloc(opts->palettefiles, 
					(opts->num_palettefiles + 1) * sizeof (char *));
			opts->palettefiles[opts->num_palettefiles] = optarg;
			opts->num_palettefiles++;
			break;
		case 'l':
			opts->list = 1;
			break;
		case 'o':
			opts->outdir = optarg;
			break;
		case 'm':
			opts->plut = atoi(optarg);
			break;
		case 'n':
			opts->prefix = optarg;
			break;
		case 'p':
			opts->print = 1;
			break;
		case 'r':
			opts->crop = 1;
			break;
		case '0':
			opts->zeropos++;
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
	if (opts->outdir == NULL)
		opts->outdir = ".";
}

index_header_t* readIndex(uint8_t *buf)
{
	index_header_t *h;
	const uint8_t *bufptr;
	int i;
	header_t* fh = (header_t*) buf;
	uint32_t cofs;
	uint32_t csize;

	h = malloc(sizeof (index_header_t));
	
	// convert all from little endian
	fh->magic = get_32_le(&fh->magic);
	fh->z = get_32_le(&fh->z);
	fh->ofs = get_16_le(&fh->ofs);
	fh->recs_and_flags = get_16_le(&fh->recs_and_flags);

	if (fh->magic != 0xffffffff || fh->z != 0x00000000)
	{
		verbose(1, "File is not a valid dos .ani file.\n");
		exit(EXIT_FAILURE);
	}
	
	h->num_frames = INDEX_GET(fh->recs_and_flags) + 1;
	h->flags = TYPE_GET(fh->recs_and_flags);
	h->data_ofs = HEADER_START + fh->ofs;
	
	bufptr = buf + sizeof(*fh);
	h->frames = malloc(h->num_frames * sizeof (frame_info_t));
	if (!h->frames)
	{
		verbose(1, "Out of memory parsing frame descriptors\n");
		exit(EXIT_FAILURE);
	}
	memset(h->frames, 0, h->num_frames * sizeof (frame_info_t));

	for (i = 0; i < h->num_frames; i++, bufptr += sizeof(frame_desc_t))
	{
		frame_desc_t* desc = (frame_desc_t*) bufptr;
		
		// convert all from little endian
		desc->hotspot.x = get_16_le(&desc->hotspot.x);
		desc->hotspot.y = get_16_le(&desc->hotspot.y);
		desc->bounds.w = get_16_le(&desc->bounds.w);
		desc->bounds.h = get_16_le(&desc->bounds.h);
		desc->flags = get_16_le(&desc->flags);
		desc->xsize8 = get_16_le(&desc->xsize8);
		desc->size8 = get_32_le(&desc->size8);
		desc->mflags = get_16_le(&desc->mflags);
		desc->xsize1 = get_16_le(&desc->xsize1);
		desc->size1 = get_16_le(&desc->size1);
		desc->index = get_16_le(&desc->index);

		if (desc->zb != 0 || desc->zc != 0 || desc->zj != 0 || desc->zn != 0 || desc->zo != 0)
		{
			verbose(1, "Error: one of zero fields is not zero in frame index %d\n", i);
			exit(EXIT_FAILURE);
		}

		h->frames[i].desc = desc;
	}

	// read or compute offsets and sizes
	cofs = h->data_ofs;
	if (h->flags & GFX_HAS_MASKS)
	{	// all 1bit masks have to be processed first
		for (i = 0; i < h->num_frames; ++i, cofs += csize)
		{
			frame_info_t* info = h->frames + i;

			if (info->desc->mflags & GFX_DATA_COMPRESSED)
			{	// compressed data
				uint16_t* psize = (uint16_t*) (buf + cofs);

				csize = get_16_le(psize);
				
				info->data1 = buf + cofs + sizeof(*psize);
				info->csize1 = csize - sizeof(*psize);
			}
			else
			{	// uncompressed data
				info->data1 = buf + cofs;
				info->csize1 = csize = info->desc->size1;
			}
		}
	}
	if (h->flags & GFX_HAS_IMAGES)
	{	// then all 8bit images
		for (i = 0; i < h->num_frames; ++i, cofs += csize)
		{
			frame_info_t* info = h->frames + i;

			if (info->desc->flags & GFX_DATA_COMPRESSED)
			{	// compressed data
				uint16_t* psize = (uint16_t*) (buf + cofs);

				csize = get_16_le(psize);
				
				info->data8 = buf + cofs + sizeof(*psize);
				info->csize8 = csize - sizeof(*psize);
			}
			else
			{	// uncompressed data
				info->data8 = buf + cofs;
				info->csize8 = csize = info->desc->size8;
			}
		}
	}

	return h;
}

void freeFrame(frame_info_t* f)
{
	if ((f->malloced & 1) && f->data1)
		free(f->data1);
	if ((f->malloced & 2) && f->data8)
		free(f->data8);
}

void freeIndex(index_header_t* h)
{
	int i;

	if (!h)
		return;
	if (h->frames)
	{
		for (i = 0; i < h->num_frames; ++i)
			freeFrame(h->frames + i);
		
		free(h->frames);
	}
	free(h);
}

void printIndex(const index_header_t *h, const uint8_t *buf, FILE *out)
{
	fprintf(out, "0x%08x  Number of frames:        %d\n",
			offsetof(header_t, recs_and_flags),
			h->num_frames);
	
	fprintf(out, "0x%08x  Flags: 0x%02x - %s%s%s\n",
			offsetof(header_t, recs_and_flags),
			h->flags,
			(h->flags & GFX_HAS_COMPRESSED) ?
			"HAS_COMPRESSED " : "",
			(h->flags & GFX_HAS_MASKS) ?
			"HAS_MASKS " : "",
			(h->flags & GFX_HAS_IMAGES) ?
			"HAS_IMAGES " : "");
	fprintf(out, "0x%08x  frame info:\n", sizeof(header_t));
	{
		int i;
		
		for (i = 0; i < h->num_frames; i++)
		{
			frame_desc_t* desc = h->frames[i].desc;

			fprintf(out, "0x%08x    Frame %d:\n", (uint8_t*)desc - buf,
					INDEX_GET(desc->index));

			fprintf(out, "0x%08x      Hotspot: (%d, %d)\n", (uint8_t*)&desc->hotspot - buf,
					(int)desc->hotspot.x, (int)desc->hotspot.y);
			fprintf(out, "0x%08x      Size: %dx%d\n", (uint8_t*)&desc->bounds - buf,
					(int)desc->bounds.w, (int)desc->bounds.h);

			fprintf(out, "0x%08x      Flags: 0x%02x - %s%s%s\n",
					(uint8_t*)&desc->flags - buf,
					desc->flags,
					(desc->flags & GFX_DATA_COMPRESSED) ?
					"DATA_COMPRESSED " : "",
					(desc->flags & GFX_DATA_FLAG2) ?
					"DATA_FLAG2 " : "",
					(desc->flags & GFX_DATA_CHILD) ?
					"DATA_CHILD" : "");
		}
	}
}

void listFrames(const index_header_t* h, FILE *out)
{
	int i;
	
	for (i = 0; i < h->num_frames; i++)
	{
		frame_desc_t* desc = h->frames[i].desc;

		fprintf(out, "0x%04x  %dx%d image, %s%s%s\n",
				(int)INDEX_GET(desc->index),
				(int)desc->bounds.w, (int)desc->bounds.h,
				(desc->flags & GFX_DATA_COMPRESSED) ?
				"DATA_COMPRESSED " : "",
				(desc->flags & GFX_DATA_FLAG2) ?
				"DATA_FLAG2 " : "",
				(desc->flags & GFX_DATA_CHILD) ?
				"DATA_CHILD" : "");
	}
}

void generateAni(const index_header_t *h, const char *prefix, int zeropos, int plut, FILE *out)
{
	int i;
	int transparentPixel = -1;
	char fmt[40] = "%s.%";

	if (zeropos > 0)
		sprintf(fmt + strlen(fmt), "0%dd", zeropos);
	else
		strcat(fmt, "d");
	strcat(fmt, ".%s %d %d %d %d\n");
	
	for (i = 0; i < h->num_frames; i++)
	{
		frame_info_t* info = h->frames + i;
		frame_desc_t* desc = info->desc;
		int actplut = plut;
		
		if ((h->flags & GFX_HAS_IMAGES) && (h->flags & GFX_HAS_MASKS))
		{
			char tempbuf[sizeof("frame -2147483648")];
			sprintf(tempbuf, "frame %d", i);
			transparentPixel = findTransparentColour(tempbuf, info);
		}
		else if (h->flags & GFX_HAS_MASKS)
		{
			transparentPixel = 0; // masks always have index 0 transparent
			actplut = -1; // no colormap for masks
		}

		fprintf(out, fmt,
				prefix, (int)INDEX_GET(desc->index), "png",
				transparentPixel, actplut,
				(int)desc->hotspot.x - info->sx0,
				(int)desc->hotspot.y - info->sy0);
	}
}

void writeFile(const char *file, const uint8_t *data, size_t len)
{
	FILE* f;

	// check if all the path components exist
	{
		const char *ptr;
		char path[512];
		
		ptr = file;
		if (ptr[0] == '/')
			ptr++;
		while (1) {
			ptr = strchr(ptr, '/');
			if (ptr == NULL)
				break;
			ptr++;
			memcpy(path, file, ptr - file);
			path[ptr - file] = '\0';
			if (makedir(path) == -1)
			{
				if (errno == EEXIST)
					continue;
				verbose(1, "Error: Could not make directory %s: %s\n",
					path, strerror(errno));
				exit(EXIT_FAILURE);
			}
		}
	}
	
	f = fopen(file, "wb");
	if (!f)
	{
		perror("Could not open file for writing");
		exit(EXIT_FAILURE);
	}
	while (len > 0)
	{
		size_t written;
		written = fwrite(data, 1, len, f);
		if (!written)
		{
			if (errno != EINTR)
			{
				perror("write failed");
				exit(1);
			}
		}
		len -= written;
		data += written;
	}
	fclose(f);
}

// If there are no transparent pixels, returns -1
// Otherwise, returns an index in the palette that is never used so can be
// used for transparancy. If all palette entries are used, a warning is
// printed and 0 is used.
int findTransparentColour(const char *name, const frame_info_t* f)
{
	const uint8_t *src1, *src8;
	uint8_t mask;
	int y, x, i;
	int paletteUsed[PAL_COLORS];
	int haveTransparent;
		
	if (!f->data1)
		return -1; // transparency is not used in this image
	
	memset(paletteUsed, 0, sizeof (paletteUsed));
	haveTransparent = 0;

	for (y = 0; y < (int)f->desc->bounds.h; ++y)
	{
		src1 = f->data1 + f->desc->xsize1 * y;
		src8 = f->data8 + f->desc->xsize8 * y;

		for (x = 0, mask = 0x80; x < (int)f->desc->bounds.w; ++x, ++src8, mask >>= 1)
		{
			if (!mask)
			{	// next mask byte
				mask = 0x80;
				++src1;
			}

			if (*src1 & mask)
			{	// opaque pixel
				paletteUsed[*src8] = 1;
			}
			else
			{	// transparent pixel
				haveTransparent = 1;
			}
		}
	}

	if (!haveTransparent)
		return -1;
	
	for (i = 0; i < PAL_COLORS; i++)
	{
		if (!paletteUsed[i])
			return i;
	}
	verbose(2, "WARNING: Could not find an unused palette entry to use"
			" for transparency for %s\n", name);
	return -1;
}

void writePaletted(const char *filename, const frame_info_t* f)
{
	frame_desc_t* desc = f->desc;
	uint32_t bufsize;
	uint8_t *buf;
	const uint8_t *src8, *src1;
	uint8_t mask;
	uint8_t *dst;
	uint8_t **lines;
	int transparentPixel;
	int x, y;
	
	transparentPixel = findTransparentColour(filename, f);
	bufsize = desc->bounds.w * desc->bounds.h * sizeof(uint8_t);
	buf = alloca(bufsize);
	if (!buf)
	{
		verbose(1, "Out of stack while writing image\n");
		exit(EXIT_FAILURE);
	}
	memset(buf, 0, bufsize);

	lines = alloca(desc->bounds.h * sizeof(uint8_t *));

	for (y = 0; y < (int)desc->bounds.h; ++y)
	{
		src1 = f->data1 + desc->xsize1 * y;
		src8 = f->data8 + desc->xsize8 * y;
		dst = buf + desc->bounds.w * y;
		lines[y] = dst + f->sx0;

		for (x = 0, mask = 0x80; x < (int)desc->bounds.w; ++x, ++src8, ++dst, mask >>= 1)
		{
			if (!mask)
			{	// next mask byte
				mask = 0x80;
				++src1;
			}

			if (transparentPixel >= 0 && f->data1 && !(*src1 & mask))
				*dst = transparentPixel;
			else
				*dst = *src8;
		}
	}

	{
		FILE *file;
		png_structp png_ptr;
		png_infop info_ptr;
		png_color_8 sig_bit;
	
		png_ptr = png_create_write_struct
				(PNG_LIBPNG_VER_STRING, (png_voidp) NULL /* user_error_ptr */,
				NULL /* user_error_fn */, NULL /* user_warning_fn */);
		if (!png_ptr)
		{
			verbose(1, "png_create_write_struct failed.\n");
			exit(EXIT_FAILURE);
		}
	
		info_ptr = png_create_info_struct(png_ptr);
		if (!info_ptr)
		{
			png_destroy_write_struct(&png_ptr, (png_infopp) NULL);
			verbose(1, "png_create_info_struct failed.\n");
			exit(EXIT_FAILURE);
		}
		if (setjmp(png_jmpbuf(png_ptr)))
		{
			verbose(1, "png error.\n");
			png_destroy_write_struct(&png_ptr, &info_ptr);
			exit(EXIT_FAILURE);
		}

		file = fopen(filename, "wb");
		if (!file)
		{
			verbose(1, "Could not open file '%s': %s\n",
					filename, strerror(errno));
			png_destroy_write_struct(&png_ptr, &info_ptr);
			exit(EXIT_FAILURE);
		}
		png_init_io(png_ptr, file);
		png_set_IHDR(png_ptr, info_ptr,
				desc->bounds.w - f->sx0 - f->sx1,
				desc->bounds.h - f->sy0 - f->sy1,
				8 /* bit_depth per channel */, PNG_COLOR_TYPE_PALETTE,
				PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
				PNG_FILTER_TYPE_DEFAULT);
		sig_bit.red = 8;
		sig_bit.green = 8;
		sig_bit.blue = 8;
		png_set_sBIT(png_ptr, info_ptr, &sig_bit);
		png_set_PLTE(png_ptr, info_ptr, palette, PAL_COLORS);
		if (transparentPixel >= 0)
		{	// generate transparency chunk
			// for indexed PNGs, tRNS chunk contains an array of
			// alpha values corresponding to palette entries
			png_byte trans[PAL_COLORS];
			// set all palette alpha values to 0xff initially
			memset(trans, 0xff, PAL_COLORS * sizeof (png_byte));
			trans[transparentPixel] = 0; // the only one
			// only need to write out upto and including transparentPixel
			png_set_tRNS(png_ptr, info_ptr, trans, transparentPixel + 1, NULL);
		}
		png_write_info(png_ptr, info_ptr);
		png_write_image(png_ptr, lines + f->sy0);
		png_write_end(png_ptr, info_ptr);
		png_destroy_write_struct(&png_ptr, &info_ptr);
		fclose(file);
	}
}

#if 0
// this version writes out 1bit PNGs
void writeBitmapMask(const char *filename, const frame_info_t* f)
{
	uint8_t **lines;
	int y;

	lines = alloca(f->desc->bounds.h * sizeof (uint8_t *));

	for (y = 0; y < (int)f->desc->bounds.h; ++y)
		lines[y] = f->data1 + f->desc->xsize1 * y;

	{
		FILE *file;
		png_structp png_ptr;
		png_infop info_ptr;
	
		png_ptr = png_create_write_struct
				(PNG_LIBPNG_VER_STRING, (png_voidp) NULL /* user_error_ptr */,
				NULL /* user_error_fn */, NULL /* user_warning_fn */);
		if (!png_ptr)
		{
			verbose(1, "png_create_write_struct failed.\n");
			exit(EXIT_FAILURE);
		}
	
		info_ptr = png_create_info_struct(png_ptr);
		if (!info_ptr)
		{
			png_destroy_write_struct(&png_ptr, (png_infopp) NULL);
			verbose(1, "png_create_info_struct failed.\n");
			exit(EXIT_FAILURE);
		}
		if (setjmp(png_jmpbuf(png_ptr)))
		{
			verbose(1, "png error.\n");
			png_destroy_write_struct(&png_ptr, &info_ptr);
			exit(EXIT_FAILURE);
		}

		file = fopen(filename, "wb");
		if (!file)
		{
			verbose(1, "Could not open file '%s': %s\n",
					filename, strerror(errno));
			png_destroy_write_struct(&png_ptr, &info_ptr);
			exit(EXIT_FAILURE);
		}
		png_init_io(png_ptr, file);
		png_set_IHDR(png_ptr, info_ptr, f->desc->bounds.w, f->desc->bounds.h,
				1 /* bit_depth per channel */, PNG_COLOR_TYPE_GRAY,
				PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
				PNG_FILTER_TYPE_DEFAULT);
		png_write_info(png_ptr, info_ptr);
		png_write_image(png_ptr, (png_byte **) lines);
		png_write_end(png_ptr, info_ptr);
		png_destroy_write_struct(&png_ptr, &info_ptr);
		fclose(file);
	}
}
#else
// this version writes out 8bit PNGs with 2-color pal
void writeBitmapMask(const char *filename, const frame_info_t* f)
{
	frame_desc_t* desc = f->desc;
	uint32_t bufsize;
	uint8_t *buf;
	const uint8_t *src1;
	uint8_t mask;
	uint8_t *dst;
	uint8_t **lines;
	int x, y;
	
	bufsize = desc->bounds.w * desc->bounds.h * sizeof(uint8_t);
	buf = alloca(bufsize);
	if (!buf)
	{
		verbose(1, "Out of stack while writing image\n");
		exit(EXIT_FAILURE);
	}
	memset(buf, 0, bufsize);

	lines = alloca(desc->bounds.h * sizeof(uint8_t *));

	for (y = 0; y < (int)desc->bounds.h; ++y)
	{
		src1 = f->data1 + desc->xsize1 * y;
		dst = buf + desc->bounds.w * y;
		lines[y] = dst + f->sx0;

		for (x = 0, mask = 0x80; x < (int)desc->bounds.w; ++x, ++dst, mask >>= 1)
		{
			if (!mask)
			{	// next mask byte
				mask = 0x80;
				++src1;
			}

			*dst = (*src1 & mask) ? 1 : 0;
		}
	}

	{
		FILE *file;
		png_structp png_ptr;
		png_infop info_ptr;
		png_color_8 sig_bit;
		png_color bmppal[2] = {{0x00, 0x00, 0x00}, {0xff, 0xff, 0xff}};
		png_byte trans[2] = {0x00, 0xff};
	
		png_ptr = png_create_write_struct
				(PNG_LIBPNG_VER_STRING, (png_voidp) NULL /* user_error_ptr */,
				NULL /* user_error_fn */, NULL /* user_warning_fn */);
		if (!png_ptr)
		{
			verbose(1, "png_create_write_struct failed.\n");
			exit(EXIT_FAILURE);
		}
	
		info_ptr = png_create_info_struct(png_ptr);
		if (!info_ptr)
		{
			png_destroy_write_struct(&png_ptr, (png_infopp) NULL);
			verbose(1, "png_create_info_struct failed.\n");
			exit(EXIT_FAILURE);
		}
		if (setjmp(png_jmpbuf(png_ptr)))
		{
			verbose(1, "png error.\n");
			png_destroy_write_struct(&png_ptr, &info_ptr);
			exit(EXIT_FAILURE);
		}

		file = fopen(filename, "wb");
		if (!file)
		{
			verbose(1, "Could not open file '%s': %s\n",
					filename, strerror(errno));
			png_destroy_write_struct(&png_ptr, &info_ptr);
			exit(EXIT_FAILURE);
		}
		png_init_io(png_ptr, file);
		png_set_IHDR(png_ptr, info_ptr,
				desc->bounds.w - f->sx0 - f->sx1,
				desc->bounds.h - f->sy0 - f->sy1,
				8 /* bit_depth per channel */, PNG_COLOR_TYPE_PALETTE,
				PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
				PNG_FILTER_TYPE_DEFAULT);
		sig_bit.red = 8;
		sig_bit.green = 8;
		sig_bit.blue = 8;
		png_set_sBIT(png_ptr, info_ptr, &sig_bit);
		png_set_PLTE(png_ptr, info_ptr, bmppal, 2);
		// generate transparency chunk
		// only need to write out upto and including transparent index
		png_set_tRNS(png_ptr, info_ptr, trans, 1, NULL);
		png_write_info(png_ptr, info_ptr);
		png_write_image(png_ptr, lines + f->sy0);
		png_write_end(png_ptr, info_ptr);
		png_destroy_write_struct(&png_ptr, &info_ptr);
		fclose(file);
	}
}
#endif // 0 or 1

void writeFiles(const index_header_t *h, const char *path, const char *prefix, int zeropos)
{
	int i;
	char filename[512];
	char fmt[32] = "%s/%s.%";

	if (zeropos > 0)
		sprintf(fmt + strlen(fmt), "0%dd", zeropos);
	else
		strcat(fmt, "d");
	strcat(fmt, ".%s");

	for (i = 0; i < h->num_frames; i++)
	{
		frame_desc_t* desc = h->frames[i].desc;

		sprintf(filename, fmt, path, prefix, (int)INDEX_GET(desc->index), "png");

		if (desc->flags & GFX_DATA_FLAG2)
		{
			verbose(2, "Do not know the meaning of DATA_FLAG2, image index %d\n", i);
		}
		else if (h->flags & GFX_HAS_IMAGES)
		{
			writePaletted(filename, h->frames + i);
		}
		else if (h->flags & GFX_HAS_MASKS)
		{
			writeBitmapMask(filename, h->frames + i);
		}
		else
		{
			verbose(2, "Huh? Nothing in the frame pack? (Flags=%0x02x)\n", h->flags);
			return;
		}
	}
}

typedef struct
{
	uint8_t* buf;
	long len;
	long ptr;
} decomp_handle_t;

size_t decRead(void* buf, size_t size, size_t count, lzhuf_handle_t h)
{
	decomp_handle_t* p = (decomp_handle_t*)h;
	long cb;
	size_t crec;

	cb = count * size;
	if (cb > p->len - p->ptr)
		cb = p->len - p->ptr;
	crec = cb / size;
	cb = crec * size;
	if (!cb)
		return 0;

	memcpy(buf, p->buf + p->ptr, cb);
	p->ptr += cb;

	return crec;
}

size_t decWrite(const void* buf, size_t size, size_t count, lzhuf_handle_t h)
{
	decomp_handle_t* p = (decomp_handle_t*)h;
	long cb;
	size_t crec;

	cb = count * size;
	if (cb > p->len - p->ptr)
		cb = p->len - p->ptr;
	crec = cb / size;
	cb = crec * size;
	if (!cb)
		return 0;

	memcpy(p->buf + p->ptr, buf, cb);
	p->ptr += cb;

	return crec;
}

int decSeek(lzhuf_handle_t h, long ofs, int origin)
{
	decomp_handle_t* p = (decomp_handle_t*)h;
	long newptr;

	switch (origin)
	{
	case SEEK_SET:
		newptr = ofs;
		break;
	case SEEK_CUR:
		newptr = p->ptr + ofs;
		break;
	case SEEK_END:
		newptr = p->len + ofs;
		break;
	}
	if (newptr < 0 || newptr > p->len)
		return EOF;
	p->ptr = newptr;
	return 0;
}

long decTell(lzhuf_handle_t h)
{
	decomp_handle_t* p = (decomp_handle_t*)h;
	return p->ptr;
}

int decompressData(uint8_t* src, uint32_t srclen, uint8_t* dst, uint32_t dstlen)
{
	lzhuf_session_t sess;
	decomp_handle_t readh = {src, srclen, 0};
	decomp_handle_t writeh = {dst, dstlen, 0};
	int ret;

	lzhuf_startSession(&sess, 0);
	sess.infile = &readh;
	sess.outfile = &writeh;
	sess.read = decRead;
	sess.write = decWrite;
	sess.seek = decSeek;
	sess.tell = decTell;
	ret = lzhuf_decode(&sess);
	lzhuf_endSession(&sess);

	if (ret != 0)
		return EXIT_FAILURE;

	return writeh.ptr;
}

inline uint32_t getLzDecompressedSize(void* lzdata)
{
	return get_32_le((uint32_t*)lzdata);
}

int decompressFrames(const index_header_t* h)
{
#define DEC_REASONABLE_SIZE 0x100000
	int i;

	for (i = 0; i < h->num_frames; i++)
	{
		frame_info_t* info = h->frames + i;
		frame_desc_t* desc = info->desc;

		if ((desc->mflags & GFX_DATA_COMPRESSED) && info->csize1 != 0 && info->data1 != 0)
		{
			int declen;
			uint32_t buflen = desc->size1;
			uint8_t* buf = 0;
			uint32_t lzlen = getLzDecompressedSize(info->data1);

			if (buflen != lzlen)
				verbose(2, "WARNING: decompressed size in LZ data does not match the descriptor, frame index %d\n", i);
			if (buflen < lzlen)
				buflen = lzlen;
			if (buflen > DEC_REASONABLE_SIZE)
				buflen = DEC_REASONABLE_SIZE;
			buf = malloc(buflen);
			if (!buf)
			{
				verbose(1, "Out of memory decompressing mask bitmap, frame index %d\n", i);
				return EXIT_FAILURE;
			}
			
			declen = decompressData(info->data1, info->csize1, buf, buflen);
			if (declen <= 0)
			{
				verbose(1, "Decompression of mask bitmap failed, frame index %d\n", i);
				free(buf);
				return EXIT_FAILURE;
			}
			if (declen != (int)desc->size1)
				verbose(2, "WARNING: Decompressed mask bitmap data length does not match the descriptor, frame index %d\n", i);

			info->malloced |= 1;
			info->data1 = realloc(buf, declen);
			info->csize1 = declen;
		}

		if ((desc->flags & GFX_DATA_COMPRESSED) && info->csize8 != 0 && info->data8 != 0)
		{
			int declen;
			uint32_t buflen = desc->size8;
			uint8_t* buf = 0;
			uint32_t lzlen = getLzDecompressedSize(info->data8);

			if (buflen != lzlen)
				verbose(2, "WARNING: decompressed size in LZ data does not match the descriptor, frame index %d\n", i);
			if (buflen < lzlen)
				buflen = lzlen;
			if (buflen > DEC_REASONABLE_SIZE)
				buflen = DEC_REASONABLE_SIZE;
			buf = malloc(buflen);
			if (!buf)
			{
				verbose(1, "Out of memory decompressing image, frame index %d\n", i);
				return EXIT_FAILURE;
			}
			
			declen = decompressData(h->frames[i].data8, h->frames[i].csize8, buf, buflen);
			if (declen <= 0)
			{
				verbose(1, "Decompression of image failed, frame index %d\n", i);
				free(buf);
				return EXIT_FAILURE;
			}
			if (declen != (int)desc->size8)
				verbose(2, "WARNING: Decompressed image data length does not match the descriptor, frame index %d\n", i);

			info->malloced |= 2;
			info->data8 = realloc(buf, declen);
			info->csize8 = declen;
		}
	}
	
	return 0;
}

int cropFrame(frame_info_t* f)
{
	frame_desc_t* desc = f->desc;
	const uint8_t *src1;
	uint8_t mask;
	int y, x;
	int mcx0, mcx1, mcy0, mcy1;
	int cx0, cx1;
	int sawOpaqueX, sawOpaqueY;
	
	if (!f->data1)
		return 0; // cannot crop - transparency is not used in this image
	
	mcx0 = mcx1 = desc->bounds.w;
	mcy0 = mcy1 = 0;
	
	sawOpaqueY = 0;

	for (y = 0; y < (int)f->desc->bounds.h; ++y)
	{
		src1 = f->data1 + f->desc->xsize1 * y;

		sawOpaqueX = 0;
		cx0 = cx1 = 0;

		for (x = 0, mask = 0x80; x < (int)f->desc->bounds.w; ++x, mask >>= 1)
		{
			if (!mask)
			{	// next mask byte
				mask = 0x80;
				++src1;
			}

			if (*src1 & mask)
			{	// opaque pixel
				sawOpaqueX = 1;
				cx1 = 0;
			}
			else
			{	// transparent pixel
				if (sawOpaqueX)
					cx1++;
				else
					cx0++;
			}
		}

		if (cx0 < mcx0)
			mcx0 = cx0;
		if (cx1 < mcx1 && cx0 < desc->bounds.w)
			mcx1 = cx1;

		if (sawOpaqueX)
		{
			sawOpaqueY = 1;
			mcy1 = 0;
		}
		else
			mcy1++;

		if (!sawOpaqueY)
			mcy0++;
	}

	if (mcy0 == desc->bounds.h)
	{	// no opaque pixels!?
		verbose(2, "WARNING: Completely empty image detected\n");
		// generate a 1x1 image
		mcx0 = 0;
		mcx1 = desc->bounds.w - 1;
		mcy0 = 0;
		mcy1 = desc->bounds.h - 1;
	}

	f->sx0 = mcx0;
	f->sx1 = mcx1;
	f->sy0 = mcy0;
	f->sy1 = mcy1;

	return (mcx0 + mcx1 + mcy0 + mcy1 != 0);
}

int cropFrames(const index_header_t* h)
{
	int i;

	if (!(h->flags & GFX_HAS_MASKS))
		return 0; // cannot crop w/o masks

	for (i = 0; i < h->num_frames; i++)
	{
		frame_info_t* info = h->frames + i;
		//frame_desc_t* desc = info->desc;
		int cropped;
		
		//if ((h->flags & GFX_HAS_IMAGES) && !(desc->flags & GFX_DATA_MASK))
		//	continue;

		cropped = cropFrame(info);
		if (cropped)
			verbose(2, "Frame %02d cropped x0=%d x1=%d y0=%d y1=%d\n",
					i, info->sx0, info->sx1, info->sy0, info->sy1);
	}
	
	return 0;
}

int loadPalette(const char *filename)
{
	FILE* in;
	size_t inlen;
	uint8_t *buf;
	colormap_t* map;
	color_t* clr;
	int i;

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

	map = (colormap_t*)buf;

	verbose(2, "%s contains %d palette entries\n",
			filename, (int)buf[1] - buf[0] + 1);
	
	for (i = map->first, clr = map->pal.colors; i <= (int)map->last; ++i, ++clr)
	{
		palette[i].red   = conv_chan(clr->r);
		palette[i].green = conv_chan(clr->g);
		palette[i].blue  = conv_chan(clr->b);
	}

	free(buf);

	return 0;
}

inline channel_t conv_chan(uint8_t c)
{	// this uniformly distributes the lower 'unknown' bits
	return (c << (8 - DOS_SIG_BITS)) | (c >> (DOS_SIG_BITS - (8 - DOS_SIG_BITS)));
}

inline uint16_t get_16_le(uint16_t* val)
{
	return (uint16_t)(((uint8_t*)val)[1] << 8) | ((uint8_t*)val)[0];
}

inline uint32_t get_32_le(uint32_t* val)
{
	return (uint32_t)(get_16_le((uint16_t*)val + 1) << 16) | get_16_le((uint16_t*)val);
}
