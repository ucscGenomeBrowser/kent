/* axtLib - Convert chain to axt */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "chainBlock.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "chainNet.h"
#include "nib.h"
#include "axt.h"
#include "axtLib.h"
#include "axtInfo.h"
#include "chainToAxt.h"
#include "hdb.h"


struct axt *netFillToAxt(struct cnFill *fill, struct dnaSeq *tChrom , int tSize,
	struct hash *qChromHash, char *nibDir,
	struct chain *chain, boolean swap)
/* Convert subset of chain as defined by fill to axt. swap query and target if swap is true*/
{
struct dnaSeq *qSeq;
boolean isRev = (chain->qStrand == '-');
struct chain *subChain, *chainToFree;
int qOffset;
struct axt *axtList = NULL , *axt;
struct nibInfo *nib = hashFindVal(qChromHash, fill->qName);

/* Get query sequence fragment. */
    {
    if (nib == NULL)
        {
	char path[512];
	AllocVar(nib);
	safef(path, sizeof(path), "%s/%s.nib", nibDir, fill->qName);
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
if (subChain != NULL)
    {
    axtList = chainToAxt(subChain, qSeq, qOffset, tChrom, fill->tStart, 100, BIGNUM);
    if (swap)
        {
        for (axt = axtList ; axt != NULL ; axt = axt->next)
            axtSwap(axt, tSize, nib->size);
        }
    }
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
int size = end-start;
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
axt->qStrand = strand;
axt->symCount = size;
axt->score = 0;
seq = nibLoadPart(nibFile, start,size);
axt->tSym = cloneMem(seq->dna, size+1);
axt->qSym = cloneMem(gapPt, size+1);
return axt;
}

void axtFillGap(struct axt **aList, char *nibDir, char direction)
/* fill gaps between blocks with null axts with seq on t and q seq all gaps*/
/* direction = '+' ascending on + strand, - is descending on - strand */
{
struct axt *axt, *next, *axtGap, *tmp, *prevAxt = NULL;
int prevEnd = 0;
int prevStart = 0;
char nib[128];

for (axt = *aList; axt != NULL; axt = next)
    {
    if (((axt->tStart)-1) > prevEnd  && prevEnd > 0 && direction == '+')
        {
        assert(prevAxt != NULL);
        tmp = prevAxt->next;
        safef(nib, sizeof(nib), "%s/%s.nib",nibDir,axt->tName);
        axtGap = createAxtGap(nib,axt->tName,prevEnd,(axt->tStart),axt->qStrand);
        axtGap->next = tmp;
        prevAxt->next = axtGap;
        }
    if (((axt->tEnd)+1) < prevStart  && prevStart > 0 && direction == '-')
        {
        assert(prevAxt != NULL);
        tmp = prevAxt->next;
        safef(nib, sizeof(nib), "%s/%s.nib",nibDir,axt->tName);
        axtGap = createAxtGap(nib,axt->tName,(axt->tEnd),prevStart,axt->qStrand);
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
struct sqlConnection *conn = hAllocConn(fromDb);
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


