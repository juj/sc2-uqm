/*
 * UQM Starmap image generator
 * SDL/SVG driver implementation
 *
 * Uses: SDL, SDL_ttf
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
#include "SDL_image.h"
#include "SDL.h"
#include "SDL_ttf.h"

static double width, height;

struct mg_font
{
	char* name;
	TTF_Font* font;
	double size;
};

struct mg_image
{
	SDL_Surface* img;
	char *filename;
	double width;
	double height;
};

static int drv_init(int argc, char *argv[]);
static int drv_term(void);
static void drv_usage(void);
static int drv_begin(double w, double h);
static int drv_write(void);
static int drv_end(void);
static mg_font_t drv_loadFont(const char* name, double size, const char*);
static void drv_freeFont(mg_font_t);
static mg_image_t drv_loadImage(const char*);
static void drv_freeImage(mg_image_t);
static int drv_getImageSize(mg_image_t, mg_rectf_t*);
static int drv_setResolution(int dpi_x, int dpi_y);
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

const mg_driver_t sdlsvg_drv = 
{
	"sdlsvg", // name
	"SDL/SVG image writer", // description

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

static int parseArguments(int argc, char* argv[]);

static const char* fontdir = ".";
static const char* outdev = "starmap.svg";
static FILE* fout;
static int dpix, dpiy;

static inline int inchToPixelsY(double inch)
{
	int v = (int)(inch * dpiy);
	// make sure it's at least 1 pixel wide if present
	if (!v && inch)
		v = 1;
	return v;
}

static char colbuffer[8];

static char *color_string(mg_color_t color)
{
	snprintf(colbuffer, 8, "#%2.2x%2.2x%2.2x", color.r, color.g, color.b);
	return colbuffer;
}

static char alphabuffer[6];

static char *alpha_string(mg_color_t color)
{
	snprintf(alphabuffer, 6, "%1.3f", color.a/255.0);
	return alphabuffer;
}

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
			   "sdlsvg driver : [-f <fontdir>] [-o <outfile>]\n"
			   "Options:\n"
			   "\t-f  dir where font files are located\n"
			   "\t-o  save image as <outfile> (default is 'starmap.svg')\n"
		);
}

static int parseArguments(int argc, char* argv[])
{
	int ch;
	
	while (-1 != (ch = getopt(argc, argv, "-f:o:")))
	{
		switch (ch)
		{
			case 'f':
				fontdir = optarg;
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
	width = w;
	height = h;
	fout = fopen(outdev, "wt");
	if (!fout)
	{
		mg_verbose(1, "Cannot open file '%s' -- %s\n", outdev, strerror(errno));
		return EXIT_FAILURE;
	}

	fprintf(fout, "<?xml version=\"1.0\" standalone=\"no\"?>\n");
	fprintf(fout, "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\"\n");
	fprintf(fout, "\"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n");
	fprintf(fout, "<svg width=\"%fin\" height=\"%fin\" xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" version=\"1.1\">\n", w, h);

	return EXIT_SUCCESS;
}

static int drv_end(void)
{
	if (fout)
	{
		fprintf(fout, "</svg>\n");
		fclose(fout);
	
		fout = 0;
	}

	return EXIT_SUCCESS;
}

static int drv_write(void)
{
	if (!fout)
		return EXIT_FAILURE;

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
		mg_verbose(1, "sdlsvg: filename not specified for font '%s'\n", name);
		return 0;
	}

	fnt = calloc(1, sizeof(*fnt));
	if (!fnt)
		return 0;
	fnt->size = size;
	fnt->name = strdup(name ? name : "<Unknown>");
	fnt->font = openFont(filename, inchToPixelsY(fnt->size));
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
	img->filename = strdup(filename);
	if (!img->filename)
	{
		drv_freeImage(img);
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

	if (img->filename)
		free(img->filename);

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

static int clipon = 0;
static int clipcount = 0;

static void drv_setClipRect(const mg_rectf_t* r)
{
	if (clipon)
		fprintf(fout, "</g>\n");
	if (!r)
		clipon = 0;
	else
	{
		mg_color_t t;
		fprintf(fout, "<clipPath id=\"clip%d\">\n", clipcount);
		t.r = t.g = t.b = t.a = 255;
		drv_drawFilledRect(r, t);
		fprintf(fout, "</clipPath>\n");
		clipon = 1;
		fprintf(fout, "<g clip-path=\"url(#clip%d)\">\n", clipcount++);
	}
}

static void drv_drawPixel(double xf, double yf, mg_color_t color, double weightf)
{
	drv_drawLine(xf, yf, xf, yf, color, weightf);
}

static void drv_drawLine(double x0f, double y0f, double x1f, double y1f, mg_color_t color, double weightf)
{
	fprintf(fout, 
			"<line x1=\"%fin\" y1=\"%fin\" x2=\"%fin\" y2=\"%fin\" stroke=\"%s\" stroke-width=\"%fin\" opacity=\"%s\"/>\n",
			x0f, y0f, x1f, y1f, color_string(color), weightf, alpha_string(color));
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
	if (!r)
	{
		mg_rectf_t t;
		t.x = t.y = 0;
		t.w = width;
		t.h = height;
		drv_drawFilledRect(&t, color);
	}
	else
	{
		fprintf(fout, 
				"<rect x=\"%fin\" y=\"%fin\" width=\"%fin\" height=\"%fin\" stroke=\"none\" fill=\"%s\" opacity=\"%s\"/>\n",
				r->x, r->y, r->w, r->h, color_string(color),
				alpha_string(color));
		
	}
}

static void drv_drawEllipse(double xcf, double ycf, double xrf, double yrf, mg_color_t color, double weight)
{
	fprintf(fout, 
			"<ellipse cx=\"%fin\" cy=\"%fin\" rx=\"%fin\" ry=\"%fin\" stroke=\"%s\" opacity=\"%s\" fill=\"none\" stroke-width=\"%fin\"/>\n",
			xcf, ycf, xrf, yrf, color_string(color), alpha_string(color), weight);
	
}

static void drv_drawFilledEllipse(double xcf, double ycf, double xrf, double yrf, mg_color_t color)
{
	fprintf(fout, 
			"<ellipse cx=\"%fin\" cy=\"%fin\" rx=\"%fin\" ry=\"%fin\" fill=\"%s\" opacity=\"%s\"/>\n",
			xcf, ycf, xrf, yrf, color_string(color), alpha_string(color));
	
}

static void drv_drawImage(double xf, double yf, struct mg_image* img)
{
	fprintf(fout, 
			"<image xlink:href=\"%s\" x=\"%fin\" y=\"%fin\" width=\"%fin\" height=\"%fin\"/>\n",
			img->filename, xf, yf, img->width, img->height);	
}

static void drv_drawText(struct mg_font* fnt, double xf, double yf, const unsigned char* utf8str, mg_color_t color)
{
	mg_rectf_t t;

	if (!fnt || !utf8str)
		return;

	drv_getTextSize(fnt, utf8str, &t);

	yf += t.h;

	fprintf(fout, 
			"<text x=\"%fin\" y=\"%fin\" font-size=\"%f\" font-family=\"%s\" fill=\"%s\" dominant-baseline=\"text-after-edge\" opacity=\"%s\">\n",
			xf, yf, fnt->size*96.0, fnt->name, color_string(color), alpha_string(color));
	fprintf(fout, "%s</text>", utf8str);
	
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

	TTF_SizeUTF8(fnt->font, (const char*) utf8str, &w, &h);
	r->w = (double)w / dpix;
	r->h = (double)h / dpiy;
}
