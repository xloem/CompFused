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



int cf_scan_rec(char *dir, Info info, int verbose) {
	struct stat buf;
        struct dirent **namelist;
	char path[4096];
        int n;
	int ifd;

        n = scandir(dir, &namelist, 0, alphasort);
    	if (n < 0)
    		perror("scandir");
   	else {
              	while(n--) {
			strcpy(path, dir);
			strcat(path, "/");
			strcat(path, namelist[n]->d_name);

			if (verbose) {
	                   	printf("\r%s", path);
				fflush(stdout);
			}

			stat(path, &buf);
			if ( S_ISREG(buf.st_mode)) {
				ifd = open(path, O_RDONLY);
				info[comp] += file_size(ifd);
				close(ifd);
				
				info[files]++;
				info[raw] += buf.st_size;
			}

			if (strcmp(namelist[n]->d_name, "..")  && strcmp(namelist[n]->d_name, ".")) {
				if ( S_ISDIR(buf.st_mode)) {
					info[dirs]++;
					cf_scan_rec(path, info, verbose);
				}
			}
                   	free(namelist[n]);
               	}
               	free(namelist);
       }

	return 0;
}

int cf_scan(char *path, Info info, int verbose) {
	cf_scan_rec(path, info, verbose);

	return 0;
}
