/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* maToFf - convert between mrnaAli and ffAli representation of
 * an aligment. */
#include "common.h"
#include "fuzzyFind.h"
#include "hgap.h"
#include "maDbRep.h"
#include "maToFf.h"

static int countFfBlocks(struct ffAli *ff)
/* How many blocks in ff? */
{
int count = 0;
while (ff != NULL)
    {
    ++count;
    ff = ff->right;
    }
return count;
}

struct mrnaAli *ffToMa(struct ffAli *ffLeft, 
	struct dnaSeq *mrnaSeq, char *mrnaAcc,
	struct dnaSeq *genoSeq, char *genoAcc, 
	boolean isRc, boolean isEst)
/* Convert ffAli structure to mrnaAli. */
{
HGID queryId;
char query[128];
struct hgBac *bac = hgGetBac(genoAcc);
HGID bacId = bac->id;
struct sqlConnection *conn = hgAllocConn();
struct sqlResult *sr;
char **row;
int readDir;
struct mrnaAli *ma;
int blockCount = countFfBlocks(ffLeft);
int intronOrientation;
int qOrientation = (isRc ? -1 : 1);
int qSize;
struct ffAli *ff, *farRight = ffRightmost(ffLeft);
unsigned *blockSizes;	/* Size of each block */
unsigned *qBlockStarts;	/* Start of each block in mRNA */
unsigned *tBlockBacs;	/* Bac each block starts in */
unsigned *tBlockStarts;	/* Position within bac of each block start */
unsigned short *startGoods = ma->startGoods; /* Distance of perfect alignment from left. */
unsigned short *endGoods = ma->endGoods; /* Distance of perfect alignment from right. */
int i;
DNA *mrnaDna = mrnaSeq->dna, *genoDna = genoSeq->dna;

/* Get a little more info about RNA from database. */
sprintf(query, 
  "select mrna.id,mrna.direction,seq.size from mrna,seq "
  "where mrna.acc='%s' and mrna.id = seq.id", 
  mrnaAcc);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    errAbort("Couldn't find mRNA %s in database", mrnaAcc);
queryId = sqlUnsigned(row[0]);
readDir = (row[1][0] == '3' ? -1 : 1);
qSize = sqlUnsigned(row[2]);
sqlFreeResult(&sr);
hgFreeConn(&conn);

intronOrientation = ffIntronOrientation(ffLeft);

AllocVar(ma);
ma->id = hgNextId();
if (intronOrientation)
    ma->readDir = intronOrientation * qOrientation;
else
    ma->readDir = readDir;
ma->orientation = qOrientation;
ma->hasIntrons = (intronOrientation != 0);
ma->isEst = isEst;
ma->score = ffScoreCdna(ffLeft);
strncpy(ma->qAcc, mrnaAcc, sizeof(ma->qAcc));
ma->qId = queryId;
ma->qTotalSize = qSize;
ma->qStart = ffLeft->nStart - mrnaDna;
ma->qEnd = farRight->nEnd - mrnaDna;
ma->tStartBac = bacId;
ma->tStartPos = ffLeft->hStart - genoDna;
ma->tEndBac = bacId;
ma->tEndPos = farRight->hEnd - genoDna;
ma->blockCount = blockCount;

ma->blockSizes = AllocArray(blockSizes, blockCount);
ma->qBlockStarts = AllocArray(qBlockStarts, blockCount);
ma->tBlockBacs = AllocArray(tBlockBacs, blockCount);
ma->tBlockStarts = AllocArray(tBlockStarts, blockCount);
ma->startGoods = AllocArray(startGoods, blockCount);
ma->endGoods = AllocArray(endGoods, blockCount);
for (ff=ffLeft,i=0; ff != NULL; ff=ff->right,++i)
    {
    blockSizes[i] = ff->nEnd - ff->nStart;
    qBlockStarts[i] = ff->nStart - mrnaDna;
    tBlockBacs[i] = bacId;
    tBlockStarts[i] = ff->hStart - genoDna;
    startGoods[i] = ff->startGood;
    endGoods[i] = ff->endGood;
    }
return ma;
}

struct ffAli *maToFf(struct mrnaAli *ma, DNA *needle, DNA *haystack)
/* Convert from database to internal representation of alignment. */
{
int blockCount = ma->blockCount;
unsigned *blockSizes = ma->blockSizes;	/* Size of each block */
unsigned *qBlockStarts = ma->qBlockStarts;   /* Start of each block in mRNA */
unsigned *tBlockStarts = ma->tBlockStarts; /* Position within bac of each block start */
unsigned short *startGoods = ma->startGoods; /* Distance of perfect alignment from left. */
unsigned short *endGoods = ma->endGoods; /* Distance of perfect alignment from right. */
int bSize;
int i;
struct ffAli *rightMost = NULL, *ff;

for (i = 0; i<blockCount; ++i)
    {
    AllocVar(ff);
    bSize = blockSizes[i];
    ff->nStart = needle+qBlockStarts[i];
    ff->nEnd = ff->nStart + bSize;
    ff->hStart = haystack + tBlockStarts[i];
    ff->hEnd = ff->hStart + bSize;
    ff->startGood = startGoods[i];
    ff->endGood = endGoods[i];
    ff->left = rightMost;
    rightMost = ff;
    }
ff = ffMakeRightLinks(rightMost);
ffCountGoodEnds(ff);
return ff;
}
