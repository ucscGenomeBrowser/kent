/* genoFind - Quickly find where DNA occurs in genome.. */
#include "common.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "nib.h"
#include "fa.h"
#include "dystring.h"
#include "errabort.h"
#include "genoFind.h"

char *gfSignature()
/* Return signature that starts each command to gfServer. Helps defend 
 * server from confused clients. */
{
static char signature[] = "0ddf270562684f29";
return signature;
}

void gfSendString(int sd, char *s)
/* Send a string down a socket - length byte first. */
{
int length = strlen(s);
UBYTE len;

if (length > 255)
    errAbort("Trying to send a string longer than 255 bytes (%d byte)", length);
len = length;
if (write(sd, &len, 1)<0)
    errnoAbort("Couldn't send string to socket");
if (write(sd, s, length)<0)
    errnoAbort("Couldn't send string to socket");
}

int gfReadMulti(int sd, void *vBuf, size_t size)
/* Read in until all is read or there is an error. */
{
char *buf = vBuf;
size_t totalRead = 0;
size_t oneRead;

while (totalRead < size)
    {
    oneRead = read(sd, buf + totalRead, size - totalRead);
    if (oneRead < 0)
        {
	perror("Couldn't finish large read");
	return oneRead;
	}
    totalRead += oneRead;
    }
return totalRead;
}

char *gfGetString(int sd, char buf[256])
/* Read string into buf and return it.  If buf is NULL
 * an internal buffer will be used. Print warning message
 * and return NULL if any problem. */
{
static char sbuf[256];
UBYTE len;
int length;
if (buf == NULL) buf = sbuf;
if (read(sd, &len, 1)<0)
    {
    warn("Couldn't sockRecieveString string from socket");
    return NULL;
    }
length = len;
if (length > 0)
    if (gfReadMulti(sd, buf, length) < 0)
	{
	warn("Couldn't sockRecieveString string from socket");
	return NULL;
	}
buf[length] = 0;
return buf;
}

char *gfRecieveString(int sd, char buf[256])
/* Read string into buf and return it.  If buf is NULL
 * an internal buffer will be used. Abort if any problem. */
{
char *s = gfGetString(sd, buf);
if (s == NULL)
     noWarnAbort();   
}

void genoFindFree(struct genoFind **pGenoFind)
/* Free up a genoFind index. */
{
struct genoFind *gf = *pGenoFind;
if (gf != NULL)
    {
    freeMem(gf->lists);
    freeMem(gf->listSizes);
    freeMem(gf->allocated);
    freeMem(gf->sources);
    freez(pGenoFind);
    }
}

static struct genoFind *gfNewEmpty(int minMatch, int maxGap, int tileSize, int maxPat)
/* Return an empty pattern space. */
{
struct genoFind *gf;
int seedBitSize = tileSize*2;
int tileSpaceSize;

if (tileSize < 8 || tileSize > 14)
    errAbort("Seed size must be between 8 and 14");
AllocVar(gf);
gf->tileSize = tileSize;
tileSpaceSize = gf->tileSpaceSize = (1<<seedBitSize);
gf->tileMask = tileSpaceSize-1;
gf->lists = needHugeZeroedMem(tileSpaceSize * sizeof(gf->lists[0]));
gf->listSizes = needHugeZeroedMem(tileSpaceSize * sizeof(gf->listSizes[0]));
gf->minMatch = minMatch;
gf->maxGap = maxGap;
gf->maxPat = maxPat;
return gf;
}

int gfPowerOf20(int n)
/* Return a 20 to the n */
{
int res = 20;
while (--n > 0)
    res *= 20;
return res;
}

static struct genoFind *gfPepNewEmpty(int minMatch, int maxGap, int tileSize, int maxPat)
/* Return an empty peptide pattern space. */
{
struct genoFind *gf;
int seedBitSize = tileSize*2;
int tileSpaceSize;

if (tileSize < 3 || tileSize > 6)
    errAbort("Seed size must be between 3 and 6");
AllocVar(gf);
gf->tileSize = tileSize;
tileSpaceSize = gf->tileSpaceSize = gfPowerOf20(tileSize);
gf->lists = needHugeZeroedMem(tileSpaceSize * sizeof(gf->lists[0]));
gf->listSizes = needHugeZeroedMem(tileSpaceSize * sizeof(gf->listSizes[0]));
gf->minMatch = minMatch;
gf->maxGap = maxGap;
gf->maxPat = maxPat;
return gf;
}


void gfCountTile(struct genoFind *gf, DNA *dna)
/* Add N-mer to count. */
{
bits32 tile = 0;
int i, n = gf->tileSize;
for (i=0; i<n; ++i)
    {
    tile <<= 2;
    tile += ntValNoN[dna[i]];
    }
if (gf->listSizes[tile] < gf->maxPat)
    ++gf->listSizes[tile];
}

static void gfCountSeq(struct genoFind *gf, struct dnaSeq *seq)
/* Add all N-mers in seq. */
{
DNA *dna = seq->dna;
int tileSize = gf->tileSize;
int i, lastTile = seq->size - tileSize;

for (i=0; i<=lastTile; i += tileSize)
    {
    gfCountTile(gf, dna);
    dna += tileSize;
    }
}

static void gfCountTilesInNib(struct genoFind *gf, char *nibName)
/* Count all tiles in nib file. */
{
FILE *f;
int tileSize = gf->tileSize;
int bufSize = tileSize * 16*1024;
int nibSize, i;
int endBuf, basesInBuf;
struct dnaSeq *seq;

printf("Counting tiles in %s\n", nibName);
nibOpenVerify(nibName, &f, &nibSize);
for (i=0; i < nibSize; i = endBuf)
    {
    endBuf = i + bufSize;
    if (endBuf >= nibSize) endBuf = nibSize;
    basesInBuf = endBuf - i;
    seq = nibLdPart(nibName, f, nibSize, i, basesInBuf);
    gfCountSeq(gf, seq);
    freeDnaSeq(&seq);
    }
fclose(f);
}

int gfPepTile(DNA *dna, int n)
/* Make up packed representation of translated protein. */
{
int tile = 0;
int aa;
while (--n >= 0)
    {
    tile *= 20;
    aa = lookupCodon(dna);
    if (aa == 0 || aa == 'X')
        return -1;
    tile += aaVal[aa];
    dna += 3;
    }
return tile;
}

void gfPepCountTile(struct genoFind *gf, DNA *dna)
/* Add peptide N-mer to count. */
{
int tile = gfPepTile(dna, gf->tileSize);

if (tile >= 0 && gf->listSizes[tile] < gf->maxPat)
    ++gf->listSizes[tile];
}


static void gfPepCountSeq(struct genoFind *gf, struct dnaSeq *seq)
/* Add all N-mers in seq. */
{
DNA *dna = seq->dna;
int ntTileSize = gf->tileSize*3;
int i, lastTile = seq->size - ntTileSize;

for (i=0; i<=lastTile; i += ntTileSize)
    {
    gfPepCountTile(gf, dna);
    dna += ntTileSize;
    }
}


static void gfPepCountTilesInNib(struct genoFind *gf, char *nibName)
/* Count all peptide tiles in nib file. */
{
FILE *f;
int tileSize = gf->tileSize;
int bufSize = tileSize * 3 * 8 * 1024;
int nibSize, i;
int endBuf, basesInBuf;
struct dnaSeq *seq;
int frame;

printf("Counting tiles in %s\n", nibName);
nibOpenVerify(nibName, &f, &nibSize);
for (frame = 0; frame < 3; ++frame)
    {
    for (i=frame; i < nibSize; i = endBuf)
	{
	endBuf = i + bufSize;
	if (endBuf >= nibSize) endBuf = nibSize;
	basesInBuf = endBuf - i;
	seq = nibLdPart(nibName, f, nibSize, i, basesInBuf);
	gfPepCountSeq(gf, seq);
	freeDnaSeq(&seq);
	}
    fclose(f);
    }
}


static int gfAllocLists(struct genoFind *gf)
/* Allocate pat space lists and set up list pointers. 
 * Returns size of all lists. */
{
int oneCount;
int count = 0;
int i;
bits16 *listSizes = gf->listSizes;
bits32 **lists = gf->lists;
bits32 *allocated;
int ignoreCount = 0;
bits16 maxPat = gf->maxPat;
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

void gfAddTile(struct genoFind *gf, DNA *dna, int offset)
/* Add N-mer to tile index. */
{
bits32 tile = 0;
int i, n = gf->tileSize;
for (i=0; i<n; ++i)
    {
    tile <<= 2;
    tile += ntValNoN[dna[i]];
    }
if (gf->listSizes[tile] < gf->maxPat)
    gf->lists[tile][gf->listSizes[tile]++] = offset;
}

void gfPepAddTile(struct genoFind *gf, DNA *dna, int offset)
/* Add peptide N-mer to tile index. */
{
int tile = gfPepTile(dna, gf->tileSize);
if (tile >= 0)
    {
    if (gf->listSizes[tile] < gf->maxPat)
	gf->lists[tile][gf->listSizes[tile]++] = offset;
    }
}

void gfAddSeq(struct genoFind *gf, struct dnaSeq *seq, bits32 offset)
/* Add all N-mers in seq. */
{
DNA *dna = seq->dna;
int tileSize = gf->tileSize;
int i, lastTile = seq->size - tileSize;

for (i=0; i<=lastTile; i += tileSize)
    {
    gfAddTile(gf, dna, offset);
    offset += tileSize;
    dna += tileSize;
    }
}

void gfPepAddSeq(struct genoFind *gf, struct dnaSeq *seq, bits32 offset)
/* Add all N-mers in seq. */
{
DNA *dna = seq->dna;
int ntTileSize = gf->tileSize*3;
int i, lastTile = seq->size - ntTileSize;

for (i=0; i<=lastTile; i += ntTileSize)
    {
    gfPepAddTile(gf, dna, offset);
    offset += ntTileSize;
    dna += ntTileSize;
    }
}

static int gfAddTilesInNib(struct genoFind *gf, char *nibName, bits32 offset)
/* Add all tiles in nib file.  Returns size of nib file. */
{
FILE *f;
int tileSize = gf->tileSize;
int bufSize = tileSize * 16*1024;
int nibSize, i;
int endBuf, basesInBuf;
struct dnaSeq *seq;

printf("Adding tiles in %s\n", nibName);
nibOpenVerify(nibName, &f, &nibSize);
for (i=0; i < nibSize; i = endBuf)
    {
    endBuf = i + bufSize;
    if (endBuf >= nibSize) endBuf = nibSize;
    basesInBuf = endBuf - i;
    seq = nibLdPart(nibName, f, nibSize, i, basesInBuf);
    gfAddSeq(gf, seq, i + offset);
    freeDnaSeq(&seq);
    }
fclose(f);
return nibSize;
}

static int gfPepAddTilesInNib(struct genoFind *gf, char *nibName, bits32 offset)
/* Add all peptide tiles in nib file. ~~~ Untested! */
{
FILE *f;
int tileSize = gf->tileSize;
int bufSize = tileSize * 3 * 8 * 1024;
int nibSize, i;
int endBuf, basesInBuf;
struct dnaSeq *seq;
int frame;

printf("Adding tiles in %s\n", nibName);
nibOpenVerify(nibName, &f, &nibSize);
for (frame = 0; frame < 3; ++frame)
    {
    for (i=frame; i < nibSize; i = endBuf)
	{
	endBuf = i + bufSize;
	if (endBuf >= nibSize) endBuf = nibSize;
	basesInBuf = endBuf - i;
	seq = nibLdPart(nibName, f, nibSize, i, basesInBuf);
	gfPepAddSeq(gf, seq, i+offset);
	freeDnaSeq(&seq);
	}
    fclose(f);
    }
printf("Done adding\n");
return nibSize;
}



void gfZeroOverused(struct genoFind *gf)
/* Zero out counts of overused tiles. */
{
bits16 *sizes = gf->listSizes;
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
printf("Got %d overused %d-mers\n", overCount, gf->tileSize);
}

static void gfZeroNonOverused(struct genoFind *gf)
/* Zero out counts of non-overused tiles. */
{
bits16 *sizes = gf->listSizes;
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


struct genoFind *gfIndexNibs(int nibCount, char *nibNames[],
	int minMatch, int maxGap, int tileSize, int maxPat)
/* Make index for all nib files. */
{
struct genoFind *gf = gfNewEmpty(minMatch, maxGap, tileSize, maxPat);
int i;
bits32 offset = 0, nibSize;
char *nibName;
struct gfSeqSource *ss;

for (i=0; i<nibCount; ++i)
    gfCountTilesInNib(gf, nibNames[i]);
gfAllocLists(gf);
gfZeroNonOverused(gf);
AllocArray(gf->sources, nibCount);
gf->sourceCount = nibCount;
for (i=0; i<nibCount; ++i)
    {
    nibName = nibNames[i];
    nibSize = gfAddTilesInNib(gf, nibName, offset);
    ss = gf->sources+i;
    ss->fileName = nibName;
    ss->start = offset;
    offset += nibSize;
    ss->end = offset;
    }
printf("Done adding\n");
gfZeroOverused(gf);
return gf;
}

struct genoFind *gfPepIndexNibs(int nibCount, char *nibNames[],
	int minMatch, int maxGap, int tileSize, int maxPat)
/* Make index for all nib files. */
{
struct genoFind *gf = gfPepNewEmpty(minMatch, maxGap, tileSize, maxPat);
int i;
bits32 offset = 0, nibSize;
char *nibName;
struct gfSeqSource *ss;

for (i=0; i<nibCount; ++i)
    gfPepCountTilesInNib(gf, nibNames[i]);
gfAllocLists(gf);
gfZeroNonOverused(gf);
AllocArray(gf->sources, nibCount);
gf->sourceCount = nibCount;
for (i=0; i<nibCount; ++i)
    {
    nibName = nibNames[i];
    nibSize = gfPepAddTilesInNib(gf, nibName, offset);
    ss = gf->sources+i;
    ss->fileName = nibName;
    ss->start = offset;
    offset += nibSize;
    ss->end = offset;
    }
gfZeroOverused(gf);
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
slFreeList(&clump->hitList);
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
struct gfHit *hit;
struct gfSeqSource *ss = clump->target;

fprintf(f, "%u-%u %s %u-%u, hits %d\n", 
	clump->qStart, clump->qEnd, ss->fileName,
	clump->tStart - ss->start, clump->tEnd - ss->start,
	clump->hitCount);
#ifdef SOMETIMES
for (hit = clump->hitList; hit != NULL; hit = hit->next)
    fprintf(f, "   q %d, t %d, diag %d\n", hit->qStart, hit->tStart, hit->diagonal);
#endif
}

static int cmpQuerySize;

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


static int gfClumpCmpHitCount(const void *va, const void *vb)
/* Compare to sort based on hit count. */
{
const struct gfClump *a = *((struct gfClump **)va);
const struct gfClump *b = *((struct gfClump **)vb);

return (b->hitCount - a->hitCount);
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
    int start, end;
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
	    slFreeList(&inList);
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

static struct gfClump *clumpHits(struct genoFind *gf, struct gfHit *hitList, int minMatch)
/* Clump together hits according to parameters in gf. */
{
struct gfClump *clumpList = NULL, *clump = NULL;
int maxGap = gf->maxGap;
struct gfHit *clumpStart = NULL, *hit, *nextHit, *lastHit = NULL;
int clumpSize = 0;
int tileSize = gf->tileSize;

for (hit = hitList; ; hit = nextHit)
    {
    /* See if time to finish old clump/start new one. */
    if (lastHit == NULL || hit == NULL || hit->diagonal - lastHit->diagonal > maxGap)
        {
	if (clumpSize >= minMatch)
	    {
	    clump->hitCount = clumpSize;
	    findClumpBounds(clump, tileSize);
	    targetClump(gf, &clumpList, clump);
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


struct gfClump *gfFindClumps(struct genoFind *gf, struct dnaSeq *seq)
/* Find clumps associated with one sequence. */
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
bits32 qStart, tStart, *tList;
int minMatch = gf->minMatch;

if (size < (minMatch+1) * (gf->tileSize+1))
    {
    minMatch = size/(gf->tileSize+1) - 1;
    if (minMatch < 2)
	minMatch = 2;
    }
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
    listSize = gf->listSizes[bits];
    qStart = i-tileSizeMinusOne;
    tList = gf->lists[bits];
    for (j=0; j<listSize; ++j)
        {
	AllocVar(hit);
	hit->qStart = qStart;
	hit->tStart = tList[j];
	hit->diagonal = hit->tStart + seq->size - qStart;
	slAddHead(&hitList, hit);
	}
    }
// uglyf("Got %d hits\n", slCount(hitList));
cmpQuerySize = seq->size;
slSort(&hitList, gfHitCmpDiagonal);
return clumpHits(gf, hitList, minMatch);
}

