/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* dystring - dynamically resizing string. */
#include "common.h"
#include "dystring.h"

struct dyString *newDyString(int initialBufSize)
/* Allocate dynamic string with initial buffer size.  (Pass zero for default) */
{
struct dyString *ds;
AllocVar(ds);
ds->string = needMem(initialBufSize+1);
ds->bufSize = initialBufSize;
return ds;
}

void freeDyString(struct dyString **pDs)
/* Free up dynamic string. */
{
struct dyString *ds;
if ((ds = *pDs) != NULL)
    {
    freeMem(ds->string);
    freez(pDs);
    }
}

static void dyStringExpandBuf(struct dyString *ds, int newSize)
/* Expand buffer to new size. */
{
ds->string = needMoreMem(ds->string, ds->stringSize+1, newSize+1);
ds->bufSize = newSize;
}

void dyStringAppendN(struct dyString *ds, char *string, int stringSize)
/* Append string of given size to end of string. */
{
int oldSize = ds->stringSize;
int newSize = oldSize + stringSize;
int newAllocSize = newSize + oldSize;
char *buf;
if (newSize > ds->bufSize)
    dyStringExpandBuf(ds,newAllocSize);
buf = ds->string;
memcpy(buf+oldSize, string, stringSize);
ds->stringSize = newSize;
buf[newSize] = 0;
}

void dyStringAppend(struct dyString *ds, char *string)
/* Append zero terminated string to end of dyString. */
{
dyStringAppendN(ds, string, strlen(string));
}

void dyStringVaPrintf(struct dyString *ds, char *format, va_list args)
/* VarArgs Printf to end of dyString. */
{
char string[4*1024];	/* Sprintf buffer */
int size;

size = vsprintf(string, format, args);
if (size >= sizeof(string))
    errAbort("Sprintf size too long in dyStringVaPrintf");	/* If we're still alive... */
dyStringAppendN(ds, string, size);
}

void dyStringPrintf(struct dyString *ds, char *format, ...)
/*  Printf to end of dyString.  Don't do more than 1000 characters this way... */
{
va_list args;
va_start(args, format);
dyStringVaPrintf(ds, format, args);
va_end(args);
}
