/*--anim2pcx.c----------------------------------------------------------
 *
 *	Program to translate an anim into a series of PCX picture files,
 *  and back from picture files into an anim.
 *  Note: all pictures are assumed to have the same 256 palette colors.
 *
 *  This code is intended to be used as a starting point for programmers
 *  needing to convert anims to/from other formats, or to read/write
 *  anim files from their own applications.
 *
 * The source code for ANIM2PCX may be freely incorporated into
 * commercial programs.
 * 
 * Disclaimer:
 *   THIS PROGRAM IS PROVIDED WITHOUT SEPARATE CHARGE AND IS
 *   PROVIDED AS IS.  ELECTRONIC ARTS DISCLAIMS ALL WARRANTIES,
 *   INCLUDING IMPLIED WARRANTIES AND WARRANTIES OF
 *   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 *
 * *** History *********
 * Date      Who Changes
 * --------- --- ---------------------------------------------------
 * 24-Oct-90 ss  Original release.
 *------------------------------------------------------------------
 * Note: all files editted with tab stops every 4 spaces.
 */

#include "main.h"
#include "anim.h"

#include <fcntl.h>
#include <sys\types.h>
#include <sys\stat.h>
#include <io.h>
#include <stdio.h>

#define local static

extern char lpfSuffix[];
local char pcxSuffix[]= "PCX";

BOOL FileError = FALSE;	/* error reading or writing a file */
BOOL EscFlag = FALSE;	/* set TRUE when Esc key detected */

LONG curpal[MAX_COLORS];	/* palette */

/*-----------------------------------------------------------------*/

/* --- array of bit masks for one byte */
BYTE bit_masks[8] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};

BOOL toAnim;
UWORD animFrames = 10;
WORD curFrame = FIRST_FRAME_N;	/* Frame currently in hidbm. */
char animName[FILENAME_ROOT_LENGTH], pictRoot[FILENAME_ROOT_LENGTH];

BITMAP hidbm, prevFrame;	/* Bitmap describing current & previous frames.*/
UWORD lpSeg = NULL;		/* Segment of lp buffer. */
UWORD allocSeg = NULL;	/* Segment allocated for delta buffer. */

/*-----------------------------------------------------------------*/
/* forward procedure definitions */


/* --------------- main ---------------
 */
main(argc, argv)
WORD argc;
char *argv[];
{
	WORD result;
	BITMAP bitmap;

	/* ----------------------------------------
	 * Analyze arguments.
	 */
	switch (argc) {
	  case 1+1:		/* anim => pcx */
		if (strlen(argv[1]) > FILENAME_ROOT_LENGTH)
			goto usage;
		strcpy(animName, argv[1]);
		toAnim = FALSE;
		break;
	  case 2+1:		/* pcx => anim */
		if (strcmp(argv[1],"join") != SUCCESS  &&
			strcmp(argv[1],"j") != SUCCESS)
			goto usage;
		if (strlen(argv[2]) > FILENAME_ROOT_LENGTH)
			goto usage;
		strcpy(animName, argv[2]);
		toAnim = TRUE;
		break;
	  default:		/* Bad arguments. */
usage:
#define PR(s)	printf(s)
 PR("\nUsage:\n");
 PR("  \"anim2pcx name\"       to split anim name.ANM into a group of\n");
 PR("                          picture files name0001.PCX, name0002.PCX...\n");
 PR("  \"anim2pcx j name\"     or\n");
 PR("  \"anim2pcx join name\"  to join a group of picture files\n");
 PR("                          name0001.PCX, name0002.PCX, into an anim.\n");
 PR("\n");
 PR("NOTE: Only the first four letters of \"name\" are used \n");
 PR("in the picture file names.\n");
 PR("  \"anim2pcx foobar\"     splits FOOBAR.ANM into FOOB0001.PCX...\n");
 PR("  \"anim2pcx j foobar\"   joins FOOB0001.PCX... into FOOBAR.ANM.\n");
		exit(1);
	}

	/* ----------------------------------------
	 * Prepare for conversion.
	 */

	/* --- Pict name's suffix is first four characters of anim name.
	 * Note: pictRoot may be shorter than four characters.
	 */
	strcpy(pictRoot, animName);
	pictRoot[SEQUENCE_ROOT_LENGTH] = NULL;

	NewBitMap(&hidbm, N_COLUMNS, N_LINES);
	NewBitMap(&prevFrame, N_COLUMNS, N_LINES);
	allocSeg = DAlloc(MAX_LARGE_PAGE_SIZE);
	lpSeg = DAlloc(MAX_LARGE_PAGE_SIZE);
	if (!allocSeg || !lpSeg)
		NotEnoughMemExit();

	if (toAnim)
		result = PCXFilesToAnimFile();
	else
		result = AnimFileToPCXFiles();

	/* ASSUME DAlloc'd memory automatically freed. */

	if (EscFlag == TRUE)
		EmergencyExit("Terminated at user's request");
	if (FileError)
		EmergencyExit("File error");
	if (result != SUCCESS)
		EmergencyExit("Failed to complete operation");
	printf("File(s) successfully converted.\n");
}

/* --------------- save_picture0 ---------------
 */
UWORD save_picture0(char *filename)
{
	UWORD result, file;

	file = CreateFile(filename, pcxSuffix);
	if (!file)   return FAIL;

	OpenIOBuffer(TRUE);
	result = WritePCX_Header(file, &hidbm, curpal);
	if (result != SUCCESS)   goto done;
	result = WritePCX_BODY(file, &hidbm);
	if (result != SUCCESS)   goto done;
	result = WritePCX_256Palette(file, curpal);

done:
	WritePCX_Done();
	CloseIOBuffer(file);
	closedos(file);
	return result;
}

/* --------------- AnimFileToPCXFiles ---------------
 * ASSUMES "animName" already set to the root part of the animation file's
 * name.  (Note: don't include ".ANM" in "animName".)
 */
WORD AnimFileToPCXFiles()
{
	WORD i, len, result = FAIL;
	char nm[FILENAME_LENGTH+1];
	char nstr[SEQUENCE_DIGITS_LENGTH+1];

	if (OpenLpFile() == FAIL) {
		printf("File \"%s.%s\" not found.\n", animName, lpfSuffix);
		exit(1);
	}

	printf("Creating %d PCX files from \"%s.%s\".\n",
		   animFrames, animName, lpfSuffix);

	FirstAnimFrame0();
	for (i = 0;  i < animFrames;  i++) {
		strcpy(nm, pictRoot);
		num_to_string_zeroes(curFrame, nstr, SEQUENCE_DIGITS_LENGTH);
		strcat(nm, nstr);
		if (save_picture0(nm) == FAIL)
			goto done;	/* TBD: message to user? */
		printf(".");	/* Inform user of progress. */
		NextAnimFrame0();
		if (CheckAbort())
			goto done;
	}
	printf("\n");	/* Inform user of progress. */
	result = SUCCESS;

done:
	return result;
}

/* --------------- load_picture0 ---------------
 */
UWORD load_picture0(char *filename)
{
	UWORD result, file;
	BitMapHeader bmHdr;

	file = OpenFile(filename, pcxSuffix);
	if (!file)   return FAIL;

	OpenIOBuffer(FALSE);
	result = ReadPCX_Header(file, &hidbm, curpal, &bmHdr);
	if (result != SUCCESS)   goto done;
	result = ReadPCX_BODY(file, &hidbm, &bmHdr);

done:
	ReadPCX_Done();
	CloseIOBuffer(file);
	closedos(file);
	return result;
}

/* --------------- PCXFilesToAnimFile ---------------
 * ASSUMES "animName" already set to the root part of the animation file's
 * name.  (Note: don't include ".ANM" in "animName".)
 */
WORD PCXFilesToAnimFile()
{
	WORD i, file, len, result = FAIL;
	char nm[FILENAME_LENGTH+1];
	char nstr[SEQUENCE_DIGITS_LENGTH+1];

	for (i = 0;  i < MAX_SUPPORTED_FRAMES;  i++) {
		strcpy(nm, pictRoot);
		num_to_string_zeroes(i+FIRST_FRAME_N, nstr, SEQUENCE_DIGITS_LENGTH);
		strcat(nm, nstr);
		/* --- Verify file exists, then inform user of operation.*/
		file = OpenFile(nm, pcxSuffix);
		if (!file) {
			if (i == 0) {
				printf("File \"%s.%s\" not found.\n", nm, pcxSuffix);
				exit(1);
			} else
				break;	/* Exit "for i" loop that was adding frames. */
		}
		CloseFile(file);
		if (i == 0) {
			CreateLpFile();
			FirstAnimFrame0();
			printf("Creating \"%s.%s\" from PCX files.\n",
			   animName, lpfSuffix);
		} else {
			AddFrame();		/* And make that frame current. */
			far_movmem(BSEG(&hidbm), 0, BSEG(&prevFrame), 0,
						N_BYTES_IN_BITMAP);
				/* New frame's "previous frame" has identical contents. */
		}
		if (load_picture0(nm) == FAIL)
			break;		/* No more pictures to load.  NOT an error,
						 * TBD: but should delete last added frame! */
		printf(".");	/* Inform user of progress. */
		WriteFinalAnimFrame();
		if (CheckAbort()) {
			result = SUCCESS;	/* So anim so far will be kept. */
			i++;	/* So correct notion of frame count. */
			goto done;
		}
	}

	/* Create last-to-first delta.
	 * First create it as a new frame at the end of the anim,
	 * then remove it from the frame count and mark the anim
	 * as containing a last-to-first delta.
	 */
	strcpy(nm, pictRoot);
	num_to_string_zeroes(FIRST_FRAME_N, nstr, SEQUENCE_DIGITS_LENGTH);
	strcat(nm, nstr);
	AddFrame();		/* And make that frame current. */
	far_movmem(BSEG(&hidbm), 0, BSEG(&prevFrame), 0,
				N_BYTES_IN_BITMAP);
	load_picture0(nm);
	WriteFinalAnimFrame();
	ConvertFinalFrameToLastDelta();

	result = SUCCESS;

done:
	WriteHdrAndCloseLpFile();
	if (result == SUCCESS)
		printf("\nAnim has %d frames.\n", i);	/* Inform user of progress. */
	else
		DeleteFile(animName, lpfSuffix);
	return result;
}
