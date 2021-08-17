#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "cf_file.h"

int main(int argc, char **argv) {
	int fd;
	int sz;
	Cf_header h;
	struct stat st_buf;

	fd = open(argv[1], O_RDWR);

	fstat(fd, &st_buf);

	h = cf_header_read(NULL, fd);

	printf("compressed with ");
	switch (cf_header_get_compression(h)) {
		case CF_NONE:
			puts("none");
			break;

		case CF_LZO:
			puts("lzo");
			break;

		case CF_ZLIB:
			puts("zlib");
			break;

		case CF_BZIP2:
			puts("bzip2");
			break;
	}

	sz = file_size(fd);
	printf("compression: %d to %d (%2.1f%%)\n", cf_header_get_size(h), sz, 100*(float)sz/(float)cf_header_get_size(h));

	close(fd);

	return 0;
}
