#include <stdio.h>
#include "compiler.h"

main (argc, argv)
int	argc;
char	*argv[];
{
    FILE		*infp, *outfp;
    COUNT		bytes_read;
    DWORD		filelen;
    static char	buf[512];

    outfp = fopen (argv[1], "wb");

    infp = fopen (argv[2], BUFFERED_BINREAD);
    ffilelength (infp, &filelen);
    while (filelen)
    {
	bytes_read = filelen >= sizeof (buf) ?
		sizeof (buf) : (COUNT)filelen;
	fwrite (buf, 1, fread (buf, 1, bytes_read, infp), outfp);
	filelen -= bytes_read;
    }
    fclose (infp);

    infp = fopen (argv[3], BUFFERED_BINREAD);
    ffilelength (infp, &filelen);
    while (filelen)
    {
	bytes_read = filelen >= sizeof (buf) ?
		sizeof (buf) : (COUNT)filelen;
	fwrite (buf, 1, fread (buf, 1, bytes_read, infp), outfp);
	filelen -= bytes_read;
    }
    fclose (infp);

    fclose (outfp);
}
