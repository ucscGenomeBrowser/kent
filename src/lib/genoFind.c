/* genoFind - Quickly find where DNA occurs in genome.. */
#include "common.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "nib.h"
#include "fa.h"
#include "dystring.h"
#include "errabort.h"
#include "ooc.h"
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

int gfPowerOf20(int n)
/* Return a 20 to the n */
{
int res = 20;
while (--n > 0)
    res *= 20;
return res;
}

static struct genoFind *gfNewEmpty(int minMatch, int maxGap, int tileSize, int maxPat,
	char *oocFile, boolean isPep)
/* Return an empty pattern space. oocFile parameter may be NULL*/
{
struct genoFind *gf;
int tileSpaceSize;

if (isPep)
    {
    if (tileSize < 3 || tileSize > 6)
	errAbort("Seed size must be between 3 and 6");
    }
else
    {
    if (tileSize < 8 || tileSize > 14)
	errAbort("Seed size must be between 8 and 14");
    }
AllocVar(gf);
gf->tileSize = tileSize;
gf->isPep = isPep;
if (isPep)
    tileSpaceSize = gf->tileSpaceSize = gfPowerOf20(tileSize);
else
    {
    int seedBitSize = tileSize*2;
    tileSpaceSize = gf->tileSpaceSize = (1<<seedBitSize);
    gf->tileMask = tileSpaceSize-1;
    }
gf->lists = needHugeZeroedMem(tileSpaceSize * sizeof(gf->lists[0]));
gf->listSizes = needHugeZeroedMem(tileSpaceSize * sizeof(gf->listSizes[0]));
gf->minMatch = minMatch;
gf->maxGap = maxGap;
gf->maxPat = maxPat;
if (oocFile != NULL)
    {
    oocMaskCounts(oocFile, gf->listSizes, tileSize, maxPat);
    oocMaskSimpleRepeats(gf->listSizes, tileSize, maxPat);
    }
return gf;
}

int gfDnaTile(DNA *dna, int n)
/* Make a packed DNA n-mer. */
{
int tile = 0;
int i;
for (i=0; i<n; ++i)
    {
    tile <<= 2;
    tile += ntValNoN[dna[i]];
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
    aa = aaVal[*pep++];
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
int maxPat = gf->maxPat;
int tile;
bits32 *listSizes = gf->listSizes;
int i, lastTile = seq->size - tileSize;
int (*makeTile)(char *poly, int n) = (gf->isPep ? gfPepTile : gfDnaTile);

for (i=0; i<=lastTile; i += tileSize)
    {
    if ((tile = makeTile(poly, tileSize)) >= 0)
	{
        if (listSizes[tile] < maxPat)
	    {
	    listSizes[tile] += 1;
	    }
	}
    poly += tileSize;
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

static int gfAllocLists(struct genoFind *gf)
/* Allocate pat space lists and set up list pointers. 
 * Returns size of all lists. */
{
int oneCount;
int count = 0;
int i;
bits32 *listSizes = gf->listSizes;
bits32 **lists = gf->lists;
bits32 *allocated;
int ignoreCount = 0;
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

#ifdef OLD
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
#endif

static void gfAddSeq(struct genoFind *gf, bioSeq *seq, bits32 offset)
/* Add all N-mers in seq. */
{
char *poly = seq->dna;
int tileSize = gf->tileSize;
int i, lastTile = seq->size - tileSize;
int (*makeTile)(char *poly, int n) = (gf->isPep ? gfPepTile : gfDnaTile);
int maxPat = gf->maxPat;
int tile;
bits32 *listSizes = gf->listSizes;
bits32 **lists = gf->lists;
int patCount;

for (i=0; i<=lastTile; i += tileSize)
    {
    tile = makeTile(poly, tileSize);
    if (tile >= 0)
        {
	if (listSizes[tile] < maxPat)
	    {
	    lists[tile][listSizes[tile]++] = offset;
	    }
	}
    offset += tileSize;
    poly += tileSize;
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


struct genoFind *gfIndexNibs(int nibCount, char *nibNames[],
	int minMatch, int maxGap, int tileSize, int maxPat, char *oocFile)
/* Make index for all seqs in list. 
 *      minMatch - minimum number of matching tiles to trigger alignments
 *      maxGap   - maximum deviation from diagonal of tiles
 *      tileSize - size of tile in nucleotides
 *      maxPat   - maximum use of tile to not be considered a repeat
 *      oocFile  - .ooc format file that lists repeat tiles.  May be NULL. */
{
struct genoFind *gf = gfNewEmpty(minMatch, maxGap, tileSize, maxPat, oocFile, FALSE);
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

void gfIndexTransNibs(struct genoFind *transGf[2][3], int nibCount, char *nibNames[], 
    int minMatch, int maxGap, int tileSize, int maxPat)
/* Make translated (6 frame) index for all nib files. */
{
uglyf("gfIndexTransNibs\n");
uglyAbort("All for now\n");
}

struct genoFind *gfIndexSeq(bioSeq *seqList,
	int minMatch, int maxGap, int tileSize, int maxPat, char *oocFile, boolean isPep)
/* Make index for all seqs in list. */
{
struct genoFind *gf = gfNewEmpty(minMatch, maxGap, tileSize, maxPat, oocFile, isPep);
int seqCount = slCount(seqList);
bioSeq *seq;
int i;
bits32 offset = 0, nibSize;
char *nibName;
struct gfSeqSource *ss;

for (seq = seqList; seq != NULL; seq = seq->next)
    {
    gfCountSeq(gf, seq);
    }
gfAllocLists(gf);
gfZeroNonOverused(gf);
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
    // if (hit != NULL) uglyf(" %d %d (%d)\n", hit->qStart, hit->tStart, hit->diagonal);
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
#ifdef DEBUG
uglyf("Dumping clumps\n");
for (clump = clumpList; clump != NULL; clump = clump->next)	/* uglyf */
    {
    uglyf(" %d %d %s %d %d (%d hits)\n", clump->qStart, clump->qEnd, clump->target->seq->name,   clump->tStart, clump->tEnd, clump->hitCount);
    }
#endif /* DEBUG */
return clumpList;
}


struct gfClump *gfFindDnaClumps(struct genoFind *gf, struct dnaSeq *seq)
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
cmpQuerySize = seq->size;
slSort(&hitList, gfHitCmpDiagonal);
return clumpHits(gf, hitList, minMatch);
}

struct gfClump *gfFindPepClumps(struct genoFind *gf, aaSeq *seq)
/* Find clumps associated with one sequence. */
{
struct gfHit *hitList = NULL, *hit;
int size = seq->size;
int tileSize = gf->tileSize;
int lastStart = size - tileSize;
int tileSizeMinusOne = tileSize - 1;
int mask = gf->tileMask;
AA *pep = seq->dna;
int i, j;
int tile;
bits32 bVal;
int listSize;
bits32 qStart, tStart, *tList;
int minMatch = gf->minMatch;

for (i=0; i<=lastStart; ++i)
    {
    tile = gfPepTile(pep+i, tileSize);
    if (tile < 0)
        continue;
    listSize = gf->listSizes[tile];
    qStart = i;
    tList = gf->lists[tile];
    for (j=0; j<listSize; ++j)
        {
	AllocVar(hit);
	hit->qStart = qStart;
	hit->tStart = tList[j];
	hit->diagonal = hit->tStart + seq->size - qStart;
	slAddHead(&hitList, hit);
	}
    }
cmpQuerySize = seq->size;
slSort(&hitList, gfHitCmpDiagonal);
return clumpHits(gf, hitList, minMatch);
}

struct gfClump *gfFindClumps(struct genoFind *gf, bioSeq *seq)
/* Find clump whether its peptide or dna. */
{
if (gf->isPep)
    return gfFindPepClumps(gf, seq);
else
    return gfFindDnaClumps(gf, seq);
}

void gfTransFindClumps(struct genoFind *gfs[3], aaSeq *seq, struct gfClump *clumps[3])
/* Find clumps associated with one sequence in three translated reading frames. */
{
int frame;
for (frame = 0; frame < 3; ++frame)
    clumps[frame] = gfFindPepClumps(gfs[frame], seq);
}

void gfTransTransFindClumps(struct genoFind *gfs[3], aaSeq *seqs[3], 
	struct gfClump *clumps[3][3])
/* Find clumps associated with three sequences in three translated 
 * reading frames. Used for translated/translated protein comparisons. */
{
int qFrame;

for (qFrame = 0; qFrame<3; ++qFrame)
    gfTransFindClumps(gfs, seqs[qFrame], clumps[qFrame]);
}

