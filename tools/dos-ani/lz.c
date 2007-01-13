/*************************************************************************
  LZ packer/unpacker

  Code is mostly from lzhuf.c found at
    http://www.cs.umu.se/~isak/Snippets

***********Original Comment***********************************************
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
*************************************************************************/

//#include <sys/types.h>
//#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
//#include <stdarg.h>
#include <string.h>
#include <ctype.h>
//#include <errno.h>
#include <malloc.h>
#if defined(WIN32) && defined(_MSC_VER)
#	define inline __inline
#elif defined(__GNUC__)
#	define inline __inline__
#endif 
#include "lzhuf.h"


int main(int argc, char *argv[])
{
	char* s;
	lzhuf_session_t sess;
	FILE* infile;
	FILE* outfile;
	int ret;

	if (argc != 4)
	{
		fprintf(stderr,
				"'lz e file1 file2' encodes file1 into file2.\n"
				"'lz d file2 file1' decodes file2 into file1.\n");
		return EXIT_FAILURE;
	}
	
	if ((s = argv[1], s[1] || strchr("DEde", s[0]) == NULL)
		|| (s = argv[2], (infile = fopen(s, "rb")) == NULL)
		|| (s = argv[3], (outfile = fopen(s, "wb")) == NULL))
	{
		fprintf(stderr, "??? %s\n", s);
		return EXIT_FAILURE;
	}

	lzhuf_startSession(&sess, LZHUF_STDIO);
	sess.infile = infile;
	sess.outfile = outfile;
	
	if (toupper(*argv[1]) == 'E')
		ret = lzhuf_encode(&sess);
	else
		ret = lzhuf_decode(&sess);
	
	lzhuf_endSession(&sess);

	if (ret != 0)
		return EXIT_FAILURE;

	fclose(infile);
	fclose(outfile);
	
	return EXIT_SUCCESS;
}
