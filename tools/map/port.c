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


