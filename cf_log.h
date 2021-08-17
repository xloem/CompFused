#ifndef __CF_LOG__
#define __CF_LOG__


#define	cf_puts		cf_log_puts
#define	cf_printf	cf_log_printf

void cf_log_open(char *s);
void cf_log_close();

void cf_log_puts(const char *s);
void cf_log_printf(char *format, ...);

#endif
