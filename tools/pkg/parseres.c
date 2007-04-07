/*
 * Parses resinst.h files
 * By Serge van den Boom (svdb@stack.nl), 20021023
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

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/param.h>

#include "parseres.h"

#define BUFLEN 8192
#define ITEMS_AT_ONCE  16


void addItems(ResourceItemList *items, const char *filename);
int itemSort(const void *a1, const void *a2);

int
main(int argc, char *argv[]) {
	ResourceItemList items;
	
	if (argc != 2) {
		fprintf(stderr, "Syntax: parseres <infile>\n");
		return EXIT_FAILURE;
	}
	items.numItems = 0;
	items.item = NULL;
	addItems(&items, argv[1]);
//	fprintf(stderr, "%d items\n", items.numItems);
	qsort(items.item, items.numItems, sizeof (ResourceItem), itemSort);
	{
		size_t i;
		for (i = 0; i < items.numItems; i++)
			fprintf(stdout, "0x%08x  %s\n",
					items.item[i].res,
					items.item[i].name);
	}
	return EXIT_SUCCESS;
}

int
itemSort(const void *a1, const void *a2) {
	return ((ResourceItem *) a1)->res >= ((ResourceItem *) a2)->res;
}

void
addItems(ResourceItemList *items, const char *filename) {
	FILE *file;
	char *base;
	char buf[BUFLEN];
	char *ptr;
	size_t maxItems;
	int line;
	
	file = fopen(filename, "r");
	if (file == NULL) {
		fprintf(stderr, "Could not open %s.\n", filename);
		exit(EXIT_FAILURE);
	}
	ptr = strrchr(filename, '/');
	if (ptr == NULL) {
		base = "";
	} else {
		base = alloca(ptr - filename + 2);
		strncpy(base, filename, ptr - filename + 1);
		base[ptr - filename + 1] = '\0';
	}
	
	maxItems = items->numItems;
	line = 0;
	while (1) {
		if (fgets(buf, BUFLEN, file) == NULL) {
			if (feof(file))
				break;
			fprintf(stderr, "Error: %s, line %d: Error while reading: %s\n",
					filename, line, strerror(errno));
		}
		line++;
#if 0
		len = strlen(buf);
		if (len == 0)
			continue;
		while (buf[len - 1] == '\n' || buf[len - 1] == '\r') {
			len--;
			buf[len] = '\0';
		}
#endif
		if (strncmp(buf, "#include \"", 10) == 0) {
			char *endptr;
			char name[MAXPATHLEN];
			
			ptr = buf + 10;
			endptr = strchr(ptr, '\"');
			if (endptr == NULL) {
				fprintf(stderr, "Error: %s, line %d: \" expected.\n",
						filename, line);
				exit(EXIT_FAILURE);
			}
			*endptr = '\0';
			strcpy(name, base);
			strcat(name, ptr);
			addItems(items, name);
			continue;
		}
		if (strncmp(buf, "#define", 7) == 0) {
			char *name;
			uint32 res;
				ptr = buf + 7;
			while (isspace(*ptr))
				ptr++;
			name = ptr;
			while (!isspace(*ptr)) {
				if (*ptr == '\0') {
					fprintf(stderr, "Error: %s, line %d: unexpected end of "
							"line.\n", filename, line);
					exit(EXIT_FAILURE);
				}
				ptr++;
			}
			*ptr = '\0';
			ptr++;
			res = strtoul(ptr, NULL, 0);
			// ignore the rest of the line
			if (items->numItems >= maxItems) {
				items->item = realloc(items->item, sizeof (ResourceItem) *
						(items->numItems + ITEMS_AT_ONCE));
				maxItems = items->numItems + ITEMS_AT_ONCE;
			}
			items->item[items->numItems].res = res;
			items->item[items->numItems].name = strdup(name);
#if 0
			fprintf(stderr, "0x%08x  %s\n",
					items->item[items->numItems].res,
					items->item[items->numItems].name);
#endif
			items->numItems++;
			continue;
		}
	}
	items->item = realloc(items->item, sizeof (ResourceItem) *
			(items->numItems));
	fclose(file);
}

