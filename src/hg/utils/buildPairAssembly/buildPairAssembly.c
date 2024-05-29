/* buildPairAssembly - From a single coverage chain build a pairwise assembly including all sequence from both query and target. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "chain.h"
#include "twoBit.h"
#include "dnaseq.h"
#include "psl.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "buildPairAssembly - From a single coverage chain build a pairwise assembly including all sequence from both query and target\n"
  "usage:\n"
  "   buildPairAssembly input.chain tSequenceName tSeq.2bit qSequenceName qSeq.2bit out.fa query.psl target.psl map.bed dup.bed mismatch.bed baseTarget.bed baseQuery.bed\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

struct linearBlock
{
struct linearBlock *next;
unsigned pStart, pEnd;
unsigned tStart, tEnd;
unsigned qStart, qEnd;
boolean isFlipped;
unsigned anchor;
};

int lbCmpQuery(const void *va, const void *vb)
/* Compare to sort based on target start. */
{
const struct linearBlock *a = *((struct linearBlock **)va);
const struct linearBlock *b = *((struct linearBlock **)vb);
return a->qStart - b->qStart;
}

int lbCmpTarget(const void *va, const void *vb)
/* Compare to sort based on target start. */
{
const struct linearBlock *a = *((struct linearBlock **)va);
const struct linearBlock *b = *((struct linearBlock **)vb);
return a->tStart - b->tStart;
}

void outOverlap(FILE *f, char *tSequenceName,char *qSequenceName, struct cBlock *first,  struct cBlock *second, int count)
{
printf("first %d %d second %d %d\n", first->tStart, first->tEnd, second->tStart, second->tEnd);
//if (first->tEnd >= second->tEnd)
    {
    second->score = 3;
    fprintf(f, "%s %d %d %s %d %d %d\n",qSequenceName, second->qStart, second->qEnd, tSequenceName, second->tStart, second->tEnd, count);
    }
//else
    //errAbort("oops");

/*
first->tEnd = second->tStart;

if (first->tStart == first->tEnd)
    first->score = 3;
first->qEnd  = second->qStart;
*/

}

void buildPairAssembly(char *inputChain, char *tSequenceName, char *t2bitName, char *qSequenceName, char *q2bitName, char *outFaName, char *outQueryPsl, char *outTargetPsl, char *outMapBed, char *outDupBed, char *outMissBed, char *outBaseTargetBed, char *outBaseQueryBed, char *outSynBlockBed)
/* buildPairAssembly - From a single coverage chain build a pairwise assembly including all sequence from both query and target. */
{
struct lineFile *lf = lineFileOpen(inputChain, TRUE);
struct chain *chain;
struct cBlock *blockList = NULL, *cBlock, *nextBlock;
struct twoBitFile *q2bit = twoBitOpen(q2bitName);
struct twoBitFile *t2bit = twoBitOpen(t2bitName);
FILE *outFa = mustOpen(outFaName, "w");
FILE *outMap = mustOpen(outMapBed, "w");
FILE *outDup = mustOpen(outDupBed, "w");
FILE *outMiss = mustOpen(outMissBed, "w");
FILE *outBaseTarget = mustOpen(outBaseTargetBed, "w");
FILE *outBaseQuery = mustOpen(outBaseQueryBed, "w");
FILE *outSynBlock = mustOpen(outSynBlockBed, "w"); 
fprintf(outFa, ">buildPair\n");

while ((chain = chainRead(lf)) != NULL)
    {
    if (!sameString(tSequenceName, chain->tName) || !sameString(qSequenceName, chain->qName))
        continue;

    for (cBlock = chain->blockList; cBlock; cBlock = nextBlock)
        {
        nextBlock = cBlock->next;
        if (chain->qStrand == '-')
            {
            unsigned qStart = cBlock->qStart;
            cBlock->qStart = chain->qSize - cBlock->qEnd;
            cBlock->qEnd = chain->qSize - qStart;
            cBlock->score = 1;
            }
        else
            cBlock->score = 0;

        slAddHead(&blockList, cBlock);
        }
    }

slSort(&blockList, cBlockCmpTarget);
unsigned prevBase = blockList->tEnd;
//unsigned prevBase = 0;
boolean firstBlock = TRUE;
struct cBlock *prevCBlock = NULL;
int count = 0;

for (cBlock = blockList->next; cBlock; cBlock = cBlock->next)
    {
    //if (cBlock->score == 2)
        //continue;
    if (!firstBlock)
        {
        if (cBlock->tStart < prevBase)
            {
            outOverlap(outDup, tSequenceName, qSequenceName, prevCBlock, cBlock, count++);
            //printf("tBlock overlap %d %d\n", cBlock->tStart, prevBase);
            //cBlock->score = 3;
            continue;
            }
        }
    else 
        firstBlock = FALSE;
    prevBase = cBlock->tEnd;
    prevCBlock = cBlock;
    }

slSort(&blockList, cBlockCmpQuery);
prevBase = blockList->qEnd;
struct linearBlock *linearBlock;
for (cBlock = blockList->next; cBlock;  cBlock = cBlock->next)
    {
    if (cBlock->score == 3)
        continue;
    if (cBlock->qStart < prevBase)
        {
        //printf("qBlock overlap %d %d\n", cBlock->qStart, prevBase);
        cBlock->score = 2;
        continue;
        }
    if (cBlock->qStart > prevBase)
        {
        AllocVar(linearBlock);
        linearBlock->qStart = prevBase;
        linearBlock->qEnd = cBlock->qStart;
        if (cBlock->score == 1)
            linearBlock->isFlipped = TRUE;
        cBlock->data = (void *)linearBlock;
        }
    prevBase = cBlock->qEnd;
    }

slSort(&blockList, cBlockCmpTarget);
prevBase = 0;
struct linearBlock *linearBlockList = NULL;
firstBlock = TRUE;
prevCBlock = NULL;

for (cBlock = blockList; cBlock;   cBlock = cBlock->next)
    {
    if (cBlock->score >= 2)
        continue;
    if (!firstBlock)
        {
        if (cBlock->tStart > prevBase)
            {
            AllocVar(linearBlock);
            linearBlock->tStart = prevBase;
            linearBlock->tEnd = cBlock->tStart;
            slAddHead(&linearBlockList, linearBlock);
            }
        }
    else 
        firstBlock = FALSE;

    if ((cBlock->data != NULL) && (cBlock->score == 0))
        {
        struct linearBlock *qInsert = (struct linearBlock *)cBlock->data;
        slAddHead(&linearBlockList, qInsert);
        }

    AllocVar(linearBlock);
    linearBlock->qStart = cBlock->qStart;
    linearBlock->qEnd = cBlock->qEnd;
    linearBlock->tStart = cBlock->tStart;
    linearBlock->tEnd = cBlock->tEnd;
    if (cBlock->score == 1)
        linearBlock->isFlipped = TRUE;
    slAddHead(&linearBlockList, linearBlock);

    if ((cBlock->data != NULL) && (cBlock->score == 1))
        {
        struct linearBlock *qInsert = (struct linearBlock *)cBlock->data;
        slAddHead(&linearBlockList, qInsert);
        }

    prevBase = cBlock->tEnd;
    prevCBlock = cBlock;
    }

slReverse(&linearBlockList);

#define BOTH_COLOR       "20,40,140"
#define INVERT_COLOR       "140,20,140"
#define REFONLY_COLOR       "125,125,250"
#define QUERYONLY_COLOR       "180,125,60"

unsigned startAddress = 0, size;
boolean firstBlockFlipped = linearBlockList->isFlipped;
char strand;
char *color;
count = 0;
for(linearBlock = linearBlockList; linearBlock; linearBlock = linearBlock->next)
    {
    if (linearBlock->qStart == linearBlock->qEnd)
        {
        size = linearBlock->tEnd - linearBlock->tStart;
        struct dnaSeq *dna = twoBitReadSeqFrag(t2bit, tSequenceName, linearBlock->tStart, linearBlock->tEnd);
        writeSeqWithBreaks(outFa, dna->dna, size, 50 );
        fprintf(outMap,"buildPair %d %d refOnly 0 + %d %d %s %d %d 0 0\n", startAddress, startAddress + size,  startAddress, startAddress + size, REFONLY_COLOR, linearBlock->tStart, linearBlock->tEnd);
        }
    else if (linearBlock->tStart == linearBlock->tEnd)
        {
        size = linearBlock->qEnd - linearBlock->qStart;
        struct dnaSeq *dna = twoBitReadSeqFrag(q2bit, qSequenceName, linearBlock->qStart, linearBlock->qEnd);
        if (linearBlock->isFlipped)
            reverseComplement(dna->dna, size);

        writeSeqWithBreaks(outFa, dna->dna, size, 50 );
        char strand = linearBlock->isFlipped ? '-' : '+';
        fprintf(outMap,"buildPair %d %d queryOnly 0 %c %d %d %s 0 0 %d %d\n", startAddress, startAddress + size, strand, startAddress, startAddress + size, QUERYONLY_COLOR, linearBlock->qStart, linearBlock->qEnd);
        }
    else
        {
        size = linearBlock->qEnd - linearBlock->qStart;
        struct dnaSeq *dna = twoBitReadSeqFrag(t2bit, tSequenceName, linearBlock->tStart, linearBlock->tEnd);
        writeSeqWithBreaks(outFa, dna->dna, size, 50 );
        struct dnaSeq *dna2 = twoBitReadSeqFrag(q2bit, qSequenceName, linearBlock->qStart, linearBlock->qEnd);
        if (linearBlock->isFlipped)
            reverseComplement(dna2->dna, size);
        int ii;
        for(ii = 0; ii < size; ii++)
            {
            if (toupper(dna->dna[ii]) != toupper(dna2->dna[ii]))
                fprintf(outMiss, "buildPair %d %d %c->%c\n", startAddress + ii, startAddress + ii + 1, toupper(dna->dna[ii]), toupper(dna2->dna[ii]));

            }
        if (linearBlock->isFlipped == firstBlockFlipped)
            {
            strand = '+';
            color = BOTH_COLOR;
            //fprintf(outMap,"buildPair %d %d both 0 %c %d %d %s %d %d %d %d\n", startAddress, startAddress + size, strand, startAddress, startAddress + size, color, linearBlock->tStart, linearBlock->tEnd, linearBlock->qStart, linearBlock->qEnd);
            }
        else
            {
            strand = '-';
            color = INVERT_COLOR;
            fprintf(outMap,"buildPair %d %d invert 0 %c %d %d %s %d %d %d %d\n", startAddress, startAddress + size, strand, startAddress, startAddress + size, color, linearBlock->tStart, linearBlock->tEnd, linearBlock->qStart, linearBlock->qEnd);
            }

        linearBlock->anchor = ++count;
        }
    linearBlock->pStart = startAddress;
    linearBlock->pEnd = startAddress + size;
    startAddress += size;
    }

//for(linearBlock = linearBlockList; linearBlock; linearBlock = linearBlock->next)
    //printf("%d %d\n", linearBlock->pStart, linearBlock->pEnd);

unsigned sumSize = startAddress;
unsigned tSize = twoBitSeqSize(t2bit, tSequenceName);
unsigned qSize = twoBitSeqSize(q2bit, qSequenceName);
printf("qSize %d\n", qSize);
FILE *outQuery = mustOpen(outQueryPsl, "w");
FILE *outTarget = mustOpen(outTargetPsl, "w");
startAddress = 0;
for(linearBlock = linearBlockList; linearBlock; linearBlock = linearBlock->next)
    {
    if (linearBlock->qStart == linearBlock->qEnd)
        {
        size = linearBlock->tEnd - linearBlock->tStart;
        struct psl *psl = pslNew(tSequenceName, tSize, linearBlock->tStart, linearBlock->tEnd,
            "buildPair", sumSize,  startAddress, startAddress + size,
            "+", 1, 0);
        psl->qStarts[0] = linearBlock->tStart;
        psl->tStarts[0] = startAddress;
        psl->blockSizes[0] = size;
        psl->blockCount = 1;
        pslTabOut(psl, outTarget);
        }
    else if (linearBlock->tStart == linearBlock->tEnd)
        {
        size = linearBlock->qEnd - linearBlock->qStart;
        struct psl *psl = pslNew(qSequenceName, qSize, linearBlock->qStart, linearBlock->qEnd,
            "buildPair", sumSize,  startAddress, startAddress + size,
            linearBlock->isFlipped ? "-" : "+", 1, 0);
        if (linearBlock->isFlipped)
            psl->qStarts[0] = qSize - linearBlock->qEnd;
        else
            psl->qStarts[0] = linearBlock->qStart;
        psl->tStarts[0] = startAddress;
        psl->blockSizes[0] = size;
        psl->blockCount = 1;
        pslTabOut(psl, outQuery);
        }
    else
        {
        size = linearBlock->qEnd - linearBlock->qStart;
        struct psl *psl = pslNew(tSequenceName, tSize, linearBlock->tStart, linearBlock->tEnd,
            "buildPair", sumSize,  startAddress, startAddress + size,
            "+", 1, 0);
        psl->qStarts[0] = linearBlock->tStart;
        psl->tStarts[0] = startAddress;
        psl->blockSizes[0] = size;
        psl->blockCount = 1;
        pslTabOut(psl, outTarget);

        {
        struct psl *psl = pslNew(qSequenceName, qSize, linearBlock->qStart, linearBlock->qEnd,
            "buildPair", sumSize,  startAddress, startAddress + size,
            linearBlock->isFlipped ? "-" : "+", 1, 0);
        if (linearBlock->isFlipped)
            psl->qStarts[0] = qSize - linearBlock->qEnd;
        else
            psl->qStarts[0] = linearBlock->qStart;
        psl->tStarts[0] = startAddress;
        psl->blockSizes[0] = size;
        psl->blockCount = 1;
        pslTabOut(psl, outQuery);
        }
        }
    startAddress += size;
    }

startAddress = 0;
unsigned currentAddress = 0;
unsigned totalSize = 0;
unsigned startT = 0;
unsigned endT = 0;
//unsigned size;
//slSort(&linearBlockList, lbCmpTarget);
for(linearBlock = linearBlockList; linearBlock; linearBlock = linearBlock->next)
    {
    if (linearBlock->tStart == linearBlock->tEnd)
        size = linearBlock->qEnd - linearBlock->qStart;
    else
        size = linearBlock->tEnd - linearBlock->tStart;

    if ((linearBlock->tStart == linearBlock->tEnd)  || ((endT != 0) && (endT != linearBlock->tStart)))
        {
        if (startT != 0)
            fprintf(outBaseTarget, "buildPair %d %d %d %d\n",startAddress, currentAddress, startT, endT);
        startT = endT = totalSize = 0;
        }
    //else
    if (linearBlock->tStart != linearBlock->tEnd)
        {
        if (startT == 0)
            {
            startAddress = currentAddress;
            startT = linearBlock->tStart;
            }
        endT = linearBlock->tEnd;
        }
//totalSize  += size;
    currentAddress += size;
    }

startAddress = 0;
currentAddress = 0;
totalSize = 0;
startT = 0;
endT = 0;
for(linearBlock = linearBlockList; linearBlock; linearBlock = linearBlock->next)
    {
    if (linearBlock->tStart == linearBlock->tEnd)
        size = linearBlock->qEnd - linearBlock->qStart;
    else
        size = linearBlock->tEnd - linearBlock->tStart;

    if ((linearBlock->qStart == linearBlock->qEnd)  || ((endT != 0) && (endT != linearBlock->qStart)))
        {
        if (startT != 0)
            fprintf(outBaseQuery, "buildPair %d %d %d %d\n",startAddress, currentAddress, startT, endT);
        startT = endT = totalSize = 0;
        }
    //else
    if (linearBlock->qStart != linearBlock->qEnd)
        {
        if (startT == 0)
            {
            startAddress = currentAddress;
            startT = linearBlock->qStart;
            }
        endT = linearBlock->qEnd;
        }
//totalSize  += size;
    currentAddress += size;
    }

slSort(&linearBlockList, lbCmpQuery);
int isLastFlipped = linearBlockList->isFlipped;
unsigned lastEnd = linearBlockList->qEnd;
unsigned lastAnchor;
startAddress = linearBlockList->pStart;
count = 0;
boolean first = TRUE;
for(linearBlock = linearBlockList; linearBlock; linearBlock = linearBlock->next)
    {
    if (linearBlock->anchor)
        {
        if (first)
            {
            first = FALSE;
            startAddress = linearBlock->pStart;
            isLastFlipped = linearBlock->isFlipped;
            lastEnd = linearBlock->pEnd;
            lastAnchor = linearBlock->anchor;
            }
        else if ((abs(linearBlock->anchor - lastAnchor) != 1  ) || (isLastFlipped != linearBlock->isFlipped))
            {
            int signedCount = ++count;
            if (isLastFlipped)
                signedCount *= -1;
            if (startAddress > lastEnd)
                fprintf(outSynBlock, "buildPair %d %d %d\n",lastEnd, startAddress, signedCount);
            else
                fprintf(outSynBlock, "buildPair %d %d %d\n",startAddress, lastEnd, signedCount);
            startAddress = linearBlock->pStart;
            }
        lastEnd = linearBlock->pEnd;
        lastAnchor = linearBlock->anchor;
        isLastFlipped = linearBlock->isFlipped;
        }
    }
int signedCount = ++count;
if (isLastFlipped)
    signedCount *= -1;
if (startAddress > lastEnd)
    fprintf(outSynBlock, "buildPair %d %d %d\n",lastEnd, startAddress, signedCount);
else
    fprintf(outSynBlock, "buildPair %d %d %d\n",startAddress, lastEnd, signedCount);

// validate
slSort(&linearBlockList, lbCmpTarget);
firstBlock = TRUE;
for(linearBlock = linearBlockList; linearBlock; linearBlock = linearBlock->next)
    {
    if (linearBlock->tStart == linearBlock->tEnd)
        continue;
    if (!firstBlock)
        {
        if (linearBlock->tStart != startAddress)
            errAbort("tStart %d %d", startAddress, linearBlock->tStart);
        }
    else 
        firstBlock = FALSE;
    startAddress = linearBlock->tEnd;
    }
firstBlock = TRUE;
slSort(&linearBlockList, lbCmpQuery);
for(linearBlock = linearBlockList; linearBlock; linearBlock = linearBlock->next)
    {
    if (linearBlock->qStart == linearBlock->qEnd)
        continue;
    if (!firstBlock)
        {
        if (linearBlock->qStart != startAddress)
            errAbort("qStart %d %d", startAddress, linearBlock->qStart);
        }
    else
        firstBlock = FALSE;
    startAddress = linearBlock->qEnd;
    }

}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 15)
    usage();
buildPairAssembly(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], argv[7], argv[8], argv[9], argv[10], argv[11], argv[12], argv[13], argv[14]);
return 0;
}
