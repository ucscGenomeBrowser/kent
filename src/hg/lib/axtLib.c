/* axtLib - Convert chain to axt */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "chainBlock.h"
#include "chainNet.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "nib.h"
#include "axt.h"
#include "axtLib.h"
#include "axtInfo.h"
#include "chainToAxt.h"
#include "hdb.h"


struct hash *chainReadAll(char *fileName)
/* Read chains into a hash keyed by id. */
{
char nameBuf[16];
struct hash *hash = hashNew(18);
struct chain *chain;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
int count = 0;
boolean qChain = FALSE;  /* Do chain from query side. */

while ((chain = chainRead(lf)) != NULL)
    {
    sprintf(nameBuf, "%x", chain->id);
    if (hashLookup(hash, nameBuf))
        errAbort("Duplicate chain %d ending line %d of %s", 
		chain->id, lf->lineIx, lf->fileName);
    if (qChain)
        chainSwap(chain);
    hashAdd(hash, nameBuf, chain);
    ++count;
    }
lineFileClose(&lf);
fprintf(stderr, "read %d chains in %s\n", count, fileName);
return hash;
}

struct chain *chainLookup(struct hash *hash, int id)
/* Find chain in hash. */
{
char nameBuf[16];
sprintf(nameBuf, "%x", id);
return hashMustFindVal(hash, nameBuf);
}

#ifdef DUPE
struct nibInfo
/* Info on a nib file. */
    {
    struct nibInfo *next;
    char *fileName;	/* Name of nib file. */
    int size;		/* Number of bases in nib. */
    FILE *f;		/* Open file. */
    };


struct axt *axtFromBlocks(
	struct chain *chain,
	struct boxIn *startB, struct boxIn *endB,
	struct dnaSeq *qSeq, int qOffset,
	struct dnaSeq *tSeq, int tOffset)
/* Convert a list of blocks (guaranteed not to have inserts in both
 * strands between them) to an axt. */
{
int symCount = 0;
int dq, dt, blockSize = 0, symIx = 0;
struct boxIn *b, *a = NULL;
struct axt *axt;
char *qSym, *tSym;

/* Make a pass through figuring out how big output will be. */
for (b = startB; b != endB; b = b->next)
    {
    if (a != NULL)
        {
	dq = b->qStart - a->qEnd;
	dt = b->tStart - a->tEnd;
	symCount += dq + dt;
	}
    blockSize = b->qEnd - b->qStart;
    symCount += blockSize;
    a = b;
    }

/* Allocate axt and fill in most fields. */
AllocVar(axt);
axt->qName = cloneString(chain->qName);
axt->qStart = startB->qStart;
axt->qEnd = a->qEnd;
axt->qStrand = chain->qStrand;
axt->tName = cloneString(chain->tName);
axt->tStart = startB->tStart;
axt->tEnd = a->tEnd;
axt->tStrand = '+';
axt->symCount = symCount;
axt->qSym = qSym = needLargeMem(symCount+1);
qSym[symCount] = 0;
axt->tSym = tSym = needLargeMem(symCount+1);
tSym[symCount] = 0;

/* Fill in symbols. */
a = NULL;
for (b = startB; b != endB; b = b->next)
    {
    if (a != NULL)
        {
	dq = b->qStart - a->qEnd;
	dt = b->tStart - a->tEnd;
	if (dq == 0)
	    {
	    memset(qSym+symIx, '-', dt);
	    memcpy(tSym+symIx, tSeq->dna + a->tEnd - tOffset, dt);
	    symIx += dt;
	    }
	else
	    {
	    assert(dt == 0);
	    memset(tSym+symIx, '-', dq);
	    memcpy(qSym+symIx, qSeq->dna + a->qEnd - qOffset, dq);
	    symIx += dq;
	    }
	}
    blockSize = b->qEnd - b->qStart;
    memcpy(qSym+symIx, qSeq->dna + b->qStart - qOffset, blockSize);
    memcpy(tSym+symIx, tSeq->dna + b->tStart - tOffset, blockSize);
    symIx += blockSize;
    a = b;
    }
assert(symIx == symCount);

/* Fill in score and return. */
axt->score = axtScoreDnaDefault(axt);
return axt;
}

struct axt *axtListFromChain(struct chain *chain, 
	struct dnaSeq *qSeq, int qOffset,
	struct dnaSeq *tSeq, int tOffset, int maxGap)
/* Convert a chain to a list of axt's. */
REPLACED BY chainToAxt in lib/chainToAxt.c
{
struct boxIn *startB = chain->blockList, *endB, *a = NULL, *b;
struct axt *axtList = NULL, *axt;

for (b = chain->blockList; b != NULL; b = b->next)
    {
    if (a != NULL)
        {
	int dq = b->qStart - a->qEnd;
	int dt = b->tStart - a->tEnd;
	if ((dq > 0 && dt > 0) || dt > maxGap || dq > maxGap)
	    {
	    axt = axtFromBlocks(chain, startB, b, qSeq, qOffset, tSeq, tOffset);
	    slAddHead(&axtList, axt);
	    startB = b;
	    }
	}
    a = b;
    }
axt = axtFromBlocks(chain, startB, NULL, qSeq, qOffset, tSeq, tOffset);
slAddHead(&axtList, axt);
slReverse(&axtList);
return axtList;
}
#endif /* DUPE */

void writeGaps(struct chain *chain, FILE *f)
/* Write gaps to simple two column file. */
{
struct boxIn *a, *b;
a = chain->blockList;
for (b = a->next; b != NULL; b = b->next)
    {
    fprintf(f, "%d\t%d\n", b->tStart - a->tEnd, b->qStart - a->qEnd);
    a = b;
    }
}

struct axt *convertFill(struct cnFill *fill, struct dnaSeq *tChrom,
	struct hash *qChromHash, char *nibDir,
	struct chain *chain, FILE *gapFile)
/* Convert subset of chain as defined by fill to axt. */
{
struct dnaSeq *qSeq;
boolean isRev = (chain->qStrand == '-');
struct chain *subChain, *chainToFree;
int qOffset;
struct axt *axtList ;

/* Get query sequence fragment. */
    {
    struct nibInfo *nib = hashFindVal(qChromHash, fill->qName);
    if (nib == NULL)
        {
	char path[512];
	AllocVar(nib);
	sprintf(path, "%s/%s.nib", nibDir, fill->qName);
	nib->fileName = cloneString(path);
	nibOpenVerify(path, &nib->f, &nib->size);
	hashAdd(qChromHash, fill->qName, nib);
	}
    qSeq = nibLoadPartMasked(NIB_MASK_MIXED, nib->fileName, 
    	fill->qStart, fill->qSize);
    if (isRev)
	{
        reverseComplement(qSeq->dna, qSeq->size);
	qOffset = nib->size - (fill->qStart + fill->qSize);
	}
    else
	qOffset = fill->qStart;
    }
chainSubsetOnT(chain, fill->tStart, fill->tStart + fill->tSize, 
	&subChain, &chainToFree);
if (subChain == NULL)
    subChain = chain;
axtList = chainToAxt(subChain, qSeq, qOffset, tChrom, fill->tStart, 100);
chainFree(&chainToFree);
freeDnaSeq(&qSeq);
return axtList;
}

void axtListReverse(struct axt **axtList, char *queryDb)
/* reverse complement an entire axtList */
{
struct axt *axt;
int tmp;

for (axt = *axtList; axt != NULL; axt = axt->next)
    {
    int qSize = 0;
        
    if (sameString(axt->qName , "gap"))
        qSize = axt->qEnd;
    else
        qSize = hdbChromSize(queryDb, axt->qName);

    reverseComplement(axt->qSym, axt->symCount);
    reverseComplement(axt->tSym, axt->symCount);
    tmp = qSize - axt->qStart;
    axt->qStart = qSize - axt->qEnd;
    axt->qEnd = tmp;
    }
slReverse(axtList);
}

struct axt *createAxtGap(char *nibFile, char *chrom, 	
			 int start, int end, char strand)
/* return an axt alignment with the query all deletes - null aligment */
{
struct axt *axt;
int size = end-start+1;
char *gapPt = needLargeMem(size+1);
char *p;
struct dnaSeq *seq = NULL;

for (p=gapPt;p<=gapPt+size;p++)
    *p = '-';
AllocVar(axt);
axt->tName = chrom;
axt->tStart = start;
axt->tEnd = end;
axt->tStrand = strand;
axt->qName = "gap";
axt->qStart = 1;
axt->qEnd = size;
axt->qStrand = ' ';
axt->symCount = size;
axt->score = 0;
seq = nibLoadPart(nibFile, start,size);
axt->tSym = cloneMem(seq->dna, size+1);
axt->qSym = cloneMem(gapPt, size+1);
return axt;
}

void axtFillGap(struct axt **aList, char *nib)
/* fill gaps between blocks with null axts with seq on t and q seq all gaps*/
{
struct axt *axt, *next, *axtGap, *tmp, *prevAxt = NULL;
int prevEnd = 0;
int prevStart = 0;

for (axt = *aList; axt != NULL; axt = next)
    {
    if (((axt->tStart)-1) > prevEnd  && prevEnd > 0)
        {
        assert(prevAxt != NULL);
        tmp = prevAxt->next;
        axtGap = createAxtGap(nib,axt->tName,prevEnd,(axt->tStart)-1,axt->qStrand);
        axtGap->next = tmp;
        prevAxt->next = axtGap;
        }
    prevEnd = axt->tEnd;
    prevStart = axt->tStart;
    prevAxt = axt;
    next = axt->next;
    }
}
char *getAxtFileName(char *chrom, char *toDb, char *alignment, char *fromDb)
/* return file name for a axt alignment */
{
char query[256];
struct sqlResult *sr;
struct sqlConnection *conn = hAllocConnDb(fromDb);
char **row;
struct axtInfo *ai = NULL;

if (alignment != NULL)
    snprintf(query, sizeof(query),
	     "select * from axtInfo where chrom = '%s' and species = '%s' and alignment = '%s'",
	     chrom, toDb, alignment);
else
    snprintf(query, sizeof(query),
	     "select * from axtInfo where chrom = '%s' and species = '%s'",
	     chrom, toDb);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    ai = axtInfoLoad(row );
    }
if (ai == NULL)
    {
    printf("\nNo alignments available for %s (database %s).\n\n",
	   hFreezeFromDb(toDb), toDb);
    axtInfoFree(&ai);
    return NULL;
    }
//axtInfoFree(&ai);
return cloneString(ai->fileName);
}


