#include <stdint.h>

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;

#define MAKE_WORD(x1, x2) (((x2) << 8) | (x1))
#define MAKE_DWORD(x1, x2, x3, x4) (((x4) << 24) | ((x3) << 16) | \
		((x2) << 8) | (x1))

#define TYPE_BITS 8
#define INSTANCE_BITS 13
#define PACKAGE_BITS 11

#define GET_TYPE(res) \
		((uint8)(res) & (uint8)((1 << TYPE_BITS) - 1))
#define GET_INSTANCE(res) \
		((uint16)((res) >> TYPE_BITS) & ((1 << INSTANCE_BITS) - 1))
#define GET_PACKAGE(res) \
		((uint16)((res) >> (TYPE_BITS + INSTANCE_BITS)) & \
		((1 << PACKAGE_BITS) - 1))

typedef struct
{
	uint16 instance_count;
#if 0
	RES_VECTORS func_vectors;
#endif
} type_desc;

typedef struct {
	uint8 path_offset[2];
	char filename[8];
	char extension[3];
} file_info;

typedef struct {
	uint8 num_types;
	uint16 num_instances;  // only low 13 bits used
	uint16 file_index;  // only low 11 bits used
	uint8 flags;
	uint32 data_loc;  // only low 24 bits used
} package_desc;

typedef struct {
	uint32 packmem_list_offs;
	uint32 path_list_offs;
	uint32 file_list_offs;
	uint16 num_packages;
	uint8 num_types;
	uint8 res_flags;
	uint16 header_len;
	package_desc *package_list;
	type_desc *type_list;

#if 0
#ifndef PACKAGING
	char index_file_name[36];
	DWORD data_offs;
	MEM_HANDLE hPredHeader, hSuccHeader;
#endif /* PACKAGING */
#endif
} index_header;

