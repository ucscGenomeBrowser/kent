/* freen - My Pet Freen. */
#include "common.h"
#include "memalloc.h"
#include "dystring.h"
#include "linefile.h"
#include "hash.h"
#include "jksql.h"
#include "psl.h"

static char const rcsid[] = "$Id: freen.c,v 1.75 2007/03/02 17:12:53 kent Exp $";

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen file \n");
}

int test_pslCalcMilliBad(struct psl *psl, boolean isMrna)
/* Calculate badness in parts per thousand. */
{
int sizeMul = pslIsProtein(psl) ? 3 : 1;
int qAliSize, tAliSize, aliSize;
int milliBad = 0;
int sizeDif;
int insertFactor;
int total;

qAliSize = sizeMul * (psl->qEnd - psl->qStart);
tAliSize = psl->tEnd - psl->tStart;
uglyf("qAliSize %d\n", qAliSize);
uglyf("tAliSize %d\n", tAliSize);
uglyf("sizeMul %d\n", sizeMul);
aliSize = min(qAliSize, tAliSize);
if (aliSize <= 0)
    return 0;
sizeDif = qAliSize - tAliSize;
uglyf("sizeDif %d\n", sizeDif);
if (sizeDif < 0)
    {
    if (isMrna)
	sizeDif = 0;
    else
	sizeDif = -sizeDif;
    }
insertFactor = psl->qNumInsert;
if (!isMrna)
    insertFactor += psl->tNumInsert;

total = (sizeMul * (psl->match + psl->repMatch + psl->misMatch));
uglyf("total = %d\n", total);
if (total != 0)
    milliBad = (1000 * (psl->misMatch*sizeMul + insertFactor + round(3*log(1+sizeDif)))) / total;
uglyf("3*log(1+sizeDif) = %f\n", 3*log(1+sizeDif));
return milliBad;
}


void freen(char *inFile)
/* Test some hair-brained thing. */
{
struct psl *psl = pslLoadAll(inFile);
pslTabOut(psl, stdout);
uglyf("isMrna %d\n", test_pslCalcMilliBad(psl, TRUE));
uglyf("!isMrna %d\n", test_pslCalcMilliBad(psl, FALSE));
}

int main(int argc, char *argv[])
/* Process command line. */
{
pushCarefulMemHandler(1000000*1024L);
if (argc != 2)
   usage();
freen(argv[1]);
return 0;
}
