#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include "cf_config.h"
#include "cf_file.h"
#include "cf_file.h"

void msg() {
	puts("Support for the compression format was NOT compiled into this binary. See Makefile CFLAGS");

	exit(1);
}

int main(int argc, char **argv) {
	int ifd;
	int ofd;
	int sz;
	Cf_header h;
	Cf_config config;
	int c;
	char *ifile = NULL;
	char *ofile = NULL;
	char *buffer;
     
	opterr = 0;

     	while ((c = getopt (argc, argv, "i:o:")) != -1) {
     		switch (c) {
      		case 'i':
			ifile = strdup(optarg);
        		break;

      		case 'o':
			ofile = strdup(optarg);
        		break;

		default:
			puts("The following option is unknown, quiting");
			putchar(c);
			puts("-----------------------");
			puts("Usage: cf_extract -i cffile -o normalfile");
			exit(1);
		}
	}

	if (!ifile || !ofile ) {
		puts("parameters missing, need both -i and -o");
		exit(2);
	}

	config = cf_config_alloc();

	ifd = open(ifile, O_RDONLY);
	ofd = open(ofile, O_WRONLY|O_CREAT);

	h = cf_header_read(NULL, ifd);
	buffer = (char *) malloc(sizeof(char) * cf_header_size(h));

	printf("compressed with ");
	switch (cf_header_get_compression(h)) {
		case CF_NONE:
			cf_config_set_decompressor(config, none_decompressor);
			puts("none");
			break;

		case CF_LZO:
			puts("lzo");
#ifdef CF_USE_LZO
			cf_config_set_decompressor(config, lzo_decompressor);
#else
			msg();
#endif
			break;

		case CF_ZLIB:
			puts("zlib");
#ifdef CF_USE_ZLIB
			cf_config_set_decompressor(config, (Cf_decompressor_f)uncompress);
#else
			msg();
#endif
			break;

		case CF_BZIP2:
			puts("bzip2");
#ifdef CF_USE_BZIP2
			cf_config_set_decompressor(config, bzip2_decompressor);
#else
			msg();
#endif
			break;
	}

	cf_data_read(config, ifd, buffer, cf_header_size(h));

	write(ofd, buffer, cf_header_size(h));

	sz = file_size(ifd);
	printf("compression: %d to %d (%2.1f%%)\n", cf_header_get_size(h), sz, 100*(float)sz/(float)cf_header_get_size(h));

	close(ifd);
	close(ofd);

	return 0;
}
