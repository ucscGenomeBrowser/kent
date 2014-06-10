/* chainDist - determine distributions of query sequence to target sequnce in chains. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "chain.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "chainDist - determine distributions of query sequence to target sequnce in chains\n"
  "usage:\n"
  "   chainDist chain out\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct mappingCnts
/* counts by mapping from query to target */
{
    struct queryCnts *qCnts;  // cnts by query list
    struct hash *qCntsTbl;    // and map
};

static struct mappingCnts *mappingCntsNew()
/* constructor */
{
struct mappingCnts *mCnts;
AllocVar(mCnts);
mCnts->qCntsTbl = hashNew(0);
return mCnts;
}

struct queryCnts 
/* counts for a query */
{
    struct queryCnts *next;
    char *qName;
    int mappedCnt;            // total bases mapped to chrom
    struct targetCnts *tCnts; // per-target base counts
    struct hash *tCntsTbl;    // and map
};

static struct queryCnts *queryCntsNew(char *qName)
/* query counts */
{
struct queryCnts *qCnts;
AllocVar(qCnts);
qCnts->qName = cloneString(qName);
qCnts->tCntsTbl = hashNew(0);
return qCnts;
}

static int queryCntsCmp(const void *va, const void *vb)
/* compare queryCnts by name */
{
const struct queryCnts *a = *((struct queryCnts **)va);
const struct queryCnts *b = *((struct queryCnts **)vb);
return strcmp(a->qName, b->qName);
}

static struct queryCnts *queryCntsGet(struct mappingCnts *mCnts, char *qName)
/* query counts */
{
struct queryCnts *qCnts = hashFindVal(mCnts->qCntsTbl, qName);
if (qCnts == NULL)
    {
    qCnts = queryCntsNew(qName);
    hashAdd(mCnts->qCntsTbl, qName, qCnts);
    slAddHead(&mCnts->qCnts, qCnts);
    }
return qCnts;
}

struct targetCnts
/* counts for a query */
{
    struct targetCnts *next;
    char *tName;
    int mappedCnt;             // bases mapped to this target
    struct queryCnts *qCnts;   // link back to query
};

static struct targetCnts *targetCntsNew(char *tName, struct queryCnts *qCnts)
/* target counts */
{
struct targetCnts *tCnts;
AllocVar(tCnts);
tCnts->tName = cloneString(tName);
tCnts->qCnts = qCnts;
return tCnts;
}

static struct targetCnts *targetCntsGet(struct mappingCnts *mCnts, char *qName, char *tName)
/* target counts */
{
struct queryCnts *qCnts = queryCntsGet(mCnts, qName);
struct targetCnts *tCnts = hashFindVal(qCnts->tCntsTbl, tName);
if (tCnts == NULL)
    {
    tCnts = targetCntsNew(tName, qCnts);
    hashAdd(qCnts->tCntsTbl, tName, tCnts);
    slAddHead(&qCnts->tCnts, tCnts);
    }
return tCnts;
}

static void cntChain(struct mappingCnts *mCnts, struct chain *chain)
/* count a chain */
{
struct cBlock *cBlk;
struct targetCnts *tCnts = targetCntsGet(mCnts, chain->qName, chain->tName);
for (cBlk = chain->blockList; cBlk != NULL; cBlk = cBlk->next)
    {
    int cnt = (cBlk->tEnd - cBlk->tStart);
    tCnts->mappedCnt += cnt;
    tCnts->qCnts->mappedCnt += cnt;
    }
}

static int targetCntsCmp(const void *va, const void *vb)
/* compare targetCnts by reverse cnts */
{
const struct targetCnts *a = *((struct targetCnts **)va);
const struct targetCnts *b = *((struct targetCnts **)vb);
return b->mappedCnt - a->mappedCnt;
}

static struct mappingCnts *cntChains(char *chainFile)
/* count all chains */
{
struct mappingCnts *mCnts = mappingCntsNew();
struct lineFile *chainLf = lineFileOpen(chainFile, TRUE);
struct chain *chain;
while ((chain = chainRead(chainLf)) != NULL)
    cntChain(mCnts, chain);
lineFileClose(&chainLf);
return mCnts;
}

static void reportQueryDists(struct queryCnts *qCnts, FILE *outFh)
/* generate report for a query */
{
slSort(&qCnts->tCnts, targetCntsCmp);
struct targetCnts *tCnts;
for (tCnts = qCnts->tCnts; tCnts != NULL; tCnts = tCnts->next)
    fprintf(outFh, "%s\t%s\t%d\t%0.3f\n", qCnts->qName, tCnts->tName,
            tCnts->mappedCnt, ((float)tCnts->mappedCnt)/qCnts->mappedCnt);
}

static void reportChainDists(struct mappingCnts *mCnts, char *outFile)
/* generate report */
{
FILE *outFh = mustOpen(outFile, "w");
fputs("#qName\ttName\tmappedBases\tmappedFrac\n", outFh);
slSort(&mCnts->qCnts, queryCntsCmp);
struct queryCnts *qCnts;
for (qCnts = mCnts->qCnts; qCnts != NULL; qCnts = qCnts->next)
    reportQueryDists(qCnts, outFh);
carefulClose(&outFh);
}

static void chainDist(char *chainFile, char *outFile)
/* determine distributions of query sequence to target sequnce in chains. */
{
struct mappingCnts *mCnts = cntChains(chainFile);
reportChainDists(mCnts, outFile);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
chainDist(argv[1], argv[2]);
return 0;
}
