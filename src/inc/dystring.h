/* dystring - dynamically resizing string.
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#ifndef DYSTRING_H	/* Wrapper to avoid including this twice. */
#define DYSTRING_H

#include "common.h"

struct dyString
/* Dynamically resizable string that you can do formatted
 * output to. */
    {
    struct dyString *next;	/* Next in list. */
    char *string;		/* Current buffer. */
    long bufSize;		/* Size of buffer. */
    long stringSize;		/* Size of string. */
    };

struct dyString *newDyString(long initialBufSize);
/* Allocate dynamic string with initial buffer size.  (Pass zero for default) */

#define dyStringNew newDyString

void freeDyString(struct dyString **pDs);
/* Free up dynamic string. */

#define dyStringFree(a) freeDyString(a);

void freeDyStringList(struct dyString **pDs);
/* Free up a list of dynamic strings */

#define dyStringFreeList(a) freeDyStringList(a);

void dyStringAppend(struct dyString *ds, char *string);
/* Append zero terminated string to end of dyString. */

void dyStringAppendN(struct dyString *ds, char *string, long stringSize);
/* Append string of given size to end of string. */

char dyStringAppendC(struct dyString *ds, char c);
/* Append char to end of string. */

void dyStringAppendMultiC(struct dyString *ds, char c, int n);
/* Append N copies of char to end of string. */

void dyStringAppendEscapeQuotes(struct dyString *dy, char *string,
	char quot, char esc);
/* Append escaped-for-quotation version of string to dy. */

#define dyStringWriteOne(dy, var) dyStringAppendN(dy, (char *)(&var), sizeof(var))
/* Write one variable (binary!) to dyString - for cases when want to treat string like
 * a file stream. */

void dyStringVaPrintf(struct dyString *ds, char *format, va_list args);
/* VarArgs Printf to end of dyString. */

void dyStringPrintf(struct dyString *ds, char *format, ...)
/*  Printf to end of dyString. */
#ifdef __GNUC__
__attribute__((format(printf, 2, 3)))
#endif
;

struct dyString *dyStringCreate(char *format, ...)
/*  Create a dyString with a printf style initial content */
#if defined(__GNUC__)
__attribute__((format(printf, 1, 2)))
#endif
;

#define dyStringClear(ds) (ds->string[0] = ds->stringSize = 0)
/* Clear string. */

struct dyString * dyStringSub(char *orig, char *in, char *out);
/* Make up a duplicate of orig with all occurences of in substituted
 * with out. */

void dyStringBumpBufSize(struct dyString *ds, long size);
/* Force dyString buffer to be at least given size. */

char *dyStringCannibalize(struct dyString **pDy);
/* Kill dyString, but return the string it is wrapping
 * (formerly dy->string).  This should be free'd at your
 * convenience. */

#define dyStringContents(ds) (ds)->string
/* return raw string. */

#define dyStringLen(ds) ds->stringSize
/* return raw string length. */

#define dyStringIsEmpty(ds) (ds->stringSize == 0)
/* Return TRUE if dyString is empty. */

#define dyStringIsNotEmpty(ds) (ds->stringSize > 0)
/* Return TRUE if dyString is not empty. */

void dyStringResize(struct dyString *ds, long newSize);
/* resize a string, if the string expands, blanks are appended */

void dyStringQuoteString(struct dyString *dy, char quotChar, char *text);
/* Append quotChar-quoted text (with any internal occurrences of quotChar
 * \-escaped) onto end of dy. */

INLINE void dyStringAppendSep(struct dyString *dy, char *sep)
/* If dy is not empty then append sep; otherwise leave it empty.  For building up lists without
 * a separator at the end. */
{
if (dyStringIsNotEmpty(dy))
    dyStringAppend(dy, sep);
}

#endif /* DYSTRING_H */

