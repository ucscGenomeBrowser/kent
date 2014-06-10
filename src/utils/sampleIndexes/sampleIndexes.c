/* sampleIndexes - Generate N random indexes into a array of M where M>N and indexes aren't repeated. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "obscure.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "sampleIndexes - Generate N random indexes into a array of M where M>N and indexes aren't repeated\n"
  "usage:\n"
  "   sampleIndexes sampleSize totalSize output\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void sampleIndexes(char *sampleSizeString, char *totalSizeString, char *output)
/* sampleIndexes - Generate N random indexes into a array of M where M>N and indexes aren't repeated. */
{
FILE *f = mustOpen(output, "w");
int sampleSize = sqlUnsigned(sampleSizeString);
int totalSize = sqlUnsigned(totalSizeString);
if (sampleSize > totalSize)
    errAbort("Sample size greater thatn totalSize");
int *array;
AllocArray(array, totalSize);
int i;
for (i=0; i<totalSize; ++i)
   array[i] = i;
shuffleArrayOfInts(array, totalSize);
for (i=0; i<sampleSize; ++i)
    fprintf(f, "%d\n", array[i]);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
sampleIndexes(argv[1], argv[2], argv[3]);
return 0;
}
