/*
 * UQM Starmap image generator
 * SDL/PNG driver implementation
 *
 * Uses: SDL, SDL_image, SDL_ttf, parts of SDL_gfx, and libpng
 *
 *
 * The GPL applies
 *
 */

#include "mapdrv.h"
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>
#include "SDL.h"
#include "SDL_image.h"
#include "SDL_ttf.h"
#include "SDL_gfxPrimitives.h"
#include <png.h>

struct mg_font
{
	char* name;
	TTF_Font* font;
	int size;
};

struct mg_image
{
	SDL_Surface* img;
	double width;
	double height;
};

static int drv_init(int argc, char *argv[]);
static int drv_term(void);
static void drv_usage(void);
static int drv_setResolution(int dpi_x, int dpi_y);
static int drv_begin(double w, double h);
static int drv_write(void);
static int drv_end(void);
static mg_font_t drv_loadFont(const char* name, double size, const char*);
static void drv_freeFont(mg_font_t);
static mg_image_t drv_loadImage(const char*);
static void drv_freeImage(mg_image_t);
static int drv_getImageSize(mg_image_t, mg_rectf_t*);

static void drv_setClipRect(const mg_rectf_t*);
static void drv_drawPixel(double x, double y, mg_color_t, double weight);
static void drv_drawLine(double x0, double y0, double x1, double y1, mg_color_t, double weight);
static void drv_drawRect(const mg_rectf_t*, mg_color_t color, double weight);
static void drv_drawFilledRect(const mg_rectf_t*, mg_color_t color);
static void drv_drawEllipse(double xc, double yc, double xr, double yr, mg_color_t, double weight);
static void drv_drawFilledEllipse(double xc, double yc, double xr, double yr, mg_color_t);
static void drv_drawImage(double x, double y, mg_image_t);
static void drv_drawText(mg_font_t, double x, double y, const unsigned char* utf8str, mg_color_t);
static void drv_getTextSize(mg_font_t, const unsigned char* utf8str, mg_rectf_t*);

const mg_driver_t sdlpng_drv = 
{
	"sdlpng", // name
	"SDL/PNG image writer", // description

	// func ptrs
	drv_init,
	drv_term,
	drv_usage,
	drv_setResolution,
	drv_begin,
	drv_write,
	drv_end,
	drv_loadFont,
	drv_freeFont,
	drv_loadImage,
	drv_freeImage,
	drv_getImageSize,

	// draw func ptrs
	drv_setClipRect,
	drv_drawPixel,
	drv_drawLine,
	drv_drawRect,
	drv_drawFilledRect,
	drv_drawEllipse,
	drv_drawFilledEllipse,
	drv_drawImage,
	drv_drawText,
	drv_getTextSize,
	
};

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
#	define R_MASK 0xff000000
#	define G_MASK 0x00ff0000
#	define B_MASK 0x0000ff00
#	define A_MASK 0x000000ff

#	define PNG_R_MASK 0x00ff0000
#	define PNG_G_MASK 0x0000ff00
#	define PNG_B_MASK 0x000000ff
#	define PNG_A_MASK 0xff000000

#else
#	define R_MASK 0x00ff0000
#	define G_MASK 0x0000ff00
#	define B_MASK 0x000000ff
#	define A_MASK 0xff000000

#	define PNG_R_MASK 0x000000ff
#	define PNG_G_MASK 0x0000ff00
#	define PNG_B_MASK 0x00ff0000
#	define PNG_A_MASK 0xff000000
#endif

static int parseArguments(int argc, char* argv[]);

static SDL_PixelFormat* imgfmt;
static SDL_Surface* fmtimg;
static SDL_Surface* outimg;

static int use32bpp;
static const char* fontdir = ".";
static const char* outdev = "starmap.png";
static FILE* fout;
static int dpix, dpiy;

static int drv_init(int argc, char *argv[])
{
	int ret;
	ret = parseArguments(argc, argv);
	if (ret)
		return ret;

	if (SDL_Init(SDL_INIT_NOPARACHUTE))
	{
		mg_verbose(1, "Cannot init SDL\n");
		return EXIT_FAILURE;
	}

	if (TTF_Init())
	{
		mg_verbose(1, "Cannot init SDL_ttf\n");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static int drv_term(void)
{
	imgfmt = 0;
	if (fmtimg)
		SDL_FreeSurface(fmtimg);
	fmtimg = 0;
	if (outimg)
		SDL_FreeSurface(outimg);
	outimg = 0;

	if (fout)
		fclose(fout);
	fout = 0;

	TTF_Quit();
	SDL_Quit();

	return EXIT_SUCCESS;
}

static void drv_usage(void)
{
	mg_verbose(0,
			"sdlpng driver : [-f <fontdir>] [-n] [-o <outfile>]\n"
			"Options:\n"
			"\t-f  dir where font files are located\n"
			"\t-n  use native 32bpp surface for rendering\n"
			"\t    doubles memory requirement but may increase quality in some cases\n"
			"\t-o  save image as <outfile> (default is 'starmap.png')\n"
			);
}

static int parseArguments(int argc, char* argv[])
{
	int ch;
	
	while (-1 != (ch = getopt(argc, argv, "-f:no:")))
	{
		switch (ch)
		{
		case 'f':
			fontdir = optarg;
			break;
		case 'n':
			use32bpp = 1;
			break;
		case 'o':
			outdev = optarg;
			break;
		default:
			// non-option -- probably an arg to unknown option
			;
		}
	}
	
	return EXIT_SUCCESS;
}

static int drv_setResolution(int dpi_x, int dpi_y)
{
	dpix = dpi_x;
	dpiy = dpi_y;

	return EXIT_SUCCESS;
}

static inline int inchToPixelsX(double inch)
{
	int v = (int)(inch * dpix);
	// make sure it's at least 1 pixel wide if present
	if (!v && inch)
		v = 1;
	return v;
}

static inline int inchToPixelsY(double inch)
{
	int v = (int)(inch * dpiy);
	// make sure it's at least 1 pixel wide if present
	if (!v && inch)
		v = 1;
	return v;
}

static inline double pixelsToInchX(int pixels)
{
	return (double)pixels / dpix;
}

static inline double pixelsToInchY(int pixels)
{
	return (double)pixels / dpiy;
}

static int drv_begin(double w, double h)
{
	int targetbpp;

	fout = fopen(outdev, "wb");
	if (!fout)
	{
		mg_verbose(1, "Cannot open file '%s' -- %s\n", outdev, strerror(errno));
		return EXIT_FAILURE;
	}

	if (use32bpp)
	{	// SDL-native channel order; allows exact accelerated blits
		fmtimg = SDL_CreateRGBSurface(SDL_SWSURFACE, 0, 0, 32,
				R_MASK, G_MASK, B_MASK, A_MASK);
		targetbpp = 32;
	}
	else
	{	// PNG-native channel order; saves memory
		fmtimg = SDL_CreateRGBSurface(SDL_SWSURFACE, 0, 0, 32,
				PNG_R_MASK, PNG_G_MASK, PNG_B_MASK, PNG_A_MASK);
		targetbpp = 24;
	}
	
	if (!fmtimg)
	{
		mg_verbose(1, "Cannot create format image\n");
		return EXIT_FAILURE;
	}
	imgfmt = fmtimg->format;

	outimg = SDL_CreateRGBSurface(SDL_SWSURFACE, inchToPixelsX(w), inchToPixelsY(h),
			targetbpp, imgfmt->Rmask, imgfmt->Gmask, imgfmt->Bmask, 0);
	if (!outimg)
	{
		mg_verbose(1, "Cannot create work image\n");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static int drv_end(void)
{
	imgfmt = 0;
	if (fmtimg)
		SDL_FreeSurface(fmtimg);
	fmtimg = 0;
	if (outimg)
		SDL_FreeSurface(outimg);
	outimg = 0;

	if (fout)
		fclose(fout);
	fout = 0;

	return EXIT_SUCCESS;
}

static int drv_write(void)
{
	png_structp png_ptr;
	png_infop info_ptr;
	SDL_Surface* cimg = NULL;
	SDL_Surface* wimg = NULL;
	SDL_PixelFormat* fmt;
	png_bytep* lines = NULL;
	int i;
	
	if (!outimg || !fout)
		return EXIT_FAILURE;

	mg_verbose(2, "Writing image to '%s'\n", outdev);

	fmt = outimg->format;
	if (fmt->BytesPerPixel != 3 ||
			fmt->Rmask != PNG_R_MASK ||
			fmt->Gmask != PNG_G_MASK ||
			fmt->Bmask != PNG_B_MASK)
	{	// convert the generated image to format suitable for libpng
		cimg = SDL_CreateRGBSurface(SDL_SWSURFACE, outimg->w, outimg->h, 24,
				PNG_R_MASK, PNG_G_MASK, PNG_B_MASK, 0);
		if (!cimg)
			return EXIT_FAILURE;
		SDL_BlitSurface(outimg, NULL, cimg, NULL);
		wimg = cimg;
	}
	else
	{	// use the generated image 'as is'
		wimg = outimg;
	}

	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
			NULL, NULL, NULL);
	if (!png_ptr)
	{
		mg_verbose(1, "png_create_write_struct failed.\n");
		return EXIT_FAILURE;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
	{
		png_destroy_write_struct(&png_ptr, (png_infopp) NULL);
		mg_verbose(1, "png_create_info_struct failed.\n");
		return EXIT_FAILURE;
	}
	if (setjmp(png_jmpbuf(png_ptr)))
	{
		mg_verbose(1, "png error.\n");
		png_destroy_write_struct(&png_ptr, &info_ptr);
		return EXIT_FAILURE;
	}

	lines = calloc(wimg->h, sizeof(png_bytep));
	if (!lines)
		return EXIT_FAILURE;
	for (i = 0; i < wimg->h; ++i)
		lines[i] = (Uint8*)wimg->pixels + i * wimg->pitch;

	png_init_io(png_ptr, fout);
	png_set_IHDR(png_ptr, info_ptr, wimg->w, wimg->h, 8,
			PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
			PNG_COMPRESSION_TYPE_DEFAULT,
			PNG_FILTER_TYPE_BASE);
	png_set_compression_level(png_ptr, Z_BEST_COMPRESSION);
	png_set_pHYs(png_ptr, info_ptr, dpix * 10000 / 254, dpiy * 10000 / 254,
			PNG_RESOLUTION_METER);
	png_write_info(png_ptr, info_ptr);
	png_set_packing(png_ptr);
	
	png_write_image(png_ptr, lines);
	
	png_write_end(png_ptr, info_ptr);
	png_destroy_write_struct(&png_ptr, &info_ptr);

	if (cimg)
		SDL_FreeSurface(cimg);
	if (lines)
		free(lines);

	return EXIT_SUCCESS;
}

static TTF_Font* openFont(const char* filename, int size)
{
	char buf[512];
	char namebuf[128];
	TTF_Font* font;

	strncpy(namebuf, filename, sizeof(namebuf));
	namebuf[sizeof(namebuf) - 1] = '\0';

	// first try the proper case filename
	snprintf(buf, sizeof(buf), "%s/%s", fontdir, namebuf);
	font = TTF_OpenFont(buf, size);
	if (font)
		return font;

	// then try the lowercase filename
	strlwr(namebuf);
	snprintf(buf, sizeof(buf), "%s/%s", fontdir, namebuf);
	font = TTF_OpenFont(buf, size);
	if (font)
		return font;

	// last try the uppercase filename
	strupr(namebuf);
	snprintf(buf, sizeof(buf), "%s/%s", fontdir, namebuf);
	font = TTF_OpenFont(buf, size);
	if (font)
		return font;

	mg_verbose(2, "TTF_OpenFont(): %s\n", TTF_GetError());

	return 0;
}

static mg_font_t drv_loadFont(const char* name, double size, const char* filename)
{
	struct mg_font* fnt;

	if (!filename)
	{
		mg_verbose(1, "sdlpng: filename not specified for font '%s'\n", name);
		return 0;
	}

	fnt = calloc(1, sizeof(*fnt));
	if (!fnt)
		return 0;
	fnt->size = inchToPixelsY(size);
	fnt->name = strdup(name ? name : "<Unknown>");
	fnt->font = openFont(filename, fnt->size);
	if (!fnt->font)
	{
		free(fnt);
		return 0;
	}

	return fnt;
}

static void drv_freeFont(mg_font_t fnt)
{
	if (!fnt)
		return;

	if (fnt->name)
		free(fnt->name);
	if (fnt->font)
		TTF_CloseFont(fnt->font);
	free(fnt);
}

static mg_image_t drv_loadImage(const char* filename)
{
	struct mg_image* img;

	img = calloc(1, sizeof(*img));
	if (!img)
		return 0;
	img->img = IMG_Load(filename);
	if (!img->img)
	{
		free(img);
		return 0;
	}
	img->width = pixelsToInchX(img->img->w);
	img->height = pixelsToInchY(img->img->h);

	return img;
}

static void drv_freeImage(mg_image_t img)
{
	if (!img)
		return;

	if (img->img)
		SDL_FreeSurface(img->img);
	free(img);
}

static int drv_getImageSize(mg_image_t img, mg_rectf_t* r)
{
	if (!r)
		return EXIT_FAILURE;

	r->x = r->y = 0;
	if (img)
	{
		r->w = img->width;
		r->h = img->height;
	}
	else
	{
		r->w = 0;
		r->h = 0;
	}
	return EXIT_SUCCESS;
}

static void drv_setClipRect(const mg_rectf_t* r)
{
	if (!r)
	{
		SDL_SetClipRect(outimg, 0);
	}
	else
	{
		SDL_Rect dr;
		dr.x = (int)(r->x * dpix);
		dr.y = (int)(r->y * dpiy);
		dr.w = (int)(r->w * dpix);
		dr.h = (int)(r->h * dpiy);
		SDL_SetClipRect(outimg, &dr);
	}
}

static inline Uint32 colorToGfx(mg_color_t clr)
{
	return (clr.r << 24) | (clr.g << 16) | (clr.b << 8) | (clr.a);
}

static inline Uint32 colorToSDL(mg_color_t clr)
{
	return SDL_MapRGBA(imgfmt, clr.r, clr.g, clr.b, clr.a);
}

static void drv_drawPixel(double xf, double yf, mg_color_t color, double weightf)
{
		// weight is ignored here
	const int x0 = (int)(xf * dpix);
	const int y0 = (int)(yf * dpiy);
	const Uint32 clr = colorToGfx(color);
	pixelColor(outimg, x0, y0, clr);

	(void)weightf; // hush little compiler
}

static void drawHLine(int x0, int x1, int y, mg_color_t color, int weight)
{
	int i;
	int y0 = y - weight / 2;
	const Uint32 clr = colorToGfx(color);
	Uint32 aaclr;
	Uint32 c;

	color.a = 0xb0;
	aaclr = colorToGfx(color);

	for (i = 0; i < weight; ++i, ++y0)
	{
		if (weight > 2 && (i == 0 || i == weight - 1))
			c = aaclr;
		else
			c = clr;
		
		hlineColor(outimg, x0, x1, y0, c);
	}
}

static void drawVLine(int x, int y0, int y1, mg_color_t color, int weight)
{
	int i;
	int x0 = x - weight / 2;
	const Uint32 clr = colorToGfx(color);
	Uint32 aaclr;
	Uint32 c;

	color.a = 0xb0;
	aaclr = colorToGfx(color);

	for (i = 0; i < weight; ++i, ++x0)
	{
		if (weight > 2 && (i == 0 || i == weight - 1))
			c = aaclr;
		else
			c = clr;
		
		vlineColor(outimg, x0, y0, y1, c);
	}
}

// This is a pretty crude floating point algorithm
//  for drawing thick anti-aliased lines with rounded end-points
//  Not optimized at all, but we do not need speed here
static void drv_drawLine(double x0f, double y0f, double x1f, double y1f, mg_color_t color, double weightf)
{
	int x0 = (int)(x0f * dpix);
	int y0 = (int)(y0f * dpiy);
	int x1 = (int)(x1f * dpix);
	int y1 = (int)(y1f * dpiy);
	int dx, dy;
	double length;
	double sinl, cosl;
	double hw, thres;
	int x, y;
	const Uint32 clr = colorToGfx(color);
	const Uint32 clrna = clr & 0xffffff00;
	int weight;

	if (x0 == x1)
	{
		drawVLine(x0, y0, y1, color, inchToPixelsX(weightf));
		return;
	}
	if (y0 == y1)
	{
		drawHLine(x0, x1, y0, color, inchToPixelsY(weightf));
		return;
	}

	weight = (int)((weightf * dpix + weightf * dpiy) / 2);
	if (weight <= 1)
	{	// 1-pixel wide
		aalineColor(outimg, x0, y0, x1, y1, clr);
		return;
	}

	if (x1 < x0)
	{	// swap line endpoints
		int t;
		t = x0;
		x0 = x1;
		x1 = t;
		t = y0;
		y0 = y1;
		y1 = t;
	}

	dx = x1 - x0;
	dy = y1 - y0;
	length = sqrt(dx*dx + dy*dy);
	sinl = (double)dy / length;
	cosl = (double)dx / length;
	hw = (double)weight / 2;
	thres = hw - 0.6;
	if (thres < 0)
		thres = 0;

	for (x = x0 - weight; x < x1 + weight; ++x)
	{
		double py = (double)(x - x0) / dx * dy;
		double ly = y0 + py;
		int iy;
		int sy;

		if (x < x0)
			sy = y0;
		else if (x > x1)
			sy = y1;
		else
			sy = (int)ly;

		for (iy = -1; iy <= 1; ++iy)
		{
			if (iy == 0)
			{
				++iy;
				++sy;
			}

			for (y = sy; ; y += iy)
			{
				double oy = py + (y - ly) * fabs(sinl);
				double dist;

				if (y1 < y0)
					oy = -oy;

				// distance from ideal line point dictates opacity
				if (oy < 0)
				{
					dist = sqrt((x - x0) * (x - x0) + (y - y0) * (y - y0));
				}
				else if (oy > abs(dy))
				{
					dist = sqrt((x - x1) * (x - x1) + (y - y1) * (y - y1));
				}
				else
				{
					dist = fabs((double)y - ly) * fabs(cosl);
				}

				if (dist <= thres)
				{
					pixelColor(outimg, x, y, clr);
				}
				else
				{	// anti-alias the pixel
					int a = (int)((1 - (dist - thres)) * 255);
					if (a <= 0)
						break;
					pixelColor(outimg, x, y, clrna | ((color.a * a) >> 8));
				}
			}
		}
	}
}

static void drv_drawRect(const mg_rectf_t* r, mg_color_t color, double weightf)
{
	// draw the box
	drv_drawLine(r->x, r->y, r->x + r->w, r->y, color, weightf);
	drv_drawLine(r->x, r->y + r->h, r->x + r->w, r->y + r->h, color, weightf);
	drv_drawLine(r->x, r->y, r->x, r->y + r->h, color, weightf);
	drv_drawLine(r->x + r->w, r->y, r->x + r->w, r->y + r->h, color, weightf);
}

static void drv_drawFilledRect(const mg_rectf_t* r, mg_color_t color)
{
	const Uint32 clr = colorToSDL(color);

	if (!r)
	{	// whole surface
		SDL_FillRect(outimg, NULL, clr);
	}
	else
	{
		SDL_Rect dr;
		dr.x = (int)(r->x * dpix);
		dr.y = (int)(r->y * dpiy);
		dr.w = (int)(r->w * dpix);
		dr.h = (int)(r->h * dpiy);
		SDL_FillRect(outimg, &dr, clr);
	}
}

static void drv_drawEllipse(double xcf, double ycf, double xrf, double yrf, mg_color_t color, double weight)
{
		// weight currently ignored
	int xc = (int)(xcf * dpix);
	int yc = (int)(ycf * dpiy);
	int xr = (int)(xrf * dpix);
	int yr = (int)(yrf * dpiy);
	const Uint32 clr = colorToGfx(color);

	aaellipseColor(outimg, xc, yc, xr, yr, clr);

	(void)weight; // hush little compiler
}

// This is a pretty crude floating point algorithm
//  for drawing anti-aliased filled ellipse
//  Not really optimized at all, but we do not need speed here
static void drv_drawFilledEllipse(double xcf, double ycf, double xrf, double yrf, mg_color_t color)
{
	const int xc = (int)(xcf * dpix);
	const int yc = (int)(ycf * dpiy);
	const int xr = (int)(xrf * dpix);
	const int yr = (int)(yrf * dpiy);
	const Uint32 clr = colorToGfx(color);
	const Uint32 clrna = clr & 0xffffff00;
	int x, y;
	double radius, radius_2, rad_thres;
	double fx_2, fy_2;

	// the (more or less) classic elliptic equation is
	//   (fx*x)^2 + (fy*y)^2 = r^2 (classic: fx=r/a, fy=r/b)
	// we'll take a=r (or b=r) and transform
	//   x^2 + (fy*y)^2 = r^2
	// or more precisely, along the major axis
	if (xr >= yr)
		radius = xr;
	else
		radius = yr;
	
	radius_2 = radius * radius;
	fx_2 = radius / xr;
	fx_2 = fx_2 * fx_2;
	fy_2 = radius / yr;
	fy_2 = fy_2 * fy_2;
	rad_thres = (radius + 1) * (radius + 1);

	for (y = -yr; y <= yr; y++)
	{
		double y_2 = y * y * fy_2;

		for (x = -xr; x <= xr; x++)
		{
			double r_2 = y_2 + x * x * fx_2;
			
			if (r_2 >= rad_thres)
			{	// outside bounds
				continue;
			}

			if (r_2 <= radius_2) 
			{	// solid pixels inside the ellipse
				pixelColor(outimg, xc + x, yc + y, clr);
			}
			else
			{	// anti-aliased edge
				int a = (int) ((1 - (sqrt(r_2) - radius)) * 255);
				if (a < 0)
					continue; // should never happen
				pixelColor(outimg, xc + x, yc + y, clrna | ((color.a * a) >> 8));
			}
		}
	}

}

static void setImageAlpha(SDL_Surface* img, int alpha)
{
	SDL_PixelFormat* fmt = img->format;
	int x, y;

	if (!fmt->Amask)
	{
		SDL_SetAlpha(img, SDL_SRCALPHA, alpha);
		return;
	}

	if (fmt->BytesPerPixel != 4)
	{
		mg_verbose(1, "Unsuppoted surface format for alpha -- %d bytes/pixel\n", (int)fmt->BytesPerPixel);
		return;
	}

	// adjust the alpha channel accordingly
	for (y = 0; y < img->h; ++y)
	{
		Uint32* dst = (Uint32*)((Uint8*)img->pixels + img->pitch * y);

		for (x = 0; x < img->w; ++x, ++dst)
		{
			Uint32 p = *dst;
			Uint32 a = (p & fmt->Amask) >> fmt->Ashift;
			p &= ~fmt->Amask;
			a = a * alpha / 255;
			*dst = p | (a << fmt->Ashift);
		}
	}
}

static void drv_drawImage(double xf, double yf, struct mg_image* img)
{
	SDL_Rect dr;
	
	dr.x = (int)(xf * dpix);
	dr.y = (int)(yf * dpiy);

	SDL_BlitSurface(img->img, NULL, outimg, &dr);
}

static void drv_drawText(struct mg_font* fnt, double xf, double yf, const unsigned char* utf8str, mg_color_t color)
{
	SDL_Rect dr;
	SDL_Color clr = {color.r, color.g, color.b, 0};
	SDL_Surface* text;

	if (!fnt || !utf8str)
		return;

	dr.x = (int)(xf * dpix);
	dr.y = (int)(yf * dpiy);

	text = TTF_RenderUTF8_Blended(fnt->font, utf8str, clr);
	if (!text)
		return;
	if (color.a != 0xff)
		setImageAlpha(text, color.a);
	SDL_BlitSurface(text, NULL, outimg, &dr);
	SDL_FreeSurface(text);
}

static void drv_getTextSize(struct mg_font* fnt, const unsigned char* utf8str, mg_rectf_t* r)
{
	int w = 0;
	int h = 0;

	if (!r)
		return;
	if (!fnt || !utf8str)
	{
		r->w = 0;
		r->h = 0;
		return;
	}

	TTF_SizeUTF8(fnt->font, utf8str, &w, &h);
	r->w = (double)w / dpix;
	r->h = (double)h / dpiy;
}
