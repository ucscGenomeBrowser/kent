/* faCmp - Compare two .fa files. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "fa.h"
#include "options.h"
#include "nib.h"

static char const rcsid[] = "$Id: faCmp.c,v 1.5 2007/03/13 12:21:00 baertsch Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "faCmp - Compare two .fa files\n"
  "usage:\n"
  "   faCmp [options] a.fa b.fa\n"
  "options:\n"
  "    -softMask - use the soft masking information during the compare\n"
  "                Differences will be noted if the masking is different.\n"
  "    -sortName - sort input files by name before comparing\n"
  "default:\n"
  "    no masking information is used during compare.  It is as if both\n"
  "    sequences were not masked.\n"
  );
}

int faSortByName(const void *va, const void *vb)    
/* used for slSorting dnaSeq */
{
const struct dnaSeq *a = *((struct dnaSeq **)va);
const struct dnaSeq *b = *((struct dnaSeq **)vb);
return(strcmp(a->name, b->name));
}

void faCmp(int options, char *aFile, char *bFile)
/* faCmp - Compare two .fa files. */
{
struct dnaSeq *aList = (struct dnaSeq *) NULL;
struct dnaSeq *bList = (struct dnaSeq *) NULL;
int aCount = 0;
int bCount = 0;
struct dnaSeq *a, *b;
DNA *aDna, *bDna;
int size, i;

if ( NIB_MASK_MIXED & options ) {
	aList = faReadAllMixed(aFile);
	bList = faReadAllMixed(bFile);
} else {
	aList = faReadAllDna(aFile);
	bList = faReadAllDna(bFile);
}
aCount = slCount(aList);
bCount = slCount(bList);
if ( optionExists("sortName") )
    {
    verbose(1, "sorting %s\n",aFile);
    slSort(&aList, faSortByName);
    verbose(1, "sorting %s\n",bFile);
    slSort(&bList, faSortByName);
    }

if (aCount != bCount)
   errAbort("%d sequences in %s, %d in %s", aCount, aFile, bCount, bFile);
for (a = aList, b = bList; a != NULL && b != NULL; a = a->next, b = b->next)
    {
    verbose(2, "comparing %s to %s\n",a->name, b->name);
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
int options = 0;
optionHash(&argc, argv);
if ( optionExists("softMask") )
    options |= NIB_MASK_MIXED;
if (argc != 3)
    usage();
faCmp(options, argv[1], argv[2]);
return 0;
}
