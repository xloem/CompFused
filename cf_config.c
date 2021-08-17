#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <dirent.h>
#include <ctype.h>

#include "cf_config.h"
/*
#include "cf_defines.h"
*/
#include "cf_log.h"

Cf_config cf_config_alloc() {
	Cf_config ret;

	ret = (Cf_config) malloc(sizeof(Cf_config_base));
	if (!ret)
		puts("cf_config_alloc() failed");

	return ret;
}

void cf_config_dealloc(Cf_config c) {
	free(c);
}

void cf_config_set_backendpath(Cf_config c, char *path) {
	strcpy(cf_config_get_backendpath(c), path);
}

void cf_config_blacklist_add(Cf_config c, char *e) {
	char **tmp;
	char *entry;

	/* Get more mem */	
	tmp = (char **) realloc(cf_config_get_blacklist(c), (cf_config_get_blacklist_size(c)+1)* sizeof (char *));
	if (tmp) {
		cf_config_set_blacklist(c, tmp);
		cf_config_set_blacklist_size(c, cf_config_get_blacklist_size(c)+1);
	}

	entry = (char *) malloc(strlen(e)+1);
	strcpy(entry, e);

	tmp[cf_config_get_blacklist_size(c)-1] = entry;

	printf(" %s", entry);
	
}

void cf_config_init(Cf_config c) {
	/* Fill in the blacklist: file extensions for files not too compress */
	int i;
	char *list[22] = { 
		"jpg",
		"jpeg",
		"gif",
		"png",
		"mov",
		"mpg",
		"mpeg",
		"avi",
		"mp3",
		"mp2",
		"rm",
		"rmvb",
		"ra",
		"ram",
		"zip",
		"gz",
		"arj",
		"lha",
		"bz2",
		"tgz",
		"tbz2",
		NULL
	};

	puts("-----");
	puts("compFUSEd configuration defaults");
	puts("Files with the following extensions will never be compressed by compFUSEd:");

	cf_config_set_blacklist(c, NULL);
	cf_config_set_blacklist_size(c, 0);

	i = 0;
	while (list[i]) {
		cf_config_blacklist_add(c, list[i]);
		i++;
	}
	puts("");

	/* Max 128 buffers */
	cf_config_set_map_size(c, 128);
	/* Max 10MB of unused buffers (cache) */
	cf_config_set_cache_size(c, 10);
	/* Leave a unused buffer live for max 60 secs */
	cf_config_set_cache_ttl(c, 60);

	cf_log_open(NULL);
	puts("compFUSEd configuration defaults set");
}

FILE* cf_config_open(char *path) {
	FILE *ret;
	char home[128];

	printf("Using config file ");

	/* Start with current dir */
	ret = fopen(path, "r");
	if (ret) {
		puts(path);
		return ret;
	}

	/* Look in home dir */
	strcpy(home, getenv("PWD"));
	strcat(home, path);	
	ret = fopen(home, "r");
	if (ret) {
		puts(home);
		return ret;
	}

	puts(" NONE! Problem!");

	return NULL;
}

void lowerify(char *str) {
	int i = 0;

	while(str[i] != ' ') {
		str[i] = tolower(str[i]);
		i++;
	}
}

void cf_config_read(Cf_config c, char *path) {
	FILE *fp;
	char localpath[512];
	char fullpath[512];
	char line[512];
	char comp_lib[512];
	char log[512];
	char ext[5];
	int size;
	DIR *dp;

	fp = cf_config_open(path);
	if (!fp) {
		puts("Can not open config file!");
		puts(path);
		puts("Configarution file must have same names as executable binary followed by \".conf\" extension");
		puts("Things will go wrong from this point one...");

		exit(-1);
	}

	/* Set some defaults ¨*/
	cf_config_set_compression_id(c, CF_NONE);
	cf_config_set_buffersizer(c, none_buffersizer);
	cf_config_set_compressor(c, none_compressor);
	cf_config_set_decompressor(c, none_decompressor);
	cf_config_set_forced_scan(c, 0);

	puts("-----");
	puts("Reading compFUSEd configuration file");
	puts("Config file reading code is simple, good luck");

	while (!feof(fp)) {
		fscanf(fp,"%[^\n]\n", line);

		/* Catch comment */
		if (line[0] == '#')
			continue;

		/* Line start with blank? skip */
		if (line[0] == ' ')
			continue;


		/* So short? Must be crap, skip */
		if (strlen(line) < 4)
			continue;

		/* Convert line to lower case (for dummy users..) */
		lowerify(line);

		if (sscanf(line, "map_size %d", &size) == 1) {
			cf_config_set_map_size(c, size);
		}

		if (sscanf(line, "cache_size %d", &size) == 1) {
			cf_config_set_cache_size(c, size);
			if (cf_config_get_cache_size(c)== 0)
				puts("caching is disabled");
		}

		if (sscanf(line, "cache_ttl %d", &size) == 1) {
			cf_config_set_cache_ttl(c, size);
		}

		if (sscanf(line, "log %s", log) == 1) {
			cf_log_open(log);
		}

		if (sscanf(line, "exclude %s", ext) == 1) {
			printf("never compress: ");
			cf_config_blacklist_add(c, ext);
			puts("");
		}

		if (sscanf(line, "backend %s", localpath) == 1) {
			if (localpath[0] == '/') {
				/* Localpath is absolute */
				cf_config_set_backendpath(c, localpath);				
			} else {
				/* Make localpath absolute */
				strcpy(fullpath, getenv("PWD"));
				strcat(fullpath, "/");
				strcat(fullpath, localpath);
				cf_config_set_backendpath(c, fullpath);
			}
			printf("compressed files are stored in %s\n", cf_config_get_backendpath(c));
		}

		if (sscanf(line, "compression %s", comp_lib) == 1) {
			printf("compression using %s\n", comp_lib);
			if (!strcmp(comp_lib, "none")) {
				cf_config_set_error(c, none_error);
				cf_config_set_compression_id(c, CF_NONE);
				cf_config_set_buffersizer(c, none_buffersizer);
				cf_config_set_compressor(c, none_compressor);
				cf_config_set_decompressor(c, none_decompressor);
			}

			if (!strcmp(comp_lib, "zlib")) {
#ifdef CF_USE_ZLIB
				cf_config_set_error(c, zlib_error);
				cf_config_set_compression_id(c, CF_ZLIB);
				cf_config_set_buffersizer(c, (Cf_buffersizer_f) compressBound);
				cf_config_set_compressor(c, (Cf_compressor_f) compress);
				cf_config_set_decompressor(c, (Cf_decompressor_f) uncompress);
#else
				printf("Sorry %s is NOT compiled into this version\n", "zlib");
				exit(1);
#endif
			}

			if (!strcmp(comp_lib, "lzo")) {
#ifdef CF_USE_LZO
				cf_config_set_error(c, lzo_error);
				cf_config_set_compression_id(c, CF_LZO);
				cf_config_set_buffersizer(c, lzo_buffersizer);
				cf_config_set_compressor(c, lzo_compressor);
				cf_config_set_decompressor(c, lzo_decompressor);

			        lzo_init();
#else
				printf("Sorry %s is NOT compiled into this version\n", "lzo");
				exit(1);
#endif
			}

			if (!strcmp(comp_lib, "bzip2")) {
#ifdef CF_USE_BZIP2
				cf_config_set_error(c, bzip2_error);
				cf_config_set_compression_id(c, CF_BZIP2);
				cf_config_set_buffersizer(c, bzip2_buffersizer);
				cf_config_set_compressor(c, bzip2_compressor);
				cf_config_set_decompressor(c, bzip2_decompressor);
#else
				printf("Sorry %s is NOT compiled into this version\n", "bzip2");
				exit(1);
#endif
			}
		}

		/* Force a full filesystem scan upon mount */
		if (!strcmp(line, "forced_scan")) {
			cf_config_set_forced_scan(c, 1);
		}
	}

	/* Check whether backend directory does exist */
	dp = opendir(cf_config_get_backendpath(c));
	if (!dp) {
		puts("DAMN your backend directory does not exist :'( Create it first!");

		exit(-1);
	}
	closedir(dp);

	/* blahblah */
	puts("done reading configuration file");	
}
