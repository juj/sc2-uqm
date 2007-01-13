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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "unicode.h"
#include "port.h"


// Resynchronise (skip everything starting with 0x10xxxxxx):
static inline void
resyncUTF8(const unsigned char **ptr) {
	while ((**ptr & 0xc0) == 0x80)
		*ptr++;
}

// Get one character from a UTF-8 encoded string.
// *ptr will point to the start of the next character.
// Returns 0 if the encoding is bad. This can be distinguished from the
// '\0' character by checking whether **ptr == '\0' before calling this
// function.
wchar_t
getCharFromString(const unsigned char **ptr) {
	wchar_t result;

	if (**ptr < 0x80) {
		// 0xxxxxxx, regular ASCII
		result = **ptr;
		(*ptr)++;

		return result;
	}

	if ((**ptr & 0xe0) == 0xc0) {
		// 110xxxxx; 10xxxxxx must follow
		// Value between 0x00000080 and 0x000007ff (inclusive)
		result = **ptr & 0x1f;
		(*ptr)++;
		
		if ((**ptr & 0xc0) != 0x80)
			goto err;
		result = (result << 6) | ((**ptr) & 0x3f);
		(*ptr)++;
		
		if (result < 0x00000080) {
			// invalid encoding - must reject
			goto err;
		}
		return result;
	}

	if ((**ptr & 0xf0) == 0xe0) {
		// 1110xxxx; 10xxxxxx 10xxxxxx must follow
		// Value between 0x00000800 and 0x0000ffff (inclusive)
		result = **ptr & 0x0f;
		(*ptr)++;
		
		if ((**ptr & 0xc0) != 0x80)
			goto err;
		result = (result << 6) | ((**ptr) & 0x3f);
		(*ptr)++;
		
		if ((**ptr & 0xc0) != 0x80)
			goto err;
		result = (result << 6) | ((**ptr) & 0x3f);
		(*ptr)++;
		
		if (result < 0x00000800) {
			// invalid encoding - must reject
			goto err;
		}
		return result;
	}

	if ((**ptr & 0xf8) == 0xf0) {
		// 11110xxx; 10xxxxxx 10xxxxxx 10xxxxxx must follow
		// Value between 0x00010000 and 0x0010ffff (inclusive)
		result = **ptr & 0x07;
		(*ptr)++;
		
		if ((**ptr & 0xc0) != 0x80)
			goto err;
		result = (result << 6) | ((**ptr) & 0x3f);
		(*ptr)++;
		
		if ((**ptr & 0xc0) != 0x80)
			goto err;
		result = (result << 6) | ((**ptr) & 0x3f);
		(*ptr)++;
		
		if ((**ptr & 0xc0) != 0x80)
			goto err;
		result = (result << 6) | ((**ptr) & 0x3f);
		(*ptr)++;
		
		if (result < 0x00010000) {
			// invalid encoding - must reject
			goto err;
		}
		return result;
	}
	
err:
	fprintf(stderr, "Warning: Invalid UTF8 sequence.\n");
	
	// Resynchronise (skip everything starting with 0x10xxxxxx):
	resyncUTF8(ptr);
	
	return 0;
}

size_t
utf8StringCount(const unsigned char *start) {
	size_t count = 0;
	wchar_t ch;

	for (;;) {
		ch = getCharFromString(&start);
		if (ch == '\0')
			return count;
		count++;
	}
}

// Locates a wide char (ch) in a UTF-8 string (pStr)
// returns the char positions when found
//  -1 when not found
int
utf8StringPos (const unsigned char *pStr, wchar_t ch)
{
	int pos;
 
	for (pos = 0; *pStr != '\0'; ++pos)
	{
		if (getCharFromString (&pStr) == ch)
			return pos;
	}

	if (ch == '\0' && *pStr == '\0')
		return pos;

	return -1;
}

// Safe version of strcpy(), somewhat analogous to strncpy()
// except it guarantees a 0-term when size > 0
// when size == 0, returns NULL
unsigned char *
utf8StringCopy (unsigned char *dst, size_t size, const unsigned char *src)
{
	if (size == 0)
		return 0;

	strncpy (dst, src, size);
	dst[size - 1] = '\0';
	
	return dst;
}

// Decodes a UTF-8 string (start) into a wide char string (wstr)
// returns number of chars decoded and stored, not counting 0-term
// any chars that do not fit are truncated
// wide string term 0 is always appended, unless the destination
// buffer is 0 chars long
size_t
getWideFromString(wchar_t *wstr, size_t maxcount, const unsigned char *start)
{
	wchar_t *next;

	if (maxcount == 0)
		return 0;

	// always leave room for 0-term
	--maxcount;

	for (next = wstr; maxcount > 0; ++next, --maxcount)
	{
		*next = getCharFromString(&start);
		if (*next == 0)
			break;
	}

	*next = 0; // term

	return next - wstr;
}

int
isWideGraphChar(wchar_t ch)
{	// this is not technically sufficient, but close enough for us
	// we'll consider all non-control (CO and C1) chars in 'graph' class
	return (ch > 0xa0) || (ch > 0x20 && ch < 0x7f);
}

int
isWidePrintChar(wchar_t ch)
{	// this is not technically sufficient, but close enough for us
	// chars in 'print' class are 'graph' + 'space' classes
	// the only space we currently have defined is 0x20
	return (ch == 0x20) || isWideGraphChar(ch);
}

wchar_t
toWideUpper(wchar_t ch)
{	// this is a very basic Latin-1 implementation
	// just to get things going
	return (ch < 0x100) ? toupper(ch) : ch;
}

wchar_t
toWideLower(wchar_t ch)
{	// this is a very basic Latin-1 implementation
	// just to get things going
	return (ch < 0x100) ? tolower(ch) : ch;
}
