#include "cf_map.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include "cf_file.h"
#include "cf_scan.h"


int scan(char *path, Info info) {
	printf("Scanning the entire backend directory.\n");
	printf("This may take a long time depending on your filesystem...\n");

	/* Do a verbose scan */
	cf_scan(path, info, 1); 

	printf("\n");
	printf("compFUSEd mount contains :\n");
	printf("%d files in %d directories\n", info[files] , info[dirs]);
	printf("%d Mbytes data on disk\n", info[raw] / (1024*1024));
	printf("%d Mbytes compressed data\n", info[comp] / (1024*1024));
	printf("\n");

	return 0;
}

int main(int argc, char **argv) {
	int fd;
	char answer[5];
	char path[128];
	Cf_map map;
	Info info;

	info[files]  = info[dirs] 
		= info[raw] 
		= info[comp] 
		= 0;

	if (argc != 2) {
		puts("Usage: cf_fsinfo backend_path");
	
		return 1;
	}

	sprintf(path, "%s/.cf_fs_info.bin", argv[1]);

	fd = open(path, O_RDONLY);

	if (fd == -1) {
		puts("Can not read info! Can happen if the compFUSEd fs is mounted, umount first.");

		return 2;
	}

	map = cf_map_alloc();

	read(fd, map, sizeof(Cf_map_base)); 

	printf("According to the fs info this compFUSEd fs contains %ld bytes\n", cf_map_get_map_data_blocks(map));

	printf("%s can do a full check, proceed? If so type YES: ", argv[0]);
	scanf("%s", answer);
	if (!strcmp(answer, "YES")) 
		scan(argv[1], info);

	close(fd);

	return 0;
}
