/* udcCleanup - Clean up old unused files in udcCache.. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "udc.h"


char *cacheDir = NULL;
boolean testOnly = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "udcCleanup - Clean up old unused files in udcCache.\n"
  "usage:\n"
  "   udcCleanup maxUnusedDays\n"
  "example:\n"
  "   udcCleanup 7.5\n"
  "options:\n"
  "   -cacheDir=dir use the indicated cache dir instead of the default (%s)\n"
  "   -test  - don't actually clean up, but do still figure cleanup bytes\n"
  , udcDefaultDir()
  );
}

static struct optionSpec options[] = {
   {"cacheDir", OPTION_STRING},
   {"test", OPTION_BOOLEAN},
   {NULL, 0},
};

void doCleanup(char *dayString)
/* doCleanup - Clean up old unused files in udcCache.. */
{
double maxDays = atof(dayString);
if (maxDays <= 0)
    errAbort("The maxUnusedDays needs to be a positive value");
bits64 cleanedBytes = udcCleanup(cacheDir, maxDays, testOnly);
printf("Cleaned up ");
printLongWithCommas(stdout, cleanedBytes);
printf(" bytes from files unused for %s days\n", dayString);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
cacheDir = optionVal("cacheDir", udcDefaultDir());
testOnly = optionExists("test");
doCleanup(argv[1]);
return 0;
}
