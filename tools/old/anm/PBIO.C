/*--pbio.c----------------------------------------------------------*
 *								    *
 *  Routines to read & write PC Paintbrush files.		    *
 *								    *
 * Date      Who Changes					    *
 * --------- --- -------------------------------------------------- *
 * 31-Mar-89 mjc band read & write to allow large files.	    *
 * 19-Apr-89 mjc 256 color palette added at end of paintbrush file. *
 *								    *
 * TBD: haven't added scroll-offset logic to "to planar" case, since
 * ANIMDISK is only 8 bpp.
 *------------------------------------------------------------------*/

#include "main.h"
#include <fcntl.h>
#include <malloc.h>
#include <sys\types.h>
#include <sys\stat.h>
#include <io.h>
#include <stdio.h>

#define local static

/* RowBytes computes the number of bytes in a row, from the width in pixels.*/
#define RowBytes(w)   (((w) + 15) >> 4 << 1)

/*------------------------------------------------------------------
 *			256 Color Palette
 *
 *  When reading the paintbrush file, the 256 color palette is
 *  sought only if the version number in the header is 5 and the
 *  bits per pixel * planes == 8.  The last 769 bytes of the file
 *  must start with 0x0C, and be followed by the 768 byte palette.
 *
 *  When writing the paintbrush file, we write
 *  version 5 in the header, and append the palette to the end of
 *  the file in the same format as above (0x0C followed by 768
 *  bytes of palette).
 *------------------------------------------------------------------
 */

/* Return SUCCESS, FAIL, or specific errors ... */
#define PB_NOT_ENUF_MEM		2
#define PB_NO_256_PALETTE	3

local WORD readCount;	/* For compression. */
local UBYTE readChar;

/* --- Forward declarations. */
void GetByte(WORD file);
BOOL PutBytes(BYTE far * ptr, WORD bytes, WORD file);

/*-----------------------------------------------------------------*/

typedef struct {
	BYTE manufacturer;
	BYTE version;
	BYTE encoding;
	BYTE bitsperpixel;
	WORD xmin, ymin, xmax, ymax;
	WORD hres, vres;
	BYTE colormap[16][3];
	BYTE reserved;
	BYTE nplanes;
	WORD bytesperline;
} PCPAINTBRUSH_HEADER;

local PCPAINTBRUSH_HEADER *pbh = NULL;	/* paintbrush header pointer */
	/* Global pointer so can allocate in header, use fields in body. */

/*--------------------- Bad Paintbrush Header ----------------------*/
#define BadPbHeader(s)		(FAIL)

/*-------------------- Invalid Paintbrush File ---------------------*/
#define InvalidPbFile()		(FAIL)

#define WrongPbFileType()	(FAIL)	/* Expected different data format.*/
#define PCXNo256PaletteMsg()

/*-----------------------------------------------------------------*
 *  Read the header of a pc paintbrush file.
 *  Sets bmHdr fields.  Calls reSize if necessary.
 *  Returns SUCCESS or FAIL.
 *-----------------------------------------------------------------*/

BOOL ReadPCX_Header(WORD file, BITMAP *bitmap, LONG *palette,
					BitMapHeader *bmHdr)
{
	register WORD i;
	UBYTE c, palet[256][3];

	if (!pbh)			/* Test in case we come through twice w/o freeing. */
		pbh = malloc(sizeof(PCPAINTBRUSH_HEADER) + 4);
	if (!pbh)
		return (FAIL);

	if (read(file, (char *)pbh, sizeof(PCPAINTBRUSH_HEADER)) !=
		sizeof(PCPAINTBRUSH_HEADER))
		return (InvalidPbFile());

	/* --- To start of pixel data. */
	if (0 > lseek(file, (LONG) 128, SEEK_SET))
		return (InvalidPbFile());

	/* --- Check that the header info is valid. */
	if (pbh->manufacturer != 10)
		return (InvalidPbFile());

	/* --- Expecting version with palette at end. */
	if (pbh->version != 5)
		return (BadPbHeader(s_wrongPBVersion));

	if (pbh->encoding != 1)
		return (BadPbHeader(s_unknownEncoding));

	if (pbh->bitsperpixel != 1 && pbh->nplanes != 1)
		return (BadPbHeader(s_wrongLayout));

	if (pbh->bitsperpixel != 8)
		return (WrongPbFileType());

	/* --- Compute dimensions and # colors. */
	setmem(bmHdr, sizeof(BitMapHeader), 0);
	bmHdr->w = 1 + pbh->xmax - pbh->xmin;
	bmHdr->h = 1 + pbh->ymax - pbh->ymin;
	bmHdr->nPlanes = pbh->bitsperpixel * pbh->nplanes;
			/* MAY BE PACKED. */

	/* --- Get the color map. */
	if (palette != NULL) {
		/* If a 256 color palette should be present at the end of the
		 * file, try to fetch it from disk and copy it to the palette.
		 */
		c = 0;
		if (lseek(file, (LONG)-769, SEEK_END) >= 0)
			if (read(file, &c, 1) == 1)
				if (read(file, (UBYTE *)&palet[0][0], 768) == 768)
					if (c == 0x0C)
						for (i = 0; i < 256; ++i)
							palette[i] = ((LONG)palet[i][0] << 16) +
								((LONG)palet[i][1] << 8) +
								((LONG)palet[i][2]);
			/* --- To start of pixel data (again). */
			lseek(file, (LONG)128, SEEK_SET);
		if (c != 0x0C) {
			/* ---- We couldn't read a 256 color palette at the end
			 * of the file.
			 */
			PCXNo256PaletteMsg();
			return (PB_NO_256_PALETTE);
		}
	}
	return (SUCCESS);			/* Header done; BODY yet to do. */
}

/*-----------------------------------------------------------------*
 *  Read the pixel data into the bitmap.  Return SUCCESS or FAIL.
 *-----------------------------------------------------------------*/
#define MaxPCXPlanes	8		/* 256-color, no mask. */

BOOL ReadPCX_BODY(WORD file, BITMAP *bitmap, BitMapHeader *bmHdr)
{
	register WORD i, iPlane;
	BOOL result = SUCCESS;
	BYTE far *dest;
	WORD bodyNRows, srcPlanesToUse;
	WORD bodyIRow;	/* Increment from 0..bodyNRows-1. */
	BYTE far *dests[MaxPCXPlanes];	/* Ptrs to dest scan lines. */
	WORD useBytes;

	/* --------- Initialization. ---------- */
	readCount = 0;				/* For compression. */

	bodyNRows = MIN(bmHdr->h, bitmap->box.h);
	srcPlanesToUse = MIN(bitmap->planes + bpp_minus1, bmHdr->nPlanes);

	/* ---------- Read all scan lines. ---------- */
	OpenIOBuffer(FALSE);

	for (bodyIRow = 0; bodyIRow < bodyNRows; ++bodyIRow) {
		readCount = 0;	 /* Just in case, supposed to start anew each line. */

		useBytes = MIN(bitmap->width, pbh->bytesperline);

		/* ----- Read a scan line. */
		dest = (BYTE far *)BMLineStart(bitmap, bodyIRow, 0);
		for (i = 0; i < pbh->bytesperline; i++) {
			GetByte(file);
			if (i < useBytes)
				*dest++ = readChar;
		}
	}
	CloseIOBuffer(file);

done:
	return (result);
}

/*--------------------- Close Paintbrush File ---------------------*/
/* --- NOTE: if change, must change WritePCX_Done, since it calls this. */

void ReadPCX_Done()
{
		/* --- Free pbh, which is shared by header & body routines. */
	if (pbh)
		free(pbh);
	pbh = NULL;
}

void WritePCX_Done()
{
	ReadPCX_Done();
}

/*---------------------------- Get Byte ---------------------------*/
/* Set readCount & readChar.  At EOF or file error, returns 0's forever. */

void GetByte(WORD file)
{
	UBYTE c;

		/* --- if in the middle of a run of encoded bytes, don't read any
		 *    more
		 */
	if (readCount > 0) {
		--readCount;
		return;
	}
	readCount = 0;
	if (GReadOne(file, &c) != 1) {
sawEOF:
		readCount = 32767;		/* Huge count of 0's to pad the end. */
		readChar = 0;
		return;
	}
	if ((c & 0xc0) == 0xc0) {
		readCount = (0x3f & c) - 1;		/* "-1": consuming one of count. */
		if (GReadOne(file, &c) != 1)
			goto sawEOF;
	}
	readChar = c;
}

/*----------------------------------------------------------------------*
	 Output File Routines & Variables
 *----------------------------------------------------------------------*/

/*---------------------------------------------------------------*
	Output header for a PC Paintbrush file with the given number
    of bitsperpixel and planes.  One of these two parms must be 1.
	ASSUMES OKAY TO TRASH buffer[].
	Returns SUCCESS or FAIL.
 *---------------------------------------------------------------*/

BOOL WritePCX_Header(WORD file, BITMAP *bitmap, LONG *palette)
{
	register PCPAINTBRUSH_HEADER *ppbh;
	WORD i;
#define GAP_SIZE	(128 - sizeof(PCPAINTBRUSH_HEADER))
	UBYTE buffer1[GAP_SIZE];

		/* --- init the header that we'll write.
		 *		Note: since DPaint never reads & writes at the same time,
		 *		it is okay to share the same read & write structure,
		 *		unlike in CONVERT.EXE.
		 */
	if (!pbh)					/* Test in case we come through twice w/o
								 * freeing. */
		pbh = malloc(sizeof(PCPAINTBRUSH_HEADER) + 4);
	if (!pbh)
		return (FAIL);
	ppbh = pbh;				/* Register variable for smaller, faster code. */

	setmem(ppbh, sizeof(PCPAINTBRUSH_HEADER) + 4, 0);
	ppbh->manufacturer = 10;
	ppbh->version = 5;
	ppbh->encoding = 1;

	ppbh->bitsperpixel = bpp;
	ppbh->xmin = ppbh->ymin = 0;
	ppbh->xmax = bitmap->box.w - 1;
	ppbh->ymax = bitmap->box.h - 1;
	ppbh->hres = N_COLUMNS;
	ppbh->vres = N_LINES;

	/* --- bytesperline doesn't take into account multiple planes.
	 * Output in same format as bitmap (planar vs packed).
	 */
	ppbh->bytesperline = bitmap->width;

	ppbh->nplanes = bitmap->planes;
	ppbh->reserved = 0;

	/* --- First 16 colors of our palette info. */
	for (i = 0; i < 16; ++i) {
		ppbh->colormap[i][0] = (BYTE)(palette[i] >> 16);
		ppbh->colormap[i][1] = (BYTE)(palette[i] >> 8);
		ppbh->colormap[i][2] = (BYTE) palette[i];
	}

	/* --- write the 128-byte header */
	if (GWrite(file, (char *)ppbh, sizeof(PCPAINTBRUSH_HEADER))
		!= sizeof(PCPAINTBRUSH_HEADER))
		goto error;

	setmem(buffer1, GAP_SIZE, 0);
	if (GWrite(file, buffer1, GAP_SIZE) != GAP_SIZE)
error:
		return (FAIL);

	return (SUCCESS);
}

/* --------------- WritePCX_BODY ---------------
 *  Write the pixel data.
 *  Returns SUCCESS or an error condition.
 * -------------------------------------------------------------------
 */
WORD WritePCX_BODY(WORD file, BITMAP *bitmap)
{
	register WORD y;

	/* --- Write to a bit-packed file. */
	for (y = 0;  y < bitmap->box.h;  ++y) {		/* for each line in band */
		if (PutBytes((BYTE far *)BMLineStart(bitmap, y, 0),
					 pbh->bytesperline, file))
			return FAIL;
	}
	return SUCCESS;
}

/*-------------------- Output any 256-color palette ------------------------*/
BOOL WritePCX_256Palette(WORD file, LONG *palette)
{
	register WORD i;
	UBYTE c, palet[256][3];

	if (pbh->version == 5 && pbh->bitsperpixel == 8) {
		for (i = 0; i < 256; ++i) {
			palet[i][0] = (BYTE)(palette[i] >> 16);
			palet[i][1] = (BYTE)(palette[i] >> 8);
			palet[i][2] = (BYTE) palette[i];
		}
		c = 0x0C;
		if (GWrite(file, &c, 1) != 1 ||
			GWrite(file, (char *)&palet[0][0], 768) != 768)
			return FAIL;
	}
	return SUCCESS;
}

/*-------------------------------------------------------------------
 *	Write bytes to a file, handling packing as it goes.
 *	Returns SUCCESS or FAIL.
 *-------------------------------------------------------------------
 */
BOOL PutBytes(BYTE far *ptr, WORD bytes, WORD file)
{
	register WORD startbyte, count;
	char b;

	while (bytes > 0) {
		/* --- check for a repeating byte value */
		startbyte = *ptr++;
		--bytes;
		count = 1;
		while (*ptr == startbyte && bytes > 0 && count < 63) {
			++ptr;
			--bytes;
			++count;
		}
		/* --- If we can pack the sequence, or if we have to add a
		 * byte before it because the top 2 bits of the value
		 * are 1's, write a packed sequence of 2 bytes.
		 * Otherwise, just write the byte value.
		 */
		if (count > 1  ||  (startbyte & 0xc0) == 0xc0) {
			b = 0xc0 | count;
			if (GWrite(file, &b, 1) != 1)
				return FAIL;
		}
		b = startbyte;
		if (GWrite(file, &b, 1) != 1)
			return FAIL;
	}
	return SUCCESS;
}
