/*----------------------------------------------------------------------*/
/*																		*/
/*		    animio.c  -  Page-flip animation-- file io code.			*/
/*																		*/
/*----------------------------------------------------------------------*/


#include "main.h"
#include "anim.h"		/* FIRST_FRAME_N */
#include "dpiff.h"
#include "dialogmg.h"	/* for LIST definition */
#include "string.h"
#include <sys\types.h>
#include <sys\stat.h>

extern SHORT curDepth;
extern UWORD *ytable;
extern BITMAP hidbm;
extern AnimOb curAnimBr;
extern SHORT curfps, nPages, curPage;
extern animProcHandle *animOps;

#define local static

BOOL isAnimBrIO = NO;

#if 0
SHORT saveFrameNum;
SHORT saveStart, nSave, loadStart = 0;

#endif

typedef struct IntuiMessage IMsg;


DPAnimChunk dpAnimChnk;

#if 0
#define AnimOp(N) (*animOps[N])()
#endif

FreeDelta(register Delta * del)
{
	register int p;

	for (p = 0; p < curDepth; p++) {
		FP_SEG(del->plane[p]) = SFree(FP_SEG(del->plane[p]));
	}
}


/*--------------------------------------------------------------*/
/* Read In ANIM iff files										*/
/*--------------------------------------------------------------*/


#ifdef DBGDLTA
BOOL dbgdlta = YES;

#endif


/*--------------------------------------------------------------*/
/*- Read in a DPAN chunk 							 -----------*/
/*--------------------------------------------------------------*/
IFFP GetDPAnimChunk(context)
GroupContext *context;
{
	IFFP iffp = IFFReadBytes(
			  context, (BYTE *) & dpAnimChnk, (LONG) sizeof(DPAnimChunk));

	ByteFlipAnimChunk(&dpAnimChnk);
#ifdef DBGDLTA
	if (dbgdlta) {
		printf(" GetDPAnimChunk , vers = %d, nframes = %d, flags = %d \n",
			   dpAnimChnk.version, dpAnimChnk.nframes, dpAnimChnk.flags);
	}
#endif
	return (iffp);
}


/*--------------------------------------------------------------*/
/*- Write out a DPAN chunk 							 -----------*/
/*--------------------------------------------------------------*/
IFFP PutDPAN(context)
GroupContext *context;
{
	DPAnimChunk dpAnCh;

	setmem(&dpAnCh, sizeof(DPAnimChunk), 0);
	dpAnCh.version = DPANIM_VERS3;
	if (isAnimBrIO) {
		dpAnCh.nframes = curAnimBr.nCels;
		dpAnCh.duration = curAnimBr.duration;
		dpAnCh.flags = curAnimBr.flags;
		dpAnCh.rate = 0;
	}
#if 0	/* Not supporting anims, just animbrushes. */
	else {
		dpAnCh.nframes = nSave;
		dpAnCh.rate = curfps;
	}
#endif
	ByteFlipAnimChunk(&dpAnCh);
	return
		PutCk(context, ID_DPAN, (LONG) sizeof(DPAnimChunk), (BYTE *) & dpAnCh);
	/* NOTE: needn't flip back to IBM byte-order, since not using further. */
}

#if ANIMDISK
/*--------------------------------------------------------------*/
/* Anim file IO													*/
/*--------------------------------------------------------------*/
typedef char FileName[FILENAME_LENGTH + 1];
extern WORD curFrame;

/*---------------------------------------------------------------*/
/* Saving anim as separate ILBM's 								 */
/*                                                               */
/* Assumes <name> contains the desired file extension and that   */
/* 1 <= start <= animFrames, 1 <= end <= animFrames              */
/*---------------------------------------------------------------*/
void SaveAnimAsILBMs(name, start, end)
char *name;
SHORT start, end;
{
	auto int i, len;
	auto int nframes = end - start + 1;
	FileName nm, bsname;
	auto char nstr[SEQUENCE_SUFFIX_LENGTH], ext[FILENAME_SUFFIX_LENGTH + 2];
	auto struct stat filestat;
	auto char *ptr;
	auto BOOL overwrite = NO;

		/* seperate out the file extension, if any */
	strcpy(bsname, name);
	ptr = strchr(bsname, '.');
	if (ptr) {
		strncpy(ext, ptr, sizeof(ext));
		ext[sizeof(ext) - 1] = *ptr = '\0';
	} else
		ext[0] = '\0';

	len = strlen(bsname);
	if (!len) {
		NotAcceptableFileNameMsg();
		return;
	}
	if ((len + SEQUENCE_SUFFIX_LENGTH) > FILENAME_ROOT_LENGTH) {
		BaseNameTooLongMsg();
		return;
	}
	AnimGoTo(start);
	for (i = 0; i < nframes; i++) {
		strcpy(nm, bsname);
		num_to_string_zeroes(curFrame, nstr, SEQUENCE_SUFFIX_LENGTH);
		strcat(nm, nstr);
		strcat(nm, ext);
		if (stat(nm, &filestat) != -1) {
			if (filestat.st_mode & S_IFDIR) {
				CantReplaceDirMsg();
				break;
			} else if (!(filestat.st_mode & S_IWRITE)) {
				FileIsWriteProtectedMsg();
				break;
			} else if (!overwrite) {
				if (!ReplaceSequence(nm))
					break;
				else
					overwrite = YES;
			}
		}
		if (!save_picture0(nm))
			break;
		AnimStepPage(1);
		if (CheckAbort()) {
			ClearAbortFlag();
			break;
		}
	}
}

#endif	/* ANIMDISK */


/*--------------------------------------------------------------*/
/* Loading ILBMs as Anim 										*/
/* tries to load a sequence of ilbm files (bname...bname+num-1) */
/* as a new anim with <num> frames.								*/
/*--------------------------------------------------------------*/
extern char **filename_strings;
extern char pict_dir[], dir_mark[];

LoadILBMsAsAnim(char *bname, SHORT num, BOOL insert, char *pictExtension)
{
	int i, starti, endi, off;
	char *fname;
	LIST list;

#if 1	/* Prepare filenames */
	set_full_directory(pict_dir);
	setmem(&list, sizeof(LIST), 0);
	if (!AllocFilenameStrings())
		goto bail;
	NewNames0(&list, pictExtension, FALSE);
#endif

	off = strlen(dir_mark);
	starti = -1;
	for (i = 0; i < list.num_items; i++) {
		fname = filename_strings[i] + off;
		if (strcmp(fname, bname) == 0) {
			starti = i;
			break;
		}
	}
	if (starti == -1)
		goto bail;
	endi = starti + num - 1;
	endi = MIN(endi, list.num_items - 1);
	num = endi - starti + 1;

	if (insert) {
		/* insert pages after curFrame */
		if (AddPages(num) == FAIL)
			return;
		AnimStepPage(1);		/* go to first newly added frame */
	} else {
		FreeStrings(&list);		/* Make room for NewAnim/SaveAnim dialog. */
		FreeFilenameStrings();
		/* create a new untitled anim with the appropriate # of frames */
		if (NewAnim00(TRUE, num, TRUE) == FAIL)
			return;				/* couldn't create new anim or user aborted */
#if 1	/* Prepare filenames */
		set_full_directory(pict_dir);
		setmem(&list, sizeof(LIST), 0);
		if (!AllocFilenameStrings())
			goto bail;
		NewNames0(&list, pictExtension, FALSE);
#endif
	}

	HideControls();
	ZZZCursor();
	for (i = starti; i <= endi; i++) {
		fname = filename_strings[i];
		if (fname[0] == dir_mark[0]) {
			DelPages(1);		/* But suppose many pages left unfilled? */
			break;				/* TBD: want to continue to find more
								 * dir's? */
		}
		fname += off;

		if (!load_picture00(fname, TRUE, TRUE))
			break;

#if comingSoon
		...remap picture(avoid doing on first picture, when getting
						 palette from first picture ?)
			CopyPaletteFromFile(curpal, oldPaletteFileLetter);
#endif

		if (CheckAbort()) {
			ClearAbortFlag();
			break;
		}
		AnimStepPage(1);
	}
	AnimGoTo(FIRST_FRAME_N);
	UnZZZCursor();
	ShowControls();
bail:
	FreeStrings(&list);
	FreeFilenameStrings();
}


/* --------------- Set/Clear AnBrIO ---------------
 */
SetAnBrIO()
{
	isAnimBrIO = YES;
}

ClearAnBrIO()
{
	isAnimBrIO = NO;
}


/*--------------------------------------------------------------*/
/* Anim IFF IO routines											*/
/*--------------------------------------------------------------*/

#if 0
SHORT animLoadState;

local void SetAnimLoadFail()
{
	animLoadState = FAIL;
}

#endif	/* 0 */


#define IFF_MEMFAIL -10

IFFP AnimGetDLTA(context, bmHdr, animHdr, frameCnt, buffer, bufsize)
GroupContext *context;
BitMapHeader *bmHdr;
AnimHdr *animHdr;
SHORT frameCnt;
BYTE *buffer;
LONG bufsize;
{
	IFFP iffp;
	BITMAP *tmp;
	int res;

	if (isAnimBrIO)
		return (GetAnBrDLTA(context, bmHdr, animHdr, frameCnt, buffer, bufsize));
#if 1
	BugExit("AN1");
			/* "IFF format for Anim only implemented for brush so far"); */
#else	/* screen anim */
	if (frameCnt == 1) {
		animLoadState = SUCCESS;
		if (loadStart > 0) {
#if 0	/* TBD !!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */
			if (AnimOp(ANIM_SAVECHGS) == FAIL)
				return (IFF_MEMFAIL);
#endif
		} else
			AnimClearDirty();
		if (animHdr->interleave != 0 && animHdr->interleave != 2) {
			NotValidAnimMsg();
#if 0	/* TBD: Not moved over from Amiga.  1/30/90 */
			SkipErrorMsg();		/* tell dpio not to put another error req.
								 * up */
#endif
			return (BAD_FORM);
		}
#if 0	/* TBD: Not moved over from Amiga.  1/30/90 */
		ShowLoadedPalette();
#endif
	}
	if (CheckAbort()) {
		ClearAbortFlag();
		return (IFF_DONE);
	}
	if (dpAnimChnk.version > DPANIM_VERS1) {
		if (frameCnt >= dpAnimChnk.nframes) {
			/* Skip remaining deltas: they are for circular play */
#if 0	/* TBD: Not moved over from Amiga.  1/30/90 */
			/* TBD: Not an error !!!!!! */
			SkipErrorMsg();		/* tell dpio not to put an error req. up */
#endif
			return (IFF_DONE);
		}
	}
#if 0	/* TBD !!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */
	res = (*animOps[ANIM_GETDLTA])(context, bmHdr, animHdr, frameCnt, buffer, bufsize);
#endif	/* 0 */

	return (res);
#endif	/* screen anim */
}


IFFP AnimPutDLTAs(context, buffer, bufsize)
GroupContext *context;
BYTE *buffer;
LONG bufsize;
{
	if (isAnimBrIO)
		return (PutAnBrDLTAs(context, buffer, bufsize));
#if 1
	else
		return CLIENT_ERROR;	/* Not ready to handle Anim yet. */
#else
	else
		return ((*animOps[ANIM_PUTDLTAS])(context, buffer, bufsize));
#endif	/* 0 */
}


#if 0
IFFP AnimIFFDone()
{
	SHORT res = IFF_DONE;

	if (isAnimBrIO)
		return (AnBrIFFDone());
	else {
		if (dpAnimChnk.rate > 0)
			SetRateFPS(dpAnimChnk.rate);
		res = (*animOps[ANIM_IFF_DONE])();
		if (animLoadState == FAIL)
			AnimLoadFailMsg();
		return (res);
	}
}

#endif	/* 0 */


#if 0
/*------------------------------------------------------------*/
/* assume the bitmap the delta applies to is in dbm: read the */
/*   Delta and apply it to dbm								  */
/*------------------------------------------------------------*/
IFFP ReadAndApplyDelta(context, bmHdr, animHdr, dbm, buffer, bufsize)
GroupContext *context;
BitMapHeader *bmHdr;
AnimHdr *animHdr;
BITMAP *dbm;
UBYTE *buffer;
ULONG bufsize;
{
	int i, op, bpr;
	ULONG iffp = 0;
	ULONG ploffs[16];
	ULONG planeSize[8], plast, plOffsSize;
	SHORT depth = bmHdr->nPlanes;

	op = animHdr->operation;
	if (op != ANIM_SHORTD && op != ANIM_RIFF && op != ANIM_RUNSKIP) {
		NotValidAnimMsg();
		return (IFF_DONE);
	}
	plOffsSize = (op == ANIM_SHORTD ? 32 : 64);

		/*--------------------------------------------------------
		 * NOTE: these ploffs are from the beginning of the chunk:
		 * the first one should have a value of 32 for short delta, 64 for riff,
		 * ?4? for ANIM_RUNSKIP.
		 */
	iffp = IFFReadBytes(context, (BYTE *) ploffs, plOffsSize);
	CheckIFFP();

#ifdef DBGDLTA
	if (dbgdlta) {
		printf("  planeOffsets = ");
		for (i = 0; i < 8; i++)
			printf(" %d | ", ploffs[i]);
		printf("\n");
	}
#endif

	/* Compute Size of the compressed planes */
	/* NOTE: ploffs[i] = 0 means no changes for that plane */

	plast = ChunkMoreBytes(context) + plOffsSize;
	setmem(planeSize, 8 * sizeof(LONG), 0);
	for (i = depth - 1; i >= 0; i--) {
		if (ploffs[i]) {
			planeSize[i] = plast - ploffs[i];
			plast = ploffs[i];
		}
	}
#ifdef DBGDLTA
	if (dbgdlta) {
		printf("  planeSize = ");
		for (i = 0; i < 8; i++)
			printf(" %d | ", planeSize[i]);
		printf("\n");
	}
#endif


	bpr = dbm->width;
	for (i = 0; i < depth; i++) {
		if (planeSize[i] != 0) {
			/* FOR NOW, ONLY READ PLANE IF IT ALL FITS INTO BUFFER */
			if (planeSize[i] > bufsize) {
#ifdef DBGDLTA
				InfoMessage("ReadAndApplyDLTA: plane too big", "for buffer");	/* ENGLISH */
#endif
				continue;
			}
			iffp = IFFReadBytes(context, buffer, planeSize[i]);
			if (iffp != IFF_OKAY) {
#ifdef DBGDLTA
				InfoMessage(" ReadBytes error", " in ReadAndApplyDLTA");		/* ENGLISH */
#endif
				return (iffp);
			}
			if (i < dbm->planes)
				switch (op) {
				  case ANIM_SHORTD:
					DecodeDLTA(dbm->seg[i], buffer);
					break;
				  case ANIM_RIFF:
					decode_vkplane(buffer, dbm->seg[i], bpr);
					break;
				}
		}
	}
	return (IFF_OKAY);
}

#endif	/* 0 */


#if 0
/*--------------------------------------------------------------*/
/* This Computes the RIFF delta between two bitmaps, abm and  	*/
/* bbm, and outputs it to the IFF stream 						*/
/*--------------------------------------------------------------*/

IFFP OutputRiffDelta(context, anhdr, abm, bbm, delbm)
GroupContext *context;
AnimHdr *anhdr;
BITMAP *abm, *bbm;		/* two bitmaps to be compared */
BITMAP *delbm;	/* contains the resulting delta planes */
{
	ULONG sizes[8];
	IFFP ifferror = 0;
	int i;
	SHORT bpr = abm->width;

	for (i = 0; i < abm->planes; i++)
		sizes[i] = MkRIFF(abm->seg[i], bbm->seg[i], delbm->seg[i],
						  abm->Rows, bpr, bpr);
	ifferror = WriteDelta(context, anhdr, delbm->seg, sizes);
	return (ifferror);
}

#endif	/* 0 */
