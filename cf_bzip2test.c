#include <stdio.h>
#include <sys/types.h>
       #include <sys/stat.h>
       #include <fcntl.h>
#include <bzlib.h>
#include <sys/time.h>

int main(int argc, char **argv){
	int mem;
	int wrk;
	char *src;
	int src_len;
	char *dst;
	int dst_len;;
        int r;
	struct timeval start, stop;
	float sec, rate;	
	FILE *fp;
	int sz;
	int best_mem, best_wrk;
	float best_rate = 0.0;

	fp = fopen(argv[1], "rb");
	fseek(fp, 0, SEEK_END);
	sz = ftell(fp);
	fseek(fp, 0, SEEK_SET);		

	src_len = sz;
	dst_len = sz;

	src = (char *) malloc(sz);
	dst = (char *) malloc(sz);

	fread(src, 1, sz, fp);
	fclose(fp);

	for (mem = 1; mem<10; mem++) {
		for (wrk=0; wrk<251; wrk+= 50) {
			dst_len = sz;

			gettimeofday(&start, NULL);
		        r = BZ2_bzBuffToBuffCompress(dst,&dst_len,src,src_len, mem, 0, wrk);
			gettimeofday(&stop, NULL);

			sec = (stop.tv_sec - start.tv_sec) + (stop.tv_usec - start.tv_usec) / 1000000.0;

	        	if (r != BZ_OK)
	        	        printf("ERROR: compressed %lu bytes into %lu bytes\n", (long) src_len, (long) dst_len);
			else {
				rate = (src_len - dst_len) /sec;
				printf("mem=%d wrk=%d %f(b/sec) %d in %f \n", mem, wrk, rate, src_len- dst_len, sec );

				if (rate > best_rate) {
					best_rate = rate;
					best_mem = mem;
					best_wrk = wrk;
				}
			}
		}
	}

	printf("Best settings mem= %d wrk= %d\n", best_mem, best_wrk);

        return r;
}


