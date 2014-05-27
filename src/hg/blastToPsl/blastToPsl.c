/* blastToPsl - convert blast textual output to PSLs */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "options.h"
#include "psl.h"
#include "blastParse.h"
#include "pslBuild.h"


double eVal = -1; /* default Expect value signifying no filtering */
boolean pslxFmt = FALSE; /* output in pslx format */

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"scores", OPTION_STRING},
    {"eVal", OPTION_DOUBLE},
    {"pslx", OPTION_BOOLEAN},
    {NULL, 0}
};

static void usage()
/* Explain usage and exit. */
{
errAbort(
  "blastToPsl - Convert blast alignments to PSLs.\n"
  "\n"
  "usage:\n"
  "   blastToPsl [options] blastOutput psl\n"
  "\n"
  "Options:\n"
  "  -scores=file - Write score information to this file.  Format is:\n"
  "       strands qName qStart qEnd tName tStart tEnd bitscore eVal\n"
  "  -verbose=n - n >= 3 prints each line of file after parsing.\n"
  "               n >= 4 dumps the result of each query\n"
  "  -eVal=n n is e-value threshold to filter results. Format can be either\n"
  "          an integer, double or 1e-10. Default is no filter.\n"
  "  -pslx - create PSLX output (includes sequences for blocks)\n"
  "\n"
  "Output only results of last round from PSI BLAST\n"
  );
}

static void outputPsl(struct blastBlock *bb, unsigned flags, struct psl *psl,
                      FILE* pslFh, FILE* scoreFh)
/* output a psl and optional score */
{
pslTabOut(psl, pslFh);
if (scoreFh != NULL)
    pslBuildScoresWrite(scoreFh, psl, bb->bitScore, bb->eVal);
pslCheck("blastToPsl", stderr, psl);
}

static void processBlock(struct blastBlock *bb, unsigned flags,
                         FILE* pslFh, FILE* scoreFh)
/* process one gapped alignment block */
{
struct blastGappedAli* ba = bb->gappedAli;
struct blastQuery *bq = ba->query;
struct psl *psl = pslBuildFromHsp(firstWordInLine(bq->query), bq->queryBaseCount, bb->qStart, bb->qEnd, ((bb->qStrand > 0) ? '+' : '-'), bb->qSym,
                                  firstWordInLine(ba->targetName), ba->targetSize, bb->tStart, bb->tEnd, ((bb->tStrand > 0) ? '+' : '-'), bb->tSym,
                                  flags);
if (psl->blockCount > 0 && (bb->eVal <= eVal || eVal == -1))
    outputPsl(bb, flags, psl, pslFh, scoreFh);
pslFree(&psl);
}

void processQuery(struct blastQuery *bq, unsigned flags, FILE* pslFh, FILE* scoreFh)
/* process one query. Each gaped block becomes an psl. Chaining is left
 * to other programs.  Only output last round from PSI BLAST */
{
struct blastGappedAli* ba;
struct blastBlock *bb;
for (ba = bq->gapped; ba != NULL; ba = ba->next)
    {
    if (ba->psiRound == bq->psiRounds)
        {
        for (bb = ba->blocks; bb != NULL; bb = bb->next)
            processBlock(bb, flags, pslFh, scoreFh);
        }
    }
}

static void blastToPsl(char *blastFile, char *pslFile, char* scoreFile)
/* process one query in */
{
struct blastFile *bf = blastFileOpenVerify(blastFile);
struct blastQuery *bq;
FILE *pslFh = mustOpen(pslFile, "w");
FILE *scoreFh = NULL;
if (scoreFile != NULL)
    scoreFh = pslBuildScoresOpen(scoreFile, FALSE);
unsigned flags =  pslBuildGetBlastAlgo(bf->program) |  (pslxFmt ? bldPslx : 0);

while ((bq = blastFileNextQuery(bf)) != NULL)
    {
    processQuery(bq, flags, pslFh, scoreFh);
    blastQueryFree(&bq);
    }

blastFileFree(&bf);
carefulClose(&scoreFh);
carefulClose(&pslFh);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
eVal = optionDouble("eVal", eVal);
pslxFmt = optionExists("pslx");
if (argc != 3)
    usage();
blastToPsl(argv[1], argv[2], optionVal("scores", NULL));

return 0;
}
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

