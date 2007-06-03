/*
 *  By Serge van den Boom (svdb@stack.nl)
 *
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

#include "util.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <unistd.h>

// Post: errno is in the state it was before the call.
void
vLogError(bool useErrno, const char *format, va_list args) {
	FILE *out = stderr;
	int savedErrno = errno;

	(void) fprintf(out, "Error: ");
	(void) vfprintf(out, format, args);
	if (useErrno)
		(void) fprintf(out, "Errno: %s\n", strerror(savedErrno));
	errno = savedErrno;
}

// Post: errno is in the state it was before the call.
void
logError(bool useErrno, const char *format, ...) {
	va_list args;

	va_start(args, format);
	vLogError(useErrno, format, args);
	va_end(args);
}

void
fatal(bool useErrno, const char *format, ...) {
	va_list args;

	va_start(args, format);
	vLogError(useErrno, format, args);
	va_end(args);

	exit(EXIT_FAILURE);
}

int
mmapOpen(const char *fileName, int flags, void **buf, size_t *len) {
	int fd = -1;

	if (((flags & O_ACCMODE) != O_RDONLY && (flags & O_ACCMODE) != O_RDWR)
			|| (flags & ~O_ACCMODE) != 0) {
		logError(false, "Invalid flags to mmapOpen.\n");
		errno = EINVAL;
		goto err;
	}

	fd = open(fileName, flags);
	if (fd == -1) {
		logError(true, "Failed to open() file '%s'.\n", fileName);
		goto err;
	}

	struct stat statBuf;
	if (fstat(fd, &statBuf) == -1) {
		logError(true, "Failed to fstat() file '%s'.\n", fileName);
		goto err;
	}

	int prot;
	if ((flags & O_ACCMODE) == O_RDWR) {
		prot = PROT_READ | PROT_WRITE;
	} else
		prot = PROT_READ;
	void *mapAddr = mmap(NULL, (size_t) statBuf.st_size, prot, MAP_SHARED,
			fd, 0);
	if (mapAddr == MAP_FAILED) {
		logError(true, "Failed to mmap() file '%s'.\n", fileName);
		return -1;
	}
	
	(void) close(fd);

	*buf = mapAddr;
	*len = (size_t) statBuf.st_size;
	return 0;

err:
	if (fd != -1) {
		int savedErrno = errno;
		(void) close(fd);
		errno = savedErrno;
	}
	return -1;
}

// Use base '0' for any base.
// returns 0 if the entire string is ok.
int
parseU8(const char *str, int base, uint8_t *result) {
	char *endPtr;
	unsigned long temp;

	if (str[0] == '\0')
	{
		errno = EINVAL;
		return -1;
	}

	errno = 0;
	temp = strtoul (str, &endPtr, base);
	if (errno != 0)
		return -1;

	if (*endPtr != '\0')
	{
		errno = EINVAL;
		return -1;
	}

	if (temp > UINT8_MAX) {
		errno = ERANGE;
		return -1;
	}

	*result = (uint8_t) temp;
	return 0;
}

// Use base '0' for any base.
// returns 0 if the entire string is ok.
int
parseS8(const char *str, int base, int8_t *result) {
	char *endPtr;
	long temp;

	if (str[0] == '\0')
	{
		errno = EINVAL;
		return -1;
	}

	errno = 0;
	temp = strtol (str, &endPtr, base);
	if (errno != 0)
		return -1;

	if (*endPtr != '\0')
	{
		errno = EINVAL;
		return -1;
	}

	if (temp < INT8_MIN || temp > INT8_MAX) {
		errno = ERANGE;
		return -1;
	}

	*result = (int8_t) temp;
	return 0;
}

// Use base '0' for any base.
// returns 0 if the entire string is ok.
int
parseU16(const char *str, int base, uint16_t *result) {
	char *endPtr;
	unsigned long temp;

	if (str[0] == '\0')
	{
		errno = EINVAL;
		return -1;
	}

	errno = 0;
	temp = strtoul (str, &endPtr, base);
	if (errno != 0)
		return -1;

	if (*endPtr != '\0')
	{
		errno = EINVAL;
		return -1;
	}

	if (temp > UINT16_MAX) {
		errno = ERANGE;
		return -1;
	}

	*result = (uint16_t) temp;
	return 0;
}

// Use base '0' for any base.
// returns 0 if the entire string is ok.
int
parseS16(const char *str, int base, int16_t *result) {
	char *endPtr;
	long temp;

	if (str[0] == '\0')
	{
		errno = EINVAL;
		return -1;
	}

	errno = 0;
	temp = strtol (str, &endPtr, base);
	if (errno != 0)
		return -1;

	if (*endPtr != '\0')
	{
		errno = EINVAL;
		return -1;
	}

	if (temp < INT16_MIN || temp > INT16_MAX) {
		errno = ERANGE;
		return -1;
	}

	*result = (int16_t) temp;
	return 0;
}

// Use base '0' for any base.
// returns 0 if the entire string is ok.
int
parseU32(const char *str, int base, uint32_t *result) {
	char *endPtr;
	unsigned long temp;

	if (str[0] == '\0')
	{
		errno = EINVAL;
		return -1;
	}

	errno = 0;
	temp = strtoul (str, &endPtr, base);
	if (errno != 0)
		return -1;

	if (*endPtr != '\0')
	{
		errno = EINVAL;
		return -1;
	}

	if (temp > UINT32_MAX) {
		errno = ERANGE;
		return -1;
	}

	*result = (uint32_t) temp;
	return 0;
}

// Use base '0' for any base.
// returns 0 if the entire string is ok.
int
parseS32(const char *str, int base, int32_t *result) {
	char *endPtr;
	long temp;

	if (str[0] == '\0')
	{
		errno = EINVAL;
		return -1;
	}

	errno = 0;
	temp = strtol (str, &endPtr, base);
	if (errno != 0)
		return -1;

	if (*endPtr != '\0')
	{
		errno = EINVAL;
		return -1;
	}

	if (temp < INT32_MIN || temp > INT32_MAX) {
		errno = ERANGE;
		return -1;
	}

	*result = (int32_t) temp;
	return 0;
}



