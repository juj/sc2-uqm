#include <stdint.h>

#if defined(_MSC_VER)
#	define PACKED
#	pragma pack(push, 1)
#elif defined(__GNUC__)
#	define PACKED __attribute__ ((packed))
#endif

// LPF file layout
//
// header (0x40)
// padding (?)
// palette (0x400) - get offset from header
//    4 bytes/color)
//    usually 0x100 colors
// framepackinfos (?) - get offset from header
//    get number if packs from header
// 1st pack
//    immediatelly follows framepackinfos
// 2nd pack
//    offset = offset(1st pack) + 0x10000
// ...
// last pack
// end

typedef struct
{
	uint32_t PACKED magic;        // "LPF "
	uint16_t PACKED cpackslots;   // frame-pack slots
	uint16_t PACKED cpacks;       // frame-pack count
	uint32_t PACKED cframes;      // frame count
	uint16_t PACKED opal;         // offset of palette
	uint16_t PACKED opacks;       // offset of framepack infos
	uint32_t PACKED type;         // "ANIM"
	uint16_t PACKED w;            // animation width
	uint16_t PACKED h;            // animation height
	uint16_t PACKED stuff1[0x14]; // who knows?
	uint32_t PACKED cframes2;     // again?
	uint32_t PACKED fps;          // frames/second
} header;

#define PAL_COLORS	0x100

typedef union
{
	struct {
		uint8_t b, g, r, a;
	} bchan;
	uint8_t channels[4];
	uint32_t rgba;
} lpfpalcolor;

typedef struct
{
	uint16_t PACKED frame;     // first frame in this pack
	uint16_t PACKED cframes;   // count of frames in this pack
	uint16_t PACKED datasize;  // size of data in this pack
} framepackinfo;

typedef struct
{
	framepackinfo PACKED info;     // pack info
	uint16_t      PACKED za;       // 0 word
	uint16_t      PACKED sizes[1]; // frame-size array
} framepack;

#define FRAMEPACK_SIZE(fpi) \
	(sizeof(framepack) + sizeof(uint16_t) * (fpi.cframes - 1))

typedef struct
{
	uint16_t PACKED flags;     // 0x0042 - frame flags?
	uint16_t PACKED csubs;     // 0x0001 - count of subframes?
	// packed frame data follows
} frameheader;


// Frame Data
// Everything is stored LSB first unless otherwise specified.
//
// The data is processed and dumped into the frame buffer linearly. If not
// all of the frame buffer is covered by the data packets, the remaining
// pixels are copied from the previous frame.
//
// Data Packet (typed packets follow and include the data described here):
//   length  meaning
//   1       type and count
//             - bit 7:    type flag (FD_EXT_FLAG)
//             - bits 0-6: count
//             flag=0:
//               count=0: pixel repeat; Repeat Packet
//               count=N: quote next N; Quote Packet
//             flag=1:
//               count=0: 16-bit counts; Extended Packet
//               count=N: skip next N; Skip Packet
//
// Repeat Packet:
//   1       type and count, always 0x00 for this packet type
//   1       Repeat count
//   1       Repeated pixel
//
// Quote Packet:
//   1       type and count
//             - bit 7:    type flag; always 0 for this packet type
//             - bits 0-6: Quoted pixel count
//   ?       Quoted pixels; count specified by previous byte
//
// Skip Packet:
//   1       type and count
//             - bit 7:    type flag; always 1 for this packet type
//             - bits 0-6: Skipped pixel count; the pixels are copied
//                         from the same location in the previous frame
//
// Extended Packet (typed packets follow and include the data described here):
//   1       type and count, always 0x80 for this packet type
//   2       extended type and count
//             - bit 15:        type flag1 (FD_EXT_FLAG1)
//               when 0:        Extended Skip Packet
//                 - bits 0-14: Skipped pixel count
//               when 1:
//                 - bit 14:    type flag2 (FD_EXT_FLAG2)
//                   when 0:    Extended Quote Packet
//                   when 1:    Extended Repeat Packet
//                 - bits 0-13: Repeat/Quote pixel count
//
// Extended Skip Packet:
//   1       type and count, always 0x80 for this packet type
//   2       extended type and count
//             - bit 15:     type flag1 (FD_EXT_FLAG1), always 0
//             - bits 0-14:  Skipped pixel count; the pixels are copied
//                           from the same location in the previous frame
//
// Extended Quote Packet:
//   1       type and count, always 0x80 for this packet type
//   2       extended type and count
//             - bit 15:     type flag1 (FD_EXT_FLAG1), always 1
//             - bit 14:     type flag2 (FD_EXT_FLAG2), always 0
//             - bits 0-13:  Quoted pixel count
//   ?       Quoted pixels; count specified by previous word
//
// Extended Repeat Packet:
//   1       type and count, always 0x80 for this packet type
//   2       extended type and count
//             - bit 15:     type flag1 (FD_EXT_FLAG1), always 1
//             - bit 14:     type flag2 (FD_EXT_FLAG2), always 1
//             - bits 0-13:  Repeat count
//   1       Repeated pixel
//
#define FD_EXT_FLAG        0x0080
#define FD_COUNT_MASK      0x007f
#define FD_EXT_FLAG1       0x8000
#define FD_EXT_FLAG2       0x4000
#define FD_EXT_SCOUNT_MASK 0x7fff
#define FD_EXT_RCOUNT_MASK 0x3fff
#define FD_EXT_QCOUNT_MASK FD_EXT_RCOUNT_MASK

#if defined(_MSC_VER)
#	pragma pack(pop)
#endif
#undef PACKED
