/*--lpfile.c----------------------------------------------------------
 *
 * Large Page File.  This is a format for dividing a file into 64 KB
 * "large pages" ("lp"s) that can be stored out-of-sequence in the file.
 * That is, an lp can be logically inserted into the file, without
 * having to move all the following data out of the way.
 *
 * Each lp holds one or more "records".  A record is of any length from
 * 0 to almost 64 KB.  The records in the file are sequentially numbered
 * starting with 0.  The records in each lp are a sub-sequence; e.g.,
 * "3 records starting with #6" would be #6,#7,#8.
 * 
 * A DeluxePaint Animation "ANM" ("Anim") file is built on this mechanism,
 * so that as frames change in size, they can be maintained with a minimum
 * of extra file i/o, yet without loss of playback performance.
 * In addition, there is an optional special record which is the delta from
 * the last frame to the first frame, for smooth playback of looping anims.
 */

#include "main.h"
#include "anim.h"
#include <fcntl.h>
#include <stdio.h>
#include <io.h>	/* chsize */

#define local static

extern BOOL animDisk, outputLargePageFile, editting, cycling;
extern WORD curFrame;
extern UWORD animFrames, frameDelay, startFrame, endFrame, waitVerts,
      gapLimitAddr;
extern Box screenBox, animBox, animMove;
extern BITMAP prevFrame, hidbm;

extern LONG curpal[MAX_COLORS];
extern void AnimVBlank();

#if U_COLORCYCLE
extern Range cycles[MAXNCYCS];

#endif

/* --------------- forward declarations --------------- */
UWORD DAllocBiggest(UWORD nBytes, UWORD * allocSizeP);
void ReadLargePage(UWORD nLp);
WORD WriteCurLp(void);
void ReadCurLp(void);
UWORD FindLargePageForRecord(register UWORD nRecord);
void WriteAnimFrameAndReadNext(void);

/* --------------- --------------- */

WORD lpFile = NULL;		/* Keep open so can access again. */
char lpfSuffix[]= "ANM";

local Range cycles[MAXNCYCS];

	/* Disk i/o sets after filling buffer, screen i/o clears after using. */
UWORD nBuffers = 2;
UWORD useBuffers = 2;


extern char animName[];

local UWORD saveBitmapSeg = NULL;
#define prevFrameSeg	BSEG(&prevFrame)
#define hidbmSeg		BSEG(&hidbm)

/* --------------------------------------------------------------------------
 * --------------- Description of "current record". ---------------
 */
typedef struct {
	UWORD curRecord;	/* record being accessed w/i lp. NOTE: ABSOLUTE rec
						 * #, not relative to lp baseRecord. */
	UWORD recAddr;		/* Address of first byte of record, w/i lp buffer. */
	UWORD recBytes;		/* # bytes in record. */
	UWORD bodyAddr;		/* Address of Body of current frame. */
	UWORD bodyBytes;	/* # bytes in Bitmap record's Body. */
	UWORD extraBytes;	/* # bytes in Bitmap record prior to Body.
						 * ID+FLAGS & ExtraData, incl. count & pad.
						 * Used to skip over ExtraData. */
	UWORD extraDataCnt;	/* # bytes of contents of ExtraData. */
} BmRecX;	/* Access to a Bitmap record. */

local BmRecX curRecX;	/* Description of "current record". */

#define CUR_REC		curRecX.curRecord

/* --- These should be "CUR_REC_...", but the names get unreasonably long. */
#define REC_ADDR	curRecX.recAddr
#define REC_BYTES	curRecX.recBytes
#define BODY_ADDR	curRecX.bodyAddr
#define BODY_BYTES	curRecX.bodyBytes
#define EXTRA_BYTES	curRecX.extraBytes
#define EXTRA_DATA_CNT curRecX.extraDataCnt

#define DEFAULT_BITMAP_ID_FLAGS	((0<<8) + 'B')
	/* First byte is 'B'.  On 8086 that's low byte. */

/* ----- Functions to manipulate "current record" w/i CUR_LP w/i lp buffer. */
/* TBD: we're not actually using these, but doing the logic where needed. */

/* Shift data in buffer so there is room for "n" extraBytes, and set
 * EXTRA_BYTES.  NOTE: this is NOT "extraDataCount" -- it includes
 * ID+FLAGS & extraDataCount in its size.
 */
local void CurRec_SetExtraBytes(UWORD n);

/* Shift data in buffer so there is room for "n" bodyBytes, and set
 * BODY_BYTES.
 */
local void CurRec_SetBodyBytes(UWORD n);

/* Find & set CUR_REC.  NOTE: "n" is absolute, not relative to baseRecord.
 * This will put new lp in lp buffer if necessary.
 */
local void CurRec_Set(UWORD n);
UWORD NRecord2NFrame(WORD nRecord);

/* --------------------------------------------------------------------------
 * --------------- buffering large pages ---------------
 */

local UWORD preBufferSeg = NULL;
local UWORD preFrameAddr0 = 0;

local UWORD nBuf, nNextBuf = 0, nNextPreBuf = 0, nPreLp = 0, nPreRecord;
local BOOL lastRecordInBuf, curLpIsDirty = FALSE;

	/* "lastRecordInBuf is not used during editing, so is not part of
	 *    curRecX.
	 */
#define LAST_RECORD_IN_BUF	(LP_BASE_REC + LP_N_RECS - 1)

/* -------------------- LargePage -------------------- */
#define MAX_LARGE_PAGE	256
typedef struct {
	UWORD baseRecord;	/* First record in lp. */
	UWORD nRecords;		/* # records in lp. */
	   /* bit 15 of "nRecords" == "has continuation from previous lp".
	    * bit 14 of "nRecords" == "final record continues on next lp".
	    * File format thus permits 16383 records/lp.
		* Only MAX_RECORDS_PER_LP is currently supported by the code,
		* to minimize table size in DGROUP in DP Animation.
	    * TBD: we're not handling non-zero bits 14&15.
	    */
	UWORD nBytes;	/* # bytes of contents, excluding header.
					 * Gap of "64KB - nBytesUsed - headerSize" at end of lp.
					 * headerSize == "8 + 2*nRecords".
					 * Header is followed by record #baseRecord. */
} LP_DESCRIPTOR;
local LP_DESCRIPTOR lpfTable[MAX_LARGE_PAGE];

#define LARGE_PAGE_SIZE	0x10000L

#define MAX_RECORDS_PER_LP  256	/* TBD: why restrict, other than RAM usage? */
#define LP_TABLE_ELEMENT	UWORD	/* # bytes for each record of curLp. */
					/* NOTE: lp's header must be accounted for separately.*/

local LP_TABLE_ELEMENT lpTable[MAX_RECORDS_PER_LP];
local UWORD lpTableOffset = 0;	/* So can pretend lpTable starts at some
								 * element other than 0, to fake out
								 * WriteCurLp. */

typedef struct {
	LP_DESCRIPTOR x;	/* Duplicate of lpfTable[CUR_LP].  NOT relied on,
						 * just here to help scavenge damaged files.
						 * "baseRecord" NOT kept up-to-date, because
						 * LP.AddRecord or DeleteRecord changes
						 * "baseRecord" of ALL following lps -- would be
						 * very slow to add one record near the start of a
						 * huge file!  This field is accurate whenever the
						 * lp is written, but is not relied on after that.
						 * It still helps scavenging.  Since "Optimize Anim"
						 * re-writes every lp, this field is accurate if
						 * Optimize after AddRecord/DeleteRecord. */
	UWORD nByteContinuation;	/* ==0.  FUTURE: allows record from
								 * previous lp to extend on to this lp, so
								 * don't waste file space. */
} LPageHeaderFixed;
   /* These fields precede the array giving "REC_BYTES" per record. */


#define LP_HEADER_SIZE(nRecords)	( sizeof(LPageHeaderFixed) + \
	(nRecords) * sizeof(LP_TABLE_ELEMENT) )
	/* NOTE: if this changes, so must WriteLargePage0(). */

#define FIRST_RECORD_N	0		/* Records #d from 0. */
#define LAST_RECORD_N	(totalNRecords-1)
			/* INCLUDES last-to-first delta if present! */

UWORD totalNRecords;	/* Total # records in lpFile, including lastDelta. */
UWORD nLps;		/* # large pages in lpFile. */


/* --------------------------------------------------------------------------
 * --------------- Description of "current lp". ---------------
 * One lp at a time is present in a buffer of just under 64 KB.
 * The contents of this buffer are maintained in the same form as on disk --
 * fixed header info, variable-length recordSizes table, data for records.
 */

extern UWORD lpSeg;		/* Segment of lp buffer. */

typedef struct {
	UWORD nLp;	/* lp's # w/i file */
	UWORD recAddr0;		/* First byte of first record w/i lp. (after
						 * header). */
	LP_DESCRIPTOR x;	/* Describes lp's contents. */
} LpX;		/* Access to an lp. */

local LpX curLpX;		/* Description of "current lp". */

#define NO_LP   ((UWORD)-1)
#define CUR_LP	curLpX.nLp

/* --- These should be "CUR_LP_...", but the names get unreasonably long. */
#define LP_BASE_REC	curLpX.x.baseRecord
#define LP_N_RECS	curLpX.x.nRecords
#define LP_N_BYTES	curLpX.x.nBytes
#define LP_REC_ADDR0	curLpX.recAddr0
	/* Location of first byte of frame data in lpBuffer. */

/* ASSUMES LP_REC_ADDR0 set for correct # records. */
#define LP_TOTAL_BYTES		(LP_REC_ADDR0 + LP_N_BYTES)
#define GAP_SIZE			(MAX_LARGE_PAGE_SIZE - LP_TOTAL_BYTES)

/* --------------------------------------------------------------------------
 * -------------------- LargePageFile --------------------
 * Lpf's Header size is 256 + 3 * 256 + 6 * maxLps.
 * When lpf created, maxLps must be chosen.  If file needs more lps,
 * will have to move all data in file.  By convention, set maxLps to
 * 256 initially, increase in multiples of 256 as needed.
 * FOR NOW: WON'T HANDLE FILE UNLESS SET TO 256.
 * 256 * 64 KB == 16 MB.
 */

local struct {
	ULONG id;	/* ID for LargePageFile == MakeID('L','P','F',' '). */
	UWORD maxLps;		/* 256 FOR NOW.  max # largePages allowed in this
						 * file. */
	UWORD nLps;	/* # largePages currently in this file.  0..maxLps.
				 * lps #d from 0.
				 * NOTE: In RAM, we don't keep this field up to date. */
	ULONG nRecords;		/* # records currently in this file.  65535 is
						 * current limit.  Allowing one for last-to-first
						 * delta, that restricts to 65534 frames of lo-res
						 * animation.
						 * Records #d from 0.  NOTE: In RAM, we
						 * don't keep this field up to date. */
	UWORD maxRecsPerLp;	/* 256 FOR NOW.  # records permitted in an lp.
						 * So reader knows table size to allocate. */
	UWORD lpfTableOffset;		/* 1280 FOR NOW.  Absolute Seek position of
								 * lpfTable. This allows content-specific
								 * header to be variable-size in the
								 * future. */
	ULONG contentType;	/* for ANIM == MakeID('A','N','I','M').  All
						 * following info is specific to ANIM format. */
	UWORD width;		/* in pixels. */
	UWORD height;		/* in pixels. */
	UBYTE variant;		/* 0==ANIM. */
	UBYTE version;		/* 0== cycles stored for 18x/sec timer.
						 * 1== cycles stored for 70x/sec timer. */
	BOOL hasLastDelta;	/* 1==Record(s) representing final frame are a
						 * delta from last-to-first frame.  nFrames ==
						 * (nRecords/recordsPerFrame)-hasLastDelta. */
	BOOL lastDeltaValid;		/* So needn't actually remove delta.
								 * (When hasLastDelta==1)  0==there is a last
								 * delta, but it hasn't been updated to
								 * match the current first&last frames, so
								 * it should be ignored. This is done so
								 * don't have to physically remove the
								 * delta from the file when the first frame
								 * changes. */
	UBYTE pixelType;	/* ==0 for 256-color. */
	UBYTE highestBBComp;		/* 1 (RunSkipDump) FOR NOW. highest #d
								 * "Record Bitmap.Body.compressionType".
								 * So reader can determine whether it can
								 * handle this Anim. */
	UBYTE otherRecordsPerFrame;	/* 0 FOR NOW. */
	UBYTE bitmapRecordsPerFrame;/* ==1 for 320x200,256-color. */
	UBYTE recordTypes[32];		/* Not yet implemented. */
	ULONG nFrames;		/* In case future version adds other records at end
						 * of file, we still know how many frames. NOTE:
						 * DOES include last-to-first delta when present.
						 * NOTE: In RAM, we don't keep
						 * this field up to date. */
	UWORD framesPerSecond;		/* # frames to play per second. */
	UWORD pad2[29];
#if 0	/* read/write this directly, no need to waste DGROUP */
	Range cycles[MAXNCYCS];
#endif
} lpfHdr;	/* total is 128 words == 256 bytes. */

#if CCYCLE70
#define LPF_VERSION		1		/* cycles stored for 70x/sec timer. */
#else
#define LPF_VERSION		0		/* cycles stored for 18x/sec timer. */
#endif

#if 0
UWORD checkFrames[32];	/* frame #s to checkpoint.  each ==0 when not used.
						 * # non-zero * recsPerFrame == # records used for
						 * this purpose.  Must subtract from nRecords to
						 * determine how many frames are really in file.
						 * TBD: Allocate fixed record #s for these? TBD:
						 * force to be one record per lp? NOTE: since frame
						 * #s, not record #s, seems okay to restrict to
						 * 16-bit.  If lo-res file gets > 64 K frames,
						 * scale #s so can always span # frames. Ex: if
						 * 64K..127K frames, these #s are "*2". Recommend:
						 * # checkpoints proportional to # lps. Keep 1/8th
						 * anim-size in checkpoints. Every time file
						 * doubles in size, double # checks. */

#endif

#define LARGE_PAGE_FILE_ID		MakeID('L','P','F',' ')
#define ANIM_CONTENTS_ID		MakeID('A','N','I','M')

#define LPF_HEADER_HEAD_SIZE_IN_FILE	256
	/* First few fields at start of a Large Page File. */

#define SIZEOF_LPF_TABLE_IN_FILE	(sizeof(LP_DESCRIPTOR) * MAX_LARGE_PAGE)
	/* FUTURE: will be permitted to vary! */

#define LPF_HEADER_SIZE_IN_FILE		( LPF_HEADER_HEAD_SIZE_IN_FILE + \
						 PALETTE_SIZE + SIZEOF_LPF_TABLE_IN_FILE )
 /* Everything in Large Page File before the first lp. */

#define LPF_TABLE_OFFSET	(LPF_HEADER_HEAD_SIZE_IN_FILE + PALETTE_SIZE)
#define BBC_RUN_SKIP_DUMP	1


/* ----------------------------------------------------------------------- */
/* TBD: If you want better error handling, implement these. */
#define DiskFullChangesLostExit()	exit(1)
#define CantCreateAnimFileExit()	exit(1)
#define InvalidAnimFileExit()	exit(1)
#define TooManyPagesExit()	exit(1)
#define BadBitmapRecordExit()	exit(1)
#define UnrecognizedDeltaTypeExit()	exit(1)

#define LPageTooLargeMsg()
#define LpfTooLargeMsg()
#define TroubleWritingFileMsg()
#define LpfNotHandledByVersion()
#define TroubleReadingFileMsg()
#define NotAnAnimFileMsg()


/* ----------------------------------------------------------------------- */

/* --------------- GetFramesPerSecond ---------------
 */
UWORD GetFramesPerSecond()
{
	return lpfHdr.framesPerSecond;
}


/* --------------- DeclareFramesPerSecond ---------------
 * Make sure all parties know what the (new) framesPerSecond play-rate is.
 */
DeclareFramesPerSecond(UWORD newFPS)
{
	if (newFPS == 0)
		newFPS = FASTEST_RATE;
	lpfHdr.framesPerSecond = newFPS;
}


/* --------------- AppendSuffix ---------------
 */
char sDot[] = ".";

void AppendSuffix(char *name, char *baseName, char *suffix)
{
	strcpy(name, baseName);
	strcat(name, sDot);
	strcat(name, suffix);
}


/* --------------- OpenFile ---------------
 */
WORD OpenFile(char *baseName, char *suffix)
{
	WORD file;
	char name[80];

	AppendSuffix(name, baseName, suffix);

	file = opendos(name, O_RDWR);
	if (file == -1)
		file = NULL;			/* No file open. */
	return file;
}


/* --------------- DeleteFile ---------------
 */

void DeleteFile(char *baseName, char *suffix)
{
	char name[80];

	AppendSuffix(name, baseName, suffix);
	deletedos(name);
}


/* --------------- AnimFramesX ---------------
 * Set "animFrames" to new value.
 * This is a low-level routine.
 */
void AnimFramesX(WORD nFrames)
{
	animFrames = nFrames;
}


/* --------------- OpenLpFile ---------------
 */
BOOL OpenLpFile()
{
	register UWORD i;

	lpFile = OpenFile(animName, lpfSuffix);

	if (!lpFile)
		return FAIL;

			/* --- Read lpfHdr. */
	CHECK_DOS(lseekdos(lpFile, (LONG) 0, SEEK_SET));
	CHECK_DOS(readfull(lpFile, (char *)&lpfHdr, sizeof(lpfHdr)));

		/* --- read cycle info */
	CHECK_DOS(readfull(lpFile, (char *)cycles, sizeof(cycles)));

	/* --- Read palette, after skipping future-expansion of lpfHdr.
	 * ASSUME PALETTE_SIZE == 3*256.
	 */
	CHECK_DOS(lseekdos(lpFile, (LONG) LPF_HEADER_HEAD_SIZE_IN_FILE, SEEK_SET));
	CHECK_DOS(readfull(lpFile, (char *)curpal, PALETTE_SIZE));

	/* --- Read lpfTable, immediately after palette. */
	CHECK_DOS(readfull(lpFile, (char *)lpfTable, SIZEOF_LPF_TABLE_IN_FILE));

		/* --- Check that it is a DPaint-Animate Anim. */
	if (lpfHdr.id != LARGE_PAGE_FILE_ID ||
		lpfHdr.contentType != ANIM_CONTENTS_ID) {
		NotAnAnimFileMsg();
		goto badFile;
	}
	if (lpfHdr.maxLps != MAX_LARGE_PAGE ||
		lpfHdr.nRecords > (ULONG) MAX_RECORDS) {
		LpfTooLargeMsg();
		goto badFile;
	}
	/* Note: maxLps may be too SMALL as well. */

	if (lpfHdr.maxRecsPerLp > MAX_RECORDS_PER_LP) {
		LPageTooLargeMsg();
		goto badFile;
	}
		/* --- Check that it is an Anim type we can handle. */
	if (lpfHdr.lpfTableOffset != LPF_TABLE_OFFSET ||
		lpfHdr.width != 320 ||
		lpfHdr.height != 200 || lpfHdr.highestBBComp > BBC_RUN_SKIP_DUMP ||
		lpfHdr.bitmapRecordsPerFrame != 1 || lpfHdr.otherRecordsPerFrame ||
		lpfHdr.pixelType) {
		LpfNotHandledByVersion();
		goto badFile;
	}
	for (i = 0; i < lpfHdr.nLps; i++) {
		if (lpfTable[i].nRecords > MAX_RECORDS_PER_LP ||
			lpfTable[i].nBytes > MAX_LARGE_PAGE_SIZE) {
			LPageTooLargeMsg();
			goto badFile;
		}
		/* FUTURE: hi 2 bits of nRecords indicate continuation. */
	}

	nLps = lpfHdr.nLps;			/* TBD: why not use fields in place? */
	totalNRecords = (UWORD) lpfHdr.nRecords;

	AnimFramesX(lpfHdr.nFrames - lpfHdr.hasLastDelta);
	DeclareFramesPerSecond(lpfHdr.framesPerSecond);

	CUR_LP = NO_LP;				/* None loaded. */
/*	ReadLpfFrame(FIRST_FRAME_N);	--- So have valid, for InsureBuffer...*/
	return SUCCESS;

doserr:
	TroubleReadingFileMsg();
badFile:
	closedos(lpFile);
	lpFile = NULL;
	InvalidAnimFileExit();
	return FAIL;
}


/* --------------- SeekLargePage ---------------
 * Seek lp file to the specified large page.
 * Return FALSE on error.
 */
#define LP_FILE_OFFSET(nLp)	\
			(LONG)((LONG)(nLp) * LARGE_PAGE_SIZE + LPF_HEADER_SIZE_IN_FILE)

local LONG SeekLargePage(UWORD nLp)
{
	if (nLp > MAX_LARGE_PAGE)
		BugExit("LP1");	/* "Invalid lp #"); */

	/* Seek to lp.  File has header, then lps #d from 0. */
	return (lseekdos(lpFile, LP_FILE_OFFSET(nLp), SEEK_SET));
}


/* --------------- ReadLargePage00 ---------------
 * Also reads HEADER into lpBuf, for read-ahead.
 * When editing, don't want this.
 * IMPLICIT PARAMETERS: preFrameAddr0, lpfTable.
 */
local ReadLargePage00(UWORD lpFile, WORD nLp, UWORD seg)
{
	UWORD nBytes;

	preFrameAddr0 = LP_HEADER_SIZE(lpfTable[nLp].nRecords);

	nBytes = preFrameAddr0 + lpfTable[nLp].nBytes;
	if (readdos(lpFile, seg, 0, nBytes) != nBytes)
		BugExit("LP2");			/* "ReadLargePage00: nBytes"); */
}


/* --------------- MaybeWriteCurLp ---------------
 */
local void MaybeWriteCurLp(void)
{
	if (curLpIsDirty) {
		WriteCurLp();
	}
}


/* --------------- ReadLargePage ---------------
 */
void ReadLargePage(UWORD nLp)
{
	MaybeWriteCurLp();
	SeekLargePage(nLp);
	ReadLargePage00(lpFile, nLp, lpSeg);

	curLpX.x = lpfTable[nLp];	/* LP_BASE_REC, LP_N_RECS, LP_N_BYTES */

	LP_REC_ADDR0 = LP_HEADER_SIZE(LP_N_RECS);

/*	far_movmem( lpSeg, 0,  dataseg(), (UWORD)&LP_N_BYTES,    2 ); */
/*  "baseRecord" field ignored. */
/*	far_movmem( lpSeg, 4,  dataseg(), (UWORD)&LP_N_RECS,  2 ); */
/*  "nByteContinuation" field not used. */

	far_movmem(lpSeg, sizeof(LPageHeaderFixed),
			   dataseg(), (UWORD) lpTable,
			   LP_N_RECS * sizeof(LP_TABLE_ELEMENT));

	CUR_LP = nLp;
}


/* --------------- NFrame2NRecord ---------------
 * Given a frame #, compute the record # in the lpFile.
 * ASSUME has a last-delta, which is the last record.
 */
local UWORD NFrame2NRecord(WORD nFrame)
{
	if (nFrame == LAST_DELTA_N)
		return FIRST_RECORD_N;
	return (nFrame - FIRST_FRAME_N + FIRST_RECORD_N);
}


/* --------------- NRecord2NFrame ---------------
 * Given an lpfile record #, compute the frame #.
 * ASSUME records are #d from 0.
 */
UWORD NRecord2NFrame(WORD nRecord)
{
	return nRecord + FIRST_FRAME_N;
}


/* --------------- FindLargePageForRecord ---------------
 * Step through lp descriptions, looking for one that contains
 * requested record.
 * RETURN lp #.
 * NOTE: it is possible for lpfTable[nLp].baseRecord to be non-zero
 * even on an empty page.  Fortunately, the "baseRecord <= nRecord < ..."
 * tests will always reject pages with nRecords==0.
 */
local UWORD FindLargePageForRecord(register UWORD nRecord)
{
	register UWORD nLp, baseRecord;

	for (nLp = 0; nLp < nLps; nLp++) {
		baseRecord = lpfTable[nLp].baseRecord;
		if (baseRecord <= nRecord &&
			nRecord < baseRecord + lpfTable[nLp].nRecords) {
#if DEBUG_BUFS
			if (!editting)
				printf("Find lp:%d for rec:%d\n", nLp, nRecord);		/* TESTING */
#endif
			return nLp;
		}
	}
	BugExit("LP6");				/* "Didn't find record in lpfTable"); */
}


/* --------------- ReadLargePageForRecord ---------------
 * Find lp containing record, then read it.
 */
local void ReadLargePageForRecord(register UWORD nRecord)
{
	ReadLargePage(FindLargePageForRecord(nRecord));
#if 0
	if (FALSE) {
		PrepExit();
		printf("First ReadLargePageForRecord (%d) on playback.", nRecord);

		PrintLPFState();
		exit(1);
	}
#endif	/* 0 */
}


/* --------------- ReadLpfFrame ---------------
 * Transfer to temp buffer the specified delta-frame.
 */
ReadLpfFrame(WORD nFrame)
{
	register UWORD nRecord;

	nRecord = NFrame2NRecord(nFrame);

	if ((CUR_LP >= nLps) ||		/* No lp yet. */
		(nRecord < LP_BASE_REC) ||		/* record not in CUR_LP. */
		(nRecord >= LP_BASE_REC + LP_N_RECS)) {
		if (nRecord >= totalNRecords)
			BugExit("LP7");		/* "Impossible lp record."); */
		ReadLargePageForRecord(nRecord);
	}

	/* --- Step through records in CUR_LP. */
	REC_ADDR = LP_REC_ADDR0;
	CUR_REC = LP_BASE_REC;
	while (nRecord > CUR_REC) {
		REC_ADDR += lpTable[CUR_REC - LP_BASE_REC];
		CUR_REC++;
	}
	REC_BYTES = lpTable[CUR_REC - LP_BASE_REC];

	if (REC_BYTES) {
		UWORD far *ptr;
		UBYTE bitmapId, bitmapFlags;

		ptr = WORD_FAR_PTR(lpSeg, REC_ADDR);
		bitmapId = ((UBYTE far *) ptr)[0];
		if (bitmapId != 'B')
#if 0	/* TESTING */
		{
			PrepExit();
			printf("BadB frame:%d id:%x exN:%d recN:%d bodyN:%d, addrs:%x %x, rec data:%x %x %x\n",
				   nFrame, bitmapId, EXTRA_BYTES,
				   REC_BYTES, BODY_BYTES, REC_ADDR, BODY_ADDR, ptr[0], ptr[1], ptr[2]);
			exit(1);
		}
#else
			BadBitmapRecordExit();
#endif
		bitmapFlags = ((UBYTE far *) ptr)[1];
		EXTRA_DATA_CNT = ptr[1];
		EXTRA_BYTES = (bitmapFlags ? 4 + EVEN_UP(EXTRA_DATA_CNT) : 2);
	} else {
		EXTRA_BYTES = 0;		/* No recData, therefore no extra data. */
	}

	BODY_ADDR = REC_ADDR + EXTRA_BYTES;
	BODY_BYTES = REC_BYTES - EXTRA_BYTES;

	lastRecordInBuf = (CUR_REC == LAST_RECORD_IN_BUF);
}


/* --------------- PlayDeltaFrame0 ---------------
 * Play the current delta info onto the specified frame bitmap.
 * IMPLICIT INPUT: curRecX, lpSeg.
 * NOTE: The 0 BODY_BYTES case isn't handled at this level.
 */
#define PRSD_ASM	1	/* Play RunSkipDump in assembly. */
local void PlayDeltaFrame0(UWORD bmSeg)
{
#if 1
	UWORD bodyType, nBytes;

#else
	register UWORD deltaAddr;
	UWORD picSeg;
	LONG frameType;

#endif

	if (!BODY_BYTES)
		return;					/* empty body == no change */

	if (BODY_BYTES < 2)
		BugExit("LP8");
			/* "Short body of bitmap record");
			 * TBD: return error code
			 */

				/* --- runSkipDump or uncompressed */
#define BODY_HEADER_LEN		sizeof(UWORD)
				/* UWORD holding body type precedes body data. */
	bodyType = *WORD_FAR_PTR(lpSeg, BODY_ADDR);
	nBytes = BODY_BYTES - 2;
	if (bodyType == BMBODY_UNCOMPRESSED) {
		/* --- Direct dump of screen. */
		if (nBytes != N_BYTES_IN_BITMAP)
			BugExit("LP9");		/* "Uncompressed body has wrong # bytes"); */
		far_movmem(lpSeg, BODY_ADDR + BODY_HEADER_LEN,
				   bmSeg, 0, nBytes);
	} else if (bodyType == BMBODY_RUNSKIPDUMP) {		/* --- RunSkipDump
														 * encoding of screen. */
#if PRSD_ASM	/* RunSkipDump in assembly. */
		UWORD dstAddr;
		UBYTE far *srcP;

#else	/* RunSkipDump in C. */
		register signed char cnt;
		UBYTE pixel;
		UWORD wordCnt;
		UBYTE far *srcP, far * dstP;

#endif

		if (!nBytes)
			return;				/* empty delta == no change */
#define BMBODY_STOP_CODE_LEN	3
#define BBSTOP0	LONG_OP
#define BBSTOP1	0
#define BBSTOP2	0
		srcP = BYTE_FAR_PTR(lpSeg, BODY_ADDR + BODY_BYTES - BMBODY_STOP_CODE_LEN);
		if (nBytes < BMBODY_STOP_CODE_LEN || srcP[0] != BBSTOP0 ||
			srcP[1] != BBSTOP1 || srcP[2] != BBSTOP2)
			BugExit("LPA");		/* "RunSkipDump body missing stop code"); */

#if PRSD_ASM	/* RunSkipDump in assembly. */
		dstAddr = PlayRunSkipDump(nBytes, lpSeg, BODY_ADDR + BODY_HEADER_LEN,
								  bmSeg, 0);
		if (dstAddr > N_BYTES_IN_BITMAP)
#else	/* RunSkipDump in C. */
		/* --- Here is an algorithm to playback a 256-color Body,
		 * onto a same-size screen with rows in sequence,
		 * so don't have to test each sequence to see
		 * if it is on a new row.
		 * NOTE: Once it is working, you may remove all the references to
		 * "nBytes", as the stop code is sufficient to end processing.
		 */
		srcP = BYTE_FAR_PTR(lpSeg, BODY_ADDR + BODY_HEADER_LEN);
		dstP = BYTE_FAR_PTR(bmSeg, 0);
		  /* srcP points at first sequence in Body,
		   * dstP points at pixel #0 on screen.
		   */
nextOp:
		if (nBytes < 3)
			BugExit("LPB");		/* "RunSkipDump body elided stop code"); */
		cnt = (signed char) *srcP++;
		if (cnt > 0)
			goto dump;
		if (cnt == 0)
			goto run;
		cnt -= 0x80;
		if (cnt == 0)
			goto longOp;
/* shortSkip */
		nBytes -= 1;			/* length of shortSkip code */
		dstP += cnt;			/* adding 7-bit count to 32-bit pointer */
		if (FP_OFF(dstP) > N_BYTES_IN_BITMAP)
			goto stop;
		goto nextOp;
dump:
		nBytes -= 1 + cnt;		/* length of dump code & data */
		/* ASM:  ds:si=srcP, es:di=dstP, cx=cnt, then do "rep movsb". */
		/* Equivalent:  "do {  *dstP++ = *srcP++;  } while (--cnt);" */
		far_movmem_ptrs(srcP, dstP, cnt);
		dstP += cnt;
		if (FP_OFF(dstP) > N_BYTES_IN_BITMAP)
			goto stop;
		srcP += cnt;
		goto nextOp;
run:
		wordCnt = (UBYTE) * srcP++;		/* 8-bit unsigned count */
		pixel = *srcP++;
		nBytes -= 3;			/* length of run code & data */
		/* ASM:  es:di=dstP, al=pixel, cx=wordCnt, then do "rep stosb". */
		/* Equivalent:  "do {  *dstP++ = pixel; } while (--wordCnt);" */
		far_setmem_ptr(dstP, wordCnt, pixel);
		dstP += wordCnt;
		if (FP_OFF(dstP) > N_BYTES_IN_BITMAP)
			goto stop;
		goto nextOp;
longOp:
		wordCnt = *((UWORD far *)srcP)++;
		if ((WORD)wordCnt <= 0)
			goto notLongSkip;	/* Do SIGNED test. */

/* longSkip. */
		nBytes -= 3;
		dstP += wordCnt;
		if (FP_OFF(dstP) > N_BYTES_IN_BITMAP)
			goto stop;
		goto nextOp;

notLongSkip:
		if (wordCnt == 0)
			goto stop;
		wordCnt -= 0x8000;		/* Remove sign bit. */
		if (wordCnt >= 0x4000)
			goto longRun;

/* longDump. */
		nBytes -= 3 + wordCnt;
		/* ASM:  ds:si=srcP, es:di=dstP, cx=wordCnt, then do "rep movsb". */
		/* Equivalent:  "do {  *dstP++ = *srcP++;  } while (--wordCnt);" */
		far_movmem_ptrs(srcP, dstP, wordCnt);
		dstP += wordCnt;
		if (FP_OFF(dstP) > N_BYTES_IN_BITMAP)
			goto stop;
		srcP += wordCnt;
		goto nextOp;

longRun:
		wordCnt -= 0x4000;		/* Clear "longRun" bit. */
		pixel = *srcP++;
		nBytes -= 4;
		/* ASM:  es:di=dstP, al=pixel, cx=wordCnt, then do "rep stosb". */
		/* Equivalent:  "do {  *dstP++ = pixel; } while (--wordCnt);" */
		far_setmem_ptr(dstP, wordCnt, pixel);
		dstP += wordCnt;
		if (FP_OFF(dstP) > N_BYTES_IN_BITMAP)
			goto stop;
		goto nextOp;

stop:
/* all done */
		if (FP_OFF(dstP) > N_BYTES_IN_BITMAP)
#endif	/* RunSkipDump in C. */
			BugExit("LPC");
			  /* "Wrong amount of screen data generated from RunSkipDump
			   *    body");
			   */
	}
	else {						/* TBD: Should be Info Msg, not Exit. */
		UnrecognizedDeltaTypeExit();
	}
}


/* --------------- PlayDeltaFrame ---------------
 * Play the current delta info onto the specified frame bitmap.
 */
PlayDeltaFrame(BITMAP * bm)
{
	if (BODY_BYTES == 0)
		return;					/* "0 bytes" means "no change". */
	PlayDeltaFrame0(BSEG(bm));
}


/* ----------------------------------------------------------------------- */
BOOL permitScreenSwap = TRUE;

/* --------------- CreateFile ---------------
 */
WORD CreateFile(char *baseName, char *suffix)
{
	WORD file;
	char name[80];

	AppendSuffix(name, baseName, suffix);
	file = creatdos(name, O_RDWR);
	if (file == -1)
		file = NULL;			/* No file open. */
	return file;
}


/* --------------- CloseFile ---------------
 * Once i/o is buffered, this will flush & release buffer.
 */
void CloseFile(WORD file)
{
	if (file && file != -1)
		closedos(file);
			/* NOTE: we use "NULL" to indicate non-open file. */
}


/* --------------- WriteN ---------------
 * Write near data, and return FALSE on error.
 */
BOOL WriteN(UWORD file, BYTE *data, UWORD cnt)
{
	return (cnt == writedos(file, dataseg(), data, cnt));
}


/* --------------- WriteFar ---------------
 * Write far data, and return FALSE on error.
 */
BOOL WriteFar(UWORD file, WORD far * data, UWORD cnt)
{
	return (cnt == writedos(file, FP_SEG(data), FP_OFF(data), cnt));
}


/* --------------- ReadN ---------------
 * Read near data, and return FALSE on error.
 */
BOOL ReadN(UWORD file, BYTE * data, UWORD cnt)
{
	return (cnt == read(file, data, cnt));
}


/* --------------- ReadFar ---------------
 * Read far data, and return FALSE on error.
 */
BOOL ReadFar(UWORD file, WORD far * data, UWORD cnt)
{
	return (cnt == readdos(file, FP_SEG(data), FP_OFF(data), cnt));
}


/* --------------- CreateLpFile0 ---------------
 * Create file with one frame.
 * If "blank", the first frame is blank, otherwise it is the
 * contents of "hidbm".
 */
BOOL animOptimized;		/* TRUE when anim is packed for optimal playback. */
BOOL cleanGap = FALSE;	/* TRUE to put nulls in gap when write lp. */

void CreateLpFile0(BOOL blank)
{
	lpFile = CreateFile(animName, lpfSuffix);
	if (!lpFile)
		CantCreateAnimFileExit();

	setmem(&lpfHdr, sizeof(lpfHdr), 0);	/* BEFORE set any fields. */
#if 0	/* ...done by setmem... */
	lpfHdr.variant = lpfHdr.hasLastDelta =
		lpfHdr.lastDeltaValid = lpfHdr.pixelType =
		lpfHdr.otherRecordsPerFrame = 0;
#endif
	lpfHdr.version = LPF_VERSION;

		/* TBD: Whenever create new anim, starts at a specified rate.
		 * Should this instead be done when program boots, then use whatever
		 * gets loaded?
		 * Hard to say, since can either be set by user,
		 * or by loading an anim (in which case user may not realize it
		 *    changed).
		 */
	   /* TBD: Boot, or NewAnim, now creates at 10 fps.  Good choice? */
	DeclareFramesPerSecond(10 /* FASTEST_RATE */ );

	lpfHdr.id = LARGE_PAGE_FILE_ID;
	lpfHdr.maxLps = MAX_LARGE_PAGE;
	lpfHdr.maxRecsPerLp = MAX_RECORDS_PER_LP;
/*	lpfHdr.nLps & .nRecords are over-ridden by independent variables. */
	lpfHdr.lpfTableOffset = LPF_TABLE_OFFSET;
	lpfHdr.contentType = ANIM_CONTENTS_ID;
	lpfHdr.width = 320;
	lpfHdr.height = 200;
	lpfHdr.bitmapRecordsPerFrame = 1;
	lpfHdr.highestBBComp = BBC_RUN_SKIP_DUMP;

	AnimFramesX(totalNRecords = 1);		/* # records in whole lpFile */
	lpfHdr.nFrames = animFrames + lpfHdr.hasLastDelta;
	nLps = 1;
	CUR_LP = 0;

	CUR_REC = LP_BASE_REC = 0;
	LP_N_RECS = 1;				/* # records in current large page */
	LP_N_BYTES = 0;
	lpTable[0] = 0;				/* A zero-byte page description. */
		/* (Since it is first frame, zero-bytes means all-white). */

	LP_REC_ADDR0 = 0;			/* No header info before frame data in
								 * buffer seg. */
#if 1
	/* --- Hack: Force zero-size first frame to be re-computed as
	 * a series of all-white runs, since the player no longer sets bitmap
	 * to white before first frame.
	 * THEN, say the anim is not dirty, so won't save it unless some
	 * change is made.  This avoids asking user to save the empty
	 * untitled anim, when he boots DP-A, then does LoadAnim.
	 * !!! Is ugly to be calling these routines from inside here. !!!
	 */
	ReadLpfFrame(FIRST_FRAME_N);
	if (blank) {
		SetBitMap(&hidbm, white_color);		/* Clear what will be frame 0. */
		FirstAnimFrame0();
		WriteAnimFrameAndReadNext();
	} else {					/* Use "hidbm" as first frame's contents. */
		FirstAnimFrame00();
		WriteAnimFrameAndReadNext();
	}
	animOptimized = TRUE;
#endif

	WriteCurLp();				/* Make sure disk is up-to-date. */
	ReadCurLp();				/* Overkill.  Makes sure buffer & tables
								 * consistent. */
	ReadLpfFrame(FIRST_FRAME_N);
		/* Adjust BODY_ADDR, etc., for correct header size. */
}


/* --------------- CreateLpFile ---------------
 * Create file, with first frame blank.
 */
void CreateLpFile()
{
	CreateLpFile0(TRUE);
}


/* --------------- WriteHdrAndCloseLpFile ---------------
 * IMPLICIT PARAMETERS: lpFile, lpfHdr, nLps, lpfTable, totalNRecords.
 */
WriteHdrAndCloseLpFile()
{
	register UWORD i;
	BOOL tryAgain = TRUE;

again:
	if (!lpFile)
		return;

	MaybeWriteCurLp();

/*	lpfHdr.version = LPF_VERSION; --redundant */
	lpfHdr.nLps = nLps;
	lpfHdr.nRecords = totalNRecords;
	lpfHdr.nFrames = animFrames + lpfHdr.hasLastDelta;

		/* --- Clear unused elements of lpfTable. */
	setmem(&lpfTable[nLps],
		   sizeof(LP_DESCRIPTOR) * (MAX_LARGE_PAGE - nLps),
		   0);

	/* --- Write lpfHdr. */
	CHECK_DOS(lseekdos(lpFile, (LONG) 0, SEEK_SET));
	CHECK_DOS(writefull(lpFile, (char *) &lpfHdr, sizeof(lpfHdr)));

		/* --- Write cycle info as part of header */
		/* TBD: byte flipping for 68000 */
	CHECK_DOS(writefull(lpFile, (char *) cycles, sizeof(cycles)));

	/* --- Write palette, immediately after future-expansion of lpfHdr.
	 * ASSUME PALETTE_SIZE == 3*256.
	 * NOTE: Palette byte-order not changed between 8086 & 68000.
	 */
	CHECK_DOS(writefull(lpFile, (char *)curpal, PALETTE_SIZE));

		/* --- Write lpfTable, immediately after palette. */
	CHECK_DOS(writefull(lpFile, (char *)lpfTable, SIZEOF_LPF_TABLE_IN_FILE));
	goto ok;

doserr:
	if (tryAgain) {				/* Attempt to overcome spurious disk error. */
		tryAgain = FALSE;
		goto again;
	}
	TroubleWritingFileMsg();
ok:
	closedos(lpFile);
	lpFile = NULL;

	if ((animFrames + lpfHdr.hasLastDelta) != totalNRecords)
		BugExit("LPH");			/* "inconsistent #frames vs #records in
								 * file"); */
								/* NOTE: when hasLastDelta is TRUE,
								 *    nRecords includes that delta.
								 * In that case, number of frames in anim is
								 *    "nRecords-1".
								 * ASSUME TRUE == 1.
								 * WARNING: Calling "BugExit" would be
								 *    recursive, if done before
								 * set lpFile = NULL.
								 */
}


/* --------------- WriteLargePage0 ---------------
 * IMPLICIT PARAMETERS: "lpfTable" describes output lpFile.
 * TBD: should be descriptor containing both lpFile & lpfTable.
 * ASSUME lpFile is open for writing.
 * Q: Can this change to do in one disk i/o operation?
 * That requires lpTable to be moved to head of lpSeg, yet lpTable is
 * variable-size.  So will have to MOVE the 64KB of data.
 * ANS: CANNOT have "lpTable" part of lpSeg (requiring this routine to
 * shift data in lpSeg), as this routine is used to
 * WriteNewLp, while lpSeg still holding other useful data!
 */
#define LP_SIZE_IN_FILE(nLp) \
	(lpfTable[nLp].nBytes + LP_HEADER_SIZE(lpfTable[nLp].nRecords))
 /* Size of an lp in file -- contents PLUS header. */

WORD WriteLargePage0(
					 UWORD lpFile, UWORD seg, LP_TABLE_ELEMENT lpTable[],
					 register LpX * lpX)
{
	LPageHeaderFixed lphf;
	UWORD gapSize, writeSize;
	UBYTE zeroes[512];	/* 512 bytes on stack.  TBD: can be larger? */
	UWORD zeroSeg, zeroAddr;
	BOOL tryAgain = TRUE;

again:
	lpfTable[lpX->nLp] = lpX->x;

	CHECK_DOS(SeekLargePage(lpX->nLp));

		/* --- Write header for lp.
		 * Note: if header changes, so must LP_HEADER_SIZE.
		 */
	lphf.x = lpX->x;
	lphf.nByteContinuation = 0;

	CHECK_DOS(writefull(lpFile, (char *) &lphf, sizeof(lphf)));

	if (lpX->x.nRecords)
		CHECK_DOS(writefull(lpFile,
		   (char *) lpTable, lpX->x.nRecords * sizeof(LP_TABLE_ELEMENT)));
	   /* --- Table describing the records. */

	if (lpX->x.nBytes)
		CHECK_DOS(writedosfull(lpFile, seg, lpX->recAddr0, lpX->x.nBytes));

	if (cleanGap) {				/* Fill gap with nulls. */
		zeroSeg = DAllocBiggest(32768, &writeSize);
		if (writeSize <= sizeof(zeroes))
			zeroSeg = SFree(zeroSeg);
		if (zeroSeg)
			zeroAddr = 0;		/* Allocated a large far buffer */
		else {
			zeroSeg = dataseg();/* Use stack buffer */
			zeroAddr = (UWORD) zeroes;
			writeSize = sizeof(zeroes);
		}
		far_setmem(zeroSeg, zeroAddr, writeSize, 0);
		gapSize = (65535 - LP_SIZE_IN_FILE(lpX->nLp)) + 1;
		while (gapSize) {
			if (writeSize > gapSize)
				writeSize = gapSize;
			gapSize -= writeSize;
			CHECK_DOS(writedosfull(
								   lpFile, zeroSeg, zeroAddr, writeSize));
		}
		if (zeroSeg != dataseg())
			zeroSeg = SFree(zeroSeg);
	}
	return (SUCCESS);
doserr:
	if (tryAgain) {				/* Attempt to overcome spurious disk error. */
		tryAgain = FALSE;
		goto again;
	}
	TroubleWritingFileMsg();
	return (FAIL);
}


/* --------------- WriteCurLp ---------------
 * Write current large page back into buffer.
 */
local WORD WriteCurLp(void)
{
	WORD result;

	result = WriteLargePage0(lpFile, lpSeg, &lpTable[lpTableOffset], &curLpX);
	curLpIsDirty = FALSE;
	return result;
}


/* --------------- ReadCurLp ---------------
 * Read current large page back into buffer.
 * TBD: If no lp specified yet, does nothing.  Should it load first frame
 * instead?
 */
local void ReadCurLp(void)
{
	if (CUR_LP < nLps)
		ReadLargePage(CUR_LP);
}


/* --------------- RecordFits ---------------
 */
BOOL RecordFits(UWORD srcBytes)
{
	if (LP_N_RECS == MAX_RECORDS_PER_LP)
		return FALSE;

			/* --- If added this record, check whether header + body still
			 *    fit.
			 */
	if (LP_HEADER_SIZE(LP_N_RECS + 1) + (ULONG)LP_N_BYTES + (ULONG)srcBytes >
		(ULONG)MAX_LARGE_PAGE_SIZE)
		return FALSE;
	return TRUE;
}


/* --------------- LpfFrameLimitAddr ---------------
 * For current lpf frame, RETURN limitAddr.
 * == current size of frame.
 */
UWORD LpfFrameLimitAddr()
{
	return BODY_ADDR + BODY_BYTES;
}


/* ------------------------------------------------------------ */
/* --------------- Other routines --------------- */

/* --------------- IsLpfId ---------------
 * RETURN TRUE if id matches Large-Page-File's id.
 */
BOOL IsLpfId(ULONG id)
{
	return (id == LARGE_PAGE_FILE_ID);
}


/* ------------------------------------------------------------------------
 *							Delta management.
 * ------------------------------------------------------------------------
 */


/* --------------- delta buffer ---------------
 * Buffer to hold one delta (difference between two bitmaps).
 * (seg:addr) is start of buffer, (seg:limit) is just past end of buffer,
 * "nBytes" is length of delta.
 */
typedef struct {
	UWORD seg, addr, limit, nBytes;
}      DeltaX;
DeltaX deltaX = {0};

extern UWORD allocSeg;

/* --------------- ComputeDeltaX ---------------
 * Given prevFrame & current frame in hidbm, compute delta in save_bitmap.
 * IMPLICIT INPUT: deltaX, hidbmSeg, prevFrameSeg.
 * RETURN TRUE if succeeded in creating delta, FALSE if delta didn't fit.
 * SIDE_EFFECT: set deltaX.nBytes, store that many bytes at deltaX.(seg:addr).
 * Note: if algorithm is changed, must be careful that can't exceed
 * MAX_LARGE_PAGE_SIZE.
 */
#define MRSD_ASM	1	/* Make RunSkipDump in assembly. */

#define CLIP_X(stopLabel)	 {	\
	if (x == w) {				\
		x = 0;					\
		if (inFinalRow) {		\
			mustStop = TRUE;	\
			goto stopLabel; }	\
		if (src2 == finalRow2) {\
			w = finalRowWidth;	\
			inFinalRow = TRUE; }\
		}						\
	}

#define CROSSED_W(n)	(xWas + (n) > width)
	/* Test avoids using SHORT ops when line-wrap.
	 * This is specified, so can write algorithm to play onto wider screen,
	 * without having to test shortDump & shortRun for line-wrap on any pixel.
	 * "width" rather than "w", since is from previous line, not current line.
	 * This only matters on last line.
	 */

#define PUT_DUMP(body, cnt)   if (cnt) {			\
			if ((cnt) <= MAX_SHORT_DUMP  &&  !CROSSED_W(cnt)) \
				*dst++ = OP_DUMP | (cnt);		\
			else{								\
				*dst++ = LONG_OP;				\
				*((UWORD far *)dst)++ =			\
				   (LONG_DUMP<<8) | (cnt); }	\
			if (dst - dst0 > finalDst-(cnt))	\
				goto incompressible;			\
			far_movmem_ptrs( body, dst, cnt );	\
			dst += cnt;  }
 /* NOTE the check for zero cnt.
  * NOTE that cnt must already have been limited to MAX_DUMP.
  */

#define PUT_RUN(wordCnt,runPix)	if (wordCnt) {	\
			if ((wordCnt) <= MAX_SHORT_RUN  &&  !CROSSED_W(wordCnt)) { \
				*dst++ = OP_RUN;				\
				*dst++ = wordCnt; }				\
			else{								\
				*dst++ = LONG_OP;				\
				*((UWORD far *)dst)++ =			\
				   (LONG_RUN<<8) | (wordCnt); }	\
			*dst++ = runPix;					\
			}


local BOOL ComputeDeltaX(BOOL skipPermitted)
{
	/* Inputs: */
	UBYTE far *src1, far * src2, far * final2, far * dst;
	UWORD width, finalDst;

	/* src1 points to previous frame, src2 to screen, limit2 to final
	 *    screen
	 * byte, dst points to "finalDst+1" byte buffer for Body,
	 * width is # pixels in a row.  For 64 KB buffer, finalDst==0xFFFF.
	 */
	/* Local variables: */
	UBYTE far *finalRow2, far * dst0;
	UBYTE pixel, runPix, skipInDump, runInDump;
	UWORD x, w, finalRowWidth, wordCnt, xWas;
	BOOL inFinalRow, mustStop;

#if 0
	UWORD deltaSize;

#endif
	if (!deltaX.limit)
		BugExit("LPJ");			/* "ComputeDelta -- deltaX.limit"); */

	src1 = BYTE_FAR_PTR(prevFrameSeg, 0);
	src2 = BYTE_FAR_PTR(hidbmSeg, 0);
	final2 = BYTE_FAR_PTR(hidbmSeg, NBYTES_IN_BITMAP(&hidbm) - 1);
	dst = BYTE_FAR_PTR(deltaX.seg, deltaX.addr);
	width = hidbm.box.w;
	finalDst = deltaX.limit;

	w = width;
	finalRowWidth = (UWORD)(final2 - src2 + 1) % width;
	if (!finalRowWidth)
		finalRowWidth = width;	/* # pixels in final row. */
	finalRow2 = final2 - (finalRowWidth - 1);	/* Ptr to start of final
												 * row. */
	inFinalRow = (finalRow2 == src2);
	  /* FALSE unless src2 is a single row. */
	if (inFinalRow)
		w = finalRowWidth;		/* In case single-row src is shorter than
								 * width. */


#if MRSD_ASM	/* RunSkipDump in assembly. */
	return MakeRunSkipDump(prevFrameSeg, hidbmSeg, NBYTES_IN_BITMAP(&hidbm) - 1,
					deltaX.seg, deltaX.addr, deltaX.limit, &deltaX.nBytes,
		  w, FP_OFF(finalRow2), finalRowWidth, inFinalRow, skipPermitted);

#else	/* ---- RunSkipDump in C. --- most likely out of date ------ */
	dst0 = dst;					/* Remember, so can calculate Body size. */
	*(UWORD far *) dst = BMBODY_RUNSKIPDUMP;	/* 2-byte header with type. */
	dst += 2;

	x = 0;						/* Keep track of pixel-position, so break
								 * at row end. */
	mustStop = FALSE;

notInASequence:
	wordCnt = 0;

/*--- "pixel" must be set when jump to beDump with "wordCnt != 0".*/
beDump:
	xWas = x + width - wordCnt;	/* Where in line dump started. */
	if (xWas >= width)
		xWas -= width;
			  /* "width" logic in case x < wordCnt, because already
			   *    line-wrapped.
			   */
	if (FP_OFF(dst) > finalDst - 3)
		goto incompressible;
			/* "3" is longDump overhead.  Dump DATA is checked inside
			 *    PUT_DUMP.
			 */
	skipInDump = 0;
	runInDump = (wordCnt ? 1 : 0);		/* Any pixel is run==1. */
inDump:
	CLIP_X(stopDump);			/* If mustStop, goto to "stopDump" to get
								 * final dump. */
	runPix = pixel;
	pixel = *src2++;
	x++;
	wordCnt++;					/* Get&count byte. */
	if (pixel == *src1++)
		skipInDump++;			/* # skippable bytes. */
	else
		skipInDump = 0;
	if (pixel == runPix)
		runInDump++;			/* # runnable bytes. */
	else
		runInDump = 1;			/* Any pixel is run==1. */

	if (skipInDump == MIN_SKIP && skipPermitted) {		/* Minimum worthwhile
														 * skip. */
		PUT_DUMP(src2 - wordCnt, wordCnt - MIN_SKIP);
		wordCnt = MIN_SKIP;
		goto beSkip;			/* "Skip 2" so far. */
	}
	if (runInDump == MIN_RUN) {	/* Minimum worthwhile run. */
		PUT_DUMP(src2 - wordCnt, wordCnt - MIN_RUN);
		wordCnt = MIN_RUN;
		goto beRun;				/* "Run 4" so far. */
	}
	/* --- Byte still in Dump. */
	if (wordCnt == MAX_LONG_DUMP) {
stopDump:
		PUT_DUMP(src2 - wordCnt, wordCnt);
		if (mustStop)
			goto stop;
		goto notInASequence;
	}
	goto inDump;

beRun:
	xWas = x + width - wordCnt;	/* Where in line run started. */
	if (xWas >= width)
		xWas -= width;
	if (FP_OFF(dst) > finalDst - 4)
		goto incompressible;	/* "4" is length of longRun. */
	skipInDump = 0;				/* ACTUALLY skip is in run, not in dump... */
	runPix = pixel;				/* Only need do once per run. */
inRun:
	CLIP_X(stopRun);			/* If mustStop, goto to "stopRun" to get
								 * final run. */
	pixel = *src2++;
	x++;
	wordCnt++;					/* Get byte & count it. */
	if (pixel == *src1++)
		skipInDump++;			/* # skippable bytes. */
	else
		skipInDump = 0;
		/* Note: check for byte runnable comes later. */

	if (skipInDump == MIN_RUN_SKIP && skipPermitted) {
		wordCnt -= MIN_RUN_SKIP;
		PUT_RUN(wordCnt, runPix);
		wordCnt = MIN_RUN_SKIP;
		goto beSkip;			/* "Skip 4" so far. */
	}
	if (pixel != runPix) {		/* Byte not runnable -- requires Dump. */
		wordCnt--;				/* Retract pixel from Run. */
		PUT_RUN(wordCnt, runPix);
		wordCnt = 1;
		goto beDump;			/* "Dump 1" so far. */
	}
	/* --- Byte runnable. */
	if (wordCnt == MAX_LONG_RUN) {
stopRun:
		PUT_RUN(wordCnt, runPix);
		if (mustStop)
			goto stop;
		goto notInASequence;
	}
	goto inRun;

beSkip:
	xWas = x + width - wordCnt;	/* Where in line skip started. */
	if (xWas >= width)
		xWas -= width;
	if (FP_OFF(dst) > finalDst - 3)
		goto incompressible;	/* "3" is length of LongSkip. */
inSkip:
	runPix = pixel;
	pixel = *src2++;
	x++;
	wordCnt++;					/* Get byte & count it. */
	if (pixel != *src1++) {		/* Byte not skippable. */
		wordCnt--;				/* Retract pixel from Skip. */
		if (wordCnt <= MAX_SHORT_SKIP) {
			*dst++ = OP_SKIP | wordCnt;
		} else if (wordCnt <= 2 * MAX_SHORT_SKIP) {
			*dst++ = OP_SKIP | MAX_SHORT_SKIP;
			wordCnt -= MAX_SHORT_SKIP;
			*dst++ = OP_SKIP | wordCnt;
		} else {
			UWORD wordCnt0 = wordCnt;

			while (wordCnt0) {
				wordCnt = MIN(wordCnt0, MAX_LONG_SKIP);
				wordCnt0 -= wordCnt;
				*dst++ = LONG_OP;
				*dst++ = (UBYTE)wordCnt;		/* Low byte of UWORD. */
				*dst++ = wordCnt >> 8;	/* High byte of UWORD. */
			}
		}
		wordCnt = 1;
		goto beDump;			/* "Dump 1" so far. */
	}
	/* --- Byte skippable. */
	/* --- Final Skip is NOT output prior to Stop. */
	CLIP_X(stop);
	goto inSkip;				/* Skip may be any length up to 64KB-1. */

stop:
	if (FP_OFF(dst) > finalDst - 3)
		goto incompressible;	/* "3" is length of Stop. */
	*dst++ = LONG_OP;			/* Stop represented as "LongOp #0". */
	/* 68000 note: Don't use word op to put cnt, as may not be
	 *    word-aligned.
	 */
	*dst++ = 0;					/* High byte of cnt==0. */
	*dst++ = 0;					/* Low byte of cnt==0. */
	deltaX.nBytes = dst - dst0;
	  /* "body size": # bytes in Body, excluding any header.
	   * NOTE: incompressible tests guarantee that deltaX.nBytes won't quite
	   * reach 64 KB, which would overflow UWORD.
	   */
	return TRUE;				/* deltaX.nBytes is set as a side-effect. */
	  /* NOTE: if deltaX.nBytes == 3, all there is is a Stop.
	   * Client may wish to set deltaX.nBytes == 0 & have no data in this
	   *    case.
	   */

incompressible:
	return FALSE;
#endif	/* RunSkipDump in C. */
}


/* --------------- AbandonChangesMaybeExit ---------------
 */
local void AbandonChangesMaybeExit(BOOL closeAndExit)
{
	CUR_LP = (UWORD)-1;		/* None loaded -- abandon lpBuffer. */
	curLpIsDirty = FALSE;
	/* --- Drastic solution -- don't try to continue. */
	if (closeAndExit)
		DiskFullChangesLostExit();		/* closes lp. */
}


/* --------------- FindFreeLp ---------------
 */
local UWORD FindFreeLp(void)
{
	register UWORD nLp;

	for (nLp = 0;  nLp < nLps;  nLp++) {
		if (lpfTable[nLp].nRecords == 0)
			return nLp;			/* Found empty lp in middle of file. */
	}
	if (nLps == MAX_LARGE_PAGE) {
		BugExit("LPK");			/* "Too many pages in anim"); */
	}
	return nLps++;			/* Page # of next lp to use (lps #d from 0). */
}


/* --------------- LpTableToLpBufferHeader ---------------
 * Assuming space made available, put correct contents in header,
 * based on lpTable & variables.
 * ASSUMES LP_N_RECS & LP_N_BYTES has already been changed to reflect # recs
 * & new contents in lp.
 * Copy these changes to lpfTable, so FindLargePageForRecord will see truth.
 */
local void LpTableToLpBufferHeader(void)
{
	far_movmem(dataseg(), (UWORD)&curLpX.x, lpSeg, 0, sizeof(LP_DESCRIPTOR));
	*WORD_FAR_PTR(lpSeg, sizeof(LPageHeaderFixed) - 2) = 0;	   /* no cont'. */
		/* ASSUMES nByteContinuation is last field. */
	far_movmem(dataseg(), (UWORD)lpTable, lpSeg, sizeof(LPageHeaderFixed),
			   sizeof(LP_TABLE_ELEMENT) * LP_N_RECS);

	lpfTable[CUR_LP] = curLpX.x;	/* IMPORTANT! */
}


/* --------------- UpdateLpBufferHeader ---------------
 * Shift the data in the lp buffer, and re-write the header, so that it
 * matches lpTable.
 */
local void UpdateLpBufferHeader(void)
{
	UWORD newRecAddr0 = LP_HEADER_SIZE(LP_N_RECS);

	far_movmem_same_seg(lpSeg, LP_REC_ADDR0, newRecAddr0, LP_N_BYTES);

		/* --- "current record" info has changed location. */
	REC_ADDR = REC_ADDR - LP_REC_ADDR0 + newRecAddr0;
	BODY_ADDR = BODY_ADDR - LP_REC_ADDR0 + newRecAddr0;

	LP_REC_ADDR0 = newRecAddr0;
	LpTableToLpBufferHeader();
	curLpIsDirty = TRUE;
}


/* --------------- WriteNewLp ---------------
 * Extract a portion of lp buffer as a new lp.
 * Return FAIL if trouble writing (presumably, disk is full).
 */
#define SV_BASE_REC	svLpX.x.baseRecord
local WORD WriteNewLp(UWORD startRecord, UWORD endRecord)
{
	register UWORD nRecord;
	WORD result;
	LpX svLpX;

	svLpX = curLpX;
	CUR_LP = FindFreeLp();

	lpTableOffset = startRecord - SV_BASE_REC;
		/* Fake out WriteCurLp. */

			/* NOTE: need unmodified startRecord & endRecord for this. */
	LP_BASE_REC = startRecord;
	LP_N_RECS = endRecord - startRecord + 1;

	startRecord -= SV_BASE_REC;
	endRecord -= SV_BASE_REC;

		/* NOTE: use startRecord/endRecord RELATIVE to SV_BASE_REC. */
		/* --- Get from real LP_REC_ADDR0 to fake LP_REC_ADDR0 (at
		 *    startRecord).
		 */
	for (nRecord = 0; nRecord < startRecord; nRecord++) {
		LP_REC_ADDR0 += lpTable[nRecord];
	}

		/* --- Sum of bytes for desired records. */
	LP_N_BYTES = 0;				/* So far */
	for (nRecord = startRecord; nRecord <= endRecord; nRecord++) {
		LP_N_BYTES += lpTable[nRecord];
	}
	result = WriteCurLp();		/* New lp. */

	lpTableOffset = 0;			/* Done faking out WriteCurLp. */
	curLpX = svLpX;				/* Restore access to "current lp". */
							/* Including LP_REC_ADDR0! */
	return result;
}


/* --------------- CurRec_NewBody ---------------
 * Place a new body into CurRec.  If necessary, records before and/or after
 * current are moved into new lps.
 * IMPLICIT INPUT: curRecX, deltaX, lpSeg, curLpX.
 */
local void CurRec_NewBody(void)
{
	WORD result = SUCCESS;

	/* 1. Determine how much room for new body. */
#define AVAIL_BODY_SIZE		(GAP_SIZE + BODY_BYTES)
	UWORD availBodySize = AVAIL_BODY_SIZE;
	BOOL newExtraBytes = (EXTRA_BYTES == 0);

		/* If there was no record at all, create an ID+FLAGS UWORD. */
	if (newExtraBytes)
		availBodySize -= 2;		/* For ID+FLAGS. */

			/* 2A. If won't fit, move other records to new lps. */
	if (deltaX.nBytes > availBodySize) {
		/* --- Dumbest possible approach: split into 3 buffers. */
		UWORD newRecAddr0 = LP_HEADER_SIZE(1);

				/* --- Following records. */
		if (CUR_REC != LAST_RECORD_IN_BUF && result == SUCCESS) {
			result = WriteNewLp(CUR_REC + 1, LAST_RECORD_IN_BUF);
/*			if (result != SUCCESS)   goto fail;   TBD... */
			/* Cur lp now has just base-rec thru cur-rec. */
			LP_N_RECS = (CUR_REC + 1) - LP_BASE_REC;
			LP_N_BYTES = (REC_ADDR + REC_BYTES) - LP_REC_ADDR0;
			UpdateLpBufferHeader();
			availBodySize = AVAIL_BODY_SIZE;
			if (newExtraBytes)
				availBodySize -= 2;		/* For ID+FLAGS. */
			if (deltaX.nBytes <= availBodySize)
				goto nowFits;
		}
		if (CUR_REC > LP_BASE_REC && result == SUCCESS)
			result = WriteNewLp(LP_BASE_REC, CUR_REC - 1);		/* Previous records */

		if (result != SUCCESS) {
			/* TBD: Don't use new lps. */
			BugExit("NewLp");
		}
				/* --- Prepare lp with just CUR_REC. */
		LP_BASE_REC = CUR_REC;
		LP_N_RECS = 1;
		if (newExtraBytes)
			EXTRA_BYTES = 2;
		else
			far_movmem(lpSeg, REC_ADDR, lpSeg, newRecAddr0, EXTRA_BYTES);
		BODY_BYTES = deltaX.nBytes;
		LP_REC_ADDR0 = REC_ADDR = newRecAddr0;
		BODY_ADDR = REC_ADDR + EXTRA_BYTES;
		REC_BYTES = EXTRA_BYTES + BODY_BYTES;
		LP_N_BYTES = lpTable[0] = REC_BYTES;
		LpTableToLpBufferHeader();		/* Buffer now has single frame in
										 * it! */
											/* NOTE: should NOT do i/o,
											 *    just preparing variables.
											 */
	} else {					/* 2B. Make room for new body, & set
								 * variables. */
		UWORD curRecLimit;
		UWORD sizeChange;

nowFits:
		curRecLimit = REC_ADDR + REC_BYTES;
		sizeChange = deltaX.nBytes - BODY_BYTES;
			/* NOTE: May be NEGATIVE number.  Relying on 16-bit wrapping! */
#define BYTES_AFTER_BODY	(LP_TOTAL_BYTES - curRecLimit)

		if (newExtraBytes) {
			EXTRA_BYTES = 2;
			sizeChange += EXTRA_BYTES;
			BODY_ADDR = REC_ADDR + EXTRA_BYTES;
				/* No longer access old body. */
		}
#define NEW_REC_LIM			(curRecLimit + sizeChange)
			/* ASSUMES body is last field in record. */
			/* ASSUMES lp starts at addr 0 w/i lpSeg. */

		far_movmem_same_seg(lpSeg, curRecLimit, NEW_REC_LIM, BYTES_AFTER_BODY);
		BODY_BYTES = deltaX.nBytes;		/* When newExtra, not same as
										 * sizeChange */
		REC_BYTES += sizeChange;
		LP_N_BYTES += sizeChange;
		lpTable[CUR_REC - LP_BASE_REC] = REC_BYTES;
	}

		/* 3. Get new body into lp buffer. */
	if (newExtraBytes)
		*WORD_FAR_PTR(lpSeg, REC_ADDR) = DEFAULT_BITMAP_ID_FLAGS;
	far_movmem(deltaX.seg, deltaX.addr, lpSeg, BODY_ADDR, BODY_BYTES);
	curLpIsDirty = TRUE;
}

/* --------------- DAllocBiggest ---------------
 * Allocate the requested block.  If can't, find and allocate
 * the biggest possible.
 * TBD: Is there a DOS or BIOS call to find biggest available block?
 * RETURNS seg # of allocation, NULL if failed.
 * SIDE_EFFECT: sets "*allocSizeP" to # bytes actually allocated.
 */
local UWORD DAllocBiggest(UWORD nBytes, UWORD * allocSizeP)
{
	UWORD seg, allocParas;

		/* --- First, try to allocate full request, kicking out other stuff
		 * if necessary.
		 */
	seg = DAlloc(nBytes);
	if (seg) {
		*allocSizeP = nBytes;
		return seg;
	}

	/* --- Couldn't get all, get what we can. */

#define N_PARAGRAPHS(nBytes)	( (nBytes >> 4) + (nBytes & 0x0f ? 1 : 0) )
		/* NOTE: Don't do as "(nBytes+15) >> 4", since may overflow UWORD. */

	seg = allocsome(N_PARAGRAPHS(nBytes), &allocParas);
	*allocSizeP = allocParas << 4;
	if (!seg)					/* Only happens if memory completely full. */
		*allocSizeP = 0;
	return seg;
}


/* --------------- ComputeDelta ---------------
 * Compute delta into a large temporary buffer.
 * IMPLICIT INPUT: curRecX, lpSeg.
 * SIDE_EFFECT: sets deltaX.
 */
local void ComputeDelta(void)
{
	BOOL skipPermitted = (curFrame != FIRST_FRAME_N);

	/* Disallow skip code on first frame, so don't need to blank
	 * frame before drawing delta -- will overwrite every pixel
	 * via Dumps and Runs.
	 */

	deltaX.seg = allocSeg;
	deltaX.addr = 0;
	deltaX.limit = MAX_LARGE_PAGE_SIZE;

bigEnough:
	if (!ComputeDeltaX(skipPermitted)) {
		/* Handle incompressible frame. */
		UWORD rawSize = N_BYTES_IN_BITMAP;

		deltaX.nBytes = rawSize + sizeof(UWORD);
			/* UWORD for header, then raw dump of screen data. */
		if (deltaX.limit - deltaX.addr < deltaX.nBytes)
			BugExit("LPM");
				/* "Buffer too small for incompressible frame"); */
		*WORD_FAR_PTR(deltaX.seg, deltaX.addr) = BMBODY_UNCOMPRESSED;
			/* header contains type "uncompressed". */
		far_movmem(hidbmSeg, 0, deltaX.seg, deltaX.addr + 2, rawSize);
	}
}


/* --------------- WriteFinalAnimFrame ---------------
 * Compute and transfer to file the current delta-frame,
 * which must be the last frame in the file.
 * (If not last frame, must use WriteAnimFrameAndReadNext).
 */
void WriteFinalAnimFrame(void)
{
	ComputeDelta();
	CurRec_NewBody();			/* Implicit: deltaX. */
}

/* --------------- WriteAnimFrameAndReadNext ---------------
 * Transfer to file the specified delta-frame.
 * Also compute & transfer the next delta-frame, as it is affected by the
 * changes in the current anim-frame.
 * TBD: Some caller's may wish to MaybeWriteCurLp, so that lp buffer is
 * free for immediate use by menus/pop-ups.
 */
void WriteAnimFrameAndReadNext(void)
{
	ComputeDelta();

	/* ---2. Exchange prevFrame with new-frame in hidbm.
	 *		 Because new-frame is next-frame's prevFrame.
	 */
	far_swapmem(BSEG(&hidbm), 0, prevFrameSeg, 0, N_BYTES_IN_BITMAP);

	/* ---3. Apply old-delta to hidbm.  (Convert prev- to old- frame.) */
	PlayDeltaFrame(&hidbm);		/* Implicit: curRecX, lpSeg. */
	CurRec_NewBody();			/* Implicit: deltaX. */

	/* ---4. If at anim end, simply set up first frame. */
	/* FUTURE: may compute delta between last & first frame.
	 * Could store in special frame before the normal first frame.
	 */
	curFrame++;
	if (curFrame > LAST_FRAME_N) {
		FirstAnimFrame0();
		goto done;
	}

	/* ---5. Read & apply next delta. */
	ReadLpfFrame(curFrame);
	PlayDeltaFrame(&hidbm);		/* Convert old-frame to next-frame. */

	/* ---6. Recompute next-frame's delta (since prevFrame is new.) */
	ComputeDelta();
	CurRec_NewBody();

done: ;
}

/* --------------- AddFrame ---------------
 * ASSUMES current frame is last frame in lpfile.
 * Add an identical frame after it, and make it current.
 * NOTE: Be careful to leave valid cur rec & cur lp variables.
 */
void AddFrame()
{
	UWORD afterRecord = CUR_REC; /* Hold, in case lp changes affect CUR_REC.*/
	WORD afterFrame = curFrame;

	if ( MAX_RECORDS_PER_LP - LP_N_RECS < 1  ||		/* lpTable overflow*/
		 MAX_LARGE_PAGE_SIZE - LP_N_BYTES <
			LP_HEADER_SIZE(LP_N_RECS+1)				/* lp overflow */
	   ) {
		/* --- Create new lp, with only an empty frame. */
		MaybeWriteCurLp();

		LP_BASE_REC = afterRecord + 1;
		LP_N_BYTES = 0;
		setmem( &lpTable[0], sizeof(LP_TABLE_ELEMENT), 0 );
			/* Zero the new frame. */

		/* --- SWITCH ACCESS from current lp to new lp. */
		CUR_LP = FindFreeLp();
		LP_N_RECS = 1;

		WriteCurLp();
	} else {
		/* Add frame to current lp.
		 * Zero the new frame.  record # relative to CUR_LP's base rec.
		 */
		lpTable[afterRecord+1 - LP_BASE_REC] = 0;
		LP_N_RECS += 1;
	}
	totalNRecords += 1;
	AnimFramesX(animFrames + 1);

	UpdateLpBufferHeader();
	ReadLpfFrame(curFrame = afterFrame+1);	/* Make this frame current. */
}

/* --------------- ConvertFinalFrameToLastDelta ---------------
 */
ConvertFinalFrameToLastDelta()
{
	animFrames--;
	lpfHdr.hasLastDelta = lpfHdr.lastDeltaValid = TRUE;
	curFrame--;
}
