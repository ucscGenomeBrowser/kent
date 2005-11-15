/* freen - My Pet Freen. */
#include "common.h"
#include "memalloc.h"
#include "linefile.h"
#include "hash.h"
#include "dnaseq.h"
#include "fa.h"
#include "psl.h"

static char const rcsid[] = "$Id: freen.c,v 1.59 2005/11/01 22:43:11 kent Exp $";

void usage()
{
errAbort("freen - test some hair brained thing.\n"
         "usage:  freen now\n");
}

void freen(char *dnaName, char *proteinName, char *pslName)
/* Test some hair-brained thing. */
{
bioSeq *dna = faReadDna(dnaName);
bioSeq *protein = faReadAa(proteinName);
struct psl *psl = pslLoadAll(pslName);

pslShowAlignment(psl, TRUE, protein->name, protein, 0, protein->size,
	dna->name, dna, 0, dna->size, stdout);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 4)
   usage();
freen(argv[1], argv[2], argv[3]);
return 0;
}
