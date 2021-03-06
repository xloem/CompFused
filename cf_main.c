/*
#include <config.h>
*/
#ifdef linux
/* For pread()/pwrite() */
#define _XOPEN_SOURCE 500
#endif

#define FUSE_USE_VERSION 25

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/statfs.h>
#include <string.h>
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

#include "cf_log.h"
#include "cf_file.h"
#include "cf_scan.h"
#include "cf_map.h"
#include "cf_compression.h"
#include "cf_config.h"

#define CF_PATH_MAX	4096

#define CF_DEBUG_OFF
#define CF_DEBUG_L2_OFF


/* GLOBALS */
Cf_config config;
Cf_map mapping;
pthread_t scrubber;

/* New function needed for FUSE 2.5.x */

static inline DIR *get_dirp(struct fuse_file_info *fi)
{
    return (DIR *) (uintptr_t) fi->fh;
}


/* ------------------------------------------------- */

static int cf_getattr(const char *path, struct stat *stbuf)
{
    int res;
    int fd;
    char xpath[CF_PATH_MAX];
    Cf_header header;

    cf_path(config, path, xpath);

#ifdef CF_DEBUG
    cf_printf("%s %s\n", __FUNCTION__, xpath);
#endif 

    res = lstat(xpath, stbuf);
    if(res == -1)
        return -errno;

    if (S_ISDIR(stbuf->st_mode))
	return 0;

#ifdef CF_DEBUG
    cf_printf("looking in mapping for attribs\n");
#endif 
    res = cf_map_findname(mapping, xpath);
    if (res != -1) {
#ifdef CF_DEBUG
    cf_printf("found in mapping\n");
#endif 
	if (!cf_map_get_no_compress(mapping,res)) {
		stbuf->st_size = cf_map_get_size(mapping, res);
		stbuf->st_atime = cf_map_get_atime(mapping, res);
		stbuf->st_mtime = cf_map_get_ctime(mapping, res);
			
		return 0;
	}
    }

    if (S_ISREG(stbuf->st_mode) && (stbuf->st_size > 0)) {

#ifdef CF_DEBUG
    cf_printf("Not found in mapping read from header\n");
#endif 

        fd = open (xpath, O_RDONLY);
	if (fd != -1) {
            header = cf_header_read(config, fd);
    	    stbuf->st_size = cf_header_size(header);
	    close(fd);
	} else {
	    cf_printf("File %s not found... in %s\n", xpath, __FUNCTION__);
	}
    }

    return 0;
}


static int cf_readlink(const char *path, char *buf, size_t size)
{
    int res;
    char xpath[CF_PATH_MAX];

    cf_path(config, path, xpath);

#ifdef CF_DEBUG
    cf_printf("%s %s\n", __FUNCTION__, xpath);
#endif 

    res = readlink(xpath, buf, size - 1);
    if(res == -1)
        return -errno;

    buf[res] = '\0';
    return 0;
}

static int cf_opendir(const char *path, struct fuse_file_info *fi)
{
    DIR *dp;

    char xpath[CF_PATH_MAX];

    cf_path(config, path, xpath);

#ifdef CF_DEBUG
    cf_printf("%s %s\n", __FUNCTION__, xpath);
#endif 

    dp = opendir(xpath);
    if (dp == NULL)
        return -errno;

    fi->fh = (unsigned long) dp;
    return 0;
}

static int cf_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi)
{
    DIR *dp = get_dirp(fi);
    struct dirent *de;

    (void) path;
    seekdir(dp, offset);
    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        if (filler(buf, de->d_name, &st, de->d_off))
            break;
    }

    return 0;
}

static int cf_releasedir(const char *path, struct fuse_file_info *fi)
{
    DIR *dp = get_dirp(fi);
    (void) path;
    closedir(dp);
    return 0;
}

static int cf_mknod(const char *path, mode_t mode, dev_t rdev)
{
    int res;
    char xpath[CF_PATH_MAX];

    cf_path(config, path, xpath);

#ifdef CF_DEBUG
    cf_printf("%s %s\n", __FUNCTION__, xpath);
#endif 

    res = mknod(xpath, mode, rdev);
    if(res == -1) 
        return -errno;

    return 0;
}

static int cf_mkdir(const char *path, mode_t mode)
{
    int res;
    char xpath[CF_PATH_MAX];

    /* Some apps create too long paths ... */
    if (strlen(path) >= CF_PATH_MAX)
	return -ENAMETOOLONG;

    cf_path(config, path, xpath);

    res = mkdir(xpath, mode);
    if(res == -1)
        return -errno;

    return 0;
}

static int cf_unlink(const char *path)
{
    int res;
    int pos;
    int fd;
    int size;
    char xpath[CF_PATH_MAX];
    Cf_header header;

    cf_path(config, path, xpath);

#ifdef CF_DEBUG
    cf_printf("%s %s\n", __FUNCTION__, xpath);
#endif 

    res = unlink(xpath);
    if(res == -1)
        return -errno;

    pthread_mutex_lock(&cf_map_get_map_lock(mapping));

    fd = open (xpath, O_RDONLY);
    if (fd != -1) {
	header = cf_header_read(config, fd);
	size = cf_header_size(header);
	close(fd);
    } else {
	cf_printf("%s not found in %s\n", xpath, __FUNCTION__);
    }


    /* Update total fs size */
    cf_map_set_map_data_blocks(mapping, cf_map_get_map_data_blocks(mapping) - size);

    /* Delete from mapping (if present) */
    pos = cf_map_findname(mapping, xpath);
    /* Was it an already opened/cached entry */
    if ((pos != -1) && (cf_map_get_fd(mapping, pos) == -1)) {
#ifdef CF_DEBUG
	cf_printf("CANCELLING %s\n", xpath);
#endif 
/*
	cf_map_clean(mapping, pos);
*/
	strcpy(cf_map_get_name(mapping, pos), "");
    }
    pthread_mutex_unlock(&cf_map_get_map_lock(mapping));

    return 0;
}

static int cf_rmdir(const char *path)
{
    int res;
    char xpath[CF_PATH_MAX];

    cf_path(config, path, xpath);

    res = rmdir(xpath);
    if(res == -1)
        return -errno;

    return 0;
}

static int cf_symlink(const char *from, const char *to)
{
    int res;
    char xfrom[CF_PATH_MAX];
    char xto[CF_PATH_MAX];

    strcpy(xfrom, cf_config_get_backendpath(config));
    if (from[0] != '/')
	strcat(xfrom, "/");

    strcat(xfrom, from);

#ifdef CF_DEBUG
    cf_printf("%s %s\n", __FUNCTION__, xfrom);
#endif 
    cf_path(config, to, xto);

    res = symlink(from, xto);
    if(res == -1)
        return -errno;

    return 0;
}

static int cf_rename(const char *from, const char *to)
{
    int res;
    int pos;
    char xfrom[CF_PATH_MAX];
    char xto[CF_PATH_MAX];
    int replacing = 0;
    int sz = -1;
    int fd;

#ifdef CF_DEBUG_L2
cf_printf ("RENAME %s to %s\n", from, to);
#endif

    cf_path(config, from, xfrom);
    cf_path(config, to, xto);

    pthread_mutex_lock(&cf_map_get_map_lock(mapping));
    /* Remove cached mapping entry if present */
    pos = cf_map_findname(mapping, xto);
    if (pos != -1){
	if (cf_map_get_fd(mapping, pos) != -1)
           cf_printf("ERROR: renaming to a file (%s) which is still open in our mapping\n", xto);
    	else {
	   strcpy(cf_map_get_name(mapping, pos), "");
	   /* We replace a cached file */
	   replacing = 1;
	   sz = cf_map_get_size(mapping, pos);
	}
    }

    /* Are we replacing an existing but NOT cached file with this rename? */
    if (!replacing) {
	fd = open (xto, O_RDONLY);

	if (fd >= 0)
	   sz = file_size(fd);

    	if (sz >= 0) {
	  /* Replacing */
	  replacing = 1;
        }
    }
    pthread_mutex_unlock(&cf_map_get_map_lock(mapping));

    res = rename(xfrom, xto);
    if(res == -1)
        return -errno;


    pthread_mutex_lock(&cf_map_get_map_lock(mapping));
    /* So if this replaces a file then we must update the total fs size */
    if (replacing) {
	/* Remove size of overwritten file from total data size */
	cf_map_set_map_data_blocks(mapping, cf_map_get_map_data_blocks(mapping) - sz );
    }

    

    /* Update mapping entry if still open */
    pos = cf_map_findname(mapping, xfrom);
    if (pos != -1){
	if (cf_map_get_fd(mapping, pos) != -1)
           cf_printf("ERROR: renaming a file (%s) which was still open in our mapping\n", xfrom);
    	else
	   strcpy(cf_map_get_name(mapping, pos), xto);
    }
    pthread_mutex_unlock(&cf_map_get_map_lock(mapping));

    return 0;
}

static int cf_link(const char *from, const char *to)
{
    int res;
    char xfrom[CF_PATH_MAX];
    char xto[CF_PATH_MAX];

    cf_path(config, from, xfrom);
    cf_path(config, to, xto);

    res = link(xfrom, xto);
    if(res == -1)
        return -errno;

    return 0;
}

static int cf_chmod(const char *path, mode_t mode)
{
    int res;
    char xpath[CF_PATH_MAX];

    cf_path(config, path, xpath);

#ifdef CF_DEBUG
    cf_printf("%s %s\n", __FUNCTION__, xpath);
#endif 

    res = chmod(xpath, mode);
    if(res == -1)
        return -errno;

    /* Lock mapping */
    pthread_mutex_lock(&cf_map_get_map_lock(mapping));
    res = cf_map_findname(mapping, xpath);
    if ((res != -1) && (cf_map_get_fd(mapping, res) != -1))
        cf_map_set_ctime(mapping, res, time(NULL));
    /* Unlock mapping */
    pthread_mutex_unlock(&cf_map_get_map_lock(mapping));

    return 0;
}

static int cf_chown(const char *path, uid_t uid, gid_t gid)
{
    int res;
    char xpath[CF_PATH_MAX];

    cf_path(config, path, xpath);

#ifdef CF_DEBUG
    cf_printf("%s %s\n", __FUNCTION__, xpath);
#endif 

    res = lchown(xpath, uid, gid);
    if(res == -1)
        return -errno;

    return 0;
}

static int cf_truncate(const char *path, off_t size)
{
    int res;
    char xpath[CF_PATH_MAX];

    cf_path(config, path, xpath);

#ifdef CF_DEBUG
    cf_printf("TRUNCATE %s\n", xpath);
#endif 

    res = truncate(xpath, size);
    if(res == -1)
        return -errno;

    res = cf_map_open(mapping, xpath, O_RDONLY);


    pthread_mutex_lock(&cf_map_get_map_lock(mapping));
    if (res != -1) {
	pthread_rwlock_wrlock(&cf_map_get_lock(mapping,res));
	cf_map_set_size(mapping, res, size);
	cf_map_set_dirty(mapping, res, 1);
	pthread_rwlock_unlock(&cf_map_get_lock(mapping,res));
    }
    pthread_mutex_unlock(&cf_map_get_map_lock(mapping));

    /* cf_map_close will update the total fs size */
    if (res != -1)
        cf_map_close(mapping, res);

    return 0;
}

static int cf_utime(const char *path, struct utimbuf *buf)
{
    int res;
    char xpath[CF_PATH_MAX];

    cf_path(config, path, xpath);

#ifdef CF_DEBUG
    cf_printf("%s %s\n", __FUNCTION__, xpath);
#endif 


    pthread_mutex_lock(&cf_map_get_map_lock(mapping));
    /* Remove cached mapping entry if present */
    res = cf_map_findname(mapping, xpath);
    if (res != -1){
	/* Set mtime */
	cf_map_set_mtime(mapping, res, buf->modtime);

	/* Set atime */
	cf_map_set_atime(mapping, res, buf->actime);
    }
    pthread_mutex_unlock(&cf_map_get_map_lock(mapping));


    res = utime(xpath, buf);
    if(res == -1)
        return -errno;

    return 0;
}


static int cf_open(const char *path, struct fuse_file_info *fi)
{
    int pos;
    char xpath[CF_PATH_MAX];

    /* Some apps create too long paths ... */
    if (strlen(path) >= CF_PATH_MAX)
	return -ENAMETOOLONG;

    cf_path(config, path, xpath);

#ifdef CF_DEBUG
    cf_printf("%s %s\n", __FUNCTION__, xpath);
#endif 

    pos = cf_map_open(mapping, xpath, fi->flags);
    if (pos == -1) {
        return -errno;
    }

    fi->fh = pos;

    return 0;
}

static int cf_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi)
{
    int res;
    int pos;
    int i_size;
    int len;
    char *i_buffer;

    (void) path;

#ifdef CF_DEBUG
cf_printf("READ %s\n", path);
#endif

    pos = fi->fh;

    /* Normal read for files not compressed by us */
    if (cf_map_get_no_compress(mapping, pos)) {
	res = pread(cf_map_get_fd(mapping, pos), buf, size, sizeof(Cf_header)+offset);

	if (res == -1)
		res = -errno;

	return res;
    }

    pthread_rwlock_rdlock(&cf_map_get_lock(mapping,pos));

    cf_map_set_atime(mapping, pos, time(NULL));

    i_buffer = cf_map_get_data(mapping, pos);
    i_size = cf_map_get_size(mapping, pos);


#ifdef CF_DEBUG_L2
cf_printf("READ in %d (%f)\n", i_size, (float)offset+(float)size);
#endif

    if (offset > i_size)
	cf_printf("ERROR: offset (%d) beyond i_size (%d)\n", (int)offset, i_size);

/*
	[.... .... .... ....]
        0		     i_size
		^         ^
		|         |
	      offset  offset+size

	|-------------------|  = i_size
		|---------| = size
		
*/
    len = size;

    /* Adjust size to data present */   
    if (i_size < (offset+size))
	len = i_size - offset;


#ifdef CF_DEBUG_L2
cf_printf("copying %d (%f) ", len, (float)offset+(float)len);
#endif

    /* Copy to output */
    memcpy(buf, i_buffer+offset, len);

#ifdef CF_DEBUG_L2
cf_puts("copied");
#endif


    res = len;

    pthread_rwlock_unlock(&cf_map_get_lock(mapping,pos));

    return res;
}

static int cf_write(const char *path, const char *buf, size_t size,
                     off_t offset, struct fuse_file_info *fi)
{
    int res;
    int pos;
    int i_size;
    int i_buffer_size;
    char *i_buffer;

#ifdef CF_DEBUG
cf_printf("WRITE %s\n", path);
#endif

    pos = fi->fh;



	if (cf_map_get_flags(mapping, pos) == O_RDONLY)
		cf_puts("ERROR: writing to a readonly file!");


	i_size = cf_map_get_size(mapping, pos);
	if (offset+size > i_size) {
		cf_map_set_size(mapping, pos, offset+size);
	}

	/* No to be compressed by us */
	if (cf_map_get_no_compress(mapping, pos)) {
		res = pwrite(cf_map_get_fd(mapping, pos), buf, size, sizeof(Cf_header)+offset);

		if (res == -1)
			res = -errno;

		return res;
	}


	pthread_rwlock_wrlock(&cf_map_get_lock(mapping,pos));

    cf_map_set_mtime(mapping, pos, time(NULL));

	i_buffer = cf_map_get_data(mapping, pos);
	i_buffer_size = cf_map_get_buffer_size(mapping, pos);

	if (offset+size > i_buffer_size) {
		i_buffer_size = offset+size;
		i_buffer = (char *) realloc(i_buffer, i_buffer_size);

		cf_map_set_data(mapping, pos, i_buffer);
		cf_map_set_buffer_size(mapping, pos, i_buffer_size);
	}

	/* copy */
	memcpy(i_buffer+offset, buf, size);

	cf_map_set_dirty(mapping, pos, 1);

    res = size;


pthread_rwlock_unlock(&cf_map_get_lock(mapping,pos));

    return res;
}

static int cf_statfs(const char *path,  struct statvfs  *stbuf)
{
    int res;
    char xpath[CF_PATH_MAX];

    cf_path(config, path, xpath);

    res = statvfs(xpath, stbuf);
    if(res == -1)
        return -errno;

    /* We do not lock the mapping but we can not be very wrong anyway */
/*
    stbuf->f_blocks = stbuf->f_bavail + ((int)cf_map_get_map_data_blocks(mapping) / stbuf->f_bsize);
*/
/* df: used = blocks - free */

    stbuf->f_bfree = stbuf->f_blocks - (cf_map_get_map_data_blocks(mapping) / stbuf->f_bsize);
    stbuf->f_bavail = stbuf->f_bfree;

    return 0;
}

static int cf_release(const char *path, struct fuse_file_info *fi)
{
    int res;
    int pos = fi->fh;

    res = cf_map_close(mapping, pos);

    if(res == -1)
	return -errno;

    return 0;
}

static int cf_fsync(const char *path, int isdatasync,
                     struct fuse_file_info *fi)
{
    int res;
    int pos;
    (void) path;

    pos = fi->fh;

    if (isdatasync)
        res = fdatasync(cf_map_get_fd(mapping, pos));
    else
        res = fsync(cf_map_get_fd(mapping, pos));
    if(res == -1)
        return -errno;

    return 0;
}

#ifdef HAVE_SETXATTR
/* xattr operations are optional and can safely be left unimplemented */
static int cf_setxattr(const char *path, const char *name, const char *value,
                        size_t size, int flags)
{
    int res;

    char xpath[CF_PATH_MAX];

    cf_path(config, path, xpath);

    res = lsetxattr(xpath, name, value, size, flags);
    if(res == -1)
        return -errno;
    return 0;
}

static int cf_getxattr(const char *path, const char *name, char *value,
                    size_t size)
{
    int res;
    char xpath[CF_PATH_MAX];

    cf_path(config, path, xpath);

    res = lgetxattr(path, name, value, size);
    if(res == -1)
        return -errno;
    return res;
}

static int cf_listxattr(const char *path, char *list, size_t size)
{
    int res;
    char xpath[CF_PATH_MAX];

    cf_path(config, path, xpath);

    res = llistxattr(path, list, size);
    if(res == -1)
        return -errno;
    return res;
}

static int cf_removexattr(const char *path, const char *name)
{
    int res;
    char xpath[CF_PATH_MAX];

    cf_path(config, path, xpath);

    res = lremovexattr(path, name);
    if(res == -1)
        return -errno;
    return 0;
}
#endif /* HAVE_SETXATTR */


void* cf_scrubber(void *ptr) {
	while (1) {
		sleep(100);

		if (!pthread_mutex_lock(&cf_map_get_map_lock(mapping))) {
			#ifdef CF_DEBUG
			cf_puts("scrub");
			#endif

			cf_map_scrub(mapping);

			#ifdef CF_DEBUG
			cf_map_print(mapping);
			#endif
			pthread_mutex_unlock(&cf_map_get_map_lock(mapping));
		}
	}
}

void cf_message(void) {
	/* A little ego tripping... */
	printf("---------------------------------------------------------------------\n");
	printf("compFUSEd version %d.%d by Johan Parent  (johan@info.vub.ac.be)\n", CF_VERSION_MAJ, CF_VERSION_MIN);
	printf("\n");
	printf("\t - This version can run in multi-threaded mode!\n");
	printf("\t - You run this program at your own risk, do not store valuable data on this FS\n");
	printf("\t - NEVER modify anything in the backend directory while the compFUSEd FS is mounted\n");
	printf("\n");
	printf("Feedback at above mentioned address\n");
	printf("---------------------------------------------------------------------\n");
	printf("\n");
}

static void* cf_init(void)
{
	/*
		We open then delete the cf_fs_info.bin file!!
	*/
	int fd;
	char xpath[CF_PATH_MAX];
	Cf_map m;
	Info info;

	m = cf_map_alloc();

	cf_path(config, "/.cf_fs_info.bin", xpath);

	fd = open(xpath, O_RDONLY);

	if (fd == -1)
		perror("Can not read fs info, this is normal if it is the first time your start it.");


	if ((fd == -1) || cf_config_get_forced_scan(config)) {
		printf("compFUSEd will attempt to build or rebuild some filesystem information\n");
		printf("This may take a while... \n\n");
		/* Do a quiet scan */
		cf_scan(cf_config_get_backendpath(config), info, 0);
		printf("\n");
		printf("%d Mbytes compressed to %d Mbytes (%d files, %d dirs)\n", info[comp] / (1024*1024), info[raw]/(1024*1024), info[files], info[dirs]);
		cf_map_set_map_data_blocks(mapping, info[comp]);
		
	} else {
		pread(fd, m, sizeof(Cf_map_base), 0);
		cf_map_set_map_data_blocks(mapping, cf_map_get_map_data_blocks(m));		
		close(fd);
	}

	cf_printf("compFUSEd contains %ld bytess\n", cf_map_get_map_data_blocks(mapping));

        unlink(xpath);

	/* Lock for mapping global var */
	pthread_mutex_init(&cf_map_get_map_lock(mapping), NULL);

	/* Start the thread which will scan the mapping to free buffers */
	pthread_create(&scrubber, NULL, cf_scrubber, config);

	cf_message();

	return m;
}

static void cf_destroy(void *ptr)
{
	/*
		All this is useless crap right now, I save nothing usefull.
	*/
	int fd;
	char xpath[CF_PATH_MAX];
	Cf_map m = (Cf_map) ptr;

	cf_path(config, "/.cf_fs_info.bin", xpath);

	fd = open(xpath, O_WRONLY|O_CREAT, S_IRWXU);
	if (fd == -1) {
		cf_puts("Can not write fs info");
	} else {
		cf_printf("compFUSEd contains %ld bytes\n", cf_map_get_map_data_blocks(mapping));
		pwrite(fd, mapping, sizeof(Cf_map_base), 0);
		close(fd);
	}


	cf_map_dealloc(m);
	cf_log_close();

}

static struct fuse_operations cf_oper = {
    .getattr	= cf_getattr,
    .readlink	= cf_readlink,
    .opendir	= cf_opendir,
    .readdir	= cf_readdir,
    .releasedir	= cf_releasedir,
    .mknod	= cf_mknod,
    .mkdir	= cf_mkdir,
    .symlink	= cf_symlink,
    .unlink	= cf_unlink,
    .rmdir	= cf_rmdir,
    .rename	= cf_rename,
    .link	= cf_link,
    .chmod	= cf_chmod,
    .chown	= cf_chown,
    .truncate	= cf_truncate,
    .utime	= cf_utime,
    .open	= cf_open,
    .read	= cf_read,
    .write	= cf_write,
    .statfs	= cf_statfs,
    .release	= cf_release,
    .fsync	= cf_fsync,
    .init	= cf_init,
    .destroy	= cf_destroy,
#ifdef HAVE_SETXATTR
    .setxattr	= cf_setxattr,
    .getxattr	= cf_getxattr,
    .listxattr	= cf_listxattr,
    .removexattr= cf_removexattr,
#endif
};


int main(int argc, char *argv[])
{
	char config_file[128];

	/* Read config file */
	strcpy(config_file, argv[0]);
	strcat(config_file, ".conf");

	config = cf_config_alloc();
	cf_config_init(config);
	cf_config_read(config, config_file);

        /* Init defaults */
	mapping = cf_map_alloc();
	cf_map_init(mapping, config);


    return fuse_main(argc, argv, &cf_oper);
}
