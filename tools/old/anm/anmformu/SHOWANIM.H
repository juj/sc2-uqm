/* structure declarations for deluxe animate large page files */


typedef struct {

 ULONG id;		/* 4 character ID == "LPF " */

 UWORD maxLps;		/* max # largePages allowed. 256 FOR NOW.   */

 UWORD nLps;		/* # largePages in this file. */

 ULONG nRecords;	/* # records in this file.  65534 is current limit plus */
			/* one for last-to-first delta for looping the animation */

 UWORD maxRecsPerLp;	/* # records permitted in an lp. 256 FOR NOW.   */

 UWORD lpfTableOffset;	/* Absolute Seek position of lpfTable.  1280 FOR NOW.
			The lpf Table is an array of 256 large page structures
			that is used to facilitate finding records in an anim
			file without having to seek through all of the Large
 			Pages to find which one a specific record lives in. */

 ULONG contentType;	/* 4 character ID == "ANIM" */

 UWORD width;		/* Width of screen in pixels. */
 UWORD height;		/* Height of screen in pixels. */
 UBYTE variant;	/* 	0==ANIM. */
 UBYTE version;	/* 	0==frame rate is multiple of 18 cycles/sec.
 			1==frame rate is multiple of 70 cycles/sec.  */

 UBYTE hasLastDelta;	/* 1==Last record is a delta from last-to-first frame. */

 UBYTE lastDeltaValid;	/* 0==The last-to-first delta (if present) hasn't been
			updated to match the current first&last frames,	so it
 			should be ignored. */

 UBYTE pixelType;	/* 0==256 color. */

 UBYTE CompressionType;	/* 1==(RunSkipDump) Only one used FOR NOW. */

 UBYTE otherRecsPerFrm;	/* 0 FOR NOW. */

 UBYTE bitmaptype;  /* 	1==320x200, 256-color.  Only one implemented so far. */

 UBYTE recordTypes[32];	/* Not yet implemented. */

 ULONG nFrames;	 /* 	In case future version adds other records at end of 
			file, we still know how many actual frames.
 			NOTE: DOES include last-to-first delta when present. */

 UWORD framesPerSecond;	/* Number of frames to play per second. */

 UWORD pad2[29];	/* 58 bytes of filler to round up to 128 bytes total. */
} lpfileheader;


/* this is the format of an large page structure */
typedef struct {
 UWORD baseRecord;	/* Number of first record in this large page. */

 UWORD nRecords;	/* Number of records in lp.
						bit 15 of "nRecords" == "has continuation from previous lp".
   	 				bit 14 of "nRecords" == "final record continues on next lp". */

 UWORD nBytes;		/* Total number of bytes of contents, excluding header. */

} lp_descriptor;


