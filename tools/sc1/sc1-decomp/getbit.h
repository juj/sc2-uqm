/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _GETBIT_H
#define _GETBIT_H

static inline WORD
haveBit(BitStreamContext *bsc) {
	return BSC_haveBit_OPAB(bsc);
}

static inline WORD
getBit(BitStreamContext *bsc) {
	if (haveBit(bsc))
		return BSC_getBit_OPAB(bsc);

	bsc->eof = true;
	return 0;
}

static inline WORD
haveBits(BitStreamContext *bsc, BYTE count) {
	return BSC_haveBits_OPAB(bsc, count);
}

// On EOF, this outputs garbage; this is how the original TFB code worked.
static inline WORD
getBits(BitStreamContext *bsc, BYTE count) {
	if (haveBits(bsc, count))
		return BSC_getBits_OPAB(bsc, count);

	bsc->eof = true;
	return BSC_getBits_OPAB(bsc, BSC_bitsLeft_OPAB(bsc));
}

#endif  /* _GETBIT_H */

