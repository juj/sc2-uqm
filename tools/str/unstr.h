#include <stdint.h>

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef short int16;
typedef uint8 bool;
#define TRUE   1
#define FALSE  0

#define MAKE_WORD(x1, x2) (((x2) << 8) | (x1))
#define MAKE_DWORD(x1, x2, x3, x4) (((x4) << 24) | ((x3) << 16) | \
		((x2) << 8) | (x1))

typedef struct {
	uint32 size;
	uint32 offset;  // offset from beginning of strings
} entry_desc;

typedef struct {
	uint16 num_entries;
	entry_desc *entries;
} index_header;

