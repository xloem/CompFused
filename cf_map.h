#ifndef __CF_MAP__
#define __CF_MAP__

#define _GNU_SOURCE


#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <utime.h>
#include <pthread.h>

#include "cf_config.h"

#define MAP_NAMESIZE	4096


typedef	struct {
		/* Control read() and write */
		pthread_rwlock_t lock;
		/* Lock data */
		pthread_mutex_t datalock;
		/* Control open() and release() */
		pthread_rwlock_t openlock;

		char name[MAP_NAMESIZE];

	        int fd;		
	        unsigned int flags;

		/* Time info */
		time_t atime;
		time_t mtime;
		time_t ctime;

		int cached;
		int no_compress;
		int opens;
	        int dirty;
		long dead;

		/* Buffer + size */
	        char *data; 
		int buffer_size;

	        int size;
	        int old_size;

		/* Prev&next index inlist */
		int prev;
		int next; } Cf_map_entry;

typedef struct {
	pthread_mutex_t lock;
	int map_size;
	int map_buffer;
	int map_cache;

	long data_blocks;	/* in bytes */

	/* Indexed free list */
	int head_free;
	int tail_free;

	/* Indexed cached list */
	int head_cached;
	int tail_cached;


	Cf_config config;

	Cf_map_entry *map;
} Cf_map_base;

typedef Cf_map_base *Cf_map;

#define cf_map_get_map(m,pos)			m->map[pos]

#define cf_map_get_map_lock(m)			m->lock
#define cf_map_get_map_size(m)			m->map_size
#define cf_map_get_map_config(m)		m->config
#define cf_map_get_map_buffer(m)		m->map_buffer
#define cf_map_get_map_cache(m)			m->map_cache
#define cf_map_get_map_data_blocks(m)		m->data_blocks
#define cf_map_get_map_head_free(m)		m->head_free
#define cf_map_get_map_tail_free(m)		m->tail_free
#define cf_map_get_map_head_cached(m)		m->head_cached
#define cf_map_get_map_tail_cached(m)		m->tail_cached

#define cf_map_get_name(m, pos)          	cf_map_get_map(m,pos).name
#define cf_map_get_lock(m, pos)          	cf_map_get_map(m,pos).lock
#define cf_map_get_openlock(m, pos)          	cf_map_get_map(m,pos).openlock
#define cf_map_get_datalock(m, pos)          	cf_map_get_map(m,pos).datalock
#define cf_map_get_fd(m, pos)          		cf_map_get_map(m,pos).fd
#define cf_map_get_flags(m, pos)       		cf_map_get_map(m,pos).flags
#define cf_map_get_data(m, pos)        		cf_map_get_map(m,pos).data
#define cf_map_get_cached(m, pos)        	cf_map_get_map(m,pos).cached
#define cf_map_get_size(m, pos)        		cf_map_get_map(m,pos).size
#define cf_map_get_old_size(m, pos)        	cf_map_get_map(m,pos).old_size
#define cf_map_get_buffer_size(m, pos)        	cf_map_get_map(m,pos).buffer_size
#define cf_map_get_opens(m, pos)       		cf_map_get_map(m,pos).opens
#define cf_map_get_dirty(m, pos)       		cf_map_get_map(m,pos).dirty
#define cf_map_get_dead(m, pos)       		cf_map_get_map(m,pos).dead
#define cf_map_get_no_compress(m, pos)       	cf_map_get_map(m,pos).no_compress
#define cf_map_get_atime(m, pos)       		cf_map_get_map(m,pos).atime
#define cf_map_get_mtime(m, pos)       		cf_map_get_map(m,pos).mtime
#define cf_map_get_ctime(m, pos)       		cf_map_get_map(m,pos).ctime

#define cf_map_get_prev(m, pos)			cf_map_get_map(m,pos).prev
#define cf_map_get_next(m, pos)			cf_map_get_map(m,pos).next

#define cf_map_set_map_config(m, s)		cf_map_get_map_config(m) = s
#define cf_map_set_map_size(m, s)		cf_map_get_map_size(m) = s
#define cf_map_set_map_buffer(m, s)		cf_map_get_map_buffer(m) = s
#define cf_map_set_map_cache(m, s)		cf_map_get_map_cache(m) = s
#define cf_map_set_map_data_blocks(m, s)	cf_map_get_map_data_blocks(m) = s
#define cf_map_set_map_head_free(m,s)		cf_map_get_map_head_free(m) = s
#define cf_map_set_map_tail_free(m,s)		cf_map_get_map_tail_free(m) = s
#define cf_map_set_map_head_cached(m,s)		cf_map_get_map_head_cached(m) = s
#define cf_map_set_map_tail_cached(m,s)		cf_map_get_map_tail_cached(m) = s

#define cf_map_set_fd(m, pos, f)          	cf_map_get_fd(m,pos) = (f)
#define cf_map_set_flags(m, pos, fl)      	cf_map_get_flags(m, pos) = (fl)
#define cf_map_set_data(m, pos, d)        	cf_map_get_data(m, pos) = (d)
#define cf_map_set_cached(m, pos, s)        	cf_map_get_cached(m, pos) = (s)
#define cf_map_set_size(m, pos, s)        	cf_map_get_size(m, pos) = (s)
#define cf_map_set_old_size(m, pos, s)        	cf_map_get_old_size(m, pos) = (s)
#define cf_map_set_buffer_size(m, pos, s)       cf_map_get_buffer_size(m, pos) = (s)
#define cf_map_set_opens(m, pos, d)       	cf_map_get_opens(m, pos) = (d)
#define cf_map_set_dirty(m, pos, d)       	cf_map_get_dirty(m, pos) = (d)
#define cf_map_set_dead(m, pos, d)       	cf_map_get_dead(m, pos) = (d)
#define cf_map_set_no_compress(m, pos,c)    	cf_map_get_no_compress(m, pos) = (c)
#define cf_map_set_atime(m, pos,t)    		cf_map_get_atime(m, pos) = (t)
#define cf_map_set_mtime(m, pos,t)    		cf_map_get_mtime(m, pos) = (t)
#define cf_map_set_ctime(m, pos,t)    		cf_map_get_ctime(m, pos) = (t)

#define cf_map_set_prev(m, pos, x)		cf_map_get_prev(m,pos) = x
#define cf_map_set_next(m, pos, x)		cf_map_get_next(m,pos) = x


Cf_map cf_map_alloc();
void cf_map_dealloc();

void cf_map_print(Cf_map );
void cf_map_init(Cf_map m, Cf_config c);
int cf_map_open(Cf_map m, const char *name, unsigned int flags);
int cf_map_close(Cf_map m, int fd);

int cf_map_findname(Cf_map m, const char *name);
int cf_map_find(Cf_map m, int fd);
int cf_map_add(Cf_map m, int fd);
int cf_map_addname(Cf_map m, const char *name, int fd);

void cf_map_clean(Cf_map m, int pos);
int cf_map_del(Cf_map m, int fd);
void cf_map_scrub(Cf_map m);

#endif
