/**************************************************************************
    lzhuf.c
    written by Haruyasu Yoshizaki 1988/11/20
    some minor changes 1989/04/06
    comments translated by Haruhiko Okumura 1989/04/07
    getbit and getbyte modified 1990/03/23 by Paul Edwards
      so that they would work on machines where integers are
      not necessarily 16 bits (although ANSI guarantees a
      minimum of 16).  This program has compiled and run with
      no errors under Turbo C 2.0, Power C, and SAS/C 4.5
      (running on an IBM mainframe under MVS/XA 2.2).  Could
      people please use YYYY/MM/DD date format so that everyone
      in the world can know what format the date is in?
    external storage of filesize changed 1990/04/18 by Paul Edwards to
      Intel's "little endian" rather than a machine-dependant style so
      that files produced on one machine with lzhuf can be decoded on
      any other.  "little endian" style was chosen since lzhuf
      originated on PC's, and therefore they should dictate the
      standard.
    initialization of something predicting spaces changed 1990/04/22 by
      Paul Edwards so that when the compressed file is taken somewhere
      else, it will decode properly, without changing ascii spaces to
      ebcdic spaces.  This was done by changing the ' ' (space literal)
      to 0x20 (which is the far most likely character to occur, if you
      don't know what environment it will be running on.
*****************************************************************************
    This code was found at http://www.cs.umu.se/~isak/Snippets
	with the following Copyright notice. A lot information in the notice
	is quite outdated by today (at least FidoNet is pretty dead).
*****************************************************************************
Welcome to SNIPPETS!

  All the code I put into SNIPPETS for distribution is Public Domain or free
to the best that I can determine. What this means is that:

1.  I know or can contact the original author(s) to verify the presumed
    copyright ownership, and

2.  The work bears an explicit Public Domain notice, or

3.  The work is copyrighted but includes a free use license, or

4.  The work was published without a copyright notice prior to the effective
    date of the new copyright law.

  This has been occasionally annoying when I've had to pass up some useful
piece of code because it's questionable whether anyone can use it without
incurring liability (distributing someone else's property makes me an
accessory, doncha know).

  Since SNIPPETS includes both public domain and free code, be sure to
carefully read each header for any free license restrictions which may
apply.

Distribution:

  Starting with the December 1992 version, SNIPPETS is distributed in two
files:

SNIPmmyy.LZH is the full SNIPPETS collection.
SNPDmmyy.LZH contains only the files changes since the last release.

  SNIPPETS is distributed through the FidoNet Programmer's Distribution
Network (PDN - see the file PDN.LST for a list of PDN sites and further
information. The SNIPPETS files are also available from my "home" BBS, Comm
Port One, (713) 980-9671, FidoNet address 1:106/2000 using the "magic" F'req
names of "SNIPPETS" and "SNIPDIFF". Various Internet mirror sites also carry
SNIPPETS, but I'm not sure which ones have it an any given time. One place to
try is oak.oakland.edu in /pub/msdos/c.

                                                        ...Bob Stout
-------------------------------  Enjoy!  -----------------------------------
*****************************************************************************

    made into lib-like reentrant by Alex Volkov 20050208

*****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "lzhuf.h"

static size_t h_read(lzhuf_session_t* sess, void* buf, size_t size, size_t count)
{
	if (!sess->read)
		return 0;
	return sess->read(buf, size, count, sess->infile);
}

static size_t h_write(lzhuf_session_t* sess, const void* buf, size_t size, size_t count)
{
	if (!sess->write)
		return 0;
	return sess->write(buf, size, count, sess->outfile);
}

static int h_putc(lzhuf_session_t* sess, unsigned char c)
{
	return h_write(sess, &c, sizeof(c), 1) == 1 ? c : EOF;
}

static int h_getc(lzhuf_session_t* sess)
{
	unsigned char c;
	return h_read(sess, &c, sizeof(c), 1) == 1 ? c : EOF;
}

static int h_seek(lzhuf_session_t* sess, lzhuf_handle_t h, long ofs, int origin)
{
	if (!sess->seek)
		return -1;
	return sess->seek(h, ofs, origin);
}

static long h_tell(lzhuf_session_t* sess, lzhuf_handle_t h)
{
	if (!sess->tell)
		return -1;
	return sess->tell(h);
}

static void InitTree(lzhuf_session_t* sess)  /* initialize trees */
{
    int  i;

    for (i = LZHUF_N + 1; i <= LZHUF_N + 256; i++)
        sess->rson[i] = LZHUF_NIL;        /* root */
    for (i = 0; i < LZHUF_N; i++)
        sess->dad[i] = LZHUF_NIL;         /* node */
}

static void InsertNode(lzhuf_session_t* sess, int r)  /* insert to tree */
{
    int  i, p, cmp;
    unsigned char  *key;
    unsigned c;

    cmp = 1;
    key = &sess->text_buf[r];
    p = LZHUF_N + 1 + key[0];
    sess->rson[r] = sess->lson[r] = LZHUF_NIL;
    sess->match_length = 0;
    for ( ; ; ) {
        if (cmp >= 0) {
            if (sess->rson[p] != LZHUF_NIL)
                p = sess->rson[p];
            else {
                sess->rson[p] = r;
                sess->dad[r] = p;
                return;
            }
        } else {
            if (sess->lson[p] != LZHUF_NIL)
                p = sess->lson[p];
            else {
                sess->lson[p] = r;
                sess->dad[r] = p;
                return;
            }
        }
        for (i = 1; i < LZHUF_F; i++)
            if ((cmp = key[i] - sess->text_buf[p + i]) != 0)
                break;
        if (i > LZHUF_THRESHOLD) {
            if (i > sess->match_length) {
                sess->match_position = ((r - p) & (LZHUF_N - 1)) - 1;
                if ((sess->match_length = i) >= LZHUF_F)
                    break;
            }
            if (i == sess->match_length) {
                if ((c = ((r - p) & (LZHUF_N-1)) - 1) < (unsigned)sess->match_position) {
                    sess->match_position = c;
                }
            }
        }
    }
    sess->dad[r] = sess->dad[p];
    sess->lson[r] = sess->lson[p];
    sess->rson[r] = sess->rson[p];
    sess->dad[sess->lson[p]] = r;
    sess->dad[sess->rson[p]] = r;
    if (sess->rson[sess->dad[p]] == p)
        sess->rson[sess->dad[p]] = r;
    else
        sess->lson[sess->dad[p]] = r;
    sess->dad[p] = LZHUF_NIL; /* remove p */
}

static void DeleteNode(lzhuf_session_t* sess, int p)  /* remove from tree */
{
    int  q;

    if (sess->dad[p] == LZHUF_NIL)
        return;         /* not registered */
    if (sess->rson[p] == LZHUF_NIL)
        q = sess->lson[p];
    else
    if (sess->lson[p] == LZHUF_NIL)
        q = sess->rson[p];
    else {
        q = sess->lson[p];
        if (sess->rson[q] != LZHUF_NIL) {
            do {
                q = sess->rson[q];
            } while (sess->rson[q] != LZHUF_NIL);
            sess->rson[sess->dad[q]] = sess->lson[q];
            sess->dad[sess->lson[q]] = sess->dad[q];
            sess->lson[q] = sess->lson[p];
            sess->dad[sess->lson[p]] = q;
        }
        sess->rson[q] = sess->rson[p];
        sess->dad[sess->rson[p]] = q;
    }
    sess->dad[q] = sess->dad[p];
    if (sess->rson[sess->dad[p]] == p)
        sess->rson[sess->dad[p]] = q;
    else
        sess->lson[sess->dad[p]] = q;
    sess->dad[p] = LZHUF_NIL;
}


/* table for encoding and decoding the upper 6 bits of position */

/* for encoding */
static const unsigned char p_len[64] = {
    0x03, 0x04, 0x04, 0x04, 0x05, 0x05, 0x05, 0x05,
    0x05, 0x05, 0x05, 0x05, 0x06, 0x06, 0x06, 0x06,
    0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08
};

static const unsigned char p_code[64] = {
    0x00, 0x20, 0x30, 0x40, 0x50, 0x58, 0x60, 0x68,
    0x70, 0x78, 0x80, 0x88, 0x90, 0x94, 0x98, 0x9C,
    0xA0, 0xA4, 0xA8, 0xAC, 0xB0, 0xB4, 0xB8, 0xBC,
    0xC0, 0xC2, 0xC4, 0xC6, 0xC8, 0xCA, 0xCC, 0xCE,
    0xD0, 0xD2, 0xD4, 0xD6, 0xD8, 0xDA, 0xDC, 0xDE,
    0xE0, 0xE2, 0xE4, 0xE6, 0xE8, 0xEA, 0xEC, 0xEE,
    0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7,
    0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};

/* for decoding */
static const unsigned char d_code[256] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
    0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
    0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A,
    0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B,
    0x0C, 0x0C, 0x0C, 0x0C, 0x0D, 0x0D, 0x0D, 0x0D,
    0x0E, 0x0E, 0x0E, 0x0E, 0x0F, 0x0F, 0x0F, 0x0F,
    0x10, 0x10, 0x10, 0x10, 0x11, 0x11, 0x11, 0x11,
    0x12, 0x12, 0x12, 0x12, 0x13, 0x13, 0x13, 0x13,
    0x14, 0x14, 0x14, 0x14, 0x15, 0x15, 0x15, 0x15,
    0x16, 0x16, 0x16, 0x16, 0x17, 0x17, 0x17, 0x17,
    0x18, 0x18, 0x19, 0x19, 0x1A, 0x1A, 0x1B, 0x1B,
    0x1C, 0x1C, 0x1D, 0x1D, 0x1E, 0x1E, 0x1F, 0x1F,
    0x20, 0x20, 0x21, 0x21, 0x22, 0x22, 0x23, 0x23,
    0x24, 0x24, 0x25, 0x25, 0x26, 0x26, 0x27, 0x27,
    0x28, 0x28, 0x29, 0x29, 0x2A, 0x2A, 0x2B, 0x2B,
    0x2C, 0x2C, 0x2D, 0x2D, 0x2E, 0x2E, 0x2F, 0x2F,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
    0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
};

static const unsigned char d_len[256] = {
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
    0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
    0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
    0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
    0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
    0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
};


static int GetBit(lzhuf_session_t* sess)    /* get one bit */
{
    unsigned i;

    while (sess->getlen <= 8) {
        if ((int)(i = h_getc(sess)) < 0)
			i = 0;
        sess->getbuf |= i << (8 - sess->getlen);
        sess->getlen += 8;
    }
    i = sess->getbuf;
    sess->getbuf <<= 1;
    sess->getlen--;
    return (int)((i & 0x8000) >> 15);
}

static int GetByte(lzhuf_session_t* sess)   /* get one byte */
{
    unsigned i;

    while (sess->getlen <= 8) {
        if ((int)(i = h_getc(sess)) < 0)
			i = 0;
        sess->getbuf |= i << (8 - sess->getlen);
        sess->getlen += 8;
    }
    i = sess->getbuf;
    sess->getbuf <<= 8;
    sess->getlen -= 8;
    return (int)((i & 0xff00) >> 8);
}

static int Putcode(lzhuf_session_t* sess, int l, unsigned c)     /* output c bits of code */
{
    sess->putbuf |= c >> sess->putlen;
    if ((sess->putlen += l) >= 8) {
        if (h_putc(sess, (unsigned char)(sess->putbuf >> 8)) == EOF)
            return -1;
        if ((sess->putlen -= 8) >= 8) {
            if (h_putc(sess, (unsigned char)sess->putbuf) == EOF)
                return -1;
            sess->codesize += 2;
            sess->putlen -= 8;
            sess->putbuf = c << (l - sess->putlen);
        } else {
            sess->putbuf <<= 8;
            sess->codesize++;
        }
    }
	return 0;
}


/* initialization of tree */

static void StartHuff(lzhuf_session_t* sess)
{
    int i, j;

    for (i = 0; i < LZHUF_N_CHAR; i++) {
        sess->freq[i] = 1;
        sess->son[i] = i + LZHUF_T;
        sess->prnt[i + LZHUF_T] = i;
    }
    i = 0; j = LZHUF_N_CHAR;
    while (j <= LZHUF_R) {
        sess->freq[j] = sess->freq[i] + sess->freq[i + 1];
        sess->son[j] = i;
        sess->prnt[i] = sess->prnt[i + 1] = j;
        i += 2; j++;
    }
    sess->freq[LZHUF_T] = 0xffff;
    sess->prnt[LZHUF_R] = 0;
}


/* reconstruction of tree */

static void reconst(lzhuf_session_t* sess)
{
    int i, j, k;
    unsigned f, l;

    /* collect leaf nodes in the first half of the table */
    /* and replace the freq by (freq + 1) / 2. */
    j = 0;
    for (i = 0; i < LZHUF_T; i++) {
        if (sess->son[i] >= LZHUF_T) {
            sess->freq[j] = (sess->freq[i] + 1) / 2;
            sess->son[j] = sess->son[i];
            j++;
        }
    }
    /* begin constructing tree by connecting sons */
    for (i = 0, j = LZHUF_N_CHAR; j < LZHUF_T; i += 2, j++) {
        k = i + 1;
        f = sess->freq[j] = sess->freq[i] + sess->freq[k];
        for (k = j - 1; f < sess->freq[k]; k--)
			;
        k++;
        l = j - k;
        memmove(sess->freq + k + 1, sess->freq + k, l * sizeof(sess->freq[0]));
        sess->freq[k] = f;
        memmove(sess->son + k + 1, sess->son + k, l * sizeof(sess->son[0]));
        sess->son[k] = i;
    }
    /* connect prnt */
    for (i = 0; i < LZHUF_T; i++) {
        if ((k = sess->son[i]) >= LZHUF_T) {
            sess->prnt[k] = i;
        } else {
            sess->prnt[k] = sess->prnt[k + 1] = i;
        }
    }
}


/* increment frequency of given code by one, and update tree */

static void update(lzhuf_session_t* sess, int c)
{
    int i, j, k, l;

    if (sess->freq[LZHUF_R] == LZHUF_MAX_FREQ) {
        reconst(sess);
    }
    c = sess->prnt[c + LZHUF_T];
    do {
        k = ++sess->freq[c];

        /* if the order is disturbed, exchange nodes */
        if ((unsigned)k > sess->freq[l = c + 1]) {
            while ((unsigned)k > sess->freq[++l])
				;
            l--;
            sess->freq[c] = sess->freq[l];
            sess->freq[l] = k;

            i = sess->son[c];
            sess->prnt[i] = l;
            if (i < LZHUF_T)
				sess->prnt[i + 1] = l;

            j = sess->son[l];
            sess->son[l] = i;

            sess->prnt[j] = c;
            if (j < LZHUF_T)
				sess->prnt[j + 1] = c;
            sess->son[c] = j;

            c = l;
        }
    } while ((c = sess->prnt[c]) != 0); /* repeat up to root */
}


static void EncodeChar(lzhuf_session_t* sess, unsigned c)
{
    unsigned i;
    int j, k;

    i = 0;
    j = 0;
    k = sess->prnt[c + LZHUF_T];

    /* travel from leaf to root */
    do {
        i >>= 1;

        /* if node's address is odd-numbered, choose bigger brother node */
        if (k & 1)
			i += 0x8000;

        j++;
    } while ((k = sess->prnt[k]) != LZHUF_R);
    Putcode(sess, j, (i & 0xffff));
    update(sess, c);
}

static void EncodePosition(lzhuf_session_t* sess, unsigned c)
{
    unsigned i;

    /* output upper 6 bits by table lookup */
    i = c >> 6;
    Putcode(sess, p_len[i], (unsigned)p_code[i] << 8);

    /* output lower 6 bits verbatim */
    Putcode(sess, 6, (c & 0x3f) << 10);
}

static int EncodeEnd(lzhuf_session_t* sess)
{
    if (sess->putlen) {
        if (h_putc(sess, (unsigned char)(sess->putbuf >> 8)) == EOF)
            return -1;
        sess->codesize++;
    }
	return 0;
}

static int DecodeChar(lzhuf_session_t* sess)
{
    unsigned c;

    c = sess->son[LZHUF_R];

    /* travel from root to leaf, */
    /* choosing the smaller child node (son[]) if the read bit is 0, */
    /* the bigger (son[]+1} if 1 */
    while (c < LZHUF_T) {
        c += GetBit(sess);
        c = sess->son[c];
    }
    c -= LZHUF_T;
    update(sess, c);
    return (int)c;
}

static int DecodePosition(lzhuf_session_t* sess)
{
    unsigned i, j, c;

    /* recover upper 6 bits from table */
    i = GetByte(sess);
    c = (unsigned)d_code[i] << 6;
    j = d_len[i];

    /* read lower 6 bits verbatim */
    j -= 2;
    while (j--) {
        i = (i << 1) + GetBit(sess);
    }
    return (int)(c | (i & 0x3f));
}

/* compression */

int lzhuf_encode(lzhuf_session_t* sess)  /* compression */
{
    int  i, c, len, r, s, last_match_length;
	unsigned long int  textsize = 0;
	int ret;

    h_seek(sess, sess->infile, 0L, SEEK_END);
    textsize = h_tell(sess, sess->infile);
    
	if (h_putc(sess, (unsigned char)((textsize & 0xff))) == EOF ||
		h_putc(sess, (unsigned char)((textsize & 0xff00) >> 8)) == EOF ||
		h_putc(sess, (unsigned char)((textsize & 0xff0000L) >> 16)) == EOF ||
		h_putc(sess, (unsigned char)((textsize & 0xff000000L) >> 24)) == EOF)
		return -1;
    if (textsize == 0)
        return 0;
    h_seek(sess, sess->infile, 0L, SEEK_SET);
    textsize = 0;           /* rewind and re-read */
    StartHuff(sess);
    InitTree(sess);
    s = 0;
    r = LZHUF_N - LZHUF_F;
    for (i = s; i < r; i++)
        sess->text_buf[i] = 0x20;
    for (len = 0; len < LZHUF_F && (c = h_getc(sess)) != EOF; len++)
        sess->text_buf[r + len] = (unsigned char)c;
    textsize = len;
    for (i = 1; i <= LZHUF_F; i++)
        InsertNode(sess, r - i);
    InsertNode(sess, r);
    do {
        if (sess->match_length > len)
            sess->match_length = len;
        if (sess->match_length <= LZHUF_THRESHOLD) {
            sess->match_length = 1;
            EncodeChar(sess, sess->text_buf[r]);
        } else {
            EncodeChar(sess, 255 - LZHUF_THRESHOLD + sess->match_length);
            EncodePosition(sess, sess->match_position);
        }
        last_match_length = sess->match_length;
        for (i = 0; i < last_match_length && (c = h_getc(sess)) != EOF; i++) {
            DeleteNode(sess, s);
            sess->text_buf[s] = (unsigned char)c;
            if (s < LZHUF_F - 1)
                sess->text_buf[s + LZHUF_N] = (unsigned char)c;
            s = (s + 1) & (LZHUF_N - 1);
            r = (r + 1) & (LZHUF_N - 1);
            InsertNode(sess, r);
        }
        textsize += i;
        while (i++ < last_match_length) {
            DeleteNode(sess, s);
            s = (s + 1) & (LZHUF_N - 1);
            r = (r + 1) & (LZHUF_N - 1);
            if (--len)
				InsertNode(sess, r);
        }
    } while (len > 0);
    
	ret = EncodeEnd(sess);

	return ret;
}

int lzhuf_decode(lzhuf_session_t* sess)  /* recover */
{
    int  i, j, k, r, c;
    unsigned long int  count;
	unsigned long int  textsize = 0;

    if (EOF == (c = h_getc(sess)))
		return -1;
	textsize = c;
    if (EOF == (c = h_getc(sess)))
		return -1;
    textsize |= (c << 8);
    if (EOF == (c = h_getc(sess)))
		return -1;
    textsize |= (c << 16);
    if (EOF == (c = h_getc(sess)))
		return -1;
    textsize |= (c << 24);
    if (textsize == 0)
        return 0;
    StartHuff(sess);
    for (i = 0; i < LZHUF_N - LZHUF_F; i++)
        sess->text_buf[i] = 0x20;
    r = LZHUF_N - LZHUF_F;
    for (count = 0; count < textsize; ) {
        c = DecodeChar(sess);
        if (c < 256) {
            if (h_putc(sess, (unsigned char)c) == EOF)
                return -1;
            sess->text_buf[r++] = (unsigned char)c;
            r &= (LZHUF_N - 1);
            count++;
        } else {
            i = (r - DecodePosition(sess) - 1) & (LZHUF_N - 1);
            j = c - 255 + LZHUF_THRESHOLD;
            for (k = 0; k < j; k++) {
                c = sess->text_buf[(i + k) & (LZHUF_N - 1)];
                if (h_putc(sess, (unsigned char)c) == EOF)
                    return -1;
                sess->text_buf[r++] = (unsigned char)c;
                r &= (LZHUF_N - 1);
                count++;
            }
        }
    }
	return 0;
}

void lzhuf_startSession(lzhuf_session_t* sess, int bStdIO)
{
	memset(sess, 0, sizeof(*sess));
	if (bStdIO)
	{
		sess->read = fread;
		sess->write = fwrite;
		sess->seek = fseek;
		sess->tell = ftell;
	}
}

void lzhuf_endSession(lzhuf_session_t* sess)
{
	// do nothing
	(void)sess;
}
