/* gfClientLib - stuff to interface with a genoFind server running somewhere
 * on the web. */
#include "common.h"
#include "linefile.h"
#include "sqlNum.h"
#include "fa.h"
#include "fuzzyFind.h"
#include "supStitch.h"
#include "genoFind.h"
#include "errabort.h"
#include "nib.h"
#include "trans3.h"

struct gfRange
/* A range of bases found by genoFind.  Recursive
 * data structure.  Lowest level roughly corresponds
 * to an exon. */
    {
    struct gfRange *next;  /* Next in singly linked list. */
    int qStart;	/* Start in query */
    int qEnd;	/* End in query */
    char *tName;	/* Target name */
    struct dnaSeq *tSeq;	/* Target sequence. (May be NULL if in .nib.  Not allocated here.) */
    int tStart;	/* Start in target */
    int tEnd;	/* End in target */
    struct gfRange *components;	/* Components of range. */
    int hitCount;	/* Number of hits. */
    int frame;		/* Reading frame (just for translated alignments) */
    struct trans3 *t3;	/* Translated frame or NULL. */
    };

static void gfRangeFreeList(struct gfRange **pList);
/* Free a list of dynamically allocated gfRange's */

static void gfRangeFree(struct gfRange **pEl)
/* Free a single dynamically allocated gfRange such as created
 * with gfRangeLoad(). */
{
struct gfRange *el;

if ((el = *pEl) == NULL) return;
freeMem(el->tName);
if (el->components != NULL)
    gfRangeFreeList(&el->components);
freez(pEl);
}

static void gfRangeFreeList(struct gfRange **pList)
/* Free a list of dynamically allocated gfRange's */
{
struct gfRange *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gfRangeFree(&el);
    }
*pList = NULL;
}

static struct gfRange *gfRangeLoad(char **row)
/* Load a gfRange from row fetched with select * from gfRange
 * from database.  Dispose of this with gfRangeFree(). */
{
struct gfRange *ret;
int sizeOne,i;
char *s;

AllocVar(ret);
ret->qStart = atoi(row[0]);
ret->qEnd = atoi(row[1]);
ret->tName = cloneString(row[2]);
ret->tStart = atoi(row[3]);
ret->tEnd = atoi(row[4]);
ret->hitCount = atoi(row[5]);
return ret;
}

static int gfRangeCmpTarget(const void *va, const void *vb)
/* Compare to sort based on hit count. */
{
const struct gfRange *a = *((struct gfRange **)va);
const struct gfRange *b = *((struct gfRange **)vb);
int diff;

diff = strcmp(a->tName, b->tName);
if (diff == 0)
    diff = a->tStart - b->tStart;
return diff;
}

static void startSeqQuery(int conn, bioSeq *seq, char *type)
/* Send a query that involves some sequence. */
{
char buf[256];
sprintf(buf, "%s%s %d", gfSignature(), type, seq->size);
write(conn, buf, strlen(buf));
read(conn, buf, 1);
if (buf[0] != 'Y')
    errAbort("Expecting 'Y' from server, got %c", buf[0]);
write(conn, seq->dna, seq->size);
}


static struct gfRange *gfQuerySeq(int conn, struct dnaSeq *seq)
/* Ask server for places sequence hits. */
{
struct gfRange *rangeList = NULL, *range;
char buf[256], *row[6];
int rowSize;

startSeqQuery(conn, seq, "query");

/* Read results line by line and save in list, and return. */
for (;;)
    {
    gfRecieveString(conn, buf);
    if (sameString(buf, "end"))
	{
	break;
	}
    else
	{
	rowSize = chopLine(buf, row);
	if (rowSize < 6)
	    errAbort("Expecting 6 words from server got %d", rowSize);
	range = gfRangeLoad(row);
	slAddHead(&rangeList, range);
	}
    }
slReverse(&rangeList);
return rangeList;
}

static void gfQuerySeqTrans(int conn, aaSeq *seq, struct gfClump *clumps[2][3], struct gfSeqSource **retSsList, int *retTileSize)
/* Query server for clumps where aa sequence hits translated index. */
{
int frame, isRc, rowSize;
struct gfClump *clump;
struct gfHit *hit;
int tileSize = 4;
char *line, *var, *val, *word;
char *s = NULL;
char buf[256], *row[12];
struct gfSeqSource *ssList = NULL, *ss;

for (isRc = 0; isRc <= 1; ++isRc)
    for (frame = 0; frame<3; ++frame)
	clumps[isRc][frame] = NULL;

/* Send sequence to server. */
startSeqQuery(conn, seq, "protQuery");

/* Parse first line from server: var/val pairs. */
line = gfRecieveString(conn, buf);
for (;;)
    {
    var = nextWord(&line);
    if (var == NULL)
         break;
    val = nextWord(&line);
    if (val == NULL)
	 {
	 internalErr();
         break;
	 }
    if (sameString("tileSize", var))
         {
	 tileSize = atoi(val);
	 if (tileSize <= 0)
	     internalErr();
	 }
    }


/* Read results line by line and save in memory. */
for (;;)
    {
    /* Read and parse first line that describes clump overall. */
    gfRecieveString(conn, buf);
    if (sameString(buf, "end"))
	{
	break;
	}
    rowSize = chopLine(buf, row);
    if (rowSize < 8)
	errAbort("Expecting 8 words from server got %d", rowSize);
    AllocVar(clump);
    clump->qStart = sqlUnsigned(row[0]);
    clump->qEnd = sqlUnsigned(row[1]);
    AllocVar(ss);
    ss->fileName = cloneString(row[2]);
    slAddHead(&ssList, ss);
    clump->target = ss;
    clump->tStart = sqlUnsigned(row[3]);
    clump->tEnd = sqlUnsigned(row[4]);
    clump->hitCount = sqlUnsigned(row[5]);
    isRc = ((row[6][0] == '-') ? 1 : 0);
    frame = sqlUnsigned(row[7]);
    slAddHead(&clumps[isRc][frame], clump);

    /* Read and parse next (long) line that describes hits. */
    s = line = gfRecieveLongString(conn);
    for (;;)
        {
	if ((row[0] = nextWord(&line)) == NULL)
	     break;
	if ((row[1] = nextWord(&line)) == NULL)
	     internalErr();
	AllocVar(hit);
	hit->qStart = sqlUnsigned(row[0]);
	hit->tStart = sqlUnsigned(row[1]);
	slAddHead(&clump->hitList, hit);
	}
    slReverse(&clump->hitList);
    assert(slCount(clump->hitList) == clump->hitCount);
    freez(&s);
    }
for (isRc = 0; isRc <= 1; ++isRc)
    for (frame = 0; frame<3; ++frame)
	slReverse(&clumps[isRc][frame]);
*retSsList = ssList;
*retTileSize = tileSize;
}


static struct gfRange *gfRangesBundle(struct gfRange *exonList, int maxIntron)
/* Bundle a bunch of 'exons' into plausable 'genes'.  It's
 * not necessary to be precise here.  The main thing is to
 * group together exons that are close to each other in the
 * same target sequence. */
{
struct gfRange *geneList = NULL, *gene = NULL, *lastExon = NULL, *exon, *nextExon;

for (exon = exonList; exon != NULL; exon = nextExon)
    {
    nextExon = exon->next;
    if (lastExon == NULL || !sameString(lastExon->tName, exon->tName) 
        || exon->tStart - lastExon->tEnd > maxIntron)
	{
	AllocVar(gene);
	gene->tStart = exon->tStart;
	gene->tEnd = exon->tEnd;
	gene->tName = cloneString(exon->tName);
	gene->tSeq = exon->tSeq;
	gene->qStart = exon->qStart;
	gene->qEnd = exon->qEnd;
	gene->hitCount = exon->hitCount;
	gene->t3 = exon->t3;
	slAddHead(&gene->components, exon);
	slAddHead(&geneList, gene);
	}
    else
        {
	if (exon->qStart < gene->qStart) gene->qStart = exon->qStart;
	if (exon->qEnd > gene->qEnd) gene->qEnd = exon->qEnd;
	if (exon->tStart < gene->tStart) gene->tStart = exon->tStart;
	if (exon->tEnd > gene->tEnd) gene->tEnd = exon->tEnd;
	gene->hitCount += exon->hitCount;
	slAddTail(&gene->components, exon);
	}
    lastExon = exon;
    }
slReverse(&geneList);
return geneList;
}

static int extendRespect(int oldX, int newX)
/* Return newX modified slightly so as to be in same frame as oldX. */
{
int frame = oldX % 3;
newX = newX - (newX % 3) + frame;
return newX;
}

static void expandRange(struct gfRange *range, int qSize, int tSize, boolean respectFrame)
/* Expand range to cover an additional 500 bases on either side. */
{
int extra = 500;
int x;

x = range->qStart - extra;
if (x < 0) x = 0;
range->qStart = x;

x = range->qEnd + extra;
if (x > qSize) x = qSize;
range->qEnd = x;

x = range->tStart - extra;
if (x < 0) x = 0;
if (respectFrame) 
    {
    x = extendRespect(range->tStart, x);
    }
range->tStart = x;

x = range->tEnd + extra;
if (x > tSize) x = tSize;
if (respectFrame)
    {
    x = extendRespect(range->tEnd, x);
    if (x > tSize)
        x -= 3;
    }
range->tEnd = x;
}

static struct dnaSeq *expandAndLoad(struct gfRange *range, char *nibDir, int querySize, int *retNibSize, boolean respectFrame, boolean isRc)
/* Expand range to cover an additional 500 bases on either side.
 * Load up target sequence and return. (Done together because don't
 * know target size before loading.) */
{
struct dnaSeq *target = NULL;
char nibFileName[512];
FILE *f = NULL;
int nibSize;
int s, e;

sprintf(nibFileName, "%s/%s", nibDir, range->tName);
nibOpenVerify(nibFileName, &f, &nibSize);
if (isRc)
    {
    s = nibSize - range->tEnd;
    e = nibSize - range->tStart;
    range->tStart = s;
    range->tEnd = e;
    }
expandRange(range, querySize, nibSize, respectFrame);
target = nibLdPart(nibFileName, f, nibSize, range->tStart, range->tEnd - range->tStart);
if (isRc)
    {
    reverseComplement(target->dna, target->size);
    s = nibSize - range->tEnd;
    e = nibSize - range->tStart;
    range->tStart =  s;
    range->tEnd = e;
    }
fclose(f);
*retNibSize = nibSize;
return target;
}

static boolean alignComponents(struct gfRange *combined, struct ssBundle *bun, 
	enum ffStringency stringency)
/* Align each piece of combined->components and put result in
 * bun->ffList. */
{
struct gfRange *range;
struct dnaSeq *qSeq = bun->qSeq, *tSeq = bun->genoSeq;
struct ssFfItem *ffi;
struct ffAli *ali;
int qStart, qEnd, tStart, tEnd;
int extra = 250;
boolean gotAny = FALSE;

for (range = combined->components; range != NULL; range = range->next)
    {
    /* Expand to include some extra sequence around range. */
    qStart = range->qStart - extra;
    tStart = range->tStart - extra;
    qEnd = range->qEnd + extra;
    tEnd = range->tEnd + extra;
    if (range == combined->components)
	{
        qStart -= extra;
	tStart -= extra;
	}
    if (range->next == NULL)
        {
	qEnd += extra;
	tEnd += extra;
	}
    if (qStart < combined->qStart) qStart = combined->qStart;
    if (tStart < combined->tStart) tStart = combined->tStart;
    if (qEnd > combined->qEnd) qEnd = combined->qEnd;
    if (tEnd > combined->tEnd) tEnd = combined->tEnd;

    ali = ffFind(qSeq->dna + qStart,
                 qSeq->dna + qEnd,
		 tSeq->dna + tStart - combined->tStart,
		 tSeq->dna + tEnd - combined->tStart,
		 stringency);
    if (ali != NULL)
        {
	AllocVar(ffi);
	ffi->ff = ali;
	slAddHead(&bun->ffList, ffi);
	gotAny = TRUE;
	}
    }
return gotAny;
}

static void saveAlignments(char *chromName, int chromSize, int chromOffset, 
	struct ssBundle *bun, void *outData, boolean isRc, 
	enum ffStringency stringency, int minMatch, GfSaveAli outFunction)
/* Save significant alignments to file in .psl format. */
{
struct dnaSeq *tSeq = bun->genoSeq, *qSeq = bun->qSeq;
struct ssFfItem *ffi;

if (minMatch > qSeq->size/2) minMatch = qSeq->size/2;
for (ffi = bun->ffList; ffi != NULL; ffi = ffi->next)
    {
    struct ffAli *ff = ffi->ff;
    (*outFunction)(chromName, chromSize, chromOffset, ff, tSeq, qSeq, isRc, stringency, minMatch, outData);
    }
}



void gfAlignStrand(int conn, char *nibDir, struct dnaSeq *seq,
    boolean isRc,  enum ffStringency stringency, int minMatch, GfSaveAli outFunction, void *outData)
/* Search genome on server with one strand of other sequence to find homology. 
 * Then load homologous bits of genome locally and do detailed alignment.
 * Call 'outFunction' with each alignment that is found. */
{
struct ssBundle *bun;
struct gfRange *rangeList = NULL, *range;
struct dnaSeq *targetSeq;
char dir[256], chromName[128], ext[64];
int chromSize;

rangeList = gfQuerySeq(conn, seq);
slSort(&rangeList, gfRangeCmpTarget);
rangeList = gfRangesBundle(rangeList, 500000);
for (range = rangeList; range != NULL; range = range->next)
    {
    splitPath(range->tName, dir, chromName, ext);
    targetSeq = expandAndLoad(range, nibDir, seq->size, &chromSize, FALSE, FALSE);
    AllocVar(bun);
    bun->qSeq = seq;
    bun->genoSeq = targetSeq;
    bun->data = range;
    alignComponents(range, bun, stringency);
    ssStitch(bun, stringency);
    saveAlignments(chromName, chromSize, range->tStart, 
	bun, outData, isRc, stringency, minMatch, outFunction);
    ssBundleFree(&bun);
    freeDnaSeq(&targetSeq);
    }
gfRangeFreeList(&rangeList);
}

char *clumpTargetName(struct gfClump *clump)
/* Return target name of clump - whether it is in memory or on disk. */
{
struct gfSeqSource *target = clump->target;
char *name;
if (target->seq != NULL)
    name = target->seq->name;
else
    name = target->fileName;
if (name == NULL)
    internalErr();
return name;
}

static struct gfRange *seqClumpToRangeList(struct gfClump *clumpList)
/* Convert from clump list to range list. */
{
struct gfRange *rangeList = NULL, *range;
struct gfClump *clump;
struct dnaSeq *seq;
char *name;

for (clump = clumpList; clump != NULL; clump = clump->next)
    {
    AllocVar(range);
    range->qStart = clump->qStart;
    range->qEnd = clump->qEnd;
    name = clumpTargetName(clump);
    range->tName = cloneString(name);
    range->tStart = clump->tStart;
    range->tEnd = clump->tEnd;
    range->tSeq = clump->target->seq;
    slAddHead(&rangeList, range);
    }
slReverse(&rangeList);
return rangeList;
}

void gfAlignDnaClumps(struct gfClump *clumpList, struct dnaSeq *seq,
    boolean isRc,  enum ffStringency stringency, int minMatch, 
    GfSaveAli outFunction, void *outData)
/* Convert gfClumps to an actual alignment that gets saved via 
 * outFunction/outData. */
{
struct ssBundle *bun;
struct gfRange *rangeList = NULL, *range;
struct dnaSeq *targetSeq;

rangeList = seqClumpToRangeList(clumpList);
slSort(&rangeList, gfRangeCmpTarget);
rangeList = gfRangesBundle(rangeList, 500000);
for (range = rangeList; range != NULL; range = range->next)
    {
    targetSeq = range->tSeq;
    expandRange(range, seq->size, targetSeq->size, FALSE);
    range->tStart = 0;
    range->tEnd = targetSeq->size;
    AllocVar(bun);
    bun->qSeq = seq;
    bun->genoSeq = targetSeq;
    bun->data = range;
    alignComponents(range, bun, stringency);
    ssStitch(bun, stringency);
    saveAlignments(targetSeq->name, targetSeq->size, 0, 
	bun, outData, isRc, stringency, minMatch, outFunction);
    ssBundleFree(&bun);
    }
gfRangeFreeList(&rangeList);
}

static int maxDown = 10;

void extendHitRight(int qMax, int tMax,
	char **pEndQ, char **pEndT, int (*scoreMatch)(char a, char b))
/* Extend endQ/endT as much to the right as possible. */
{
int maxScore = 0;
int score = 0;
int maxPos = -1;
int last = min(qMax, tMax);
int i;
char *q = *pEndQ, *t = *pEndT;

for (i=0; i<last; ++i)
    {
    score += scoreMatch(q[i], t[i]);
    if (score > maxScore)
	 {
         maxScore = score;
	 maxPos = i;
	 }
    else
         {
	 if (i - maxPos >= maxDown)
	     break;
	 }
    }
*pEndQ = q+maxPos+1;
*pEndT = t+maxPos+1;
}

void extendHitLeft(int qMax, int tMax,
	char **pStartQ, char **pStartT, int (*scoreMatch)(char a, char b))
/* Extend startQ/startT as much to the left as possible. */
{
int maxScore = 0;
int score = 0;
int maxPos = 0;
int last = -min(qMax, tMax);
int i;
char *q = *pStartQ, *t = *pStartT;

for (i=-1; i>=last; --i)
    {
    score += scoreMatch(q[i], t[i]);
    if (score > maxScore)
	 {
         maxScore = score;
	 maxPos = i;
	 }
    else
         {
	 if (maxPos - i>= maxDown)
	     break;
	 }
    }
*pStartQ = q+maxPos;
*pStartT = t+maxPos;
}


void clumpToHspRange(struct gfClump *clump, aaSeq *qSeq, int tileSize,
	int frame, struct trans3 *t3, struct gfRange **pRangeList)
/* Covert clump->hitList to HSPs (high scoring local sequence pair,
 * that is longest alignment without gaps) and add resulting HSPs to
 * rangeList. */
{
struct gfSeqSource *target = clump->target;
aaSeq *tSeq = target->seq;
int maxDown = 10;
AA *qs, *ts, *qe, *te;
int maxScore = 0, maxPos = 0, score, pos;
struct gfHit *hit;
int qStart, tStart, qEnd, tEnd, newQ, newT;
boolean outOfIt = TRUE;		/* Logically outside of a clump. */
struct gfRange *range;
AA *lastQs = NULL, *lastQe = NULL, *lastTs = NULL, *lastTe = NULL;

if (tSeq == NULL)
    internalErr();

/* The termination condition of this loop is a little complicated.
 * We want to output something either when the next hit can't be
 * merged into the previous, or at the end of the list.  To avoid
 * duplicating the output code we're forced to complicate the loop
 * termination logic. */
for (hit = clump->hitList; ; hit = hit->next)
    {
    if (hit != NULL)
        {
	newQ = hit->qStart;
	newT = hit->tStart - target->start;
	}

    /* See if it's time to output merged (diagonally adjacent) hits. */
    if (!outOfIt)	/* Not first time through. */
        {
	if (hit == NULL || newQ != qEnd || newT != tEnd)
	    {
	    qs = qSeq->dna + qStart;
	    ts = tSeq->dna + tStart;
	    qe = qSeq->dna + qEnd;
	    te = tSeq->dna + tEnd;
	    extendHitRight(qSeq->size - qEnd, tSeq->size - tEnd,
		&qe, &te, aaScore2);
	    extendHitLeft(qStart, tStart, &qs, &ts, aaScore2);
	    if (qs != lastQs || ts != lastTs || qe != lastQe || qs !=  lastQs)
		{
		lastQs = qs;
		lastTs = ts;
		lastQe = qe;
		lastTe = te;
		AllocVar(range);
		range->qStart = qs - qSeq->dna;
		range->qEnd = qe - qSeq->dna;
		range->tName = cloneString(tSeq->name);
		range->tSeq = tSeq;
		range->tStart = ts - tSeq->dna;
		range->tEnd = te - tSeq->dna;
		range->hitCount = qe - qs;
		range->frame = frame;
		range->t3 = t3;
		slAddHead(pRangeList, range);
		}
	    outOfIt = TRUE;
	    }
	}
    if (hit == NULL)
        break;

    if (outOfIt)
        {
	qStart = newQ;
	qEnd = qStart + tileSize;
	tStart = newT;
	tEnd = tStart + tileSize;
	outOfIt = FALSE;
	}
    else
        {
	qEnd = newQ + tileSize;
	tEnd = newT + tileSize;
	}
    }
}

struct ssFfItem *rangesToFfItem(struct gfRange *rangeList, aaSeq *qSeq)
/* Convert ranges to ssFfItem's. */
{
AA *q = qSeq->dna;
struct ffAli *ffList = NULL, *ff;
struct gfRange *range;
struct ssFfItem *ffi;

for (range = rangeList; range != NULL; range = range->next)
    {
    aaSeq *tSeq = range->tSeq;
    AA *t = tSeq->dna;
    AllocVar(ff);
    ff->nStart = q + range->qStart;
    ff->nEnd = q + range->qEnd;
    ff->hStart = t + range->tStart;
    ff->hEnd = t + range->tEnd;
    ff->left = ffList;
    ffList = ff;
    }
AllocVar(ffi);
ffi->ff = ffMakeRightLinks(ffList);
return ffi;
}

void gfAlignAaClumps(struct genoFind *gf,  struct gfClump *clumpList, aaSeq *seq,
    boolean isRc,  enum ffStringency stringency, int minMatch, 
    GfSaveAli outFunction, void *outData)
/* Convert gfClumps to an actual alignment that gets saved via 
 * outFunction/outData. */
{
struct gfClump *clump;
struct gfRange *rangeList = NULL, *range;
aaSeq *targetSeq;
struct ssBundle *bun;

for (clump = clumpList; clump != NULL; clump = clump->next)
    {
    // gfClumpDump(gf, clump, uglyOut);
    clumpToHspRange(clump, seq, gf->tileSize, 0, NULL, &rangeList);
    }
slReverse(&rangeList);
slSort(&rangeList, gfRangeCmpTarget);
rangeList = gfRangesBundle(rangeList, 500000/3);
for (range = rangeList; range != NULL; range = range->next)
    {
    targetSeq = range->tSeq;
    AllocVar(bun);
    bun->qSeq = seq;
    bun->genoSeq = targetSeq;
    bun->data = range;
    bun->ffList = rangesToFfItem(range->components, seq);
    bun->isProt = TRUE;
    ssStitch(bun, stringency);
    saveAlignments(targetSeq->name, targetSeq->size, 0, 
	bun, outData, isRc, stringency, minMatch, outFunction);
    ssBundleFree(&bun);
    }
gfRangeFreeList(&rangeList);
}

void gfFindAlignAaTrans(struct genoFind *gfs[3], aaSeq *qSeq, struct hash *t3Hash, 
	enum ffStringency stringency, int minMatch, 
	GfSaveAli outFunction, void *outData)
/* Look for qSeq alignment in three translated reading frames. Save alignment
 * via outFunction/outData. */
{
struct gfClump *clumps[3];
int frame;
struct gfClump *clump;
struct gfRange *rangeList = NULL, *range;
aaSeq *targetSeq;
struct ssBundle *bun;
int tileSize = gfs[0]->tileSize;
struct trans3 *t3;

gfTransFindClumps(gfs, qSeq, clumps);
for (frame=0; frame<3; ++frame)
    {
    for (clump = clumps[frame]; clump != NULL; clump = clump->next)
	{
	// gfClumpDump(gfs[frame], clump, uglyOut);
	clumpToHspRange(clump, qSeq, tileSize, frame, NULL, &rangeList);
	}
    }
slReverse(&rangeList);
slSort(&rangeList, gfRangeCmpTarget);
rangeList = gfRangesBundle(rangeList, 500000/3);
for (range = rangeList; range != NULL; range = range->next)
    {
    targetSeq = range->tSeq;
    t3 = hashMustFindVal(t3Hash, targetSeq->name);
    AllocVar(bun);
    bun->qSeq = qSeq;
    bun->genoSeq = targetSeq;
    bun->data = range;
    bun->ffList = rangesToFfItem(range->components, qSeq);
    bun->isProt = TRUE;
    bun->t3List = t3;
    ssStitch(bun, stringency);
    saveAlignments(targetSeq->name, targetSeq->size, 0, 
	bun, outData, FALSE, stringency, minMatch, outFunction);
    ssBundleFree(&bun);
    }
for (frame=0; frame<3; ++frame)
    gfClumpFreeList(&clumps[frame]);
}

struct trans3 *findTrans3(struct hash *t3Hash, char *name, int start, int end)
/* Find trans3 in hash which corresponds to sequence of given name and includes
 * bases between start and end. */
{
struct trans3 *t3;
for (t3 = hashFindVal(t3Hash, name); t3 != NULL; t3 = t3->next)
    {
    if (t3->start <= start && t3->end >= end)
        return t3;
    }
internalErr();
return NULL;
}

void gfAlignTrans(int conn, char *nibDir, aaSeq *seq,
    int minMatch, GfSaveAli outFunction, struct gfSavePslxData *outData)
/* Search indexed translated genome on server with an amino acid sequence. 
 * Then load homologous bits of genome locally and do detailed alignment.
 * Call 'outFunction' with each alignment that is found. */
{
struct ssBundle *bun;
struct gfClump *clumps[2][3], *clump;
struct gfRange *rangeList = NULL, *range, *rl;
struct dnaSeq *targetSeq, *tSeqList = NULL;
char dir[256], chromName[128], ext[64];
int chromSize, tileSize;
int frame, isRc = 0;
struct hash *t3Hash = NULL;
struct slRef *t3RefList = NULL, *ref;
struct gfSeqSource *ssList = NULL, *ss;
struct trans3 *t3;

/* Get clumps from server. */
gfQuerySeqTrans(conn, seq, clumps, &ssList, &tileSize);

for (isRc = 0; isRc <= 1;  ++isRc)
    {
    t3Hash = newHash(10);

    /* Figure out which parts of sequence we need to load. */
    for (frame = 0; frame < 3; ++frame)
	{
	rl = seqClumpToRangeList(clumps[isRc][frame]);
	rangeList = slCat(rangeList, rl);
	}
    /* Convert from amino acid to nucleotide coordinates. */
    for (range = rangeList; range != NULL; range = range->next)
	{
	range->qStart *= 3;
	range->qEnd *= 3;
	range->tStart *= 3;
	range->tEnd *= 3;
	}
    slSort(&rangeList, gfRangeCmpTarget);
    rangeList = gfRangesBundle(rangeList, 500000);

    /* Construct t3Hash and load sequence into it (but don't expand to
     * all translated reading frames yet.) */
    for (range = rangeList; range != NULL; range = range->next)
	{
	int chromSize;
	struct trans3 *oldT3;

	targetSeq = expandAndLoad(range, nibDir, seq->size*3, &chromSize, TRUE, isRc);
	slAddHead(&tSeqList, targetSeq);
	freez(&targetSeq->name);
	targetSeq->name = cloneString(range->tName);
	t3 = trans3New(targetSeq);
	refAdd(&t3RefList, t3);
	t3->start = range->tStart;
	t3->end = range->tEnd;
	t3->nibSize = chromSize;
	t3->isRc = isRc;
	if ((oldT3 = hashFindVal(t3Hash, range->tName)) != NULL)
	    {
	    slAddTail(&oldT3->next, t3);
	    }
	else
	    {
	    hashAdd(t3Hash, range->tName, t3);
	    }
	}

    /* The old range list was not very precise - it was just to get
     * the DNA loaded.  */
    gfRangeFreeList(&rangeList);


    /* Patch up clump list and associated sequence source to refer
     * to bits of genome loaded into memory. */
    for (frame = 0; frame < 3; ++frame)
	{
	for (clump = clumps[isRc][frame]; clump != NULL; clump = clump->next)
	    {
	    struct gfSeqSource *ss = clump->target;
	    t3 = findTrans3(t3Hash, clumpTargetName(clump), clump->tStart*3, clump->tEnd*3);
	    ss->seq = t3->trans[frame];
	    ss->start = t3->start/3;
	    ss->end = t3->end/3;
	    clumpToHspRange(clump, seq, tileSize, frame, t3, &rangeList);
	    /* ~~~ */
	    }
	}
    slReverse(&rangeList);
    slSort(&rangeList, gfRangeCmpTarget);
    rangeList = gfRangesBundle(rangeList, 500000/3);
    for (range = rangeList; range != NULL; range = range->next)
	{
	targetSeq = range->tSeq;
	AllocVar(bun);
	bun->qSeq = seq;
	bun->genoSeq = targetSeq;
	bun->data = range;
	bun->ffList = rangesToFfItem(range->components, seq);
	bun->isProt = TRUE;
	t3 = hashMustFindVal(t3Hash, range->tName);
	bun->t3List = t3;
	ssStitch(bun, ffCdna);
	outData->targetRc = isRc;
	outData->t3Hash = t3Hash;
	saveAlignments(targetSeq->name, t3->nibSize/3, 0, 
	    bun, outData, FALSE, ffCdna, minMatch, outFunction);
	ssBundleFree(&bun);
	}

    /* Cleanup for this strand of database. */
    gfRangeFreeList(&rangeList);
    freeHash(&t3Hash);
    for (ref = t3RefList; ref != NULL; ref = ref->next)
        {
	struct trans3 *t3 = ref->val;
	trans3Free(&t3);
	}
    slFreeList(&t3RefList);
    freeDnaSeqList(&tSeqList);
    }

/* Final cleanup. */
for (isRc=0; isRc<=1; ++isRc)
    for (frame=0; frame<3; ++frame)
	gfClumpFreeList(&clumps[isRc][frame]);
for (ss = ssList; ss != NULL; ss = ss->next)
    freeMem(ss->fileName);
slFreeList(&ssList);
}

void untranslateRangeList(struct gfRange *rangeList, int qFrame, int tFrame, struct hash *t3Hash)
/* Translate coordinates from protein to dna. */
{
struct gfRange *range;
struct trans3 *t3;
for (range = rangeList; range != NULL; range = range->next)
    {
    range->qStart = 3*range->qStart + qFrame;
    range->qEnd = 3*range->qEnd + qFrame;
    range->tStart = 3*range->tStart + tFrame;
    range->tEnd = 3*range->tEnd + tFrame;
    t3 = hashMustFindVal(t3Hash, range->tSeq->name);
    range->tSeq = t3->seq;
    }
}

void gfFindAlignTransTrans(struct genoFind *gfs[3], struct dnaSeq *qSeq, struct hash *t3Hash, 
	boolean isRc, enum ffStringency stringency, int minMatch, 
	GfSaveAli outFunction, void *outData)
/* Look for alignment to three translations of qSeq in three translated reading frames. 
 * Save alignment via outFunction/outData. */
{
struct trans3 *qTrans = trans3New(qSeq), *t3;
int qFrame, tFrame;
struct gfClump *clumps[3][3], *clump;
struct gfRange *rangeList = NULL, *range;
int tileSize = gfs[0]->tileSize;
bioSeq *targetSeq;
struct ssBundle *bun;

gfTransTransFindClumps(gfs, qTrans->trans, clumps);
for (qFrame = 0; qFrame<3; ++qFrame)
    {
    for (tFrame=0; tFrame<3; ++tFrame)
	{
	for (clump = clumps[qFrame][tFrame]; clump != NULL; clump = clump->next)
	    {
	    struct gfRange *rangeSet = NULL;
	    // gfClumpDump(gfs[tFrame], clump, uglyOut);
	    clumpToHspRange(clump, qSeq, tileSize, tFrame, NULL, &rangeSet);
	    untranslateRangeList(rangeSet, qFrame, tFrame, t3Hash);
	    rangeList = slCat(rangeSet, rangeList);
	    }
	}
    }
slSort(&rangeList, gfRangeCmpTarget);
rangeList = gfRangesBundle(rangeList, 500000);
for (range = rangeList; range != NULL; range = range->next)
    {
    targetSeq = range->tSeq;
    AllocVar(bun);
    bun->qSeq = qSeq;
    bun->genoSeq = targetSeq;
    bun->data = range;
    bun->ffList = rangesToFfItem(range->components, qSeq);
    ssStitch(bun, stringency);
    saveAlignments(targetSeq->name, targetSeq->size, 0, 
	bun, outData, isRc, stringency, minMatch, outFunction);
    ssBundleFree(&bun);
    }
trans3Free(&qTrans);
for (qFrame = 0; qFrame<3; ++qFrame)
    for (tFrame=0; tFrame<3; ++tFrame)
	gfClumpFreeList(&clumps[qFrame][tFrame]);
}

void trans3Offset(struct trans3 *t3List, AA *aa, int *retOffset, int *retFrame)
/* Figure out offset of peptide in context of larger sequences. */
{
struct trans3 *t3;
int frame;
aaSeq *seq;

for (t3 = t3List; t3 != NULL; t3 = t3->next)
    {
    for (frame = 0; frame < 3; ++frame)
        {
	seq = t3->trans[frame];
	if (seq->dna <= aa && aa < seq->dna + seq->size)
	    {
	    *retOffset = aa - seq->dna + t3->start/3;
	    *retFrame = frame;
	    return;
	    }
	}
    }
internalErr();
}

static int t3Offset(char *pt, bioSeq *seq, struct hash *t3Hash)
/* Return offset of pt within sequence or within triple 
 * sequence in t3Hash. */
{
if (t3Hash != NULL)   
    {
    struct trans3 *t3List = hashMustFindVal(t3Hash, seq->name);
    int frame, offset;
    trans3Offset(t3List, pt, &offset, &frame);
    return offset;
    }
else
    return pt - seq->dna;
}

void gfSavePslOrPslx(char *chromName, int chromSize, int chromOffset,
	struct ffAli *ali, struct dnaSeq *genoSeq, struct dnaSeq *otherSeq, 
	boolean isRc, enum ffStringency stringency, int minMatch, FILE *out,
	struct hash *t3Hash, boolean reportTargetStrand, boolean targetIsRc)
/* Analyse one alignment and if it looks good enough write it out to file in
 * psl format (or pslX format - if t3Hash is non-NULL).  */
{
/* This function was stolen from psLayout and slightly modified (mostly because 
 * we don't
 * have repeat data). */
struct ffAli *ff, *nextFf;
struct ffAli *right = ffRightmost(ali);
DNA *needle = otherSeq->dna;
DNA *hay = genoSeq->dna;
int nStart = ali->nStart - needle;
int nEnd = right->nEnd - needle;
int hStart = t3Offset(ali->hStart, genoSeq, t3Hash);
int hEnd = t3Offset(right->hEnd, genoSeq, t3Hash);
int nInsertBaseCount = 0;
int nInsertCount = 0;
int hInsertBaseCount = 0;
int hInsertCount = 0;
int matchCount = 0;
int mismatchCount = 0;
int repMatch = 0;
int countNs = 0;
DNA *np, *hp, n, h;
int blockSize;
int i;
int badScore;
struct trans3 *t3 = NULL;

/* Count up matches, mismatches, inserts, etc. */
for (ff = ali; ff != NULL; ff = nextFf)
    {
    nextFf = ff->right;
    blockSize = ff->nEnd - ff->nStart;
    np = ff->nStart;
    hp = ff->hStart;
    for (i=0; i<blockSize; ++i)
	{
	n = np[i];
	h = hp[i];
	if (n == 'n' || h == 'n')
	    ++countNs;
	else
	    {
	    if (n == h)
		++matchCount;
	    else
		++mismatchCount;
	    }
	}
    if (nextFf != NULL)
	{
	if (ff->nEnd != nextFf->nStart)
	    {
	    ++nInsertCount;
	    nInsertBaseCount += nextFf->nStart - ff->nEnd;
	    }
	if (ff->hEnd != nextFf->hStart)
	    {
	    ++hInsertCount;
	    hInsertBaseCount += t3Offset(nextFf->hStart, genoSeq, t3Hash) - t3Offset(ff->hEnd, genoSeq, t3Hash);
	    }
	}
    }

/* See if it looks good enough to output. */
if (matchCount >= minMatch)
    {
    if (isRc)
	{
	int temp;
	int oSize = otherSeq->size;
	temp = nStart;
	nStart = oSize - nEnd;
	nEnd = oSize - temp;
	}
    if (targetIsRc)
        {
	int temp;
	temp = hStart;
	hStart = chromSize - hEnd;
	hEnd = chromSize - temp;
	}
    fprintf(out, "%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%c",
	matchCount, mismatchCount, repMatch, countNs, nInsertCount, nInsertBaseCount, hInsertCount, hInsertBaseCount,
	(isRc ? '-' : '+'));
    if (reportTargetStrand)
        fprintf(out, "%c", (targetIsRc ? '-' : '+') );
    fprintf(out, "\t%s\t%d\t%d\t%d\t"
		 "%s\t%d\t%d\t%d\t%d\t",
	otherSeq->name, otherSeq->size, nStart, nEnd,
	chromName, chromSize, hStart, hEnd,
	ffAliCount(ali));
    for (ff = ali; ff != NULL; ff = ff->right)
	fprintf(out, "%d,", ff->nEnd - ff->nStart);
    fprintf(out, "\t");
    for (ff = ali; ff != NULL; ff = ff->right)
	fprintf(out, "%d,", ff->nStart - needle);
    fprintf(out, "\t");
    for (ff = ali; ff != NULL; ff = ff->right)
	fprintf(out, "%d,", t3Offset(ff->hStart, genoSeq, t3Hash) + chromOffset);
    if (t3Hash != NULL)
        {
	struct trans3 *t3List = hashMustFindVal(t3Hash, genoSeq->name);
	int offset, frame;
	fprintf(out, "\t");
	for (ff = ali; ff != NULL; ff = ff->right)
	    {
	    trans3Offset(t3List, ff->hStart, &offset, &frame);
	    fprintf(out, "%d,", frame);
	    }
	}
    fprintf(out, "\n");
    if (ferror(out))
	{
	perror("");
	errAbort("Write error to .psl");
	}
    }
}

void gfSavePsl(char *chromName, int chromSize, int chromOffset,
	struct ffAli *ali, struct dnaSeq *genoSeq, struct dnaSeq *otherSeq, 
	boolean isRc, enum ffStringency stringency, int minMatch, void *outputData)
{
gfSavePslOrPslx(chromName, chromSize, chromOffset, ali, genoSeq, otherSeq,
    isRc, stringency, minMatch, outputData, NULL, FALSE, FALSE);
}

void gfSavePslx(char *chromName, int chromSize, int chromOffset,
	struct ffAli *ali, struct dnaSeq *genoSeq, struct dnaSeq *otherSeq, 
	boolean isRc, enum ffStringency stringency, int minMatch, void *outputData)
{
struct gfSavePslxData *data = outputData;
gfSavePslOrPslx(chromName, chromSize, chromOffset, ali, genoSeq, otherSeq,
    isRc, stringency, minMatch, data->f, data->t3Hash, data->reportTargetStrand, data->targetRc);
}

