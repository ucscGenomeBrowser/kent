/* txPslFilter - Do rna/rna filter.. */

/* Copyright (C) 2007 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "psl.h"

double nearBest = 0.001;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txPslFilter - Do rna/rna filter.\n"
  "usage:\n"
  "   txPslFilter in.psl out.psl\n"
  "options:\n"
  "   -nearBest=0.N - Set how close must be to best alignment pass.\n"
  "                   default %g. Set to 0 to only past best scoring.\n"
  , nearBest);
}

static struct optionSpec options[] = {
   {"nearBest", OPTION_DOUBLE},
   {NULL, 0},
};

int txScorePsl(struct psl *psl)
/* Do score appropriate for our application. */
{
return psl->match + psl->repMatch - psl->misMatch 
	- 2*(psl->qNumInsert + psl->tNumInsert)
	- (psl->qBaseInsert + psl->tBaseInsert)/2;
}

void outputBestOnly(struct psl *startPsl, struct psl *endPsl, FILE *f)
/* Output only best scoring.  There may be multiple best scoring though. */
{
struct psl *psl;
int bestScore = 0;
for (psl = startPsl; psl != endPsl; psl = psl->next)
    {
    int score = txScorePsl(psl);
    if (score > bestScore) bestScore = score;
    }
double threshold = bestScore * (1.0 - nearBest);
for (psl = startPsl; psl != endPsl; psl = psl->next)
    if (txScorePsl(psl) >= threshold)
	pslTabOut(psl, f);
}

void txPslFilter(char *inName, char *outName)
/* txPslFilter - Do rna/rna filter.. */
{
struct psl *pslList = pslLoadAll(inName);
FILE *f = mustOpen(outName, "w");
slSort(&pslList, pslCmpQuery);
struct psl *startPsl, *endPsl;
for (startPsl = pslList; startPsl != NULL; startPsl = endPsl)
    {
    for (endPsl = startPsl->next; endPsl != NULL; endPsl = endPsl->next)
        {
	if (!sameString(startPsl->qName, endPsl->qName))
	    break;
	}
    outputBestOnly(startPsl, endPsl, f);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
nearBest = optionDouble("nearBest", nearBest);
txPslFilter(argv[1], argv[2]);
return 0;
}
