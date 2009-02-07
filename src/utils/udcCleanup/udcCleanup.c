/* udcCleanup - Clean up old unused files in udcCache.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "udc.h"

static char const rcsid[] = "$Id: udcCleanup.c,v 1.1 2009/02/07 17:42:08 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "udcCleanup - Clean up old unused files in udcCache.\n"
  "usage:\n"
  "   udcCleanup cacheDir maxUnusedDays\n"
  "example:\n"
  "   udcCleanup /tmp/udcCache 7.5"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void doCleanup(char *cacheDir, char *dayString)
/* doCleanup - Clean up old unused files in udcCache.. */
{
double maxDays = atof(dayString);
if (maxDays <= 0)
    errAbort("The maxUnusedDays needs to be a positive value");
bits64 cleanedBytes = udcCleanup(cacheDir, maxDays);
printf("Cleaned up ");
printLongWithCommas(stdout, cleanedBytes);
printf(" bytes from files unused for %s days\n", dayString);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
doCleanup(argv[1], argv[2]);
return 0;
}
