#include <stdio.h>
#include "compiler.h"

main (argc, argv)
int	argc;
char	*argv[];
{
    FILE		*infp, *outfp;
    COUNT		bytes_read;
    DWORD		filelen, halflen;
    static char	buf[512];

    infp = fopen (argv[1], "rb");

    ffilelength (infp, &filelen);
    halflen = filelen >> 1;
    filelen -= halflen;

    outfp = fopen (argv[2], "wb");
    while (halflen)
    {
	bytes_read = halflen >= sizeof (buf) ?
		sizeof (buf) : (COUNT)halflen;
	fwrite (buf, 1, fread (buf, 1, bytes_read, infp), outfp);
	halflen -= bytes_read;
    }
    fclose (outfp);

    outfp = fopen (argv[3], "wb");
    while (filelen)
    {
	bytes_read = filelen >= sizeof (buf) ?
		sizeof (buf) : (COUNT)filelen;
	fwrite (buf, 1, fread (buf, 1, bytes_read, infp), outfp);
	filelen -= bytes_read;
    }
    fclose (outfp);

    fclose (infp);
}
