#include <stdio.h>
#include <io.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "resource/resintrn.h"

static FILE	*package_fp;
static BOOLEAN	sanity_check;
char		workbuf[8192];
char		file_buf[256];
static long	package_offset;

PROC_GLOBAL(
MEM_HANDLE near load_package, (ResHeaderPtr, res_package),
    ARG		(INDEX_HEADERPTR	ResHeaderPtr)
    ARG_END	(RES_PACKAGE		res_package)
);
PROC_GLOBAL(
void near get_resource_filename, (ResHeaderPtr, pbuf, file_index),
    ARG		(INDEX_HEADERPTR	ResHeaderPtr)
    ARG		(PSTR			pbuf)
    ARG_END	(COUNT			file_index)
);
PROC_GLOBAL(
DWORD near get_packmem_offs, (ResHeaderPtr, res_package),
    ARG		(INDEX_HEADERPTR	ResHeaderPtr)
    ARG_END	(RES_PACKAGE		res_package)
);

PROC(STATIC
void near copy_data, (in_fp, out_fp, length),
    ARG		(FILE	*in_fp)
    ARG		(FILE	*out_fp)
    ARG_END	(DWORD	length)
)
{
    do
    {
	COUNT	num_bytes;

	num_bytes = length >= (DWORD)sizeof (workbuf) ?
		sizeof (workbuf) : (COUNT)length;
	fread (workbuf, num_bytes, 1, in_fp);
	fwrite (workbuf, num_bytes, 1, out_fp);
	length -= num_bytes;
    } while (length);
}

PROC(STATIC
MEM_HANDLE write_resource, (in_fp, length),
    ARG		(FILE	*in_fp)
    ARG_END	(DWORD	length)
)
{
    length = (length + 3) & ~3L;
    if (strcmp (workbuf, file_buf))
    {
	if (!sanity_check)
	    copy_data (in_fp, package_fp, length);
    }
    else
    {
	printf ("FILE NAMES THE SAME -- COPY ABORTED!!!\n");
	package_offset = 0L;
    }

    return ((MEM_HANDLE)(length >> 2));
}

PROC(STATIC
BOOLEAN file_error, (pFileName),
    ARG_END	(PSTR	pFileName)
)
{
    printf ("UNABLE TO FIND '%s'\n", pFileName);
    if (!sanity_check)
	exit (1);

    return (TRUE);
}

PROC(STATIC
BOOLEAN near parse_args, (argc, argv, infile, outfile),
    ARG		(int	argc)
    ARG		(char	*argv[])
    ARG		(char	infile[])
    ARG_END	(char	outfile[])
)
{
    while (--argc > 0)
    {
	++argv;
	if ((*argv)[0] == '-'
		&& toupper ((*argv)[1]) == 'S')
	    sanity_check = TRUE;
	else if (infile[0] == '\0')
	    strcpy (infile, *argv);
	else if (outfile[0] == '\0')
	    strcpy (outfile, *argv);
    }

    return (infile[0] != '\0' && outfile[0] != '\0');
}

PROC(
void CDECL main, (argc, argv),
    ARG		(int	argc)
    ARG_END	(char	*argv[])
)
{
    char	outfile[80];

    mem_init ((MEM_SIZE)0, 0, NULL_PTR);

    if (parse_args (argc, argv, workbuf, outfile))
    {
	long			file_offs;
	FILE			*out_fp;
	COUNT			i, num_types;
	DWORD			old_path_list_offs, old_file_list_offs,
				package_path_list_offs, package_file_list_offs,
				header_len;
	FILE			*res_fp;
	INDEX_HEADERPTR		ResHeaderPtr;
	MEM_HANDLE		hList;
	RES_HANDLE_LISTPTR	ResourceHandleListPtr;
	ENCODEPTR		TypeEncodePtr;
	DATAPTR			DataPtr;

	if (InitResourceSystem (workbuf, 0, file_error) == 0)
	    exit (1);
	res_fp = fopen (workbuf, "rb");

	ResHeaderPtr = _get_current_index_header ();
	ResHeaderPtr->res_fp = res_fp;
	old_path_list_offs = ResHeaderPtr->path_list_offs;
	old_file_list_offs = ResHeaderPtr->file_list_offs;

	/* subtract char index_file_name[36]; DWORD data_offs; MEM_HANDLE hPredHeader, hSuccHeader; */
	header_len = old_path_list_offs;
printf ("INDEX HEADER SIZE: %lu -- (0x%04x)\n", header_len, res_fp);

	if (!sanity_check)
	{
	    out_fp = fopen (outfile, "wb");
	    fseek (res_fp, 0L, SEEK_SET);
	    copy_data (res_fp, out_fp, header_len);
	    fclose (out_fp);
	}

	num_types = CountResourceTypes ();
	for (i = 1; i <= num_types; ++i)
	    InstallResTypeVectors (i, write_resource, NULL_PTR);

	fseek (res_fp, old_file_list_offs -
		sizeof (package_path_list_offs), SEEK_SET);
	fread (&package_path_list_offs,
		sizeof (package_path_list_offs), 1, res_fp);
	fseek (res_fp, filelength (fileno (res_fp)) -
		sizeof (package_file_list_offs), SEEK_SET);
	fread (&package_file_list_offs,
		sizeof (package_file_list_offs), 1, res_fp);
	for (i = 1; i <= ResHeaderPtr->num_packages; ++i)
	{
	    COUNT	file_index;

	    file_index = GET_PACKAGE (
		    ResHeaderPtr->PackageList[i - 1].packmem_info
		    );
	    ResHeaderPtr->path_list_offs = package_path_list_offs;
	    ResHeaderPtr->file_list_offs = package_file_list_offs;
	    get_resource_filename (ResHeaderPtr, workbuf, file_index);
	    ResHeaderPtr->path_list_offs = old_path_list_offs;
	    ResHeaderPtr->file_list_offs = old_file_list_offs;

	    if (!sanity_check && strcmp (workbuf, outfile) == 0)
	    {
		out_fp = fopen (outfile, "r+b");
		file_offs = (long)package_file_list_offs +
			(long)file_index * FILE_LIST_SIZE
			+ sizeof (UWORD);	/* skip path_offset */
		fseek (out_fp, file_offs, 0);
		putc ('\0', out_fp);
		fclose (out_fp);
	    }

	    package_fp = fopen (workbuf, "ab");
	    package_offset = LengthResFile (package_fp);
printf ("Packaging '%s' at offset %ld . . .\n", workbuf, package_offset);
	    hList = load_package (ResHeaderPtr, i);
	    fclose (package_fp);

	    ResHeaderPtr->PackageList[i - 1].flags_and_data_loc =
		    MAKE_DWORD (hList, 0);
	    if (hList)
	    {
		LockResourceHandleList (ResHeaderPtr, hList, i,
			&ResourceHandleListPtr, &TypeEncodePtr, &DataPtr);
		ResourceHandleListPtr->flags_and_data_loc = 0xFF000000 | package_offset;
		UnlockResourceHandleList (hList);
	    }
	}

	if (!sanity_check)
	    out_fp = fopen (outfile, "r+b");

	for (i = 1; i <= ResHeaderPtr->num_packages; ++i)
	{
	    COUNT	num_types, num_instances;

	    num_types = CountPackageTypes (ResHeaderPtr, i);
	    num_instances = CountPackageInstances (ResHeaderPtr, i);

	    hList = (MEM_HANDLE)LOWORD (
		    ResHeaderPtr->PackageList[i - 1].flags_and_data_loc);

	    if (hList)
	    {
		LockResourceHandleList (ResHeaderPtr, hList, i,
			&ResourceHandleListPtr, &TypeEncodePtr, &DataPtr);
		ResHeaderPtr->PackageList[i - 1].flags_and_data_loc =
			ResourceHandleListPtr->flags_and_data_loc;
	    }

	    if (!sanity_check)
	    {
		file_offs = (long)get_packmem_offs (ResHeaderPtr, i)
			+ (num_types * PACKMEM_LIST_SIZE);
		fseek (out_fp, file_offs, SEEK_SET);
		while (num_instances--)
		{
		    COUNT	length;

		    length = hList ? MAKE_WORD (DataPtr[0], DataPtr[1]) : 0;
		    DataPtr += 2;
		    fwrite (&length, sizeof (length), 1, out_fp);
		}
	    }

	    if (hList)
	    {
		UnlockResourceHandleList (hList);
		FreeResourceHandleList (hList);
	    }
	}

	if (!sanity_check)
	{
	    ResHeaderPtr->res_fp = (FILE *)~0;
	    ResHeaderPtr->path_list_offs = package_path_list_offs;
	    ResHeaderPtr->file_list_offs = package_file_list_offs;
	    ResHeaderPtr->index_info.header_len = header_len;

	    fseek (out_fp, 0L, SEEK_SET);
	    fwrite (ResHeaderPtr, sizeof (INDEX_HEADER)
		    + ResHeaderPtr->num_packages * sizeof (PACKAGE_DESC), 1, out_fp);
	    fclose (out_fp);
	}

	UninitResourceSystem ();
    }

    mem_uninit ();

    exit (0);
}
