#include <stdint.h>

typedef uint32_t aifc_ID;
typedef uint8_t aifc_Extended[10];
		/* This should actually be a 80-bits integer, but
		 * long double takes 96 bits, even though only 80 are used */

#define aifc_MAKE_ID(x1, x2, x3, x4) \
		(((x4) << 24) | ((x3) << 16) | ((x2) << 8) | (x1))

#define aifc_FormID         aifc_MAKE_ID('F', 'O', 'R', 'M')
#define aifc_FormVersionID  aifc_MAKE_ID('F', 'V', 'E', 'R')
#define aifc_CommonID       aifc_MAKE_ID('C', 'O', 'M', 'M')
#define aifc_SoundDataID    aifc_MAKE_ID('S', 'S', 'N', 'D')

#define aifc_FormTypeAIFF   aifc_MAKE_ID('A', 'I', 'F', 'F')
#define aifc_FormTypeAIFC   aifc_MAKE_ID('A', 'I', 'F', 'C')

#define aifc_CompressionTypeSDX2  aifc_MAKE_ID('S', 'D', 'X', '2')

struct aifc_ChunkHeader {
	aifc_ID ckID;
	uint32_t ckSize;
};

struct aifc_Chunk {
	aifc_ID ckID;       /* Chunk ID */
	uint32_t ckSize;    /* Chunk size, excluding header */
	uint8_t ckData[1];  /* placeholder for data */
};

struct aifc_ContainerChunk {
	aifc_ID ckID;      /* "FORM" */
	uint32_t ckSize;   /* number of bytes of data */
	aifc_ID formType;  /* type of file, "AIFC" */
};

struct aifc_FormatVersionChunk {
	aifc_ID ckID;        /* "FVER" */
	uint32_t ckSize;     /* 4 */
	uint32_t timestamp;  /* date of format version, in Mac format */
};


struct aifc_ExtCommonChunk {
	aifc_ID ckID;          /* "COMM" */
	uint32_t ckSize;       /* size of chunk data */
	uint16_t numChannels;  /* number of channels */
	uint32_t numSampleFrames __attribute__ ((packed));	
	                       /* number of sample frames */
	uint16_t sampleSize __attribute__ ((packed));
	                       /* number of bits per sample */
	aifc_Extended sampleRate __attribute__ ((packed));
	                       /* number of frames per second */
	aifc_ID compressionType __attribute__ ((packed));
	                       /* compression type ID */
	char *compressionName;
	                       /* compression type name */
};


struct aifc_SoundDataChunk {
	aifc_ID ckID;        /* "SSND" */
	uint32_t ckSize;     /* size of chunk data */
	uint32_t offset;     /* offset to sound data */
	uint32_t blocksize;  /* size of alignment blocks */
	uint8_t data[1];     /* placeholder for data */
};

