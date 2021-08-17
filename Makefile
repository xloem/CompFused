CC=gcc
AR=
CFLAGS= -Wall -g -D_FILE_OFFSET_BITS=64 
LIBS= -lfuse 
IDIR=-I.
LDIR=
TARGETS=cf_main cf_inspect cf_extract cf_fsinfo
DATE=`date +%Y%j%H`
PWD=`pwd`

#
# Set to 1 to include support
#
USE_ZLIB=1
USE_BZIP2=1
USE_LZO=1

ifeq ($(USE_ZLIB),1)
	CFLAGS += -D CF_USE_ZLIB
	LIBS += -lz
endif

ifeq ($(USE_LZO),1)
	CFLAGS += -D CF_USE_LZO
	LIBS += -llzo
endif

ifeq ($(USE_BZIP2),1)
	CFLAGS += -D CF_USE_BZIP2
	LIBS += -lbz2
endif

default: $(TARGETS)

%.o:	%.c %.h
	$(CC) -c $< $(CFLAGS) $(IDIR)

debug:  CFLAGS += -D CF_DEBUG
debug:	default

debugmax:  CFLAGS += -D CF_DEBUG_L2
debugmax: debug

libcf.a: cf_file.o cf_map.o cf_config.o cf_compression.o cf_log.o cf_scan.o
	ar -r $@  $^

cf_fsinfo: cf_fsinfo.c libcf.a
	$(CC) -o $@ $^ -g $(CFLAGS) $(LIBS) $(LDIR)

cf_inspect: cf_inspect.c libcf.a
	$(CC) -o $@ $^ -g $(CFLAGS) $(LIBS) $(LDIR)

cf_extract: cf_extract.c libcf.a
	$(CC) -o $@ $^ -g $(CFLAGS) $(LIBS) $(LDIR)

cf_main:  cf_main.c libcf.a
	$(CC) -o $@ $^ -g  $(CFLAGS) $(LIBS) $(LDIR)


clean:
	rm -fv *.o $(TARGETS)

tar:
	tar -zcvf cf-$(DATE).tgz ../../CompFused/v0.04/* --exclude=*.tgz --exclude=*~ \
		--exclude=cf_backend/* --exclude=*.o --exclude=cf_main --exclude=cf_mount/* \
		--exclude=*.log --exclude=log.txt --exclude=libcf.a
