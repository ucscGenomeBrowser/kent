/* faCmp - Compare two .fa files. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "fa.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "faCmp - Compare two .fa files\n"
  "usage:\n"
  "   faCmp a.fa b.fa\n");
}

void faCmp(char *aFile, char *bFile)
/* faCmp - Compare two .fa files. */
{
struct dnaSeq *aList = faReadAllDna(aFile);
struct dnaSeq *bList = faReadAllDna(bFile);
int aCount = slCount(aList);
int bCount = slCount(bList);
struct dnaSeq *a, *b;
DNA *aDna, *bDna;
int size, i;


if (aCount != bCount)
   errAbort("%d sequences in %s, %d in %s", aCount, aFile, bCount, bFile);
for (a = aList, b = bList; a != NULL && b != NULL; a = a->next, b = b->next)
    {
    if (a->size != b->size)
        errAbort("%s in %s has %d bases.  %s in %s has %d bases",
		a->name, aFile, a->size, b->name, bFile, b->size);
    aDna = a->dna;
    bDna = b->dna;
    size = a->size;
    for (i=0; i<size; ++i)
        {
	if (aDna[i] != bDna[i])
	    errAbort("%s in %s differs from %s at %s at base %d (%c != %c)",
	    	a->name, aFile, b->name, bFile, i, aDna[i], bDna[i]);
	}
    }
printf("%s and %s are the same\n", aFile, bFile);
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 3)
    usage();
faCmp(argv[1], argv[2]);
return 0;
}
