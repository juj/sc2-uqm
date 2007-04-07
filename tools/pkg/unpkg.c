/*
 * .pkg file extractor
 * By Serge van den Boom (svdb@stack.nl), 20021012
 *
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <string.h>
#include <errno.h>

#include "unpkg.h"


struct options {
	char *infile;
	char *outdir;
	char list;
	char print;
	char verbose;
};


index_header *readIndex(const uint8 *buf);
void printIndex(const index_header *h, const uint8 *buf, FILE *out);
char *get_file_name(const uint8 *buf, const index_header *h,
		uint16 file_index);
void writeFiles(const index_header *h, const uint8 *data, const char *path);
void parse_arguments(int argc, char *argv[], struct options *opts);
void listResources(const index_header *h, const uint8 *buf, FILE *out);


int
main(int argc, char *argv[]) {
	int in;
	struct stat sb;
	uint8 *buf;
	index_header *h;
	struct options opts;

	parse_arguments(argc, argv, &opts);

	in = open(opts.infile, O_RDONLY);
	if (in == -1) {
		fprintf(stderr, "Error: Could not open file %s: %s\n", opts.infile,
				strerror(errno));
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

	h = readIndex(buf);
	if (opts.print)
		printIndex(h, buf, stdout);

	if (opts.list)
		listResources(h, buf, stdout);

	if (opts.outdir != NULL) {
		size_t len;
		struct stat sb;

		if (!h->res_flags) {
			fprintf(stderr, "Error: Cannot unpack data from unpackaged "
					"file.\n");
			exit(EXIT_FAILURE);
		}

		if (access(opts.outdir, W_OK) == -1) {
			fprintf(stderr, "Error: Invalid destination dir %s: %s\n",
					opts.outdir, strerror(errno));
			exit(EXIT_FAILURE);
		}
		if (stat(opts.outdir, &sb) == -1) {
			fprintf(stderr, "Error: Could not stat %s: %s\n", opts.outdir,
					strerror(errno));
			exit(EXIT_FAILURE);
		}
		if (!S_ISDIR(sb.st_mode)) {
			fprintf(stderr, "Error: %s is not a directory.\n",
					opts.outdir);
			exit(EXIT_FAILURE);
		}
				
		len = strlen(opts.outdir);
		if (opts.outdir[len - 1] == '/')
			opts.outdir[len - 1] = '\0';

		writeFiles(h, buf, opts.outdir);
	}

	// freeIndex(h);
	munmap(buf, sb.st_size);
	return EXIT_SUCCESS;
}

void
usage() {
	fprintf(stderr, "unpkg [-o outputdir] [-v] [-l listfile] <infile>\n");
}

void
parse_arguments(int argc, char *argv[], struct options *opts) {
	char ch;
	
	memset(opts, '\0', sizeof (struct options));
	while (1) {
		ch = getopt(argc, argv, "hlo:pv");
		if (ch == -1)
			break;
		switch(ch) {
			case 'l':
				opts->list = 1;
				break;
			case 'o':
				opts->outdir = optarg;
				break;
			case 'p':
				opts->print = 1;
				break;
			case 'v':
				opts->verbose = 1;
				break;
			case '?':
			case 'h':
			default:
				usage();
				exit(EXIT_FAILURE);
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 1) {
		usage();
		exit(EXIT_FAILURE);
	}
	opts->infile = argv[0];
}

index_header *
readIndex(const uint8 *buf) {
	index_header *h;
	const uint8 *bufptr;
	
	h = malloc(sizeof (index_header));
	h->res_flags = MAKE_WORD(buf[0x00], buf[0x01]) ? 1 : 0;
	h->packmem_list_offs =
			MAKE_DWORD(buf[0x02], buf[0x03], buf[0x04], buf[0x05]);
	h->path_list_offs =
			MAKE_DWORD(buf[0x06], buf[0x07], buf[0x08], buf[0x09]);
	h->file_list_offs =
			MAKE_DWORD(buf[0x0a], buf[0x0b], buf[0x0c], buf[0x0d]);
	h->num_packages = MAKE_WORD(buf[0x0e], buf[0x0f]);
	h->num_types = MAKE_WORD(buf[0x10], buf[0x11]);
	h->header_len = MAKE_DWORD(buf[0x12], buf[0x13], buf[0x14], buf[0x15]);
	
	bufptr = buf + 0x16;
	h->package_list = malloc(h->num_packages * sizeof (package_desc));
	{
		int i;
		uint32 temp;
		
		for (i = 0; i < h->num_packages; i++) {
			temp = MAKE_DWORD(bufptr[0], bufptr[1], bufptr[2], bufptr[3]);
			h->package_list[i].num_types = GET_TYPE(temp);
			h->package_list[i].num_instances = GET_INSTANCE(temp);
			h->package_list[i].file_index = GET_PACKAGE(temp);
			bufptr += 4;
			
			temp = MAKE_DWORD(bufptr[0], bufptr[1], bufptr[2], bufptr[3]);
			h->package_list[i].flags = temp >> 24;
			h->package_list[i].data_loc = temp & 0x00ffffff;
			bufptr += 4;
		}
	}
	
	// Type info:
	h->type_list = malloc(h->num_types * sizeof (type_desc));
	{
		int i;
		
		for (i = 0; i < h->num_types; i++) {
			h->type_list[i].instance_count = 
					MAKE_WORD(bufptr[0], bufptr[1]);
			bufptr += 2;
		}
	}
	
	if ((uint32) (bufptr - buf) != h->packmem_list_offs) {
		bufptr = buf + h->packmem_list_offs;
	}
	{
		int i, j;
		int num_types, num_instances;
		uint32 temp;
		
		for (i = 0; i < h->num_packages; i++) {
			uint32 next_offset;
			if (h->res_flags) {
				// packaged
				char *temp;
				temp = get_file_name(buf, h, h->package_list[i].file_index);
				h->package_list[i].filename =
						(temp == NULL) ? NULL : strdup(temp);
			} else {
				// not packaged
				h->package_list[i].filename = NULL;
			}

			num_types = h->package_list[i].num_types;
			h->package_list[i].type_list =
					malloc(num_types * sizeof (packtype_desc));
			for (j = 0; j < num_types; j++) {
				temp = MAKE_DWORD(bufptr[0], bufptr[1], bufptr[2], bufptr[3]);
				h->package_list[i].type_list[j].type = GET_TYPE(temp);
				h->package_list[i].type_list[j].first_instance =
						GET_INSTANCE(temp);
				h->package_list[i].type_list[j].num_instances =
						GET_PACKAGE(temp);
				bufptr += 4;
			}

			if (h->res_flags) {
				// packaged
				next_offset = h->package_list[i].data_loc;
			}
			for (j = 0; j < num_types; j++) {
				int k;
				
				num_instances = h->package_list[i].type_list[j].num_instances;
				h->package_list[i].type_list[j].instances =
						malloc(sizeof (packdef_instance) * num_instances);
				for (k = 0; k < num_instances; k++) {
					if (h->res_flags) {
						// packaged
						h->package_list[i].type_list[j].instances[k].size = 
								4 * MAKE_WORD(bufptr[0], bufptr[1]);
						h->package_list[i].type_list[j].instances[k].offset =
								next_offset;
						h->package_list[i].type_list[j].instances[k].
								filename = NULL;
						next_offset += h->package_list[i].type_list[j].
								instances[k].size;
					} else {
						char *temp;
						h->package_list[i].type_list[j].instances[k].size = 0;
						h->package_list[i].type_list[j].instances[k].offset =
								0;
						temp = get_file_name(buf, h,
								MAKE_WORD(bufptr[0], bufptr[1]));
						h->package_list[i].type_list[j].instances[k].
								filename = (temp == NULL) ?
								NULL : strdup(temp);
					}
					bufptr += 2;
				}  // for k
			}  // for j
		} // for i
	}
	return h;
}

void
printIndex(const index_header *h, const uint8 *buf, FILE *out) {
	const uint8 *bufptr;
	
	fprintf(out, "0x00000000  File type:               %s\n",
			h->res_flags ? "packaged" : "not packaged");
	fprintf(out, "0x00000002  packmem_list_offs:       0x%08x\n",
			h->packmem_list_offs);
	fprintf(out, "0x00000006  path_list_offs:          0x%08x\n",
			h->path_list_offs);
	fprintf(out, "0x0000000a  file_list_offs:          0x%08x\n",
			h->file_list_offs);
	fprintf(out, "0x0000000e  Number of packages:      %d\n",
			h->num_packages);
	fprintf(out, "0x00000010  Number of package types: %d\n",
			h->num_types);
	fprintf(out, "0x00000012  Header length:           0x%08x\n",
			h->header_len);
	
	bufptr = buf + 0x16;
	fprintf(out, "0x00000016  package info:\n");
	{
		int i;
		
		for (i = 0; i < h->num_packages; i++) {
			fprintf(out, "0x%08x    #%d, %d types, %d instances, "
					"file index 0x%04x, ", bufptr - buf, i + 1,
					h->package_list[i].num_types,
					h->package_list[i].num_instances,
					h->package_list[i].file_index);
			bufptr += 4;
			
			if (h->package_list[i].flags != 0xff) {
				fprintf(out, "BAD OFFSET!\n");
			} else {
				fprintf(out, "offset 0x%06x\n",
						h->package_list[i].data_loc);
			}
			bufptr += 4;
		}
	}
	
	// Type info:
	fprintf(out, "0x%08x  type info:\n", bufptr - buf);
	{
		int i;
		
		for (i = 0; i < h->num_types; i++) {
			fprintf(out, "0x%08x    #%d, %d instances\n",
					bufptr - buf, i + 1, h->type_list[i].instance_count);
			bufptr += 2;
		}
	}
	
	if ((uint32) (bufptr - buf) != h->packmem_list_offs) {
		fprintf(out, "PACKMEM_LIST NOT IMMEDIATELY AFTER TYPE LIST!\n");
		bufptr = buf + h->packmem_list_offs;
	}
	fprintf(out, "0x%08x  packmem info:\n", bufptr - buf);
	{
		int i, j;
		int num_types, num_instances;
		
		for (i = 0; i < h->num_packages; i++) {
			if (h->res_flags) {
				// packaged
				fprintf(out, "0x%08x    Package #%d (%s):\n",
						bufptr - buf, i + 1,
						(h->package_list[i].filename == NULL) ?
						"<<UNNAMED>>" : h->package_list[i].filename);
			} else {
				// not packaged
				fprintf(out, "0x%08x    Package #%d:\n",
						bufptr - buf, i + 1);
			}

			num_types = h->package_list[i].num_types;
			for (j = 0; j < num_types; j++) {
				fprintf(out, "0x%08x      Type #%d: type %d, "
						"%d instances starting with #%d\n",
						bufptr - buf, j,
						h->package_list[i].type_list[j].type,
						h->package_list[i].type_list[j].num_instances,
						h->package_list[i].type_list[j].first_instance);
				bufptr += 4;
			}

			for (j = 0; j < num_types; j++) {
				int k;
				
				num_instances = h->package_list[i].type_list[j].num_instances;
				for (k = 0; k < num_instances; k++) {
					if (h->res_flags) {
						// packaged
						fprintf(out, "0x%08x      Instance #%d of "
								"type %d: size %d bytes\n", bufptr - buf,
								k + h->package_list[i].type_list[j].
								first_instance,
								h->package_list[i].type_list[j].type,
								h->package_list[i].type_list[j].instances[k].
								size);
					} else {
						fprintf(out, "0x%08x      Instance #%d of "
								"type %d: %s\n", bufptr - buf,
								k + h->package_list[i].type_list[j].
								first_instance,
								h->package_list[i].type_list[j].type,
								(h->package_list[i].type_list[j].instances[k].
								filename == NULL) ? "<<UNNAMED>>" :
								h->package_list[i].type_list[j].instances[k].
								filename);
					}
					bufptr += 2;
				}  // for k
			}  // for j
		} // for i
	}
}

void
listResources(const index_header *h, const uint8 *buf, FILE *out) {
	int i, j;
	int num_types, num_instances;
	char *filename;
	
	for (i = 0; i < h->num_packages; i++) {
		if (h->res_flags) {
			// packaged
			filename = (h->package_list[i].filename == NULL) ?
					"<<UNNAMED>>" : h->package_list[i].filename;
		}

		num_types = h->package_list[i].num_types;
		for (j = 0; j < num_types; j++) {
			int k;
			
			num_instances = h->package_list[i].type_list[j].num_instances;
			for (k = 0; k < num_instances; k++) {
				if (!h->res_flags) {
					filename = (h->package_list[i].type_list[j].instances[k].
							filename == NULL) ? "<<UNNAMED>>" :
							h->package_list[i].type_list[j].instances[k].
							filename;
				}
				fprintf(out, "0x%08x  %s\n",
						MAKE_RESOURCE(i + 1,
						h->package_list[i].type_list[j].type,
						h->package_list[i].type_list[j].first_instance + k),
						filename);
			}  // for k
		}  // for j
	} // for i
	(void) buf;
}

char *
get_file_name(const uint8 *buf, const index_header *h, uint16 file_index) {
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

void
writeFile(const char *file, const uint8 *data, size_t len) {
	int fd;

	// check if all the path components exist
	{
		const char *ptr;
		char path[MAXPATHLEN];
		
		ptr = file;
		if (ptr[0] == '/')
			ptr++;
		while (1) {
			ptr = strchr(ptr, '/');
			if (ptr == NULL)
				break;
			ptr++;
			memcpy(path, file, ptr - file);
			path[ptr - file] = '\0';
			if (mkdir(path, 0777) == -1) {
				if (errno == EEXIST)
					continue;
				fprintf(stderr, "Error: Could not make directory %s: %s\n",
						path, strerror(errno));
				exit(EXIT_FAILURE);
			}
		}
	}
	
	fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (fd == -1) {
		perror("Could not open file for writing");
		exit(EXIT_FAILURE);
	}
	while (len > 0) {
		ssize_t written;
		written = write(fd, data, len);
		if (written < 0) {
			if (errno != EINTR) {
				perror("write failed");
				exit(1);
			}
		}
		len -= written;
		data += written;
	}
	close(fd);
}


void
writeFiles(const index_header *h, const uint8 *data, const char *path) {
	int i;
	char filename[MAXPATHLEN];
	int num_types, num_instances;
	
	for (i = 0; i < h->num_packages; i++) {
		int j;

		num_types = h->package_list[i].num_types;
		for (j = 0; j < num_types; j++) {
			int k;
			
			num_instances = h->package_list[i].type_list[j].num_instances;
			for (k = 0; k < num_instances; k++) {
				sprintf(filename, "%s/%08x",
						path,
						MAKE_RESOURCE(i + 1,
						h->package_list[i].type_list[j].type,
						h->package_list[i].type_list[j].first_instance + k));
#if 0
				sprintf(filename, "%s/%08x=%03x-%02x-%03x",
						path,
						MAKE_RESOURCE(i + 1,
						h->package_list[i].type_list[j].type,
						h->package_list[i].type_list[j].first_instance + k),
						i + 1,
						h->package_list[i].type_list[j].type,
						h->package_list[i].type_list[j].first_instance + k);
#endif
				writeFile(filename,
						&data[h->package_list[i].type_list[j].instances[k].
						offset],
						h->package_list[i].type_list[j].instances[k].size);
			}
		}
	}
}

