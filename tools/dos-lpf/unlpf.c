/**
 * DOS SC2 LPF animation files extractor (.anm?)
 * By Alex Volkov (codepro@usa.net), 20031003
 * GPL applies
 *
 * The frames are dumped into separate raw 8bpp files and
 * the palette (common to all frames) is saved separatelly
 * in PhotoShop .act format
 *
**/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "unlpf.h"

typedef union
{
	struct {
		uint8_t r, g, b;
	} bchan;
	uint8_t channels[3];
} palcolor;


header hdr;
framepackinfo* packs = 0;
const char* outmask = 0;
palcolor* palette = 0;
uint8_t buf[0x10000];
uint8_t* fimg = 0;
uint32_t fimgsize = 0;

int readhead(FILE* f)
{
	if (fread(&hdr, sizeof(hdr), 1, f) != 1)
		return 250;

	fprintf(stderr,"Total frames %u\n", hdr.cframes);

	return 0;
}

int readpalette(FILE* f)
{
	int i;
	lpfpalcolor* lpfpal;

	fprintf(stderr, "Converting palette, %d colors...\n", PAL_COLORS);

	lpfpal = (lpfpalcolor*) alloca(sizeof(lpfpalcolor) * PAL_COLORS);
	palette = (palcolor*) malloc(sizeof(palcolor) * PAL_COLORS);
	if (!lpfpal || !palette)
		return 240;

	fseek(f, hdr.opal, SEEK_SET);

	if (fread(lpfpal, sizeof(lpfpalcolor), PAL_COLORS, f) != PAL_COLORS)
		return 241;

	for (i = 0; i < PAL_COLORS; i++)
	{
		palette[i].bchan.r = lpfpal[i].bchan.r;
		palette[i].bchan.g = lpfpal[i].bchan.g;
		palette[i].bchan.b = lpfpal[i].bchan.b;
	}

	return 0;
}

int writepalette()
{
	FILE* f;
	char filename[250];
	int c;

	strcat(strcpy(filename, outmask), ".act");
	f = fopen(filename, "wb");
	if (!f)
		return 231;

	c = fwrite(palette, sizeof(palcolor), PAL_COLORS, f);
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
		fprintf(stderr, "pack %02d: start frame %03d, frames %03d\n", i, packs[i].frame, packs[i].cframes);

		fseek(f, packofs + i * 0x10000, SEEK_SET);
		fread(pack, FRAMEPACK_SIZE(packs[i]), 1, f);

		for (j = 0; j < pack->info.cframes; ++j)
		{
			fprintf(stderr, "\tframe %03d: size 0x%04x", pack->info.frame + j, pack->sizes[j]);

			if (pack->sizes[j])
			{
				fread(buf, 1, pack->sizes[j], f);

				fprintf(stderr, " dump ");

				for (k = 0; k < 16 && k < pack->sizes[j]; ++k)
					fprintf(stderr, "%02x ", buf[k]);
			}
			fprintf(stderr, "\n");
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

	fprintf(stderr, "\tpacked data: ");
	for (i = 0; i < 16 && i < size; i++)
		fprintf(stderr, "%02x ", frame[i]);
	fprintf(stderr, "\n");
	
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

						//fprintf(stderr, "\t\tlong repeat: %04x, pixel %02x, ofs %04x\n", flags, pixel, fd - frame + 4);
						memset(fo, pixel, count);
						fo += count;
						total += count;
					}
					else
					{	// quote
						//fprintf(stderr, "\t\tlong quote: %04x, ofs %04x\n", flags, fd - frame + 4);
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
				//fprintf(stderr, "\t\tshort skip: %02x, ofs %04x\n", fd->skip.count, (uint8_t*)fd - frame + 4);
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

	fprintf(stderr, "\t\toutput bytes: %04x\n", total);
	
	return total;
}

int packfromframe(uint16_t iframe)
{
	uint16_t i;

	for (i = 0; i < hdr.cpacks && (iframe < packs[i].frame || iframe >= packs[i].frame + packs[i].cframes); ++i)
		;
	
	return i;
}

int processframes(FILE* f)
{
	uint32_t i;
	uint32_t packofs;
	framepack* pack;
	char filename[250];

	pack = (framepack*) alloca(sizeof(framepack) + sizeof(uint16_t) * 128);
	packofs = hdr.opacks + hdr.cpackslots * sizeof(framepackinfo);

	// process frames
	for (i = 0; i < hdr.cframes; ++i)
	{
		int iframe;
		int ofs;
		int j;
		int size;
		FILE* out;
		int ipack = packfromframe(i);

		fseek(f, packofs + ipack * 0x10000, SEEK_SET);
		fread(pack, FRAMEPACK_SIZE(packs[ipack]), 1, f);
		iframe = i - pack->info.frame;
		size = pack->sizes[iframe];

		for (j = 0, ofs = 0; j < iframe; ofs += pack->sizes[j], ++j)
			;

		fprintf(stderr, "frame %03d: pack %03d, index %03d, ofs 0x%04x, size 0x%04x\n", i, ipack, iframe, ofs, size);
		
		fseek(f, ofs, SEEK_CUR);
		fread(buf, 1, size, f);

		processframe(buf, size, fimg);

		sprintf(filename, "%s.%03d.raw", outmask, i);
		out = fopen(filename, "wb");
		if (out)
		{
			fwrite(fimg, 1, fimgsize, out);
			fclose(out);
		}
	}

	return 0;
}

int main(int argc, char* argv[])
{
	FILE *f;
	int ret = 0;

	if (argc != 3) {
		fprintf(stderr, "Usage: %s <lpf-file> <out-name>\n",argv[0]);
		fprintf(stderr, "\twill create raw 8bpp images of mask <out-name>.<frameN>.raw\n");
		fprintf(stderr, "\tand PhotoShop palette file <out-name>.act\n");
		//fprintf(stderr, "\tani format file will be printed to stdout\n");
		return 0;
	}

	outmask = argv[2];

	f = fopen(argv[1],"rb");
	if (!f) {
		fprintf(stderr,"Cannot open %s\n",argv[1]);
		return 1;
	}

	fprintf(stderr,"Reading file %s\n",argv[1]);

	ret = readhead(f);
	if (!ret)
		ret = readpalette(f);
	if (!ret)
		ret = writepalette();
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

	fclose(f);

	return ret;
}
