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
#include "cf_log.h"

#define CF_DEBUG_OFF

#define REVERSE_NAME_HACK

void cf_map_rev(const char *src, char *dst) {

#ifdef REVERSE_NAME_HACK
	const int stop = strlen(src);
	unsigned char *x = (unsigned char *) src;
	unsigned char *y = (unsigned char *) dst + stop - 1;

	/* Install '\0' */
	dst[stop] = '\0';

	while (*x) {
		*y = *x;

		/* Move along */
		x++;
		y--;
	}
#else
	strcpy(dst, src);
#endif
}

Cf_map cf_map_alloc() {
	Cf_map ret;

	ret = (Cf_map) malloc(sizeof(Cf_map_base));

	if (ret) {
		memset(ret, 0, sizeof(Cf_map_base));
		ret->map = NULL;
		cf_map_set_map_size(ret, 0);
		cf_map_set_map_config(ret, NULL);
		cf_map_set_map_buffer(ret, 0);
		cf_map_set_map_cache(ret, 0);
		cf_map_set_map_data_blocks(ret, 0);
	}

	return ret;
}

void cf_map_dealloc(Cf_map m) {

	if (m->map)
		free(m->map);

	free(m);
}

void cf_map_cached_add(Cf_map m, int pos) {
	if (cf_map_get_map_tail_cached(m) == -1)
		cf_map_set_map_tail_cached(m, pos);

	cf_map_set_next(m, pos, cf_map_get_map_head_cached(m));
	cf_map_set_map_head_cached(m, pos);
}

int cf_map_cached_lru(Cf_map m) {
	int ret = cf_map_get_map_tail_cached(m);

	if (cf_map_get_map_tail_cached(m) == cf_map_get_map_head_cached(m)) {
		cf_map_set_map_head_cached(m, -1);
		cf_map_set_map_tail_cached(m, -1);
	} else {
		cf_map_set_map_tail_cached(m, cf_map_get_prev(m,ret));
	}

	return ret;
}

void cf_map_free_add(Cf_map m, int pos) {
	if (cf_map_get_map_tail_free(m) == -1)
		cf_map_set_map_tail_free(m, pos);

	cf_map_set_next(m, pos, cf_map_get_map_head_free(m));
	cf_map_set_map_head_free(m, pos);
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

	/* These are just counters */
	cf_map_set_map_buffer(m, 0);
	cf_map_set_map_cache(m, 0);

	/* Set list indexes */

	/* Nothing cached */
	cf_map_set_map_head_cached(m, -1);
	cf_map_set_map_tail_cached(m, -1);

	/* All the map entries are free */
	cf_map_set_map_head_free(m, 0);
	cf_map_set_map_tail_free(m,stop -1);


	pthread_mutex_init(&cf_map_get_map_lock(m), NULL);

	for (i=0; i<stop; i++) {
		pthread_rwlock_init(&cf_map_get_lock(m, i), NULL);
		pthread_rwlock_init(&cf_map_get_openlock(m, i), NULL);
		pthread_mutex_init(&cf_map_get_datalock(m, i), NULL);
		cf_map_get_name(m,i)[0] = '\0';
		cf_map_set_fd(m, i, -1);
		cf_map_set_size(m,i, -1);
		cf_map_set_old_size(m,i, -1);
		cf_map_set_buffer_size(m,i, -1);
		cf_map_set_data(m, i, NULL);
		cf_map_set_dirty(m, i, 0);
		cf_map_set_opens(m, i, 0);
		cf_map_set_cached(m, i, 0);

		cf_map_set_prev(m,i, i-1);

		if (i < (stop-1))
			cf_map_set_next(m, i, i+1);
		else
			cf_map_set_next(m, i, -1);

	}

	cf_printf("map: max %d buffers\n", cf_map_get_map_size(m));
	cf_printf("map: max %dMB cache buffers (in total)\n", cf_config_get_cache_size(c));
	cf_printf("map: max cache ttl %d\n", cf_config_get_cache_ttl(c));
}

void cf_map_print(Cf_map m) {
	cf_printf("map_size %d\n", cf_map_get_map_size(m));
	cf_printf("map_buffer %d\n", cf_map_get_map_buffer(m));
	cf_printf("map_cache %d\n", cf_map_get_map_cache(m));
}

int cf_map_findname(Cf_map m, const char *name) {
	int i = 0;
	int stop = cf_map_get_map_size(m);
	char rname[4096];

	cf_map_rev(name, rname);
	for(i=0; i<stop; i++) {
		if (!strcmp(cf_map_get_name(m,i), rname))
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
	cf_puts("get_free");
	#endif

#if 0
	i=cf_map_get_map_head_free(m); 
	while (i<cf_map_get_map_tail_free(m)) {
		cf_map_set_map_head_free(m, cf_map_get_next(m, i));

		return i;
	}
#endif

	for (i=0; i<stop; i++) {
		/* Skip if buffer is locked */
		if (pthread_mutex_trylock(&cf_map_get_datalock(m, i))) {
			continue;
		} else {
			pthread_mutex_unlock(&cf_map_get_datalock(m, i));
			if ((cf_map_get_fd(m,i) == -1) && (cf_map_get_cached(m,i)==0)) {
				return i;
			}
		}
	}

	return -1;
}

/* Really free the buffer memory, delete the name, ... */
void cf_map_clean_low(Cf_map m, int pos) {

	cf_map_set_map_cache(m, cf_map_get_map_cache(m)-1);

	/* Free resources */
	if (cf_map_get_data(m, pos))
		free(cf_map_get_data(m, pos));

	/* Reset */
	cf_map_get_name(m, pos)[0] = '\0';
	cf_map_set_data(m, pos, NULL);
	cf_map_set_size(m, pos, 0);
	cf_map_set_buffer_size(m, pos, 0);
	cf_map_set_cached(m, pos, 0);

}

/* MT aware version of clean_low */
void cf_map_clean(Cf_map m, int pos) {
	/* Skip if buffer is locked */
	if (pthread_mutex_trylock(&cf_map_get_datalock(m, pos)))
		return;
	else
		pthread_mutex_unlock(&cf_map_get_datalock(m, pos));

	cf_map_clean_low(m, pos);
}

int cf_map_cleanup(Cf_map m ) {
	/* Look for FIRST not open entry and delete the data */
	int i = 0;

	int stop = cf_map_get_map_size(m);

	for (i=0; i<stop; i++) {
		/* Must have no fd */
		if (cf_map_get_fd(m,i) == -1) {
			/* Skip if buffer is locked */
			if (pthread_mutex_trylock(&cf_map_get_datalock(m, i))) {
				continue;
			} else {
				pthread_mutex_unlock(&cf_map_get_datalock(m, i));
				cf_map_clean(m, i);
				return i;
			}
		}
	}

	return -1;
}

int cf_map_addname(Cf_map m, const char *name, int fd) {
	int pos;
	int i;
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

		cf_map_rev(name, cf_map_get_name(m, pos));
/*
		strcpy(cf_map_get_name(m, pos), name);
*/
	} else {

		cf_printf("\t ERROR: no space left in cf_mapping for fd %d\n", fd);
		cf_printf("\t Use more buffers! Resizing to twice as much for you....\n");

sleep(5);
		e = (Cf_map_entry *) realloc(m->map, stop * 2 * sizeof(Cf_map_entry));
		if (e) {
			m->map = e;
			cf_map_set_map_size(m, stop*2);

			for (i=stop; i<stop*2; i++) {
				pthread_rwlock_init(&cf_map_get_lock(m, i), NULL);
				pthread_rwlock_init(&cf_map_get_openlock(m, i), NULL);
				pthread_mutex_init(&cf_map_get_datalock(m, i), NULL);
				cf_map_get_name(m,i)[0] = '\0';
				cf_map_set_fd(m, i, -1);
				cf_map_set_old_size(m,i, -1);
				cf_map_set_size(m,i, -1);
				cf_map_set_buffer_size(m,i, -1);
				cf_map_set_data(m, i, NULL);
				cf_map_set_dirty(m, i, 0);
				cf_map_set_opens(m, i, 0);
				cf_map_set_cached(m, i, 0);
			}


			/* And try again */
			return cf_map_addname(m, name, fd);
		} else {
			cf_puts("Resizing failed, giving up");
			exit(-2);
		}
	}		

	return pos;
}

/* Delete an entry from the mapping: invalidate fd, mark as cached */
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
		cf_map_set_cached(m, pos, 1);

		/* Add to cached list */
		cf_map_cached_add(m, pos);

		return 0;
	} else {
		cf_printf("ERROR: pos %d not found in cf_mapping\n", pos);
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

		#ifdef CF_DEBUG
		if (res == EBUSY)
			cf_printf("locked (%d) %s\n", i, cf_map_get_name(m,i));
		#endif

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
		#ifdef CF_DEBUG
		cf_printf("released %d buffers\n", count);
		#endif

		return;
	}

	/* Compute size of unlocked entry that are NOT used (fd == -1) && (data) */
	for (i=0; i<stop; i++) {
		res = pthread_rwlock_trywrlock(&cf_map_get_openlock(m, i));

		#ifdef CF_DEBUG
		if (cf_map_get_cached(m,i))
			cf_printf("(%d) %s\n", i, cf_map_get_name(m,i));
		if (res == EBUSY)
			cf_printf("locked (%d) %s\n", i, cf_map_get_name(m,i));
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

	cf_printf("released %d buffers\n", count);

}

int cf_map_read(Cf_map m, int pos) {
    Cf_config config;
    int i_size;
    int fd;
    char *i_buffer;

    config = cf_map_get_map_config(m);

	fd = cf_map_get_fd(m,pos);

	if (cf_map_get_data(m, pos))
		cf_puts("data not empty!");

    	i_buffer = cf_map_get_data(m, pos);
    	i_size = cf_map_get_size(m, pos);

    	/* Data already in mapping? */
    	if ((!i_buffer) && i_size && !cf_map_get_no_compress(m, pos)) {
		#ifdef CF_DEBUG
		cf_printf("READ FROM DISK (handle %d)", fd);
		#endif
		/* No not present: read from file */
		i_buffer = (char *) malloc(i_size);
		cf_map_set_data(m, pos, i_buffer);

		if (cf_data_read(config, fd, i_buffer, i_size))
			cf_printf("ERROR: cf_data_read problem for %s\n", cf_map_get_name(m, pos));

    	}


	return 0;
}

int cf_map_write(Cf_map m, int pos) {
    int res= 0;
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

/*
	if (nocompress)
		i_size = file_size(fd)-sizeof(Cf_header);
*/

	/* Update total fs block size */
	if (cf_map_get_old_size(m, pos) != i_size) {
		cf_map_set_map_data_blocks(m, cf_map_get_map_data_blocks(m) + i_size - cf_map_get_old_size(m, pos));
	}

	/* Indicate this buffer is free BUT releasing thr datalock will really make it free! */
	cf_map_del(m, pos);


	/* No caching: remove the name */
	if (cf_config_get_cache_size(cf_map_get_map_config(m)) == 0)
			cf_map_get_name(m, pos)[0] = '\0';

	/* Unlock mapping */
	pthread_mutex_unlock(&cf_map_get_map_lock(m));


	if (nocompress) {
		#ifdef CF_DEBUG
		cf_printf("Write header for %d (not compressed)\n", fd);
		#endif
		cf_header_write(config, fd, i_size);

		res = 1;
	} else {
		/* Compress ONLY if data changed */
		if ( dirty )  {
			#ifdef CF_DEBUG
			cf_printf("Write header for %d\n", fd);
			#endif
			cf_header_write(config, fd, i_size);
			res = cf_data_write(config, fd, i_buffer, i_size, nocompress);

			#ifdef CF_DEBUG
			cf_printf("Release: %d to %d (handle = %d)\n", i_size, res, fd);
			#endif
		}
	}

	if (fd < 0) {
		cf_printf("ERROR: fd is %d for %s\n", fd, cf_map_get_name(m, pos));
	}

	close(fd);

	return res;
}



int cf_map_open(Cf_map m, const char *path, unsigned int flags) {
    int res = 0;
    int pos;
    unsigned int flags_old;
    struct stat st_buf;
    Cf_header header;

    /* Lock entire mapping */
    pthread_mutex_lock(&cf_map_get_map_lock(m));


    #ifdef CF_DEBUG
    cf_printf("%s ", path);

    if ((flags & O_ACCMODE) ==  O_RDWR)
	cf_puts("RDWR ");

    if ((flags & O_ACCMODE) == O_RDONLY)
	cf_puts("RDONLY ");

    if ((flags & O_ACCMODE) == O_WRONLY)
	cf_puts("WRONLY ");

    if (flags & O_APPEND)
	cf_puts("APPEND ");

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

	header = cf_header_read(cf_map_get_map_config(m), res);
	cf_map_set_size(m, pos, cf_header_size(header));
	cf_map_set_buffer_size(m, pos, cf_header_size(header));
	cf_map_set_old_size(m, pos, cf_map_get_size(m, pos));

	fstat(res, &st_buf);
	cf_map_set_atime(m, pos, st_buf.st_atime);
	cf_map_set_mtime(m, pos, st_buf.st_mtime);
	cf_map_set_ctime(m, pos, st_buf.st_ctime);

	pthread_mutex_lock(&cf_map_get_datalock(m, pos));

	/* Unlock mapping asap (do decompress after this line) */	
	pthread_mutex_unlock(&cf_map_get_map_lock(m));

	#ifdef CF_DEBUG
	cf_printf("new map entry (handle %d)\n", res);
	#endif

	/* Some files contain compressed data (ad hoc compress) if so not compression : mp3, jpg, ... */
	cf_map_set_no_compress(m, pos, cf_file_no_compress(cf_map_get_map_config(m), path));
	cf_map_set_flags(m, pos, flags_old);
	cf_map_set_map_buffer(m, cf_map_get_map_buffer(m)+1);

	/* Lock for read to track number of open() calls and avoid close() */
	res = pthread_rwlock_rdlock(&cf_map_get_openlock(m,pos));

	cf_map_read(m, pos);

    } else {
	pthread_mutex_lock(&cf_map_get_datalock(m, pos));

	/* Lock for read to track number of open() calls and avoid close() */
	res = pthread_rwlock_rdlock(&cf_map_get_openlock(m,pos));

	/* Could be cached: file already closed -> reopen and install fd */
	if (cf_map_get_cached(m, pos)) {
		res = open(path, flags); /*  */
	    	if(res == -1) {
			/* Unlock */
			pthread_mutex_unlock(&cf_map_get_map_lock(m));

			pthread_mutex_unlock(&cf_map_get_datalock(m, pos));

	        	return -errno;
		}

		cf_map_set_map_buffer(m, cf_map_get_map_buffer(m)+1);
		cf_map_set_map_cache(m, cf_map_get_map_cache(m)-1);

		#ifdef CF_DEBUG
		cf_printf("cached (handle %d)\n", res);
		#endif

		cf_map_set_fd(m, pos, res);
		cf_map_set_cached(m, pos, 0);
		cf_map_set_flags(m, pos, flags_old);
		cf_map_set_old_size(m, pos, cf_map_get_size(m, pos));
	} else {
		#ifdef CF_DEBUG
		cf_puts("buffered");
		#endif
	}


	/* Unlock mapping asap (do decompress after this line) */	
	pthread_mutex_unlock(&cf_map_get_map_lock(m));
    }


	pthread_mutex_unlock(&cf_map_get_datalock(m, pos));

    return pos;
}


int cf_map_close(Cf_map m, int pos)
{
    int res = 0;


    pthread_mutex_lock(&cf_map_get_map_lock(m));
    pthread_mutex_lock(&cf_map_get_datalock(m, pos));

    /* Unlock: decrease number of readlocks = nr of open() calls */	
    res = pthread_rwlock_unlock(&cf_map_get_openlock(m,pos));


    /* If no more thread with rdlock the wrlock means we can close */
    if (pthread_rwlock_trywrlock(&cf_map_get_openlock(m, pos))) {
	#ifdef CF_DEBUG
	cf_printf("not releasing %s, still got threads\n", cf_map_get_name(m,pos));
	#endif
	pthread_mutex_unlock(&cf_map_get_map_lock(m));
	pthread_mutex_unlock(&cf_map_get_datalock(m, pos));

	return 0;
    }


	#ifdef CF_DEBUG
	cf_printf("closing %d %s\n", pos, cf_map_get_name(m, pos));
	#endif

	/* Compression happens here */
	res = cf_map_write(m, pos);

	/* No caching: free immediately */
	if (cf_config_get_cache_size(cf_map_get_map_config(m)) == 0)
			cf_map_clean(m, pos);


	/* unlock this entry, make new rwlocks possible */
	pthread_rwlock_unlock(&cf_map_get_openlock(m,pos));

	pthread_mutex_unlock(&cf_map_get_datalock(m, pos));



	if(res == -1)
		return -errno;

    return 0;
}

