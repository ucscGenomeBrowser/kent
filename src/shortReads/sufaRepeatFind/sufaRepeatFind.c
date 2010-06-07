/* sufaRepeatFind - Use a sufa index to find short sequence that repeat more than a given number 
 * of times.. */
/* This file is copyright 2008 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dnautil.h"
#include "options.h"
#include "sufa.h"

static char const rcsid[] = "$Id: sufaRepeatFind.c,v 1.4 2008/11/06 07:03:00 kent Exp $";

int readSize = 25;
int minCount = 5;


void usage()
/* Explain usage and exit. */
{
errAbort(
  "sufaRepeatFind - Use a sufa index to find short sequence that repeat more than a given number of times.\n"
  "usage:\n"
  "   sufaRepeatFind in.sufa out.srf\n"
  "options:\n"
  "   -readSize=N Size of basic read size(default %d)\n"
  "   -minCount=N Minimum count of repeats to report (default %d)\n"
  , readSize, minCount);
}

static struct optionSpec options[] = {
   {"readSize", OPTION_INT},
   {"minCount", OPTION_INT},
   {NULL, 0},
};

int countDnaSame(DNA *dna, int tileSize, bits32 *array, int arraySize)
/* Count up number of places in a row in array, where the referenced DNA is the
 * same up to tileSize bases. */
{
int i;
DNA *first = dna + array[0];
for (i=1; i<arraySize; ++i)
    {
    if (memcmp(first, dna+array[i], tileSize) != 0)
        break;
    }
return i;
}

void sufaRepeatFind(char *in, char *out)
/* sufaRepeatFind - Use a sufa index to find short sequence that repeat more than a given number of times.. */
{
struct sufa *sufa = sufaRead(in, FALSE);
FILE *f = mustOpen(out, "w");
DNA *dna = sufa->allDna;
bits32 *array = sufa->array;
int arraySize = sufa->header->arraySize;
int i, sameSize;
for (i=0; i<arraySize; i += sameSize)
    {
    sameSize = countDnaSame(dna, readSize, array+i, arraySize-i);
    if (sameSize >= minCount)
        {
	mustWrite(f, dna + array[i], readSize);
	fprintf(f, " %d\n", sameSize);
	}
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
readSize = optionInt("readSize", readSize);
minCount = optionInt("minCount", minCount);
sufaRepeatFind(argv[1], argv[2]);
return 0;
}
