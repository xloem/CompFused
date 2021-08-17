
#define	_GNU_SOURCE

#include <stdio.h>
#include <stdarg.h>

#include "cf_log.h"

FILE *output;

void cf_log_open(char *path) {

	if (path == NULL)
		output = stdout;
	else
		output = fopen(path, "a");
}

void cf_log_close() {
	if (output != stdout)
		fclose(output);
}

void cf_log_puts(const char *s) {
	fputs(s, output);
	fflush(output);
}

void cf_log_printf(char *format, ...) {
     va_list ap;

     
     va_start ( ap, format );
     vfprintf ( output, format, ap );
     va_end ( ap );

     fflush(output);
}
