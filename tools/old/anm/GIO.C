/*--gio.c-----------------------------------------------------------------
 * Buffered i/o.
 */

#include "main.h"
#include <io.h>
#include <stdio.h>

#define	local static

/* --- Segment of optional buffer for speeding-up i/o. */
local UWORD ioBufferSeg = NULL;
local UWORD ioBufSize = 0, ioBufNext, ioBufLimit;
local BOOL ioWriting;

/* ---------- OpenIOBuffer ---------- */
void OpenIOBuffer(BOOL forWriting)
{
	register WORD bufSize;
	auto WORD tempSeg;

	ioWriting = forWriting;

	/* --- Allocate i/o buffer. */
	if (ioBufferSeg == NULL) {
		for (bufSize = 16 * 1024; bufSize >= 512; bufSize >>= 1) {
			ioBufferSeg = DAlloc(bufSize);
			if (ioBufferSeg) {
				/* --- Leave at least 4 KB free for other uses. */
				tempSeg = DAlloc(4096);
				if (tempSeg) {
					tempSeg = SFree(tempSeg);
					break;		/* Success; leave loop. */
				} else
					ioBufferSeg = SFree(ioBufferSeg);
			}
		}
		if (ioBufferSeg != NULL)
			ioBufSize = bufSize;
	}
	ioBufNext = ioBufLimit = 0;
		/* Reading: When next==limit, buffer is empty.
		 * Writing: When next== 0, buffer is empty.
		 */
}

/* ---------- CloseIOBuffer -----------------
 * Writing: flush any remaining data to file.
 * Free the i/o buffer.  Do this whenever finish i/o, so RAM not tied up.
 */
void CloseIOBuffer(WORD file)
{
	if (ioWriting)
		GFlush(file);	/* TBD: ignoring error. */
	ioBufferSeg = SFree(ioBufferSeg);
	ioBufSize = 0;
}

/* ---------- GRead ---------------------------------------------
 * Like DOS "read" function, RETURNS # bytes read, or 0 at EOF,
 * or -1 at error.
 */
WORD GRead(WORD file, UBYTE *buffer, UWORD nBytes)
{
	UWORD nRemaining, nAvail, nThisTime;

	if (ioBufferSeg) {
		nAvail = ioBufLimit - ioBufNext;
		if (!nAvail && nBytes >= ioBufSize)
			goto readDirectly;	/* More efficient to read buffer directly. */

		/* --- Remaining # bytes that need reading. */
		for (nRemaining = nBytes;  nRemaining;  nRemaining -= nThisTime) {
			if (!nAvail) {
				ioBufLimit = nAvail =
					readdos(file, ioBufferSeg, 0, ioBufSize);
				if (nAvail == 0xffff || nAvail == 0) {
					ioBufLimit = 0;
					return (nAvail);
				}
				ioBufNext = 0;
			}
			nThisTime = MIN(nRemaining, nAvail);
			far_movmem(ioBufferSeg, ioBufNext,
					   dataseg(), (UWORD)buffer,
					   (UWORD)nThisTime);
			buffer += nThisTime;
			ioBufNext += nThisTime;
			nAvail -= nThisTime;
		}
		return (nBytes);
	} else						/* No ioBuf */
readDirectly:
		return (read(file, buffer, nBytes));
}

/* --------------- GReadOne --------------------------------
 * Like "GRead", but read a single byte, with less overhead.
 */
WORD GReadOne(WORD file, UBYTE *buffer)
{
	if (!ioBufferSeg || ioBufLimit == ioBufNext)
		return GRead(file, buffer, 1);

	*buffer = *BYTE_FAR_PTR(ioBufferSeg, ioBufNext);
	ioBufNext++;
	return (1);
}

/* ---------- GFlush ------------------------------
 * Write out any bytes remaining in the buffer.
 * Return what writedos does, or 0 if nothing to do.
 */
WORD GFlush(WORD file)
{
	auto WORD result = 0;

	if (ioBufferSeg && ioBufNext && ioWriting) {
		result = writedos(file, ioBufferSeg, 0, ioBufNext);
		if (result < ioBufNext)
			result = -1;	/* Disk must be full, turn it into an error. */
		ioBufNext = 0;		/* Writing: buffer is empty. */
	}
	return (result);
}

/* ---------- GSeek ----------------------------------------------------
 * When reading, ASSUMES seeking forwards.
 * When writing, flush buffer and use lseek.
 * RETURNS 0 if succeeded in seeking w/i buffer.  DIFFERENT THAN LSEEK.
 */
LONG GSeek(WORD file, LONG count, WORD seekType)
{
	UWORD nAvail;

	if (ioBufferSeg) {
		if (ioWriting && ioBufNext) {
			GFlush(file);		/* TBD: Ignoring error code. */
			goto seekDirectly;
		}
		nAvail = ioBufLimit - ioBufNext;

		if (seekType != SEEK_CUR) {		/* Check for absolute positioning */
			goto emptyAndSeekDirectly;
		}
		if (count > nAvail) {
			count -= nAvail;   /* Adjust seek for bytes already read ahead. */
emptyAndSeekDirectly:
			ioBufNext = ioBufLimit;		/* Reading: buffer is empty. */
			goto seekDirectly;
		}
		ioBufNext += (UWORD)count;		/* Seek within the buffer. */
		return (0);				/* DIFFERENT THAN LSEEK */
	} else
seekDirectly:
		return lseek(file, count, seekType);
}

/* ---------- GWrite ---------------------------------------------
 * Like DOS "write" function, RETURNS # bytes written, or -1 at error.
 * DIFFERENCE: if can't write them all, returns -1 instead of # written,
 * UNLESS "writeDirectly", which just uses write().
 */
WORD GWrite(WORD file, UBYTE * buffer, UWORD nBytes)
{
	UWORD nRemaining, nUnused, nThisTime, nWritten;

	if (ioBufferSeg) {
		if (!ioBufNext && nBytes >= ioBufSize)
			goto writeDirectly;	/* More efficient to write buffer directly. */

		nUnused = ioBufSize - ioBufNext;	/* # bytes unused in buffer. */

		/* Remaining # bytes that need writing. */
		for (nRemaining = nBytes; nRemaining; nRemaining -= nThisTime) {
			if (!nUnused) {
				nWritten = writedos(file, ioBufferSeg, 0, ioBufNext);
				if (nWritten != ioBufNext) {
					ioBufNext = 0;		/* Flush the data -- it is lost. */
					return (-1);		/* ERROR */
				}
				ioBufNext = 0;
				nUnused = ioBufSize;
			}
			nThisTime = MIN(nRemaining, nUnused);
			far_movmem(dataseg(),
					   (UWORD)buffer, ioBufferSeg,
					   ioBufNext, (UWORD)nThisTime);
			buffer += nThisTime;
			ioBufNext += nThisTime;
			nUnused -= nThisTime;
		}
		return (nBytes);		/* OKAY */
	} else						/* No ioBuf */
writeDirectly:
		return (write(file, buffer, nBytes));
}
