#ifndef __CF_FILE__
#define __CF_FILE__

#include "cf_config.h"

/*
#define	MIN_SIZE		4096
*/
#define	MIN_SIZE		512

#define CF_SIGN1        	'\r'
#define CF_SIGN2        	'\n'
#define CF_VERSION_MAJ  	0
#define CF_VERSION_MIN		3


typedef struct cf_header_s {
	char    signature[4];
	char	compression;
	int     size;
} Cf_header;

#define cf_header_signature(h,i)        h.signature[i]
#define cf_header_size(h)               h.size
#define cf_header_compression(h)	h.compression


#define cf_header_get_signature(h,i)	(h).signature[i]
#define cf_header_get_size(h)    	(h).size
#define cf_header_get_compression(h)	(h).compression

#define cf_header_set_signature(h,i,s)	cf_header_get_signature(h,i) = (s)
#define cf_header_set_size(h,s)    	cf_header_get_size(h) = (s)
#define cf_header_set_compression(h,c)	cf_header_get_compression(h) = (c)

void cf_path(Cf_config c, const char *path, char *cf_path);

Cf_header cf_header_read(Cf_config c, int fd);
int cf_header_write(Cf_config c, int fd, int size);
int cf_data_read(Cf_config c, int fd, char *o_data, int o_size);
int cf_data_write(Cf_config c, int fd, char *o_data, int o_size, int no_compress);

int cf_file_no_compress(Cf_config c, const char *path);

int file_size(int fd);

#endif
