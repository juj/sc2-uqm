/*--anim.h----------------------------------------------------------
 * anim definitions for ANIM2PCX.
 */

/* --- DeluxePaint Animation currently imposes this limit, though
 * file format is capable of more.
 */
#define MAX_SUPPORTED_FRAMES	(UWORD)9999
#define MAX_FRAME_CHARS         4		/* # digits in MAX_SUPPORTED_FRAMES */

#define MAX_FRAMES	(UWORD)65534
	/* Maximum # frames supported by current anim format.
	 * "65534": 1 left at end for "last-to-first" delta.
	 * NOTE: can't usefully be this large, as need free lp's during editting.
	 * Most anim's won't fit 255 pages per lp, so until we allow more than
	 * 255 lp's, won't reach MAX_FRAMES anyway.
	 */
#define MAX_RECORDS	(UWORD)65535

#define FASTEST_RATE	70		/* Fastest rate anim can be played at. */

#define MAX_LARGE_PAGE_SIZE	(UWORD)(320*203)
	/* WARNING: this may be stored in DP Animation's picture buffer,
	 * ASSUMES we've allocated three extra lines to the buffer.
	 * This allows us to come close to 64 KB, to minimize file wastage.
	 */

#define NBYTES_IN_BITMAP(bm)	((UWORD)(bm)->width * (UWORD)(bm)->box.h)
	/* TBD: ASSUMES < 64 KB.  Won't be true for Super-VGA. */

#define BSEG(bitmap)	(bitmap)->seg[0]

#define PALETTE_SIZE	(MAX_COLORS * sizeof(LONG))

#define FIRST_FRAME_N	1		/* Frame #s start at 1. */
#define LAST_DELTA_N	(FIRST_FRAME_N-1)
	/* Frame # used to store delta from last-frame to first. */
#define LAST_FRAME_N	(animFrames-1+FIRST_FRAME_N)
extern UWORD animFrames;

/* --------------- from IFF.H (Interchange File Format) ---------------
 */
/* --- Four-character IDentifier builder. */
#define MakeID(d,c,b,a) ((LONG)(a)<<24 | (LONG)(b)<<16 | \
						 (LONG)(c)<<8 | (LONG)(d) )


/* --------------- anim file format -------------------- */

#define CCYCLE70	0		/* 70 Hz color-cycle interrupt */
  /* P:Caused performance problems on slow machines, especially with cache on.
   */

/* ---Bitmap body types. */
#define BMBODY_UNCOMPRESSED	0
#define BMBODY_RUNSKIPDUMP	1	/* Must match ANIMA.ASM/MakeRunSkipDump. */

/* --- RunSkipDump Bitmap body format.  Matches ANIM.INC*/
#define OP_DUMP		 0x00		/* | cnt */
#define OP_RUN		 0x00		/* Cnt in separate byte. */
#define OP_SKIP		 0x80		/* | cnt */
#define LONG_OP		 0x80
#define LONG_SKIP	  0x00		/* sub-ops for LONG_OP */
#define LONG_DUMP	  0x80
#define LONG_RUN	  0xC0
#define MAX_SHORT_DUMP	  127
#define MAX_LONG_DUMP  0x3fff
#define MAX_SHORT_RUN 	  255
#define MAX_LONG_RUN   0x3fff
#define MAX_SHORT_SKIP	  127
#define MAX_LONG_SKIP  0x7fff
#define MIN_RUN		 4	/* When in Dump, minimum worthwhile Run. */
#define MIN_SKIP	 2	/* When in Dump, minimum worthwhile Skip. */
#define MIN_RUN_SKIP 4	/* When in Run, minimum worthwhile Skip. */
	/* Note: deltaX.nBytes shorter if can subsume Skip in the Run,
	 * but PC screen i/o is slow enough that it is worth time-saving
	 * of having the Skip.
	 */

/*----------------------------------------------------------------------*/
/* Color Cycles															*/
/*----------------------------------------------------------------------*/

#define MAXNCYCS 16
/* lpfile depends on the size of this structure */
typedef struct {
	WORD count;
	WORD rate;
	WORD flags;
	UBYTE low, high;		/* bounds of range */
	} Range;
