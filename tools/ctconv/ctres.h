#include <stdint.h>

#if defined(_MSC_VER)
#	define PACKED
#	pragma pack(push, 1)
#elif defined(__GNUC__)
#	define PACKED __attribute__ ((packed))
#endif

/* string res structures from unstr.h */

#define CTTAB_MAGIC 0xffffffff

typedef struct
{
	uint32_t PACKED magic;
	uint16_t PACKED unknown1;      // always 0
	uint16_t PACKED num_entries;
	uint32_t PACKED offset;        // should always be 0
	uint32_t PACKED entry_size[1]; // 1 or more entry sizes
} cttab_header_t;

#define CLUT_COLORS  0x20
#define CLUT_STEPS   8

typedef uint16_t clut_color_t;
typedef clut_color_t clut_t[CLUT_COLORS];

typedef struct
{
	uint8_t PACKED start_clut;
	uint8_t PACKED end_clut;     // inclusive
	clut_t  PACKED cluts[1];     // 0 or more cluts
} clut_tab_t;

#define PAL_COLORS    0x100
#define PAL_CHANNELS  3

typedef uint8_t pal_channel_t;

typedef union
{
	struct {
		pal_channel_t r, g, b;
	} chan;
	pal_channel_t channels[PAL_CHANNELS];
} pal_color_t;

typedef pal_color_t pal_t[PAL_COLORS];

typedef struct
{
	uint8_t PACKED start_clut;
	uint8_t PACKED end_clut;    // inclusive
	pal_t   PACKED pals[1];     // 0 or more pals
} pal_tab_t;


#if defined(_MSC_VER)
#	pragma pack(pop)
#endif
#undef PACKED
