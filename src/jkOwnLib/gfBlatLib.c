/* gfBlatLib - stuff that blat-related clients of genoFind library or
 * gfServer on the web use. */
/* Copyright 2001-2005 Jim Kent.  All rights reserved. */

#include "common.h"
#include "net.h"
#include "linefile.h"
#include "sqlNum.h"
#include "dnaseq.h"
#include "fa.h"
#include "fuzzyFind.h"
#include "supStitch.h"
#include "genoFind.h"
#include "gfInternal.h"
#include "errabort.h"
#include "nib.h"
#include "twoBit.h"
#include "trans3.h"


static char const rcsid[] = "$Id: gfBlatLib.c,v 1.27 2008/12/09 08:06:20 galt Exp $";

static int ssAliCount = 16;	/* Number of alignments returned by ssStitch. */

#ifdef DEBUG
void dumpRange(struct gfRange *r, FILE *f)
/* Dump range to file. */
{
fprintf(f, "%d-%d %s %d-%d, hits %d\n", r->qStart, r->qEnd, r->tName, r->tStart, r->tEnd, r->hitCount);
}

void dumpRangeList(struct gfRange *rangeList, FILE *f)
/* Dump range list to file for debugging. */
{
struct gfRange *range;
for (range = rangeList; range != NULL; range = range->next)
    dumpRange(range, f);
}
#endif /* DEBUG */

void gfRangeFree(struct gfRange **pEl)
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

void gfRangeFreeList(struct gfRange **pList)
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
/* Load a gfRange from array of strings parsed from
 * server. Dispose of this with gfRangeFree(). */
{
struct gfRange *ret;

AllocVar(ret);
ret->qStart = atoi(row[0]);
ret->qEnd = atoi(row[1]);
ret->tName = cloneString(row[2]);
ret->tStart = atoi(row[3]);
ret->tEnd = atoi(row[4]);
ret->hitCount = atoi(row[5]);
return ret;
}

int gfRangeCmpTarget(const void *va, const void *vb)
/* Compare to sort based on target position. */
{
const struct gfRange *a = *((struct gfRange **)va);
const struct gfRange *b = *((struct gfRange **)vb);
int diff;

diff = strcmp(a->tName, b->tName);
if (diff == 0)
    {
    long lDiff = a->t3 - b->t3;
    if (lDiff < 0)
       diff = -1;
    else if (lDiff > 0)
       diff = 1;
    else
       diff = 0;
#ifdef SOLARIS_WORKAROUND_COMPILER_BUG_BUT_FAILS_IN_64_BIT
    diff = (unsigned long)(a->t3) - (unsigned long)(b->t3);	/* Casts needed for Solaris.  Thanks Darren Platt! */
#endif /* SOLARIS_WORKAROUND_COMPILER_BUG_BUT_FAILS_IN_64_BIT */
    }
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

static void gfServerWarn(bioSeq *seq, char *warning)
/* Write out warning. */
{
warn("couldn't process %s: %s", seq->name, warning);
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
    netRecieveString(conn, buf);
    if (sameString(buf, "end"))
	{
	break;
	}
    else if (startsWith("Error:", buf))
        {
	gfServerWarn(seq, buf);
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

static int findTileSize(char *line)
/* Parse through line/val pairs looking for tileSize. */
{
char *var, *val;
int tileSize = 4;
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
return tileSize;
}

struct gfHit *getHitsFromServer(int conn, struct lm *lm)
/* Read a lone line of hits from server. */
{
char *s, *line, *q, *t;
struct gfHit *hitList = NULL, *hit;
s = line = netRecieveLongString(conn);
for (;;)
    {
    if ((q = nextWord(&line)) == NULL)
	 break;
    if ((t = nextWord(&line)) == NULL)
	 internalErr();
    lmAllocVar(lm, hit);
    hit->qStart = sqlUnsigned(q);
    hit->tStart = sqlUnsigned(t);
    slAddHead(&hitList, hit);
    }
freez(&s);
slReverse(&hitList);
return hitList;
}

static void gfQuerySeqTrans(int conn, aaSeq *seq, struct gfClump *clumps[2][3], 
    struct lm *lm, struct gfSeqSource **retSsList, int *retTileSize)
/* Query server for clumps where aa sequence hits translated index. */
{
int frame, isRc, rowSize;
struct gfClump *clump;
int tileSize = 0;
char *line;
char buf[256], *row[12];
struct gfSeqSource *ssList = NULL, *ss;

for (isRc = 0; isRc <= 1; ++isRc)
    for (frame = 0; frame<3; ++frame)
	clumps[isRc][frame] = NULL;

/* Send sequence to server. */
startSeqQuery(conn, seq, "protQuery");
line = netRecieveString(conn, buf);
if (!startsWith("Error:", line))
    {
    tileSize = findTileSize(line);

    /* Read results line by line and save in memory. */
    for (;;)
	{
	/* Read and parse first line that describes clump overall. */
	netRecieveString(conn, buf);
	if (sameString(buf, "end"))
	    {
	    break;
	    }
	else if (startsWith("Error:", buf))
	    {
	    gfServerWarn(seq, buf);
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
	clump->hitList = getHitsFromServer(conn, lm);
	assert(slCount(clump->hitList) == clump->hitCount);
	}
    for (isRc = 0; isRc <= 1; ++isRc)
	for (frame = 0; frame<3; ++frame)
	    slReverse(&clumps[isRc][frame]);
    }
else
    {
    gfServerWarn(seq, line);
    }
*retSsList = ssList;
*retTileSize = tileSize;
}

static void gfQuerySeqTransTrans(int conn, struct dnaSeq *seq, 
    struct gfClump *clumps[2][3][3], 
    struct lm *lm, struct gfSeqSource **retSsList, int *retTileSize)
/* Query server for clumps where translated DNA sequence hits translated 
 * index. */
{
int qFrame, tFrame, isRc, rowSize;
struct gfClump *clump;
int tileSize = 0;
char *line;
char buf[256], *row[12];
struct gfSeqSource *ssList = NULL, *ss;

for (isRc = 0; isRc <= 1; ++isRc)
    for (qFrame = 0; qFrame<3; ++qFrame)
	for (tFrame = 0; tFrame<3; ++tFrame)
	    clumps[isRc][qFrame][tFrame] = NULL;

/* Send sequence to server. */
startSeqQuery(conn, seq, "transQuery");
line = netRecieveString(conn, buf);
if (!startsWith("Error:", line))
    {
    tileSize = findTileSize(line);

    /* Read results line by line and save in memory. */
    for (;;)
	{
	/* Read and parse first line that describes clump overall. */
	netRecieveString(conn, buf);
	if (sameString(buf, "end"))
	    {
	    break;
	    }
	else if (startsWith("Error:", buf))
	    {
	    gfServerWarn(seq, buf);
	    break;
	    }
	rowSize = chopLine(buf, row);
	if (rowSize < 9)
	    errAbort("Expecting 9 words from server got %d", rowSize);
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
	qFrame = sqlUnsigned(row[7]);
	tFrame = sqlUnsigned(row[8]);
	slAddHead(&clumps[isRc][qFrame][tFrame], clump);

	/* Read and parse next (long) line that describes hits. */
	clump->hitList = getHitsFromServer(conn, lm);
	assert(slCount(clump->hitList) == clump->hitCount);
	}
    for (isRc = 0; isRc <= 1; ++isRc)
	for (qFrame = 0; qFrame<3; ++qFrame)
	    for (tFrame = 0; tFrame<3; ++tFrame)
		slReverse(&clumps[isRc][qFrame][tFrame]);
    }
else
    {
    gfServerWarn(seq, buf);
    }
*retSsList = ssList;
*retTileSize = tileSize;
}



struct gfRange *gfRangesBundle(struct gfRange *exonList, int maxIntron)
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
	|| exon->t3  != lastExon->t3
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

static int usualExpansion = 500;

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

static int scoreAli(struct ffAli *ali, boolean isProt, 
	enum ffStringency stringency, 
	struct dnaSeq *tSeq, struct trans3 *t3List)
/* Score alignment. */
{
int (*scoreFunc)(char *a, char *b, int size);
struct ffAli *ff, *nextFf;
int score = 0;
if (isProt) 
    scoreFunc = aaScoreMatch;
else
    scoreFunc = dnaScoreMatch;
for (ff = ali; ff != NULL; ff = nextFf)
    {
    nextFf = ff->right;
    score += scoreFunc(ff->nStart, ff->hStart, ff->nEnd-ff->nStart);
    if (nextFf != NULL)
        {
	int nhStart = trans3GenoPos(nextFf->hStart, tSeq, t3List, FALSE);
	int ohEnd = trans3GenoPos(ff->hEnd, tSeq, t3List, TRUE);
	int hGap = nhStart - ohEnd;
	int nGap = nextFf->nStart - ff->nEnd;
	score -= ffCalcGapPenalty(hGap, nGap, stringency);
	}
    }
return score;
}

static void saveAlignments(char *chromName, int chromSize, int chromOffset, 
	struct ssBundle *bun, struct hash *t3Hash, 
	boolean qIsRc, boolean tIsRc,
	enum ffStringency stringency, int minMatch, struct gfOutput *out)
/* Save significant alignments to file in .psl format. */
{
struct dnaSeq *tSeq = bun->genoSeq, *qSeq = bun->qSeq;
struct ssFfItem *ffi;
for (ffi = bun->ffList; ffi != NULL; ffi = ffi->next)
    {
    struct ffAli *ff = ffi->ff;
    struct trans3 *t3List = NULL;
    int score;
    if (t3Hash != NULL)
	t3List = hashMustFindVal(t3Hash, tSeq->name);
    score = scoreAli(ff, bun->isProt, stringency, tSeq, t3List);
    if (score >= minMatch)
	{
	out->out(chromName, chromSize, chromOffset, ff, tSeq, t3Hash, qSeq, 
	    qIsRc, tIsRc, stringency, minMatch, out);
	}
    }
}

struct hash *gfFileCacheNew()
/* Create hash for storing info on .nib and .2bit files. */
{
return hashNew(0);
}

static void gfFileCacheFreeEl(struct hashEl *el)
/* Free up one file cache info. */
{
char *name = el->name;
if (nibIsFile(name))
    {
    struct nibInfo *nib = el->val;
    nibInfoFree(&nib);
    }
else
    {
    struct twoBitFile *tbf = el->val;
    twoBitClose(&tbf);
    }
el->val = NULL;
}

void gfFileCacheFree(struct hash **pCache)
/* Free up resources in cache. */
{
struct hash *cache = *pCache;
if (cache != NULL)
    {
    hashTraverseEls(cache, gfFileCacheFreeEl);
    hashFree(pCache);
    }
}

static void getTargetName(char *tSpec, boolean includeFile, char *targetName)
/* Put sequence name, optionally prefixed by file: in targetName. */
{
if (includeFile)
    {
    char seqName[128];
    char fileName[PATH_LEN];
    gfiGetSeqName(tSpec, seqName, fileName);
    safef(targetName, PATH_LEN, "%s:%s", fileName, seqName);
    }
else
    gfiGetSeqName(tSpec, targetName, NULL);
}

void gfAlignStrand(int *pConn, char *tSeqDir, struct dnaSeq *seq,
    boolean isRc, int minMatch, struct hash *tFileCache, struct gfOutput *out)
/* Search genome on server with one strand of other sequence to find homology. 
 * Then load homologous bits of genome locally and do detailed alignment.
 * Call 'outFunction' with each alignment that is found. */
{
struct ssBundle *bun;
struct gfRange *rangeList = NULL, *range;
struct dnaSeq *targetSeq;
char targetName[PATH_LEN];

rangeList = gfQuerySeq(*pConn, seq);
close(*pConn);
*pConn = -1;
slSort(&rangeList, gfRangeCmpTarget);
rangeList = gfRangesBundle(rangeList, ffIntronMax);
for (range = rangeList; range != NULL; range = range->next)
    {
    getTargetName(range->tName, out->includeTargetFile, targetName);
    targetSeq = gfiExpandAndLoadCached(range, tFileCache, tSeqDir, 
    	seq->size, &range->tTotalSize, FALSE, FALSE, usualExpansion);
    AllocVar(bun);
    bun->qSeq = seq;
    bun->genoSeq = targetSeq;
    alignComponents(range, bun, ffCdna);
    ssStitch(bun, ffCdna, minMatch, ssAliCount);
    saveAlignments(targetName, range->tTotalSize, range->tStart, 
	bun, NULL, isRc, FALSE, ffCdna, minMatch, out);
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

static struct gfRange *seqClumpToRangeList(struct gfClump *clumpList, int frame)
/* Convert from clump list to range list. */
{
struct gfRange *rangeList = NULL, *range;
struct gfClump *clump;
char *name;
int tOff;

for (clump = clumpList; clump != NULL; clump = clump->next)
    {
    tOff = clump->target->start;
    AllocVar(range);
    range->qStart = clump->qStart;
    range->qEnd = clump->qEnd;
    name = clumpTargetName(clump);
    range->tName = cloneString(name);
    range->tStart = clump->tStart - tOff;
    range->tEnd = clump->tEnd - tOff;
    range->tSeq = clump->target->seq;
    range->frame = frame;
    slAddHead(&rangeList, range);
    }
slReverse(&rangeList);
return rangeList;
}

static struct ssBundle *gfClumpsToBundles(struct gfClump *clumpList, 
    boolean isRc, struct dnaSeq *seq, int minScore,  
    struct gfRange **retRangeList)
/* Convert gfClumps to an actual alignments (ssBundles) */ 
{
struct ssBundle *bun, *bunList = NULL;
struct gfRange *rangeList = NULL, *range;
struct dnaSeq *targetSeq;

rangeList = seqClumpToRangeList(clumpList, 0);
slSort(&rangeList, gfRangeCmpTarget);
rangeList = gfRangesBundle(rangeList, 2000);
for (range = rangeList; range != NULL; range = range->next)
    {
    targetSeq = range->tSeq;
    gfiExpandRange(range, seq->size, targetSeq->size, FALSE, isRc, 
    	usualExpansion);
    range->tStart = 0;
    range->tEnd = targetSeq->size;
    AllocVar(bun);
    bun->qSeq = seq;
    bun->genoSeq = targetSeq;
    alignComponents(range, bun, ffCdna);
    ssStitch(bun, ffCdna, minScore, ssAliCount);
    slAddHead(&bunList, bun);
    }
slReverse(&bunList);
*retRangeList = rangeList;
return bunList;
}


static void extendHitRight(int qMax, int tMax,
	char **pEndQ, char **pEndT, int (*scoreMatch)(char a, char b), 
	int maxDown)
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
    else if (i > maxPos + maxDown)
         {
	 break;
	 }
    }
*pEndQ = q+maxPos+1;
*pEndT = t+maxPos+1;
}

static void extendHitLeft(int qMax, int tMax,
	char **pStartQ, char **pStartT, int (*scoreMatch)(char a, char b),
	int maxDown)
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
    else if (i < maxPos - maxDown)
         {
	 break;
	 }
    }
*pStartQ = q+maxPos;
*pStartT = t+maxPos;
}


static void clumpToHspRange(struct gfClump *clump, bioSeq *qSeq, int tileSize,
	int frame, struct trans3 *t3, struct gfRange **pRangeList, 
	boolean isProt, boolean fastMap)
/* Covert clump->hitList to HSPs (high scoring local sequence pair,
 * that is longest alignment without gaps) and add resulting HSPs to
 * rangeList. */
{
struct gfSeqSource *target = clump->target;
aaSeq *tSeq = target->seq;
BIOPOL *qs, *ts, *qe, *te;
struct gfHit *hit;
int qStart = 0, tStart = 0, qEnd = 0, tEnd = 0, newQ = 0, newT = 0;
boolean outOfIt = TRUE;		/* Logically outside of a clump. */
struct gfRange *range;
BIOPOL *lastQs = NULL, *lastQe = NULL, *lastTs = NULL, *lastTe = NULL;
int (*scoreMatch)(char a, char b) = (isProt ? aaScore2 : dnaScore2);
int maxDown, minSpan;

if (fastMap)
    {
    maxDown = 1;
    minSpan = 50;
    }
else
    {
    maxDown = 10;
    minSpan = 0;
    }


if (tSeq == NULL)
    internalErr();

/* The termination condition of this loop is a little complicated.
 * We want to output something either when the next hit can't be
 * merged into the previous, or at the end of the list.  To avoid
 * duplicating the output code we're forced to complicate the loop
 * termination logic.  Hence the check for hit == NULL to break
 * the loop is not until near the end of the loop. */
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
	/* As a micro-optimization handle strings of adjacent hits
	 * specially.  Don't do the extensions until we've merged
	 * all adjacent hits. */
	if (hit == NULL || newQ != qEnd || newT != tEnd)
	    {
	    qs = qSeq->dna + qStart;
	    ts = tSeq->dna + tStart;
	    qe = qSeq->dna + qEnd;
	    te = tSeq->dna + tEnd;
	    extendHitRight(qSeq->size - qEnd, tSeq->size - tEnd,
		&qe, &te, scoreMatch, maxDown);
	    extendHitLeft(qStart, tStart, &qs, &ts, scoreMatch, maxDown);
	    if (qs != lastQs || ts != lastTs || qe != lastQe || qs !=  lastQs)
		{
		lastQs = qs;
		lastTs = ts;
		lastQe = qe;
		lastTe = te;
		if (qe - qs >= minSpan)
		    {
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
		    assert(range->tEnd <= tSeq->size);
		    slAddHead(pRangeList, range);
		    }
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

struct ssFfItem *gfRangesToFfItem(struct gfRange *rangeList, aaSeq *qSeq)
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

static struct ssBundle *fastMapClumpsToBundles(struct genoFind *gf, struct gfClump *clumpList, bioSeq *qSeq)
/* Convert gfClumps ffAlis. */
{
struct gfClump *clump;
struct gfRange *rangeList = NULL, *range;
bioSeq *targetSeq;
struct ssBundle *bunList = NULL, *bun;

for (clump = clumpList; clump != NULL; clump = clump->next)
    clumpToHspRange(clump, qSeq, gf->tileSize, 0, NULL, &rangeList, FALSE, TRUE);
slReverse(&rangeList);
slSort(&rangeList, gfRangeCmpTarget);
rangeList = gfRangesBundle(rangeList, 256);
for (range = rangeList; range != NULL; range = range->next)
    {
    targetSeq = range->tSeq;
    AllocVar(bun);
    bun->qSeq = qSeq;
    bun->genoSeq = targetSeq;
    bun->ffList = gfRangesToFfItem(range->components, qSeq);
    bun->isProt = FALSE;
    slAddHead(&bunList, bun);
    }
gfRangeFreeList(&rangeList);
return bunList;
}

static void gfAlignSomeClumps(struct genoFind *gf,  struct gfClump *clumpList, 
    bioSeq *seq, boolean isRc,  int minMatch, 
    struct gfOutput *out, boolean isProt, enum ffStringency stringency)
/* Convert gfClumps to an actual alignment that gets saved via 
 * outFunction/outData. */
{
struct gfClump *clump;
struct gfRange *rangeList = NULL, *range;
bioSeq *targetSeq;
struct ssBundle *bun;
int intronMax = ffIntronMax;

if (isProt)
    intronMax /= 3;
for (clump = clumpList; clump != NULL; clump = clump->next)
    {
    clumpToHspRange(clump, seq, gf->tileSize, 0, NULL, &rangeList, isProt, FALSE);
    }
slReverse(&rangeList);
slSort(&rangeList, gfRangeCmpTarget);
rangeList = gfRangesBundle(rangeList, intronMax);
for (range = rangeList; range != NULL; range = range->next)
    {
    targetSeq = range->tSeq;
    AllocVar(bun);
    bun->qSeq = seq;
    bun->genoSeq = targetSeq;
    bun->ffList = gfRangesToFfItem(range->components, seq);
    bun->isProt = isProt;
    ssStitch(bun, stringency, minMatch, ssAliCount);
    saveAlignments(targetSeq->name, targetSeq->size, 0, 
	bun, NULL, isRc, FALSE, stringency, minMatch, out);
    ssBundleFree(&bun);
    }
gfRangeFreeList(&rangeList);
}

void gfAlignAaClumps(struct genoFind *gf,  struct gfClump *clumpList, 
    aaSeq *seq, boolean isRc,  int minMatch, 
    struct gfOutput *out)
{
gfAlignSomeClumps(gf, clumpList, seq, isRc, minMatch, out, TRUE, ffTight);
}

int rangeScore(struct gfRange *range, struct dnaSeq *qSeq)
/* Return score associated with range. */
{
struct gfRange *comp;
struct dnaSeq *tSeq;
int score = 0;
for (comp = range->components; comp != NULL; comp = comp->next)
    {
    tSeq = comp->tSeq;
    score += dnaScoreMatch(tSeq->dna + range->tStart, qSeq->dna + range->qStart, 
        range->tEnd - range->tStart);
    if (comp->next != NULL)
        score -= 4;
    }
return score;
}


void gfFindAlignAaTrans(struct genoFind *gfs[3], aaSeq *qSeq, struct hash *t3Hash, 
	boolean tIsRc, int minMatch, struct gfOutput *out)
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
int hitCount;
struct lm *lm = lmInit(0);

gfTransFindClumps(gfs, qSeq, clumps, lm, &hitCount);
for (frame=0; frame<3; ++frame)
    {
    for (clump = clumps[frame]; clump != NULL; clump = clump->next)
	{
	clumpToHspRange(clump, qSeq, tileSize, frame, NULL, &rangeList, TRUE, FALSE);
	}
    }
slReverse(&rangeList);
slSort(&rangeList, gfRangeCmpTarget);
rangeList = gfRangesBundle(rangeList, ffIntronMax/3);
for (range = rangeList; range != NULL; range = range->next)
    {
    targetSeq = range->tSeq;
    t3 = hashMustFindVal(t3Hash, targetSeq->name);
    AllocVar(bun);
    bun->qSeq = qSeq;
    bun->genoSeq = targetSeq;
    bun->ffList = gfRangesToFfItem(range->components, qSeq);
    bun->isProt = TRUE;
    bun->t3List = t3;
    ssStitch(bun, ffCdna, minMatch, ssAliCount);
    saveAlignments(targetSeq->name, t3->seq->size, 0, 
	bun, t3Hash, FALSE, tIsRc, ffCdna, minMatch, out);
    ssBundleFree(&bun);
    }
gfRangeFreeList(&rangeList);
for (frame=0; frame<3; ++frame)
    gfClumpFreeList(&clumps[frame]);
lmCleanup(&lm);
}

void rangeCoorTimes3(struct gfRange *rangeList)
/* Multiply coordinates on range list times three (to go from
 * amino acid to nucleotide. */
{
struct gfRange *range;
for (range = rangeList; range != NULL; range = range->next)
    {
    range->qStart *= 3;
    range->qEnd *= 3;
    range->tStart = range->tStart*3;
    range->tEnd = range->tEnd*3;
    }
}

static void loadHashT3Ranges(struct gfRange *rangeList, 
	char *tSeqDir, struct hash *tFileCache, int qSeqSize, boolean isRc, 
	struct hash **retT3Hash, struct dnaSeq **retSeqList,
	struct slRef **retT3RefList)
/* Load DNA in ranges into memory, and put translation in a hash
 * that gets returned. */
{
struct hash *t3Hash = newHash(10);
struct dnaSeq *targetSeq, *tSeqList = NULL;
struct slRef *t3RefList = NULL;
struct gfRange *range;

for (range = rangeList; range != NULL; range = range->next)
    {
    struct trans3 *t3, *oldT3;

    targetSeq = gfiExpandAndLoadCached(range, tFileCache,
    	tSeqDir, qSeqSize*3, &range->tTotalSize, TRUE, isRc, usualExpansion);
    slAddHead(&tSeqList, targetSeq);
    freez(&targetSeq->name);
    targetSeq->name = cloneString(range->tName);
    t3 = trans3New(targetSeq);
    refAdd(&t3RefList, t3);
    t3->start = range->tStart;
    t3->end = range->tEnd;
    t3->nibSize = range->tTotalSize;
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
*retT3Hash = t3Hash;
*retSeqList = tSeqList;
*retT3RefList = t3RefList;
}


void gfAlignTrans(int *pConn, char *tSeqDir, aaSeq *seq, int minMatch, 
    struct hash *tFileCache, struct gfOutput *out)
/* Search indexed translated genome on server with an amino acid sequence. 
 * Then load homologous bits of genome locally and do detailed alignment.
 * Call 'outFunction' with each alignment that is found. */
{
struct ssBundle *bun;
struct gfClump *clumps[2][3], *clump;
struct gfRange *rangeList = NULL, *range, *rl;
struct dnaSeq *targetSeq, *tSeqList = NULL;
char targetName[PATH_LEN];
int tileSize;
int frame, isRc = 0;
struct hash *t3Hash = NULL;
struct slRef *t3RefList = NULL, *ref;
struct gfSeqSource *ssList = NULL, *ss;
struct trans3 *t3;
struct lm *lm = lmInit(0);

/* Get clumps from server. */
gfQuerySeqTrans(*pConn, seq, clumps, lm, &ssList, &tileSize);
close(*pConn);
*pConn = -1;

for (isRc = 0; isRc <= 1;  ++isRc)
    {
    /* Figure out which parts of sequence we need to load. */
    for (frame = 0; frame < 3; ++frame)
	{
	rl = seqClumpToRangeList(clumps[isRc][frame], frame);
	rangeList = slCat(rangeList, rl);
	}
    /* Convert from amino acid to nucleotide coordinates. */
    rangeCoorTimes3(rangeList);
    slSort(&rangeList, gfRangeCmpTarget);
    rangeList = gfRangesBundle(rangeList, ffIntronMax);
    loadHashT3Ranges(rangeList, tSeqDir, tFileCache, seq->size, 
    	isRc, &t3Hash, &tSeqList, &t3RefList);

    /* The old range list was not very precise - it was just to get
     * the DNA loaded.  */
    gfRangeFreeList(&rangeList);


    /* Patch up clump list and associated sequence source to refer
     * to bits of genome loaded into memory.  Create new range list
     * by extending hits in clumps. */
    for (frame = 0; frame < 3; ++frame)
	{
	for (clump = clumps[isRc][frame]; clump != NULL; clump = clump->next)
	    {
	    struct gfSeqSource *ss = clump->target;
	    t3 = trans3Find(t3Hash, clumpTargetName(clump), clump->tStart*3, clump->tEnd*3);
	    ss->seq = t3->trans[frame];
	    ss->start = t3->start/3;
	    ss->end = t3->end/3;
	    clumpToHspRange(clump, seq, tileSize, frame, t3, &rangeList, TRUE, FALSE);
	    }
	}
    slReverse(&rangeList);
    slSort(&rangeList, gfRangeCmpTarget);
    rangeList = gfRangesBundle(rangeList, ffIntronMax/3);

    /* Do detailed alignment of each of the clustered ranges. */
    for (range = rangeList; range != NULL; range = range->next)
	{
	targetSeq = range->tSeq;
	AllocVar(bun);
	bun->qSeq = seq;
	bun->genoSeq = targetSeq;
	bun->ffList = gfRangesToFfItem(range->components, seq);
	bun->isProt = TRUE;
	t3 = hashMustFindVal(t3Hash, range->tName);
	bun->t3List = t3;
	ssStitch(bun, ffCdna, minMatch, ssAliCount);
	getTargetName(range->tName, out->includeTargetFile, targetName);
	saveAlignments(targetName, t3->nibSize, 0, 
	    bun, t3Hash, FALSE, isRc, ffCdna, minMatch, out);
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
lmCleanup(&lm);
}

void untranslateRangeList(struct gfRange *rangeList, int qFrame, int tFrame, 
	struct hash *t3Hash, struct trans3 *t3, int tOffset)
/* Translate coordinates from protein to dna. */
{
struct gfRange *range;
for (range = rangeList; range != NULL; range = range->next)
    {
    range->qStart = 3*range->qStart + qFrame;
    range->qEnd = 3*range->qEnd + qFrame;
    range->tStart = 3*range->tStart + tFrame;
    range->tEnd = 3*range->tEnd + tFrame;
    if (t3Hash)
	t3 = trans3Find(t3Hash, range->tSeq->name, range->tStart + tOffset, range->tEnd + tOffset);
    range->tSeq = t3->seq;
    range->t3 = t3;
    }
}

void gfAlignTransTrans(int *pConn, char *tSeqDir, struct dnaSeq *qSeq, 
	boolean qIsRc, int minMatch, struct hash *tFileCache, 
	struct gfOutput *out, boolean isRna)
/* Search indexed translated genome on server with an dna sequence.  Translate
 * this sequence in three frames. Load homologous bits of genome locally
 * and do detailed alignment.  Call 'outFunction' with each alignment
 * that is found. */
{
struct gfClump *clumps[2][3][3], *clump;
char targetName[PATH_LEN];
int qFrame, tFrame, tIsRc;
struct gfSeqSource *ssList = NULL, *ss;
struct lm *lm = lmInit(0);
int tileSize;
struct gfRange *rangeList = NULL, *rl, *range;
struct trans3 *qTrans = trans3New(qSeq), *t3;
struct slRef *t3RefList = NULL, *t3Ref;
struct hash *t3Hash = NULL;
struct dnaSeq *tSeqList = NULL;
enum ffStringency stringency = (isRna ? ffCdna : ffLoose);

/* Query server for clumps. */
gfQuerySeqTransTrans(*pConn, qSeq, clumps, lm, &ssList, &tileSize);
close(*pConn);
*pConn = -1;

for (tIsRc=0; tIsRc <= 1; ++tIsRc)
    {
    /* Figure out which ranges need to be loaded and load them. */
    for (qFrame = 0; qFrame < 3; ++qFrame)
        {
        for (tFrame = 0; tFrame < 3; ++tFrame)
            {
	    rl = seqClumpToRangeList(clumps[tIsRc][qFrame][tFrame], tFrame);
	    rangeList = slCat(rangeList, rl);
	    }
	}
    rangeCoorTimes3(rangeList);
    slSort(&rangeList, gfRangeCmpTarget);
    rangeList = gfRangesBundle(rangeList, ffIntronMax);
    loadHashT3Ranges(rangeList, tSeqDir, tFileCache,
    	qSeq->size/3, tIsRc, &t3Hash, &tSeqList, &t3RefList);

    /* The old range list was not very precise - it was just to get
     * the DNA loaded.  */
    gfRangeFreeList(&rangeList);

    /* Patch up clump list and associated sequence source to refer
     * to bits of genome loaded into memory.  Create new range list
     * by extending hits in clumps. */
    for (qFrame = 0; qFrame < 3; ++qFrame)
	{
	for (tFrame = 0; tFrame < 3; ++tFrame)
	    {
	    for (clump = clumps[tIsRc][qFrame][tFrame]; clump != NULL; clump = clump->next)
		{
		struct gfSeqSource *ss = clump->target;
		struct gfRange *rangeSet = NULL;
		t3 = trans3Find(t3Hash, clumpTargetName(clump), clump->tStart*3, clump->tEnd*3);
		ss->seq = t3->trans[tFrame];
		ss->start = t3->start/3;
		ss->end = t3->end/3;
		clumpToHspRange(clump, qTrans->trans[qFrame], tileSize, tFrame, t3, &rangeSet, TRUE, FALSE);
		untranslateRangeList(rangeSet, qFrame, tFrame, NULL, t3, t3->start);
		rangeList = slCat(rangeSet, rangeList);
		}
	    }
	}
    slReverse(&rangeList);
    slSort(&rangeList, gfRangeCmpTarget);
    rangeList = gfRangesBundle(rangeList, ffIntronMax);

    for (range = rangeList; range != NULL; range = range->next)
	{
	struct dnaSeq *targetSeq = range->tSeq;
	struct ssBundle *bun;

	AllocVar(bun);
	bun->qSeq = qSeq;
	bun->genoSeq = targetSeq;
	bun->ffList = gfRangesToFfItem(range->components, qSeq);
	ssStitch(bun, stringency, minMatch, ssAliCount);
	getTargetName(range->tName, out->includeTargetFile, targetName);
	t3 = range->t3;
	saveAlignments(targetName, t3->nibSize, t3->start, 
	    bun, NULL, qIsRc, tIsRc, stringency, minMatch, out);
	ssBundleFree(&bun);
	}

    /* Cleanup for this strand of database. */
    gfRangeFreeList(&rangeList);
    freeHash(&t3Hash);
    for (t3Ref = t3RefList; t3Ref != NULL; t3Ref = t3Ref->next)
        {
	struct trans3 *t3 = t3Ref->val;
	trans3Free(&t3);
	}
    slFreeList(&t3RefList);
    freeDnaSeqList(&tSeqList);
    }
trans3Free(&qTrans);
for (ss = ssList; ss != NULL; ss = ss->next)
    freeMem(ss->fileName);
slFreeList(&ssList);
lmCleanup(&lm);
}


static struct ssBundle *gfTransTransFindBundles(struct genoFind *gfs[3], struct dnaSeq *qSeq, 
	struct hash *t3Hash, boolean isRc, int minMatch, boolean isRna)
/* Look for alignment to three translations of qSeq in three translated reading frames. 
 * Save alignment via outFunction/outData. */
{
struct trans3 *qTrans = trans3New(qSeq);
int qFrame, tFrame;
struct gfClump *clumps[3][3], *clump;
struct gfRange *rangeList = NULL, *range;
int tileSize = gfs[0]->tileSize;
bioSeq *targetSeq;
struct ssBundle *bun, *bunList = NULL;
int hitCount;
struct lm *lm = lmInit(0);
enum ffStringency stringency = (isRna ? ffCdna : ffLoose);

gfTransTransFindClumps(gfs, qTrans->trans, clumps, lm, &hitCount);
for (qFrame = 0; qFrame<3; ++qFrame)
    {
    for (tFrame=0; tFrame<3; ++tFrame)
	{
	for (clump = clumps[qFrame][tFrame]; clump != NULL; clump = clump->next)
	    {
	    struct gfRange *rangeSet = NULL;
	    clumpToHspRange(clump, qTrans->trans[qFrame], tileSize, tFrame, NULL, &rangeSet, TRUE, FALSE);
	    untranslateRangeList(rangeSet, qFrame, tFrame, t3Hash, NULL, 0);
	    rangeList = slCat(rangeSet, rangeList);
	    }
	}
    }
slSort(&rangeList, gfRangeCmpTarget);
rangeList = gfRangesBundle(rangeList, 2000);
for (range = rangeList; range != NULL; range = range->next)
    {
    targetSeq = range->tSeq;
    AllocVar(bun);
    bun->qSeq = qSeq;
    bun->genoSeq = targetSeq;
    bun->ffList = gfRangesToFfItem(range->components, qSeq);
    ssStitch(bun, stringency, minMatch, ssAliCount);
    slAddHead(&bunList, bun);
    }
for (qFrame = 0; qFrame<3; ++qFrame)
    for (tFrame=0; tFrame<3; ++tFrame)
	gfClumpFreeList(&clumps[qFrame][tFrame]);
gfRangeFreeList(&rangeList);
trans3Free(&qTrans);
lmCleanup(&lm);
slReverse(&bunList);
return bunList;
}

static void addToBigBundleList(struct ssBundle **pOneList, struct hash *bunHash, 
	struct ssBundle **pBigList, struct dnaSeq *query)
/* Add bundles in one list to bigList, consolidating bundles that refer
 * to the same target sequence.  This will destroy oneList in the process. */
{
struct ssBundle *oneBun, *bigBun;

for (oneBun = *pOneList; oneBun != NULL; oneBun = oneBun->next)
    {
    char *name = oneBun->genoSeq->name;
    if ((bigBun = hashFindVal(bunHash, name)) == NULL)
        {
	AllocVar(bigBun);
	slAddHead(pBigList, bigBun);
	hashAdd(bunHash, name, bigBun);
	bigBun->qSeq = query;
	bigBun->genoSeq = oneBun->genoSeq;
	bigBun->isProt = oneBun->isProt;
	bigBun->avoidFuzzyFindKludge = oneBun->avoidFuzzyFindKludge;
	}
    bigBun->ffList = slCat(bigBun->ffList, oneBun->ffList);
    oneBun->ffList = NULL;
    }
ssBundleFreeList(pOneList);
}

static boolean jiggleSmallExons(struct ffAli *ali, struct dnaSeq *nSeq, struct dnaSeq *hSeq)
/* See if can jiggle small exons to match splice sites a little
 * better. */
{
struct ffAli *left, *mid, *right;
int orient;
boolean creeped = FALSE;

if (ffAliCount(ali) < 3)
    return FALSE;
orient = ffIntronOrientation(ali);
left = ali;
mid = left->right;
right = mid->right;
while (right != NULL)
    {
    int midSizeN = mid->nEnd - mid->nStart;
    if (midSizeN < 10 && mid->hStart - left->hEnd > 1 && right->hStart - mid->hEnd > 1)
        {
	DNA *spLeft, *spRight;	/* Splice sites on either side of exon. */
	DNA exonX[10+2+2];    /* Storage for exon with splice sites. */
	DNA *match;
	static int creeps[4][2] = { {2, 2}, {2, 1}, {1, 2}, {1, 1},};
	int creepIx, creepL, creepR;
	DNA *hs = mid->hStart, *he = mid->hEnd;
	DNA *hMin = left->hEnd,  *hMax = right->hStart;
	if (orient >= 0)
	    {
	    spLeft = "ag";
	    spRight = "gt";
	    }
	else
	    {
	    spLeft = "ac";
	    spRight = "ct";
	    }
        for (creepIx=0; creepIx<4; ++creepIx)
	    {
	    creepL = creeps[creepIx][0];
	    creepR = creeps[creepIx][1];
	    /* Check to see if we already match consensus, and if so just bail. */
	    if (hs[-1] == spLeft[1] && he[0] == spRight[0])
	        {
		if ((creepL == 1 || hs[-2] == spLeft[0]) 
			&& (creepR == 1 || he[1] == spRight[1]))
		    {
		    break;
		    }
		}
	    memcpy(exonX, spLeft + 2 - creepL, creepL);
	    memcpy(exonX + creepL, mid->nStart, midSizeN);
	    memcpy(exonX + creepL + midSizeN, spRight, creepR);
	    match = memMatch(exonX, midSizeN + creepR + creepL, hMin, hMax - hMin);
	    if (match != NULL)
	        {
		mid->hStart = match + creepL;
		mid->hEnd = mid->hStart + (he - hs);
		creeped = TRUE;
		break;
		}
	    }
	}
    left = mid;
    mid = right;
    right = right->right;
    }
if (creeped)
    ffSlideIntrons(ali);
return creeped;
}

static struct ffAli *refineSmallExons(struct ffAli *ff, 
	struct dnaSeq *nSeq, struct dnaSeq *hSeq)
/* Tweak small exons slightly - refining positions to match splice
 * sites if possible and looking a little harder for small first
 * and last exons. */
{
if (jiggleSmallExons(ff, nSeq, hSeq))
    ff = ffRemoveEmptyAlis(ff, TRUE);
return ff;
}

static void refineSmallExonsInBundle(struct ssBundle *bun)
/* Tweak small exons slightly - refining positions to match splice
 * sites if possible and looking a little harder for small first
 * and last exons. */
{
struct ssFfItem *fi;    /* Item list - memory owned by bundle. */

for (fi = bun->ffList; fi != NULL; fi = fi->next)
    {
    fi->ff = refineSmallExons(fi->ff, bun->qSeq, bun->genoSeq);
    }
}

#ifdef DEBUG
void dumpBunList(struct ssBundle *bunList)
/* Write out summary info on bundle list. */
{
int totalItems = 0;
int totalBlocks = 0;
int totalBases = 0;
struct ssBundle *bun;
for (bun = bunList; bun != NULL; bun = bun->next)
    {
    struct ssFfItem *item;
    for (item = bun->ffList; item != NULL; item = item->next)
	{
	printf("item: ");
	struct ffAli *ff;
	for (ff = item->ff; ff != NULL; ff = ff->right)
	    {
	    totalBases += ff->hEnd - ff->hStart;
	    totalBlocks += 1;
	    printf("%d,", ff->hEnd - ff->hStart);
	    }
	printf("\n");
	totalItems += 1;
	}
    }
printf("total bundles %d, alignments %d, blocks %d, bases %d\n", 
    slCount(bunList), totalItems, totalBlocks, totalBases);
}
#endif /* DEBUG */

static void gfAlignSomeClumps(struct genoFind *gf,  struct gfClump *clumpList, 
    bioSeq *seq, boolean isRc,  int minMatch, 
    struct gfOutput *out, boolean isProt, enum ffStringency stringency);

void gfLongDnaInMem(struct dnaSeq *query, struct genoFind *gf, 
   boolean isRc, int minScore, Bits *qMaskBits, 
   struct gfOutput *out, boolean fastMap, boolean band)
/* Chop up query into pieces, align each, and stitch back
 * together again. */
{
int hitCount;
int maxSize = 5000;
int preferredSize = 4500;
int overlapSize = 250;
struct dnaSeq subQuery = *query;
struct lm *lm = lmInit(0);
int subOffset, subSize, nextOffset;
DNA saveEnd, *endPos;
struct ssBundle *oneBunList = NULL, *bigBunList = NULL, *bun;
struct hash *bunHash = newHash(8);

for (subOffset = 0; subOffset<query->size; subOffset = nextOffset)
    {
    struct gfClump *clumpList;
    struct gfRange *rangeList = NULL;

    /* Figure out size of this piece.  If query is
     * maxSize or less do it all.   Otherwise just
     * do prefered size, and set it up to overlap
     * with surrounding pieces by overlapSize.  */
    if (subOffset == 0 && query->size <= maxSize)
	nextOffset = subSize = query->size;
    else
        {
	subSize = preferredSize;
	if (subSize + subOffset >= query->size)
	    {
	    subSize = query->size - subOffset;
	    nextOffset = query->size;
	    }
	else
	    {
	    nextOffset = subOffset + preferredSize - overlapSize;
	    }
	}
    subQuery.dna = query->dna + subOffset;
    subQuery.size = subSize;
    endPos = &subQuery.dna[subSize];
    saveEnd = *endPos;
    *endPos = 0;
    if (band)
	{
	oneBunList = ffSeedExtInMem(gf, &subQuery, qMaskBits, subOffset, lm, minScore, isRc);
	}
    else
	{
	clumpList = gfFindClumpsWithQmask(gf, &subQuery, qMaskBits, subOffset, lm, &hitCount);
	if (fastMap)
	    {
	    oneBunList = fastMapClumpsToBundles(gf, clumpList, &subQuery);
	    }
	else
	    {
	    oneBunList = gfClumpsToBundles(clumpList, isRc, &subQuery, minScore, &rangeList);
	    gfRangeFreeList(&rangeList);
	    }
	gfClumpFreeList(&clumpList);
	}
    addToBigBundleList(&oneBunList, bunHash, &bigBunList, query);
    *endPos = saveEnd;
    }
#ifdef DEBUG
dumpBunList(bigBunList);
#endif /* DEBUG */
for (bun = bigBunList; bun != NULL; bun = bun->next)
    {
    ssStitch(bun, ffCdna, minScore, ssAliCount);
    if (!fastMap && !band)
	refineSmallExonsInBundle(bun);
    saveAlignments(bun->genoSeq->name, bun->genoSeq->size, 0, 
	bun, NULL, isRc, FALSE, ffCdna, minScore, out);
    }
ssBundleFreeList(&bigBunList);
freeHash(&bunHash);
lmCleanup(&lm);
}


void gfLongTransTransInMem(struct dnaSeq *query, struct genoFind *gfs[3], 
   struct hash *t3Hash, boolean qIsRc, boolean tIsRc, boolean qIsRna,
   int minScore, struct gfOutput *out)
/* Chop up query into pieces, align each in translated space, and stitch back
 * together again as nucleotides. */
{
enum ffStringency stringency = (qIsRna ? ffCdna : ffLoose);
int maxSize = 1500;
int preferredSize = 1200;	/* PreferredSize - overlapSize might need to be multiple of 3. */
int overlapSize = 270;
struct dnaSeq subQuery = *query;
int subOffset, subSize, nextOffset;
DNA saveEnd, *endPos;
struct ssBundle *oneBunList = NULL, *bigBunList = NULL, *bun;
struct hash *bunHash = newHash(8);

for (subOffset = 0; subOffset<query->size; subOffset = nextOffset)
    {
    /* Figure out size of this piece.  If query is
     * maxSize or less do it all.   Otherwise just
     * do prefered size, and set it up to overlap
     * with surrounding pieces by overlapSize.  */
    if (subOffset == 0 && query->size <= maxSize)
	nextOffset = subSize = query->size;
    else
        {
	subSize = preferredSize;
	if (subSize + subOffset >= query->size)
	    {
	    subSize = query->size - subOffset;
	    nextOffset = query->size;
	    }
	else
	    {
	    nextOffset = subOffset + preferredSize - overlapSize;
	    }
	}
    subQuery.dna = query->dna + subOffset;
    subQuery.size = subSize;
    endPos = &subQuery.dna[subSize];
    saveEnd = *endPos;
    *endPos = 0;
    oneBunList = gfTransTransFindBundles(gfs, &subQuery, t3Hash, qIsRc, minScore, qIsRna);
    addToBigBundleList(&oneBunList, bunHash, &bigBunList, query);
    *endPos = saveEnd;
    }
for (bun = bigBunList; bun != NULL; bun = bun->next)
    {
    ssStitch(bun, ffCdna, minScore, ssAliCount);
    saveAlignments(bun->genoSeq->name, bun->genoSeq->size, 0, 
	bun, NULL, qIsRc, tIsRc, stringency, minScore, out);
    }
hashFree(&bunHash);
ssBundleFreeList(&bigBunList);
}

