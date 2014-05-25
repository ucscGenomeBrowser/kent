/* errCatchTest - test error catching by putting a wrapper
 * around a file read. */

/* Copyright (C) 2004 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "errCatch.h"

void usage()
/* Print usage message */
{
errAbort(
   "errCatchTest - check error catching\n"
   "usage:\n"
   "   errCatchTest password\n"
   "This will abort (but be caught) unless password is 'secret'\n");
}

void flakyStuff(char *password)
/* Count lines in file. */
{
printf("Checking password %s\n", password);
if (!sameString(password, "secret"))
    {
    errAbort("%s is not secret password!", password);
    }
}

void test(char *password)
/* See if password works. */
{
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    flakyStuff(password);
    printf("access confirmed\n");
    }
else
    {
    printf("access denied\n");
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    printf("Caught error: %s", errCatch->message->string);
else
    printf("You know the secret password\n");
errCatchFree(&errCatch);
}

int main(int argc, char *argv[])
{
if (argc != 2)
    usage();
test(argv[1]);
return 0;
}


