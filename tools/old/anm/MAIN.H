/*--main.h----------------------------------------------------------
 * Main include file for ANIM2PCX.
 * Note: all files editted with tab stops every 4 spaces.
 */

/* ------------------------------------------------------------------------
 * Typedefs for portability.
 */

typedef char BOOL;				/* 0==FALSE, 1==TRUE */
typedef char BYTE;				/* signed    8-bit number */
typedef unsigned char UBYTE;	/* unsigned  8-bit number */
typedef int WORD;				/* signed   16-bit number */
typedef unsigned int UWORD;		/* unsigned 16-bit number */
typedef long LONG;				/* signed   32-bit number */
typedef unsigned long ULONG;	/* unsigned 32-bit number */
typedef void (*ProcHandle)();

/* --- enable argument checking in Microsoft's include files */
#define LINT_ARGS

/*-----------------------------------------------------------------*/

#ifndef NULL
#define NULL	0
#endif

#ifndef FALSE
#define FALSE	0
#define TRUE	1
#endif

#define FAIL	1
#define SUCCESS	0

#define MAX_INTEGER		32767

#define HIWORD(longValue)	((UWORD)((longValue) >> 16))
#define LOWORD(longValue)	((UWORD)(longValue))

/* --- this depends on the memory model we're using */
/* Within the 64 KB data segment. */
#define NULL_DATAPTR 0
/* #define NULL_PROCPTR ((BYTE far *) NULL) */
#define NULL_PROCPTR ((ProcHandle) NULL)

/* --- separate Microsoft "far" ptrs into their segment & offset parts */
#define FP_SEG(fp) (*((UWORD *)&(fp) + 1))
#define FP_OFF(fp) (*((UWORD *)&(fp)))

/* --- combine segment & offset into a "far" ptr */
#define BYTE_FAR_PTR(seg,off) ((BYTE far *)(((LONG)(seg)<<16)+(off)))
#define WORD_FAR_PTR(seg,off) ((WORD far *)(((LONG)(seg)<<16)+(UWORD)(off)))

/*----------------------------------------------------------------------*/

#define SWAP(a,b)		{ (a) ^= (b); (b) ^= (a); (a) ^= (b); }
#define ORDER(a,b)		if ((a)>(b))   SWAP((a),(b))
#define MAX(a,b)		((a)>(b)?(a):(b))
#define MIN(a,b)		((a)<(b)?(a):(b))
#define ABS(a)			((a)>0?(a):-(a))
#define DIV_ROUND(x, d)	(((x) + ((d) >> 1)) / (d))		/* ASSUME d > 0. */
#define EVEN_UP(a)			( ((a)+1) & ~1 )
	/* Even number >= a. */

#define CHECK_DOS(fn)  if ((fn) == -1) goto doserr

/* ------------------------------------------------------------------------
 * Graphics data structures.
 */

typedef struct {
	WORD x, y;
} POINT;
typedef struct {
	WORD w, h;
} Dims;
typedef struct {
	WORD x, y, w, h;
} Box;

/*
 * kludge for easy access to individual red/green/blue values within a LONG.
 *	NOTE: this depends on 808x byte order being low-high.
 */
typedef struct {
	UBYTE blue, green, red, unused1;
} LONG_RGB;

/* --- Max # colors visible on screen. */
#define MAX_COLORS 256

/* --- Max # planes that a bitmap can have. */
#define MAX_PLANES 4

/* --- BITMAP structure */
typedef struct {
	WORD width;				/* Width in bytes of each row.  Must be even. */
	Box box;				/* Pixel bounds of bitmap.  (x,y == 0.) */
	WORD planes;			/* Number of planes. */
	UWORD seg[MAX_PLANES];	/* SEG where each plane is allocated. */
} BITMAP;

typedef struct {
	BITMAP *bitmap;
	WORD nPlanes;		/* used only in FMT_ONLY mode */
	UBYTE xpcolor;
} MaskBM;

typedef struct {
    UWORD w, h;         /* raster width & height in pixels */
    UBYTE nPlanes;      /* # source bitplanes */
} BitMapHeader;

/* --------------------------------------------------------------------------
 * Description of screen format "f" (320 x 200, 256-colors).
 */
#define ppb			1	/* pixels per byte */
#define ppb_minus1	0	/* ppb - 1 */
#define ppb_shift	0	/* 1 << ppb_shift == ppb */
#define bpp			8	/* bits per pixel */
#define bpp_minus1	7	/* bpp - 1 */
#define bpp_shift	3	/* 1 << bpp_shift == bpp */
#define curDepth	1	/* Single plane in byte-per-pixel format. */
#define white_color	0xff	/* register # for default white. */

/* -----------------------------------------------------------------------
 * Number of chars in an MS-DOS filename or directory (path) name.
 */
#define FILENAME_ROOT_LENGTH	8
#define FILENAME_SUFFIX_LENGTH	3
#define SEQUENCE_ROOT_LENGTH  4	/* XXXX in XXXX0001.PCX, XXXX0002.PCX...*/
#define SEQUENCE_DIGITS_LENGTH  (FILENAME_ROOT_LENGTH - SEQUENCE_ROOT_LENGTH)
#define FILENAME_LENGTH 12		/* ROOT + '.' + SUFFIX */
#define PATHNAME_LENGTH 66		/* drive + path (no filename) */

/* -----------------------------------------------------------------------
 * Screen Format "f".
 */
#define N_COLUMNS	320
#define N_LINES		200
#define N_BYTES_IN_BITMAP (N_COLUMNS * N_LINES)

/* -----------------------------------------------------------------------
 * Buffered i/o.
 */
void OpenIOBuffer(BOOL forWriting);
void CloseIOBuffer(WORD file);
WORD GFlush(WORD file);
LONG GSeek(WORD file, LONG count, WORD seekType);
WORD GRead(WORD file, UBYTE *buffer, UWORD nBytes);
WORD GWrite(WORD file, UBYTE *buffer, UWORD nBytes);

/* -----------------------------------------------------------------------
 * Procedures that need to be declared so parameters or return values of
 * correct type.
 */
WORD far *BMLineStart(BITMAP*, WORD, WORD);

/* --- mnemonic names for Microsoft library functions */
#define movmem(source,dest,length) memmove(dest,source,length)
#define setmem(ptr,length,value) memset(ptr,value,length)
