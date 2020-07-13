/*
bin2h for Nintendont (Kernel)

Copyright (C) 2014 FIX94

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation version 2.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int main(int argc, char **argv)
{
	const char *src_bin;	// argv[1]
	const char *dest_h;	// argv[2]

	FILE *f;
	size_t fsize;
	unsigned char *bin;
	size_t basenamelen;
	char *basename = NULL;
	char *p;

	time_t curtime;
	struct tm *loctime;
	size_t i;

	// Syntax: bin2h src.bin dest.h
	if (argc < 3) {
		fprintf(stderr, "Syntax: %s src.bin dest.h\n", argv[0]);
		return EXIT_FAILURE;
	}

	src_bin = argv[1];
	dest_h = argv[2];

	/* read in file */
	f = fopen(src_bin, "rb");
	if (f == NULL) {
		fprintf(stderr, "*** ERROR reading source file: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}
	fseek(f, 0, SEEK_END);
	fsize = ftell(f);
	rewind(f);
	bin = (unsigned char*)malloc(fsize);
	fread(bin, 1, fsize, f);
	fclose(f);

	/* name for the .h content */
	basenamelen = strrchr(dest_h, '.') - dest_h;
	if(strchr(dest_h, '/') != NULL)
		basenamelen -= (strrchr(dest_h, '/')+1 - dest_h);

	basename = calloc(basenamelen+1, sizeof(char));
	if(strchr(argv[1], '/') != NULL)
		strncpy(basename, strrchr(argv[1], '/')+1, basenamelen);
	else
		strncpy(basename, argv[1], basenamelen);

	/* replace dashes with underscores */
	for (p = basename; *p != '\0'; p++) {
		if (*p == '-') {
			*p = '_';
		}
	}

	/* get creation time */
	curtime = time(NULL);
	loctime = localtime(&curtime);
	/* create .h file */
	f = fopen(dest_h, "w");
	if (!f) {
		fprintf(stderr, "*** ERROR creating destination file: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}
	fputs("/*\n",f);
	fprintf(f,"\tFilename    : %s\n", strchr(dest_h, '/') != NULL ? strrchr(dest_h, '/')+1 : dest_h);
	fprintf(f,"\tDate created: %s", asctime(loctime));
	fputs("*/\n\n",f);
	fprintf(f, "#define %s_size 0x%x\n\n", basename, (unsigned int)fsize);
	fprintf(f, "const unsigned char %s[] = {", basename);
	free(basename);

	for (i = 0; i < fsize; ) {
		if((i % 16) == 0)
			fputs("\n",f);
		if((i % 4) == 0)
			fputs("\t",f);
		fprintf(f,"0x%02X", *(bin+i));
		i++;
		if(i < fsize)
			fputs(", ",f);
	}

	fprintf(f,"\n};\n");
	fclose(f);
	free(bin);
	return 0;
}
