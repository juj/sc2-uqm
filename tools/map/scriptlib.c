/*  Copyright (c) 2005 Michael C. Martin */

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope thta it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  Se the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "port.h"
#include "scriptlib.h"
#include "alist.h"
#include "stringbank.h"

static alist *map = NULL;

static void
check_map_init (void)
{
	if (map == NULL) {
		map = Alist_New ();
	}
}

void
scr_ClearTables (void)
{
	if (map != NULL) {
		Alist_Free (map);
		map = NULL;
	}
}

/* Type conversion routines. */
static const char *
bool2str (bool b)
{
	return b ? "yes" : "no";
}

static const char *
int2str (int i) {
	char buf[20];
	sprintf (buf, "%d", i);
	return StringBank_AddOrFindString (buf);
}

static int
str2int (const char *s) {
	return atoi(s);
}

static float
str2float (const char *s) {
	return (float) atof(s);
}

static bool
str2bool (const char *s) {
	if (!stricmp (s, "yes") ||
	    !stricmp (s, "true") ||
	    !stricmp (s, "1") )
		return true;
	return false;
}

void
scr_LoadFile (FILE *s)
{
	alist *d;
	check_map_init ();
	
	d = Alist_New_FromFile (s);
	if (d) {
		Alist_PutAll (map, d);
		Alist_Free (d);
	}
}

void
scr_LoadFilename (const char *fname)
{
	alist *d;
	check_map_init ();
	
	d = Alist_New_FromFilename (fname);
	if (d) {
		Alist_PutAll (map, d);
		Alist_Free (d);
	}
}

void
scr_SaveFile (FILE *f, const char *root)
{
	check_map_init ();
	Alist_Dump (map, f, root);
}

void
scr_SaveFilename (const char *fname, const char *root)
{
	FILE *f;
	
	check_map_init ();
	f = fopen (fname, "wt");
	if (f) {
		scr_SaveFile (f, root);
		fclose (f);
	}
}

bool
scr_IsBoolean (const char *key)
{
	const char *d;
	check_map_init ();

	d = scr_GetString (key);
	if (!d) return 0;
		
	return !stricmp (d, "yes") ||
	       !stricmp (d, "no") ||
	       !stricmp (d, "true") ||
	       !stricmp (d, "false") ||
	       !stricmp (d, "0") ||
	       !stricmp (d, "1") ||
	       !stricmp (d, "");
}

bool
scr_IsInteger (const char *key)
{
	const char *d;
	check_map_init ();

	d = scr_GetString (key);
	while (*d) {
		if (!isdigit (*d))
			return 0;
		d++;
	}
	return 1;
}

const char *
scr_GetString (const char *key)
{
	check_map_init ();
	return Alist_GetString (map, key);
}

const char *
scr_GetStringDef (const char *key, const char *def)
{
	const char *v;
	check_map_init ();
	
	v = scr_GetString (key);
	if (!v)
		v = def;
	return v;
}

void
scr_PutString (const char *key, const char *value)
{
	check_map_init ();
	Alist_PutString (map, key, value);
}

int
scr_GetInteger (const char *key)
{
	check_map_init ();
	return str2int (scr_GetString (key));
}

int
scr_GetIntegerDef (const char *key, int def)
{
	const char *v;
	check_map_init ();
	
	v = scr_GetString (key);
	return v ? str2int (v) : def;
}

void
scr_PutInteger (const char *key, int value)
{
	check_map_init ();
	scr_PutString (key, int2str(value));
}

float
scr_GetFloat (const char *key)
{
	check_map_init ();
	return str2float (scr_GetString (key));
}

float
scr_GetFloatDef (const char *key, float def)
{
	const char *v;
	check_map_init ();
	
	v = scr_GetString (key);
	return v ? str2float (v) : def;
}

bool
scr_GetBoolean (const char *key)
{
	check_map_init ();
	return str2bool (scr_GetString (key));
}

bool
scr_GetBooleanDef (const char *key, bool def)
{
	const char *v;
	check_map_init ();
	
	v = scr_GetString (key);
	return v ? str2bool (v) : def;
}

void
scr_PutBoolean (const char *key, bool value)
{
	check_map_init ();
	scr_PutString (key, bool2str(value));
}

bool
scr_HasKey (const char *key)
{
	check_map_init ();
	return (scr_GetString (key) != NULL);
}
