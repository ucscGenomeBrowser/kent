/* dystring - dynamically resizing string.
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "dystring.h"


struct dyString *newDyString(long initialBufSize)
/* Allocate dynamic string with initial buffer size.  (Pass zero for default) */
{
struct dyString *ds;
AllocVar(ds);
if (initialBufSize == 0)
    initialBufSize = 512;
ds->string = needMem(initialBufSize+1);
ds->bufSize = initialBufSize;
return ds;
}

void dyStringFree(struct dyString **pDs)
/* Free up dynamic string. */
{
struct dyString *ds;
if ((ds = *pDs) != NULL)
    {
    freeMem(ds->string);
    freez(pDs);
    }
}

static void checkNOSQLINJ(struct dyString *ds)
/* Check if we are manipulating a special SQL source code string
 * and abort with stackdump if so. This is forbidden for SQL Injection security. */
{
if (startsWith("NOSQLINJ ", ds->string))
    {
    char *dump = getenv("noSqlInj_dumpStack");
    if (!(dump && sameString(dump, "off")))  // dump unless set to off
	dumpStack("dyString functions are not allowed for SQL source code. Use sqlDy safe functions instead.\n");
    char *level = getenv("noSqlInj_level");
    if (!level) level = "abort"; // Default
    if (sameString(level, "abort"))
	errAbort("dyString is not allowed. use sqlDy functions that are safe instead.");
    if (sameString(level, "warn"))
	warn("dyString is not allowed. use sqlDy functions that are safe instead.");
    if (sameString(level, "logOnly"))
	fprintf(stderr, "dyString is not allowed. use sqlDy functions that are safe instead.");
    }
}

char *dyStringCannibalize(struct dyString **pDy)
/* Kill dyString, but return the string it is wrapping
 * (formerly dy->string).  This should be free'd at your
 * convenience. */
{
char *s;
struct dyString *ds = *pDy;
assert(ds != NULL);
s = ds->string;
freez(pDy);
return s;
}

void dyStringListFree(struct dyString **pDs)
/* free up a list of dyStrings */
{
struct dyString *ds, *next;
for(ds = *pDs; ds != NULL; ds = next)
    {
    next = ds->next;
    dyStringFree(&ds);
    }
*pDs = NULL;
}

static void dyStringExpandBuf(struct dyString *ds, long newSize)
/* Expand buffer to new size. */
{
ds->string = needMoreMem(ds->string, ds->stringSize+1, newSize+1);
ds->bufSize = newSize;
}

void dyStringBumpBufSize(struct dyString *ds, long size)
/* Force dyString buffer to be at least given size. */
{
if (ds->bufSize < size)
    dyStringExpandBuf(ds, size);
}

void dyStringAppendN(struct dyString *ds, char *string, long stringSize)
/* Append string of given size to end of string. */
{
long oldSize = ds->stringSize;
long newSize = oldSize + stringSize;
char *buf;
if (newSize > ds->bufSize)
    {
    long newAllocSize = newSize + oldSize;
    long oldSizeTimesOneAndAHalf = oldSize * 1.5;
    if (newAllocSize < oldSizeTimesOneAndAHalf)
        newAllocSize = oldSizeTimesOneAndAHalf;
    dyStringExpandBuf(ds,newAllocSize);
    }
buf = ds->string;
memcpy(buf+oldSize, string, stringSize);
ds->stringSize = newSize;
buf[newSize] = 0;
checkNOSQLINJ(ds);
}

char dyStringAppendC(struct dyString *ds, char c)
/* Append char to end of string. */
{
char *s;
if (ds->stringSize >= ds->bufSize)
     dyStringExpandBuf(ds, ds->bufSize+256);
s = ds->string + ds->stringSize++;
*s++ = c;
*s = 0;
return c;
}

void dyStringAppendMultiC(struct dyString *ds, char c, int n)
/* Append N copies of char to end of string. */
{
long oldSize = ds->stringSize;
long newSize = oldSize + n;
long newAllocSize = newSize + oldSize;
char *buf;
if (newSize > ds->bufSize)
    dyStringExpandBuf(ds,newAllocSize);
buf = ds->string;
memset(buf+oldSize, c, n);
ds->stringSize = newSize;
buf[newSize] = 0;
}

void dyStringAppend(struct dyString *ds, char *string)
/* Append zero terminated string to end of dyString. */
{
dyStringAppendN(ds, string, strlen(string));
checkNOSQLINJ(ds);
}

void dyStringAppendEscapeQuotes(struct dyString *ds, char *string,
	char quot, char esc)
/* Append escaped-for-quotation version of string to dy. */
{
char c;
char *s = string;
while ((c = *s++) != 0)
     {
     if (c == quot)
         dyStringAppendC(ds, esc);
     dyStringAppendC(ds, c);
     }
checkNOSQLINJ(ds);
}

void dyStringVaPrintf(struct dyString *ds, char *format, va_list args)
/* VarArgs Printf to end of dyString. */
{
/* attempt to format the string in the current space.  If there
 * is not enough room, increase the buffer size and try again */
long avail, sz;
while (TRUE)
    {
    va_list argscp;
    va_copy(argscp, args);
    avail = ds->bufSize - ds->stringSize;
    if (avail <= 0)
        {
	/* Don't pass zero sized buffers to vsnprintf, because who knows
	 * if the library function will handle it. */
        dyStringExpandBuf(ds, ds->bufSize+ds->bufSize);
	avail = ds->bufSize - ds->stringSize;
	}
    sz = vsnprintf(ds->string + ds->stringSize, avail, format, argscp);
    va_end(argscp);

    /* note that some version return -1 if too small */
    if ((sz < 0) || (sz >= avail))
        dyStringExpandBuf(ds, ds->bufSize+ds->bufSize);
    else
        {
        ds->stringSize += sz;
        break;
        }
    }
}

void dyStringPrintf(struct dyString *ds, char *format, ...)
/*  Printf to end of dyString. */
{
va_list args;
va_start(args, format);
dyStringVaPrintf(ds, format, args);
va_end(args);
checkNOSQLINJ(ds);
}

struct dyString *dyStringCreate(char *format, ...)
/*  Create a dyString with a printf style initial content */
{
int len = strlen(format) * 3;
struct dyString *ds = dyStringNew(len);
va_list args;
va_start(args, format);
dyStringVaPrintf(ds, format, args);
va_end(args);
checkNOSQLINJ(ds);
return ds;
}

struct dyString * dyStringSub(char *orig, char *in, char *out)
/* Make up a duplicate of orig with all occurences of in substituted
 * with out. */
{
long inLen = strlen(in), outLen = strlen(out), origLen = strlen(orig);
struct dyString *ds = dyStringNew(origLen + 2*outLen);
char *s, *e;

if (orig == NULL) return NULL;
for (s = orig; ;)
    {
    e = stringIn(in, s);
    if (e == NULL)
	{
        e = orig + origLen;
	dyStringAppendN(ds, s, e - s);
	break;
	}
    else
        {
	dyStringAppendN(ds, s, e - s);
	dyStringAppendN(ds, out, outLen);
	s = e + inLen;
	}
    }
checkNOSQLINJ(ds);
return ds;
}

void dyStringResize(struct dyString *ds, long newSize)
/* resize a string, if the string expands, blanks are appended */
{
long oldSize = ds->stringSize;
if (newSize > oldSize)
    {
    /* grow */
    if (newSize > ds->bufSize)
        dyStringExpandBuf(ds, newSize + ds->stringSize);
    memset(ds->string+newSize, ' ', newSize);
    }
ds->string[newSize] = '\0';
ds->stringSize = newSize;
}

void dyStringQuoteString(struct dyString *ds, char quotChar, char *text)
/* Append quotChar-quoted text (with any internal occurrences of quotChar
 * \-escaped) onto end of dy. */
{
char c;

dyStringAppendC(ds, quotChar);
while ((c = *text++) != 0)
    {
    if (c == quotChar || c == '\\')
        dyStringAppendC(ds, '\\');
    dyStringAppendC(ds, c);
    }
dyStringAppendC(ds, quotChar);
checkNOSQLINJ(ds);
}

