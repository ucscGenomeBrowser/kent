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
  "   buildPairAssembly input.chain tSequenceName tSeq.2bit qSequenceName qSeq.2bit out.fa query.psl target.psl map.bed dup.bed mismatch.bed\n"
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
unsigned tStart, tEnd;
unsigned qStart, qEnd;
boolean isFlipped;
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
if (first->tEnd >= second->tEnd)
    {
    second->score = 3;
    fprintf(f, "%s %d %d %s %d %d %d\n",qSequenceName, second->qStart, second->qEnd, tSequenceName, second->tStart, second->tEnd, count);
    }
else
    errAbort("oops");

/*
first->tEnd = second->tStart;

if (first->tStart == first->tEnd)
    first->score = 3;
first->qEnd  = second->qStart;
*/

}

void buildPairAssembly(char *inputChain, char *tSequenceName, char *t2bitName, char *qSequenceName, char *q2bitName, char *outFaName, char *outQueryPsl, char *outTargetPsl, char *outMapBed, char *outDupBed, char *outMissBed)
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
//prevBase = blockList->tEnd;
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

        if (cBlock->data != NULL)
            {
            struct linearBlock *qInsert = (struct linearBlock *)cBlock->data;
            slAddHead(&linearBlockList, qInsert);
            }
        }
    else 
        firstBlock = FALSE;

    AllocVar(linearBlock);
    linearBlock->qStart = cBlock->qStart;
    linearBlock->qEnd = cBlock->qEnd;
    linearBlock->tStart = cBlock->tStart;
    linearBlock->tEnd = cBlock->tEnd;
    if (cBlock->score == 1)
        linearBlock->isFlipped = TRUE;
    slAddHead(&linearBlockList, linearBlock);

    prevBase = cBlock->tEnd;
    prevCBlock = cBlock;
    }

slReverse(&linearBlockList);

#define BOTH_COLOR       "20,40,140"
#define INVERT_COLOR       "140,20,140"
#define REFONLY_COLOR       "255,255,0"
#define QUERYONLY_COLOR       "170,255,60"

unsigned startAddress = 0, size;
boolean firstBlockFlipped = linearBlockList->isFlipped;
char strand;
char *color;
for(linearBlock = linearBlockList; linearBlock; linearBlock = linearBlock->next)
    {
 //   printf("start %d %d\n", linearBlock->tStart, linearBlock->tEnd);
    if (linearBlock->qStart == linearBlock->qEnd)
        {
        struct dnaSeq *dna = twoBitReadSeqFrag(t2bit, tSequenceName, linearBlock->tStart, linearBlock->tEnd);
        fprintf(outFa, "%s\n", (char *)dna->dna);
        size = linearBlock->tEnd - linearBlock->tStart;
        //printf("chr1 %d %d refOnly 0 + %d %d 255,255,0\n", startAddress, startAddress + size, linearBlock->isFlipped, linearBlock->tStart, linearBlock->tEnd);
        //fprintf(outMap,"buildPair %d %d refOnly 0 + %d %d 255,255,0\n", startAddress, startAddress + size, startAddress, startAddress + size);
        fprintf(outMap,"buildPair %d %d refOnly 0 + %d %d %s %d %d 0 0\n", startAddress, startAddress + size,  startAddress, startAddress + size, REFONLY_COLOR, linearBlock->tStart, linearBlock->tEnd);
        }
    else if (linearBlock->tStart == linearBlock->tEnd)
        {
        struct dnaSeq *dna = twoBitReadSeqFrag(q2bit, qSequenceName, linearBlock->qStart, linearBlock->qEnd);
        fprintf(outFa, "%s\n", (char *)dna->dna);
        size = linearBlock->qEnd - linearBlock->qStart;
        char strand = linearBlock->isFlipped ? '-' : '+';
        //printf("chr1 %d %d %d queryOnly %d %d\n", startAddress, startAddress + size, linearBlock->isFlipped, linearBlock->qStart, linearBlock->qEnd);
        //fprintf(outMap,"buildPair %d %d queryOnly 0 %c %d %d 170,255,60\n", startAddress, startAddress + size, strand, startAddress, startAddress + size);
        fprintf(outMap,"buildPair %d %d queryOnly 0 %c %d %d %s 0 0 %d %d\n", startAddress, startAddress + size, strand, startAddress, startAddress + size, QUERYONLY_COLOR, linearBlock->qStart, linearBlock->qEnd);
        }
    else
        {
        struct dnaSeq *dna = twoBitReadSeqFrag(t2bit, tSequenceName, linearBlock->tStart, linearBlock->tEnd);
        fprintf(outFa, "%s\n", (char *)dna->dna);
        struct dnaSeq *dna2 = twoBitReadSeqFrag(q2bit, qSequenceName, linearBlock->qStart, linearBlock->qEnd);
        size = linearBlock->qEnd - linearBlock->qStart;
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
            }
        else
            {
            strand = '-';
            color = INVERT_COLOR;
            }
        //char strand = linearBlock->isFlipped ? '-' : '+';
        //printf("chr1 %d %d both 0 %c  %d %d %d %d\n", startAddress, startAddress + size, linearBlock->isFlipped,linearBlock->tStart, linearBlock->tEnd, linearBlock->qStart, linearBlock->qEnd);
        //fprintf(outMap,"buildPair %d %d both 0 %c  %d %d 20,40,140\n", startAddress, startAddress + size, strand,  startAddress, startAddress + size); 
        fprintf(outMap,"buildPair %d %d both 0 %c %d %d %s %d %d %d %d\n", startAddress, startAddress + size, strand, startAddress, startAddress + size, color, linearBlock->tStart, linearBlock->tEnd, linearBlock->qStart, linearBlock->qEnd);
        }
    startAddress += size;
    }

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
if (argc != 12)
    usage();
buildPairAssembly(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], argv[7], argv[8], argv[9], argv[10], argv[11]);
return 0;
}
