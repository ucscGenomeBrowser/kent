/* Output in a variaty of alignment formats. */
/* Copyright 2005 Jim Kent.  All rights reserved. */

#include "common.h"
#include "hash.h"
#include "axt.h"
#include "chainToPsl.h"
#include "chainToAxt.h"
#include "psl.h"
#include "maf.h"
#include "bzp.h"
#include "blatz.h"


static struct hash *hashTargetsFromIndex(struct blatzIndex *indexList)
/* Scan index-list and use it to create a hash indexed by target->name. */
{
struct hash *hash = newHash(0);
struct blatzIndex *index;
for (index = indexList; index != NULL; index = index->next)
    hashAdd(hash, index->target->name, index);
return hash;
}

static void pslOffset(struct psl *psl, int qStart, int qEnd, int qSize,
	int tStart, int tSize)
/* Add offsets to psl */
{
int i;
int qOffset;
if (psl->strand[0] == '-')
    qOffset = qSize - qEnd;
else
    qOffset = qStart;
psl->tStart += tStart;
psl->tEnd += tStart;
psl->tSize = tSize;
psl->qStart += qStart;
psl->qEnd += qStart;
psl->qSize = qSize;
for (i=0; i<psl->blockCount; ++i)
    {
    psl->qStarts[i] += qOffset;
    psl->tStarts[i] += tStart;
    }
}

static void axtOffset(struct axt *axt, int qStart, int qEnd, int qSize,
	int tStart, int tSize)
/* Add offsets to psl */
{
int i;
int qOffset;
if (axt->qStrand == '-')
    qOffset = qSize - qEnd;
else
    qOffset = qStart;
axt->tStart += tStart;
axt->tEnd += tStart;
axt->qStart += qOffset;
axt->qEnd += qOffset;
}



static void chainWriteAllAsPsl(struct chain *chainList, struct dnaSeq *query,
	int queryStart, int queryEnd, int querySize,
        struct hash *targetHash, FILE *f)
/* Write out chain list to file in psl format. */
{
struct chain *chain;
struct dnaSeq *rQuery = cloneDnaSeq(query);
reverseComplement(rQuery->dna, rQuery->size);
for (chain = chainList; chain != NULL; chain = chain->next)
    {
    struct blatzIndex *index = hashMustFindVal(targetHash, chain->tName);
    struct dnaSeq *target = index->target;
    struct psl *psl = chainToFullPsl(chain, query, rQuery, target);
    pslOffset(psl, queryStart, queryEnd, querySize, 
    	index->targetStart, index->targetParentSize);
#ifdef NOREP /* Work around bug in blat for comparison */
    psl->match += psl->repMatch;
    psl->repMatch = 0;
#endif
    pslTabOut(psl, f);
    pslFree(&psl);
    }
dnaSeqFree(&rQuery);
}

void chainWriteAllAsAxtOrMaf(struct chain *chainList, char *format,
    struct dnaSeq *query, int queryStart, int queryEnd, int querySize,
    struct hash *targetHash, char *mafQ, char *mafT,
    FILE *f)
/* Write chainList to file in either axt or maf according to
 * format string. */
{
struct dnaSeq *rQuery = cloneDnaSeq(query);
struct chain *chain;
if (mafQ == NULL) mafQ = "";
if (mafT == NULL) mafT = "";
reverseComplement(rQuery->dna, rQuery->size);
for (chain = chainList; chain != NULL; chain = chain->next)
    {
    struct axt *axtList, *axt;
    struct dnaSeq *qSeq = (chain->qStrand == '-' ? rQuery : query);
    struct blatzIndex *index = hashMustFindVal(targetHash, chain->tName);
    struct dnaSeq *target = index->target;
    axtList = chainToAxt(chain, qSeq, 0, target, 0, 40, BIGNUM);
    for (axt = axtList; axt != NULL; axt = axt->next)
        {
	axtOffset(axt, queryStart, queryEnd, querySize, 
	    index->targetStart, index->targetParentSize);
        if (sameString(format, "maf"))
            {
            struct mafAli *maf = mafFromAxt(axt, target->size, mafT, query->size, mafQ);
            mafWrite(f, maf);
            mafAliFree(&maf);
            }
        else
            axtWrite(axt, f);
        }
    axtFreeList(&axt);
    }
dnaSeqFree(&rQuery);
}

static long long sumSeqSizes(struct hash *seqHash)
/* Return total size of all sequence in hash. */
{
struct hashEl *el, *elList = hashElListHash(seqHash);
long long total = 0;
for (el = elList; el != NULL; el = el->next)
    {
    struct blatzIndex *index = el->val;
    total += index->target->size;
    }
slFreeList(&elList);
return total;
}

void chainWriteAllAsBlast(struct chain *chainList, char *blastType, 
        struct dnaSeq *query,  int queryStart, int queryEnd, int querySize,
	struct hash *targetHash,  FILE *f)
/* Write chainList to file in a blat variant */
{
struct dnaSeq *rQuery = cloneDnaSeq(query);
long long dbSize = sumSeqSizes(targetHash);
struct chain *chain;
reverseComplement(rQuery->dna, rQuery->size);
for (chain = chainList; chain != NULL; chain = chain->next)
    {
    struct axt *axtList, *axt;
    struct dnaSeq *qSeq = (chain->qStrand == '-' ? rQuery : query);
    struct axtBundle *bun = NULL;
    struct blatzIndex *index = hashMustFindVal(targetHash, chain->tName);
    struct dnaSeq *target = index->target;
    AllocVar(bun);
    bun->qSize = qSeq->size;
    bun->tSize = target->size;
    bun->axtList = chainToAxt(chain, qSeq, 0, target, 0, 40, BIGNUM);
    axtOffset(bun->axtList, queryStart, queryEnd, querySize, 
	index->targetStart, index->targetParentSize);
    /* set input parameter minIdentity to 0.0 */
    axtBlastOut(bun, 0, FALSE, f, "database", targetHash->elCount, dbSize, blastType, "blatz", 0.0);
    axtBundleFree(&bun);
    }
dnaSeqFree(&rQuery);
}

static void blatzOffsetChains(struct chain *chainList, 
	int qStart, int qEnd, int qSize,
	struct hash *targetHash)
/* Add targetHash[tName]->targetOffset to chainList. */
{
struct chain *chain;
for (chain = chainList; chain != NULL; chain = chain->next)
    {
    struct blatzIndex *index = hashMustFindVal(targetHash, chain->tName);
    struct cBlock *block;
    int tOffset = index->targetStart;
    int tSize = index->targetParentSize;
    int qOffset;
    if (chain->qStrand == '-')
        qOffset = qSize - qEnd;
    else
        qOffset = qStart;
    chain->qStart += qOffset;
    chain->qEnd += qOffset;
    chain->qSize = qSize;
    chain->tStart += tOffset;
    chain->tEnd += tOffset;
    chain->tSize = tSize;
    for (block = chain->blockList; block != NULL; block = block->next)
        {
	block->qStart += qOffset;
	block->qEnd += qOffset;
	block->tStart += tOffset;
	block->tEnd += tOffset;
	}
    }
}
        
void blatzWriteChains(struct bzp *bzp, struct chain **pChainList,
	struct dnaSeq *query, int queryStart, int queryEnd, int queryParentSize,
	struct blatzIndex *targetIndexList, FILE *f)
/* Output chainList to file in format specified by bzp->out. 
 * This will free chainList as well. */
{
struct hash *targetHash = hashTargetsFromIndex(targetIndexList);
struct chain *chainList = *pChainList;
char *out = bzp->out;

if (bzp->bestScoreOnly && chainList != NULL)
    chainFreeList(&chainList->next);
if (sameString(out, "chain"))
    {
    blatzOffsetChains(chainList, queryStart, queryEnd, queryParentSize, targetHash);
    chainWriteAll(chainList, f);
    }
else if (sameString(out, "psl"))
    {
    chainWriteAllAsPsl(chainList, query, queryStart, queryEnd, queryParentSize, targetHash, f);
    }
else if (sameString(out, "axt") || sameString(out, "maf"))
    chainWriteAllAsAxtOrMaf(chainList, out, query,  queryStart, queryEnd, queryParentSize, 
	    targetHash, bzp->mafQ, bzp->mafT, f);
else if (sameString(out, "wublast") || sameString(out, "blast") ||
         sameString(out, "blast8") || sameString(out, "blast9"))
   chainWriteAllAsBlast(chainList, out, query, queryStart, queryEnd, queryParentSize,
   	targetHash, f);
else 
    errAbort("blatzWriteChains doesn't recognize format %s", bzp->out);
chainFreeList(pChainList);
hashFree(&targetHash);
}

