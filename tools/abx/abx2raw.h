#include <stdint.h>

struct abx_header {
	uint16_t num_frames;
	uint32_t tot_size __attribute__ ((packed));
	uint16_t bufsize __attribute__ ((packed));
	uint16_t freq __attribute__ ((packed));
};

struct frame_info {
	uint32_t addr;
	uint16_t fsize;  // compressed file size
	uint16_t usize;  // uncompressed file size
};

#define SQLCH 0x40		// Squelch byte flag
#define RESYNC 0x80 	// Resync byte flag.

#define DELTAMOD 0x30 	// Delta modulation bits.

#define ONEBIT 0x10 		// One bit delta modulate
#define TWOBIT 0x20 		// Two bit delta modulate
#define FOURBIT 0x30		// four bit delta modulate

#define MULTIPLIER 0x0F  // Bottom nibble contains multiplier value.
#define SQUELCHCNT 0x3F  // Bits for squelching.

