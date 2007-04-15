/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <stdbool.h>
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
#define MAKE_RESOURCE(p,t,i) \
		(((uint32)(p) << (TYPE_BITS + INSTANCE_BITS)) | \
		((uint32)(i) << TYPE_BITS) | \
		((uint32)(t)))

typedef struct {
	uint32 size;  // only low 18 bits used, multiple of 4
	uint32 offset;
	uint16 file_index;
	char *filename;
} packdef_instance;

typedef struct {
	uint8 type;
	uint16 first_instance;  // only low 13 bits used
	uint16 num_instances;  // only low 11 bits used
	packdef_instance *instances;
} packtype_desc;

typedef struct {
	uint16 instance_count;
} type_desc;

typedef struct {
	uint8 path_offset[2];
	char filename[8];
	char extension[3];
} file_info;

typedef struct {
	uint16 index;
			// Index of this package. Acquired by counting; not part of the
			// package file.

	uint8 num_types;
	uint16 num_instances;  // only low 13 bits used
	uint16 file_index;  // only low 11 bits used
	uint8 flags;
	uint32 data_loc;  // only low 24 bits used
	uint32 size;
			// Size of the package; derived from the sizes of the resource
			// instances in the package.
	char *filename;
	packtype_desc *type_list;
	
} package_desc;

typedef struct {
	bool packaged;
	uint32 packmem_list_offs;
	uint32 path_list_offs;
	uint32 file_list_offs;
	uint16 num_packages;
	uint16 num_types;
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

// Statistics for one file refered to from packages.
typedef struct {
	size_t numPackages;
			// Number of packages that refer to this file.
	package_desc **packages;
			// Array of packages that refer to this file.
			// Sorted on their offset.
	uint32 fileSize;
} FileStats;

// Statistics for all files refered to from resource instances.
typedef struct {
	size_t indexUpper;
			// 1 higher than the last valid index for fileStat.
	FileStats *fileStats;
			// array of pointers to FileStat structures
} FilesStats;


