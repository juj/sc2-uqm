/*
 * UQM Starmap image generator
 *   By Alex Volkov (codepro@usa.net), 20060220
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
#include <math.h>
#include <ctype.h>
#include "port.h"
#include "scriptlib.h"
#include "unicode.h"
#include "mapdrv.h"

#ifndef M_PI
#	define M_PI  3.1415927
#endif
#ifndef SQR
#	define SQR(x)  ((x) * (x))
#endif

extern const mg_driver_t sdlpng_drv;
extern const mg_driver_t sdlsvg_drv;

static const mg_driver_t* drvs[] =
{
	&sdlpng_drv,
	&sdlsvg_drv,
	0 /* term */
};
static const mg_driver_t* drv;

typedef struct
{
	const char* infile;
	const char* driver;
	int verbose;

} options_t;

typedef struct
{
	mg_image_t img;
	mg_pointf_t hot;
	char color;
	int size;
	double radius;
	mg_color_t rend_color; // alternate rendering
} star_image_t;

typedef struct
{
	char cluster[128];
	int prefix;
	mg_pointf_t pos;
	char color;
	int size;
	const star_image_t* image;
} star_t;

typedef star_t* cluster_conn_t[2];

typedef struct
{
	int first;
	int cstars;
	int text_dx, text_dy;
	mg_pointf_t center;
	int cconns;
	cluster_conn_t conns[20];
} cluster_t;

typedef struct
{
	const char* name;
	mg_pointf_t center;
	double radius;
	mg_color_t clr;
	mg_font_t font;
	mg_pointf_t tweak;
} soi_t;

typedef enum
{
	sht_Null = 0,
	sht_Point,
	sht_Line,
	sht_Rect,
	sht_Ellipse,

} shape_type_t;

typedef struct
{
	shape_type_t type;
	mg_pointf_t pt0;
	mg_pointf_t pt1;
	double radiusx;
	double radiusy;

} shape_t;

typedef enum
{
	objt_Null = 0,
	objt_Sphere      = (1 << 0),
	objt_SphereText  = (1 << 1),
	objt_Star        = (1 << 2),
	objt_ClusterLine = (1 << 3),
	objt_ClusterText = (1 << 4),
	objt_Designation = (1 << 5),

	objt_DesigPlacement =
			objt_Star |
			objt_ClusterLine |
			objt_Designation,

	objt_TextPlacement =
			objt_Star |
			objt_ClusterLine |
			objt_ClusterText |
			objt_Designation,

} obj_type_t;

typedef struct
{
	obj_type_t type;
	shape_t shape;

} object_t;

typedef struct
{
	unsigned char str[8];
} desig_char_t;

typedef struct
{
	unsigned char name[32];
} layer_t;

typedef struct
{
	int dpix;
	int dpiy;
	// all doubles represent inches
	double w;
	double h;

	mg_color_t backclr;

	layer_t* layers;
	int clayers;
	
	int gridmx; // max grid coordinates
	int gridmy;
	int grids1; // grid steps; 1: numbers
	int grids2; //  2: grid lines
	int grids3; //  3: smaller marks
	mg_rectf_t gridr;
	mg_color_t gridclr;
	mg_color_t gridmclr;
	mg_font_t gridfnt;
	double gridbw; // box weight
	double gridlw; // lines weight

	const char* starclrs;
	int starsizes;
	star_image_t* starimgs;
	int cstarimgs;
	star_t* stars;
	int cstars;

	cluster_t* clusters;
	int cclusters;
	mg_color_t clustclr;
	double clustlw; // lines weight
	mg_font_t cnamefnt;
	mg_color_t cnameclr;

	mg_font_t desigfnt;
	mg_color_t desigclr;
	desig_char_t* desigtab;
	int cdesigtab;

	soi_t* sois;
	int csois;
	const char* soifont;
	const char* soifontfile;
	double soifontsize;

	object_t* objs;
	int cobjs;
	int objalloc;

} script_t;

int verbose_level = 0;
void mg_verbose(int level, const char* fmt, ...);
static void parseArguments(int argc, char* argv[], options_t* opts);

static const mg_driver_t* findDriver(const char* name);

static int readScript(script_t*, FILE*);
static void freeScript(script_t*);
static void preprocessStars(script_t* scr);
static object_t* addObject(script_t*, obj_type_t, shape_type_t, double x0, double y0,
		double x1, double y1, double rx, double ry);

static void drawLayers(const mg_driver_t*, script_t*);
static void drawGrid(const mg_driver_t*, script_t*);
static void drawStars(const mg_driver_t*, script_t*);
static void drawStarDesignations(const mg_driver_t*, script_t*);
static void drawClusterLines(const mg_driver_t*, script_t*);
static void drawClusterNames(const mg_driver_t*, script_t*);
static void drawSpheres(const mg_driver_t*, script_t*);
static void drawSphereNames(const mg_driver_t*, script_t*);

static const struct map_layer
{
	const char* name;
	void (* draw)(const mg_driver_t*, script_t*);
}
map_layers[] =
{
	{"grid",     drawGrid},
	{"sois",     drawSpheres},
	{"soinames", drawSphereNames},
	{"clusters", drawClusterLines},
	{"cnames",   drawClusterNames},
	{"stars",    drawStars},
	{"starnums", drawStarDesignations},

	{0, 0} // term
};

int main(int argc, char *argv[])
{
	int ret = EXIT_FAILURE;
	options_t opts;
	script_t scr;
	FILE* fin = NULL;

	memset(&scr, 0, sizeof(scr));

	parseArguments(argc, argv, &opts);
	verbose_level = opts.verbose;

	if (!opts.driver)
		opts.driver = "sdlpng"; // default

	drv = findDriver(opts.driver);
	if (!drv)
	{
		mg_verbose(1, "Driver '%s' not found\n", opts.driver);
		return EXIT_FAILURE;
	}

	mg_verbose(2, "Using driver %s -- %s\n", drv->name, drv->description);

	do
	{
		if (opts.infile)
		{
			fin = fopen(opts.infile, "rt");
			if (!fin)
			{
				mg_verbose(1, "Cannot open file '%s' -- %s\n", opts.infile, strerror(errno));
				break;
			}
		}
		else
		{
			fin = stdin;
		}

		ret = drv->init(argc, argv);
		if (ret)
		{
			mg_verbose(1, "Driver '%s' failed to initialize\n", drv->name);
			break;
		}
		
		if (readScript(&scr, fin))
		{
			mg_verbose(1, "Cannot process input script -- %s\n", strerror(errno));
			break;
		}
		preprocessStars(&scr);

		ret = drv->begin(scr.w, scr.h);
		if (ret)
		{
			mg_verbose(1, "Driver '%s' could not start\n");
			break;
		}

		drv->drawFilledRect(NULL, scr.backclr);

		drawLayers(drv, &scr);

		ret = drv->write();
		if (ret)
		{
			mg_verbose(1, "Driver could not write to the device\n");
			break;
		}

		drv->end();

		ret = EXIT_SUCCESS;
	} while (0);

	freeScript(&scr);

	if (fin)
		fclose(fin);
	drv->term();

	return ret;
}

void mg_verbose(int level, const char* fmt, ...)
{
	va_list args;
	
	if (verbose_level < level)
		return;
	
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
}

static void usage()
{
	fprintf(stderr,
			"mapgen [-i <infile>] [-d <driver>] [driver options]\n"
			"Options:\n"
			"\t-i  input script file; stdin when none\n"
			"\t-d  use <driver>; default is sdlpng\n"
			"\t-v  increase verbosity level (use more than once)\n"
			"mapgen -d <driver> -h  [to see driver options]\n"
			);
}

static void parseArguments(int argc, char *argv[], options_t *opts)
{
	int ch;
	
	opterr = 0;
	memset(opts, 0, sizeof (options_t));

	while (-1 != (ch = getopt(argc, argv, "-h?vi:d:")))
	{
		switch (ch)
		{
		case 'i':
			opts->infile = optarg;
			break;
		case 'f':
			break;
		case 'd':
			opts->driver = optarg;
			break;
		case 'n':
			break;
		case 'v':
			opts->verbose++;
			break;
		case '?':
			if (optopt != '?')
			{	// unknown option -- let driver handle it
				break;
			}
		case 'h':
			{
				const mg_driver_t* drv = findDriver(opts->driver);
				usage();
				if (drv)
					drv->usage();
				exit(EXIT_FAILURE);
			}
		default:
			// non-option -- probably an arg to unknown option
			;
		}
	}
	// TODO: this does not work with all versions of getopt()
	optind = 1; // let the driver handle the rest
}

static const mg_driver_t* findDriver(const char* name)
{
	const mg_driver_t** d;

	if (!name)
		return 0;

	for (d = drvs; *d && strcmp((*d)->name, name) != 0; ++d)
		;

	return *d;
}

static void parseColor(mg_color_t* clr, const char* key, uint32_t def)
{
	uint32_t cv;
	const char* v = scr_GetString(key);
	if (!v || 1 != sscanf(v, "%x", &cv))
		cv = def;
	clr->r = (cv >> 16) & 0xff;
	clr->g = (cv >>  8) & 0xff;
	clr->b = (cv      ) & 0xff;
	clr->a = 0xff;
}

static int parseFont(mg_font_t* fnt, const char* key)
{
	char buf[128];
	const char* name;
	const char* filename;
	double size;

	sprintf(buf, "%s.name", key);
	name = scr_GetString(buf);
	sprintf(buf, "%s.filename", key);
	filename = scr_GetString(buf);
	sprintf(buf, "%s.size", key);
	size = scr_GetFloatDef(buf, 10) / 64.0;

	*fnt = drv->loadFont(name, size, filename);

	if (!*fnt)
		mg_verbose(1, "Warning: cannot load font '%s' (%s)\n", name, filename);
	
	return *fnt != NULL;
}

static void freeFont(mg_font_t* fnt)
{
	if (!fnt)
		return;
	
	drv->freeFont(*fnt);
	*fnt = NULL;
}

static layer_t* loadLayers(const char* key, int* count)
{
	layer_t* tab;
	char buf[256];
	int i, max = 32;
	char* sl;
	layer_t* l;

	*count = 0;

	strcpy(buf, scr_GetStringDef(key,
			"sois,grid,soinames,clusters,stars,starnums,cnames"));

	tab = calloc(max, sizeof(*tab));
	if (!tab)
		return 0;

	for (i = 0, sl = strtok(buf, ","), l = tab; i < max && sl;
			++i, ++l, sl = strtok(0, ","))
	{
		strcpy(l->name, sl);
	}
	
	*count = i;

	return tab;
}

static desig_char_t* loadDesignations(const char* key, int* count)
{
	const char* name;
	FILE* f;
	char buf[256] = "";
	char* end;
	int len;
	desig_char_t* tab;
	const unsigned char* p;
	int i;

	*count = 0;

	name = scr_GetString(key);
	if (!name)
		return 0;

	f = fopen(name, "rt");
	if (!f)
	{
		mg_verbose(1, "Cannot open '%s' -- %s\n", name, strerror(errno));
		return 0;
	}
	fgets(buf, sizeof(buf), f);
	end = strchr(buf, '\n');
	if (end)
		*end = '\0';
	fclose(f);

	len = utf8StringCount(buf);
	tab = calloc(len, sizeof(desig_char_t));
	if (!tab)
	{
		mg_verbose(1, "Out of memory\n");
		return 0;
	}

	for (i = 0, p = buf; i < len; ++i)
	{
		const unsigned char* prev = p;
		int cl;

		getCharFromString(&p);
		cl = p - prev;
		memcpy(tab[i].str, prev, cl);
		tab[i].str[cl] = '\0';
	}

	*count = len;

	return tab;
}

static star_image_t* loadStarImages(const char* key, const char* colors, int sizes, int* count)
{
	star_image_t* tab;
	int ccolors = strlen(colors);
	const char* c;
	int s;
	star_image_t* img;

	*count = 0;
	if (!ccolors || !sizes)
		return 0;

	tab = calloc(ccolors * sizes, sizeof(*tab));
	if (!tab)
		return 0;

	for (c = colors, img = tab; *c; ++c)
	{
		char starkey[256];
		char buf[300];

		for (s = 1; s <= sizes; ++s, ++img)
		{
			const char* name;
			mg_rectf_t r;

			sprintf(starkey, "%s.%c.%d", key, *c, s);
			img->color = *c;
			img->size = s - 1;

			sprintf(buf, "%s.image", starkey);
			name = scr_GetString(buf);
			if (name)
			{
				img->img = drv->loadImage(name);
				if (img->img)
				{
					drv->getImageSize(img->img, &r);
					img->hot.x = r.w / 2;
					img->hot.y = r.h / 2;
				}
				else
				{
					mg_verbose(1, "Warning: cannot load image '%s'\n", name);
				}
			}

			sprintf(buf, "%s.color", starkey);
			parseColor(&img->rend_color, buf, 0); 

			sprintf(buf, "%s.radius", starkey);
			img->radius = scr_GetFloatDef(buf, 1) / 64.0;
		}
	}
	
	*count = ccolors * sizes;

	return tab;
}

static int cmpStars(const void* elem1, const void* elem2)
{
	const star_t* star1 = (const star_t*)elem1;
	const star_t* star2 = (const star_t*)elem2;
	int ret;

	ret = strcmp(star1->cluster, star2->cluster);
	if (ret != 0)
		return ret;

	if (star1->prefix < star2->prefix)
		return -1;
	if (star1->prefix > star2->prefix)
		return 1;
	return 0;
}

static int desigToPrefix(char desig)
{
	static const char* desigtab = "*ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	const char* cl = strchr(desigtab, toupper(desig));
	if (!cl || *cl == '\0')
		return 0;
	else
		return cl - desigtab;
}

static star_t* loadStars(const char* name, int* count)
{
	star_t* tab;
	int tabsize;
	int cstars;
	FILE* f;

	*count = 0;
	if (!name)
		return 0;

	// grab more than we need to reduce reallocs
	tabsize = 1000;
	tab = malloc(tabsize * sizeof(*tab));
	if (!tab)
	{
		mg_verbose(1, "Out of memory\n");
		return 0;
	}
	memset(tab, 0, tabsize * sizeof(*tab));

	f = fopen(name, "rt");
	if (!f)
	{
		mg_verbose(1, "Cannot open '%s' -- %s\n", name, strerror(errno));
		free(tab);
		return 0;
	}

	for (cstars = 0; !feof(f); )
	{
		char buf[256];
		char dbuf[20];
		char cbuf[20];
		star_t* star;

		if (!fgets(buf, sizeof(buf), f) || !buf[0] || buf[0] == '#')
			continue;

		if (cstars >= tabsize)
		{
			tabsize += tabsize / 2;
			tab = realloc(tab, tabsize * sizeof(*tab));
			if (!tab)
			{
				mg_verbose(1, "Out of memory\n");
				return 0;
			}
		}
		star = tab + cstars;

		if (6 != sscanf(buf, "%127[^,\n\r],%19[^,\n\r],%lf,%lf,%19[^,\n\r],%d",
				star->cluster, dbuf, &star->pos.x, &star->pos.y,
				cbuf, &star->size))
			continue;

		star->prefix = desigToPrefix(dbuf[0]);
		star->color = toupper(cbuf[0]);
		star->size--;

		++cstars;
		++star;
	}
	
	fclose(f);

	tab = realloc(tab, cstars * sizeof(*tab));
	*count = cstars;

	// sort the stars
	qsort(tab, cstars, sizeof(*tab), cmpStars);

	return tab;
}

static cluster_t* initClusters(const star_t* stars, int cstars, int* count)
{
	int cclusters;
	cluster_t* tab;
	cluster_t* cluster;
	int tabsize;
	int i;

	*count = 0;
	if (!stars || cstars == 0)
		return 0;

	// grab more than we need to reduce reallocs
	tabsize = 50;
	tab = malloc(tabsize * sizeof(*tab));
	if (!tab)
	{
		mg_verbose(1, "Out of memory\n");
		return 0;
	}
	memset(tab, 0, tabsize * sizeof(*tab));

	for (i = 0, cclusters = 0; i < cstars; ++i)
	{
		if (i > 0 && 0 == strcmp(stars[i].cluster, stars[i - 1].cluster))
			continue;

		// next cluster
		if (cclusters > 0)
		{	// update last one
			cluster = tab + cclusters - 1;
			cluster->cstars = i - cluster->first;
		}

		if (cclusters >= tabsize)
		{
			int newsize = tabsize + tabsize / 2;
			tab = realloc(tab, newsize * sizeof(*tab));
			if (!tab)
			{
				mg_verbose(1, "Out of memory\n");
				return 0;
			}
			memset(tab + cclusters, 0, tabsize / 2 * sizeof(*tab));
			tabsize = newsize;
		}
		
		cluster = tab + cclusters;
		cluster->first = i;

		++cclusters;
	}
	if (cclusters > 0)
	{	// update last one
		cluster = tab + cclusters - 1;
		cluster->cstars = i - cluster->first;
	}
	
	tab = realloc(tab, cclusters * sizeof(*tab));
	*count = cclusters;

	// calc cluster centers
	for (i = 0; i < cclusters; ++i)
	{
		int s;
		double minx = 1000000000.0f, miny = 1000000000.0f;
		double maxx = 0, maxy = 0;

		cluster = tab + i;

		for (s = 0; s < cluster->cstars; ++s)
		{
			const star_t* star = stars + cluster->first + s;
			if (star->pos.x < minx)
				minx = star->pos.x;
			if (star->pos.x > maxx)
				maxx = star->pos.x;
			if (star->pos.y < miny)
				miny = star->pos.y;
			if (star->pos.y > maxy)
				maxy = star->pos.y;
		}

		cluster->center.x = (minx + maxx) / 2;
		cluster->center.y = (miny + maxy) / 2;
	}

	return tab;
}

static cluster_t* findCluster(const char* cname, const script_t* scr)
{
	int i;

	for (i = 0; i < scr->cclusters &&
			strcmp(cname, scr->stars[scr->clusters[i].first].cluster) != 0;
			++i)
		;
	if (i < scr->cclusters)
		return scr->clusters + i;
	else
		return 0;
}

static star_t* findStar(const char* cname, char desig, const script_t* scr)
{
	int i;
	cluster_t* cluster;
	int prefix;

	cluster = findCluster(cname, scr);
	if (!cluster)
		return 0;

	prefix = desigToPrefix(desig);
	for (i = cluster->first; i < cluster->first + cluster->cstars &&
			scr->stars[i].prefix != prefix;
			++i)
		;
	if (i < cluster->first + cluster->cstars)
		return scr->stars + i;
	else
		return 0;
}

static int loadClusters(const char* name, script_t* scr)
{
	FILE* f;

	if (!name)
		return 0;

	f = fopen(name, "rt");
	if (!f)
	{
		mg_verbose(1, "Cannot open '%s' -- %s\n", name, strerror(errno));
		return 0;
	}

	for ( ; !feof(f); )
	{
		char buf[256];
		char* cname;
		char* obuf;
		char* pair;
		char* next;
		cluster_t* cluster;
		int cconns;

		if (!fgets(buf, sizeof(buf), f) || !buf[0] || buf[0] == '#')
			continue;

		cname = buf;
		next = strchr(cname, ',');
		if (!next)
			continue;
		*next = '\0';
		obuf = next + 1;
		next = strchr(obuf, ',');
		if (next)
		{
			*next = '\0';
			pair = next + 1;
		}
		else
			pair = 0;

		cluster = findCluster(cname, scr);
		if (!cluster)
		{
			mg_verbose(2, "Cluster '%s' not found\n", cname);
			continue;
		}

		strupr(obuf);
		if (strcmp(obuf, "N") == 0)
		{
			cluster->text_dx = 0;
			cluster->text_dy = 1;
		}
		else if (strcmp(obuf, "NE") == 0)
		{
			cluster->text_dx = 1;
			cluster->text_dy = 1;
		}
		else if (strcmp(obuf, "NW") == 0)
		{
			cluster->text_dx = -1;
			cluster->text_dy = 1;
		}
		else if (strcmp(obuf, "S") == 0)
		{
			cluster->text_dx = 0;
			cluster->text_dy = -1;
		}
		else if (strcmp(obuf, "SE") == 0)
		{
			cluster->text_dx = 1;
			cluster->text_dy = -1;
		}
		else if (strcmp(obuf, "SW") == 0)
		{
			cluster->text_dx = -1;
			cluster->text_dy = -1;
		}
		else if (strcmp(obuf, "E") == 0)
		{
			cluster->text_dx = 1;
			cluster->text_dy = 0;
		}
		else if (strcmp(obuf, "W") == 0)
		{
			cluster->text_dx = -1;
			cluster->text_dy = 0;
		}
		
		for (cconns = 0; pair && cconns < (int)countof(cluster->conns); )
		{
			next = strchr(pair, ',');
			if (next)
				next++;

			if (pair[0] != '\0' && pair[1] != '\0')
			{
				cluster->conns[cconns][0] = findStar(cname, toupper(pair[0]), scr);
				cluster->conns[cconns][1] = findStar(cname, toupper(pair[1]), scr);
				if (cluster->conns[cconns][0] && cluster->conns[cconns][1])
					++cconns;
			}
			
			pair = next;
		}
		cluster->cconns = cconns;
	}
	
	fclose(f);

	return 1;
}

static soi_t* loadSois(const char* key, int count, script_t* scr)
{
	soi_t* tab;
	int i;
	soi_t* soi;

	if (!key || !count)
		return 0;

	tab = calloc(count, sizeof(*tab));
	if (!tab)
		return 0;

	for (i = 1, soi = tab; i <= count; ++i, ++soi)
	{
		char buf[256];
		double fntsize;

		sprintf(buf, "%s.%d.%s", key, i, "name");
		soi->name = scr_GetString(buf);
		sprintf(buf, "%s.%d.%s", key, i, "color");
		parseColor(&soi->clr, buf, 0);
		sprintf(buf, "%s.%d.%s", key, i, "center.x");
		soi->center.x = scr_GetFloatDef(buf, 0);
		sprintf(buf, "%s.%d.%s", key, i, "center.y");
		soi->center.y = scr_GetFloatDef(buf, 0);
		sprintf(buf, "%s.%d.%s", key, i, "radius");
		soi->radius = scr_GetFloatDef(buf, 0);
		sprintf(buf, "%s.%d.%s", key, i, "tweak.x");
		soi->tweak.x = scr_GetFloatDef(buf, 0);
		sprintf(buf, "%s.%d.%s", key, i, "tweak.y");
		soi->tweak.y = scr_GetFloatDef(buf, 0);
		sprintf(buf, "%s.%d.%s", key, i, "font.size");
		fntsize = scr_GetFloatDef(buf, scr->soifontsize) / 64.0;
		soi->font = drv->loadFont(scr->soifont, fntsize, scr->soifontfile);
		if (!soi->font)
			mg_verbose(1, "Warning: cannot load font '%s' (%s)\n", scr->soifont, scr->soifontfile);
	}
	
	return tab;
}

static int readScript(script_t* scr, FILE* f)
{
	int ret = EXIT_SUCCESS;

	memset(scr, 0, sizeof(*scr));

	scr_LoadFile(f);

	scr->dpix = scr_GetIntegerDef("image.dpi.x", 300);
	scr->dpiy = scr_GetIntegerDef("image.dpi.y", 300);

	ret = drv->setResolution(scr->dpix, scr->dpiy);
	if (ret)
	{
		mg_verbose(1, "Driver '%s' rejected resolution %dx%d\n", scr->dpix, scr->dpiy);
		return ret;
	}

	scr->w = scr_GetFloatDef("image.width", 297.0f / 25.4f);
	scr->h = scr_GetFloatDef("image.height", 370.0f / 25.4f);

	parseColor(&scr->backclr, "image.backcolor", 0x200020);

	scr->layers = loadLayers("image.layers", &scr->clayers);

	scr->gridmx = scr_GetIntegerDef("grid.extent.x", 1000);
	scr->gridmy = scr_GetIntegerDef("grid.extent.y", 1000);
	scr->grids1 = scr_GetIntegerDef("grid.step.1", 100);
	scr->grids2 = scr_GetIntegerDef("grid.step.2", 50);
	scr->grids3 = scr_GetIntegerDef("grid.step.3", 10);
	scr->gridr.x = scr_GetFloatDef("grid.origin.x", 1.0f);
	scr->gridr.y = scr_GetFloatDef("grid.origin.y", 1.0f);
	scr->gridr.w = scr_GetFloatDef("grid.width", 9.5f);
	scr->gridr.h = scr_GetFloatDef("grid.height", 9.5f);
	scr->gridbw = scr_GetFloatDef("grid.box.weight", 1.0f) / 64.0;
	scr->gridlw = scr_GetFloatDef("grid.lines.weight", 0.75f) / 64.0;
	parseFont(&scr->gridfnt, "grid.font");
	parseColor(&scr->gridclr, "grid.color", 0x9566cb);
	parseColor(&scr->gridmclr, "grid.marks.color", 0x8771b6);

	scr->starclrs = scr_GetStringDef("stars.colors", "");
	scr->starsizes = scr_GetIntegerDef("stars.sizes", 0);

	parseColor(&scr->clustclr, "clusters.lines.color", 0x8181a9);
	scr->clustlw = scr_GetFloatDef("clusters.lines.weight", 1.0f) / 64.0;
	parseColor(&scr->cnameclr, "clusters.names.color", 0x7aa2ef);
	parseFont(&scr->cnamefnt, "clusters.names.font");

	parseColor(&scr->desigclr, "stars.desig.color", 0x989898);
	parseFont(&scr->desigfnt, "stars.desig.font");
	
	scr->desigtab = loadDesignations("stars.desig.table", &scr->cdesigtab);

	scr->starimgs = loadStarImages("stars", scr->starclrs, scr->starsizes, &scr->cstarimgs);
	scr->stars = loadStars(scr_GetString("stars.definition"), &scr->cstars);
	scr->clusters = initClusters(scr->stars, scr->cstars, &scr->cclusters);
	loadClusters(scr_GetString("clusters.definition"), scr);

	scr->csois = scr_GetIntegerDef("sois.count", 0);
	scr->soifont = scr_GetString("sois.font.name");
	scr->soifontfile = scr_GetString("sois.font.filename");
	scr->soifontsize = scr_GetFloatDef("sois.font.size", 16);
	scr->sois = loadSois("sois", scr->csois, scr);

	return ret;
}

static void freeMemory(void** ptr)
{
	if (*ptr)
		free(*ptr);
	*ptr = NULL;
}

static void freeScript(script_t* scr)
{
	int i;

	freeFont(&scr->gridfnt);
	freeFont(&scr->cnamefnt);
	freeFont(&scr->desigfnt);

	// free SoI fonts
	for (i = 0; i < scr->csois; ++i)
	{
		freeFont(&scr->sois[i].font);
	}

	// free star images
	for (i = 0; i < scr->cstarimgs; ++i)
	{
		if (scr->starimgs[i].img)
			drv->freeImage(scr->starimgs[i].img);
		scr->starimgs[i].img = 0;
	}

	freeMemory(&scr->desigtab);
	freeMemory(&scr->starimgs);
	freeMemory(&scr->stars);
	freeMemory(&scr->clusters);
	freeMemory(&scr->sois);
	freeMemory(&scr->objs);
}

static void preprocessStars(script_t* scr)
{
	// lookup and record corresponding star image definitions
	int i;
	star_t* star;

	for (i = 0, star = scr->stars; i < scr->cstars; ++i, ++star)
	{
		const char* cl;
		int iclr;

		cl = strchr(scr->starclrs, star->color);
		if (!cl)
			continue; // cannot draw this color star
		iclr = cl - scr->starclrs;

		star->image = scr->starimgs + iclr * scr->starsizes + star->size;
	}
}

static void drawLayers(const mg_driver_t* dst, script_t* scr)
{
	int i;
	layer_t* l;

	for (i = 0, l = scr->layers; i < scr->clayers; ++i, ++l)
	{
		int j;
		const struct map_layer* map;

		for (j = 0, map = map_layers;
				map->name && strcmp(map->name, l->name) != 0;
				++j, ++map)
			;
		if (!map->name)
		{
			mg_verbose(1, "Warning: layer '%s' not defined\n", l->name);
			continue;
		}

		map->draw(dst, scr);
	}
}

static object_t* addObject(script_t* scr, obj_type_t otype, shape_type_t stype,
		double x0, double y0, double x1, double y1, double rx, double ry)
{
	object_t* obj;

	if (!scr->objs)
	{	// prealloc something decent
		int count = 2000;
		scr->objs = calloc(count, sizeof(*scr->objs));
		if (!scr->objs)
			return 0;
		scr->objalloc = count;
	}
	else if (scr->cobjs >= scr->objalloc)
	{	// need more room
		int count = scr->objalloc + scr->objalloc / 2;
		object_t* newa = realloc(scr->objs, count * sizeof(*scr->objs));
		if (!newa)
			return 0;
		scr->objs = newa;
		scr->objalloc = count;
	}

	obj = scr->objs + scr->cobjs;
	scr->cobjs++;

	obj->type = otype;
	obj->shape.type = stype;
	obj->shape.pt0.x = x0;
	obj->shape.pt0.y = y0;
	obj->shape.pt1.x = x1;
	obj->shape.pt1.y = y1;
	obj->shape.radiusx = rx;
	obj->shape.radiusy = ry;

	return obj;
}

static void shapeToBox(const shape_t* shp, mg_rectf_t* r)
{
	switch (shp->type)
	{
	case sht_Point:
		r->w = r->h = 0;
		r->x = shp->pt0.x;
		r->y = shp->pt0.y;
		break;

	case sht_Line:
	case sht_Rect:
		r->x = shp->pt0.x < shp->pt1.x ? shp->pt0.x : shp->pt1.x;
		r->y = shp->pt0.y < shp->pt1.y ? shp->pt0.y : shp->pt1.y;
		r->w = fabs(shp->pt0.x - shp->pt1.x);
		r->h = fabs(shp->pt0.y - shp->pt1.y);
		break;

	case sht_Ellipse:
		r->x = shp->pt0.x - shp->radiusx;
		r->y = shp->pt0.y - shp->radiusy;
		r->w = shp->radiusx * 2;
		r->h = shp->radiusy * 2;
		break;

	default:
		r->w = r->h = 0;
		r->x = r->y = -1000;
	}
}

static int overlapBox(const mg_rectf_t* r1, const mg_rectf_t* r2)
{
	double dx, dy;
	int overlap = 0;

	dx = r2->x - r1->x;
	if ((dx >= 0 && dx <= r1->w) || (dx < 0 && -dx <= r2->w))
		overlap = 1;

	if (overlap)
	{
		dy = r2->y - r1->y;
		if ((dy >= 0 && dy <= r1->h) || (dy < 0 && -dy <= r2->h))
			return 1;
	}

	return 0;
}

static inline int pointInBox(const mg_rectf_t* r, double x, double y)
{
	double dx = x - r->x;
	double dy = y - r->y;
	return dx >= 0 && dx < r->w && dy >= 0 && dy < r->h;
}

static inline int coordSame(double c1, double c2)
{
#define DIST_THRESH 0.0000001
	return fabs(c1 - c2) < DIST_THRESH;
}

static inline int coordInRange(double c, double l0, double l1)
{
	double t;
	if (l1 < l0)
	{	// swap ends
		t = l0;
		l0 = l1;
		l1 = t;
	}
	return c >= l0 && c <= l1;
}

static inline int pointOnLine(const shape_t* lin, double x, double y)
{
	double dx = lin->pt1.x - lin->pt0.x;
	double dy = lin->pt1.y - lin->pt0.y;
	double dl;
	if (dx == 0 && dy == 0)
	{	// line is a point
		return coordSame(x, lin->pt0.x) && coordSame(y, lin->pt0.y);
	}
	else if (dx == 0)
	{	// line is vertical
		dl = (y - lin->pt0.y) / dy;
		return coordSame(x, lin->pt0.x) && (dl >= 0 && dl <= 1.0);
	}
	
	dl = (x - lin->pt0.x) / dx;
	return coordSame(y, lin->pt0.y + dl * dy) && (dl >= 0 && dl <= 1.0);
}

static inline int pointInEllipse(const shape_t* ell, double x, double y)
{
	double fy, radius_2;
	double dist_2;

	x -= ell->pt0.x;
	y -= ell->pt0.y;

	radius_2 = ell->radiusx * ell->radiusx;
	fy = ell->radiusx / ell->radiusy;

	dist_2 = y * y * fy * fy + x * x;

	return dist_2 <= radius_2;
}

static int overlapLine(const shape_t* lin1, const shape_t* lin2)
{
	const double dx1 = lin1->pt1.x - lin1->pt0.x;
	const double dy1 = lin1->pt1.y - lin1->pt0.y;
	const double dx2 = lin2->pt1.x - lin2->pt0.x;
	const double dy2 = lin2->pt1.y - lin2->pt0.y;
	const double v1t = dx2 * (lin1->pt0.y - lin2->pt0.y) - dy2 * (lin1->pt0.x - lin2->pt0.x);
	const double v2t = dx1 * (lin1->pt0.y - lin2->pt0.y) - dy1 * (lin1->pt0.x - lin2->pt0.x);
	const double dm = dy2 * dx1 - dx2 * dy1; // slope ratio

	if (dm == 0)
	{	// al least parallel
		if (v1t == 0 || v2t == 0)
		{	// coinciding
			const double cx1 = (lin1->pt0.x + lin1->pt1.x);
			const double cx2 = (lin2->pt0.x + lin2->pt1.x);
			const double cy1 = (lin1->pt0.y + lin1->pt1.y);
			const double cy2 = (lin2->pt0.y + lin2->pt1.y);

			return (fabs(cx1 - cx2) <= fabs(dx1) + fabs(dx2) &&
					fabs(cy1 - cy2) <= fabs(dy1) + fabs(dy2) );
		}
		return 0;
	}
	else
	{
		double v1 = v1t / dm;
		double v2 = v2t / dm;

		return (v1 >= 0 && v1 <= 1) && (v2 >= 0 && v2 <= 1);
	}
}

static int overlapBoxWithLine(const mg_rectf_t* r, const shape_t* lin)
{
	double dx, dy, bc;

	// first the easy cases: obvious contained points
	if (pointInBox(r, lin->pt0.x, lin->pt0.y)
			|| pointInBox(r, lin->pt1.x, lin->pt1.y))
		return 1;

	dx = lin->pt1.x - lin->pt0.x;
	dy = lin->pt1.y - lin->pt0.y;

	if (dx == 0 && dy == 0)
	{	// line is a point
		return 0; // point-in-box would catch it
	}
	else if (dx == 0)
	{	// line is vertical
		// check if it crosses the rect completely,
		// otherwise point-in-box would catch it
		return (lin->pt0.x >= r->x) && (lin->pt0.x <= r->x + r->w)
				&& (r->y - lin->pt0.y) * (lin->pt1.y - (r->y + r->h)) >= 0;
	}
	else if (dy == 0)
	{	// line is horizontal
		// check if it crosses the rect completely,
		// otherwise point-in-box would catch it
		return (lin->pt0.y >= r->y) && (lin->pt0.y <= r->y + r->h)
				&& (r->x - lin->pt0.x) * (lin->pt1.x - (r->x + r->w)) >= 0;
	}
	
	// now check intercect with each of the 4 box-bounding lines
	// non-parallel lines always intersect at *some* point in space
	// we just check *which* point
	bc = lin->pt0.y + (r->x - lin->pt0.x) / dx * dy;
	if (coordInRange(bc, r->y, r->y + r->h))
		return 1;

	bc = lin->pt0.y + (r->x + r->w - lin->pt0.x) / dx * dy;
	if (coordInRange(bc, r->y, r->y + r->h))
		return 1;

	bc = lin->pt0.x + (r->y - lin->pt0.y) / dy * dx;
	if (coordInRange(bc, r->x, r->x + r->w))
		return 1;

	bc = lin->pt0.x + (r->y + r->h - lin->pt0.y) / dy * dx;
	if (coordInRange(bc, r->x, r->x + r->w))
		return 1;

	return 0;
}

static int overlapEllipseWithLine(const shape_t* ell, const shape_t* lin)
{
	shape_t l = *lin;
	double dx = l.pt1.x - l.pt0.x;
	double dy = l.pt1.y - l.pt0.y;
	double fx, r, radius_2;
	double a, b, c, d;

	// first the easy cases: obvious contained points
	if (pointInEllipse(ell, lin->pt0.x, lin->pt0.y)
			|| pointInEllipse(ell, lin->pt1.x, lin->pt1.y))
		return 1;

	if (dx == 0 && dy == 0)
	{	// line is a point
		return 0; // point-in-ellipse would catch it
	}
	else if (dx == 0)
	{	// line is vertical
		// check if it crosses the ellipse completely,
		// otherwise point-in-ellipse would catch it
		return (lin->pt0.x >= ell->pt0.x - ell->radiusx)
				&& (lin->pt0.x <= ell->pt0.x + ell->radiusx)
				&& ((ell->pt0.y - ell->radiusy) - lin->pt0.y)
					* (lin->pt1.y - (ell->pt0.y + ell->radiusy)) >= 0;
	}
	else if (dy == 0)
	{	// line is horizontal
		// check if it crosses the ellipse completely,
		// otherwise point-in-ellipse would catch it
		return (lin->pt0.y >= ell->pt0.y - ell->radiusy)
				&& (lin->pt0.y <= ell->pt0.y + ell->radiusy)
				&& ((ell->pt0.x - ell->radiusx) - lin->pt0.x)
					* (lin->pt1.x - (ell->pt0.x + ell->radiusx)) >= 0;
	}

	// make center of ellipse origin
	l.pt0.x -= ell->pt0.x;
	l.pt0.y -= ell->pt0.y;
	l.pt1.x -= ell->pt0.x;
	l.pt1.y -= ell->pt0.y;

	// the (more or less) classic elliptic equation is
	//   (fx*x)^2 + (fy*y)^2 = r^2 (classic: fx=r/a, fy=r/b)
	// we'll take b=r and transform
	//   (fx*x)^2 + y^2 = r^2
	r = ell->radiusy;
	radius_2 = r * r;
	fx = r / ell->radiusx;
	// the classic line equation is
	//   y = b*x + c
	b = dy / dx;
	c = l.pt0.y - l.pt0.x * b;

	//   (b*x + c)^2 = r^2 - fx^2*x^2
	//   b^2*x^2 + 2*b*x*c + c^2 = r^2 - fx^2*x^2
	//   (b^2+fx^2)x^2 + (2*b*c)x + (c^2-r^2) = 0
	a = b*b + fx*fx;
	b = 2 * b * c;
	c = c*c - r*r;
	d = b*b - 4*a*c;

	if (d >= 0)
	{
		double root = sqrt(d);
		double x;

		x = (-b + root) / (2*a);
		if (coordInRange(x, l.pt0.x, l.pt1.x))
			return 1;

		x = (-b - root) / (2*a);
		if (coordInRange(x, l.pt0.x, l.pt1.x))
			return 1;
	}

	return 0;
}

static int overlapBoxWithEllipse(const mg_rectf_t* r, const shape_t* ell)
{
	shape_t lin;

	// first the easy cases: obvious contained points
	if (pointInBox(r, ell->pt0.x, ell->pt0.y)
			|| pointInEllipse(ell, r->x, r->y)
			|| pointInEllipse(ell, r->x + r->w, r->y)
			|| pointInEllipse(ell, r->x, r->y + r->h)
			|| pointInEllipse(ell, r->x + r->w, r->y + r->h))
		return 1;

	// now at least one rect-bounding line must intersect
	// the ellipse, or no overlap
	lin.pt0.x = r->x;
	lin.pt1.x = r->x + r->w;
	lin.pt0.y = lin.pt1.y = r->y;
	if (overlapEllipseWithLine(ell, &lin))
		return 1;

	lin.pt0.y = lin.pt1.y = r->y + r->h;
	if (overlapEllipseWithLine(ell, &lin))
		return 1;

	lin.pt0.y = r->y;
	lin.pt1.y = r->y + r->h;
	lin.pt0.x = lin.pt1.x = r->x;
	if (overlapEllipseWithLine(ell, &lin))
		return 1;

	lin.pt0.x = lin.pt1.x = r->x + r->w;
	if (overlapEllipseWithLine(ell, &lin))
		return 1;

	return 0;
}

static int overlapEllipse(const shape_t* el1, const shape_t* el2)
{
		// This is mostly unfinished
		//  but it's not currently necessary for our application
	double mr1, mr2; // major radius
	double dx, dy;
	double dist;

	// first the easy cases: obvious contained points
	if (pointInEllipse(el1, el2->pt0.x, el2->pt0.y)
			|| pointInEllipse(el2, el1->pt1.x, el1->pt1.y))
		return 1;

	mr1 = el1->radiusx > el1->radiusy ? el1->radiusx : el1->radiusy;
	mr2 = el2->radiusx > el2->radiusy ? el2->radiusx : el2->radiusy;

	// see if too far apart
	dx = el2->pt0.x - el1->pt0.x;
	dy = el2->pt0.y - el1->pt0.y;
	dist = sqrt(dx*dx + dy*dy);
	if (dist > mr1 + mr2)
		return 0; // too far apart

	return 0;
}

static int overlapShapes(const shape_t* obj1, const shape_t* obj2)
{
	mg_rectf_t r1, r2;

	shapeToBox(obj1, &r1);
	shapeToBox(obj2, &r2);

	if (!overlapBox(&r1, &r2))
		return 0;

	switch (obj1->type)
	{
	case sht_Point:
		if (obj2->type == sht_Line)
			return pointOnLine(obj2, r1.x, r1.y);
		if (obj2->type == sht_Ellipse)
			return pointInEllipse(obj2, r1.x, r1.y);
		// fallthrough
	case sht_Rect:
		if (obj2->type == sht_Point || obj2->type == sht_Rect)
			return 1; // overlap is enough
		if (obj2->type == sht_Line)
			return overlapBoxWithLine(&r1, obj2);
		if (obj2->type == sht_Ellipse)
			return overlapBoxWithEllipse(&r1, obj2);
		break;

	case sht_Line:
		if (obj2->type == sht_Point)
			return pointOnLine(obj1, r2.x, r2.y);
		if (obj2->type == sht_Line)
			return overlapLine(obj1, obj2);
		if (obj2->type == sht_Rect)
			return overlapBoxWithLine(&r2, obj1);
		if (obj2->type == sht_Ellipse)
			return overlapEllipseWithLine(obj2, obj1);
		break;

	case sht_Ellipse:
		if (obj2->type == sht_Point)
			return pointInEllipse(obj1, r2.x, r2.y);
		if (obj2->type == sht_Line)
			return overlapEllipseWithLine(obj1, obj2);
		if (obj2->type == sht_Rect)
			return overlapBoxWithEllipse(&r2, obj1);
		if (obj2->type == sht_Ellipse)
			return overlapEllipse(obj1, obj2);
		break;

	default:
		;
	}
	
	return 0;
}

static int isCollidingWith(const script_t* scr, const shape_t* shp, obj_type_t witht)
{
	int i;
	object_t* testobj;

	for (i = 0, testobj = scr->objs; i < scr->cobjs; ++i, ++testobj)
	{
		if (!(testobj->type & witht))
			continue;

		if (overlapShapes(shp, &testobj->shape))
			return 1;
	}

	return 0;
}

static void imageToRectShape(const script_t* scr, const mg_image_t img, shape_t* shp)
{
	const double stepx = scr->gridr.w / scr->gridmx;
	const double stepy = scr->gridr.h / scr->gridmy;
	mg_rectf_t r;

	drv->getImageSize(img, &r);
	shp->type = sht_Rect;
	shp->pt0.x = 0;
	shp->pt0.y = 0;
	shp->pt1.x = r.w / stepx;
	shp->pt1.y = r.h / stepy;
}

static void boxToRectShape(const script_t* scr, const mg_rectf_t* r, shape_t* shp)
{
	const double stepx = scr->gridr.w / scr->gridmx;
	const double stepy = scr->gridr.h / scr->gridmy;

	shp->type = sht_Rect;
	shp->pt0.x = 0;
	shp->pt0.y = 0;
	shp->pt1.x = r->w / stepx;
	shp->pt1.y = r->h / stepy;
}


static int isAlmostMod0(double x, double y)
{
#undef DIST_THRESH
#define DIST_THRESH 0.000001
	double m = fmod(x, y);
	return (m < DIST_THRESH || m > (y - DIST_THRESH));
}

static void drawGrid(const mg_driver_t* dst, script_t* scr)
{
	int i;
	double x, y;
	const double gxstep1 = scr->grids1 ? scr->gridr.w / (scr->gridmx / scr->grids1) : 0;
	const double gystep1 = scr->grids1 ? scr->gridr.h / (scr->gridmy / scr->grids1) : 0;
	const double gxstep2 = scr->grids2 ? scr->gridr.w / (scr->gridmx / scr->grids2) : 0;
	const double gystep2 = scr->grids2 ? scr->gridr.h / (scr->gridmy / scr->grids2) : 0;
	const double gxstep3 = scr->grids3 ? scr->gridr.w / (scr->gridmx / scr->grids3) : 0;
	const double gystep3 = scr->grids3 ? scr->gridr.h / (scr->gridmy / scr->grids3) : 0;
	const double gxl1 = scr->gridr.w / 120;
	const double gyl1 = scr->gridr.h / 120;
	const double gxl2 = scr->gridr.w / 180;
	const double gyl2 = scr->gridr.h / 180;
	const double gxl3 = scr->gridr.w / 240;
	const double gyl3 = scr->gridr.h / 240;
	const mg_rectf_t r = scr->gridr;

	// draw the box
	dst->drawRect(&r, scr->gridclr, scr->gridbw);

	// draw grid lines
	if (gxstep2)
	{
		for (x = r.x + gxstep2; x < r.x + r.w - gxstep2 / 2; x += gxstep2)
			dst->drawLine(x, r.y, x, r.y + r.h, scr->gridclr, scr->gridlw);
		for (y = r.y + gystep2; y < r.y + r.h - gystep2 / 2; y += gystep2)
			dst->drawLine(r.x, y, r.x + r.w, y, scr->gridclr, scr->gridlw);
	}

	// draw ruler markers
	if (gxstep3)
	{
		for (x = r.x; x <= r.x + r.w; x += gxstep3)
		{
			if ((gxstep2 && isAlmostMod0(x - r.x, gxstep2)) ||
					(gxstep1 && isAlmostMod0(x - r.x, gxstep1)))
				continue;
			dst->drawLine(x, r.y - gyl3, x, r.y, scr->gridmclr, scr->gridlw);
			dst->drawLine(x, r.y + r.h, x, r.y + r.h + gyl3, scr->gridmclr, scr->gridlw);
		}
		for (y = r.y; y <= r.y + r.h; y += gystep3)
		{
			if ((gystep2 && isAlmostMod0(y - r.y, gystep2)) ||
					(gystep1 && isAlmostMod0(y - r.y, gystep1)))
				continue;
			dst->drawLine(r.x - gxl3, y, r.x, y, scr->gridmclr, scr->gridlw);
			dst->drawLine(r.x + r.w, y, r.x + r.w + gxl3, y, scr->gridmclr, scr->gridlw);
		}
	}
	if (gxstep2)
	{
		for (x = r.x; x <= r.x + r.w; x += gxstep2)
		{
			if (gxstep1 && isAlmostMod0(x - r.x, gxstep1))
				continue;
			dst->drawLine(x, r.y - gyl2, x, r.y, scr->gridmclr, scr->gridlw);
			dst->drawLine(x, r.y + r.h, x, r.y + r.h + gyl2, scr->gridmclr, scr->gridlw);
		}
		for (y = r.y; y <= r.y + r.h; y += gystep2)
		{
			if (gystep1 && isAlmostMod0(y - r.y, gystep1))
				continue;
			dst->drawLine(r.x - gxl2, y, r.x, y, scr->gridmclr, scr->gridlw);
			dst->drawLine(r.x + r.w, y, r.x + r.w + gxl2, y, scr->gridmclr, scr->gridlw);
		}
	}
	if (gxstep1)
	{
		for (x = r.x; x <= r.x + r.w; x += gxstep1)
		{
			dst->drawLine(x, r.y - gyl1, x, r.y, scr->gridmclr, scr->gridlw);
			dst->drawLine(x, r.y + r.h, x, r.y + r.h + gyl1, scr->gridmclr, scr->gridlw);
		}
		for (y = r.y; y <= r.y + r.h; y += gystep1)
		{
			dst->drawLine(r.x - gxl1, y, r.x, y, scr->gridmclr, scr->gridlw);
			dst->drawLine(r.x + r.w, y, r.x + r.w + gxl1, y, scr->gridmclr, scr->gridlw);
		}
	}

	// draw grid numbers
	if (!scr->gridfnt || !gxstep1)
	{
		mg_verbose(2, "Warning: not enough data to render grid numbers\n");
		return; // nothing else to do
	}

	for (i = 1, x = r.x + gxstep1; x < r.x + r.w - gxstep1 / 2; x += gxstep1, ++i)
	{
		mg_rectf_t tr;
		char buf[20];
		
		sprintf(buf, "%d", i * scr->grids1);
		drv->getTextSize(scr->gridfnt, buf, &tr);
		drv->drawText(scr->gridfnt, x - tr.w / 2, r.y - gyl1 - tr.h, buf, scr->gridclr);
		drv->drawText(scr->gridfnt, x - tr.w / 2, r.y + r.h + gyl1 + tr.h / 6, buf, scr->gridclr);
	}
	for (i = 1, y = r.y + r.h - gystep1; y > r.y + gystep1 / 2; y -= gystep1, ++i)
	{
		mg_rectf_t tr;
		char buf[20];

		sprintf(buf, "%d", i * scr->grids1);
		drv->getTextSize(scr->gridfnt, buf, &tr);
		drv->drawText(scr->gridfnt, r.x - gxl1 - tr.w * 1.125, y - tr.h / 2, buf, scr->gridclr);
		drv->drawText(scr->gridfnt, r.x + r.w + gxl1 + tr.w * 0.125, y - tr.h / 2, buf, scr->gridclr);
	}
}

static void drawStarsSize(const mg_driver_t* dst, script_t* scr, int size)
{
	int i;
	const star_t* star;
	const double orgx = scr->gridr.x;
	const double orgy = scr->gridr.y + scr->gridr.h;
	const double width = scr->gridr.w;
	const double height = scr->gridr.h;
	double stepx = width / scr->gridmx;
	double stepy = height / scr->gridmy;

	for (i = 0, star = scr->stars; i < scr->cstars; ++i, ++star)
	{
		const star_image_t* img = star->image;

		if (star->size != size || !img)
			continue;

		if (img->img)
		{	// use image
			dst->drawImage(orgx + star->pos.x * stepx - img->hot.x,
					orgy - star->pos.y * stepy - img->hot.y,
					img->img);
		}
		else
		{	// this star image is not present, or not drawn
			dst->drawFilledEllipse(
					orgx + star->pos.x * stepx,
					orgy - star->pos.y * stepy,
					img->radius, img->radius, img->rend_color);
		}

		// register an elliptical grid object
		addObject(scr, objt_Star, sht_Ellipse, star->pos.x, star->pos.y,
				0, 0, img->radius / stepx, img->radius / stepy);

	}

}

static void drawStars(const mg_driver_t* dst, script_t* scr)
{
	int i;
	int max = 0;
	star_t* star;

	if (!scr->stars || !scr->starimgs || scr->cstarimgs == 0)
	{
		mg_verbose(2, "Warning: not enough data to render stars\n");
		return;
	}

	// find max star size
	for (i = 0, star = scr->stars; i < scr->cstars; ++i, ++star)
	{
		// make sure the star is not too big for us
		if (star->size >= scr->starsizes)
			star->size = scr->starsizes - 1;

		if (star->size > max)
			max = star->size;
	}

	// draw stars from larger to smaller
	for (i = max; i >= 0; --i)
		drawStarsSize(dst, scr, i);
}

static void drawClusterLines(const mg_driver_t* dst, script_t* scr)
{
	const double orgx = scr->gridr.x;
	const double orgy = scr->gridr.y + scr->gridr.h;
	const double width = scr->gridr.w;
	const double height = scr->gridr.h;
	double stepx = width / scr->gridmx;
	double stepy = height / scr->gridmy;
	int i1;
	int i2;

	if (!scr->clusters)
	{
		mg_verbose(2, "Warning: not enough data to render cluster lines\n");
		return;
	}

	for (i1 = 0; i1 < scr->cclusters; ++i1)
	{
		cluster_t* cluster = scr->clusters + i1;

		for (i2 = 0; i2 < cluster->cconns; ++i2)
		{
			star_t* star1 = cluster->conns[i2][0];
			star_t* star2 = cluster->conns[i2][1];

			dst->drawLine(
					orgx + star1->pos.x * stepx, orgy - star1->pos.y * stepy,
					orgx + star2->pos.x * stepx, orgy - star2->pos.y * stepy,
					scr->clustclr, scr->clustlw);

			// register a line grid object
			addObject(scr, objt_ClusterLine, sht_Line,
					star1->pos.x, star1->pos.y,
					star2->pos.x, star2->pos.y,
					0, 0);
		}
	}
}

static int tryPlaceRect(script_t* scr, obj_type_t kind, mg_rectf_t* r, double x, double y)
{
	shape_t shp;

	// passed rect considered centered around x,y
	shp.type = sht_Rect;
	shp.pt0.x = x - r->w / 2;
	shp.pt0.y = y - r->h / 2;
	shp.pt1.x = shp.pt0.x + r->w;
	shp.pt1.y = shp.pt0.y + r->h;

	if (isCollidingWith(scr, &shp, kind))
		return 0;

	if (shp.pt0.x < 0 || shp.pt1.x >= scr->gridmx || shp.pt0.y < 0 || shp.pt1.y >= scr->gridmy)
		return 0; // out of bounds

	r->x = shp.pt0.x;
	r->y = shp.pt0.y;

	return 1;
}

static void drawSingularNamesSize(const mg_driver_t* dst, script_t* scr, int size)
{
	int i;
	const double orgx = scr->gridr.x;
	const double orgy = scr->gridr.y + scr->gridr.h;
	const double width = scr->gridr.w;
	const double height = scr->gridr.h;
	const double stepx = width / scr->gridmx;
	const double stepy = height / scr->gridmy;

	for (i = 0; i < scr->cclusters; ++i)
	{
		const cluster_t* cluster = scr->clusters + i;
		const star_t* star = scr->stars + cluster->first;
		const star_image_t* img = star->image;
		mg_rectf_t r;
		int placed = 0;
		double radxofs, radyofs;
		double rx, ry;

		// not interested in clusters or singulars or other sizes
		if (cluster->cstars > 1 || star->size != size)
			continue;

		if (!img)
		{
			mg_verbose(2, "Warning: star %s has no image\n", star->cluster);
			continue;
		}

		r.x = r.y = 0;
		dst->getTextSize(scr->cnamefnt, star->cluster, &r);
		// add a little x margin
		r.w += r.h / 12;
		// placement is done in grid coordinates
		r.w /= stepx;
		r.h /= stepy;

		radxofs = img->radius * 1.1 / stepx;
		radyofs = img->radius * 1.1 / stepy;

		// search for a good placement spot
		for (rx = radxofs, ry = radyofs;
				!placed && (rx < radxofs + r.h || ry < radyofs + r.h);
				rx += r.h * 0.1, ry += r.h * 0.1)
		{
			// try below
			placed = tryPlaceRect(scr, objt_TextPlacement, &r,
					star->pos.x, star->pos.y - ry - r.h / 2);
			if (placed)
				continue;
			// try above
			placed = tryPlaceRect(scr, objt_TextPlacement, &r,
					star->pos.x, star->pos.y + ry + r.h / 2);
			if (placed)
				continue;
			// try to the left
			placed = tryPlaceRect(scr, objt_TextPlacement, &r,
					star->pos.x - rx - r.w / 2, star->pos.y);
			if (placed) 
				continue;
			// try to the right
			placed = tryPlaceRect(scr, objt_TextPlacement, &r,
					star->pos.x + rx + r.w / 2, star->pos.y);
		}

		if (!placed)
		{
			mg_verbose(2, "Warning: cannot place '%s' w/o overlap\n", star->cluster);
			continue; // oops
		}

		dst->drawText(scr->cnamefnt,
				orgx + r.x * stepx,
				orgy - (r.y + r.h) * stepy,
				star->cluster, scr->cnameclr);

		// register a rect grid object
		addObject(scr, objt_ClusterText, sht_Rect,
				r.x, r.y, r.x + r.w, r.y + r.h,
				0, 0);
	}
}

static void drawSingularNames(const mg_driver_t* dst, script_t* scr)
{
	int i;
	int max = 0;
	star_t* star;

	// find max star size
	for (i = 0, star = scr->stars; i < scr->cstars; ++i, ++star)
	{
		if (star->size > max)
			max = star->size;
	}

	// draw names in the order of star size, from smaller to larger
	for (i = 0; i <= max; ++i)
		drawSingularNamesSize(dst, scr, i);
}

static void drawClusteredNames(const mg_driver_t* dst, script_t* scr)
{
	int i;
	const double orgx = scr->gridr.x;
	const double orgy = scr->gridr.y + scr->gridr.h;
	const double width = scr->gridr.w;
	const double height = scr->gridr.h;
	const double stepx = width / scr->gridmx;
	const double stepy = height / scr->gridmy;

	// draw cluster names
	for (i = 0; i < scr->cclusters; ++i)
	{
		const cluster_t* cluster = scr->clusters + i;
		const star_t* star = scr->stars + cluster->first;
		const unsigned char* cname = star->cluster;
		mg_rectf_t r;
		int placed = 0;
		int j;
		double maxd;
		double rad, dxrad, ang;

		// not interested in singular stars
		if (cluster->cstars <= 1)
			continue;

		// find the farthest star for center
		for (j = 0, maxd = 0; j < cluster->cstars; ++j, ++star)
		{
			double d = sqrt(SQR(cluster->center.x - star->pos.x)
					+ SQR(cluster->center.y - star->pos.y));
			if (d > maxd)
				d = maxd;
		}
		
		r.x = r.y = 0;
		dst->getTextSize(scr->cnamefnt, cname, &r);
		// add a little x margin
		r.w += r.h / 12;
		// placement is done in grid coordinates
		r.w /= stepx;
		r.h /= stepy;

		// stretch out the ellipse over x
		dxrad = r.w / r.h;
		dxrad -= (dxrad - 1) * 0.5; // not so severe

		for (rad = 0; !placed && rad < maxd + r.h * 3; rad += r.h * 0.1)
		{
			double astep = M_PI / (rad / r.h * 32);

			for (ang = 0; !placed && ang < M_PI * 2; ang += astep)
			{
				placed = tryPlaceRect(scr, objt_TextPlacement, &r,
						cluster->center.x + cos(ang) * rad * dxrad,
						cluster->center.y + sin(ang) * rad);
			}
		}

		if (!placed)
		{
			mg_verbose(2, "Warning: cannot place '%s' w/o overlap\n", cname);
			continue;
		}

		dst->drawText(scr->cnamefnt,
				orgx + r.x * stepx,
				orgy - (r.y + r.h) * stepy,
				cname, scr->cnameclr);

		// register a rect grid object
		addObject(scr, objt_ClusterText, sht_Rect,
				r.x, r.y, r.x + r.w, r.y + r.h,
				0, 0);
	}
}

static void drawClusterNames(const mg_driver_t* dst, script_t* scr)
{
	if (!scr->cnamefnt)
	{
		mg_verbose(2, "Warning: not enough data to render cluster names (missing font)\n");
		return;
	}
	if (!scr->stars || !scr->clusters)
	{
		mg_verbose(2, "Warning: not enough data to render cluster names\n");
		return;
	}

	drawSingularNames(dst, scr);
	drawClusteredNames(dst, scr);
}

static void drawStarDesignation(const mg_driver_t* dst, script_t* scr, const star_t* star)
{
	const double orgx = scr->gridr.x;
	const double orgy = scr->gridr.y + scr->gridr.h;
	const double width = scr->gridr.w;
	const double height = scr->gridr.h;
	double stepx = width / scr->gridmx;
	double stepy = height / scr->gridmy;
	const star_image_t* img = star->image;
	const unsigned char* sdesig = scr->desigtab[star->prefix - 1].str;
	mg_rectf_t r;
	int placed = 0;
	double rad, ang;

	if (!img)
	{
		mg_verbose(2, "Warning: star %s has no image\n", star->cluster);
		return;
	}

	r.x = r.y = 0;
	dst->getTextSize(scr->desigfnt, sdesig, &r);
	// add a little x margin
	r.w += r.h / 8;
	// placement is done in grid coordinates
	r.w /= stepx;
	r.h /= stepy;

	// search for a good placement spot
	for (rad = r.w * 1.06; !placed && rad < r.h * 2.5; rad += r.h * 0.05)
	{
		double astep = M_PI / (rad / r.h * 32);

		for (ang = 0; !placed && ang < M_PI * 2; ang += astep)
		{
			placed = tryPlaceRect(scr, objt_DesigPlacement, &r,
					star->pos.x + cos(ang) * rad,
					star->pos.y + sin(ang) * rad);
		}
	}

	if (!placed)
	{
		mg_verbose(2, "Warning: cannot place %s %d designation w/o overlap\n", star->cluster, star->prefix);
		return; // oops
	}

	dst->drawText(scr->desigfnt,
			orgx + r.x * stepx,
			orgy - (r.y + r.h) * stepy,
			sdesig, scr->desigclr);

	// register a rect grid object
	addObject(scr, objt_Designation, sht_Rect,
			r.x, r.y, r.x + r.w, r.y + r.h,
			0, 0);

}

static void drawStarDesignations(const mg_driver_t* dst, script_t* scr)
{
	int i;

	if (!scr->desigfnt)
	{
		mg_verbose(2, "Warning: not enough data to render star designations (missing font)\n");
		return;
	}
	if (!scr->stars || !scr->desigtab || scr->cdesigtab == 0 || !scr->clusters)
	{
		mg_verbose(2, "Warning: not enough data to render star designations\n");
		return;
	}

	// draw designations
	for (i = 0; i < scr->cclusters; ++i)
	{
		const cluster_t* cluster = scr->clusters + i;
		int s;

		for (s = 0; s < cluster->cstars; ++s)
		{
			const star_t* star = scr->stars + cluster->first + s;

			if (!star->prefix)
				continue; // no designation

			drawStarDesignation(dst, scr, star);
		}
	}
}

static void drawSphereNames(const mg_driver_t* dst, script_t* scr)
{
	int i;
	soi_t* soi;
	const double orgx = scr->gridr.x;
	const double orgy = scr->gridr.y + scr->gridr.h;
	const double width = scr->gridr.w;
	const double height = scr->gridr.h;
	double stepx = width / scr->gridmx;
	double stepy = height / scr->gridmy;
	
	if (!scr->sois)
	{
		mg_verbose(2, "Warning: not enough data to render Spheres of Influence\n");
		return;
	}

	dst->setClipRect(&scr->gridr);

	for (i = 0, soi = scr->sois; i < scr->csois; ++i, ++soi)
	{
		shape_t shp;
		mg_rectf_t r;
		double dstx, dsty;
		mg_color_t clr = scr->backclr;

		if (!soi->name || !soi->font)
		{
			mg_verbose(2, "Warning: not enough data to render SoI name (%d)\n", i + 1);
			continue;
		}

		r.x = r.y = 0;
		dst->getTextSize(soi->font, soi->name, &r);

		dstx = orgx + (soi->center.x + soi->tweak.x) * stepx - r.w / 2;
		dsty = orgy - (soi->center.y + soi->tweak.y) * stepy - r.h / 2;
		// make sure the text is all within the grid
		if (dstx < scr->gridr.x + stepx)
			dstx = scr->gridr.x + stepx;
		else if (dstx + r.w > scr->gridr.x + scr->gridr.w - stepx)
			dstx = scr->gridr.x + scr->gridr.w - stepx - r.w;
		if (dsty < scr->gridr.y)
			dsty = scr->gridr.y;
		else if (dsty + r.h > scr->gridr.y + scr->gridr.h)
			dsty = scr->gridr.y + scr->gridr.h - r.h;

		//soi->clr
		clr.a = 0x80;
		dst->drawText(soi->font, dstx, dsty, soi->name, clr);

		// register a rect grid object
		boxToRectShape(scr, &r, &shp);
		shp.pt0.x += soi->center.x - shp.pt1.x / 2;
		shp.pt0.y += soi->center.y - shp.pt1.y / 2;
		shp.pt1.x += shp.pt0.x;
		shp.pt1.y += shp.pt0.y;
		addObject(scr, objt_SphereText, sht_Rect,
				shp.pt0.x, shp.pt0.y, shp.pt1.x, shp.pt1.y,
				0, 0);
	}

	dst->setClipRect(0);
}

static void drawSpheres(const mg_driver_t* dst, script_t* scr)
{
	int i;
	soi_t* soi;
	const double orgx = scr->gridr.x;
	const double orgy = scr->gridr.y + scr->gridr.h;
	const double width = scr->gridr.w;
	const double height = scr->gridr.h;
	double stepx = width / scr->gridmx;
	double stepy = height / scr->gridmy;
	
	if (!scr->sois)
	{
		mg_verbose(2, "Warning: not enough data to render Spheres of Influence\n");
		return;
	}

	dst->setClipRect(&scr->gridr);

	for (i = 0, soi = scr->sois; i < scr->csois; ++i, ++soi)
	{
		mg_color_t clr = soi->clr;
		double x, y, xr, yr;

		clr.a = 0x40;
		x = orgx + soi->center.x * stepx;
		y = orgy - soi->center.y * stepy;
		xr = soi->radius * stepx;
		yr = soi->radius * stepy;
		dst->drawFilledEllipse(x, y, xr, yr, clr);
		dst->drawEllipse(x, y, xr, yr, clr, 1.0 / 128);

		// register an elliptical grid object
		addObject(scr, objt_Sphere, sht_Ellipse,
				soi->center.x, soi->center.y, 0, 0,
				soi->radius, soi->radius);
	}

	dst->setClipRect(0);
}
