/* genoFind - Quickly find where DNA occurs in genome.. */
#include "common.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "nib.h"
#include "fa.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "genoFind - Quickly find where DNA occurs in genome.\n"
  "usage:\n"
  "   genoFind probe.fa file(s).nib\n");
}

struct blockPos
/* Structure that stores where in a BAC we are - 
 * essentially an unpacked block index. */
    {
    bits16 bacIx;          /* Which bac. */
    bits16 seqIx;          /* Which subsequence in bac. */
    int offset;         /* Start position of block within subsequence. */
    int size;           /* Size of block within subsequence. */
    };

struct seqSource
/* Where a block of sequence comes from. */
    {
    struct seqSource *next;
    int fileIx;		/* Index of file. */
    int recordIx;	/* Index of record within file. */
    bits32 start,end;	/* Position withing merged sequence. */
    };

struct patSpace
/* A pattern space - something that holds an index of all N-mers in
 * genome. */
    {
    bits32 **lists;                      /* A list for each N-mer */
    bits16 *listSizes;                    /* Size of list for each N-mer */
    bits32 *allocated;                   /* Storage space for all lists. */
    int maxPat;                          /* Max # of times pattern can occur
                                          * before it is ignored. */
    int minMatch;                        /* Minimum number of tile hits needed
                                          * to trigger a clump hit. */
    int maxGap;                          /* Max gap between tiles in a clump. */
    int tileSize;			 /* Size of each N-mer. */
    int tileSpaceSize;                   /* Number of N-mer values. */
    int tileMask;			 /* 1-s for each N-mer. */
    struct seqSource *sourceList;	 /* List of sequence sources. */
    };

void freePatSpace(struct patSpace **pPatSpace)
/* Free up a pattern space. */
{
struct patSpace *ps = *pPatSpace;
if (ps != NULL)
    {
    freeMem(ps->lists);
    freeMem(ps->listSizes);
    freeMem(ps->allocated);
    slFreeList(&ps->sourceList);
    freez(pPatSpace);
    }
}

static struct patSpace *newPatSpace(int minMatch, int maxGap, int tileSize, int maxPat)
/* Return an empty pattern space. */
{
struct patSpace *ps;
int seedBitSize = tileSize*2;
int tileSpaceSize;

if (tileSize < 8 || tileSize > 14)
    errAbort("Seed size must be between 8 and 14");
AllocVar(ps);
ps->tileSize = tileSize;
tileSpaceSize = ps->tileSpaceSize = (1<<seedBitSize);
ps->tileMask = tileSpaceSize-1;
ps->lists = needLargeZeroedMem(tileSpaceSize * sizeof(ps->lists[0]));
ps->listSizes = needLargeZeroedMem(tileSpaceSize * sizeof(ps->listSizes[0]));
ps->minMatch = minMatch;
ps->maxGap = maxGap;
ps->maxPat = maxPat;
return ps;
}

static void countTile(struct patSpace *ps, DNA *dna)
/* Add N-mer to count. */
{
bits32 tile = 0;
int i, n = ps->tileSize;
for (i=0; i<n; ++i)
    {
    tile <<= 2;
    tile += ntValNoN[dna[i]];
    }
if (ps->listSizes[tile] < ps->maxPat)
    ++ps->listSizes[tile];
}

static void countSeq(struct patSpace *ps, struct dnaSeq *seq)
/* Add all N-mers in seq. */
{
DNA *dna = seq->dna;
int tileSize = ps->tileSize;
int i, lastTile = seq->size - tileSize;

for (i=0; i<=lastTile; i += tileSize)
    {
    countTile(ps, dna);
    dna += tileSize;
    }
}

static void countTilesInNib(struct patSpace *ps, char *nibName)
/* Count all tiles in nib file. */
{
FILE *f;
int tileSize = ps->tileSize;
int bufSize = tileSize * 16*1024;
int nibSize, i;
int endBuf, basesInBuf;
struct dnaSeq *seq;

uglyf("Counting tiles in %s\n", nibName);
nibOpenVerify(nibName, &f, &nibSize);
for (i=0; i < nibSize; i = endBuf)
    {
    endBuf = i + bufSize;
    if (endBuf >= nibSize) endBuf = nibSize;
    basesInBuf = endBuf - i;
    seq = nibLdPart(nibName, f, nibSize, i, basesInBuf);
    countSeq(ps, seq);
    freeDnaSeq(&seq);
    }
fclose(f);
uglyf("Done counting\n");
}

static int allocPatSpaceLists(struct patSpace *ps)
/* Allocate pat space lists and set up list pointers. 
 * Returns size of all lists. */
{
int oneCount;
int count = 0;
int i;
bits16 *listSizes = ps->listSizes;
bits32 **lists = ps->lists;
bits32 *allocated;
int ignoreCount = 0;
bits16 maxPat = ps->maxPat;
int size;
int usedCount = 0, overusedCount = 0;
int tileSpaceSize = ps->tileSpaceSize;

for (i=0; i<tileSpaceSize; ++i)
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
for (i=0; i<tileSpaceSize; ++i)
    {
    if ((size = listSizes[i]) < maxPat)
        {
        lists[i] = allocated;
        allocated += size;
        }
    }
return count;
}

static void addTile(struct patSpace *ps, DNA *dna, int offset)
/* Add N-mer to count. */
{
bits32 tile = 0;
int i, n = ps->tileSize;
for (i=0; i<n; ++i)
    {
    tile <<= 2;
    tile += ntValNoN[dna[i]];
    }
if (ps->listSizes[tile] < ps->maxPat)
    ps->lists[tile][ps->listSizes[tile]++] = offset;
}

static void addSeq(struct patSpace *ps, struct dnaSeq *seq, bits32 offset)
/* Add all N-mers in seq. */
{
DNA *dna = seq->dna;
int tileSize = ps->tileSize;
int i, lastTile = seq->size - tileSize;

for (i=0; i<=lastTile; i += tileSize)
    {
    addTile(ps, dna, offset);
    offset += tileSize;
    dna += tileSize;
    }
}

static int addTilesInNib(struct patSpace *ps, char *nibName, bits32 offset)
/* Add all tiles in nib file.  Returns size of nib file. */
{
FILE *f;
int tileSize = ps->tileSize;
int bufSize = tileSize * 16*1024;
int nibSize, i;
int endBuf, basesInBuf;
struct dnaSeq *seq;

uglyf("Adding tiles in %s\n", nibName);
nibOpenVerify(nibName, &f, &nibSize);
for (i=0; i < nibSize; i = endBuf)
    {
    endBuf = i + bufSize;
    if (endBuf >= nibSize) endBuf = nibSize;
    basesInBuf = endBuf - i;
    seq = nibLdPart(nibName, f, nibSize, i, basesInBuf);
    addSeq(ps, seq, i + offset);
    freeDnaSeq(&seq);
    }
fclose(f);
uglyf("Done adding\n");
return nibSize;
}

void zeroOverused(struct patSpace *ps)
/* Zero out counts of overused tiles. */
{
bits16 *sizes = ps->listSizes;
int tileSpaceSize = ps->tileSpaceSize, i;
int maxPat = ps->maxPat;
int overCount = 0;

for (i=0; i<tileSpaceSize; ++i)
    {
    if (sizes[i] >= maxPat)
	{
        sizes[i] = 0;
	++overCount;
	}
    }
uglyf("Got %d overused\n", overCount);
}

void zeroNonOverused(struct patSpace *ps)
/* Zero out counts of non-overused tiles. */
{
bits16 *sizes = ps->listSizes;
int tileSpaceSize = ps->tileSpaceSize, i;
int maxPat = ps->maxPat;
int overCount = 0;

for (i=0; i<tileSpaceSize; ++i)
    {
    if (sizes[i] < maxPat)
	{
        sizes[i] = 0;
	++overCount;
	}
    }
}


struct patSpace *indexNibs(int nibCount, char *nibNames[],
	int minMatch, int maxGap, int tileSize, int maxPat)
/* Make index for all nib files. */
{
struct patSpace *ps = newPatSpace(minMatch, maxGap, tileSize, maxPat);
int i;
bits32 offset = 0, nibSize;
char *nibName;
struct seqSource *ss;

for (i=0; i<nibCount; ++i)
    countTilesInNib(ps, nibNames[i]);
allocPatSpaceLists(ps);
zeroNonOverused(ps);
for (i=0; i<nibCount; ++i)
    {
    nibName = nibNames[i];
    nibSize = addTilesInNib(ps, nibName, offset);
    AllocVar(ss);
    ss->fileIx = i;
    ss->start = offset;
    offset += nibSize;
    ss->end = offset;
    slAddHead(&ps->sourceList, ss);
    }
slReverse(&ps->sourceList);
zeroOverused(ps);
return ps;
}

struct gfHit
/* A genoFind hit. */
   {
   struct gfHit *next;
   bits32 qStart;		/* Where it hits in query. */
   bits32 tStart;		/* Where it hits in target. */
   bits32 diagonal;		/* tStart + qSize - qStart. */
   };

struct gfClump
/* A clump of hits. */
    {
    struct gfClump *next;	/* Next clump. */
    bits32 qStart, qEnd;	/* Position in query. */
    bits32 tStart, tEnd;	/* Position in target. */
    int hitCount;		/* Number of hits. */
    struct gfHit *hitList;	/* List of hits. */
    };

void dumpClump(struct gfClump *clump, FILE *f)
/* Print out info on clump */
{
struct gfHit *hit;
fprintf(f, "query %d-%d, target %d-%d, hits %d\n", 
	clump->qStart, clump->qEnd, clump->tStart, clump->tEnd, clump->hitCount);
#ifdef SOMETIMES
for (hit = clump->hitList; hit != NULL; hit = hit->next)
    fprintf(f, "   q %d, t %d, diag %d\n", hit->qStart, hit->tStart, hit->diagonal);
#endif
}

static int cmpQuerySize;

int gfHitCmpDiagonal(const void *va, const void *vb)
/* Compare to sort based on 'diagonal' offset. */
{
const struct gfHit *a = *((struct gfHit **)va);
const struct gfHit *b = *((struct gfHit **)vb);

if (a->diagonal > b->diagonal)
    return 1;
else if (a->diagonal == b->diagonal)
    return 0;
else
    return -1;
}

int gfClumpCmpHitCount(const void *va, const void *vb)
/* Compare to sort based on hit count. */
{
const struct gfClump *a = *((struct gfClump **)va);
const struct gfClump *b = *((struct gfClump **)vb);

return (b->hitCount - a->hitCount);
}

void findClumpBounds(struct gfClump *clump)
/* Figure out qStart/qEnd tStart/tEnd from hitList */
{
struct gfHit *hit;
int x;
hit = clump->hitList;
if (hit == NULL)
    return;
clump->qStart = clump->qEnd = hit->qStart;
clump->tStart = clump->tEnd = hit->tStart;
for (hit = hit->next; hit != NULL; hit = hit->next)
    {
    x = hit->qStart;
    if (x < clump->qStart) clump->qStart = x;
    if (x > clump->qEnd) clump->qEnd = x;
    x = hit->tStart;
    if (x < clump->tStart) clump->tStart = x;
    if (x > clump->tEnd) clump->tEnd = x;
    }
}

struct gfClump *clumpHits(struct patSpace *ps, struct gfHit *hitList)
/* Clump together hits according to parameters in ps. */
{
struct gfClump *clumpList = NULL, *clump = NULL;
int minMatch = ps->minMatch;
int maxGap = ps->maxGap;
struct gfHit *clumpStart = NULL, *hit, *nextHit, *lastHit = NULL;
int clumpSize = 0;
int tileSize = ps->tileSize;

for (hit = hitList; ; hit = nextHit)
    {
    /* See if time to finish old clump/start new one. */
    if (lastHit == NULL || hit == NULL || hit->diagonal - lastHit->diagonal > maxGap)
        {
	if (clumpSize >= minMatch)
	    {
	    clump->hitCount = clumpSize;
	    findClumpBounds(clump);
	    clump->qEnd += tileSize;
	    clump->tEnd += tileSize;
	    slAddHead(&clumpList, clump);
	    }
	else
	    {
	    if (clump != NULL)
		{
		slFreeList(&clump->hitList);
		freez(&clump);
		}
	    }
	if (hit == NULL)
	    break;
	AllocVar(clump);
	clumpSize = 0;
	}
    nextHit = hit->next;
    slAddHead(&clump->hitList, hit);
    clumpSize += 1;
    lastHit = hit;
    }
slSort(&clumpList, gfClumpCmpHitCount);
return clumpList;
}


struct gfClump *gfFindClumps(struct patSpace *ps, struct dnaSeq *seq)
/* Find clumps associated with one sequence. */
{
struct gfHit *hitList = NULL, *hit;
int size = seq->size;
int tileSizeMinusOne = ps->tileSize - 1;
int mask = ps->tileMask;
DNA *dna = seq->dna;
int i, j;
bits32 bits = 0;
bits32 bVal;
int listSize;
bits32 qStart, tStart, *tList;

for (i=0; i<tileSizeMinusOne; ++i)
    {
    bVal = ntValNoN[dna[i]];
    bits <<= 2;
    bits += bVal;
    }
for (i=tileSizeMinusOne; i<size; ++i)
    {
    bVal = ntValNoN[dna[i]];
    bits <<= 2;
    bits += bVal;
    bits &= mask;
    listSize = ps->listSizes[bits];
    qStart = i-tileSizeMinusOne;
    tList = ps->lists[bits];
    for (j=0; j<listSize; ++j)
        {
	AllocVar(hit);
	hit->qStart = qStart;
	hit->tStart = tList[j];
	hit->diagonal = hit->tStart + seq->size - qStart;
	slAddHead(&hitList, hit);
	}
    }
uglyf("Got %d hits\n", slCount(hitList));
cmpQuerySize = seq->size;
slSort(&hitList, gfHitCmpDiagonal);
uglyf("Sorted\n");

return clumpHits(ps, hitList);
}

void genoFind(char *probeName, int nibCount, char *nibFiles[])
/* genoFind - Quickly find where DNA occurs in genome.. */
{
struct patSpace *ps = indexNibs(nibCount, nibFiles, 3, 2, 10, 4*1024);
struct dnaSeq *seq = faReadDna(probeName);
struct gfClump *clumpList = gfFindClumps(ps, seq), *clump;

for (clump = clumpList; clump != NULL; clump = clump->next)
    dumpClump(clump, stdout);
uglyf("Done\n");
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc < 3)
    usage();
genoFind(argv[1], argc-2, argv+2);
return 0;
}
