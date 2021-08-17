#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
	FILE *fp[20];
	int i;
	int stop = 2;
	char buf[512];

	if (argc > 1)
		stop = atoi(argv[1]);

	printf("open-read-close %d times\n", stop);
	for (i=0; i<stop; i++)
		fp[0] = fopen("file.txt", "rw");
		fread(buf, 10, 1, fp[0]);
		fclose(fp[0]);

	printf("open %d times\n", stop);
	for (i=0; i<stop; i++)
		fp[i] = fopen("file.txt", "rw");

	printf("read %d times\n", stop);
	for (i=0; i<stop; i++)
		if (i==0)
			fprintf(fp[i], "azerty");
		else
			fread(buf, 10, 1, fp[i]);
		

	printf("close %d times\n", stop);
	for (i=0; i<stop; i++)
		fclose(fp[i]);


	return 0;
}
	
