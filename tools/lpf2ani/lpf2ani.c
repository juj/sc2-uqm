/**
 * DOS SC2 LPF animation files converter (.anm?)
 * By Alex Volkov (codepro@usa.net), 20050121
 * GPL applies
 *
 * The frames are dumped into separate paletted 8bpp PNGs;
 * .ani and presentation .txt files are created
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
#include <png.h>
#if defined(__GNUC__)
#	include <alloca.h>
#endif
#include "lpf.h"

int opt_verbose = 0;
int opt_framerate = 0;
char* opt_inname = 0;
char* opt_outname = 0;
int opt_deltamask = 0;
int opt_fullrects = 0;
int opt_sectorsize = 8;
char* opt_actfile = 0;

header hdr;
framepackinfo* packs = 0;
const char* outmask = 0;
png_color* palette = 0;
uint8_t buf[0x10000];
uint8_t* fimg = 0;
uint32_t fimgsize = 0;

// internals
int error(int errn, const char* fmt, const char* s);
void verbose(const char* fmt, ...);

typedef struct _file_info
{
	int w, h;
	int x, y;
	uint8_t* image;
	int timestamp;
	int index;
	int identical;
	struct _file_info* next;
} file_info;

void file_infos_free();
void file_info_init(file_info* ms);
void file_info_cleanup(file_info* ms);

int cInfos = 0;
file_info* Infos = 0;
int framedelay = 100;	// default 10 fps
int _curframe = -1;
int _curdeltaframe = -1;

int error(int errn, const char* fmt, const char* s)
{
	fprintf(stderr, fmt, s);
	return errn;
}

void verbose(const char* fmt, ...)
{
	va_list args;
	
	if (!opt_verbose)
		return;
	
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
}

int readhead(FILE* f)
{
	if (fread(&hdr, sizeof(hdr), 1, f) != 1)
		return 250;

	verbose("Total frames %u\n", hdr.cframes);

	return 0;
}

int readpalette(FILE* f)
{
	int i;
	lpfpalcolor* lpfpal;

	verbose("Converting palette, %d colors...\n", PAL_COLORS);

	lpfpal = (lpfpalcolor*) alloca(sizeof(lpfpalcolor) * PAL_COLORS);
	palette = (png_color*) malloc(sizeof(png_color) * PAL_COLORS);
	if (!lpfpal || !palette)
		return 240;

	fseek(f, hdr.opal, SEEK_SET);

	if (fread(lpfpal, sizeof(lpfpalcolor), PAL_COLORS, f) != PAL_COLORS)
		return 241;

	for (i = 0; i < PAL_COLORS; i++)
	{
		palette[i].red = lpfpal[i].bchan.r;
		palette[i].green = lpfpal[i].bchan.g;
		palette[i].blue = lpfpal[i].bchan.b;
	}

	return 0;
}

int writepalette(const char* actfile)
{
	FILE* f;
	int c;

	f = fopen(actfile, "wb");
	if (!f)
		return 231;

	c = fwrite(palette, sizeof(palette[0]), PAL_COLORS, f);
	fclose(f);
	
	return c == PAL_COLORS ? 0 : 232;
}

int processpacks(FILE* f)
{
	int i, j, k;
	uint32_t packofs;
	framepack* pack;
	
	packs = (framepackinfo*) calloc(sizeof(framepackinfo), hdr.cpacks);
	pack = (framepack*) alloca(sizeof(framepack) + sizeof(uint16_t) * 128);
	if (!packs || !pack)
		return 230;

	fseek(f, hdr.opacks, SEEK_SET);

	if (fread(packs, sizeof(framepackinfo), hdr.cpacks, f) != hdr.cpacks)
		return 231;

	packofs = hdr.opacks + hdr.cpackslots * sizeof(framepackinfo);

	// dump packs
	for (i = 0; i < hdr.cpacks; ++i)
	{
		verbose("pack %02d: start frame %03d, frames %03d\n", i, packs[i].frame, packs[i].cframes);

		fseek(f, packofs + i * 0x10000, SEEK_SET);
		fread(pack, FRAMEPACK_SIZE(packs[i]), 1, f);

		for (j = 0; j < pack->info.cframes; ++j)
		{
			verbose("\tframe %03d: size 0x%04x", pack->info.frame + j, pack->sizes[j]);

			if (pack->sizes[j])
			{
				fread(buf, 1, pack->sizes[j], f);

				verbose(" dump ");

				for (k = 0; k < 16 && k < pack->sizes[j]; ++k)
					verbose("%02x ", buf[k]);
			}
			verbose("\n");
		}
	}

	return 0;
}

int processframe(uint8_t* frame, int size, uint8_t* image)
{
	uint8_t* fd;
	uint8_t* fe;
	uint8_t* fo;
	int total = 0;
	int i;

	if (size == 0)
		return total;
	
	size -= sizeof(frameheader);
	frame += sizeof(frameheader);

	verbose("\tpacked data: ");
	for (i = 0; i < 16 && i < size; i++)
		verbose("%02x ", frame[i]);
	verbose("\n");
	
	for (fd = frame, fe = frame + size, fo = image; fd < fe; )
	{
		uint8_t flags = *fd++;
		int count = flags & FD_COUNT_MASK;
		
		if (flags & FD_EXT_FLAG)
		{
			if (count == 0)
			{	// ext
				uint16_t flags = *((uint16_t*)fd)++;
				
				if (flags & FD_EXT_FLAG1)
				{	
					count = flags & FD_EXT_RCOUNT_MASK;

					if (flags & FD_EXT_FLAG2)
					{	// repeat
						uint8_t pixel = *fd++;

						//verbose("\t\tlong repeat: %04x, pixel %02x, ofs %04x\n", flags, pixel, fd - frame + 4);
						memset(fo, pixel, count);
						fo += count;
						total += count;
					}
					else
					{	// quote
						//verbose("\t\tlong quote: %04x, ofs %04x\n", flags, fd - frame + 4);
						memcpy(fo, fd, count);
						fd += count;
						fo += count;
						total += count;
					}
				}
				else
				{	// skip
					count = flags & FD_EXT_SCOUNT_MASK;
					fo += count;
					total += count;
				}
			}
			else
			{	// skip
				//verbose("\t\tshort skip: %02x, ofs %04x\n", fd->skip.count, (uint8_t*)fd - frame + 4);
				fo += count;
				total += count;
			}
		}
		else
		{
			if (count == 0)
			{	// repeat
				uint8_t pixel;

				count = *fd++;
				pixel = *fd++;
				memset(fo, pixel, count);
				fo += count;
				total += count;
			}
			else
			{	// quote
				memcpy(fo, fd, count);
				fd += count;
				fo += count;
				total += count;
			}
		}
	}

	verbose("\t\toutput bytes: %04x\n", total);
	
	return total;
}

int packfromframe(uint16_t iframe)
{
	uint16_t i;

	for (i = 0; i < hdr.cpacks && (iframe < packs[i].frame || iframe >= packs[i].frame + packs[i].cframes); ++i)
		;
	
	return i;
}

int write_png(const char* filename, uint8_t* fimg, uint32_t w, uint32_t h, png_colorp pal, uint32_t cpal)
{
	FILE* out;
	png_structp png;
	png_infop pngi;
	png_bytepp rows;
	uint32_t i;

	out = fopen(filename, "wb");
	if (!out)
		return -1;

	png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png)
	{
		fclose(out);
		return -1;
	}
	pngi = png_create_info_struct(png);
	if (!pngi)
	{
		png_destroy_write_struct(&png, (png_infopp)NULL);
		fclose(out);
		return -1;
	}

	rows = malloc(sizeof(png_bytep) * h);
	if (!rows)
	{
		png_destroy_write_struct(&png, (png_infopp)NULL);
		fclose(out);
		return -1;
	}
	for (i = 0; i < h; ++i)
		rows[i] = fimg + i * w;

	if (setjmp(png_jmpbuf(png)))
	{
		png_destroy_write_struct(&png, &pngi);
		free(rows);
		fclose(out);
		return -1;
	}

	png_init_io(png, out);
	png_set_filter(png, 0, PNG_FILTER_NONE);
	png_set_compression_level(png, Z_BEST_COMPRESSION);
	png_set_IHDR(png, pngi, w, h, 8, PNG_COLOR_TYPE_PALETTE, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	png_set_PLTE(png, pngi, pal, cpal);
	png_set_rows(png, pngi, rows);
	//png_write_png(png, pngi, PNG_TRANSFORM_IDENTITY, NULL);
	png_write_info(png, pngi);
	png_write_image(png, rows);
	png_write_end(png, pngi);

	png_destroy_write_struct(&png, &pngi);
	fclose(out);
	free(rows);

	return 0;
}

int compare_images(file_info* i1, file_info* i2)
{
	int cnt = 0;
	int w, h, x, y;
	int dx1, dx2, dy1, dy2;

	if (i1->w > i2->w)
	{
		w = i2->w;
		dx1 = i2->x;
		dx2 = 0;
	}
	else if (i1->w < i2->w)
	{
		w = i1->w;
		dx1 = 0;
		dx2 = i1->x;
	}
	else
	{
		w = i1->w;
		dx1 = dx2 = 0;
	}

	if (i1->h > i2->h)
	{
		h = i2->h;
		dy1 = i2->y;
		dy2 = 0;
	}
	else if (i1->h < i2->h)
	{
		h = i1->h;
		dy1 = 0;
		dy2 = i1->y;
	}
	else
	{
		h = i1->h;
		dy1 = dy2 = 0;
	}

	for (y = 0; y < h; y++)
	{
		for (x = 0; x < w; x++)
		{
			if (i1->image[(y + dy1) * i1->w + x + dx1] != i2->image[(y + dy2) * i2->w + x + dx2])
				cnt++;
		}
	}
	return cnt;
}

int equal_images(int i1, int i2)
{
	// deference identical chain
	while (Infos[i1].identical != -1)
		i1 = Infos[i1].identical;
	while (Infos[i2].identical != -1)
		i2 = Infos[i2].identical;

	if (i1 == i2)
		return 1;

	if (Infos[i1].x != Infos[i2].x || Infos[i1].y != Infos[i2].y 
			|| Infos[i1].w != Infos[i2].w || Infos[i1].h != Infos[i2].h)
		return 0;

	return compare_images(Infos + i1, Infos + i2) == 0;
}

int delta_adjust_positions(int* pos1, int* pos2)
{
	if (*pos1 <= *pos2)
		return 1;
	else
	{
		(*pos1)--;
		(*pos2)--;
		return -1;
	}
}

void clean_expansion_horz(unsigned char* mask, int w, int x1, int y1, int x2, int y2, int threshold)
{
	int x, y, dx, dy;
	// assume anything out of bounds is cleared
	int prevclear = threshold + 1;

	dy = delta_adjust_positions (&y1, &y2);
	dx = delta_adjust_positions (&x1, &x2);

	for (y = y1; y != y2; y += dy)
	{
		int dcnt, ecnt;
		
		dcnt = ecnt = 0;

		for (x = x1; x != x2; x += dx)
		{
			if (mask[y * w + x] == 1)
				dcnt++;
			else if (mask[y * w + x] == 2 || mask[y * w + x] == 3)
				ecnt++;
		}
		
		if (dcnt == 0 && ecnt == 0)
		{	// line is clear
			prevclear++;
		}
		else if (dcnt == 0)
		{
			if (prevclear >= threshold)
			{	// it's not clear yet, but it will be in a moment ;)
				int lx, ly = y;
				
				if (prevclear == threshold)
				{	// need to clean everything we just checked
					ly = y - prevclear * dy;
				}

				for (ly = ly; ly != y + dy; ly += dy)
					for (lx = x1; lx != x2; lx += dx)
						mask[ly * w + lx] = 0;
			}
			prevclear++;
		}
		else
		{	// line is dirty
			prevclear = 0;
		}
	}
}

void clean_expansion_vert(unsigned char* mask, int w, int x1, int y1, int x2, int y2, int threshold)
{
	int x, y, dx, dy;
	// assume anything out of bounds is cleared
	int prevclear = threshold + 1;

	dy = delta_adjust_positions (&y1, &y2);
	dx = delta_adjust_positions (&x1, &x2);

	for (x = x1; x != x2; x += dx)
	{
		int dcnt, ecnt;
		
		dcnt = ecnt = 0;

		for (y = y1; y != y2; y += dy)
		{
			if (mask[y * w + x] == 1)
				dcnt++;
			else if (mask[y * w + x] == 2 || mask[y * w + x] == 3)
				ecnt++;
		}
		
		if (dcnt == 0 && ecnt == 0)
		{	// line is clear
			prevclear++;
		}
		else if (dcnt == 0)
		{
			if (prevclear >= threshold)
			{	// it's not clear yet, but it will be in a moment ;)
				int ly, lx = x;

				if (prevclear == threshold)
				{	// need to clean everything we just checked
					lx = x - prevclear * dx;
				}

				for (lx = lx; lx != x + dx; lx += dx)
					for (ly = y1; ly != y2; ly += dy)
						mask[ly * w + lx] = 0;
			}
			prevclear++;
		}
		else
		{	// line is dirty
			prevclear = 0;
		}
	}
}

struct _expand_corner
{
	int x, y;
	int tx1, ty1;
	int tx2, ty2;
}
const expand_corner [] =
{
	{0, 0,  1, 0,  0, 1},	// top-left
	{1, 0,  0, 0,  0, 1},	// top-mid, from left
	{1, 0,  2, 0,  2, 1},	// top-mid, from right
	{2, 0,  1, 0,  2, 1},	// top-right
	{0, 1,  0, 0,  1, 0},	// mid-left, from top
	{0, 1,  0, 2,  1, 2},	// mid-left, from bottom
	{2, 1,  1, 0,  2, 0},	// mid-right, from top
	{2, 1,  1, 2,  2, 2},	// mid-right, from bottom
	{0, 2,  1, 2,  0, 1},	// bot-left
	{1, 2,  0, 1,  0, 2},	// bot-mid, from left
	{1, 2,  2, 1,  2, 2},	// bot-mid, from right
	{2, 2,  1, 2,  2, 1},	// bot-right

	{-1,-1, -1,-1, -1,-1}	// term
};

// this will recursively expand the missing corner pixels
// recursion is limited so we dont overflow the stack
int expand_rect(char* mask, int x, int y, int w, int h)
{
	static int level = 0;
	int x1, y1, x2, y2, i, lx, ly;
	const struct _expand_corner* pc;
	signed char matrix[3][3];
	int cnt = 0;

	if (level > 99)
		return 1;	// make sure parent knows it failed

	level++;

	if (x > 0)
		x1 = x - 1;
	else
	{
		for (i = 0; i < 3; i++)
			matrix[0][i] = -1;
		
		x1 = x;
	}

	if (y > 0)
		y1 = y - 1;
	else
	{
		for (i = 0; i < 3; i++)
			matrix[i][0] = -1;
		
		y1 = y;
	}

	if (x + 1 < w)
		x2 = x + 2;
	else
	{
		for (i = 0; i < 3; i++)
			matrix[2][i] = -1;
		
		x2 = x + 1;
	}

	if (y + 1 < h)
		y2 = y + 2;
	else
	{
		for (i = 0; i < 3; i++)
			matrix[i][2] = -1;

		y2 = y + 1;
	}

	for (ly = y1; ly < y2; ly++)
		for (lx = x1; lx < x2; lx++)
			matrix[lx - x + 1][ly - y + 1] = mask[ly * w + lx];

	// check corner pixels
	for (pc = expand_corner; pc->x != -1; pc++)
	{
		if (matrix[pc->x][pc->y] == 0 && matrix[pc->tx1][pc->ty1] > 0 && matrix[pc->tx2][pc->ty2] > 0)
		{	// corner pixel missing
			int ofs = (y - 1 + pc->y) * w + (x - 1 + pc->x);
			
			matrix[pc->x][pc->y] = 3;

			// but it may already be present in the mask (recursive)
			if (mask[ofs] == 0)
			{
				mask[ofs] = 3;
				cnt += 1 + expand_rect (mask, x - 1 + pc->x, y - 1 + pc->y, w, h);
			}
		}
	}

	level--;

	return cnt;
}

file_info* file_info_add_image (file_info* fi)
{
	file_info* ni;

	ni = (file_info*) malloc(sizeof(file_info));
	if (!ni)
		return 0;

	file_info_init(ni);

	while (fi->next)
		fi = fi->next;

	return fi->next = ni;
}

int is_multi_delta_image (file_info* fi)
{
	return fi && fi->next;
}

#define MASK_COLORS 4
png_color mask_pal[MASK_COLORS] =
{
	{0x00, 0x00, 0x00},
	{0xff, 0xff, 0xff},
	{0x00, 0xff, 0x00},
	{0x00, 0x00, 0xff}
};

void create_mask_png (uint8_t* mask, int w, int h)
{
	char fname[260];

	sprintf(fname, "mask_%03d_%03d.png", _curframe, _curdeltaframe);
	write_png(fname, mask, w, h, mask_pal, MASK_COLORS);
}

int build_delta(file_info* i1, file_info* i2)
{
	int w, h, x, y;
	int dx1, dx2, dy1, dy2;
	int cnt;
	char* mask = 0;

	if (i1->w > i2->w)
	{
		w = i2->w;
		dx1 = i2->x;
		dx2 = 0;
	}
	else if (i1->w < i2->w)
	{
		w = i1->w;
		dx1 = 0;
		dx2 = i1->x;
	}
	else
	{
		w = i1->w;
		dx1 = dx2 = 0;
	}

	if (i1->h > i2->h)
	{
		h = i2->h;
		dy1 = i2->y;
		dy2 = 0;
	}
	else if (i1->h < i2->h)
	{
		h = i1->h;
		dy1 = 0;
		dy2 = i1->y;
	}
	else
	{
		h = i1->h;
		dy1 = dy2 = 0;
	}

	mask = (char*) malloc(w * h);
	if (!mask)
		return 220;
	memset(mask, 0, w * h);

	// build diff mask first
	for (y = 0; y < h; y++)
	{
		for (x = 0; x < w; x++)
		{
			if (i1->image[(y + dy1) * i1->w + x + dx1] != i2->image[(y + dy2) * i2->w + x + dx2])
				// diff pixel
				mask[y * w + x] = 1;
		}
	}

	// coarse expand the diff pixels
	for (y = 0; y < h; y++)
	{
		for (x = 0; x < w; x++)
		{
			if (mask[y * w + x] == 1)
			{
				int x1 = x - 2;
				int x2 = x + 3;
				int y1 = y - 2;
				int y2 = y + 3;
				int lx;
				if (x1 < 0)
					x1 = 0;
				if (x2 > w)
					x2 = w;
				if (y1 < 0)
					y1 = 0;
				if (y2 > h)
					y2 = h;

				for (y1 = y1; y1 < y2; y1++)
					for (lx = x1; lx < x2; lx++)
						if (mask[y1 * w + lx] == 0)
							mask[y1 * w + lx] = 2;
			}
		}
	}

	// scan and remove extra expansion horizontally and vertically
	clean_expansion_vert (mask, w, 0, 0, w, h, 1);
	clean_expansion_vert (mask, w, w, 0, 0, h, 1);
	clean_expansion_horz (mask, w, 0, 0, w, h, 1);
	clean_expansion_horz (mask, w, 0, h, w, 0, 1);

	
	do	// coarse expand the diff pixels
	{	// merge would-be diff rectangles in the process
		cnt = 0;

		for (y = 0; y < h; y++)
		{
			for (x = 0; x < w; x++)
			{
				if (mask[y * w + x] != 0)
					cnt += expand_rect (mask, x, y, w, h);
			}
		}
		// repeat is something was expanded
	} while (cnt != 0);

	// at this point we should have guaranteed non-overlapping
	// rectangles that cover all of the delta areas

	if (opt_sectorsize)
	{	// final expansion cleanup
		for (y = 0; y < h; y += opt_sectorsize)
		{
			for (x = 0; x < w; x += opt_sectorsize)
			{
				int x2, y2;

				cnt = 0;
				for (y2 = y; y2 < y + opt_sectorsize && y2 < h; y2++)
					for (x2 = x; x2 < x + opt_sectorsize && x2 < w; x2++)
						if (mask[y2 * w + x2] == 1)
							cnt++;

				if (cnt > 0)
					continue;	// dirty sector

				// clean up sector
				for (y2 = y; y2 < y + opt_sectorsize && y2 < h; y2++)
					for (x2 = x; x2 < x + opt_sectorsize && x2 < w; x2++)
						mask[y2 * w + x2] = 0;

			}
		}
	}

	// check how muany pixels have to be replaced
	for (x = 0, cnt = 0; x < w * h; x++)
		if (mask[x])
			cnt++;

	if (opt_deltamask)
		create_mask_png(mask, w, h);

	// generate delta images
	if (cnt != w * h)
	{
		int ofs;

		for (y = 0, ofs = 0; y < h; y++)
		{
			for (x = 0; x < w; x++, ofs++)
			{
				if (mask[ofs] != 0)
				{	// copy masked rectangle into a new image
					// and clear the mask
					int i;
					int rw, rh;
					int x2, y2;
					unsigned char* src;
					unsigned char* dst;
					file_info* ni;
					
					ni = file_info_add_image (i1);
					if (!ni)
					{
						x = w;
						y = h;
						break;
					}
		

					// lookup delta rectangle
					for (i = x, src = mask + ofs; i < w && *src != 0; i++, src++)
						;
					ni->w = rw = i - x;

					if (opt_fullrects)
					{	// locate only complete rectangles
						y2 = y + 1;
						for (i = y + 1, src = mask + ofs; i < h && *src != 0; i++, src += w)
						{
							unsigned char* src2;

							y2 = i;
							for (x2 = x, src2 = src; x2 < x + rw && *src2 != 0; x2++, src2++)
								;

							if (x2 < x + rw)
								break;
						}
					}
					else
					{	// any rectangles
						for (y2 = y + 1, src = mask + ofs; y2 < h && *src != 0; y2++, src += w)
							;
					}
					
					ni->h = rh = y2 - y;

					ni->image = (unsigned char*) malloc (rw * rh);
					if (!ni->image)
					{
						x = w;
						y = h;
						break;
					}

					// copy the pixels
					for (i = 0, src = i1->image + (dy1 + y) * i1->w + dx1 + x, dst = ni->image;
							i < rh;
							i++, src += i1->w, dst += rw
						)
					{
						memcpy (dst, src, rw);
						memset (mask + ofs + i * w, 0, rw);
					}

					ni->x = i1->x + dx1 + x;
					ni->y = i1->y + dy1 + y;
				}
			}
		}

		if (i1->next)
		{	// dispose of the original
			file_info* ni = i1->next;
			free (i1->image);
			i1->image = ni->image;
			i1->x = ni->x;
			i1->y = ni->y;
			i1->w = ni->w;
			i1->h = ni->h;
			i1->next = ni->next;
			
			free (ni);
		}
	}
	else
	{	// break here
		cnt = 1;
	}

	if (mask)
		free (mask);

	return 0;
}

void delta_images(void)
{
	int i;

	verbose("calculating frame image deltas");

	// remove dupes
	for (i = 1; i < cInfos; i++)
	{
		int i2;

		// dereference identical chain
		for (i2 = i - 1; Infos[i2].identical != -1; i2 = Infos[i2].identical)
			;
		if (equal_images(i, i2))
		{
			Infos[i].identical = i2;
			// dont need image data anymore
			if (Infos[i].image)
			{
				free(Infos[i].image);
				Infos[i].image = 0;
			}
		}
		verbose (".", 0);
	}
	verbose ("|", 0);

	// compute deltas
	for (i = cInfos - 1; i > 0; i--)
	{
		int i2;

		if (Infos[i].identical != -1)
			// no delta needed
			continue;
		else
		{
			// deref indentical chain
			for (i2 = i - 1; i2 >= 0 && Infos[i2].identical != -1; i2 = Infos[i2].identical)
				;
			
			// debug info
			_curframe = i;
			_curdeltaframe = i2;

			build_delta (Infos + i, Infos + i2);
		}
		verbose (".", 0);
	}	
	
	verbose ("\n", 0);
}

void file_info_init(file_info* ms)
{
	memset(ms, 0, sizeof(*ms));
	ms->identical = -1;
	ms->index = -1;
}

void file_info_cleanup(file_info* ms)
{
	file_info* fi = ms;

	while (fi)
	{
		file_info* tempi = fi;

		if (fi->image)
		{
			free(fi->image);
			fi->image = 0;
		}

		fi = fi->next;
		if (tempi != ms)
			free (tempi);
	}
}

void file_infos_free()
{
	int i;

	if (Infos)
	{
		for (i = 0; i < cInfos; i++)
			file_info_cleanup (Infos + i);

		free (Infos);
		Infos = 0;
	}
}

int processframes(FILE* f)
{
	uint32_t i;
	uint32_t packofs;
	framepack* pack;
	//char filename[250];

	pack = (framepack*) alloca(sizeof(framepack) + sizeof(uint16_t) * 128);
	packofs = hdr.opacks + hdr.cpackslots * sizeof(framepackinfo);

	cInfos = hdr.cframes;
	Infos = malloc(sizeof(file_info) * cInfos);
	if (!Infos)
		return 251;
	memset(Infos, 0, sizeof(file_info) * cInfos);

	// process frames
	for (i = 0; i < hdr.cframes; ++i)
	{
		int iframe;
		int ofs;
		int j;
		int size;
		int ipack = packfromframe(i);
		file_info* rf = Infos + i;

		fseek(f, packofs + ipack * 0x10000, SEEK_SET);
		fread(pack, FRAMEPACK_SIZE(packs[ipack]), 1, f);
		iframe = i - pack->info.frame;
		size = pack->sizes[iframe];

		for (j = 0, ofs = 0; j < iframe; ofs += pack->sizes[j], ++j)
			;

		verbose("frame %03d: pack %03d, index %03d, ofs 0x%04x, size 0x%04x\n", i, ipack, iframe, ofs, size);
		
		fseek(f, ofs, SEEK_CUR);
		fread(buf, 1, size, f);

		processframe(buf, size, fimg);

		file_info_init(rf);
		rf->image = malloc(fimgsize);
		if (!rf->image)
			return 221;
		memcpy(rf->image, fimg, fimgsize);
		rf->w = hdr.w;
		rf->h = hdr.h;
		rf->x = 0;
		rf->y = 0;
	}

	return 0;
}

int write_ani_file(const char* outname)
{
	char filename[250];
	FILE* fani;
	FILE* fscr;
	int i;
	int sync;
	int imgind;

	verbose("Writing ANI, Script and PNG files\n");

	sprintf(filename, "%s.ani", outname);
	fani = fopen(filename, "wt");
	if (!fani)
		return 211;

	sprintf(filename, "%s.txt", outname);
	fscr = fopen(filename, "wt");
	if (!fscr)
	{
		fclose(fani);
		return 211;
	}

	for (i = 0, imgind = 0, sync = 0; i < cInfos; ++i, sync += framedelay)
	{
		file_info* rf = Infos + i;
		int new_png;
		int draw_cmd;
		int batched;

		if (rf->identical == -1)
		{	// unique frame
			new_png = 1;
			draw_cmd = 1;
		}
		else
		{	// frame identical to another
			new_png = 0;

			if (i > 0 && (rf->identical == i - 1 || rf->identical == Infos[i - 1].identical))
				draw_cmd = 0;	// frame has not changed since last
			else
				draw_cmd = 1;	// frame changed

			rf = Infos + rf->identical;
		}

		if (draw_cmd)	
			fprintf(fscr, "#(Sync frame)\n"
					"SYNC %d\n", sync);

		batched = 0;
		for ( ; rf && rf->image; rf = rf->next)
		{
			if (new_png)
			{	// produce new png
				rf->index = imgind++;

				sprintf(filename, "%s.%03d.png", outname, rf->index);
				// produce PNG
				write_png(filename, rf->image, rf->w, rf->h, palette, PAL_COLORS);
				// add it to .ani
				fprintf(fani, "%s -1 -1 %d %d\n", filename, -rf->x, -rf->y);
			}
			
			if (draw_cmd)
			{	// add it to script
				if (!batched && rf->next)
				{	// drawing a batch
					batched = 1;
					fprintf(fscr, "#(Batch several draws)\n"
							"BATCH\n");
				}
				fprintf(fscr, "#(Draw frame image)\n"
						"DRAW %d\n", rf->index);
			}
		}
		if (batched && draw_cmd)
		{	// unbatch
			fprintf(fscr, "#(End of batch)\n"
					"UNBATCH\n");
		}

		verbose(".");
	}
	// sync last frame
	fprintf(fscr, "#(Sync frame)\n"
			"SYNC %d\n", sync);
	verbose("\n");

	fclose(fscr);
	fclose(fani);
	
	return 0;
}

void usage()
{
	fprintf(stderr, "usage: lpf2ani [-v] [-f rate] [-r] [-s size] [-o out-name] <lpf-file>\n");
	fprintf(stderr, "\tconverts DOS LPF animation file to .ani format and scripts the animation\n");
	fprintf(stderr, "\twill create <out-name>.ani, <out-name>.txt, and\n");
	fprintf(stderr, "\t8bpp paletted PNGs of mask <out-name>.<frameN>.png\n");
	fprintf(stderr, "options:\n");
	fprintf(stderr, "  -v\t\t : be verbose, explains things no human should know\n");
	fprintf(stderr, "  -f rate\t : override framerate; rate is 1..100 per second\n");
	fprintf(stderr, "  -r\t\t : split delta frames into full rectangles only\n");
	fprintf(stderr, "  -s size\t : enable sector cleanup and set sector size (8..64)\n");
	fprintf(stderr, "  -p actname\t : produce PhotoShop palette file (.act)\n");
	fprintf(stderr, "diagnostical options:\n");
	fprintf(stderr, "  -d\t\t : generate delta-mask PNGs (form: mask_FRM1_FRM2.png)\n");
}

void parse_arguments(int argc, char *argv[])
{
	char ch;
	
	while ((ch = getopt(argc, argv, "?hvdrf:s:o:p:")) != -1)
	{
		switch(ch)
		{
		case 'o':
			opt_outname = optarg;
			break;
		case 'f':
			opt_framerate = atoi(optarg);
			if (opt_framerate < 1 || opt_framerate > 100)
			{
				fprintf(stderr, "invalid -f option value\n");
				usage();
				exit(EXIT_FAILURE);
			}
			break;
		case 'd':
			opt_deltamask = 1;
			break;
		case 'r':
			opt_fullrects = 1;
			break;
		case 's':
			opt_sectorsize = atoi(optarg);
			if (opt_sectorsize < 8 || opt_sectorsize > 64)
			{
				fprintf(stderr, "invalid -r option value\n");
				usage();
				exit(EXIT_FAILURE);
			}
			break;
		case 'v':
			opt_verbose = 1;
			break;
		case 'p':
			opt_actfile = optarg;
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
	{
		char* pch;

		opt_outname = (char*) calloc (strlen(opt_inname) + 6, 1);
		if (!opt_outname)
			return error (EXIT_FAILURE, "No memory", 0);

		pch = strrchr(opt_inname, '.');
		if (!pch)
			pch = opt_inname + strlen(opt_inname);

		strncpy(opt_outname, opt_inname, pch - opt_inname + 1);
		opt_outname[pch - opt_inname] = '\0';

		verbose("Output name: %s\n", opt_outname);
	}

	f = fopen(opt_inname, "rb");
	if (!f)
	{
		fprintf(stderr, "Cannot open %s\n", opt_inname);
		return 1;
	}

	verbose("Reading file %s\n", opt_inname);

	ret = readhead(f);
	if (!ret)
		ret = readpalette(f);
	if (!ret)
	{
		framedelay = 1000 / hdr.fps;
		if (opt_framerate)
		{	// override delay
			framedelay = 1000 / opt_framerate;
		}
		verbose("using framedelay of %d, effective fps %d\n", framedelay, 1000 / framedelay);
	}

	if (!ret && opt_actfile)
		ret = writepalette(opt_actfile);
	if (!ret)
	{
		fimgsize = hdr.w * hdr.h;
		fimg = malloc(fimgsize);
		if (!fimg)
			ret = 101;
	}
	if (!ret)
		ret = processpacks(f);
	if (!ret)
		ret = processframes(f);

	if (!ret)
	{
		delta_images();
		ret = write_ani_file(opt_outname);
	}

	file_infos_free();
	fclose(f);

	return ret;
}
