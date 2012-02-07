/* patSpace - a homology finding algorithm that occurs mostly in 
 * pattern space (as opposed to offset space). */
/* Copyright 1999-2003 Jim Kent.  All rights reserved. */
#include "common.h"
#include "portable.h"
#include "dnaseq.h"
#include "ooc.h"
#include "patSpace.h"


#define blockSize (256)

#define BIGONE	/* define this for 32 bit version. */

#ifdef BIGONE

#define maxBlockCount (2*230*1024 - 1)
#define psBits bits32
/* psBits is the size of an index word.  If 16 bits
 * patSpace will use less memory, but be limited to
 * 16 meg or less genome size. */

#else /* BIGONE */

#define maxBlockCount (64*1024 - 1)
#define psBits bits16
/* psBits is the size of an index word.  If 16 bits
 * patSpace will use less memory, but be limited to
 * 16 meg or less genome size. */

#endif /* BIGONE */


#define maxPatCount (1024*16)    /* Maximum allowed count for one pattern. */

struct blockPos
/* Structure that stores where in a BAC we are - 
 * essentially an unpacked block index. */
    {
    bits16 bacIx;          /* Which bac. */
    bits16 seqIx;          /* Which subsequence in bac. */
    struct dnaSeq *seq; /* Pointer to subsequence. */
    int offset;         /* Start position of block within subsequence. */
    int size;           /* Size of block within subsequence. */
    };

struct patSpace
/* A pattern space - something that holds an index of all 10-mers in
 * genome. */
    {
    psBits **lists;                      /* A list for each 10-mer */
    psBits *listSizes;                   /* Size of list for each 10-mer */
    psBits *allocated;                   /* Storage space for all lists. */
    int    blocksUsed;			 /* Number of blocks used, <= maxBlockCount */
    int posBuf[maxBlockCount];           /* Histogram of hits to each block. */
    int hitBlocks[maxBlockCount];        /* Indexes of blocks with hits. */
    struct blockPos blockPos[maxBlockCount]; /* Relates block to sequence. */
    psBits maxPat;                       /* Max # of times pattern can occur
                                          * before it is ignored. */
    int minMatch;                        /* Minimum number of tile hits needed
                                          * to trigger a clump hit. */
    int maxGap;                          /* Max gap between tiles in a clump. */
    int seedSize;			 /* Size of alignment seed. */
    int seedSpaceSize;                    /* Number of possible seed values. */
    };

void freePatSpace(struct patSpace **pPatSpace)
/* Free up a pattern space. */
{
struct patSpace *ps = *pPatSpace;
if (ps != NULL)
    {
    freeMem(ps->allocated);
    freez(pPatSpace);
    }
}

void tooManyBlocks()
/* Complain about too many blocks and abort. */
{
errAbort("Too many blocks, can only handle %d\n", maxBlockCount);
}

static void fixBlockSize(struct blockPos *bp, int blockIx)
/* Update size field from rest of blockPos. */
{
struct dnaSeq *seq = bp->seq;
int size = seq->size - bp->offset;
if (size > blockSize)
	size = blockSize;
bp->size = size;
}

static struct patSpace *newPatSpace(int minMatch, int maxGap, int seedSize)
/* Return an empty pattern space. */
{
struct patSpace *ps;
int seedBitSize = seedSize*2;
int seedSpaceSize;

AllocVar(ps);
ps->seedSize = seedSize;
seedSpaceSize = ps->seedSpaceSize = (1<<seedBitSize);
ps->lists = needLargeZeroedMem(seedSpaceSize * sizeof(ps->lists[0]));
ps->listSizes = needLargeZeroedMem(seedSpaceSize * sizeof(ps->listSizes[0]));
ps->minMatch = minMatch;
ps->maxGap = maxGap;
return ps;
}

static void countPatSpace(struct patSpace *ps, struct dnaSeq *seq)
/* Add up number of times each pattern occurs in sequence into ps->listSizes. */
{
int size = seq->size;
int mask = ps->seedSpaceSize-1;
DNA *dna = seq->dna;
int i;
int bits = 0;
int bVal;
int ls;

for (i=0; i<ps->seedSize-1; ++i)
    {
    bVal = ntValNoN[(int)dna[i]];
    bits <<= 2;
    bits += bVal;
    }
for (i=ps->seedSize-1; i<size; ++i)
    {
    bVal = ntValNoN[(int)dna[i]];
    bits <<= 2;
    bits += bVal;
    bits &= mask;
    ls = ps->listSizes[bits];
    if (ls < maxPatCount)
        ps->listSizes[bits] = ls+1;
    }
}

static int allocPatSpaceLists(struct patSpace *ps)
/* Allocate pat space lists and set up list pointers. 
 * Returns size of all lists. */
{
int oneCount;
int count = 0;
int i;
psBits *listSizes = ps->listSizes;
psBits **lists = ps->lists;
psBits *allocated;
psBits maxPat = ps->maxPat;
int size;
int usedCount = 0, overusedCount = 0;
int seedSpaceSize = ps->seedSpaceSize;

for (i=0; i<seedSpaceSize; ++i)
    {
    /* If pattern is too much used it's no good to us, ignore. */
    if ((oneCount = listSizes[i]) < maxPat)
        {
        count += oneCount;
        usedCount += 1;
        }
    else
        {
        overusedCount += 1;
        }
    }
ps->allocated = allocated = needLargeMem(count*sizeof(allocated[0]));
for (i=0; i<seedSpaceSize; ++i)
    {
    if ((size = listSizes[i]) < maxPat)
        {
        lists[i] = allocated;
        allocated += size;
        }
    }
return count;
}

static int addToPatSpace(struct patSpace *ps, int bacIx, int seqIx, struct dnaSeq *seq,int startBlock)
/* Add contents of one sequence to pattern space. Returns end block. */
{
int size = seq->size;
int mask = ps->seedSpaceSize-1;
DNA *dna = seq->dna;
int i;
int bits = 0;
int bVal;
int blockMod = blockSize;
int curCount;
psBits maxPat = ps->maxPat;
psBits *listSizes = ps->listSizes;
psBits **lists = ps->lists;
struct blockPos *bp = &ps->blockPos[startBlock];

bp->bacIx = bacIx;
bp->seqIx = seqIx;
bp->seq = seq;
bp->offset = 0;
fixBlockSize(bp, startBlock);
++bp;
for (i=0; i<ps->seedSize-1; ++i)
    {
    bVal = ntValNoN[(int)dna[i]];
    bits <<= 2;
    bits += bVal;
    }
for (i=ps->seedSize-1; i<size; ++i)
    {
    bVal = ntValNoN[(int)dna[i]];
    bits <<= 2;
    bits += bVal;
    bits &= mask;
    if ((curCount = listSizes[bits]) < maxPat)
        {
        listSizes[bits] = curCount+1;
        lists[bits][curCount] = startBlock;
        }
    if (--blockMod == 0)
        {
        if (++startBlock >= maxBlockCount)
	    tooManyBlocks();
        blockMod = blockSize;
	bp->bacIx = bacIx;
	bp->seqIx = seqIx;
	bp->seq = seq;
	bp->offset = i - (ps->seedSize-1) + 1;
	fixBlockSize(bp, startBlock);
	++bp;
        }
    }
for (i=0; i<ps->seedSize-1; ++i)
    {
    if (--blockMod == 0)
        {
        if (++startBlock >= maxBlockCount)
	    tooManyBlocks();
        blockMod = blockSize;
        }
    }
if (blockMod != blockSize)
    ++startBlock;
return startBlock;
}

struct patSpace *makePatSpace(
    struct dnaSeq **seqArray,       /* Array of sequence lists. */
    int seqArrayCount,              /* Size of above array. */
    int seedSize,	            /* Alignment seed size - 10 or 11. Must match oocFileName */
    char *oocFileName,              /* File with tiles to filter out, may be NULL. */
    int minMatch,                   /* Minimum # of matching tiles.  4 is good. */
    int maxGap                      /* Maximum gap size - 32k is good for 
                                       cDNA (introns), 500 is good for DNA assembly. */
    )
/* Allocate a pattern space and fill from sequences.  (Each element of
   seqArray is a list of sequences. */
{
struct patSpace *ps = newPatSpace(minMatch, maxGap,seedSize);
int i;
int startIx = 0;
int total = 0;
struct dnaSeq *seq;
psBits maxPat;
psBits *listSizes;
int seedSpaceSize = ps->seedSpaceSize;

maxPat = ps->maxPat = maxPatCount;
for (i=0; i<seqArrayCount; ++i)
    {
    for (seq = seqArray[i]; seq != NULL; seq = seq->next)
        {
        total += seq->size;
        countPatSpace(ps, seq);
        }
    }

listSizes = ps->listSizes;

/* Scan through over-popular patterns and set their count to value 
 * where they won't be added to pat space. */
oocMaskCounts(oocFileName, listSizes, ps->seedSize, maxPat);

/* Get rid of simple repeats as well. */
oocMaskSimpleRepeats(listSizes, ps->seedSize, maxPat);


allocPatSpaceLists(ps);

/* Zero out pattern counts that aren't oversubscribed. */
for (i=0; i<ps->seedSpaceSize; ++i)
    {
    if (listSizes[i] < maxPat)
        listSizes[i] = 0;
    }
for (i=0; i<seqArrayCount; ++i)
    {
	int j;
    for (seq = seqArray[i], j=0; seq != NULL; seq = seq->next, ++j)
        {
        startIx = addToPatSpace(ps, i, j, seq, startIx);
        if (startIx >= maxBlockCount)
	    tooManyBlocks();
        }
    }
ps->blocksUsed = startIx;

/* Zero local over-popular patterns. */
for (i=0; i<seedSpaceSize; ++i)
    {
    if (listSizes[i] >= maxPat)
        listSizes[i] = 0;
    }

return ps;
}


void bfFind(struct patSpace *ps, int block, struct blockPos *ret)
/* Find dna sequence and position within sequence that corresponds to block. */
{
assert(block >= 0 && block < maxBlockCount);
*ret = ps->blockPos[block];
}


static struct patClump *newClump(struct patSpace *ps, struct blockPos *first, struct blockPos *last)
/* Make new clump . */
{
struct dnaSeq *seq = first->seq;
int seqIx = first->seqIx;
int bacIx = first->bacIx;
int start = first->offset;
int end = last->offset+last->size;
int extraAtEnds = blockSize/2;
struct patClump *cl;

start -= extraAtEnds;
if (start < 0)
    start = 0;
end += extraAtEnds;
if (end >seq->size)
    end = seq->size;
AllocVar(cl);
cl->bacIx = bacIx;
cl->seqIx = seqIx;
cl->seq = seq;
cl->start = start;
cl->size = end-start;
return cl;
}


static struct patClump *clumpHits(struct patSpace *ps,
    int hitBlocks[], int hitCount, int posBuf[], 
    DNA *dna, int dnaSize)
/* Clumps together hits and returns a list. */
{
/* Clump together hits. */
int block;
int i;
int maxGap = ps->maxGap;
struct blockPos first, cur, pre;
struct patClump *patClump = NULL, *cl;

bfFind(ps, hitBlocks[0], &first);
pre = first;
for (i=1; i<hitCount; ++i)
    {
    block = hitBlocks[i];
    bfFind(ps, block, &cur);
    if (cur.seq != pre.seq || cur.offset - (pre.offset + pre.size) > maxGap)
        {
        /* Write old clump and start new one. */
        cl = newClump(ps, &first, &pre);
        slAddHead(&patClump, cl);
        first = cur;
        }
    else
        {
        /* Extend old clump. */
        }
    pre = cur;
    }
/* Write hitOut last clump. */
cl = newClump(ps, &first, &pre);
slAddHead(&patClump, cl);
slReverse(&patClump);
return patClump;
}

struct patClump *patSpaceFindOne(struct patSpace *ps, DNA *dna, int dnaSize)
/* Find occurrences of DNA in patSpace. */
{
int lastStart = dnaSize - ps->seedSize;
int i,j;
int pat;
int hitBlockCount = 0;
int totalSigHits = 0;
DNA *tile = dna;
int blocksUsed = ps->blocksUsed;
int *posBuf = ps->posBuf;
int *hitBlocks = ps->hitBlocks;
int minMatch = ps->minMatch;

memset(ps->posBuf, 0, sizeof(ps->posBuf[0]) * blocksUsed);
for (i=0; i<=lastStart; i += ps->seedSize)
    {
    psBits *list;
    psBits count;

    pat = 0;
    for (j=0; j<ps->seedSize; ++j)
        {
        int bVal = ntValNoN[(int)tile[j]];
        pat <<= 2;
        pat += bVal;
        }
    list = ps->lists[pat];
    if ((count = ps->listSizes[pat]) > 0)
        {
        for (j=0; j<count; ++j)
            posBuf[list[j]] += 1;            
        }
    tile += ps->seedSize;
    }

/* Scan through array that records counts of hits at positions. */
for (i=0; i<blocksUsed-1; ++i)
    {
    /* Save significant hits in a more compact array */
    int a = posBuf[i], b = posBuf[i+1];
    int sum = a + b;
    if (sum >= minMatch)
        {
        if (a > 0)
            {
            if (hitBlockCount == 0 || hitBlocks[hitBlockCount-1] != i)
                {
                hitBlocks[hitBlockCount++] = i;
                totalSigHits += a;
                }
            }
        if (b > 0)
            {
            hitBlocks[hitBlockCount++] = i+1;
            totalSigHits += b;
            }
        }
    }

/* Output data with significant hits. */
if (hitBlockCount > 0 && totalSigHits*ps->seedSize*8 > dnaSize)
    {
    return clumpHits(ps, hitBlocks, hitBlockCount, posBuf, dna, 
        dnaSize);
    }        
else
    return NULL;
}




