#ifndef _PORT_H
#define _PORT_H

#if defined(WIN32) && defined(_MSC_VER)
#	include <direct.h>
#	include <getopt.h>
#	define inline __inline
#else
#	include <unistd.h>
#endif
#if defined(__GNUC__)
#	define inline __inline__
#endif 

#define countof(a)	   ( sizeof(a)/sizeof(*a) )

#if defined(__WIN32__) || defined(WIN32) || defined(_WIN32)
#	define HAVE_STRICMP
#	define HAVE_STRUPR
#endif


#ifndef HAVE_STRICMP
#	define stricmp strcasecmp
#else
#	define strcasecmp stricmp
#endif

#ifdef _MSC_VER
// Defined in port.c
int snprintf(char *str, size_t size, const char *format, ...);
#endif /* _MSC_VER */

#endif /* _PORT_H */

