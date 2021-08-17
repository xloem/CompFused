#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "cf_file.h"
#include "cf_map.h"

#define CF_DEBUG_NO

Cf_map cf_map_alloc() {
	Cf_map ret;

	ret = (Cf_map) malloc(sizeof(Cf_map_base));

	return ret;
}

void cf_map_dealloc(Cf_map m) {

	free(m->map);
	free(m);

}

void cf_map_init(Cf_map m, Cf_config c) {
	int i = 0;
	int stop;
	Cf_map_entry *e;

	stop = cf_config_get_map_size(c);

	e = (Cf_map_entry *) malloc(cf_config_get_map_size(c) * sizeof(Cf_map_entry));
	if (e)
		m->map = e;

	cf_map_set_map_config(m, c);
	cf_map_set_map_size(m, stop);
	cf_map_set_map_buffer(m, 0);
	cf_map_set_map_cache(m, 0);
	pthread_mutex_init(&cf_map_get_map_lock(m), NULL);

	for (i=0; i<stop; i++) {
		pthread_rwlock_init(&cf_map_get_lock(m, i), NULL);
		pthread_rwlock_init(&cf_map_get_openlock(m, i), NULL);
		cf_map_get_name(m,i)[0] = '\0';
		cf_map_set_fd(m, i, -1);
		cf_map_set_size(m,i, -1);
		cf_map_set_data(m, i, NULL);
		cf_map_set_dirty(m, i, 0);
		cf_map_set_opens(m, i, 0);
		cf_map_set_cached(m, i, 0);
	}

	printf("map: max %d buffers\n", cf_map_get_map_size(m));
	printf("map: max %dMB cache buffers (in total)\n", cf_config_get_cache_size(c));
	printf("map: max cache ttl %d\n", cf_config_get_cache_ttl(c));
}

void cf_map_print(Cf_map m) {
	printf("map_size %d\n", cf_map_get_map_size(m));
	printf("map_buffer %d\n", cf_map_get_map_buffer(m));
	printf("map_cache %d\n", cf_map_get_map_cache(m));
}

int cf_map_findname(Cf_map m, const char *name) {
	int i = 0;
	int stop = cf_map_get_map_size(m);

	for(i=0; i<stop; i++) {
		if (!strcmp(cf_map_get_name(m,i), name))
			return i;
	}

	return -1;
}

int cf_map_find(Cf_map m, int fd) {
	int i = 0;
	int stop = cf_map_get_map_size(m);

	for(i=0; i<stop; i++) {
		if (cf_map_get_fd(m,i) == fd)
			return i;
	}

	return -1;
}

int cf_map_get_free(Cf_map m) {
	/* Find a closed entry without data */
	int i = 0;
	int stop = cf_map_get_map_size(m);

	#ifdef CF_DEBUG
	puts("get_free");
	#endif

	for (i=0; i<stop; i++) {
		if ((cf_map_get_fd(m,i) == -1) && (cf_map_get_cached(m,i)==0)) {
			return i;
		}
	}

	return -1;
}

void cf_map_clean(Cf_map m, int pos) {
	cf_map_set_map_cache(m, cf_map_get_map_cache(m)-1);

	/* Free resources */
	if (cf_map_get_data(m, pos))
		free(cf_map_get_data(m, pos));

	/* Reset */
	cf_map_get_name(m, pos)[0] = '\0';
	cf_map_set_data(m, pos, NULL);
	cf_map_set_size(m, pos, 0);
	cf_map_set_cached(m, pos, 0);
}

int cf_map_cleanup(Cf_map m ) {
	/* Look for FIRST not open entry and delete the data */
	int i = 0;
	int stop = cf_map_get_map_size(m);

	for (i=0; i<stop; i++) {
		if (cf_map_get_fd(m,i) == -1) {
			cf_map_clean(m, i);
			return i;
		}
	}

	return -1;
}

int cf_map_addname(Cf_map m, const char *name, int fd) {
	int pos;
	int stop = cf_map_get_map_size(m);
	Cf_map_entry *e;


	/* Get a free entry */
	pos = cf_map_get_free(m);

	if (pos == -1) {
		/* Delete an entry and return id */
		pos = cf_map_cleanup(m);
	}

	if (pos != -1) {
		cf_map_set_fd(m, pos, fd);
		cf_map_set_dirty(m, pos, 0);
		cf_map_set_cached(m, pos, 0);
		strcpy(cf_map_get_name(m, pos), name);
	} else {

		printf("ERROR: no space left in cf_mapping for fd %d\n", fd);
		printf("Use more buffers! Resizing to twice as much for you....\n");
		e = (Cf_map_entry *) realloc(m->map, stop * 2 * sizeof(Cf_map_entry));
		if (e) {
			m->map = e;
			cf_map_set_map_size(m, stop*2);

			/* And try again */
			return cf_map_addname(m, name, fd);
		} else {
			puts("Resizing failed, giving up");
			exit(-2);
		}
	}		

	return pos;
}

int cf_map_del(Cf_map m, int pos) {
	struct timeval t;

	if (gettimeofday(&t, NULL) == -1)
		perror("problem getting time in map_del()");

	if (pos != -1) {
		cf_map_set_fd(m, pos, -1);
		cf_map_set_dirty(m, pos, 0);
		cf_map_set_dead(m, pos, t.tv_sec);

		cf_map_set_map_buffer(m, cf_map_get_map_buffer(m)-1);
		cf_map_set_map_cache(m, cf_map_get_map_cache(m)+1);

		/* only cache entries compressed by us */
		if (!cf_map_get_no_compress(m, pos)) {
			cf_map_set_cached(m, pos, 1);
		}

		return 0;
	} else {
		printf("ERROR: pos %d not found in cf_mapping\n", pos);
		return -1;
	}
}


void cf_map_scrub(Cf_map m) {
	Cf_config c;
	int i;
	int size;
	int stop;
	int res;
	int count = 0;
	struct timeval t;

	gettimeofday(&t, NULL);

	stop = cf_map_get_map_size(m);
	c = cf_map_get_map_config(m);

	/* Look for entries that are NOT used (fd == -1) && (data) && too old */
	size = 0;
	for (i=0; i<stop; i++) {
		res = pthread_rwlock_trywrlock(&cf_map_get_openlock(m, i));
		if (res == EBUSY)
			printf("locked (%d) %s\n", i, cf_map_get_name(m,i));

		if (!res) {
			if (cf_map_get_cached(m,i)) {
				if ((t.tv_sec - cf_map_get_dead(m,i)) > cf_config_get_cache_ttl(c)) {
					cf_map_clean(m, i);
					count++;
				} else {
					size += cf_map_get_size(m, i);
				}
			}
	
			pthread_rwlock_unlock(&cf_map_get_openlock(m,i));
		} 
	}

	/* Exit if not too big */
	if (size < cf_config_get_cache_size(c) * 1024 *1024) {
		printf("released %d buffers\n", count);
		return;
	}

	/* Compute size of unlocked entry that are NOT used (fd == -1) && (data) */
	for (i=0; i<stop; i++) {
		res = pthread_rwlock_trywrlock(&cf_map_get_openlock(m, i));

		#ifdef CF_DEBUG
		if (cf_map_get_cached(m,i))
			printf("(%d) %s\n", i, cf_map_get_name(m,i));
		if (res == EBUSY)
			printf("locked (%d) %s\n", i, cf_map_get_name(m,i));
		#endif

		if (!res) {
			if ((cf_map_get_fd(m,i) == -1) && cf_map_get_data(m,i)) {
					size -= cf_map_get_size(m, i);
					cf_map_clean(m, i);
					count++;
			}
	
			pthread_rwlock_unlock(&cf_map_get_openlock(m,i));
		} 

		/* Exit if small enough */
		if (size < cf_config_get_cache_size(c) * 1024 *1024)
			return;
	}

	printf("released %d buffers\n", count);

}

int cf_map_read(Cf_map m, int pos) {
    Cf_config config;
    Cf_header header;
    int i_size;
    int fd;
    char *i_buffer;

    config = cf_map_get_map_config(m);

	fd = cf_map_get_fd(m,pos);

	if (cf_map_get_data(m, pos))
		puts("data not empty!");

	header = cf_header_read(config, fd);
	cf_map_set_size(m, pos, cf_header_size(header));

	/* Some files contain compressed data (ad hoc compress) if so not compression : mp3, jpg, ... */
	cf_map_set_no_compress(m, pos, cf_file_no_compress(config, cf_map_get_name(m,pos)));

    	i_buffer = cf_map_get_data(m, pos);
    	i_size = cf_map_get_size(m, pos);

    	/* Data already in mapping? */
    	if ((!i_buffer) && i_size && !cf_map_get_no_compress(m, pos)) {
		#ifdef CF_DEBUG
		printf("READ FROM DISK ");
		#endif
		/* No not present: read from file */
		i_buffer = (char *) malloc(i_size);
		cf_map_set_data(m, pos, i_buffer);

		if (cf_data_read(config, fd, i_buffer, i_size))
			puts("ERROR: cf_data_read problem");

    	}


	return 0;
}

int cf_map_write(Cf_map m, int pos) {
    int res;
    int i_size;
    int fd;
    int nocompress;
    int dirty;
    char *i_buffer;
    Cf_config config;

	config = cf_map_get_map_config(m);

	fd = cf_map_get_fd(m, pos);
	nocompress = cf_map_get_no_compress(m, pos);
	i_buffer = cf_map_get_data(m, pos);
	i_size = cf_map_get_size(m, pos);
	dirty = cf_map_get_dirty(m, pos);


	cf_map_del(m, pos);


	if (nocompress) {
		cf_header_write(config, fd, file_size(fd)-sizeof(Cf_header));
	} else {
		/* Compress ONLY if data changed */
		if ( dirty )  {
			cf_header_write(config, fd, i_size);
			res = cf_data_write(config, fd, i_buffer, i_size, nocompress);

			#ifdef CF_DEBUG
			printf("Release: %d to %d (handle = %d)\n", i_size, res, pos);
			#endif
		}
	}

	close(fd);

	return res;
}



int cf_map_open(Cf_map m, const char *path, unsigned int flags) {
    int res;
    int pos;
    unsigned int flags_old;

    /* Lock entire mapping */
    pthread_mutex_lock(&cf_map_get_map_lock(m));


    #ifdef CF_DEBUG
    printf("%s ", path);
    #endif

    flags_old = flags;
    flags = O_RDWR;

    pos = cf_map_findname(m, path);
    if (pos == -1) {

	res = open(path, flags); /*  */
    	if(res == -1) {
		/* Unlock mapping */
		pthread_mutex_unlock(&cf_map_get_map_lock(m));

        	return -errno;
	}
	pos = cf_map_addname(m, path, res);
	cf_map_set_flags(m, pos, flags_old);
	cf_map_set_map_buffer(m, cf_map_get_map_buffer(m)+1);

	/* Lock for read to track number of open() calls and avoid close() */
	res = pthread_rwlock_rdlock(&cf_map_get_openlock(m,pos));

	/* Decompression happens here */
	cf_map_read(m, pos);

	#ifdef CF_DEBUG
	puts("new map entry");
	#endif
    } else {
	/* Lock for read to track number of open() calls and avoid close() */
	res = pthread_rwlock_rdlock(&cf_map_get_openlock(m,pos));

	/* Could be cached: file already closed -> reopen and install fd */
	if (cf_map_get_cached(m, pos)) {
		res = open(path, flags); /*  */
	    	if(res == -1) {
			/* Unlock */
			pthread_mutex_unlock(&cf_map_get_map_lock(m));

	        	return -errno;
		}

		cf_map_set_map_buffer(m, cf_map_get_map_buffer(m)+1);
		cf_map_set_map_cache(m, cf_map_get_map_cache(m)-1);

		#ifdef CF_DEBUG
		puts("cached");
		#endif

		cf_map_set_fd(m, pos, res);
		cf_map_set_cached(m, pos, 0);
	} else {
		#ifdef CF_DEBUG
		puts("buffered");
		#endif
	}
    }


	/* Unlock mapping asap (do decompress after this line) */	
	pthread_mutex_unlock(&cf_map_get_map_lock(m));

    return pos;
}


int cf_map_close(Cf_map m, int pos)
{
    int res;

    pthread_mutex_lock(&cf_map_get_map_lock(m));

    /* Unlock: decrease number of readlocks = nr of open() calls */	
    res = pthread_rwlock_unlock(&cf_map_get_openlock(m,pos));


    /* If no more thread with rdlock the wrlock means we can close */
    if (pthread_rwlock_trywrlock(&cf_map_get_openlock(m, pos))) {
	#ifdef CF_DEBUG
	printf("not releasing %s, still got threads\n", cf_map_get_name(m,pos));
	#endif

	pthread_mutex_unlock(&cf_map_get_map_lock(m));

	return 0;
    }


	/* Compression happens here */
	res = cf_map_write(m, pos);

	#ifdef CF_DEBUG
	printf("closing %d %s\n", pos, cf_map_get_name(m, pos));
	#endif

	/* No caching: free immediately */
	if (cf_config_get_cache_size(cf_map_get_map_config(m)) == 0)
			cf_map_clean(m, pos);

	/* unlock this entry, make new rwlocks possible */
	pthread_rwlock_unlock(&cf_map_get_openlock(m,pos));

	/* Unlock mapping */
	pthread_mutex_unlock(&cf_map_get_map_lock(m));



	if(res == -1)
		return -errno;

    return 0;
}

