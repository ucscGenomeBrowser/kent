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
/* Scan index-list and use it to create a hash of target dnaSeq's. */
{
struct hash *hash = newHash(0);
struct blatzIndex *index;
for (index = indexList; index != NULL; index = index->next)
    hashAdd(hash, index->target->name, index->target);
return hash;
}

void chainWriteAllAsPsl(struct chain *chainList, struct dnaSeq *query,
        struct hash *targetHash, FILE *f)
/* Write out chain list to file in psl format. */
{
struct chain *chain;
struct dnaSeq *rQuery = cloneDnaSeq(query);
reverseComplement(rQuery->dna, rQuery->size);
for (chain = chainList; chain != NULL; chain = chain->next)
    {
    struct dnaSeq *target = hashMustFindVal(targetHash, chain->tName);
    struct psl *psl = chainToFullPsl(chain, query, rQuery, target);
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
    struct dnaSeq *query, struct hash *targetHash, char *mafQ, char *mafT,
    FILE *f)
/* Write chainList to file in either axt or maf according to
 * format string. */
{
struct dnaSeq *target;
struct dnaSeq *rQuery = cloneDnaSeq(query);
struct chain *chain;
if (mafQ == NULL) mafQ = "";
if (mafT == NULL) mafT = "";
reverseComplement(rQuery->dna, rQuery->size);
for (chain = chainList; chain != NULL; chain = chain->next)
    {
    struct axt *axtList, *axt;
    struct dnaSeq *qSeq = (chain->qStrand == '-' ? rQuery : query);
    target = hashMustFindVal(targetHash, chain->tName);
    axtList = chainToAxt(chain, qSeq, 0, target, 0, 40, BIGNUM);
    for (axt = axtList; axt != NULL; axt = axt->next)
        {
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
    struct dnaSeq *seq = el->val;
    total += seq->size;
    }
slFreeList(&elList);
return total;
}

void chainWriteAllAsBlast(struct chain *chainList, char *blastType, 
        struct dnaSeq *query, struct hash *targetHash,  FILE *f)
/* Write chainList to file in a blat variant */
{
struct dnaSeq *target;
struct dnaSeq *rQuery = cloneDnaSeq(query);
long long dbSize = sumSeqSizes(targetHash);
struct chain *chain;
reverseComplement(rQuery->dna, rQuery->size);
for (chain = chainList; chain != NULL; chain = chain->next)
    {
    struct axt *axtList, *axt;
    struct dnaSeq *qSeq = (chain->qStrand == '-' ? rQuery : query);
    struct axtBundle *bun = NULL;
    target = hashMustFindVal(targetHash, chain->tName);
    AllocVar(bun);
    bun->qSize = qSeq->size;
    bun->tSize = target->size;
    bun->axtList = chainToAxt(chain, qSeq, 0, target, 0, 40, BIGNUM);
    axtBlastOut(bun, 0, FALSE, f, "database", targetHash->elCount, dbSize, blastType, "blatz");
    axtBundleFree(&bun);
    }
dnaSeqFree(&rQuery);
}

        
void blatzWriteChains(struct bzp *bzp, struct chain *chainList,
        struct dnaSeq *query, struct blatzIndex *targetIndexList, 
        FILE *f)
/* Output chainList to file in format specified by bzp->out. */
{
struct hash *targetHash = hashTargetsFromIndex(targetIndexList);
char *out = bzp->out;

if (sameString(out, "chain"))
    chainWriteAll(chainList, f);
else if (sameString(out, "psl"))
    chainWriteAllAsPsl(chainList, query, targetHash, f);
else if (sameString(out, "axt") || sameString(out, "maf"))
    chainWriteAllAsAxtOrMaf(chainList, out, query, targetHash, 
            bzp->mafQ, bzp->mafT, f);
else if (sameString(out, "wublast") || sameString(out, "blast") ||
         sameString(out, "blast8") || sameString(out, "blast9"))
   chainWriteAllAsBlast(chainList, out, query, targetHash, f);
else 
    errAbort("blatzWriteChains doesn't recognize format %s", bzp->out);
hashFree(&targetHash);
}

