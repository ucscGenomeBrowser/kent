/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* dystring - dynamically resizing string. */
#ifndef DYSTRING_H
#define DYSTRING_H

struct dyString
/* Dynamically resizable string that you can do formatted
 * output to. */
    {
    struct dyString *next;	/* Next in list. */
    char *string;		/* Current buffer. */
    int bufSize;		/* Size of buffer. */
    int stringSize;		/* Size of string. */
    };

struct dyString *newDyString(int initialBufSize);
/* Allocate dynamic string with initial buffer size.  (Pass zero for default) */

#define dyStringNew newDyString

void freeDyString(struct dyString **pDs);
/* Free up dynamic string. */

#define dyStringFree(a) freeDyString(a);

void dyStringAppend(struct dyString *ds, char *string);
/* Append zero terminated string to end of dyString. */

void dyStringAppendN(struct dyString *ds, char *string, int stringSize);
/* Append string of given size to end of string. */

char dyStringAppendC(struct dyString *ds, char c);
/* Append char to end of string.  You might want to use the generally faster dyStringPut. */

void dyStringVaPrintf(struct dyString *ds, char *format, va_list args);
/* VarArgs Printf to end of dyString. */

void dyStringPrintf(struct dyString *ds, char *format, ...);
/*  Printf to end of dyString.  Don't do more than 4000 characters this way... */

#define dyStringClear(ds) (ds->string[0] = ds->stringSize = 0)
/* Clear string. */

#endif /* DYSTRING_H */

