#ifndef __CF_SCAN__
#define __CF_SCAN__

typedef int Info[5];

enum { raw, comp, level, files, dirs };

int cf_scan(char *dir, Info info, int verbose); 

#endif
