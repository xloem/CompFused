#ifndef __CF_CHUNK__
#define __CF_CHUNK__

/*

[header][chunk0,chunk1,chunk2,...,chunkn][toc]

*/

typedef struct {
	unsigned char version;
	insigned char coding;
	unsigned int toc_size;		/* Number entries in toc (= #chunks) */
	unsigned int toc_offset;	/* Position toc, after last data byte */
	unsigned int data_size;		/* Uncompressed size */
} Cf_chunk_header_base;

typedef Cf_chunk_header_base *Cf_chunk_header;

typedef struct {
	unsigned int size;
	char *data;
} Cf_chunk_data;


typedef struct {
	unsigned int size;	/* Compressed size */
	unsigned int offset;	/* Position in the data part */
} Cf_chunk_toc;

int cf_chunk_read_header(int fd, Cf_chunk_header *header);
int cf_chunk_read_toc(int fd, Cf_chunk_toc *toc);

void cf_chunk_read_chunk(int fd, Cf_chunk_toc *toc, Cf_chunk_data *chk);



#endif
