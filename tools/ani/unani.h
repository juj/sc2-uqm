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
	int16 x;
	int16 y;
} hotspot_type;

typedef struct {
	uint16 w;
	uint16 h;
} bounds_type;

typedef struct {
	uint16 index; // only first 12 bits used
	uint8 plut;
	uint8 flags;
	hotspot_type hotspot;
	bounds_type bounds;
//	uint32 len;
	const uint8 *start;
} frame_desc;

typedef struct {
	uint16 num_frames; // only first 12 bits used
	uint16 frames_ofs;
	frame_desc *frames;
} index_header;

#define MAX_FONT_CHARS	0x60

typedef struct {
   	uint8 leading;
   	uint8 max_ascend;
	uint8 max_descend;
	uint8 spacing;
	uint8 kern_amount;
	uint8 kerntab[MAX_FONT_CHARS];
	uint8 padding1_[3];
	frame_desc frames[MAX_FONT_CHARS];
} FONT_DESC;

#define FTYPE_SHIFT   12
#define FINDEX_MASK   ((1 << FTYPE_SHIFT) - 1)
#define FTYPE_MASK    (0xffff & ~FINDEX_MASK)

#define TYPE_GET(f)   ((f) & FTYPE_MASK)
#define INDEX_GET(f)  ((f) & FINDEX_MASK)

#define FLAG_DATA_HARDWARE   (1 << 4)
#define FLAG_DATA_COPY       (1 << 5)
#define FLAG_DATA_PACKED     (1 << 6)
#define FLAG_X_FLIP          (1 << 7)

#define PACK_EOL       0
#define PACK_LITERAL   1
#define PACK_TRANS     2
#define PACK_REPEAT    3
#define PACK_SHIFT     6
#define PACK_TYPE(c)   ((uint8) ((c) >> PACK_SHIFT))
#define PACK_COUNT(c)  ((uint8) (((c) & ((1 << PACK_SHIFT) - 1)) + 1))

