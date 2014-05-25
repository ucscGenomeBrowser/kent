/* genePredSingleCover - create single-coverage genePred files */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "options.h"
#include "chromBins.h"
#include "binRange.h"
#include "genePred.h"
#include "genePredReader.h"
#include "geneScore.h"
#include "hash.h"
#include "localmem.h"
#include "linefile.h"
#include "verbose.h"


/* Command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"scores", OPTION_STRING},
    {NULL, 0}
};

static struct chromBins *genesLoad(char *gpFile)
/* load genePreds into bins */
{
struct genePredReader *gpr = genePredReaderFile(gpFile, NULL);
struct genePred *gp;
struct chromBins *genes = chromBinsNew(NULL);

while ((gp = genePredReaderNext(gpr)) != NULL)
    {
    if (gp->cdsStart < gp->cdsEnd)
        chromBinsAdd(genes, gp->chrom, gp->cdsStart, gp->cdsEnd, gp);
    else
        genePredFree(&gp); /* no CDS */
    }
genePredReaderFree(&gpr);
return genes;
}

static char *scoreKeysMk(char *name, char *chrom, int txStart)
/* create a score key for a gene. Note static return */
{
static char key[128];
safef(key, sizeof(key), "%s\t%s\t%d", name, chrom, txStart);
return key;
}

static struct hash *scoreTblLoad(char *scoreFile)
/* load file of scores */
{
struct hash *scoreTbl = hashNew(22);
struct lineFile *lf = lineFileOpen(scoreFile, TRUE);
char *row[GENESCORE_NUM_COLS];
struct geneScore score;
char *key;
float *scorePtr;

while (lineFileNextRowTab(lf, row, GENESCORE_NUM_COLS))
    {
    geneScoreStaticLoad(row, &score);
    scorePtr = lmAlloc(scoreTbl->lm, sizeof(float));
    *scorePtr = score.score;
    key = scoreKeysMk(score.name, score.chrom, score.txStart);
    hashAdd(scoreTbl, key, scorePtr);
    }

lineFileClose(&lf);
return scoreTbl;
}

static float scoreTblGet(struct hash *scoreTbl, struct genePred *gp)
/* get the score for a gene.  If scoreTbl is NULL, return 0.0 */
{
if (scoreTbl == NULL)
    return 0.0;
else
    {
    char *key = scoreKeysMk(gp->name, gp->chrom, gp->txStart);
    float *scorePtr = hashMustFindVal(scoreTbl, key);
    return *scorePtr;
    }
}

static boolean cdsOverlaps(struct genePred *gp1, struct genePred *gp2)
/* determine if there is any overlap in the CDS of two genes */
{
int iExon1, start1, end1, iExon2, start2, end2;

for (iExon1 = 0; iExon1 < gp1->exonCount; iExon1++)
    {
    if (genePredCdsExon(gp1, iExon1, &start1, &end1))
        {
        for (iExon2 = 0; iExon2 < gp2->exonCount; iExon2++)
            {
            if (genePredCdsExon(gp2, iExon2, &start2, &end2))
                if (positiveRangeIntersection(start1, end1, start2, end2))
                    return TRUE;
            }
        }
    }
return FALSE;
}

struct genePred *findOverGenes(struct binKeeper *chrGenes,
                               struct genePred *seedGp)
/* find all genes who's CDS overlaps the specified gene and remove them
 * from chrGenes.  seedGp must already have been removed from chrGenes. */
{
struct genePred *overCds = NULL;
struct binElement *overs = binKeeperFind(chrGenes, seedGp->cdsStart, seedGp->cdsEnd);
struct binElement *gpEl;

while ((gpEl = slPopHead(&overs)) != NULL)
    {
    struct genePred *gp = gpEl->val; 
    if (cdsOverlaps(seedGp, gp))
        {
        binKeeperRemove(chrGenes, gp->cdsStart, gp->cdsEnd, gp);
        slAddHead(&overCds, gp);
        }
    freez(&gpEl);
    }
return overCds;
}

struct genePred *getOverGenes(struct binKeeper *chrGenes, struct genePred *nextGp)
/* get list of genes who's CDS overlaps the specified gene.  The returned
 * list will included the supplied gene and all genes will be removed from
 * chrGenes */
{
struct genePred *overGenes = nextGp, *gp;
boolean anyFound = TRUE;
binKeeperRemove(chrGenes, nextGp->cdsStart, nextGp->cdsEnd, nextGp);

/* continue to grow the list until no more overlapping are found.
 * this requires rechecking the expanded list each pass */
do
    {
    anyFound = FALSE;
    for (gp = overGenes; gp != NULL; gp = gp->next)
        {
        struct genePred *moreOver = findOverGenes(chrGenes, gp);
        if (moreOver != NULL)
            {
            anyFound = TRUE;
            overGenes = slCat(overGenes, moreOver);
            }
        }
    }
while (anyFound);

return overGenes;
}

static struct genePred *selectOverGene(struct genePred *overGenes,
                                       struct hash *scoreTbl)
/* select one gene in a list of overlapping genes to keep. scoreTbl maybe
 * NULL */
{
struct genePred *bestGp = overGenes, *gp;
float bestScore = scoreTblGet(scoreTbl, bestGp);
int bestCdsSize = genePredCdsSize(bestGp);

for (gp = bestGp->next; gp != NULL; gp = gp->next)
    {
    int sc = scoreTblGet(scoreTbl, gp);
    int sz = genePredCdsSize(gp);
    if ((sc > bestScore) || ((sc == bestScore) && (sz > bestCdsSize)))
        {
        bestScore = sc;
        bestGp = gp;
        bestCdsSize = sz;
        }
    }
return bestGp;
}

static void chromSingleCover(struct binKeeper *chrGenes, struct hash *scoreTbl,
                             FILE *outFh)
/* get single coverage genes for a chromosome */
{
struct binElement *geneEl;

/* pick next remaining gene and then find all that overlap it */
while ((geneEl = binKeeperFindLowest(chrGenes, chrGenes->minPos, chrGenes->maxPos)) != NULL)
    {
    struct genePred *overGenes = getOverGenes(chrGenes, (struct genePred*)geneEl->val);
    struct genePred *selGene = selectOverGene(overGenes, scoreTbl);
    genePredTabOut(selGene, outFh);
    genePredFreeList(&overGenes);
    }
}

static void genePredSingleCover(char *inGpFile, char *outGpFile, char *scoreFile)
/* create single-coverage genePred files */
{
struct chromBins *genes = genesLoad(inGpFile);
FILE *outFh = mustOpen(outGpFile, "w");
struct hash *scoreTbl = (scoreFile != NULL) ? scoreTblLoad(scoreFile) : NULL;
struct slName *chrom, *chroms = chromBinsGetChroms(genes);

while ((chrom = slPopHead(&chroms)) != NULL)
    {
    chromSingleCover(chromBinsGet(genes, chrom->name, FALSE), scoreTbl, outFh);
    freez(&chrom);
    }

carefulClose(&outFh);
chromBinsFree(&genes);
}

static void usage(char *msg)
/* Explain usage and exit. */
{
errAbort("%s\n"
    "\n"
    "genePredSingleCover - create single-coverage genePred files\n"
    "\n"
    "genePredSingleCover [options] inGenePred outGenePred\n"
    "\n"
    "Create a genePred file that have single CDS coverage of the genome.\n"
    "UTR is allowed to overlap.  The default is to keep the gene with the\n"
    "largest numberr of CDS bases.\n"
    "\n"
    "Options:\n"
    "  -scores=file - read scores used in selecting genes from this file.\n"
    "   It consists of tab seperated lines of\n"
    "       name chrom txStart score\n"
    "   where score is a real or integer number. Higher scoring genes will\n"
    "   be choosen over lower scoring ones.  Equaly scoring genes are\n"
    "   choosen by number of CDS bases.  If this option is supplied, all\n"
    "   genes must be in the file\n"
    "\n", msg);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 3)
    usage("wrong # args");
genePredSingleCover(argv[1], argv[2], optionVal("scores", NULL));

return 0;
}
