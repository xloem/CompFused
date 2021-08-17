/*
	compFUSEd: compressed Filesystem in Userspace

*/

#include <unistd.h>
#include <string.h>

#include "cf_compression.h"

/*
#include "cf_defines.h"
*/

/* ZLIB wrappers */

/* these are not needed as the signature is the same that of the wrappers */

void zlib_error(int e) {
	switch(e) {
		case Z_OK:
            		puts("Z_OK");
			break;

		case Z_STREAM_END:
			puts("Z_STREAM_END");
			break;

		case Z_NEED_DICT:
			puts("Z_NEED_DICT");
			break;

		case Z_ERRNO:
			puts("Z_ERRNO");
			break;

		case Z_STREAM_ERROR:
			puts("Z_STREAM_ERROR");
			break;

		case Z_DATA_ERROR:
			puts("Z_DATA_ERROR");
			break;

		case Z_MEM_ERROR:
			puts("Z_MEM_ERROR");
			break;

		case Z_BUF_ERROR:
			puts("Z_BUF_ERROR");
			break;

		case Z_VERSION_ERROR:
			puts("Z_VERSION_ERROR");
			break;
	}
}



/* none wrappers */

void none_error(int e) {
}

int none_buffersizer(int sz) {
	return sz;
}


int none_compressor(char *dst, int *dst_len, char *src, int src_len) {
	memcpy(dst, src, src_len);

	/* Sleep to fake compression time : compression 2X slower */
	usleep(2*src_len);

	*dst_len = src_len;

	return 0;
}

int none_decompressor(char *dst, int *dst_len, char *src, int src_len) {
	memcpy(dst, src, src_len);

	/* Sleep to fake compression time : compression 2X slower */
	usleep(src_len);

	*dst_len = src_len;

	return 0;
}

/* LZO wrappers  */

#ifdef CF_USE_LZO

void lzo_error(int e) {
	switch(e) {
		case LZO_E_ERROR:
            		puts("LZO_E_ERROR");
			break;

		case LZO_E_INPUT_OVERRUN:
			puts("LZO_E_INPUT_OVERRUN");
			break;

		case LZO_E_OUTPUT_OVERRUN:
			puts("LZO_E_OUTPUT_OVERRUN");
			break;

		case LZO_E_LOOKBEHIND_OVERRUN:
			puts("LZO_E_LOOKBEHIND_OVERRUN");
			break;

		case LZO_E_EOF_NOT_FOUND:
			puts("LZO_E_EOF_NOT_FOUND");
			break;

		case LZO_E_INPUT_NOT_CONSUMED:
			puts("LZO_E_INPUT_NOT_CONSUMED");
			break;
	}
}

int lzo_buffersizer(int sz) {
	return sz * 15 / 10;
}

int lzo_compressor(char *dst, int *dst_len, char *src, int src_len) {
        lzo_byte *wrkmem;
        int r;

/*
	printf("lzo in %d out %d\n", src_len, *dst_len);
*/
        wrkmem = (lzo_bytep) lzo_malloc(LZO1X_1_MEM_COMPRESS);

	if (!wrkmem)
		puts("lzo_malloc() failed!");

        r = lzo1x_1_compress(src,src_len,dst,dst_len,wrkmem);
        if (r != LZO_E_OK)
                printf("ERROR: compressed %lu bytes into %lu bytes\n",
                        (long) src_len, (long) dst_len);

/*
	printf("lzo in %d out %d\n", src_len, *dst_len);
*/
	if (wrkmem)
        	lzo_free(wrkmem);

        return r;
}

int lzo_decompressor(char *dst, int *dst_len, char *src, int src_len) {
        int r;

        r = lzo1x_decompress(src, src_len, dst, dst_len, NULL);
        if (r != LZO_E_OK)
                printf("ERROR: decompressed %lu bytes into %lu bytes\n",
                        (long) src_len, (long) dst_len);

        return r;
}

#endif


/* BZIP2 wrappers  */

#ifdef CF_USE_BZIP2

void bzip2_error(int e) {
	switch(e) {
		case BZ_OK:
            		puts("BZ_OK");
			break;

		case BZ_STREAM_END:
			puts("BZ_STREAM_END");
			break;

		case BZ_RUN_OK:
			puts("BZ_RUN_OK");
			break;

		case BZ_OUTBUFF_FULL:
			puts("BZ_OUTBUFF_FULL");
			break;

		case BZ_SEQUENCE_ERROR:
			puts("BZ_SEQUENCE_ERROR");
			break;

		case BZ_DATA_ERROR:
			puts("BZ_DATA_ERROR");
			break;

		case BZ_MEM_ERROR:
			puts("Z_MEM_ERROR");
			break;

		case BZ_IO_ERROR:
			puts("BZ_IO_ERROR");
			break;

		case BZ_CONFIG_ERROR:
			puts("BZ_CONFIG_ERROR");
			break;
	}
}

int bzip2_buffersizer(int sz) {
	return sz * 15 / 10;
}

int bzip2_compressor(char *dst, int *dst_len, char *src, int src_len) {
        int r;

        r = BZ2_bzBuffToBuffCompress(dst,dst_len,src,src_len, BZ2_MEM, 0, BZ2_WFACTOR);

        if (r != BZ_OK)
                printf("ERROR: compressed %lu bytes into %lu bytes\n",
                        (long) src_len, (long) dst_len);



        return r;
}

int bzip2_decompressor(char *dst, int *dst_len, char *src, int src_len) {
        int r;

        r = BZ2_bzBuffToBuffDecompress(dst,dst_len,src,src_len, 0, 0);
        if (r != BZ_OK)
                printf("ERROR: decompressed %lu bytes into %lu bytes\n",
                        (long) src_len, (long) dst_len);

        return r;
}

#endif
