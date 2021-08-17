#ifdef linux
/* For pread()/pwrite() */
#define _XOPEN_SOURCE 500
#endif

#include <fcntl.h>
#include <dirent.h>
#include <errno.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "cf_file.h"


int file_size(int fd) {
    struct stat st;
    int res;

    res = fstat(fd, &st);
    if(res == -1)
        return -errno;

    return st.st_size;
}

void cf_path(Cf_config c, const char *path, char *cf_path) {
	strcpy(cf_path, cf_config_get_backendpath(c));
	strcat(cf_path, path);
}

Cf_header cf_header_read(Cf_config c, int fd) {
	Cf_header header;
	int res;

	memset(&header, 0, sizeof(Cf_header));
/*
	printf("%d\n", file_size(fd));
*/
	res = pread(fd, &header, sizeof(Cf_header), 0);
	if (res != sizeof(Cf_header)) {
		/*
		puts("ERROR: can't read file header");
		*/
		cf_header_set_size(header, 0);
		/* Activate compression by default */
		cf_header_set_compression(header, 1);
	} else {
		if (! ((cf_header_get_signature(header,0) == CF_SIGN1) && (cf_header_get_signature(header,1) == CF_SIGN2)))
			printf("header signature not correct [%d,%d]\n",
				cf_header_get_signature(header,0), cf_header_get_signature(header,1));

		if (! ((cf_header_get_signature(header,2) == CF_VERSION_MAJ) && (cf_header_get_signature(header,3) == CF_VERSION_MIN)))
			printf("header version not correct %d.%d\n", 
				cf_header_get_signature(header,2), cf_header_get_signature(header,3));
	}

	return header;
}

int cf_header_write(Cf_config c, int fd, int size) {
	Cf_header header;
	int res;

	memset(&header, 0, sizeof(Cf_header));

	/* Fill in the header */
	cf_header_set_signature(header,0, CF_SIGN1);
	cf_header_set_signature(header,1, CF_SIGN2);
	cf_header_set_signature(header,2, CF_VERSION_MAJ);
	cf_header_set_signature(header,3, CF_VERSION_MIN);
	cf_header_set_compression(header, cf_config_get_compression_id(c));
	cf_header_set_size(header, size);

	res = pwrite(fd, &header, sizeof(Cf_header), 0);
	if (res != sizeof(Cf_header)) {
		puts("ERROR: can't write file header");
		perror("desc.");

		exit(-3);

		return res;
	}

	return 0;
}

int cf_data_read(Cf_config c, int fd, char *o_data, int o_size) {
	int res;
	int decomp_size;
	int i_size;
	char *i_buffer;

	if (o_size < 0) {
		puts("impossible o_size in cf_data_read()");

		return 1;
	}


	/* Prepare new mem for data */
	i_size = file_size(fd) - sizeof(Cf_header);
	i_buffer = (char *) malloc(i_size);
	decomp_size = o_size;

	res = pread(fd, i_buffer, i_size, sizeof(Cf_header));
	if(res == -1)
		res = -errno;

	if(res != i_size)
		puts("cf_data_read failed");

	if (i_size > o_size)
		printf("cf_file is corrupt: data bigger than original data (%d>%d)\n", i_size, o_size);

	if (i_size < o_size) {
		/* Decompress data */
		if ((res = cf_config_get_decompressor(c)(o_data, &decomp_size, i_buffer, i_size)) != 0) {
			puts("decomp problem");
			cf_config_get_error(c)(res);

			return 1;
		}

		if (decomp_size != o_size)
			printf("ERROR: decomp error %d vs %d\n", decomp_size, o_size);
	} else {
		/* Data is not compressed */
		memcpy(o_data, i_buffer, i_size);
	}

	free(i_buffer);

	return 0;
}

int cf_data_write(Cf_config c, int fd, char *o_data, int o_size, int no_compress) {
	int res;
	int ret;
	int err = 0;
	int comp_size = o_size;
	int i_size;
	int too_small = 0;
	char *i_buffer;

	/* Prepare new mem for data */
	i_size = cf_config_get_buffersizer(c)(o_size);
	i_buffer = (char *) malloc(i_size);
	comp_size = i_size;

	if (o_size < MIN_SIZE)
		too_small = 1;

	/* Compress data */
	if (!no_compress && !too_small) {
		 if ((res = cf_config_get_compressor(c)(i_buffer, &comp_size, o_data, o_size)) != 0) {
			puts("comp problem");

			cf_config_get_error(c)(res);

			err = 1;
		}
	}

	if ((comp_size < o_size) && (err == 0) && (!too_small) && (!no_compress)) {
		/* Compressed write */
		res = pwrite(fd, i_buffer, comp_size, sizeof(Cf_header));
		ret = comp_size;

		if(res != comp_size) {
			puts("cf_data_write failed");
			perror("desc.");
		}
	} else {
		/* Raw write */
		res = pwrite(fd, o_data, o_size, sizeof(Cf_header));
		ret = o_size;

		if(res != o_size) {
			puts("cf_data_write failed");
			perror("desc.");
		}
	}

	/* Shorten the backend file */
	ftruncate(fd, sizeof(Cf_header)+ret);

	if(res == -1)
		res = -errno;

	free(i_buffer);

	return ret;
}

int cf_file_no_compress(Cf_config c, const char *path) {
	int i, len;
	int start, stop;
	int basename;
	char ext[5];
	int res = 0;

	/* Get extension starting from the back.

	sscanf(path, "%*[^.].%s", ext); Can not handle path names with dots.. :'(	
	
	*/
	stop = strlen(path);
	start = stop;
	while ((start > 0) && (path[start] != '/') && (path[start] != '.'))
		start--;

	/* we do not want the dot */
	basename = start+1;

	len = strlen(path+basename);
	/* No extension or too short... */
	if ((len < 2) || (len > 4))
		return 0;

	/* lower case */
	for (i=0; i<len; i++)
		ext[i] = tolower(path[basename+i]);

	ext[len]= '\0';

	i = 0;
	while ((i<cf_config_get_blacklist_size(c)) && (res == 0)) {
		res = !strcmp(ext, cf_config_get_blacklist(c)[i]);
		i++;
	}

	return res;
}
