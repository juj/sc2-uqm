/**************************************************************
    lzhuf.h

	Lib-like reentrant definition of lzhuf.c
	Please see lzhuf.c for authorship info
**************************************************************/

#ifndef LZHUF_H_INCL_
#define LZHUF_H_INCL_

/********** LZSS compression **********/

#define LZHUF_N         4096    /* buffer size */
#define LZHUF_F         60  /* lookahead buffer size */
#define LZHUF_THRESHOLD 2
#define LZHUF_NIL     LZHUF_N   /* leaf of tree */

/* Huffman coding */

#define LZHUF_N_CHAR      (256 - LZHUF_THRESHOLD + LZHUF_F)
                /* kinds of characters (character code = 0..LZHUF_N_CHAR-1) */
#define LZHUF_T       (LZHUF_N_CHAR * 2 - 1)    /* size of table */
#define LZHUF_R       (LZHUF_T - 1)         /* position of root */
#define LZHUF_MAX_FREQ    0x8000      /* updates tree when the */


typedef void* lzhuf_handle_t;

typedef struct
{
	unsigned long int codesize;
	unsigned char text_buf[LZHUF_N + LZHUF_F - 1];
	int match_position;
	int match_length;
	int lson[LZHUF_N + 1];
	int rson[LZHUF_N + 257];
	int dad[LZHUF_N + 1];

	unsigned freq[LZHUF_T + 1]; /* frequency table */

	int prnt[LZHUF_T + LZHUF_N_CHAR]; /* pointers to parent nodes, except for the */
            /* elements [LZHUF_T..LZHUF_T + LZHUF_N_CHAR - 1] which are used to get */
            /* the positions of leaves corresponding to the codes. */

	int son[LZHUF_T];   /* pointers to child nodes (son[], son[] + 1) */

	unsigned getbuf;
	unsigned char getlen;
	
	unsigned putbuf;
	unsigned char putlen;

	/* user-settable data */
	void* userdata;

	lzhuf_handle_t infile;
	lzhuf_handle_t outfile;

	size_t (* read)(void*, size_t, size_t, lzhuf_handle_t);
	size_t (* write)(const void*, size_t, size_t, lzhuf_handle_t);
	int (* seek)(lzhuf_handle_t, long, int);
	long (* tell)(lzhuf_handle_t);

} lzhuf_session_t;

#define LZHUF_STDIO 1

void lzhuf_startSession(lzhuf_session_t *, int bStdIO);
void lzhuf_endSession(lzhuf_session_t *);
int lzhuf_encode(lzhuf_session_t *);  /* compression */
int lzhuf_decode(lzhuf_session_t *);  /* recover */


#endif /* LZHUF_H_INCL_ */
