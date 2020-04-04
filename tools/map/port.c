#include "port.h"

#ifndef HAVE_STRUPR

#include <ctype.h>

char *
strupr (char *str)
{
	char *ptr;
	
	ptr = str;
	while (*ptr)
	{
		*ptr = (char) toupper (*ptr);
		ptr++;
	}
	return str;
}
#endif


#ifdef _MSC_VER
// MSVC does not have snprintf() and vsnprintf(). It does have a _snprintf()
// and _vsnprintf(), but these do not terminate a truncated string as
// the C standard prescribes.
#include <stdio.h>
#include <stdarg.h>

int
snprintf(char *str, size_t size, const char *format, ...)
{
	int result;
	va_list args;
	
	va_start (args, format);
	result = _vsnprintf (str, size, format, args);
	if (str != NULL && size != 0)
		str[size - 1] = '\0';
	va_end (args);

	return result;
}
#endif  /* _MSC_VER */
