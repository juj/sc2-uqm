/*--misc.c----------------------------------------------------------
 * Miscellaneous routines.
 */

#include "main.h"
#include "anim.h"

#define local static

extern BOOL EscFlag;
extern WORD curFrame;
extern BITMAP hidbm, prevFrame;

/* --------------- BugExit, EmergencyExit ---------------
 */
BugExit(char *s)
{
	printf("Program Bug: %s.\n", s);
	exit(1);
}

EmergencyExit(char *s)
{
	printf("%s.\n", s);
	exit(1);
}

NotEnoughMemExit()
{
	EmergencyExit("Not enough memory to run this program");
}

/* --------------- AbortKeyHit, CheckAbort ---------------
 */
#define	ESC		0x1b

local BOOL AbortKeyHit()
{
	register WORD i;

	while (char_avail()) {	/* Consume keys, looking for ESC. */
		i = get_char();
		if (i == ESC) {
			EscFlag = TRUE;	/* Remember ESC was hit. */
			return TRUE;
		}
	}
	return FALSE;	/* Abort key not seen. */
}

/* Once abort key has been hit, further checks see EscFlag. */
BOOL CheckAbort()
{
	return (EscFlag || AbortKeyHit());
}

/* ---------------  --------------- */
#define bitmap_offs(bm, x, y)	(x + (bm)->width * (y))
#define BytesNeeded(pix)		EVEN_UP(pix)		/* one byte-per-pixel */
	/* Must be whole # words. */

/* --------------- BMLineStart ---------------
 * Returns a far pointer to the start of line y
 * of the given plane in the bitmap.
 */

WORD far *BMLineStart(BITMAP *bm, WORD y, WORD plane)
{
	return (WORD_FAR_PTR(bm->seg[plane], bitmap_offs(bm, 0, y)));
}

/* --------------- init_bitmap ---------------
 */
init_bitmap(bm, width, height)
register BITMAP *bm;
WORD width, height;
{
	bm->width = BytesNeeded(width);
	bm->box.x = 0;
	bm->box.y = 0;
	bm->box.w = width;
	bm->box.h = height;
	bm->planes = curDepth;
}

/* --------------- NewBitMap ---------------
 * CAUTION: only call when have NOT allocated bm->seg[]'s.
 */
NewBitMap(BITMAP *bm, WORD width, WORD height)
{
	init_bitmap(bm, width, height);
	bm->seg[0] = DAlloc(N_BYTES_IN_BITMAP);
	if (!bm->seg[0])
		NotEnoughMemExit();
}

/* --------------- SetBitMap ---------------
 */
void SetBitMap(BITMAP *bm, WORD color)
{
	far_setmem(bm->seg[0], 0, N_BYTES_IN_BITMAP, color);
}

/* --------------- FirstAnimFrame ---------------
 * Switch to first frame for editing.
 */

/* Read into lpBuffer, but don't transfer to hidbm.
 * This gets all the pointers correctly set-up, so that hidbm can
 * become the new first frame (by calling AnimSetDirtyBox).
 */
FirstAnimFrame00() {
	curFrame = FIRST_FRAME_N;
	ReadLpfFrame(curFrame);
	}


FirstAnimFrame0() {
	/* --- Clear junk from prevFrame. */
	far_setmem(BSEG(&prevFrame), 0, N_BYTES_IN_BITMAP, white_color);

	FirstAnimFrame00();
	PlayDeltaFrame(&hidbm);
	}

/* --------------- CopyHidbmToPrevFrame ---------------
 */
local void CopyHidbmToPrevFrame(void) {
	far_movmem( BSEG(&hidbm), 0,
				BSEG(&prevFrame), 0,  N_BYTES_IN_BITMAP );
	}

/* --------------- NextAnimFrame0 ---------------
 * Switch to next frame for editing.
 */
NextAnimFrame0() {
	curFrame++;

	if (curFrame > LAST_FRAME_N)
		FirstAnimFrame0();
	else {
		/* (old) current frame is (next) prevFrame.
		 * Don't use PlayDeltaFrame, because didn't keep Delta in RAM. */
		CopyHidbmToPrevFrame();
		ReadLpfFrame(curFrame);
		PlayDeltaFrame(&hidbm);
			/* Convert old current frame to new current frame. */
	}
}

/* -------------------------------------------------------------------------
 *		Disk i/o with critical error detection.
 * -------------------------------------------------------------------------
 */
/* returns -1 if it can't read the full number of bytes */
int readfull(int file, char *buffer, unsigned int count) {
	return (readdos(file,dataseg(),buffer,count) == count) ? 0: -1;
	}

/* returns -1 if it can't read the full number of bytes */
int readdosfull(UWORD file, UWORD seg, UWORD buffer, UWORD count) {
	return (readdos(file,seg,buffer,count) == count) ? 0: -1;
	}

/* returns -1 if it can't write the full number of bytes */
int writefull(int file, char *buffer, unsigned int count) {
	return (writedos(file,dataseg(),buffer,count) == count) ? 0: -1;
	}

/* returns -1 if it can't write the full number of bytes */
int writedosfull(UWORD file, UWORD seg, UWORD buffer, UWORD count) {
	return (writedos(file,seg,buffer,count) == count) ? 0: -1;
	}

long lseekdos(int file, long offset, int origin) {
	long result = lseek(file,offset,origin);
	return result;
	}

/*-----------------------------------------------------------------
 * --------------- do_num_to_string -------------------------------
 * Convert a number to a string.  Parameters:
 *	UWORD num = # to convert to string.
 *	char *string = Ptr to string where you want the #.
 *	UWORD length = # of chars you want in the resulting string.  Set to -1
 *		if you don't require any specific string length.  If the string
 *		is too short, it will be padded with blanks on the left.  If it
 *		is too long, it will be chopped off on the right.
 *	BOOL negative = Set to TRUE if you want numbers > 32767 to be printed
 *		as negative (with a leading "-"), or FALSE otherwise.
 */

/* TRUE before calling do_num_to_string, if want padding with 0's
 * instead of blanks.  Must set back to FALSE when done, so
 * other clients will get ' 's.
 */
local BOOL padWithZeroes = FALSE;

void do_num_to_string(UWORD num, char *string, UWORD length, BOOL negative)
{
	UWORD temp;
	register UWORD index, i;
	BOOL start;

	index = 0;

	/* --- does caller want negative # printed with leading '-'? */
	if (negative && num >= 0x8000) {
		if (index >= length)   goto terminate;
		num = -num;
		string[index++] = '-';
	}

	/* --- convert the # to a string */
	if (num == 0) {
		if (index >= length)   goto terminate;
		string[index++] = '0';
	} else {
		start = FALSE;
		for (i = 10000; i > 0; i /= 10) {
			temp = (num / i);
			if (temp || start) {
				/* --- if string has reached max length, stop */
				if (index >= length)   goto terminate;
				string[index++] = temp + '0';
				start = TRUE;
			}
			num %= i;
		}
	}

	/* --- terminate the string */
terminate:
	string[index] = 0;

	/* --- if caller wants string to be at least a certain length, do it */
	i = strlen(string);
	if ((WORD)i < (WORD)length) {
		/* --- string is too short, so add spaces to left */
		while ((WORD)i < (WORD)length) {
			movmem(string, string + 1, i + 1);
			string[0] = (padWithZeroes ?  '0' :  ' ');
			++i;
		}
	}
}

/* --------------- num_to_string_zeroes ---------------
 * Specified length, no negative, padding with zeroes.
 */
void num_to_string_zeroes(UWORD num, char *string, UWORD length) {
	padWithZeroes = TRUE;
	do_num_to_string(num, string, length, FALSE);
	padWithZeroes = FALSE;
}
