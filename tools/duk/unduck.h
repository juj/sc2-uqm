#include <stdint.h>

typedef struct {
	uint32_t sndcnt;
	uint32_t vidcnt;
} frame_header;

typedef struct {
	uint16_t magic;  // always 0xf77f
	uint16_t numsamples;
	uint16_t tag;  // this is NOT the initial predictor as it was
			// marked in some file.
	uint16_t indexl;  // initial index for left channel
	uint16_t indexr;  // initial index for right channel
	uint8_t data;  // placeholder for data
} audio_chunk;

