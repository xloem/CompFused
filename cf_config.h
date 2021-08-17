#ifndef __CF_CONFIG__
#define __CF_CONFIG__

#include "cf_compression.h"

#define CF_BACKENDPATH_SIZE	1024

typedef struct {
	Cf_error_f err;
	Cf_compressor_f compressor;
	Cf_decompressor_f decompressor;
	Cf_buffersizer_f buffersizer;

	Cf_compression compression_id;

	int map_size;

	int cache_size;
	int cache_ttl;

	char backendpath[CF_BACKENDPATH_SIZE];
	char **blacklist;
	int blacklist_size;

	/* Force or not a full fs scan upon start up */
	int forced_scan;
} Cf_config_base;

typedef Cf_config_base *Cf_config;

Cf_config cf_config_alloc();
void cf_config_dealloc();

#define cf_config_get_error(c)			(c)->err
#define cf_config_get_compressor(c)		(c)->compressor
#define cf_config_get_decompressor(c)		(c)->decompressor
#define cf_config_get_buffersizer(c)		(c)->buffersizer
#define cf_config_get_compression_id(c)		(c)->compression_id
#define cf_config_get_blacklist_size(c)		(c)->blacklist_size
#define cf_config_get_blacklist(c)		(c)->blacklist
#define cf_config_get_map_size(c)		(c)->map_size
#define cf_config_get_cache_size(c)		(c)->cache_size
#define cf_config_get_cache_ttl(c)		(c)->cache_ttl
#define cf_config_get_forced_scan(c)		(c)->forced_scan

#define cf_config_set_error(c,f)		cf_config_get_error(c) = (f)
#define cf_config_set_compressor(c,f)		cf_config_get_compressor(c) = (f)
#define cf_config_set_decompressor(c,f)		cf_config_get_decompressor(c) = (f)
#define cf_config_set_buffersizer(c,f)		cf_config_get_buffersizer(c) = (f)
#define cf_config_set_compression_id(c,id)	cf_config_get_compression_id(c) = (id)
#define cf_config_set_blacklist_size(c,s)	cf_config_get_blacklist_size(c) = (s)
#define cf_config_set_blacklist(c,l)		cf_config_get_blacklist(c) = (l)
#define cf_config_set_map_size(c,s)		cf_config_get_map_size(c) = (s)
#define cf_config_set_cache_size(c,s)		cf_config_get_cache_size(c) = (s)
#define cf_config_set_cache_ttl(c,s)		cf_config_get_cache_ttl(c) = (s)
#define cf_config_set_forced_scan(c,f)		cf_config_get_forced_scan(c) = (f)


#define cf_config_get_backendpath(c)		(c)->backendpath
void cf_config_set_backendpath(Cf_config c, char *path);

void cf_config_init(Cf_config c);
FILE* cf_config_open(char *path);
void cf_config_read(Cf_config c, char *path);

#endif
