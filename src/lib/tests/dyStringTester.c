/* dyStringTest - tests for dyString */

/* Copyright (C) 2005 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "dystring.h"

static void usage()
/* Print usage message */
{
errAbort(
   "dyStringTest - check for dyString\n"
   "usage:\n"
   "   dyStringTest\n"
   "\n");
}
int errCount = 0;  /* count of errors */

static void dyStringCheck(char *id, struct dyString *ds, char *expect)
/* check if a dyString contains the expected value */
{
if (!sameString(ds->string, expect))
    {
    fprintf(stderr, "dyString test %s failed: expected \"%s\", got \"%s\"\n",
            id, expect, ds->string);
    errCount++;
    }
}

static char *mkString(int len)
/* create test string with repetative pattern */
{
static char *base = "abcdefghijklmnopABCDEFGHIJKLMNOP";
int baseLen = strlen(base);
int numRep = (len+2*baseLen)/baseLen; /* a little slop */
int i;
char *p, *str = needMem((numRep*baseLen)+1);

for (p = str, i = 0; i < numRep; i++, p += baseLen)
    strcpy(p, base);
str[len] = '\0';
return str;
}

static void printfLargeTest(char *id, int len)
/* test dyStringPrintf with a large string */
{
struct dyString *ds = dyStringNew(0);
char *str = mkString(len);

dyStringPrintf(ds, "%s", str);
if (ds->stringSize != len)
    {
    fprintf(stderr, "dyString test %s failed: expected length of %d, got %ld\n",
            id, len, ds->stringSize);
    errCount++;
    }
else 
    dyStringCheck(id, ds, str);

freeMem(str);
dyStringFree(&ds);
}

static void printfTest()
/* test dyStringPrintf */
{
struct dyString *ds = dyStringNew(0);
int initBufSize = ds->bufSize;  /* size of new string buffer */

/* small string */
dyStringPrintf(ds, "%d %.2s %d", 10, "abcdefg", 20);
dyStringCheck("printfSmall", ds, "10 ab 20");
dyStringClear(ds);

/* large strings */
printfLargeTest("printfUnderBufSz", initBufSize-1);
printfLargeTest("printfAtBufSz", initBufSize);
printfLargeTest("printfOverBufSz", initBufSize+1);
#if 0 /* until x86_64 bug is fixed */
printfLargeTest("printf8000", 8000);
#endif

dyStringFree(&ds);
}

static void dyStringTest()
/* dyStringTest - tests for dyString */
{
printfTest();
if (errCount > 0)
    errAbort("%d tests failed", errCount);
}

int main(int argc, char *argv[])
{
if (argc != 1)
    usage();
dyStringTest();
return 0;
}


