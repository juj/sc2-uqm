/*
 * .pkg file extractor
 * By Serge van den Boom (svdb@stack.nl), 20021012
 * GPL applies
 *
 * 
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <string.h>

#include "unpkg.h"


void readIndex(const uint8 *buf);
char *get_file_name(const uint8 *buf, const index_header *h, int package);

int
main(int argc, char *argv[]) {
	char *filename;
	int in;
	struct stat sb;
	uint8 *buf;

	if (argc != 2) {
		fprintf(stderr, "unpkg <infile>\n");
		return EXIT_FAILURE;
	}

	filename = argv[1];

	in = open(filename, O_RDONLY);
	if (in == -1) {
		perror("Could not open input file");
		return EXIT_FAILURE;
	}
	
	if (fstat(in, &sb) == -1) {
		perror("stat() failed");
		return EXIT_FAILURE;
	}
	
	buf = mmap(NULL, sb.st_size, PROT_READ, MAP_SHARED, in, 0);
	if (buf == MAP_FAILED) {
		perror("mmap() failed");
		return EXIT_FAILURE;
	}
	close(in);

	readIndex(buf);

	munmap(buf, sb.st_size);

	return EXIT_SUCCESS;
}

void
readIndex(const uint8 *buf) {
	index_header h;
	const uint8 *bufptr;
	
	h.res_flags = MAKE_WORD(buf[0x00], buf[0x01]) ? 1 : 0;
	fprintf(stderr, "0x00000000  File type:               %s\n",
			h.res_flags ? "packaged" : "not packaged");
	
	h.packmem_list_offs =
			MAKE_DWORD(buf[0x02], buf[0x03], buf[0x04], buf[0x05]);
	fprintf(stderr, "0x00000002  packmem_list_offs:       0x%08x\n",
			h.packmem_list_offs);

	h.path_list_offs =
			MAKE_DWORD(buf[0x06], buf[0x07], buf[0x08], buf[0x09]);
	fprintf(stderr, "0x00000006  path_list_offs:          0x%08x\n",
			h.path_list_offs);

	h.file_list_offs =
			MAKE_DWORD(buf[0x0a], buf[0x0b], buf[0x0c], buf[0x0d]);
	fprintf(stderr, "0x0000000a  file_list_offs:          0x%08x\n",
			h.file_list_offs);

	h.num_packages = MAKE_WORD(buf[0x0e], buf[0x0f]);
	fprintf(stderr, "0x0000000e  Number of packages:      %d\n",
			h.num_packages);

	h.num_types = MAKE_WORD(buf[0x10], buf[0x11]);
	fprintf(stderr, "0x00000010  Number of package types: %d\n",
			h.num_types);

	h.header_len = MAKE_DWORD(buf[0x12], buf[0x13], buf[0x14], buf[0x15]);
	fprintf(stderr, "0x00000012  Header length:           0x%08x\n",
			h.header_len);
	
	bufptr = buf + 0x16;
	h.package_list = malloc(h.num_packages * sizeof (package_desc));
	fprintf(stderr, "0x00000016  package info:\n");
	{
		int i;
		uint32 temp;
		
		for (i = 0; i < h.num_packages; i++) {
			temp = MAKE_DWORD(bufptr[0], bufptr[1], bufptr[2], bufptr[3]);
			h.package_list[i].num_types = GET_TYPE(temp);
			h.package_list[i].num_instances = GET_INSTANCE(temp);
			h.package_list[i].file_index = GET_PACKAGE(temp);
			fprintf(stderr, "0x%08x    #%d, %d types, %d instances, "
					"file index 0x%04x, ", bufptr - buf, i + 1,
					h.package_list[i].num_types,
					h.package_list[i].num_instances,
					h.package_list[i].file_index);
			bufptr += 4;
			
			temp = MAKE_DWORD(bufptr[0], bufptr[1], bufptr[2], bufptr[3]);
			h.package_list[i].flags = temp >> 24;
			h.package_list[i].data_loc = temp & 0x00ffffff;
			
			if (h.package_list[i].flags != 0xff) {
				fprintf(stderr, "BAD OFFSET!\n");
			} else {
				fprintf(stderr, "offset 0x%06x\n",
						h.package_list[i].data_loc);
			}
			bufptr += 4;
		}
	}
	
	// Type info:
	h.type_list = malloc(h.num_types * sizeof (type_desc));
	fprintf(stderr, "0x%08x  type info:\n", bufptr - buf);
	{
		int i;
		
		for (i = 0; i < h.num_types; i++) {
			h.type_list[i].instance_count = 
					MAKE_WORD(bufptr[0], bufptr[1]);
			fprintf(stderr, "0x%08x    #%d, %d instances\n",
					bufptr - buf, i + 1, h.type_list[i].instance_count);
			bufptr += 2;
		}
	}
	
	if ((uint32) (bufptr - buf) != h.packmem_list_offs) {
		fprintf(stderr, "PACKMEM_LIST NOT IMMEDIATELY AFTER TYPE LIST!\n");
		bufptr = buf + h.packmem_list_offs;
	}
	fprintf(stderr, "0x%08x  packmem info:\n", bufptr - buf);
	{
		int i, j;
		int num_types, num_instances;
		uint32 temp;
		
		for (i = 0; i < h.num_packages; i++) {
			if (h.res_flags) {
				// packaged
				char *filename;

				filename = get_file_name(buf, &h,
						h.package_list[i].file_index);
				if (filename == NULL)
					filename = "<UNNAMED>";
				fprintf(stderr, "0x%08x    Package #%d (%s):\n",
						bufptr - buf, i + 1, filename);
			} else {
				// not packaged
				fprintf(stderr, "0x%08x    Package #%d:\n",
						bufptr - buf, i + 1);
			}

			num_types = h.package_list[i].num_types;
			h.package_list[i].type_list =
					malloc(num_types * sizeof (packtype_desc));
			for (j = 0; j < num_types; j++) {
				temp = MAKE_DWORD(bufptr[0], bufptr[1], bufptr[2], bufptr[3]);
				h.package_list[i].type_list[j].type = GET_TYPE(temp);
				h.package_list[i].type_list[j].first_instance =
						GET_INSTANCE(temp);
				h.package_list[i].type_list[j].num_instances =
						GET_PACKAGE(temp);
				fprintf(stderr, "0x%08x      Type #%d: type %d, "
						"%d instances starting with #%d\n",
						bufptr - buf, j,
						h.package_list[i].type_list[j].type,
						h.package_list[i].type_list[j].num_instances,
						h.package_list[i].type_list[j].first_instance);
				bufptr += 4;
			}

			for (j = 0; j < num_types; j++) {
				int k;
				
				num_instances = h.package_list[i].type_list[j].num_instances;
				for (k = 0; k < num_instances; k++) {
					if (h.res_flags) {
						// packaged
						fprintf(stderr, "0x%08x      Instance #%d of "
								"type %d: size %d bytes\n", bufptr - buf,
								k + h.package_list[i].type_list[j].
								first_instance,
								h.package_list[i].type_list[j].type,
								4 * MAKE_WORD(bufptr[0], bufptr[1]));
					} else {
						char *filename;
	
						filename = get_file_name(buf, &h,
								MAKE_WORD(bufptr[0], bufptr[1]));
						if (filename == NULL)
							filename = "<UNNAMED>";
						fprintf(stderr, "0x%08x      Instance #%d of "
								"type %d: %s\n", bufptr - buf,
								k + h.package_list[i].type_list[j].
								first_instance,
								h.package_list[i].type_list[j].type,
								filename);
					}
					bufptr += 2;
				}  // for k
			}  // for j
		}
	}
}

char *
get_file_name(const uint8 *buf, const index_header *h, int file_index) {
	static char result[MAXPATHLEN];
	char *ptr;
	file_info *fi;
	uint16 path_offset;
	
	ptr = result;
	fi = &((file_info *) (buf + h->file_list_offs))[file_index];
	if (fi->filename[0] == '\0')
		return NULL;

	path_offset = MAKE_WORD(fi->path_offset[0], fi->path_offset[1]);
	if (path_offset != 0xffff) {
		strcpy(ptr, buf + h->path_list_offs + path_offset);
		ptr += strlen(ptr);
		*ptr = '/';
		ptr++;
	}

	ptr[8] = '\0';
	strncpy(ptr, fi->filename, 8);
	ptr += strlen(ptr);

	*ptr = '.';
	ptr++;

	ptr[3] = '\0';
	strncpy(ptr, fi->extension, 3);
	
	return result;
}

