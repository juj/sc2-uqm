/* 
	This is a small program to play a deluxe animate file.  No attempt has
	been made to time the animation speed correctly.  The playback speed will
	be dictated by the speed of your machine.
*/

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0

#define	TRUE	1
#define	FALSE	0

/* Typedefs for portability. */

typedef char 				BOOL;		/* 0==FALSE, 1==TRUE */
typedef char 				BYTE;		/* signed    8-bit number */
typedef unsigned char 	UBYTE;	/* unsigned  8-bit number */
typedef int 				WORD;		/* signed   16-bit number */
typedef unsigned int 	UWORD;	/* unsigned 16-bit number */
typedef long 				LONG;		/* signed   32-bit number */
typedef unsigned long 	ULONG;	/* unsigned 32-bit number */

/* function prototypes */
void SetVideoMode(UWORD mode);
WORD	GetVideoMode(void);
void LoadPalette(UWORD numcolors, UWORD startcolor, UBYTE *paletteaddr);
void InitMouse(void);
void ShowMouse(void);
void HideMouse(void);
void ReadMouse(UWORD *buttonstate, UWORD *mousex, UWORD *mousey);
void PlayRunSkipDump(UBYTE *srcbuf, UBYTE *dstbuf);
void cls(UWORD color);

#include "showanim.h"		/* contains structures for large page files */

lpfileheader lpheader;		/* file header will be loaded into this structure */

lp_descriptor LpArray[256]; /* arrays of large page structs used to find frames */

UBYTE lppalette[768];		/* anim file palette is loaded here */
UBYTE *screen = (char *)0xA0000000l;	/* pointer to the screen */

UWORD curlpnum=0xFFFF;		/* initialize to an invalid Large page number */

lp_descriptor curlp;			/* header of large page currently in memory */

UWORD *thepage;				/* pointer to the buffer where current large page is loaded */

FILE	*infile;					/* input file pointer */

ULONG	inputsize;				/* input file size.  I allways get this out of habbit */


/* support routines for the rest of the program start here */

/* given a frame number return the large page number it resides in */
UWORD findpage(framenumber)
UWORD framenumber;
{
	UWORD i;

	for(i=0; i<lpheader.nLps; i++)
	{
		if(LpArray[i].baseRecord <= framenumber && LpArray[i].baseRecord + LpArray[i].nRecords > framenumber)
			return(i);
	}
	return(i);
}

/* seek out and load in the large page specified */
loadpage(pagenumber, pagepointer)
UWORD pagenumber;
UWORD *pagepointer;
{
	if(curlpnum!=pagenumber)
	{
		curlpnum=pagenumber;
		fseek(infile, 0x0B00l+(LONG)pagenumber * 0x10000l,SEEK_SET);
		fread(&curlp, sizeof(lp_descriptor), 1, infile);
		getw(infile);	/* skip empty word */
		fread(pagepointer, curlp.nBytes+(curlp.nRecords*2), 1, infile);
	}
}

/* This version of the decompressor is here for portability to non PC's */
void CPlayRunSkipDump(srcP,dstP)
BYTE *srcP;		  /* srcP points at first sequence in Body */
BYTE *dstP;		  /* dstP points at pixel #0 on screen. 	*/
{
	BYTE cnt;
	UWORD wordCnt;
	UBYTE pixel;


nextOp:
		cnt = (signed char) *srcP++;
		if (cnt > 0)
			goto dump;
		if (cnt == 0)
			goto run;
		cnt -= 0x80;
		if (cnt == 0)
			goto longOp;
/* shortSkip */
		dstP += cnt;			/* adding 7-bit count to 32-bit pointer */
		goto nextOp;
dump:
		do
		{
			*dstP++ = *srcP++;
		} while (--cnt);

		dstP += cnt;
		srcP += cnt;
		goto nextOp;
run:
		wordCnt = (UBYTE)*srcP++;		/* 8-bit unsigned count */
		pixel = *srcP++;
		do
		{
			*dstP++ = pixel;
		} while (--wordCnt);

		dstP += wordCnt;
		goto nextOp;
longOp:
		wordCnt = *((UWORD far *)srcP)++;
		if ((WORD)wordCnt <= 0)
			goto notLongSkip;	/* Do SIGNED test. */

/* longSkip. */
		dstP += wordCnt;
		goto nextOp;

notLongSkip:
		if (wordCnt == 0)
			goto stop;
		wordCnt -= 0x8000;		/* Remove sign bit. */
		if (wordCnt >= 0x4000)
			goto longRun;

/* longDump. */
		do 
		{  
			*dstP++ = *srcP++;  
		} while (--wordCnt);

		dstP += wordCnt;
		srcP += wordCnt;
		goto nextOp;

longRun:
		wordCnt -= 0x4000;		/* Clear "longRun" bit. */
		pixel = *srcP++;
		do 
		{  
			*dstP++ = pixel; 
		} while (--wordCnt);

		dstP += wordCnt;
		goto nextOp;

stop:	/* all done */
	;
}



/* draw the frame sepcified from the large page in the buffer pointed to */
renderframe(framenumber, pagepointer)
UWORD framenumber;
UWORD *pagepointer;
{
	UWORD offset=0;
	UWORD i;
	UWORD destframe;
	UBYTE *ppointer;

	destframe = framenumber - curlp.baseRecord;
	
	for(i = 0; i < destframe; i++)
	{
		offset += pagepointer[i];
	}
	ppointer = (UBYTE *)pagepointer;

	ppointer+=curlp.nRecords*2+offset;
	
	if(ppointer[1])
	{
		ppointer += (4 + (((UWORD *)ppointer)[1] + (((UWORD *)ppointer)[1] & 1)));
	}
	else
	{
		ppointer+=4;
	}
	 
/*
	this is the C version of the decompressor, uncomment it if you are not
	running on a PC
*/
/*	CPlayRunSkipDump(ppointer, screen); */

	PlayRunSkipDump(ppointer, screen);
}

/* high level frame draw routine */
void drawframe(framenumber)
UWORD framenumber;
{
	loadpage(findpage(framenumber), thepage);
	renderframe(framenumber, thepage);
}

/* main program code */
main(argc,argv)
int argc;
char *argv[];
{
	UWORD i;
	UWORD framecount;
	WORD	oldvideomode;
	UWORD	mousebutton=0, mousex=0, mousey=0;

/* had to use "halloc" because I had to allocate a buffer exactly 64k long */
	thepage =(UWORD *)halloc(0x10000l,1); /* allocate page buffer */

	if(argc < 2)
	{
		printf("'Showanim file.ANM' display Deluxe animate file on VGA screen.\n\n");
		return EXIT_FAILURE;
	}
		
	if((infile = fopen(argv[1],"rb")) == NULL)
	{
		printf("Input file not found!\n");
		return EXIT_FAILURE;
	}

	/* get the size of the anim file */	
	fseek(infile, 0L, SEEK_END);
	inputsize = ftell(infile);
	rewind(infile);

	/* read the anim file header */
	if(fread(&lpheader,sizeof(lpfileheader),1,infile) == NULL)
	{
		printf("Error reading file header!\n");
		return EXIT_FAILURE;
	}

	fseek(infile, 128L, SEEK_CUR);	/* skip color cycling structures */
	
	/* read in the color palette */
	for(i=0; i<768; i+=3)
	{
		lppalette[i+2] = (UBYTE)((getc(infile) >> 2) & 63);		/* read red */
		lppalette[i+1] = (UBYTE)((getc(infile) >> 2) & 63);		/* read green */
		lppalette[i]   = (UBYTE)((getc(infile) >> 2) & 63);		/* read blue */

		getc(infile);								/* skip over the extra pad byte */
	}

	/* read in large page descriptors */
	fread(LpArray,sizeof(lp_descriptor),256,infile);

	/* the file pointer now points to the first large page structure */

	oldvideomode = GetVideoMode();	/* save the current video mode */

	SetVideoMode(0x13);				/* set the video mode to 13h MCGA 256 color */

	cls(0);								/* clear the screen to black */

	LoadPalette(256,0,lppalette);	/* init the color palette */

	InitMouse();

	HideMouse();

	drawframe(0);					/* draw the first frame of the file */

	ShowMouse();

	mousebutton=0;
/*
	Remember, the last frame in the file is a delta back to the first frame.
	You should only draw the first frame once and then use the last frame to 
	redraw the first frame.
*/
	while(!(mousebutton&1))
	{

		for(framecount = 1; framecount <	(UWORD)lpheader.nRecords; framecount++)
		{
			ReadMouse(&mousebutton, &mousex, &mousey);

			while(!(mousebutton&3))
				ReadMouse(&mousebutton, &mousex, &mousey);

			if(mousebutton&1)
				break;
			
			HideMouse();
			drawframe(framecount);
			ShowMouse();
		}
	}

	HideMouse();
	SetVideoMode(oldvideomode);
	fclose(infile);

	hfree(thepage);

	return EXIT_SUCCESS;
}


