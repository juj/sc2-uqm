/*
 * UQM Starmap image generator
 * Output Driver decl
 *
 * The GPL applies
 *
 */

#include <sys/types.h>
#include <stdlib.h>
#include <stdint.h>
#include "port.h"

typedef struct
{
	uint8_t r, g, b, a;
} mg_color_t;

typedef struct
{
	int x;
	int y;
} mg_point_t;

typedef struct
{
	double x;
	double y;
} mg_pointf_t;

typedef struct
{
	int x;
	int y;
	int w;
	int h;
} mg_rect_t;

typedef struct
{
	double x;
	double y;
	double w;
	double h;
} mg_rectf_t;

typedef struct mg_font *mg_font_t;
typedef struct mg_image *mg_image_t;

typedef struct
{
	const char* name;
	const char* description;

	// func ptrs
	int (* init)(int argc, char *argv[]);
	int (* term)(void);
	void (* usage)(void);
	int (* setResolution)(int dpi_x, int dpi_y);
	int (* begin)(double w, double h);
	int (* write)(void);
	int (* end)(void);
	mg_font_t (* loadFont)(const char* name, double size, const char* filename /* optional */);
	void (* freeFont)(mg_font_t);
	mg_image_t (* loadImage)(const char* filename);
	void (* freeImage)(mg_image_t);
	int (* getImageSize)(mg_image_t, mg_rectf_t* r);

	// draw func ptrs
	void (* setClipRect)(const mg_rectf_t*);
	void (* drawPixel)(double x, double y, mg_color_t, double weight);
	void (* drawLine)(double x0, double y0, double x1, double y1, mg_color_t color, double weight);
	void (* drawRect)(const mg_rectf_t* r, mg_color_t color, double weight);
	void (* drawFilledRect)(const mg_rectf_t* r, mg_color_t color);
	void (* drawEllipse)(double xc, double yc, double xr, double yr, mg_color_t color, double weight);
	void (* drawFilledEllipse)(double xc, double yc, double xr, double yr, mg_color_t color);
	void (* drawImage)(double x, double y, mg_image_t image);
	void (* drawText)(mg_font_t, double x, double y, const unsigned char* utf8str, mg_color_t);
	void (* getTextSize)(mg_font_t, const unsigned char* utf8str, mg_rectf_t* r);

} mg_driver_t;

extern void mg_verbose(int level, const char* fmt, ...);

