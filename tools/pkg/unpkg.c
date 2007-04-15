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
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <string.h>
#include <errno.h>

#include "unpkg.h"

// Variations on the packaging file format.
typedef enum {
	Type_sc1,  // PC/Amiga version of SC1
	Type_pc,   // PC version of SC2
	Type_3do,  // 3DO version of SC2; also The Horde (PC version)
} PackagingType;

struct options {
	char *infile;
	char *outdir;
	char list;
	char print;
	char verbose;
	PackagingType type;
};


index_header *readIndex(const struct options *opts, const uint8 *buf);
void printIndex(const struct options *opts, const index_header *h,
		const uint8 *buf, FILE *out);
char *getFileName(const uint8 *buf, const index_header *h,
		uint16 file_index);
void writeFiles(const index_header *h, const uint8 *data, const char *path);
void parse_arguments(int argc, char *argv[], struct options *opts);
void analyzeFilesStats(struct options *opts, const index_header *h,
		const FilesStats *filesStats);
void listResources(const index_header *h, const uint8 *buf, FILE *out);
FilesStats *createFilesStats(const index_header *h, const uint8 *buf,
		size_t bufSize);


int
main(int argc, char *argv[]) {
	int in;
	struct stat sb;
	uint8 *buf;
	index_header *h;
	struct options opts;
	off_t inSize;

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
	inSize = sb.st_size;
	
	buf = mmap(NULL, inSize, PROT_READ, MAP_SHARED, in, 0);
	if (buf == MAP_FAILED) {
		perror("mmap() failed");
		return EXIT_FAILURE;
	}
	close(in);

	h = readIndex(&opts, buf);
	if (opts.print) {
		printIndex(&opts, h, buf, stdout);
		fflush(stdout);
	}

	if (opts.list) {
		listResources(h, buf, stdout);
		fflush(stdout);
	}

	{
		FilesStats *stats = createFilesStats(h, buf, inSize);
		analyzeFilesStats(&opts, h, stats);
	}

	if (opts.outdir != NULL) {
		size_t len;
		struct stat sb;

		if (!h->packaged) {
			fprintf(stderr, "Error: Cannot unpack data from non-packaged "
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
	munmap(buf, inSize);
	return EXIT_SUCCESS;
}

void
usage() {
	fprintf(stderr, "unpkg [-o <outputdir>] [-l] [-p] [-v] <infile>\n"
			"\t-o  unpack to 'outputdir'\n"
			"\t-l  list the resources\n"
			"\t-p  print the index\n"
			"\t-t  package file type; one of \"sc1\", \"pc\", or \"3do\"\n"
			"\t    \"3do\" also works for the PC version of The Horde\n"
			"\t-v  verbose mode\n");
}

void
parse_arguments(int argc, char *argv[], struct options *opts) {
	char ch;
	
	memset(opts, '\0', sizeof (struct options));
	opts->type = Type_3do;
	while (1) {
		ch = getopt(argc, argv, "hlo:pt:v");
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
			case 't':
				if (strcmp(optarg, "sc1") == 0 ||
						strcmp(optarg, "SC1") == 0) {
					opts->type = Type_sc1;
				} else if (strcmp(optarg, "pc") == 0 ||
						strcmp(optarg, "PC") == 0) {
					opts->type = Type_pc;
				} else if (strcmp(optarg, "3do") == 0 ||
						strcmp(optarg, "3DO") == 0) {
					opts->type = Type_3do;
				} else {
					fprintf(stderr, "Invalid package file type.\n");
					usage();
					exit(EXIT_FAILURE);
				}
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

// The various file formats reserve 2 bytes for the size of a resource.
// The actual size is the value from these bytes multiplied by a value
// which depends on the variant of the packaging file format.
int
resourceSizeMultiplier(PackagingType type) {
	switch (type) {
		case Type_sc1: return 1;
		case Type_pc:  return 2;
		case Type_3do: return 4;
	}
	abort();  // Should not happen.
}

index_header *
readIndex(const struct options *opts, const uint8 *buf) {
	index_header *h;
	uint16 res_flags;
	const uint8 *bufptr;
	
	h = malloc(sizeof (index_header));
	res_flags = MAKE_WORD(buf[0x00], buf[0x01]);
	h->packaged = ((res_flags & 0x1) == 0x1) ? true : false;
	switch (opts->type) {
		case Type_sc1:
			h->path_list_offs =
					MAKE_DWORD(buf[0x02], buf[0x03], buf[0x04], buf[0x05]);
			h->file_list_offs =
					MAKE_DWORD(buf[0x06], buf[0x07], buf[0x08], buf[0x09]);
			h->packmem_list_offs =
					MAKE_DWORD(buf[0x0a], buf[0x0b], buf[0x0c], buf[0x0d]);
			h->num_types = MAKE_WORD(buf[0x0e], buf[0x0f]);
			h->num_packages = MAKE_WORD(buf[0x12], buf[0x13]);
			h->header_len = 0;
			break;
		case Type_pc:
			h->packmem_list_offs =
					MAKE_DWORD(buf[0x02], buf[0x03], buf[0x04], buf[0x05]);
			h->path_list_offs =
					MAKE_DWORD(buf[0x06], buf[0x07], buf[0x08], buf[0x09]);
			h->file_list_offs =
					MAKE_DWORD(buf[0x0a], buf[0x0b], buf[0x0c], buf[0x0d]);
			h->num_packages = MAKE_WORD(buf[0x0e], buf[0x0f]);
			h->num_types = buf[0x10];
			h->header_len = MAKE_DWORD(
					buf[0x12], buf[0x13], buf[0x14], buf[0x15]);
			break;
		case Type_3do:
			h->packmem_list_offs =
					MAKE_DWORD(buf[0x02], buf[0x03], buf[0x04], buf[0x05]);
			h->path_list_offs =
					MAKE_DWORD(buf[0x06], buf[0x07], buf[0x08], buf[0x09]);
			h->file_list_offs =
					MAKE_DWORD(buf[0x0a], buf[0x0b], buf[0x0c], buf[0x0d]);
			h->num_packages = MAKE_WORD(buf[0x0e], buf[0x0f]);
			h->num_types = MAKE_WORD(buf[0x10], buf[0x11]);
			h->header_len = MAKE_DWORD(
					buf[0x12], buf[0x13], buf[0x14], buf[0x15]);
			break;
	}
	
	bufptr = buf + 0x16;
	h->package_list = malloc(h->num_packages * sizeof (package_desc));
	{
		int i;
		uint32 temp;
		
		for (i = 0; i < h->num_packages; i++) {
			package_desc *package = &h->package_list[i];

			package->index = i + 1;
			temp = MAKE_DWORD(bufptr[0], bufptr[1], bufptr[2], bufptr[3]);
			package->num_types = GET_TYPE(temp);
			package->num_instances = GET_INSTANCE(temp);
			package->file_index = GET_PACKAGE(temp);
			bufptr += 4;
			
			temp = MAKE_DWORD(bufptr[0], bufptr[1], bufptr[2], bufptr[3]);
			package->flags = temp >> 24;
			package->data_loc = temp & 0x00ffffff;
			package->size = 0;
					// Will be calculated from the sizes of the individual
					// resource instances in the package below.
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
		
		for (i = 0; i < h->num_packages; i++) {
			package_desc *package = &h->package_list[i];

			uint32 next_offset;
			if (h->packaged) {
				// One file name per package
				char *temp;
				temp = getFileName(buf, h, package->file_index);
				package->filename = (temp == NULL) ? NULL : strdup(temp);
			} else {
				// File name is specified per resource instance.
				package->filename = NULL;
			}

			num_types = package->num_types;
			package->type_list = malloc(num_types * sizeof (packtype_desc));
			for (j = 0; j < num_types; j++) {
				packtype_desc *type = &package->type_list[j];
				uint32 temp;

				temp = MAKE_DWORD(bufptr[0], bufptr[1], bufptr[2], bufptr[3]);
				type->type = GET_TYPE(temp);
				type->first_instance = GET_INSTANCE(temp);
				type->num_instances = GET_PACKAGE(temp);
				bufptr += 4;
			}

			next_offset = package->data_loc;
			for (j = 0; j < num_types; j++) {
				packtype_desc *type = &package->type_list[j];
				int k;
				
				num_instances = type->num_instances;
				type->instances =
						malloc(sizeof (packdef_instance) * num_instances);
				for (k = 0; k < num_instances; k++) {
					packdef_instance *instance = &type->instances[k];
				
					if (h->packaged) {
						instance->size = resourceSizeMultiplier(opts->type) *
								MAKE_WORD(bufptr[0], bufptr[1]);
						instance->offset = next_offset;
						next_offset += instance->size;

						instance->file_index = (uint16) -1;
						instance->filename = NULL;
					} else {
						char *temp;
						instance->size = 0;
						instance->offset = 0;
						instance->file_index =
								MAKE_WORD(bufptr[0], bufptr[1]);
						temp = getFileName(buf, h, instance->file_index);
						instance->filename = (temp == NULL) ?
								NULL : strdup(temp);
					}
					bufptr += 2;
				}  // for k

				package->size = next_offset - package->data_loc;
			}  // for j
		} // for i
	}
	return h;
}

void
printIndex(const struct options *opts, const index_header *h,
		const uint8 *buf, FILE *out) {
	const uint8 *bufptr;
	
	fprintf(out, "0x00000000  File type:               %s\n",
			h->packaged ? "packaged" : "not packaged");
	switch (opts->type) {
		case Type_sc1:
			fprintf(out, "0x00000002  path_list_offs:          0x%08x\n",
					h->path_list_offs);
			fprintf(out, "0x00000006  file_list_offs:          0x%08x\n",
					h->file_list_offs);
			fprintf(out, "0x0000000a  packmem_list_offs:       0x%08x\n",
					h->packmem_list_offs);
			fprintf(out, "0x0000000e  Number of package types: %d\n",
					h->num_types);
			fprintf(out, "0x00000010  Unknown:                 0x%04x\n",
					MAKE_WORD(buf[0x10], buf[0x11]));
			fprintf(out, "0x00000012  Number of packages:      %d\n",
					h->num_packages);
			fprintf(out, "0x00000013  Unknown:                 0x%04x\n",
					MAKE_WORD(buf[0x13], buf[0x14]));
			break;
		case Type_pc:
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
			fprintf(out, "0x00000011  Unknown:                 0x%02x\n",
					buf[0x11]);
			fprintf(out, "0x00000012  Header length:           0x%08x\n",
					h->header_len);
			break;
		case Type_3do:
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
			break;
	}
	
	bufptr = buf + 0x16;
	fprintf(out, "0x00000016  package info:\n");
	{
		int i;
		
		for (i = 0; i < h->num_packages; i++) {
			package_desc *package = &h->package_list[i];
			fprintf(out, "0x%08x    #%d, %d types, %d instances, "
					"file index 0x%04x, ", bufptr - buf, package->index,
					package->num_types, package->num_instances,
					package->file_index);
			bufptr += 4;
			
			if (package->flags != 0xff) {
				fprintf(out, "BAD OFFSET!\n"); } else {
				fprintf(out, "offset 0x%06x\n", package->data_loc);
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
			package_desc *package = &h->package_list[i];
			
			if (h->packaged) {
				fprintf(out, "0x%08x    Package #%d (%s):\n",
						bufptr - buf, package->index,
						(package->filename == NULL) ?
						"<<INTERNAL>>" : package->filename);
			} else {
				fprintf(out, "0x%08x    Package #%d:\n",
						bufptr - buf, package->index);
			}

			num_types = package->num_types;
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
				packtype_desc *type = &package->type_list[j];
				int k;
				
				num_instances = type->num_instances;
				for (k = 0; k < num_instances; k++) {
					packdef_instance *instance = &type->instances[k];
					uint32 resId = MAKE_RESOURCE(package->index, type->type,
							type->first_instance + k);

					if (h->packaged) {
						fprintf(out, "0x%08x      [%08x]: Instance #%d of "
								"type %d: size %d bytes\n", bufptr - buf,
								resId, k + type->first_instance,
								type->type, instance->size);
					} else {
						fprintf(out, "0x%08x      [%08x]: Instance #%d of "
								"type %d: %s\n", bufptr - buf, resId,
								k + type->first_instance, type->type,
								(instance->filename == NULL) ?
								"<<UNNAMED>>" : instance->filename);
							// NB. NULL filenames should not occur for
							// non-packaged files.
					}

					bufptr += 2;
				}  // for k
			}  // for j
		} // for i
	}
}

// Returns 1 more than the largest used file index, or 0 if there are
// no file indices used at all (this can only happen if the package is
// empty).
size_t
countFileIndexUpper(const index_header *h) {
	int i;
	size_t result;

	result = 0;
	for (i = 0; i < h->num_packages; i++) {
		package_desc *package = &h->package_list[i];
		
		if (package->file_index >= result)
			result = package->file_index + 1;
	}
	return result;
}

// returns "(uint32) -1" on error, in which case errno is set.
uint32
fileSize(const char *fileName) {
	struct stat sb;

	if (stat(fileName, &sb) == -1) {
		// errno is set
		return (uint32) -1;
	}

	return sb.st_size;
}

int
packageOffsetComparator(package_desc **p1, package_desc **p2) {
	if ((*p1)->file_index < (*p2)->file_index)
		return -1;
	if ((*p1)->file_index > (*p2)->file_index)
		return 1;

	if ((*p1)->data_loc < (*p2)->data_loc)
		return -1;
	if ((*p1)->data_loc > (*p2)->data_loc)
		return 1;

	return 0;
}

FilesStats *
createFilesStats(const index_header *h, const uint8 *buf, size_t bufSize) {
	FilesStats *result;
	size_t statsI;
	
	assert(h->packaged);

	result = malloc(sizeof(FilesStats));
	result->indexUpper = countFileIndexUpper(h);
	result->fileStats = malloc(result->indexUpper * sizeof(FileStats));

	// Initialise the FileStats structures.
	for (statsI = 0; statsI < result->indexUpper; statsI++) {
		FileStats *stats = &result->fileStats[statsI];
		stats->numPackages = 0;
		stats->fileSize = 0;
	}

	// Count the references to each file.
	{
		int i;
		for (i = 0; i < h->num_packages; i++) {
			package_desc *package = &h->package_list[i];
			result->fileStats[package->file_index].numPackages++;
		}
	}

	// Allocate the instances arrays of the FileStat structures.
	for (statsI = 0; statsI < result->indexUpper; statsI++) {
		FileStats *stats = &result->fileStats[statsI];

		stats->packages =
				malloc(stats->numPackages * sizeof (packdef_instance *));
	}
	
	// Clear the numPackages field of the FileStats structures,
	// so it can grow again as the FileStats array is filled.
	for (statsI = 0; statsI < result->indexUpper; statsI++) {
		FileStats *stats = &result->fileStats[statsI];
		stats->numPackages = 0;
	}

	// Fill the instances arrays of the FileStat structures.
	{
		int i;
		for (i = 0; i < h->num_packages; i++) {
			package_desc *package = &h->package_list[i];
			FileStats *stats = &result->fileStats[package->file_index];

			stats->packages[stats->numPackages] = package;
			stats->numPackages++;
		}
	}

	// Sort the packages array of each FileStats structure.
	for (statsI = 0; statsI < result->indexUpper; statsI++) {
		FileStats *stats = &result->fileStats[statsI];
		qsort(stats->packages, stats->numPackages, sizeof (package_desc *),
				(int (*)(const void *, const void *))
				packageOffsetComparator);
	}

	// Determine the sizes of the files.
	for (statsI = 0; statsI < result->indexUpper; statsI++) {
		FileStats *stats = &result->fileStats[statsI];
		const char *fileName;

		fileName = getFileName(buf, h, statsI);
		if (fileName == NULL) {
			stats->fileSize = bufSize;
		} else {
			stats->fileSize = fileSize(fileName);
			if (stats->fileSize == (uint32) -1) {
				fprintf(stderr, "Could not determine size of file %s: %s\n",
						fileName, strerror(errno));
				stats->fileSize = 0;
			}
		}
	}

	return result;
}

packdef_instance *
lastInstance(package_desc *pack) {
	packtype_desc *type = &pack->type_list[pack->num_types - 1];
	packdef_instance *instance = &type->instances[type->num_instances - 1];
	return instance;
}

void
analyzeFilesStats(struct options *opts, const index_header *h,
		const FilesStats *filesStats) {
	size_t filesStatsI;
	int multiplier = resourceSizeMultiplier(opts->type);

	for (filesStatsI = 0; filesStatsI < filesStats->indexUpper;
			filesStatsI++) {
		FileStats *stats = &filesStats->fileStats[filesStatsI];
		size_t packI;

		for (packI = 0; packI < stats->numPackages; packI++) {
			package_desc *pack = stats->packages[packI];
			uint32 off1 = pack->data_loc;
			uint32 off2;
			uint32 expectedSize;

			if (packI < stats->numPackages - 1) {
				off2 = stats->packages[packI + 1]->data_loc;
			} else {
				off2 = stats->fileSize;
				if (off2 == (uint32) -1)
					break;
			}
			expectedSize = off2 - off1;

			if (pack->size != expectedSize) {
				uint32 missing = expectedSize - pack->size;
				// The package format only reserves two bytes for the
				// size of a resource, and a multiplier, dependant on
				// the variant of the package format, is used to determine
				// the real value.
				// In the resource files of the PC version of Star Control
				// II, there are eight files which are still too large for
				// the field, so an overflow occurs, which must be
				// corrected. Fortunately, the only occurances are in
				// packages which only have one instance, so the difference
				// in offset between consecutive packages can be used as the
				// real size.
				// In the PC version of the Horde, an overflow exists for
				// two packages which consist of more than one resource
				// instance. We assume that it is the last instance which
				// has the invalid size. This is reasonable, as an invalid
				// size would cause the wrong offset to be used for
				// subsequent instances in the package, which would likely
				// have been noticed due to corruption in the game.
				// In the Amiga version of SC1, an overflow exists in
				// one package with 2 instances.
				// The DOS version of SC1 and the 3DO version of SC2 have
				// no such overflows.
				fprintf(stderr, "Package #%d: Size 0x%08x; "
						"Expected 0x%08x. %d instances. ", pack->index,
						pack->size, expectedSize, pack->num_instances);
				if (pack->size < expectedSize &&
						(missing % (multiplier * 0x10000) == 0)) {
					fprintf(stderr, "Correcting.\n");
					pack->size = expectedSize;
					lastInstance(pack)->size += missing;
				} else if (packI == stats->numPackages - 1 &&
						pack->size + multiplier > expectedSize) {
					// It's possible that the package itself came from
					// another package; in that case, there might be up to
					// multiplier - 1 bytes of padding to the end of the
					// package file to make the size a multiple of
					// 'multiplier'.
					fprintf(stderr, "Just padding.\n");
				} else {
					fprintf(stderr, "KEPT VALUE!\n");
				}
			}
		}
	}
}

void
listResources(const index_header *h, const uint8 *buf, FILE *out) {
	int i, j;
	int num_types, num_instances;
	char *filename;
	
	for (i = 0; i < h->num_packages; i++) {
		package_desc *package = &h->package_list[i];
		
		if (h->packaged) {
			// One filename for all resources in a package.
			filename = (package->filename == NULL) ?
					"<<INTERNAL>>" : package->filename;
		}

		num_types = package->num_types;
		for (j = 0; j < num_types; j++) {
			packtype_desc *type = &package->type_list[j];
			int k;
			
			num_instances = type->num_instances;
			for (k = 0; k < num_instances; k++) {
				packdef_instance *instance = &type->instances[k];
				if (!h->packaged)
					filename = (instance->filename == NULL) ?
							"<<UNNAMED>>" : instance->filename;
				fprintf(out, "0x%08x  %s\n", MAKE_RESOURCE(
						package->index, type->type,
						type->first_instance + k), filename);
			}  // for k
		}  // for j
	} // for i
	(void) buf;
}

char *
getFileName(const uint8 *buf, const index_header *h, uint16 file_index) {
	static char result[PATH_MAX];
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
		char path[PATH_MAX];
		
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
			if (errno == EINTR)
				continue;
			perror("write failed");
			exit(EXIT_FAILURE);
		}
		len -= written;
		data += written;
	}
	close(fd);
}

// If everything is ok, 0 is returned.
// Returns -1 on error, in which case errno is set.
int
copyFileSegment(const char *src, FILE *inFile, size_t offset,
		const char *dest, size_t size) {
	FILE *outFile;
#define BUFSIZE 4096
	uint8 buf[BUFSIZE];

	if (fseek(inFile, offset, SEEK_SET) == -1) {
		int savedErrno = errno;
		fprintf(stderr, "Seeking in input file %s (to %ld) failed: %s.\n",
				src, (long int) offset, strerror(errno));
		errno = savedErrno;
		return -1;
	}

	outFile = fopen(dest, "wb");
	if (outFile == NULL) {
		int savedErrno = errno;
		fprintf(stderr, "Opening output file %s failed: %s.\n",
				dest, strerror(errno));
		errno = savedErrno;
		return -1;
	}

	while (size > 0) {
		size_t toRead;
		size_t numRead;
		size_t numWritten;
		
		toRead = sizeof buf;
		if (toRead > size)
			toRead = size;

		do {
			numRead = fread(buf, 1, toRead, inFile);
		} while (ferror(inFile) && errno == EINTR);
		if (numRead != toRead) {
			if (ferror(inFile)) {
				int savedErrno = errno;
				fprintf(stderr, "Error reading file %s: %s.\n", src,
						strerror(errno));
				fclose(outFile);
				errno = savedErrno;
				return -1;
			} else {
				// EOF
				assert(feof(inFile));
				fprintf(stderr, "Unexpected end-of-file while reading "
						"file %s.\n", src);
				fclose(outFile);
				errno = EIO;
				return -1;
			}
		}

		do {
			numWritten = fwrite(buf, 1, numRead, outFile);
		} while (ferror(outFile) && errno == EINTR);
		if (numWritten != numRead) {
			int savedErrno = errno;
			fprintf(stderr, "Error writing to file %s: %s.\n", dest,
					strerror(errno));
			fclose(outFile);
			errno = savedErrno;
			return -1;
		}

		size -= numWritten;
	}

	fclose(outFile);
	return 0;
}

// Only for packaged files.
void
writeFiles(const index_header *h, const uint8 *data, const char *path) {
	int i;
	char filename[PATH_MAX];
	int num_types, num_instances;
	
	for (i = 0; i < h->num_packages; i++) {
		package_desc *package = &h->package_list[i];
		int j;
		FILE *inFile = NULL;

		if (package->filename != NULL) {
			inFile = fopen(package->filename, "rb");
			if (inFile == NULL) {
				fprintf(stderr, "Error: Could not open input file %s.\n",
						package->filename);
				continue;
			}
		}

		num_types = h->package_list[i].num_types;
		for (j = 0; j < num_types; j++) {
			packtype_desc *type = &package->type_list[j];
			int k;
			
			num_instances = type->num_instances;
			for (k = 0; k < num_instances; k++) {
				packdef_instance *instance = &type->instances[k];
				sprintf(filename, "%s/%08x", path,
						MAKE_RESOURCE(package->index, type->type,
						type->first_instance + k));
#if 0
				sprintf(filename, "%s/%08x=%03x-%02x-%03x",
						path,
						MAKE_RESOURCE(package->index, type->type,
						type->first_instance + k), package->index,
						type->type, type->first_instance + k);
#endif
				if (package->filename != NULL) {
					// Resource is contained in a different file.
					copyFileSegment(instance->filename, inFile,
							instance->offset, filename, instance->size);
				} else {
					// Resource is contained in this package file itself.
					writeFile(filename, &data[instance->offset],
							instance->size);
				}
			}
		}

		if (inFile != NULL)
			fclose(inFile);
	}
}


