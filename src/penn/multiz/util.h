/* $Id: util.h,v 1.1 2002/09/24 18:04:18 kent Exp $ */

#ifndef PIPLIB_UTIL
#define PIPLIB_UTIL

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <math.h>     /* HUGE_VAL */
#include <limits.h>   /* INT_MAX, INT_MIN, LONG_MAX, LONG_MIN, etc. */
#include <stdarg.h>
#include <assert.h>
#include <errno.h>

typedef int bool;

extern char *argv0;

void print_argv0(void);
#ifdef __GNUC__     /* avoid some "foo might be used uninitialized" warnings */
	void fatal(const char *msg) __attribute__ ((noreturn));
	void fatalf(const char *fmt, ...) __attribute__ ((noreturn));
	void fatalfr(const char *fmt, ...) __attribute__ ((noreturn));
#else
	void fatal(const char *msg);
	void fatalf(const char *fmt, ...);
	void fatalfr(const char *fmt, ...);
#endif
FILE *ckopen(const char *name, const char *mode);
void ckfree(void* p);
void *ckalloc(size_t amount);
void *ckallocz(size_t amount);
bool same_string(const char *s, const char *t);
bool starts(const char *s, const char *t);
char *skip_ws(char *s);
char *copy_string(const char *s);
char *copy_substring(const char *s, int n);
unsigned int roundup(unsigned int n, unsigned int m);

#undef MAX
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#undef MIN
#define MIN(x,y) ((x) < (y) ? (x) : (y))

#endif
