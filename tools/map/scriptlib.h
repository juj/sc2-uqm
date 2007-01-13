/*  Copyright (c) 2005 Michael C. Martin */

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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef _SCRIPTLIB_H
#define _SCRIPTLIB_H

#include <stdio.h>

#ifndef __cpluspluc
typedef enum { false, true } bool;
#endif

/* Key-Value resources */
void scr_ClearTables (void);

void scr_LoadFilename (const char *fname);
void scr_SaveFilename (const char *fname, const char *root);

void scr_LoadFile (FILE *fname);
void scr_SaveFile (FILE *fname, const char *root);

bool scr_HasKey (const char *key);

const char *scr_GetString (const char *key);
const char *scr_GetStringDef (const char *key, const char *def);
void scr_PutString (const char *key, const char *value);

bool scr_IsInteger (const char *key);
int scr_GetInteger (const char *key);
int scr_GetIntegerDef (const char *key, int def);
void scr_PutInteger (const char *key, int value);

float scr_GetFloat (const char *key);
float scr_GetFloatDef (const char *key, float def);

bool scr_IsBoolean (const char *key);
bool scr_GetBoolean (const char *key);
bool scr_GetBooleanDef (const char *key, bool def);
void scr_PutBoolean (const char *key, bool value);

#endif /* _RESLIB_H */

