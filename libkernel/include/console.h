#ifndef _PRINTK_H
#define _PRINTK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lib/stddef.h"
#include "lib/types.h"

struct _nolibc_fd;
typedef struct _nolibc_fd FILE;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

#define EOF (-1)

/* stdio.h shall not define va_list if it is included, but it shall
 * declare functions that use va_list.
 */
#ifndef va_list
#define __STDIO_H_DEFINED_va_list
#define va_list __builtin_va_list
#endif

#ifndef __printf
#define __printf(fmt, args) __attribute__((format(printf, (fmt), (args))))
#endif
#ifndef __scanf
#define __scanf(fmt, args) __attribute__((format(scanf, (fmt), (args))))
#endif

int vsnprintf(char *str, size_t size, const char *fmt, va_list ap);
int snprintf(char *str, size_t size, const char *fmt, ...) __printf(3, 4);

int vsprintf(char *str, const char *fmt, va_list ap);
int sprintf(char *str, const char *fmt, ...) __printf(2, 3);

int vfprintf(FILE *fp, const char *fmt, va_list ap);
int fprintf(FILE *fp, const char *fmt, ...) __printf(2, 3);
int fflush(FILE *fp);

int vprintf(const char *fmt, va_list ap);
int printf(const char *fmt, ...) __printf(1, 2);

int vsscanf(const char *str, const char *fmt, va_list ap);
int sscanf(const char *str, const char *fmt, ...) __scanf(2, 3);

int vasprintf(char **str, const char *fmt, va_list ap);
int asprintf(char **str, const char *fmt, ...) __printf(2, 3);

void psignal(int sig, const char *s);

int fputc(int _c, FILE *fp);
int putchar(int c);
int fputs(const char *restrict s, FILE *restrict stream);
int puts(const char *s);

#if CONFIG_LIBVFSCORE
int rename(const char *oldpath, const char *newpath);
#endif

#ifdef __STDIO_H_DEFINED_va_list
#undef va_list
#endif

#ifdef __cplusplus
}
#endif

#endif /* __STDIO_H__ */