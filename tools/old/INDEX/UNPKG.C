#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "resource/resintrn.h"

STATIC RES_PACKAGE	res_package;
STATIC COUNT		name_index;
STATIC char		file_name[100][80];
char			workbuf[8 * 1024];

PROC(STATIC
MEM_HANDLE WriteData, (fp, len),
    ARG		(FILE		*fp)
    ARG_END	(DWORD		len)
)
{
    COUNT		num_bytes;
    FILE		*out_fp;
    INDEX_HEADERPTR	ResHeaderPtr;

printf ("\tEXTRACTING %lu bytes @ '%s'\n", len, file_name[name_index]);
    ResHeaderPtr = _get_current_index_header ();
    if (CountPackageInstances (ResHeaderPtr, res_package) == 1)
    {
	RES_PACKAGE	r;

	r = res_package;
	do
	{
	    if (r == ResHeaderPtr->num_packages)
		len = LengthResFile (fp);
	    else if (GET_PACKAGE (
		    ResHeaderPtr->PackageList[r].packmem_info
		    ) != GET_PACKAGE (
		    ResHeaderPtr->PackageList[res_package - 1].packmem_info
		    ))
		len = 0;
	    else
		len = (ResHeaderPtr->PackageList[r].flags_and_data_loc
			& ~0xFF000000);
	    ++r;
	} while (len == 0);

	len -= ftell (fp);
printf ("\t\tOVERRIDING length (%lu)\n", len);
#ifdef DEBUG
{
#include <conio.h>
while (!kbhit ());
getch ();
}
#endif /* DEBUG */
    }

    out_fp = fopen (file_name[name_index++], "wb");
    do
    {
	num_bytes = len <= sizeof (workbuf) ? (COUNT)len : sizeof (workbuf);
	fread (workbuf, num_bytes, 1, fp);
	fwrite (workbuf, num_bytes, 1, out_fp);
    } while (len -= num_bytes);
    fclose (out_fp);
    return (0);
}

PROC(STATIC
MEM_HANDLE GetFileName, (fp, len),
    ARG		(FILE		*fp)
    ARG_END	(DWORD		len)
)
{
printf ("FOUND '%s'\n", _cur_resfile_name);
    strcpy (file_name[name_index++], _cur_resfile_name);
    return (0);
}

PROC(
void CDECL main, (argc, argv),
    ARG		(int	argc)
    ARG_END	(char	**argv)
)
{
    COUNT		i, num_types;
    MEM_HANDLE		hPkg, hNdx;
    INDEX_HEADERPTR	ResHeaderPtr;

    mem_init ((MEM_SIZE)0, 0, NULL_PTR);

    hPkg = InitResourceSystem (argv[1], 0, NULL_PTR);
    ResHeaderPtr = _get_current_index_header ();
    num_types = CountResourceTypes ();
    for (i = 1; i <= num_types; ++i)
    {
	InstallResTypeVectors (i, WriteData, NULL_PTR);
    }

    hNdx = OpenResourceIndexFile (argv[2]);
    SetResourceIndex (hNdx);
    for (i = 1; i <= num_types; ++i)
    {
	InstallResTypeVectors (i, GetFileName, NULL_PTR);
    }

    for (res_package = 1;
	    res_package <= ResHeaderPtr->num_packages;
	    ++res_package)
    {
	PROC_GLOBAL(
	MEM_HANDLE near load_package, (ResHeaderPtr, res_package),
	    ARG		(INDEX_HEADERPTR	ResHeaderPtr)
	    ARG_END	(RES_PACKAGE		res_package)
	);

	name_index = 0;
	SetResourceIndex (hNdx);
	load_package (_get_current_index_header (), res_package);

	name_index = 0;
	SetResourceIndex (hPkg);
	load_package (_get_current_index_header (), res_package);
    }

    CloseResourceIndex (hNdx);
    UninitResourceSystem ();

    mem_uninit ();

    exit (0);
}
