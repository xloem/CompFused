#ifndef __CF_COMPRESSION__
#define __CF_COMPRESSION__

#include <zlib.h>
#include <bzlib.h>
#include <lzo1x.h>
#include <lzoutil.h>

#define BZ2_MEM		1	/* Influences speed and mem */
#define BZ2_WFACTOR	0	/* Influences compression and speed */

typedef enum { 
	CF_NONE,
	CF_ZLIB, 
	CF_LZO, 
	CF_BZIP2 } Cf_compression;

typedef void(* Cf_error_f)(int );
typedef int (* Cf_buffersizer_f)(int );
typedef int (* Cf_compressor_f)(char *dst, int *dst_len, char *src, int src_len);
typedef int (* Cf_decompressor_f)(char *dst, int *dst_len, char *src, int src_len);


/* cf_compression.c */

void zlib_error(int);

/* NONE WRAPPERS */
void none_error(int);
int none_buffersizer(int sz);
int none_compressor(char *dst, int *dst_len, char *src, int src_len);
int none_decompressor(char *dst, int *dst_len, char *src, int src_len);

/* LZO WRAPPERS */
void lzo_error(int);
int lzo_buffersizer(int sz);
int lzo_compressor(char *dst, int *dst_len, char *src, int src_len);
int lzo_decompressor(char *dst, int *dst_len, char *src, int src_len);

/* BZIP2 WRAPPERS */
void bzip2_error(int);
int bzip2_buffersizer(int sz);
int bzip2_compressor(char *dst, int *dst_len, char *src, int src_len);
int bzip2_decompressor(char *dst, int *dst_len, char *src, int src_len);

#endif
