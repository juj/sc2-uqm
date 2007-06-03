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

#ifndef _UTIL_H
#define _UTIL_H

#include <stdarg.h>
#include <stdbool.h>
#include <sys/types.h>
#include <stdint.h>


void logError(bool useErrno, const char *format, ...)
		__attribute__((format(printf, 2, 3)));
void vLogError(bool useErrno, const char *format, va_list args);
void fatal(bool useErrno, const char *format, ...)
		__attribute__((format(printf, 2, 3))) __attribute__((noreturn));

int mmapOpen(const char *fileName, int flags, void **buf, size_t *len);

int parseU8(const char *str, int base, uint8_t *res);
int parseS8(const char *str, int base, int8_t *res);
int parseU16(const char *str, int base, uint16_t *res);
int parseS16(const char *str, int base, int16_t *res);
int parseU32(const char *str, int base, uint32_t *res);
int parseS32(const char *str, int base, int32_t *res);

#endif  /* _UTIL_H */

