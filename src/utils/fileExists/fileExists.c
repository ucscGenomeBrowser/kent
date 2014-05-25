/* fileExists - exercise the fileExists function in src/lib/common.c. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "fileExists - exercise the fileExists function in src/lib/common.c\n"
  "usage:\n"
  "   fileExists fileName\n"
  "options:\n"
  "   -testFile=fileName\n\n"
  "Verify fileName exists with kent lib function: fileExists(fileName)"
  );
}

static struct optionSpec options[] = {
   {"testFile", OPTION_STRING},
   {NULL, 0},
};

static void doFileExists(char *XXX)
/* fileExists - exercise the fileExists function in src/lib/common.c. */
{
char *testFile = optionVal("testFile", NULL);
if (testFile)
    {
    if (fileExists(testFile))
	printf(" TRUE - file exists: '%s'\n", testFile);
    else
	printf("FALSE - file does not exist: '%s'\n", testFile);
    }
else
    errAbort("ERROR: testFile argument specifies NULL ?");
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 1)
    usage();
if (optionExists("testFile"))
    doFileExists(argv[1]);
else
    {
    fprintf(stderr, "ERROR: missing required argument: -testFile=fileName\n");
    usage();
    }
return 0;
}
