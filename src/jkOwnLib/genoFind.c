/* genoFind - Quickly find where DNA occurs in genome.. */
/* Copyright 2001-2005 Jim Kent.  All rights reserved. */

#include "common.h"
#include <signal.h>
#include "obscure.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "nib.h"
#include "twoBit.h"
#include "fa.h"
#include "dystring.h"
#include "errabort.h"
#include "sig.h"
#include "ooc.h"
#include "genoFind.h"
#include "trans3.h"
#include "binRange.h"

static char const rcsid[] = "$Id: genoFind.c,v 1.24 2008/03/20 17:38:35 kent Exp $";

char *gfSignature()
/* Return signature that starts each command to gfServer. Helps defend 
 * server from confused clients. */
{
static char signature[] = "0ddf270562684f29";
return signature;
}

volatile boolean pipeBroke = FALSE;	/* Flag broken pipes here. */

static void gfPipeHandler(int sigNum)
/* Set broken pipe flag. */
{
pipeBroke = TRUE;
}

void gfCatchPipes()
/* Set up to catch broken pipe signals. */
{
signal(SIGPIPE, gfPipeHandler);
}

int gfReadMulti(int sd, void *vBuf, size_t size)
/* Read in until all is read or there is an error. */
{
char *buf = vBuf;
size_t totalRead = 0;
int oneRead;

while (totalRead < size)
    {
    oneRead = read(sd, buf + totalRead, size - totalRead);
    if (oneRead < 0)
        {
	perror("Couldn't finish large read");
	return oneRead;
	}
    else if (oneRead == 0)
    /* Avoid an infinite loop when the client closed the socket. */
        break;
    totalRead += oneRead;
    }
return totalRead;
}

void genoFindFree(struct genoFind **pGenoFind)
/* Free up a genoFind index. */
{
struct genoFind *gf = *pGenoFind;
int i;
struct gfSeqSource *sources;
if (gf != NULL)
    {
    freeMem(gf->lists);
    freeMem(gf->listSizes);
    freeMem(gf->allocated);
    if ((sources = gf->sources) != NULL)
	{
	for (i=0; i<gf->sourceCount; ++i)
	    bitFree(&sources[i].maskedBits);
	freeMem(sources);
	}
    freez(pGenoFind);
    }
}

int gfPowerOf20(int n)
/* Return a 20 to the n */
{
int res = 20;
while (--n > 0)
    res *= 20;
return res;
}

void gfCheckTileSize(int tileSize, boolean isPep)
/* Check that tile size is legal.  Abort if not. */
{
if (isPep)
    {
    if (tileSize < 3 || tileSize > 8)
	errAbort("protein tileSize must be between 3 and 8");
    }
else
    {
    if (tileSize < 6 || tileSize > 18)
	errAbort("DNA tileSize must be between 6 and 18");
    }
}

static struct genoFind *gfNewEmpty(int minMatch, int maxGap, 
	int tileSize, int stepSize, int maxPat,
	char *oocFile, boolean isPep, boolean allowOneMismatch)
/* Return an empty pattern space. oocFile parameter may be NULL*/
{
struct genoFind *gf;
int tileSpaceSize;
int segSize = 0;

gfCheckTileSize(tileSize, isPep);
if (stepSize == 0)
    stepSize = tileSize;
AllocVar(gf);
if (isPep)
    {
    if (tileSize > 5)
        {
	segSize = tileSize - 5;
	}
    tileSpaceSize = gf->tileSpaceSize = gfPowerOf20(tileSize-segSize);
    }
else
    {
    int seedBitSize;
    if (tileSize > 12)
        {
	segSize = tileSize - 12;
	}
    seedBitSize = (tileSize-segSize)*2;
    tileSpaceSize = gf->tileSpaceSize = (1<<seedBitSize);
    gf->tileMask = tileSpaceSize-1;
    }
gf->segSize = segSize;
gf->tileSize = tileSize;
gf->stepSize = stepSize;
gf->isPep = isPep;
gf->allowOneMismatch = allowOneMismatch;
if (segSize > 0)
    {
    gf->endLists = needHugeZeroedMem(tileSpaceSize * sizeof(gf->endLists[0]));
    maxPat = BIGNUM;	/* Don't filter out overused on the big ones.  It is
                         * unnecessary and would be quite complex. */
    }
else
    {
    gf->lists = needHugeZeroedMem(tileSpaceSize * sizeof(gf->lists[0]));
    }
gf->listSizes = needHugeZeroedMem(tileSpaceSize * sizeof(gf->listSizes[0]));
gf->minMatch = minMatch;
gf->maxGap = maxGap;
gf->maxPat = maxPat;
if (oocFile != NULL)
    {
    if (segSize > 0)
        errAbort("Don't yet support ooc on large tile sizes");
    oocMaskCounts(oocFile, gf->listSizes, tileSize, maxPat);
    oocMaskSimpleRepeats(gf->listSizes, tileSize, maxPat);
    }
return gf;
}

static int ntLookup[256];

static void initNtLookup()
{
static boolean initted = FALSE;
if (!initted)
    {
    int i;
    for (i=0; i<ArraySize(ntLookup); ++i)
        ntLookup[i] = -1;
    ntLookup['a'] = A_BASE_VAL;
    ntLookup['c'] = C_BASE_VAL;
    ntLookup['t'] = T_BASE_VAL;
    ntLookup['g'] = G_BASE_VAL;
    initted = TRUE;
    }
}

int gfDnaTile(DNA *dna, int n)
/* Make a packed DNA n-mer. */
{
int tile = 0;
int i, c;
for (i=0; i<n; ++i)
    {
    tile <<= 2;
    if ((c = ntLookup[(int)dna[i]]) < 0)
        return -1;
    tile += c;
    }
return tile;
}

int gfPepTile(AA *pep, int n)
/* Make up packed representation of translated protein. */
{
int tile = 0;
int aa;
while (--n >= 0)
    {
    tile *= 20;
    aa = aaVal[(int)(*pep++)];
    if (aa < 0)
        return -1;
    tile += aa;
    }
return tile;
}

static void gfCountSeq(struct genoFind *gf, bioSeq *seq)
/* Add all N-mers in seq. */
{
char *poly = seq->dna;
int tileSize = gf->tileSize;
int stepSize = gf->stepSize;
int tileHeadSize = gf->tileSize - gf->segSize;
int maxPat = gf->maxPat;
int tile;
bits32 *listSizes = gf->listSizes;
int i, lastTile = seq->size - tileSize;
int (*makeTile)(char *poly, int n) = (gf->isPep ? gfPepTile : gfDnaTile);

initNtLookup();
for (i=0; i<=lastTile; i += stepSize)
    {
    if ((tile = makeTile(poly, tileHeadSize)) >= 0)
	{
        if (listSizes[tile] < maxPat)
	    {
	    listSizes[tile] += 1;
	    }
	}
    poly += stepSize;
    }
}

static int gfCountTilesInNib(struct genoFind *gf, int stepSize, char *fileName)
/* Count all tiles in nib file.  Returns nib size. */
{
FILE *f;
int tileSize = gf->tileSize;
int bufSize = tileSize * 16*1024;
int nibSize, i;
int endBuf, basesInBuf;
struct dnaSeq *seq;

printf("Counting tiles in %s\n", fileName);
nibOpenVerify(fileName, &f, &nibSize);
for (i=0; i < nibSize; i = endBuf)
    {
    endBuf = i + bufSize;
    if (endBuf >= nibSize) endBuf = nibSize;
    basesInBuf = endBuf - i;
    seq = nibLdPart(fileName, f, nibSize, i, basesInBuf);
    gfCountSeq(gf, seq);
    freeDnaSeq(&seq);
    }
fclose(f);
return nibSize;
}

long long maxTotalBases()
/* Return maximum bases we can index. */
{
long long maxBases = 1024*1024;
maxBases *= 4*1024;
return maxBases;
}

static long long twoBitCheckTotalSize(struct twoBitFile *tbf)
/* Return total size of sequence in two bit file.  Squawk and
 * die if it's more than 4 gig. */
{
long long totalSize = twoBitTotalSize(tbf);
if (totalSize > maxTotalBases())
    errAbort("Sorry, can only index up to %lld bases, %s has %lld",
	maxTotalBases(),  tbf->fileName, totalSize);
return totalSize;
}

static void gfCountTilesInTwoBit(struct genoFind *gf, int stepSize,
	char *fileName, int *retSeqCount, long long *retBaseCount)
/* Count all tiles in 2bit file.  Returns number of sequences and
 * total size of sequences in file. */
{
struct dnaSeq *seq;
struct twoBitFile *tbf = twoBitOpen(fileName);
struct twoBitIndex *index;
int seqCount = 0;
long long baseCount = twoBitCheckTotalSize(tbf);

printf("Counting tiles in %s\n", fileName);
for (index = tbf->indexList; index != NULL; index = index->next)
    {
    seq = twoBitReadSeqFragLower(tbf, index->name, 0, 0);
    gfCountSeq(gf, seq);
    ++seqCount;
    freeDnaSeq(&seq);
    }
twoBitClose(&tbf);
*retSeqCount = seqCount;
*retBaseCount = baseCount;
}

static int gfAllocLists(struct genoFind *gf)
/* Allocate index lists and set up list pointers. 
 * Returns size of all lists. */
{
int oneCount;
int count = 0;
int i;
bits32 *listSizes = gf->listSizes;
bits32 **lists = gf->lists;
bits32 *allocated = NULL;
bits32 maxPat = gf->maxPat;
int size;
int usedCount = 0, overusedCount = 0;
int tileSpaceSize = gf->tileSpaceSize;

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
if (count > 0)
    gf->allocated = allocated = needHugeMem(count*sizeof(allocated[0]));
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

static int gfAllocLargeLists(struct genoFind *gf)
/* Allocate large index lists and set up list pointers. 
 * Returns size of all lists. */
{
int count = 0;
int i;
bits32 *listSizes = gf->listSizes;
bits16 **endLists = gf->endLists;
bits16 *allocated = NULL;
int size;
int tileSpaceSize = gf->tileSpaceSize;

for (i=0; i<tileSpaceSize; ++i)
    count += listSizes[i];
if (count > 0)
    gf->allocated = allocated = needHugeMem(3*count*sizeof(allocated[0]));
for (i=0; i<tileSpaceSize; ++i)
    {
    size = listSizes[i];
    endLists[i] = allocated;
    allocated += 3*size;
    }
return count;
}


static void gfAddSeq(struct genoFind *gf, bioSeq *seq, bits32 offset)
/* Add all N-mers in seq.  Done after gfCountSeq. */
{
char *poly = seq->dna;
int tileSize = gf->tileSize;
int stepSize = gf->stepSize;
int i, lastTile = seq->size - tileSize;
int (*makeTile)(char *poly, int n) = (gf->isPep ? gfPepTile : gfDnaTile);
int maxPat = gf->maxPat;
int tile;
bits32 *listSizes = gf->listSizes;
bits32 **lists = gf->lists;

initNtLookup();
for (i=0; i<=lastTile; i += stepSize)
    {
    tile = makeTile(poly, tileSize);
    if (tile >= 0)
        {
	if (listSizes[tile] < maxPat)
	    {
	    lists[tile][listSizes[tile]++] = offset;
	    }
	}
    offset += stepSize;
    poly += stepSize;
    }
}

static void gfAddLargeSeq(struct genoFind *gf, bioSeq *seq, bits32 offset)
/* Add all N-mers to segmented index.  Done after gfCountSeq. */
{
char *poly = seq->dna;
int tileSize = gf->tileSize;
int stepSize = gf->stepSize;
int tileTailSize = gf->segSize;
int tileHeadSize = tileSize - tileTailSize;
int i, lastTile = seq->size - tileSize;
int (*makeTile)(char *poly, int n) = (gf->isPep ? gfPepTile : gfDnaTile);
int tileHead;
int tileTail;
bits32 *listSizes = gf->listSizes;
bits16 **endLists = gf->endLists;
bits16 *endList;
int headCount;

initNtLookup();
for (i=0; i<=lastTile; i += stepSize)
    {
    tileHead = makeTile(poly, tileHeadSize);
    tileTail = makeTile(poly + tileHeadSize, tileTailSize);
    if (tileHead >= 0 && tileTail >= 0)
        {
	endList = endLists[tileHead];
	headCount = listSizes[tileHead]++;
	endList += headCount * 3;		/* Because have three slots per. */
	endList[0] = tileTail;
	endList[1] = (offset >> 16);
	endList[2] = (offset&0xffff);
	}
    offset += stepSize;
    poly += stepSize;
    }
}



static int gfAddTilesInNib(struct genoFind *gf, char *fileName, bits32 offset,
	int stepSize)
/* Add all tiles in nib file.  Returns size of nib file. */
{
FILE *f;
int tileSize = gf->tileSize;
int bufSize = tileSize * 16*1024;
int nibSize, i;
int endBuf, basesInBuf;
struct dnaSeq *seq;

printf("Adding tiles in %s\n", fileName);
nibOpenVerify(fileName, &f, &nibSize);
for (i=0; i < nibSize; i = endBuf)
    {
    endBuf = i + bufSize;
    if (endBuf >= nibSize) endBuf = nibSize;
    basesInBuf = endBuf - i;
    seq = nibLdPart(fileName, f, nibSize, i, basesInBuf);
    gfAddSeq(gf, seq, i + offset);
    freeDnaSeq(&seq);
    }
fclose(f);
return nibSize;
}


static void gfZeroOverused(struct genoFind *gf)
/* Zero out counts of overused tiles. */
{
bits32 *sizes = gf->listSizes;
int tileSpaceSize = gf->tileSpaceSize, i;
int maxPat = gf->maxPat;
int overCount = 0;

for (i=0; i<tileSpaceSize; ++i)
    {
    if (sizes[i] >= maxPat)
	{
        sizes[i] = 0;
	++overCount;
	}
    }
}

static void gfZeroNonOverused(struct genoFind *gf)
/* Zero out counts of non-overused tiles. */
{
bits32 *sizes = gf->listSizes;
int tileSpaceSize = gf->tileSpaceSize, i;
int maxPat = gf->maxPat;
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

struct genoFind *gfIndexNibsAndTwoBits(int fileCount, char *fileNames[],
	int minMatch, int maxGap, int tileSize, int maxPat, char *oocFile,
	boolean allowOneMismatch, int stepSize)
/* Make index for all nibs and .2bits in list. 
 *      minMatch - minimum number of matching tiles to trigger alignments
 *      maxGap   - maximum deviation from diagonal of tiles
 *      tileSize - size of tile in nucleotides
 *      maxPat   - maximum use of tile to not be considered a repeat
 *      oocFile  - .ooc format file that lists repeat tiles.  May be NULL. 
 *      allowOneMismatch - allow one mismatch in a tile.  
 *      stepSize - space between tiles.  Zero means default (which is tileSize). */
{
struct genoFind *gf = gfNewEmpty(minMatch, maxGap, tileSize, stepSize,
	maxPat, oocFile, FALSE, allowOneMismatch);
int i;
bits32 offset = 0, nibSize;
char *fileName;
struct gfSeqSource *ss;
long long totalBases = 0, warnAt = maxTotalBases();
int totalSeq = 0;

if (allowOneMismatch)
    errAbort("Don't currently support allowOneMismatch in gfIndexNibsAndTwoBits");
if (stepSize == 0)
    stepSize = gf->tileSize;
for (i=0; i<fileCount; ++i)
    {
    fileName = fileNames[i];
    if (twoBitIsFile(fileName))
	{
	int seqCount;
	long long baseCount;
        gfCountTilesInTwoBit(gf, stepSize, fileName, &seqCount, &baseCount);
	totalBases += baseCount;
	totalSeq += seqCount;
	}
    else if (nibIsFile(fileName))
	{
	totalBases += gfCountTilesInNib(gf, stepSize, fileName);
	totalSeq += 1;
	}
    else
        errAbort("Unrecognized file type %s", fileName);
    /* Warn if they exceed 4 gig. */
    if (totalBases >= warnAt)
	errAbort("Exceeding 4 billion bases, sorry gfServer can't handle that.");
    }
gfAllocLists(gf);
gfZeroNonOverused(gf);
AllocArray(gf->sources, totalSeq);
gf->sourceCount = totalSeq;
ss = gf->sources;
for (i=0; i<fileCount; ++i)
    {
    fileName = fileNames[i];
    if (nibIsFile(fileName))
	{
	nibSize = gfAddTilesInNib(gf, fileName, offset, stepSize);
	ss->fileName = fileName;
	ss->start = offset;
	offset += nibSize;
	ss->end = offset;
	++ss;
	}
    else
        {
	struct twoBitFile *tbf = twoBitOpen(fileName);
	struct twoBitIndex *index;
	char nameBuf[PATH_LEN+256];
	for (index = tbf->indexList; index != NULL; index = index->next)
	    {
	    struct dnaSeq *seq = twoBitReadSeqFragLower(tbf, index->name, 0,0);
	    gfAddSeq(gf, seq, offset);
	    safef(nameBuf, sizeof(nameBuf), "%s:%s", fileName, index->name);
	    ss->fileName = cloneString(nameBuf);
	    ss->start = offset;
	    offset += seq->size;
	    ss->end = offset;
	    ++ss;
	    dnaSeqFree(&seq);
	    }
	twoBitClose(&tbf);
	}
    }
gf->totalSeqSize = offset;
gfZeroOverused(gf);
printf("Done adding\n");
return gf;
}

static void maskSimplePepRepeat(struct genoFind *gf)
/* Remove tiles from index that represent repeats
 * of period one and two. */
{
int i;
int tileSize = gf->tileSize;
int maxPat = gf->maxPat;
bits32 *listSizes = gf->listSizes;

for (i=0; i<20; ++i)
    {
    int j, k;
    for (j=0; j<20; ++j)
	{
	int tile = 0;
	for (k=0; k<tileSize; ++k)
	    {
	    tile *= 20;
	    if (k&1)
		tile += j;
	    else
	        tile += i;
	    }
	listSizes[tile] = maxPat;
	}
    }
}

struct dnaSeq *readMaskedNib(char *fileName, boolean mask)
/* Read nib, optionally turning masked bits to N's. 
 * Result is lower case where not masked. */
{
struct dnaSeq *seq;
if (mask)
    {
    seq = nibLoadAllMasked(NIB_MASK_MIXED, fileName);
    toggleCase(seq->dna, seq->size);
    upperToN(seq->dna, seq->size);
    }
else
    seq = nibLoadAll(fileName);
return seq;
}

struct dnaSeq *readMaskedTwoBit(struct twoBitFile *tbf, char *seqName,
	boolean mask)
/* Read seq from twoBitFile, optionally turning masked bits to N's. 
 * Result is lower case where not masked. */
{
struct dnaSeq *seq;
if (mask)
    {
    seq = twoBitReadSeqFrag(tbf, seqName, 0, 0);
    toggleCase(seq->dna, seq->size);
    upperToN(seq->dna, seq->size);
    }
else
    seq = twoBitReadSeqFragLower(tbf, seqName, 0, 0);
return seq;
}


static void transCountBothStrands(struct dnaSeq *seq, 
    struct genoFind *transGf[2][3])
/* Count (unmasked) tiles in both strand of sequence. 
 * As a side effect this will reverse-complement seq. */
{
int isRc, frame;
struct trans3 *t3;
for (isRc=0; isRc <= 1; ++isRc)
    {
    if (isRc)
	reverseComplement(seq->dna, seq->size);
    t3 = trans3New(seq);
    for (frame = 0; frame < 3; ++frame)
	{
	struct genoFind *gf = transGf[isRc][frame];
	gfCountSeq(gf, t3->trans[frame]);
	}
    trans3Free(&t3);
    }
}

static void transIndexBothStrands(struct dnaSeq *seq, 
    struct genoFind *transGf[2][3], bits32 offset[2][3],
    int sourceIx, char *fileName)
/* Add unmasked tiles on both strands of sequence to
 * index.  As a side effect this will reverse-complement seq. */
{
int isRc, frame;
struct trans3 *t3;
struct gfSeqSource *ss;
for (isRc=0; isRc <= 1; ++isRc)
    {
    if (isRc)
	{
	reverseComplement(seq->dna, seq->size);
	}
    t3 = trans3New(seq);
    for (frame = 0; frame < 3; ++frame)
	{
	struct genoFind *gf = transGf[isRc][frame];
	ss = gf->sources + sourceIx;
	gfAddSeq(gf, t3->trans[frame], offset[isRc][frame]);
	ss->fileName = cloneString(fileName);
	ss->start = offset[isRc][frame];
	offset[isRc][frame] += t3->trans[frame]->size;
	ss->end = offset[isRc][frame];
	}
    trans3Free(&t3);
    }
}

void gfIndexTransNibsAndTwoBits(struct genoFind *transGf[2][3], 
    int fileCount, char *fileNames[], 
    int minMatch, int maxGap, int tileSize, int maxPat, char *oocFile,
    boolean allowOneMismatch, boolean doMask, int stepSize)
/* Make translated (6 frame) index for all .nib and .2bit files. */
{
struct genoFind *gf;
int i,isRc, frame;
bits32 offset[2][3];
char *fileName;
struct dnaSeq *seq;
int sourceCount = 0;
long long totalBases = 0, warnAt = maxTotalBases();

if (allowOneMismatch)
    errAbort("Don't currently support allowOneMismatch in gfIndexTransNibsAndTwoBits");
/* Allocate indices for all reading frames. */
for (isRc=0; isRc <= 1; ++isRc)
    {
    for (frame = 0; frame < 3; ++frame)
	{
	transGf[isRc][frame] = gf = gfNewEmpty(minMatch, maxGap, 
		tileSize, stepSize, maxPat, oocFile, TRUE, allowOneMismatch);
	}
    }

/* Mask simple AA repeats (of period 1 and 2). */
for (isRc = 0; isRc <= 1; ++isRc)
    for (frame = 0; frame < 3; ++frame)
	maskSimplePepRepeat(transGf[isRc][frame]);

/* Scan through .nib and .2bit files once counting tiles. */
for (i=0; i<fileCount; ++i)
    {
    fileName = fileNames[i];
    printf("Counting %s\n", fileName);
    if (nibIsFile(fileName))
	{
	seq = readMaskedNib(fileName, doMask);
	transCountBothStrands(seq, transGf);
	sourceCount += 1;
	totalBases += seq->size;
	freeDnaSeq(&seq);
	}
    else if (twoBitIsFile(fileName))
        {
	struct twoBitFile *tbf = twoBitOpen(fileName);
	struct twoBitIndex *index;
	totalBases += twoBitCheckTotalSize(tbf);

	for (index = tbf->indexList; index != NULL; index = index->next)
	    {
	    seq = readMaskedTwoBit(tbf, index->name, doMask);
	    transCountBothStrands(seq, transGf);
	    sourceCount += 1;
	    freeDnaSeq(&seq);
	    }
	twoBitClose(&tbf);
	}
    else 
	errAbort("Unrecognized file type %s", fileName);
    if (totalBases >= warnAt)
	errAbort("Exceeding 4 billion bases, sorry gfServer can't handle that.");
    }

/* Get space for entries in indexed of all reading frames. */
for (isRc=0; isRc <= 1; ++isRc)
    {
    for (frame = 0; frame < 3; ++frame)
	{
	gf = transGf[isRc][frame];
	gfAllocLists(gf);
	gfZeroNonOverused(gf);
	AllocArray(gf->sources, sourceCount);
	gf->sourceCount = sourceCount;
	offset[isRc][frame] = 0;
	}
    }

/* Scan through nibs a second time building index. */
sourceCount = 0;
for (i=0; i<fileCount; ++i)
    {
    fileName = fileNames[i];
    printf("Indexing %s\n", fileName);
    if (nibIsFile(fileName))
	{
	seq = readMaskedNib(fileName, doMask);
	transIndexBothStrands(seq, transGf, offset, sourceCount, fileName);
	freeDnaSeq(&seq);
	sourceCount += 1;
	}
    else	/* .2bit file */
        {
	struct twoBitFile *tbf = twoBitOpen(fileName);
	struct twoBitIndex *index;
	for (index = tbf->indexList; index != NULL; index = index->next)
	    {
	    char nameBuf[PATH_LEN+256];
	    safef(nameBuf, sizeof(nameBuf), "%s:%s", fileName, index->name);
	    seq = readMaskedTwoBit(tbf, index->name, doMask);
	    transIndexBothStrands(seq, transGf, offset, sourceCount, nameBuf);
	    sourceCount += 1;
	    freeDnaSeq(&seq);
	    }
	twoBitClose(&tbf);
	}
    }

for (isRc=0; isRc <= 1; ++isRc)
    {
    for (frame = 0; frame < 3; ++frame)
	{
	gf = transGf[isRc][frame];
	gf->totalSeqSize = offset[isRc][frame];
	gfZeroOverused(gf);
	}
    }
}

static struct genoFind *gfSmallIndexSeq(struct genoFind *gf, bioSeq *seqList,
	int minMatch, int maxGap, int tileSize, int maxPat, char *oocFile, 
	boolean isPep, boolean maskUpper)
/* Make index for all seqs in list. */
{
int seqCount = slCount(seqList);
bioSeq *seq;
int i;
bits32 offset = 0;
struct gfSeqSource *ss;

if (isPep)
    maskSimplePepRepeat(gf);
for (seq = seqList; seq != NULL; seq = seq->next)
    gfCountSeq(gf, seq);
gfAllocLists(gf);
gfZeroNonOverused(gf);
if (seqCount > 0)
    AllocArray(gf->sources, seqCount);
gf->sourceCount = seqCount;
for (i=0, seq = seqList; i<seqCount; ++i, seq = seq->next)
    {
    gfAddSeq(gf, seq, offset);
    ss = gf->sources+i;
    ss->seq = seq;
    ss->start = offset;
    offset += seq->size;
    ss->end = offset;
    if (maskUpper)
	ss->maskedBits = maskFromUpperCaseSeq(seq);
    }
gf->totalSeqSize = offset;
gfZeroOverused(gf);
return gf;
}

static struct genoFind *gfLargeIndexSeq(struct genoFind *gf, bioSeq *seqList,
	int minMatch, int maxGap, int tileSize, int maxPat, char *oocFile, 
	boolean isPep, boolean maskUpper)
/* Make index for all seqs in list. */
{
int seqCount = slCount(seqList);
bioSeq *seq;
int i;
bits32 offset = 0;
struct gfSeqSource *ss;

for (seq = seqList; seq != NULL; seq = seq->next)
    gfCountSeq(gf, seq);
gfAllocLargeLists(gf);
gfZeroNonOverused(gf);
AllocArray(gf->sources, seqCount);
gf->sourceCount = seqCount;
for (i=0, seq = seqList; i<seqCount; ++i, seq = seq->next)
    {
    gfAddLargeSeq(gf, seq, offset);
    ss = gf->sources+i;
    ss->seq = seq;
    ss->start = offset;
    offset += seq->size;
    ss->end = offset;
    if (maskUpper)
	ss->maskedBits = maskFromUpperCaseSeq(seq);
    }
gf->totalSeqSize = offset;
gfZeroOverused(gf);
return gf;
}


struct genoFind *gfIndexSeq(bioSeq *seqList,
	int minMatch, int maxGap, int tileSize, int maxPat, char *oocFile, 
	boolean isPep, boolean allowOneMismatch, boolean maskUpper,
	int stepSize)
/* Make index for all seqs in list.  For DNA sequences upper case bits will
 * be unindexed. */
{
struct genoFind *gf = gfNewEmpty(minMatch, maxGap, tileSize, stepSize, maxPat, 
				oocFile, isPep, allowOneMismatch);
if (stepSize == 0)
    stepSize = tileSize;
if (gf->segSize > 0)
    {
    gfLargeIndexSeq(gf, seqList, minMatch, maxGap, tileSize, maxPat, oocFile, isPep, maskUpper);
    }
else
    {
    gfSmallIndexSeq(gf, seqList, minMatch, maxGap, tileSize, maxPat, oocFile, isPep, maskUpper);
    }
return gf;
}

static int bCmpSeqSource(const void *vTarget, const void *vRange)
/* Compare function for binary search of gfSeqSource. */
{
const bits32 *pTarget = vTarget;
bits32 target = *pTarget;
const struct gfSeqSource *ss = vRange;

if (target < ss->start) return -1;
if (target >= ss->end) return 1;
return 0;
}

static struct gfSeqSource *findSource(struct genoFind *gf, bits32 targetPos)
/* Find source given target position. */
{
struct gfSeqSource *ss =  bsearch(&targetPos, gf->sources, gf->sourceCount, 
	sizeof(gf->sources[0]), bCmpSeqSource);
if (ss == NULL)
    errAbort("Couldn't find source for %d", targetPos);
return ss;
}

void gfClumpFree(struct gfClump **pClump)
/* Free a single clump. */
{
struct gfClump *clump;
if ((clump = *pClump) == NULL)
    return;
freez(pClump);
}

void gfClumpFreeList(struct gfClump **pList)
/* Free a list of dynamically allocated gfClump's */
{
struct gfClump *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gfClumpFree(&el);
    }
*pList = NULL;
}

void gfClumpDump(struct genoFind *gf, struct gfClump *clump, FILE *f)
/* Print out info on clump */
{
struct gfSeqSource *ss = clump->target;
char *name = ss->fileName;

if (name == NULL) name = ss->seq->name;
fprintf(f, "%u-%u %s %u-%u, hits %d\n", 
	clump->qStart, clump->qEnd, name,
	clump->tStart - ss->start, clump->tEnd - ss->start,
	clump->hitCount);
#ifdef SOMETIMES
for (hit = clump->hitList; hit != NULL; hit = hit->next)
    fprintf(f, "   q %d, t %d, diag %d\n", hit->qStart, hit->tStart, hit->diagonal);
#endif
}

/* Fast sorting routines for sorting gfHits on diagonal. 
 * More or less equivalent to system qsort, but with
 * comparison function inline.  Worth a little tweaking
 * since this is the bottleneck for the whole procedure. */

static void gfHitSort2(struct gfHit **ptArray, int n);

/* Some variables used by recursive function gfHitSort2
 * across all incarnations. */
static struct gfHit **nosTemp, *nosSwap;

static void gfHitSort2(struct gfHit **ptArray, int n)
/* This is a fast recursive sort that uses a temporary
 * buffer (nosTemp) that has to be as big as the array
 * that is being sorted. */
{
struct gfHit **tmp, **pt1, **pt2;
int n1, n2;

/* Divide area to sort in two. */
n1 = (n>>1);
n2 = n - n1;
pt1 = ptArray;
pt2 =  ptArray + n1;

/* Sort each area separately.  Handle small case (2 or less elements)
 * here.  Otherwise recurse to sort. */
if (n1 > 2)
    gfHitSort2(pt1, n1);
else if (n1 == 2 && pt1[0]->diagonal > pt1[1]->diagonal)
    {
    nosSwap = pt1[1];
    pt1[1] = pt1[0];
    pt1[0] = nosSwap;
    }
if (n2 > 2)
    gfHitSort2(pt2, n2);
else if (n2 == 2 && pt2[0]->diagonal > pt2[1]->diagonal)
    {
    nosSwap = pt2[1];
    pt2[1] = pt2[0];
    pt2[0] = nosSwap;
    }

/* At this point both halves are internally sorted. 
 * Do a merge-sort between two halves copying to temp
 * buffer.  Then copy back sorted result to main buffer. */
tmp = nosTemp;
while (n1 > 0 && n2 > 0)
    {
    if ((*pt1)->diagonal <= (*pt2)->diagonal)
	{
	--n1;
	*tmp++ = *pt1++;
	}
    else
	{
	--n2;
	*tmp++ = *pt2++;
	}
    }
/* One or both sides are now fully merged. */
assert(tmp + n1 + n2 == nosTemp + n);

/* If some of first side left to merge copy it to end of temp buf. */
if (n1 > 0)
    memcpy(tmp, pt1, n1 * sizeof(*tmp));

/* If some of second side left to merge, we finesse it here:
 * simply refrain from copying over it as we copy back temp buf. */
memcpy(ptArray, nosTemp, (n - n2) * sizeof(*ptArray));
}

void gfHitSortDiagonal(struct gfHit **pList)
/* Sort a singly linked list with Qsort and a temporary array. */
{
struct gfHit *list = *pList;
if (list != NULL && list->next != NULL)
    {
    int count = slCount(list);
    struct gfHit *el;
    struct gfHit **array;
    int i;
    int byteSize = count*sizeof(*array);
    array = needLargeMem(byteSize);
    nosTemp = needLargeMem(byteSize);
    for (el = list, i=0; el != NULL; el = el->next, i++)
        array[i] = el;
    gfHitSort2(array, count);
    list = NULL;
    for (i=0; i<count; ++i)
        {
        array[i]->next = list;
        list = array[i];
        }
    freez(&array);
    freez(&nosTemp);
    slReverse(&list);
    *pList = list;       
    }
}



static int cmpQuerySize;

#ifdef UNUSED
static int gfHitCmpDiagonal(const void *va, const void *vb)
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
#endif /* UNUSED */

static int gfHitCmpTarget(const void *va, const void *vb)
/* Compare to sort based on target offset. */
{
const struct gfHit *a = *((struct gfHit **)va);
const struct gfHit *b = *((struct gfHit **)vb);

if (a->tStart > b->tStart)
    return 1;
else if (a->tStart == b->tStart)
    return 0;
else
    return -1;
}

static int gfHitCmpQuery(const void *va, const void *vb)
/* Compare to sort based on target offset. (Thanks AG) */
{
const struct gfHit *a = *((struct gfHit **)va);
const struct gfHit *b = *((struct gfHit **)vb);

if (a->qStart > b->qStart)
    return 1;
else if (a->qStart == b->qStart)
    return 0;
else
    return -1;
}

static void gfClumpComputeQueryCoverage(struct gfClump *clumpList, 
	int tileSize)
/* Figure out how much of query is covered by clump list. (Thanks AG). */
{
struct gfClump *clump;
struct gfHit *hit;
struct gfHit *hitList;
int qcov;
int blockStart, blockEnd;

for (clump = clumpList; clump != NULL; clump = clump->next)
    {
    hitList = clump->hitList;
    if ( hitList != NULL)
	{
	slSort(&hitList, gfHitCmpQuery);
	qcov = 0;
	blockStart = hitList->qStart;
	blockEnd = blockStart + tileSize;
	for (hit = hitList; hit != NULL; hit = hit->next)
	    {
	    if (hit->qStart > blockEnd)
		{
		/* We're skipping some, so output block so far, and start new block. */
		qcov += blockEnd - blockStart;
		blockStart = hit->qStart;
		blockEnd = blockStart + tileSize;
		}
	    else if (hit->qStart + tileSize > blockEnd)
		{
		/* Expand current block. */
		blockEnd = hit->qStart + tileSize;
		}
	    }
	qcov += blockEnd - blockStart;
	clump->queryCoverage = qcov;
	}
    }
}



static int gfClumpCmpQueryCoverage(const void *va, const void *vb)
/* Compare to sort based on query coverage. */
{
const struct gfClump *a = *((struct gfClump **)va);
const struct gfClump *b = *((struct gfClump **)vb);

return (b->queryCoverage - a->queryCoverage);
}

static void findClumpBounds(struct gfClump *clump, int tileSize)
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
clump->tEnd += tileSize;
clump->qEnd += tileSize;
}

static void targetClump(struct genoFind *gf, struct gfClump **pClumpList, struct gfClump *clump)
/* Add target sequence to clump.  If clump had multiple targets split it into
 * multiple clumps, one per target.  Add clump(s) to list. */
{
struct gfSeqSource *ss = findSource(gf, clump->tStart);
if (ss->end >= clump->tEnd)
    {
    /* Usual simple case: all of clump hits single target. */
    clump->target = ss;
    slAddHead(pClumpList, clump);
    }
else
    {
    /* If we've gotten here, then the clump is split across multiple targets.
     * We'll have to split it into multiple clumps... */
    struct gfHit *hit, *nextHit, *inList, *outList, *oldList = clump->hitList;
    int hCount;

    while (oldList != NULL)
        {
	inList = outList = NULL;
	hCount = 0;
	for (hit = oldList; hit != NULL; hit = nextHit)
	    {
	    nextHit = hit->next;
	    if (ss->start <= hit->tStart  && hit->tStart < ss->end)
	        {
		++hCount;
		slAddHead(&inList, hit);
		}
	    else
	        {
		slAddHead(&outList, hit);
		}
	    }
	slReverse(&inList);
	slReverse(&outList);
	if (hCount >= gf->minMatch)
	    {
	    struct gfClump *newClump;
	    AllocVar(newClump);
	    newClump->hitList = inList;
	    newClump->hitCount = hCount;
	    newClump->target = ss;
	    findClumpBounds(newClump, gf->tileSize);
	    slAddHead(pClumpList, newClump);
	    }
	else
	    {
	    inList = NULL;
	    }
	oldList = outList;
	if (oldList != NULL)
	    {
	    ss = findSource(gf, oldList->tStart);
	    }
	}
    clump->hitList = NULL;	/* We ate up the hit list. */
    gfClumpFree(&clump);
    }
}

static int gfNearEnough = 300;

static struct gfClump *clumpNear(struct genoFind *gf, struct gfClump *oldClumps, int minMatch)
/* Go through clump list and make sure hits are also near each other.
 * If necessary divide clumps. */
{
struct gfClump *newClumps = NULL, *clump, *nextClump;
struct gfHit *hit, *nextHit;
int tileSize = gf->tileSize;
bits32 lastT;
int nearEnough = (gf->isPep ? gfNearEnough/3 : gfNearEnough);

for (clump = oldClumps; clump != NULL; clump = nextClump)
    {
    struct gfHit *newHits = NULL, *oldHits = clump->hitList;
    int clumpSize = 0;
    clump->hitCount = 0;
    clump->hitList = NULL;	/* Clump no longer owns list. */
    nextClump = clump->next;
    slSort(&oldHits, gfHitCmpTarget);
    lastT = oldHits->tStart;
    for (hit = oldHits; hit != NULL; hit = nextHit)
        {
	nextHit = hit->next;
	if (hit->tStart > nearEnough + lastT)
	     {
	     if (clumpSize >= minMatch)
	         {
		 slReverse(&newHits);
		 clump->hitList = newHits;
		 newHits = NULL;
		 clump->hitCount = clumpSize;
		 findClumpBounds(clump, tileSize);
		 targetClump(gf, &newClumps, clump);
		 AllocVar(clump);
		 }
	     else
	         {
		 newHits = NULL;
		 clump->hitCount = 0;
		 }
	     clumpSize = 0;
	     }
	lastT = hit->tStart;
	slAddHead(&newHits, hit);
	++clumpSize;
	}
    slReverse(&newHits);
    clump->hitList = newHits;
    if (clumpSize >= minMatch)
        {
	clump->hitCount = clumpSize;
	findClumpBounds(clump, tileSize);
	targetClump(gf, &newClumps, clump);
	}
    else
        {
	gfClumpFree(&clump);
	}
    }
return newClumps;
}

static struct gfClump *clumpHits(struct genoFind *gf, struct gfHit *hitList, int minMatch)
/* Clump together hits according to parameters in gf. */
{
struct gfClump *clumpList = NULL, *clump = NULL;
int maxGap = gf->maxGap;
struct gfHit *hit, *nextHit, *lastHit = NULL;
int totalHits = 0, usedHits = 0, clumpCount = 0;
int tileSize = gf->tileSize;
int bucketShift = 16;		/* 64k buckets. */
bits32 bucketSize = (1<<bucketShift);
int bucketCount = (gf->totalSeqSize >> bucketShift) + 1;
int nearEnough = (gf->isPep ? gfNearEnough/3 : gfNearEnough);
bits32 boundary = bucketSize - nearEnough;
int i;
struct gfHit **buckets = NULL, **pb;

/* Sort hit list into buckets. */
AllocArray(buckets, bucketCount);
for (hit = hitList; hit != NULL; hit = nextHit)
    {
    nextHit = hit->next;
    pb = buckets + (hit->tStart >> bucketShift);
    slAddHead(pb, hit);
    }

/* Sort each bucket on diagonal and clump. */
for (i=0; i<bucketCount; ++i)
    {
    int clumpSize;
    bits32 maxT;
    struct gfHit *clumpHits;
    pb = buckets + i;
    gfHitSortDiagonal(pb);
    for (hit = *pb; hit != NULL; )
         {
	 /* Each time through this loop will get info on a clump.  Will only
	  * actually create clump if it is big enough though. */
	 clumpSize = 0;
	 clumpHits = lastHit = NULL;
	 maxT = 0;
	 for (; hit != NULL; hit = nextHit)
	     {
	     nextHit = hit->next;
	     if (lastHit != NULL && hit->diagonal - lastHit->diagonal > maxGap)
		 break;
	     if (hit->tStart > maxT) maxT = hit->tStart;
	     slAddHead(&clumpHits, hit);
	     ++clumpSize;
	     ++totalHits;
	     lastHit = hit;
	     }
	 if (maxT > boundary && i < bucketCount-1)
	     {
	     /* Move clumps that are near boundary to next bucket to give them a
	      * chance to merge with hits there. */
	     buckets[i+1] = slCat(clumpHits, buckets[i+1]);
	     }
	 else if (clumpSize >= minMatch)
	     {
	     /* Save clumps that are large enough on list. */
	     AllocVar(clump);
	     slAddHead(&clumpList, clump);
	     clump->hitCount = clumpSize;
	     clump->hitList = clumpHits;
	     usedHits += clumpSize;
	     ++clumpCount;
	     }
	 }
    *pb = NULL;
    boundary += bucketSize;
    }
clumpList = clumpNear(gf, clumpList, minMatch);
gfClumpComputeQueryCoverage(clumpList, tileSize);	/* Thanks AG */
slSort(&clumpList, gfClumpCmpQueryCoverage);

#ifdef DEBUG
uglyf("Dumping clumps B\n");
for (clump = clumpList; clump != NULL; clump = clump->next)	/* uglyf */
    {
    uglyf(" %d %d %s %d %d (%d hits)\n", clump->qStart, clump->qEnd, clump->target->seq->name,   clump->tStart, clump->tEnd, clump->hitCount);
    }
#endif /* DEBUG */
freez(&buckets);
return clumpList;
}


static struct gfHit *gfFastFindDnaHits(struct genoFind *gf, struct dnaSeq *seq, 
	Bits *qMaskBits,  int qMaskOffset, struct lm *lm, int *retHitCount,
	struct gfSeqSource *target, int tMin, int tMax)
/* Find hits associated with one sequence. This is is special fast
 * case for DNA that is in an unsegmented index. */
{
struct gfHit *hitList = NULL, *hit;
int size = seq->size;
int tileSizeMinusOne = gf->tileSize - 1;
int mask = gf->tileMask;
DNA *dna = seq->dna;
int i, j;
bits32 bits = 0;
bits32 bVal;
int listSize;
bits32 qStart, *tList;
int hitCount = 0;

for (i=0; i<tileSizeMinusOne; ++i)
    {
    bVal = ntValNoN[(int)dna[i]];
    bits <<= 2;
    bits += bVal;
    }
for (i=tileSizeMinusOne; i<size; ++i)
    {
    bVal = ntValNoN[(int)dna[i]];
    bits <<= 2;
    bits += bVal;
    bits &= mask;
    listSize = gf->listSizes[bits];
    if (listSize != 0)
	{
	qStart = i-tileSizeMinusOne;
	if (qMaskBits == NULL || bitCountRange(qMaskBits, qStart+qMaskOffset, gf->tileSize) == 0)
	    {
	    tList = gf->lists[bits];
	    for (j=0; j<listSize; ++j)
		{
		int tStart = tList[j];
		if (target == NULL || 
			(target == findSource(gf, tStart) && tStart >= tMin && tStart < tMax) ) 
		    {
		    lmAllocVar(lm, hit);
		    hit->qStart = qStart;
		    hit->tStart = tStart;
		    hit->diagonal = tStart + size - qStart;
		    slAddHead(&hitList, hit);
		    ++hitCount;
		    }
		}
	    }
	}
    }
*retHitCount = hitCount;
return hitList;
}

static struct gfHit *gfStraightFindHits(struct genoFind *gf, aaSeq *seq, 
	Bits *qMaskBits, int qMaskOffset, struct lm *lm, int *retHitCount,
	struct gfSeqSource *target, int tMin, int tMax)
/* Find hits associated with one sequence in a non-segmented
 * index where hits match exactly. */
{
struct gfHit *hitList = NULL, *hit;
int size = seq->size;
int tileSize = gf->tileSize;
int lastStart = size - tileSize;
char *poly = seq->dna;
int i, j;
int tile;
int listSize;
bits32 qStart, *tList;
int hitCount = 0;
int (*makeTile)(char *poly, int n) = (gf->isPep ? gfPepTile : gfDnaTile);

initNtLookup();
for (i=0; i<=lastStart; ++i)
    {
    tile = makeTile(poly+i, tileSize);
    if (tile < 0)
	continue;
    listSize = gf->listSizes[tile];
    if (listSize > 0)
	{
	qStart = i;
	if (qMaskBits == NULL || bitCountRange(qMaskBits, qStart+qMaskOffset, tileSize) == 0)
	    {
	    tList = gf->lists[tile];
	    for (j=0; j<listSize; ++j)
		{
		int tStart = tList[j];
		if (target == NULL || 
			(target == findSource(gf, tStart) && tStart >= tMin && tStart < tMax) ) 
		    {
		    lmAllocVar(lm,hit);
		    hit->qStart = qStart;
		    hit->tStart = tStart;
		    hit->diagonal = tStart + size - qStart;
		    slAddHead(&hitList, hit);
		    ++hitCount;
		    }
		}
	    }
	}
    }
*retHitCount = hitCount;
return hitList;
}

static struct gfHit *gfStraightFindNearHits(struct genoFind *gf, aaSeq *seq, 
	Bits *qMaskBits, int qMaskOffset, struct lm *lm, int *retHitCount,
	struct gfSeqSource *target, int tMin, int tMax)
/* Find hits associated with one sequence in a non-segmented
 * index where hits can mismatch in one letter. */
{
struct gfHit *hitList = NULL, *hit;
int size = seq->size;
int tileSize = gf->tileSize;
int lastStart = size - tileSize;
char *poly = seq->dna;
int i, j;
int tile;
int listSize;
bits32 qStart, *tList;
int hitCount = 0;
int varPos, varVal;	/* Variable position. */
int (*makeTile)(char *poly, int n); 
int alphabetSize;
char oldChar, zeroChar;
int *seqValLookup;
int posMul, avoid;

initNtLookup();
if (gf->isPep)
    {
    makeTile = gfPepTile;
    alphabetSize = 20;
    zeroChar = 'A';
    seqValLookup = aaVal;
    }
else
    {
    makeTile = gfDnaTile;
    alphabetSize = 4;
    zeroChar = 't';
    seqValLookup = ntVal;
    }

for (i=0; i<=lastStart; ++i)
    {
    posMul = 1;
    for (varPos = tileSize-1; varPos >=0; --varPos)
	{
	/* Make a tile that has zero value at variable position. */
	oldChar = poly[i+varPos];
	poly[i+varPos] = zeroChar;
	tile = makeTile(poly+i, tileSize);
	poly[i+varPos] = oldChar;

	/* Avoid checking the unmodified tile multiple times. */
	if (varPos == 0)
	    avoid = -1;
	else
	    avoid = seqValLookup[(int)oldChar];

	if (tile >= 0)
	    {
	    /* Look up all possible values of variable position. */
	    for (varVal=0; varVal<alphabetSize; ++varVal)
		{
		if (varVal != avoid)
		    {
		    listSize = gf->listSizes[tile];
		    if (listSize > 0)
			{
			qStart = i;
			if (qMaskBits == NULL || bitCountRange(qMaskBits, qStart+qMaskOffset, tileSize) == 0)
			    {
			    tList = gf->lists[tile];
			    for (j=0; j<listSize; ++j)
				{
				int tStart = tList[j];
				if (target == NULL || 
					(target == findSource(gf, tStart) 
					&& tStart >= tMin && tStart < tMax) ) 
				    {
				    lmAllocVar(lm,hit);
				    hit->qStart = qStart;
				    hit->tStart = tStart;
				    hit->diagonal = tStart + size - qStart;
				    slAddHead(&hitList, hit);
				    ++hitCount;
				    }
				}
			    }
			}
		    }
		tile += posMul;
		}
	    }
	posMul *= alphabetSize;
	}
    }
*retHitCount = hitCount;
return hitList;
}

static struct gfHit *gfSegmentedFindHits(struct genoFind *gf, aaSeq *seq, 
	Bits *qMaskBits, int qMaskOffset, struct lm *lm, int *retHitCount,
	struct gfSeqSource *target, int tMin, int tMax)
/* Find hits associated with one sequence in general case in a segmented
 * index. */
{
struct gfHit *hitList = NULL, *hit;
int size = seq->size;
int tileSize = gf->tileSize;
int tileTailSize = gf->segSize;
int tileHeadSize = gf->tileSize - tileTailSize;
int lastStart = size - tileSize;
char *poly = seq->dna;
int i, j;
int tileHead, tileTail;
int listSize;
bits32 qStart;
bits16 *endList;
int hitCount = 0;
int (*makeTile)(char *poly, int n) = (gf->isPep ? gfPepTile : gfDnaTile);


initNtLookup();
for (i=0; i<=lastStart; ++i)
    {
    if (qMaskBits == NULL || bitCountRange(qMaskBits, i+qMaskOffset, tileSize) == 0)
	{
	tileHead = makeTile(poly+i, tileHeadSize);
	if (tileHead < 0)
	    continue;
	tileTail = makeTile(poly+i+tileHeadSize, tileTailSize);
	if (tileTail < 0)
	    continue;
	listSize = gf->listSizes[tileHead];
	qStart = i;
	endList = gf->endLists[tileHead];
	for (j=0; j<listSize; ++j)
	    {
	    if (endList[0] == tileTail)
		{
		int tStart = (endList[1]<<16) + endList[2];
		if (target == NULL || 
			(target == findSource(gf, tStart) 
			&& tStart >= tMin && tStart < tMax) ) 
		    {
		    lmAllocVar(lm,hit);
		    hit->qStart = qStart;
		    hit->tStart = tStart;
		    hit->diagonal = tStart + size - qStart;
		    slAddHead(&hitList, hit);
		    ++hitCount;
		    }
		}
	    endList += 3;
	    }
	}
    }
*retHitCount = hitCount;
return hitList;
}

static struct gfHit *gfSegmentedFindNearHits(struct genoFind *gf, 
	aaSeq *seq, Bits *qMaskBits, int qMaskOffset, struct lm *lm, int *retHitCount,
	struct gfSeqSource *target, int tMin, int tMax)
/* Find hits associated with one sequence in a segmented
 * index where one mismatch is allowed. */
{
struct gfHit *hitList = NULL, *hit;
int size = seq->size;
int tileSize = gf->tileSize;
int tileTailSize = gf->segSize;
int tileHeadSize = gf->tileSize - tileTailSize;
int lastStart = size - tileSize;
char *poly = seq->dna;
int i, j;
int tileHead, tileTail;
int listSize;
bits32 qStart;
bits16 *endList;
int hitCount = 0;
int varPos, varVal;	/* Variable position. */
int (*makeTile)(char *poly, int n); 
int alphabetSize;
char oldChar, zeroChar;
int headPosMul, tailPosMul, avoid;
boolean modTail;
int *seqValLookup;


initNtLookup();
if (gf->isPep)
    {
    makeTile = gfPepTile;
    alphabetSize = 20;
    zeroChar = 'A';
    seqValLookup = aaVal;
    }
else
    {
    makeTile = gfDnaTile;
    alphabetSize = 4;
    zeroChar = 't';
    seqValLookup = ntVal;
    }

for (i=0; i<=lastStart; ++i)
    {
    if (qMaskBits == NULL || bitCountRange(qMaskBits, i+qMaskOffset, tileSize) == 0)
	{
	headPosMul = tailPosMul = 1;
	for (varPos = tileSize-1; varPos >= 0; --varPos)
	    {
	    /* Make a tile that has zero value at variable position. */
	    modTail = (varPos >= tileHeadSize);
	    oldChar = poly[i+varPos];
	    poly[i+varPos] = zeroChar;
	    tileHead = makeTile(poly+i, tileHeadSize);
	    tileTail = makeTile(poly+i+tileHeadSize, tileTailSize);
	    poly[i+varPos] = oldChar;

	    /* Avoid checking the unmodified tile multiple times. */
	    if (varPos == 0)
		avoid = -1;
	    else
		avoid = seqValLookup[(int)oldChar];

	    if (tileHead >= 0 && tileTail >= 0)
		{
		for (varVal=0; varVal<alphabetSize; ++varVal)
		    {
		    if (varVal != avoid)
			{
			listSize = gf->listSizes[tileHead];
			qStart = i;
			endList = gf->endLists[tileHead];
			for (j=0; j<listSize; ++j)
			    {
			    if (endList[0] == tileTail)
				{
				int tStart = (endList[1]<<16) + endList[2];
				if (target == NULL || 
					(target == findSource(gf, tStart) 
					&& tStart >= tMin && tStart < tMax) ) 
				    {
				    lmAllocVar(lm,hit);
				    hit->qStart = qStart;
				    hit->tStart = tStart;
				    hit->diagonal = tStart + size - qStart;
				    slAddHead(&hitList, hit);
				    ++hitCount;
				    }
				}
			    endList += 3;
			    }
			}
		    if (modTail)
			tileTail += tailPosMul;
		    else 
			tileHead += headPosMul;
		    }
		}
	    if (modTail)
		tailPosMul *= alphabetSize;
	    else 
		headPosMul *= alphabetSize;
	    }
	}
    }
*retHitCount = hitCount;
return hitList;
}


static struct gfHit *gfFindHitsWithQmask(struct genoFind *gf, bioSeq *seq,
	Bits *qMaskBits, int qMaskOffset, struct lm *lm, int *retHitCount, 
	struct gfSeqSource *target, int tMin, int tMax)
/* Find hits associated with one sequence soft-masking seq according to qMaskBits.
 * The hits will be in genome rather than chromosome coordinates. */
{
struct gfHit *hitList = NULL;
if (gf->segSize == 0 && !gf->isPep && !gf->allowOneMismatch)
    {
    hitList = gfFastFindDnaHits(gf, seq, qMaskBits, qMaskOffset, lm, retHitCount,
	target, tMin, tMax);
    }
else
    {
    if (gf->segSize == 0)
	{
	if (gf->allowOneMismatch)
	    {
	    hitList = gfStraightFindNearHits(gf, seq, qMaskBits, qMaskOffset, lm, 
		retHitCount, target, tMin, tMax);
	    }
	else
	    {
	    hitList = gfStraightFindHits(gf, seq, qMaskBits, qMaskOffset, lm, retHitCount, 
		target, tMin, tMax);
	    }
	}
    else
	{
	if (gf->allowOneMismatch)
	    {
	    hitList = gfSegmentedFindNearHits(gf, seq, qMaskBits, qMaskOffset, lm, retHitCount,
		target, tMin, tMax);
	    }
	else
	    {
	    hitList = gfSegmentedFindHits(gf, seq, qMaskBits, qMaskOffset, lm, retHitCount,
		target, tMin, tMax);
	    }
	}
    }
return hitList;
}

static void dumpClump(struct gfClump *clump, FILE *f)
/* Print out a clump */
{
struct gfSeqSource *target = clump->target;
char *tName = target->fileName;
if (tName == NULL) tName = target->seq->name;
fprintf(f, "%d-%d\t%s:%d-%d\n", clump->qStart, clump->qEnd, tName, clump->tStart, clump->tEnd);
}

static void dumpClumpList(struct gfClump *clumpList, FILE *f)
/* Dump list of clumps. */
{
struct gfClump *clump;
for (clump = clumpList; clump != NULL; clump = clump->next)
    dumpClump(clump, f);
}

struct gfClump *gfFindClumpsWithQmask(struct genoFind *gf, bioSeq *seq, 
	Bits *qMaskBits, int qMaskOffset,
	struct lm *lm, int *retHitCount)
/* Find clumps associated with one sequence soft-masking seq according to qMaskBits */
{
struct gfClump *clumpList = NULL;
struct gfHit *hitList;
int minMatch = gf->minMatch;

#ifdef OLD	/* stepSize makes this obsolete. */
if (seq->size < gf->tileSize * (gf->minMatch+1))
     minMatch = 1;
#endif /* OLD */

hitList =  gfFindHitsWithQmask(gf, seq, qMaskBits, qMaskOffset, lm,
	retHitCount, NULL, 0, 0);
cmpQuerySize = seq->size;
clumpList = clumpHits(gf, hitList, minMatch);
return clumpList;
}

struct gfHit *gfFindHitsInRegion(struct genoFind *gf, bioSeq *seq, 
	Bits *qMaskBits, int qMaskOffset, struct lm *lm, 
	struct gfSeqSource *target, int tMin, int tMax)
/* Find hits restricted to one particular region. 
 * The hits returned by this will be in target sequence
 * coordinates rather than concatenated whole genome
 * coordinates as hits inside of clumps usually are.  */
{
int targetStart;
struct gfHit *hitList, *hit;
int hitCount;

targetStart = target->start;
hitList =  gfFindHitsWithQmask(gf, seq, qMaskBits, qMaskOffset, lm,
	&hitCount, target, tMin + targetStart, tMax + targetStart);
for (hit = hitList; hit != NULL; hit = hit->next)
    hit->tStart -= targetStart;
return hitList;
}

struct gfClump *gfFindClumps(struct genoFind *gf, bioSeq *seq, struct lm *lm, int *retHitCount)
/* Find clumps associated with one sequence. */
{
return gfFindClumpsWithQmask(gf, seq, NULL, 0, lm, retHitCount);
}


void gfTransFindClumps(struct genoFind *gfs[3], aaSeq *seq, struct gfClump *clumps[3], struct lm *lm, int *retHitCount)
/* Find clumps associated with one sequence in three translated reading frames. */
{
int frame;
int oneHit;
int hitCount = 0;
for (frame = 0; frame < 3; ++frame)
    {
    clumps[frame] = gfFindClumps(gfs[frame], seq, lm, &oneHit);
    hitCount += oneHit;
    }
*retHitCount = hitCount;
}

void gfTransTransFindClumps(struct genoFind *gfs[3], aaSeq *seqs[3], 
	struct gfClump *clumps[3][3], struct lm *lm, int *retHitCount)
/* Find clumps associated with three sequences in three translated 
 * reading frames. Used for translated/translated protein comparisons. */
{
int qFrame;
int oneHit;
int hitCount = 0;

for (qFrame = 0; qFrame<3; ++qFrame)
    {
    gfTransFindClumps(gfs, seqs[qFrame], clumps[qFrame], lm, &oneHit);
    hitCount += oneHit;
    }
*retHitCount = hitCount;
}

void gfMakeOoc(char *outName, char *files[], int fileCount, 
	int tileSize, bits32 maxPat, enum gfType tType)
/* Count occurences of tiles in seqList and make a .ooc file. */
{
boolean dbIsPep = (tType == gftProt || tType == gftDnaX || tType == gftRnaX);
struct genoFind *gf = gfNewEmpty(gfMinMatch, gfMaxGap, tileSize, tileSize,
	maxPat, NULL, dbIsPep, FALSE);
bits32 *sizes = gf->listSizes;
int tileSpaceSize = gf->tileSpaceSize;
bioSeq *seq, *seqList;
bits32 sig = oocSig, psz = tileSize;
bits32 i;
int oocCount = 0;
char *inName;
FILE *f = mustOpen(outName, "w");

if (gf->segSize > 0)
    errAbort("Don't yet know how to make ooc files for large tile sizes.");
for (i=0; i<fileCount; ++i)
    {
    inName = files[i];
    printf("Loading %s\n", inName);
    if (nibIsFile(inName))
        {
	seqList = nibLoadAll(inName);
	}
    else if (twoBitIsFile(inName))
        {
	seqList = twoBitLoadAll(inName);
	for (seq = seqList; seq != NULL; seq = seq->next)
	    toLowerN(seq->dna, seq->size);
	}
    else
        {
	seqList = faReadAllSeq(inName, tType != gftProt);
	}
    printf("Counting %s\n", inName);
    for (seq = seqList; seq != NULL; seq = seq->next)
	{
	int isRc;
	for (isRc = 0; isRc <= 1; ++isRc)
	    {
	    if (tType == gftDnaX || tType == gftRnaX)
		{
		struct trans3 *t3 = trans3New(seq);
		int frame;
		for (frame=0; frame<3; ++frame)
		    {
		    gfCountSeq(gf, t3->trans[frame]);
		    }
		trans3Free(&t3);
		}
	    else
		{
		gfCountSeq(gf, seq);
		}
	    if (tType == gftProt || tType == gftRnaX)
	        break;
	    else 
	        {
		reverseComplement(seq->dna, seq->size);
		}
	    }
	}
    freeDnaSeqList(&seqList);
    }
printf("Writing %s\n", outName);
writeOne(f, sig);
writeOne(f, psz);
for (i=0; i<tileSpaceSize; ++i)
    {
    if (sizes[i] >= maxPat)
	{
	writeOne(f, i);
	++oocCount;
	}
    }
carefulClose(&f);
genoFindFree(&gf);
printf("Wrote %d overused %d-mers to %s\n", oocCount, tileSize, outName);
}

struct gfSeqSource *gfFindNamedSource(struct genoFind *gf, char *name)
/* Find target of given name.  Return NULL if none. */
{
struct gfSeqSource *source = gf->sources;
int count = gf->sourceCount;

if (source->seq == NULL)	/* Use first source to see if seq or file. */
    {
    char rootName[256];
    while (--count >= 0)
	{
	splitPath(source->fileName, NULL, rootName, NULL);
	if (sameString(name, rootName))
	     return source;
	}
    }
else
    {
    while (--count >= 0)
	{
	if (sameString(source->seq->name, name))
	    return source;
	source += 1;
	}
    }
return NULL;
}

static void mergeAdd(struct binKeeper *bk, int start, int end, struct gfSeqSource *src)
/* Add interval to bin-keeper, merging with any existing overlapping
 * intervals. */
{
struct binElement *iEl, *iList = binKeeperFind(bk, start, end);
for (iEl = iList; iEl != NULL; iEl = iEl->next)
    {
    if (iEl->start < start)
        start = iEl->start;
    if (iEl->end > end)
        end = iEl->end;
    binKeeperRemove(bk, iEl->start, iEl->end, src);
    }
slFreeList(&iList);
binKeeperAdd(bk, start, end, src);
}

static struct gfClump *pcrClumps(struct genoFind *gf, char *fPrimer, int fPrimerSize, 
	char *rPrimer, int rPrimerSize, int minDistance, int maxDistance)
/* Find possible PCR hits.  The fPrimer and rPrimer are on the same strand. */
{
struct gfClump *clumpList = NULL;
int tileSize = gf->tileSize;
int fTile;
int fTileCount = fPrimerSize - tileSize;
int *rTiles, rTile;
int rTileCount = rPrimerSize - tileSize;
int fTileIx,rTileIx,fPosIx,rPosIx;
bits32 *fPosList, fPos, *rPosList, rPos;
int fPosListSize, rPosListSize;
struct hash *targetHash = newHash(0);

/* Build up array of all tiles in reverse primer. */
AllocArray(rTiles, rTileCount);
for (rTileIx = 0; rTileIx<rTileCount; ++rTileIx)
    {
    rTiles[rTileIx] = gfDnaTile(rPrimer + rTileIx, tileSize);
    if (rTiles[rTileIx] == -1)
        errAbort("Bad char in reverse primer sequence: %s", rPrimer);
    }

/* Loop through all tiles in forward primer. */
for (fTileIx=0; fTileIx<fTileCount; ++fTileIx)
    {
    fTile = gfDnaTile(fPrimer + fTileIx, tileSize);
    if (fTile >= 0)
        {
	fPosListSize = gf->listSizes[fTile];
	fPosList = gf->lists[fTile];
	for (fPosIx=0; fPosIx < fPosListSize; ++fPosIx)
	    {
	    fPos = fPosList[fPosIx];
	    /* Loop through hits to reverse primer. */
	    for (rTileIx=0; rTileIx < rTileCount; ++rTileIx)
	        {
		rTile = rTiles[rTileIx];
		rPosListSize = gf->listSizes[rTile];
		rPosList = gf->lists[rTile];
		for (rPosIx=0; rPosIx < rPosListSize; ++rPosIx)
		    {
		    rPos = rPosList[rPosIx];
		    if (rPos > fPos)
		        {
			int distance = rPos - fPos;
			if (distance >= minDistance && distance <= maxDistance)
			    {
			    struct gfSeqSource *target = findSource(gf, fPos);
			    if (rPos < target->end)
			        {
				struct binKeeper *bk;
				int tStart = target->start;
				char *tName = target->fileName;
				if (tName == NULL)
				    tName = target->seq->name;
				if ((bk = hashFindVal(targetHash, tName)) == NULL)
				    {
				    bk = binKeeperNew(0, target->end - tStart);
				    hashAdd(targetHash, tName, bk);
				    }
				mergeAdd(bk, fPos - tStart, rPos - tStart, target);
				}
			    }
			}
		    }
		}
	    }
	}
    }

/* Make clumps and clean up temporary structures. */
    {
    struct hashEl *tList, *tEl;
    tList = hashElListHash(targetHash);
    for (tEl = tList; tEl != NULL; tEl = tEl->next)
        {
	struct binKeeper *bk = tEl->val;
	struct binElement *bkList, *bkEl;
	bkList = binKeeperFindAll(bk);
	for (bkEl = bkList; bkEl != NULL; bkEl = bkEl->next)
	    {
	    struct gfClump *clump;
	    AllocVar(clump);
	    clump->target = bkEl->val;
	    clump->tStart = bkEl->start;
	    clump->tEnd = bkEl->end;
	    slAddHead(&clumpList, clump);
	    }
	slFreeList(&bkList);
	binKeeperFree(&bk);
	}
    hashElFreeList(&tList);
    hashFree(&targetHash);
    }
freez(&rTiles);
return clumpList;	
}

struct gfClump *gfPcrClumps(struct genoFind *gf, char *fPrimer, int fPrimerSize, 
	char *rPrimer, int rPrimerSize, int minDistance, int maxDistance)
/* Find possible PCR hits.  The fPrimer and rPrimer are on opposite strands. */
{
struct gfClump *clumpList;
if (gf->segSize > 0)
    errAbort("Can't do PCR on large tile sizes");
if (gf->isPep)
    errAbort("Can't do PCR on protein/translated index");
tolowers(fPrimer);
tolowers(rPrimer);
reverseComplement(rPrimer, rPrimerSize);
clumpList = pcrClumps(gf, fPrimer, fPrimerSize, rPrimer, rPrimerSize, 
	minDistance, maxDistance);
reverseComplement(rPrimer, rPrimerSize);
return clumpList;
}

