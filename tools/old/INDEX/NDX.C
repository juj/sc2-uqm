#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "reslib.h"
#include "resource/index.h"

INDEX_HEADER	index_header;
TYPE_DESC	type_list[MAX_TYPES];
char		workbuf[8192];

typedef union
{
    UWORD	data_len;
    UWORD	filename_index;
} INSTANCE_INFO;

typedef enum
{
    BOC_TOKEN,
    EOC_TOKEN,
    EOL_TOKEN,
    STRING_TOKEN
} TOKEN_TYPE;

typedef enum
{
    RESTART_STATE,
    COMMENT_STATE,
    ERROR_STATE,

    TYPE_CONTROL_STATE,
    PATH_STATE,
    PACKAGE_PATH_STATE,
    PACKAGE_STATE,
    INCLUDE_STATE,

    RESOURCE_STATE
} LINE_STATE;
#define FIRST_KEYWORD	TYPE_CONTROL_STATE
#define FIRST_TYPE	RESOURCE_STATE

static char		*keyword_tab[] =
{
    "TYPE",
    "PATH",
    "PACKAGE_PATH",
    "PACKAGE",
    "INCLUDE",
};
#define NUM_KEYWORDS	(sizeof (keyword_tab) / sizeof (keyword_tab[0]))

typedef struct
{
    TOKEN_TYPE	type;
    char	string[256];
} TOKEN;

typedef struct
{
    COUNT	string_count, index;
    MEM_HANDLE	hBuf;
} STRING_TAB;

static char		type_names[MAX_TYPES][20];
static STRING_TAB	type_string_tab[MAX_TYPES], package_string_tab;
static STRING_TAB	instance_file_string_tab, instance_path_string_tab;
static STRING_TAB	package_file_string_tab, package_path_string_tab;
static COUNT		last_instance_path_index = (COUNT)~0,
			last_package_path_index = (COUNT)~0;

PROC(STATIC
TOKEN_TYPE near get_token, (line_buf, ptoken, state),
    ARG		(char		**line_buf)
    ARG		(TOKEN		*ptoken)
    ARG_END	(LINE_STATE	state)
)
{
    char	ch, last_ch, *str;

    ptoken->type = EOL_TOKEN;
EatChars:
    last_ch = '\0';
    while ((ch = *(*line_buf)++) == '\0' ||
	    ch == ' ' || isspace (ch) || state == COMMENT_STATE ||
	    ptoken->type == BOC_TOKEN)
    {
	if (ch == '\0' || ch == '\n')
	    return (ptoken->type);
	else if (ch == '/' && last_ch == '*')
	{
	    if (state == COMMENT_STATE)
		return (ptoken->type = EOC_TOKEN);
	    else if (ptoken->type != BOC_TOKEN)
		return (ERROR_STATE);
	    else
		ptoken->type = EOL_TOKEN;
	}
	last_ch = ch;
    }

    str = ptoken->string;
    do
    {
	if ((*str++ = ch) == '*' && last_ch == '/')
	{
	    if ((str -= 2) == ptoken->string)
	    {
		ptoken->type = BOC_TOKEN;

		goto EatChars;
	    }
	    --*line_buf;

	    break;
	}
	last_ch = ch;
    } while ((ch = *(*line_buf)++) != '\0' && ch != ' ' && !isspace (ch));
    *str++ = '\0';

    --*line_buf;

    return (ptoken->type = STRING_TOKEN);
}

PROC(STATIC
LINE_STATE near GetKeywordState, (ptoken),
    ARG_END	(TOKEN	*ptoken)
)
{
    int	i;

    strupr (ptoken->string);

    for (i = 0; i < NUM_KEYWORDS; ++i)
    {
	if (!strcmp (keyword_tab[i], ptoken->string))
	    return (FIRST_KEYWORD + i);
    }

    for (i = index_header.num_types - 1; i >= 0; --i)
    {
	if (!strcmp (type_names[i], ptoken->string))
	    return (FIRST_TYPE + i);
    }

    return (ERROR_STATE);
}

#define TAB_SIZE		(64*1024)
#define AllocStringTab()	mem_request (TAB_SIZE)
#define LockStringTab(h)	(LPSTR)mem_lock (h)
#define UnlockStringTab(h)	mem_unlock (h)

PROC(STATIC
COUNT near add_string, (buf, pstring_tab, length),
    ARG		(char		*buf)
    ARG		(STRING_TAB	*pstring_tab)
    ARG_END	(int		length)
)
{
    if (length > 0)
    {
	LPSTR	lpStr;

	if (pstring_tab->hBuf == NULL_HANDLE)
	    pstring_tab->hBuf = AllocStringTab ();

	lpStr = LockStringTab (pstring_tab->hBuf);
	lpStr += pstring_tab->index;
	pstring_tab->index += length;
	do
	    *lpStr++ = *buf++;
	while (--length);
	UnlockStringTab (pstring_tab->hBuf);
    }

    return (pstring_tab->index);
}

PROC(STATIC
COUNT near get_string, (buf, pstring_tab, index, length),
    ARG		(char		*buf)
    ARG		(STRING_TAB	*pstring_tab)
    ARG		(COUNT		index)
    ARG_END	(int		length)
)
{
    LPSTR	lpStr;

    if (index >= pstring_tab->index)
    {
	*buf = '\0';
	return (0);
    }

    lpStr = LockStringTab (pstring_tab->hBuf);
    lpStr += index;

    if (length == -1)
    {
	do
	    ++index;
	while (*buf++ = *lpStr++);
    }
    else if (length > 0)
    {
	index += length;
	do
	    *buf++ = *lpStr++;
	while (--length);
    }

    UnlockStringTab (pstring_tab->hBuf);

    return (index);
}

PROC(STATIC
COUNT near find_string, (ppath_index, string, pstring_tab),
    ARG		(PCOUNT		ppath_index)
    ARG		(char		*string)
    ARG_END	(STRING_TAB	*pstring_tab)
)
{
    COUNT	i, string_index;

    for (i = string_index = 0; i < pstring_tab->string_count; ++i)
    {
	COUNT	pi;
	BYTE	buf[128];

	if (ppath_index == 0)
	{
	    pi = string_index;
	    string_index = get_string (buf,
		    pstring_tab, string_index, -1);
	    if (!strcmp (buf, string))
		return (pi);
	}
	else
	{
	    string_index = get_string ((char *)&pi,
		    pstring_tab, string_index, sizeof (pi));
	    string_index = get_string (buf,
		    pstring_tab, string_index, -1);
	    if (pi == *ppath_index && !strcmp (buf, string))
		return (i);
	}
    }

    if (ppath_index == 0)
    {
	++pstring_tab->string_count;
	add_string (string,
		pstring_tab,
		strlen (string) + 1);
	return (string_index);
    }
    else
    {
	add_string ((char *)ppath_index,
		pstring_tab,
		sizeof (*ppath_index));
	add_string (string,
		pstring_tab,
		strlen (string) + 1);

	return (pstring_tab->string_count++);
    }
}

#define MAX_NUM_FILES	20
static COUNT	top_file_stack;
static FILE	*file_stack[MAX_NUM_FILES];

PROC(STATIC
FILE * near pop_file, (),
    ARG_VOID
)
{
    if (top_file_stack == 0)
	return ((FILE *)NULL);
    else
	return (file_stack[--top_file_stack]);
}

PROC(STATIC
BOOLEAN near push_file, (fp),
    ARG_END	(FILE	*fp)
)
{
    if (top_file_stack == MAX_NUM_FILES)
	return (FALSE);
    else
    {
	file_stack[top_file_stack++] = fp;
	return (TRUE);
    }
}

PROC(STATIC
void near create_headers, (),
    ARG_VOID
)
{
    COUNT	i, file_index;
    FILE	*fp, *inst_fp;

    fp = fopen ("restypes.h", "w");
    fprintf (fp, "#ifndef _RESTYPES_H\n");
    fprintf (fp, "#define _RESTYPES_H\n\n");

    fprintf (fp, "enum\n{\n");
    for (i = 0; i < index_header.num_types; ++i)
    {
	if (i == 0)
	    fprintf (fp, "    %s = 1", type_names[i]);
	else
	    fprintf (fp, "    %s", type_names[i]);
	if (i < index_header.num_types - 1)
	    fprintf (fp, ",");
	fprintf (fp, "\n");
    }
    fprintf (fp, "};\n\n");
    fprintf (fp, "#endif /* _RESTYPES_H */\n\n");
    fclose (fp);

    fp = fopen ("respkg.h", "w");

    fprintf (fp, "enum\n{\n");
    for (i = 0, file_index = 0; i < index_header.num_packages; ++i)
    {
	COUNT	index;

	file_index = get_string ((char *)&index,
		&package_string_tab,
		file_index, sizeof (index));
	file_index = get_string (workbuf,
		&package_string_tab, file_index, -1);

	if (i == 0)
	    fprintf (fp, "    %s = 1", workbuf);
	else
	    fprintf (fp, "    %s", workbuf);
	if (i < index_header.num_packages - 1)
	    fprintf (fp, ",");
	fprintf (fp, "\n");
    }
    fprintf (fp, "};\n\n");
    fclose (fp);

    inst_fp = fopen ("resinst.h", "w");

    for (i = 0; i < index_header.num_types; ++i)
    {
	if (type_list[i].instance_count > 0)
	{
	    COUNT	j, type_index;

	    sprintf (workbuf, "i%.7s.h", type_names[i]);
	    strlwr (workbuf);
	    fprintf (inst_fp, "#include \"%s\"\n", workbuf);

	    fp = fopen (workbuf, "w");

	    for (j = 0, type_index = 0; j < type_list[i].instance_count; ++j)
	    {
		RES_PACKAGE	pack_id;
		COUNT		file_index;

		type_index = get_string ((char *)&pack_id,
			&type_string_tab[i], type_index, sizeof (pack_id));
		type_index = get_string ((char *)&file_index,
			&type_string_tab[i], type_index, sizeof (file_index));
		type_index = get_string (workbuf,
			&type_string_tab[i], type_index, -1);
		fprintf (fp, "#define %s\t0x%08lxL\n",
			workbuf, MAKE_RESOURCE (pack_id, i + 1, j));
	    }

#ifdef OLD
	    fprintf (fp, "\ntypedef RESOURCE	%s_INSTANCE;\n\n", type_names[i]);
#else /* !OLD */
	    fprintf (fp, "\n\n");
#endif /* OLD */
	    fclose (fp);
	}
    }

    fclose (inst_fp);
}

PROC(STATIC
void near create_index_file, (file_out),
    ARG_END	(char	*file_out)
)
{
    COUNT	i;
    FILE	*fp;
    DWORD	package_path_list_offs, package_file_list_offs;

    fp = fopen (file_out, "wb");
    fseek (fp, (long)sizeof (index_header) - 6, SEEK_SET);

printf ("Writing out package descriptions\n");
    {
	COUNT		file_index;
	PACKAGE_DESC	package;

	package.flags_and_data_loc = 0xFF000000L;
	for (i = 0, file_index = 0; i < index_header.num_packages; ++i)
	{
	    COUNT	j, index, num_types, num_instances;

	    num_types = num_instances = 0;
	    for (j = 0; j < index_header.num_types; ++j)
	    {
		COUNT		instance_index[MAX_TYPES];
		RES_PACKAGE	pack_id;

		if (i == 0)
		    instance_index[j] = 0;

		index = get_string ((char *)&pack_id,
			&type_string_tab[j],
			instance_index[j], sizeof (pack_id));
		if (index > 0 && pack_id == (RES_PACKAGE)(i + 1))
		{
		    ++num_types;
		    do
		    {
			INSTANCE_INFO	info;

			++num_instances;
			instance_index[j] = index;
			instance_index[j] = get_string ((char *)&info.filename_index,
				&type_string_tab[j],
				instance_index[j], sizeof (info.filename_index));
			instance_index[j] = get_string (workbuf,
				&type_string_tab[j], instance_index[j], -1);

			index = get_string ((char *)&pack_id,
				&type_string_tab[j],
				instance_index[j], sizeof (pack_id));
		    } while (index > 0 && pack_id == (RES_PACKAGE)(i + 1));
		}
	    }

	    file_index = get_string ((char *)&index,
		    &package_string_tab,
		    file_index, sizeof (index));
	    file_index = get_string (workbuf,
		    &package_string_tab, file_index, -1);
printf ("\t%u total types, %u total instances in package %u -- <0x%lx>\n", num_types, num_instances, i + 1, package.flags_and_data_loc);
	    package.packmem_info = MAKE_RESOURCE (index, num_types, num_instances);
	    fwrite (&package, sizeof (PACKAGE_DESC), 1, fp);
	}
    }

printf ("Writing out instance counts for %d types\n", index_header.num_types);
    {
	for (i = 0; i < index_header.num_types; ++i)
	    fwrite (&type_list[i].instance_count,
		    sizeof (type_list[i].instance_count), 1, fp);
    }

    index_header.packmem_list_offs = (DWORD)ftell (fp);

printf ("Writing out package member list\n");
    {				/* write out resource list */
	for (i = 0; i < index_header.num_packages; ++i)
	{
	    COUNT		index, instance_index[MAX_TYPES];
	    RES_INSTANCE	first_instance[MAX_TYPES];
	    RES_PACKAGE		pack_id;
	    COUNT		j;

printf ("PACKAGE %u\n", i + 1);
	    for (j = 0; j < index_header.num_types; ++j)
	    {
		DWORD	encoding;

		if (i == 0)
		{
		    instance_index[j] = 0;
		    first_instance[j] = 0;
		}

		index = get_string ((char *)&pack_id,
			&type_string_tab[j],
			instance_index[j], sizeof (pack_id));
		if (index > 0 && pack_id == (RES_PACKAGE)(i + 1))
		{
		    COUNT	instance_count;

		    instance_count = 0;
		    do
		    {
			INSTANCE_INFO	info;

			++instance_count;
			index = get_string ((char *)&info.filename_index,
				&type_string_tab[j],
				index, sizeof (info.filename_index));
			index = get_string (workbuf,
				&type_string_tab[j], index, -1);

			index = get_string ((char *)&pack_id,
				&type_string_tab[j],
				index, sizeof (pack_id));
		    } while (index > 0 && pack_id == (RES_PACKAGE)(i + 1));

		    encoding = MAKE_RESOURCE (instance_count,
			    j + 1, first_instance[j]);
printf ("\ttype = %u, first_instance = %u, num_instances = %u\n", j + 1, first_instance[j], instance_count);
		    first_instance[j] += instance_count;
		    fwrite (&encoding, sizeof (DWORD), 1, fp);
		}
	    }

printf ("\n");
	    for (j = 0; j < index_header.num_types; ++j)
	    {
		index = get_string ((char *)&pack_id,
			&type_string_tab[j],
			instance_index[j], sizeof (pack_id));
		if (index > 0 && pack_id == (RES_PACKAGE)(i + 1))
		{
		    do
		    {
			INSTANCE_INFO	info;

			instance_index[j] = index;
			instance_index[j] = get_string ((char *)&info.filename_index,
				&type_string_tab[j],
				instance_index[j], sizeof (info.filename_index));
			instance_index[j] = get_string (workbuf,
				&type_string_tab[j], instance_index[j], -1);

printf ("\tfilename = '%s'(%d)\n", workbuf, info.filename_index);
			fwrite (&info, INSTANCE_LIST_SIZE, 1, fp);
			index = get_string ((char *)&pack_id,
				&type_string_tab[j],
				instance_index[j], sizeof (pack_id));
		    } while (index > 0 && pack_id == (RES_PACKAGE)(i + 1));
		}
	    }
	}
    }

    package_path_list_offs = (DWORD)ftell (fp);

printf ("Writing out package path list\n");
    {
				/* write out path list */
	COUNT	path_index;

	for (i = path_index = 0; i < package_path_string_tab.string_count; ++i)
	{
	    COUNT	old_path_index;
	    BYTE	buf[256];

	    old_path_index = path_index;
	    path_index = get_string (buf,
		    &package_path_string_tab, path_index, -1);
	    fwrite (buf, path_index - old_path_index, 1, fp);
	}
    }

    package_file_list_offs = (DWORD)ftell (fp);

printf ("Writing out package file list\n");
    {				/* write out file list */
	COUNT	file_index;
	BYTE	buf[FILE_LIST_SIZE + 2];	/* account for '.' and
						 * null char in filename
						 */
	COUNT	*ppath_offs = (COUNT *)&buf[0];
	BYTE	*pfilename = &buf[sizeof (*ppath_offs)];
	BYTE	*pext = &buf[sizeof (*ppath_offs) + FILE_NAME_SIZE];

	for (i = file_index = 0; i < package_file_string_tab.string_count; ++i)
	{
	    BYTE	*pstr1, *pstr2;

	    file_index = get_string ((char *)ppath_offs,
		    &package_file_string_tab, file_index, sizeof (*ppath_offs));
	    file_index = get_string (pfilename,
		    &package_file_string_tab, file_index, -1);
printf ("\t'%s'\n", pfilename);
	    pstr1 = pfilename;
	    while (*pstr1 != '.')
		++pstr1;
	    pstr2 = pext;

	    if (pstr1 == pstr2)
	    {
		while (*pstr2++ = *++pstr1)
		    ;
	    }
	    else
	    {
		COUNT	j;

		*pstr1++ = '\0';

		pstr1 += EXTENSION_SIZE - 1;
		pstr2 += EXTENSION_SIZE - 1;
		for (j = 0; j < EXTENSION_SIZE; ++j)
		    *pstr2-- = *pstr1--;
	    }

	    fwrite (buf, FILE_LIST_SIZE, 1, fp);
	}
    }

    index_header.path_list_offs = (DWORD)ftell (fp);

printf ("Writing out instance path list\n");
    {
				/* write out path list */
	COUNT	path_index;

	for (i = path_index = 0; i < instance_path_string_tab.string_count; ++i)
	{
	    COUNT	old_path_index;
	    BYTE	buf[256];

	    old_path_index = path_index;
	    path_index = get_string (buf,
		    &instance_path_string_tab, path_index, -1);
	    fwrite (buf, path_index - old_path_index, 1, fp);
	}
    }
    fwrite (&package_path_list_offs, sizeof (package_path_list_offs), 1, fp);

    index_header.file_list_offs = (DWORD)ftell (fp);

printf ("Writing out instance file list\n");
    {				/* write out file list */
	COUNT	file_index;
	BYTE	buf[FILE_LIST_SIZE + 2];	/* account for '.' and
						 * null char in filename
						 */
	COUNT	*ppath_offs = (COUNT *)&buf[0];
	BYTE	*pfilename = &buf[sizeof (*ppath_offs)];
	BYTE	*pext = &buf[sizeof (*ppath_offs) + FILE_NAME_SIZE];

	for (i = file_index = 0; i < instance_file_string_tab.string_count; ++i)
	{
	    BYTE	*pstr1, *pstr2;

	    file_index = get_string ((char *)ppath_offs,
		    &instance_file_string_tab, file_index, sizeof (*ppath_offs));
	    file_index = get_string (pfilename,
		    &instance_file_string_tab, file_index, -1);
	    pstr1 = pfilename;
	    while (*pstr1 != '.')
		++pstr1;
	    pstr2 = pext;

	    if (pstr1 == pstr2)
	    {
		while (*pstr2++ = *++pstr1)
		    ;
	    }
	    else
	    {
		COUNT	j;

		*pstr1++ = '\0';

		pstr1 += EXTENSION_SIZE - 1;
		pstr2 += EXTENSION_SIZE - 1;
		for (j = 0; j < EXTENSION_SIZE; ++j)
		    *pstr2-- = *pstr1--;
	    }

	    fwrite (buf, FILE_LIST_SIZE, 1, fp);
	}
    }
    fwrite (&package_file_list_offs, sizeof (package_file_list_offs), 1, fp);

    fseek (fp, 0L, SEEK_SET);
    fwrite ((char *)&index_header + 2, sizeof (index_header) - 6, 1, fp);
    fclose (fp);
printf ("%d\n", sizeof (index_header) - 6);
}

PROC(STATIC
BOOLEAN near parse_args, (argc, argv, infile, outfile),
    ARG		(int	argc)
    ARG		(char	*argv[])
    ARG		(char	infile[])
    ARG_END	(char	outfile[])
)
{
    if (argc != 3)
	return (FALSE);

    strcpy (infile, argv[1]);
    strcpy (outfile, argv[2]);

    return (TRUE);
}

PROC(STATIC
LINE_STATE near parse_line, (fp, line_buf, state),
    ARG		(FILE		**fp)
    ARG		(char		*line_buf)
    ARG_END	(LINE_STATE	state)
)
{
    TOKEN_TYPE	token_type;
    TOKEN	token;

    while ((token_type = get_token (&line_buf, &token, state)) != EOL_TOKEN)
    {
	char	loc_buf[128];

	switch (state)
	{
	    case ERROR_STATE:
		state = RESTART_STATE;
	    case RESTART_STATE:
		if (token_type == BOC_TOKEN)
		    state = COMMENT_STATE;
		else
		{
		    switch (state = GetKeywordState (&token))
		    {
			case ERROR_STATE:
			    return (ERROR_STATE);
			case PATH_STATE:
			    if (get_token (&line_buf,
				    &token, state) == STRING_TOKEN)
			    {
				if (strcmp (token.string, ".") == 0)
				    last_instance_path_index = (COUNT)~0;
				else
{
				    last_instance_path_index = find_string (
					    0, token.string,
					    &instance_path_string_tab);
get_string (token.string, &instance_path_string_tab, last_instance_path_index, -1);
}

printf ("PATH\n\t'%s'\n", token.string);
				state = RESTART_STATE;
			    }
			    break;
			case PACKAGE_PATH_STATE:
			    if (get_token (&line_buf,
				    &token, state) == STRING_TOKEN)
			    {
				if (strcmp (token.string, ".") == 0)
				    last_package_path_index = (COUNT)~0;
				else
{
				    last_package_path_index = find_string (
					    0, token.string,
					    &package_path_string_tab);
get_string (token.string, &package_path_string_tab, last_package_path_index, -1);
}

printf ("PACKAGE_PATH\n\t'%s'\n", token.string);
				state = RESTART_STATE;
			    }
			    break;
			case PACKAGE_STATE:
			    if (get_token (&line_buf,
				    &token, state) == STRING_TOKEN)
			    {
				strcpy (loc_buf, token.string);

				if (get_token (&line_buf,
					&token, state) == STRING_TOKEN)
				{
				    COUNT	which_string;

				    if (index_header.num_packages == MAX_PACKAGES)
					return (ERROR_STATE);
				    ++index_header.num_packages;

				    which_string = find_string (
					    &last_package_path_index,
					    token.string,
					    &package_file_string_tab);

				    ++package_string_tab.string_count;
				    add_string ((char *)&which_string,
					    &package_string_tab,
					    sizeof (which_string));
				    add_string (loc_buf,
					    &package_string_tab,
					    strlen (loc_buf) + 1);

printf ("PACKAGE\n\t'%s'\n", loc_buf);
				    state = RESTART_STATE;
				}
			    }
			    break;
			case INCLUDE_STATE:
			    if (get_token (&line_buf,
				    &token, state) == STRING_TOKEN)
			    {
				if (get_string (loc_buf,
					&instance_path_string_tab, last_instance_path_index, -1))
				    strcat (loc_buf, "/");
				strcat (loc_buf, token.string);
				if (push_file (*fp) &&
					(*fp = fopen (loc_buf, "r")) != NULL)
				{
printf ("INCLUDE\n\t'%s'\n", loc_buf);
				    state = RESTART_STATE;
				}
			    }
			    break;
			case TYPE_CONTROL_STATE:
			    if (get_token (&line_buf,
				    &token, state) == STRING_TOKEN)
			    {
				if (GetKeywordState (&token) == ERROR_STATE &&
					index_header.num_types < MAX_TYPES)
				{
				    strcpy (type_names[index_header.num_types++],
					    token.string);
printf ("TYPE\n\t'%s'\n", token.string);
				    state = RESTART_STATE;
				}
			    }
			    break;
			default:
			{
			    COUNT	type;

			    type = (COUNT)(state - RESOURCE_STATE);
			    if (type_list[type].instance_count == MAX_INSTANCES)
				return (ERROR_STATE);
			    ++type_list[type].instance_count;

			    if (get_token (&line_buf,
				    &token, state) == STRING_TOKEN)
			    {
				++type_string_tab[type].string_count;
				add_string ((char *)&index_header.num_packages,
					&type_string_tab[type],
					sizeof (index_header.num_packages));
				add_string ((char *)&instance_file_string_tab.string_count,
					&type_string_tab[type],
					sizeof (instance_file_string_tab.string_count));
				add_string (token.string,
					&type_string_tab[type],
					strlen (token.string) + 1);

				if (get_token (&line_buf,
					&token, state) == STRING_TOKEN)
				{
				    ++instance_file_string_tab.string_count;
				    add_string ((char *)&last_instance_path_index,
					    &instance_file_string_tab,
					    sizeof (last_instance_path_index));
				    add_string (token.string,
					    &instance_file_string_tab,
					    strlen (token.string) + 1);
printf ("INSTANCE\n\t'%s'\n", token.string);
				    state = RESTART_STATE;
				}
			    }
			    break;
			}
		    }
		}
		break;
	    case COMMENT_STATE:
		if (token_type == EOC_TOKEN)
		    state = RESTART_STATE;
		break;
	}
    }

    return (state <= COMMENT_STATE ? state : ERROR_STATE);
}

PROC(
void CDECL main, (argc, argv),
    ARG		(int	argc)
    ARG_END	(char	*argv[])
)
{
    char	outfile[80];
    FILE	*fp;
    LINE_STATE	state;

    mem_init ((MEM_SIZE)0, 0, NULL_PTR);

    state = ERROR_STATE;
    if (parse_args (argc, argv, workbuf, outfile) &&
	    (fp = fopen (workbuf, "r")) != NULL)
    {
	do
	{
	    while (fgets (workbuf, sizeof (workbuf) - 1, fp) != NULL &&
		    (state = parse_line (&fp, workbuf, state)) != ERROR_STATE)
		;
	    fclose (fp);
	} while (state != ERROR_STATE && (fp = pop_file ()) != NULL);

	if (state != ERROR_STATE)
	{
printf ("creating headers\n");
	    create_headers ();
printf ("creating index file\n");
	    create_index_file (outfile);
printf ("done\n");
	}
    }

    mem_uninit ();

    if (state != ERROR_STATE)
	exit (0);
    else
	exit (1);
}
