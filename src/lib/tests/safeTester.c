/* safeTester - tests for safe* functions */

/* Copyright (C) 2006 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "errCatch.h"

static void usage()
/* Print usage message */
{
errAbort(
   "safeTester - tests for safe* functions\n"
   "usage:\n"
   "   safeTester\n"
   "\n");
}
int errCount = 0;  /* count of errors */

static void shouldHaveFailed(char *testId)
/* output test failure for case that should fail and didn't  */
{
fprintf(stderr, "Error: test %s should have failed\n", testId);
errCount++;
}
static void checkStr(char *testId, char *got, char *expect)
/* copy strings and generate error if they don't match */
{
if (!sameString(got, expect))
    {
    fprintf(stderr, "Error: test %s failed, got \"%s\", expected \"%s\"\n",
            testId, got, expect);
    errCount++;
    }
}

static void testSafecpy()
/* test safecpy */
{
struct errCatch *errCatch = errCatchNew();
char buf4[4];

safecpy(buf4, sizeof(buf4), "abc");
checkStr("safecpy len 3 to buf 4", buf4, "abc");

if (errCatchStart(errCatch))
    {
    safecpy(buf4, sizeof(buf4), "abcde");
    shouldHaveFailed("safecpy of len 5 to buf of 4");
    }
errCatchEnd(errCatch);
if (errCatchStart(errCatch))
    {
    /* no room for EOLN */
    safecpy(buf4, sizeof(buf4), "abcd");
    shouldHaveFailed("safecpy of len 4 to buf of 4");
    }
errCatchEnd(errCatch);

errCatchFree(&errCatch);
}

static void testSafencpy()
/* test safencpy */
{
struct errCatch *errCatch = errCatchNew();
char buf4[4];

safencpy(buf4, sizeof(buf4), "abc", 2);
checkStr("safencpy len 2 to buf 4", buf4, "ab");

if (errCatchStart(errCatch))
    {
    safencpy(buf4, sizeof(buf4), "abcdefgh", 5);
    shouldHaveFailed("safencpy of len 5 to buf of 4");
    }
errCatchEnd(errCatch);
if (errCatchStart(errCatch))
    {
    /* no room for EOLN */
    safencpy(buf4, sizeof(buf4), "abcd", 4);
    shouldHaveFailed("safencpy of len 4 to buf of 4");
    }
errCatchEnd(errCatch);

if (errCatchStart(errCatch))
    {
    safencpy(buf4, sizeof(buf4), "ab", 6);
    shouldHaveFailed("safencpy of len 6 with short string to buf of 4");
    }
errCatchEnd(errCatch);

errCatchFree(&errCatch);
}

static void testSafecat()
/* test safecat */
{
struct errCatch *errCatch = errCatchNew();
char buf6[6];

buf6[0] = '\0';
safecat(buf6, sizeof(buf6), "ab");
safecat(buf6, sizeof(buf6), "cd");
checkStr("safecat len 4 to buf of 6", buf6, "abcd");

if (errCatchStart(errCatch))
    {
    buf6[0] = '\0';
    safecat(buf6, sizeof(buf6), "abcdef");
    shouldHaveFailed("safecat of len 6 to buf of 6");
    }
errCatchEnd(errCatch);

if (errCatchStart(errCatch))
    {
    buf6[0] = '\0';
    safecat(buf6, sizeof(buf6), "abc");
    safecat(buf6, sizeof(buf6), "def");
    shouldHaveFailed("safecat of len 3, then 3 more to buf of 6");
    }
errCatchEnd(errCatch);

errCatchFree(&errCatch);
}

static void testSafencat()
/* test safencat */
{
struct errCatch *errCatch = errCatchNew();
char buf6[6];

buf6[0] = '\0';
safencat(buf6, sizeof(buf6), "abxyz", 2);
safencat(buf6, sizeof(buf6), "cdAABCD", 2);
checkStr("safencat len 4 to buf of 6", buf6, "abcd");

if (errCatchStart(errCatch))
    {
    buf6[0] = '\0';
    safencat(buf6, sizeof(buf6), "abcd", 6);
    shouldHaveFailed("safencat of len 6 to buf of 6");
    }
errCatchEnd(errCatch);

if (errCatchStart(errCatch))
    {
    buf6[0] = '\0';
    safencat(buf6, sizeof(buf6), "abcAAA", 3);
    safencat(buf6, sizeof(buf6), "defBBB", 3);
    shouldHaveFailed("safencat of len 3, then 3 more to buf of 6");
    }
errCatchEnd(errCatch);

errCatchFree(&errCatch);
}

static void safeTester()
/* tests for safe* functions */
{
testSafecpy();
testSafencpy();
testSafecat();
testSafencat();
if (errCount > 0)
    errAbort("%d tests failed", errCount);
}

int main(int argc, char *argv[])
{
if (argc != 1)
    usage();
safeTester();
return 0;
}


