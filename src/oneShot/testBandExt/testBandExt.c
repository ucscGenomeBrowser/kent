/* testBandExt - Test band extension. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"
#include "fa.h"
#include "axt.h"
#include "bandExt.h"

int maxInsert = 100;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "testBandExt - Test band extension\n"
  "usage:\n"
  "   testBandExt a.fa b.fa\n"
  "options:\n"
  "   -maxInsert=%d - Control maximum insert size allowed\n"
  "   -gapOpen=N - Set gap open penalty\n"
  "   -gapExtend=N - Set gap extension penalty\n"
  , maxInsert
  );
}

static struct optionSpec options[] = {
   {"maxInsert", OPTION_INT},
   {"gapOpen", OPTION_INT},
   {"gapExtend", OPTION_INT},
   {NULL, 0},
};

void testBandExt(char *aName, char *bName)
/* testBandExt - Test band extension. */
{
struct dnaSeq *aSeq = faReadAllMixed(aName);
struct dnaSeq *bSeq = faReadAllMixed(bName);
int symAlloc = aSeq->size + bSeq->size;
int symCount, i;;
char *aSym, *bSym;
struct axtScoreScheme *ss = axtScoreSchemeDefault();

ss->gapOpen = optionInt("gapOpen", ss->gapOpen);
ss->gapExtend = optionInt("gapExtend", ss->gapExtend);

AllocArray(aSym, symAlloc+1);
AllocArray(bSym, symAlloc+1);
printf("%s\n%s\n", aSeq->dna, bSeq->dna);
printf("------\n");
if (bandExt(FALSE, axtScoreSchemeDefault(), maxInsert,
	aSeq->dna, aSeq->size, bSeq->dna, bSeq->size, 1,
	symAlloc, &symCount, aSym, bSym, NULL, NULL))
    {
    aSym[symCount] = 0;
    bSym[symCount] = 0;
    printf("%s\n", aSym);
    for (i=0; i<symCount; ++i)
        {
	if (toupper(aSym[i]) == toupper(bSym[i]))
	    printf("|");
	else
	    printf(" ");
	}
    printf("\n");
    printf("%s\n\n", bSym);
    }
else
    printf("No alignment\n\n");
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
maxInsert = optionInt("maxInsert", maxInsert);
testBandExt(argv[1], argv[2]);
return 0;
}
