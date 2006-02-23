/*  Portions Copyright (C) 2005 Serge van den Boom */

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

#ifndef _UNICODE_H
#define _UNICODE_H

#include <stddef.h>

wchar_t getCharFromString(const unsigned char **ptr);
size_t utf8StringCount(const unsigned char *start);
int utf8StringPos (const unsigned char *pStr, wchar_t ch);
unsigned char *utf8StringCopy (unsigned char *dst, size_t size,
		const unsigned char *src);
size_t getWideFromString(wchar_t *wstr, size_t maxcount,
		const unsigned char *start);
int isWideGraphChar(wchar_t ch);
int isWidePrintChar(wchar_t ch);
wchar_t toWideUpper(wchar_t ch);
wchar_t toWideLower(wchar_t ch);

#endif /* _UNICODE_H */

