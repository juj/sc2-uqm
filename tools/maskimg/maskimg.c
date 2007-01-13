/*
 * Truecolor image masking (transparency) tool
 * By Alex Volkov (codepro@usa.net), 20050222
 *
 * The GPL applies
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
#	define inline __inline
#else
#	include <unistd.h>
#endif
#if defined(__GNUC__)
#	include <alloca.h>
#	define inline __inline__
#endif 
#include <png.h>

#define countof(a)	   ( sizeof(a)/sizeof(*a) )
#ifndef offsetof
#	define offsetof(s,m)  ( (size_t)(&((s*)0)->m) )
#endif

struct options
{
	const char *infile;
	const char *outfile;
	char *maskfile;
	int maskoff;
	int maskon;
	int alpha;
	int verbose;
};

typedef struct
{
	png_structp png_ptr;
	png_infop info_ptr;
	png_infop end_info;

	png_uint_32 w, h;
	int bit_depth;
	int color_type;
	int interlace_type;
	int compression_type;
	int filter_type;

	png_color_8p sig_bit;
	png_color_16p trans_values;
	png_color_16 trans_values_buf;
	png_color_16 trans_scaled;
	png_bytep trans;
	int num_trans;
	int trans_index;
	int srgb_intent;

	int channels;
	int bpp;
	png_bytep data;
	png_bytep* lines;

} image_t;

int verbose_level = 0;
void verbose(int level, const char* fmt, ...);
void parse_arguments(int argc, char *argv[], struct options *opts);

image_t* readImage(const char* filename);
int writeImage(image_t *, const char* filename);
void freeImage(image_t *);
int getImageTransIndex(image_t *);
void maskImage(image_t *, image_t* mask, int maskon, int maskoff);
void alphaImage(image_t *, image_t* mask);

int main(int argc, char *argv[])
{
	struct options opts;
	image_t* img;
	image_t* mask;

	parse_arguments(argc, argv, &opts);
	verbose_level = opts.verbose;

	img = readImage(opts.infile);
	if (!img)
		return EXIT_FAILURE;
	if (img->color_type != PNG_COLOR_TYPE_RGB)
	{
		verbose(1, "Input image must be RGB, 8 bits per channel (or fix the code)\n");
		freeImage(img);
		return EXIT_FAILURE;
	}
	if (!img->trans_values)
	{
		verbose(2, "%s has no tRNS chunk, assuming transparent black (RBG 0,0,0)\n",
				opts.infile);
		img->trans_values = &img->trans_values_buf;
	}
	img->num_trans = 0;

	mask = readImage(opts.maskfile);
	if (!mask)
	{
		freeImage(img);
		return EXIT_FAILURE;
	}
	if (mask->w < img->w || mask->h < img->h)
	{
		verbose(1, "Mask image dimensions are smaller than input image\n");
		freeImage(img);
		freeImage(mask);
		return EXIT_FAILURE;
	}
	else if (mask->w != img->w || mask->h != img->h)
		verbose(2, "Warning: input image and mask have different dimensions\n");

	if (mask->color_type != PNG_COLOR_TYPE_GRAY && mask->color_type != PNG_COLOR_TYPE_PALETTE)
	{
		verbose(1, "Mask image must be grayscale or paletted (or fix the code)\n");
		freeImage(img);
		freeImage(mask);
		return EXIT_FAILURE;
	}
	if (opts.alpha && (mask->color_type & PNG_COLOR_MASK_PALETTE))
	{
		verbose(2, "Warning: ignoring palette in mask image; using indices\n");
	}
	if (opts.alpha && mask->bit_depth != 8)
	{
		verbose(1, "Mask image for the alpha channel must be 8bpp\n");
		freeImage(img);
		freeImage(mask);
		return EXIT_FAILURE;
	}
	if (!opts.alpha && mask->trans)
	{
		mask->trans_index = getImageTransIndex(mask);
		verbose(2, "Using mask image transparency info; trans index %d\n", mask->trans_index);
	}
	else if (!opts.alpha)
	{
		mask->trans_index = 0;
		verbose(2, "Mask image has no transparency info; using index %d\n", mask->trans_index);
	}

	if (!opts.maskon && !opts.maskoff && !opts.alpha)
	{
		freeImage(img);
		freeImage(mask);
		verbose(1, "Nothing to do; specify -0 or -1, or use -a\n");
		return EXIT_SUCCESS;
	}

	if (opts.alpha && (opts.maskon || opts.maskoff))
	{
		verbose(2, "Options -0 and -1 have no effect when -a specified\n");
	}

	if (opts.alpha)
		alphaImage(img, mask);
	else
		maskImage(img, mask, opts.maskon, opts.maskoff);
	
	writeImage(img, opts.outfile);

	freeImage(img);
	freeImage(mask);

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
			"maskimg [-0] [-1] [-a] [-o <outfile>] -m <maskfile> <infile>\n"
			"Options:\n"
			"\t-o  write resulting png into <outfile>\n"
			"\t-a  use mask image as alpha channel and create result with alpha\n"
			"\t-m  specifies mask image to use (must be present)\n"
			"\t-0  mask pixels off by setting them to transparent color\n"
			"\t-1  mask pixels on by bumping red channel slightly\n"
			"\t-v  increase verbosity level (use more than once)\n"
			);
}

void parse_arguments(int argc, char *argv[], struct options *opts)
{
	char ch;
	
	memset(opts, 0, sizeof (struct options));

	while (-1 != (ch = getopt(argc, argv, "h?ao:m:01v")))
	{
		switch (ch)
		{
		case 'o':
			opts->outfile = optarg;
			break;
		case 'a':
			opts->alpha = 1;
			break;
		case 'm':
			opts->maskfile = optarg;
			break;
		case '0':
			opts->maskoff = 1;
			break;
		case '1':
			opts->maskon = 1;
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

	if (!opts->maskfile)
	{
		fprintf(stderr, "No mask image specified, use -m\n");
		usage();
		exit(EXIT_FAILURE);
	}
}

image_t* readImage(const char* filename)
{
	image_t* img;
	uint8_t header[8];
	size_t cbhead;
	FILE* fp;

	fp = fopen(filename, "rb");
	if (!fp)
	{
		verbose(1, "Error: Could not open file %s: %s\n",
				filename, strerror(errno));
		return NULL;
	}

	cbhead = fread(header, 1, sizeof(header), fp);
	if (png_sig_cmp(header, 0, cbhead))
	{
		verbose(1, "Input file is not a PNG image\n");
		fclose(fp);
		return NULL;
	}

	img = malloc(sizeof(*img));
	if (!img)
	{
		verbose(1, "Out of memory reading image\n");
		fclose(fp);
		return NULL;
	}
	memset(img, 0, sizeof(*img));

	img->png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!img->png_ptr)
	{
		verbose(1, "png_create_read_struct failed\n");
		fclose(fp);
		return NULL;
	}
	img->info_ptr = png_create_info_struct(img->png_ptr);
	if (!img->info_ptr)
	{
		verbose(1, "png_create_info_struct failed\n");
		png_destroy_read_struct(&img->png_ptr, NULL, NULL);
		fclose(fp);
		return NULL;
	}

	img->end_info = png_create_info_struct(img->png_ptr);
	if (!img->end_info)
	{
		verbose(1, "png_create_info_struct failed\n");
		png_destroy_read_struct(&img->png_ptr, &img->info_ptr, NULL);
		fclose(fp);
		return NULL;
	}

	if (setjmp(png_jmpbuf(img->png_ptr)))
	{
		verbose(1, "PNG error\n");
		png_destroy_read_struct(&img->png_ptr, &img->info_ptr, &img->end_info);
		fclose(fp);
		return NULL;
	}

	png_init_io(img->png_ptr, fp);
	png_set_sig_bytes(img->png_ptr, cbhead);

	png_read_info(img->png_ptr, img->info_ptr);

	png_get_IHDR(img->png_ptr, img->info_ptr, &img->w, &img->h, &img->bit_depth,
			&img->color_type, &img->interlace_type, &img->compression_type,
			&img->filter_type);
	png_get_tRNS(img->png_ptr, img->info_ptr, &img->trans, &img->num_trans, &img->trans_values);
	if (img->trans_values)
		img->trans_scaled = *img->trans_values;
	png_get_sBIT(img->png_ptr, img->info_ptr, &img->sig_bit);
	if (!png_get_sRGB(img->png_ptr, img->info_ptr, &img->srgb_intent))
		img->srgb_intent = -1;
	img->channels = png_get_channels(img->png_ptr, img->info_ptr);

	if (img->bit_depth < 8)
	{	// make sure we get 1 pixel per byte
		png_set_packing(img->png_ptr);
	}
	else if (img->bit_depth == 16)
	{	// or 1 byte per channel
		png_set_strip_16(img->png_ptr);
		img->bit_depth = 8;
		if (img->sig_bit)
		{
			if (img->sig_bit->red > 8) img->sig_bit->red = 8;
			if (img->sig_bit->green > 8) img->sig_bit->green = 8;
			if (img->sig_bit->blue > 8) img->sig_bit->blue = 8;
			if (img->sig_bit->alpha > 8) img->sig_bit->alpha = 8;
			if (img->sig_bit->gray > 8) img->sig_bit->gray = 8;
		}
		img->trans_scaled.red >>= 8;
		img->trans_scaled.green >>= 8;
		img->trans_scaled.blue >>= 8;
	}
	else if (img->sig_bit)
	{	// bit_depth == 8
		png_set_shift(img->png_ptr, img->sig_bit);
		img->trans_scaled.red >>= 8 - img->sig_bit->red;
		img->trans_scaled.green >>= 8 - img->sig_bit->green;
		img->trans_scaled.blue >>= 8 - img->sig_bit->blue;
	}

	img->bpp = img->channels;
	img->data = malloc(img->bpp * img->w * img->h);
	if (!img->data)
	{
		verbose(1, "Out of memory reading image\n");
		png_destroy_read_struct(&img->png_ptr, &img->info_ptr, &img->end_info);
		fclose(fp);
		return NULL;
	}
	img->lines = malloc(img->h * sizeof(img->lines[0]));
	if (!img->lines)
	{
		verbose(1, "Out of memory reading image\n");
		png_destroy_read_struct(&img->png_ptr, &img->info_ptr, &img->end_info);
		fclose(fp);
		return NULL;
	}
	{	// init lines
		png_uint_32 i;
		for (i = 0; i < img->h; ++i)
			img->lines[i] = img->data + img->w * img->bpp * i;
	}
	png_read_image(img->png_ptr, img->lines);
	img->interlace_type = PNG_INTERLACE_NONE;

	png_read_end(img->png_ptr, img->end_info);

	fclose(fp);

	return img;
}

void freeImage(image_t* img)
{
	if (!img)
		return;

	png_destroy_read_struct(&img->png_ptr, &img->info_ptr, &img->end_info);
	if (img->lines)
		free(img->lines);
	if (img->data)
		free(img->data);
	free(img);
}

int writeImage(image_t* img, const char* filename)
{
	FILE *file;
	png_structp png_ptr;
	png_infop info_ptr;

	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr)
	{
		verbose(1, "png_create_write_struct failed.\n");
		return EXIT_FAILURE;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
	{
		png_destroy_write_struct(&png_ptr, (png_infopp) NULL);
		verbose(1, "png_create_info_struct failed.\n");
		return EXIT_FAILURE;
	}
	if (setjmp(png_jmpbuf(png_ptr)))
	{
		verbose(1, "png error.\n");
		png_destroy_write_struct(&png_ptr, &info_ptr);
		return EXIT_FAILURE;
	}

	file = fopen(filename, "wb");
	if (!file)
	{
		verbose(1, "Could not open file '%s': %s\n",
				filename, strerror(errno));
		png_destroy_write_struct(&png_ptr, &info_ptr);
		return EXIT_FAILURE;
	}
	png_init_io(png_ptr, file);
	png_set_IHDR(png_ptr, info_ptr, img->w, img->h, img->bit_depth,
			img->color_type, img->interlace_type, img->compression_type,
			img->filter_type);
	if (img->sig_bit)
	{
		png_set_sBIT(png_ptr, info_ptr, img->sig_bit);
		png_set_shift(png_ptr, img->sig_bit);
	}
	if (!(img->color_type & (PNG_COLOR_MASK_PALETTE | PNG_COLOR_MASK_ALPHA))
			&& img->trans_values)
	{
		png_set_tRNS(png_ptr, info_ptr, NULL, 0, img->trans_values);
	}
	png_write_info(png_ptr, info_ptr);
	png_set_packing(png_ptr);
	png_write_image(png_ptr, (png_byte **) img->lines);
	png_write_end(png_ptr, img->end_info);
	png_destroy_write_struct(&png_ptr, &info_ptr);

	fclose(file);

	return EXIT_SUCCESS;
}

int getImageTransIndex(image_t* img)
{
	int i;

	for (i = 0; i < img->num_trans && img->trans[i] != 0; ++i)
		;
	
	return (i < img->num_trans) ? i : 0;
}

void maskImage(image_t* img, image_t* mask, int maskon, int maskoff)
{
	png_uint_32 x, y;
	int img_pitch, mask_pitch;
	png_bytep imgp, maskp;

	img_pitch = img->w * img->bpp;
	mask_pitch = mask->w * mask->bpp;

	for (y = 0; y < img->h; ++y)
	{
		imgp = img->lines[y];
		maskp = mask->lines[y];

		for (x = 0; x < img->w; ++x, imgp += img->bpp, maskp += mask->bpp)
		{
			if (maskoff && maskp[0] == mask->trans_index)
			{	// set the pixel to transparent values
				imgp[0] = (png_byte)img->trans_scaled.red;
				imgp[1] = (png_byte)img->trans_scaled.green;
				imgp[2] = (png_byte)img->trans_scaled.blue;
			}
			if (maskon && maskp[0] != mask->trans_index
					&& imgp[0] == img->trans_scaled.red
					&& imgp[1] == img->trans_scaled.green
					&& imgp[2] == img->trans_scaled.blue
				)
			{	// mask the pixel on by bumping the red channel away from transparent value
				if (imgp[0] == 0xff)
					--imgp[0];
				else
					++imgp[0];
			}
		}
	}
}

void alphaImage(image_t* img, image_t* mask)
{
	png_uint_32 x, y;
	//png_bytep imgp, maskp;
	int dpitch;
	png_bytep newdata;

	dpitch = img->w * 4;
	newdata = malloc(img->h * dpitch);
	if (!newdata)
	{
		verbose(1, "Out of memory converting image to rgba\n");
		exit(EXIT_FAILURE);
	}

	for (y = 0; y < img->h; ++y)
	{
		png_bytep sp = img->lines[y];
		png_bytep dp = newdata + dpitch * y;
		png_bytep mp = mask->lines[y];

		for (x = 0; x < img->w; ++x, sp += img->bpp, dp += 4, mp += mask->bpp)
		{
			if (img->sig_bit)
			{
				png_byte c;

				c = sp[0] << (8 - img->sig_bit->red);
				dp[0] = c | (c >> img->sig_bit->red);
				c = sp[1] << (8 - img->sig_bit->green);
				dp[1] = c | (c >> img->sig_bit->green);
				c = sp[2] << (8 - img->sig_bit->blue);
				dp[2] = c | (c >> img->sig_bit->blue);
			}
			else
			{
				dp[0] = sp[0];
				dp[1] = sp[1];
				dp[2] = sp[2];
			}
			dp[3] = mp[0];
		}
	}

	if (img->sig_bit)
	{
		img->sig_bit->red = 8;
		img->sig_bit->green = 8;
		img->sig_bit->blue = 8;
		img->sig_bit->alpha = 8;
		img->sig_bit->gray = 8;
	}

	img->trans_values = 0;
	img->color_type = PNG_COLOR_TYPE_RGB_ALPHA;
	img->channels = 4;
	img->bpp = 4;

	free(img->data);
	img->data = newdata;
	{	// init lines
		png_uint_32 i;
		for (i = 0; i < img->h; ++i)
			img->lines[i] = img->data + img->w * img->bpp * i;
	}
}
