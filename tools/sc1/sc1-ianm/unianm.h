/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _UNIANM_H
#define _UNIANM_H

typedef struct Frame Frame;
typedef struct Anim Anim;
typedef struct MasterPaletteColor MasterPaletteColor;

struct Frame {
	uint16_t width;
	uint16_t height;
	int16_t hotX;
	int16_t hotY;
	bool hasPalette;
	uint8_t transIndex;
	uint8_t *palettes[3];
	uint32_t bytesPerLine;
	uint32_t pixelDataSize;
	uint8_t *pixelData;
};

struct Anim {
	uint16_t frameCount;
	uint8_t bpp;
	struct Frame *frames;
};

struct MasterPaletteColor {
	uint8_t red;
	uint8_t green;
	uint8_t blue;
} __attribute__((packed));

#endif  /* _UNIANM_H */

