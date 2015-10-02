/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* ggCluster - This takes as input a list of mRNAs and produces
 * a list of mRNA clusters.  mRNAs are lumped into a cluster
 * if exons on the same strand overlap. */

#include "common.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "geneGraph.h"
#include "ggPrivate.h"




static void updateActiveClusters(struct ggMrnaCluster **pActiveClusters, 
    struct ggMrnaCluster **pFinishedClusters, char *strand, int tStart)
/* Move clusters that could no longer match off active list onto finished list. */
{
struct ggMrnaCluster *mc, *ggNext, *newActive = NULL;
for (mc = *pActiveClusters; mc != NULL; mc = ggNext)
    {
    ggNext = mc->next;
    if (differentString(mc->strand, strand) || mc->tEnd < tStart)
	{
	slAddHead(pFinishedClusters, mc);
	}
    else
	{
	slAddHead(&newActive, mc);
	}
    }
slReverse(&newActive);
*pActiveClusters = newActive;
}

static boolean isGoodIntron(struct dnaSeq *targetSeq, 
    struct ggMrnaBlock *a, 
    struct ggMrnaBlock *b,  boolean isRev)
/* Return true if there's a good intron between a and b. If true will update strand.*/
{
if (a->qEnd == b->qStart && b->tStart - a->tEnd >= 30)
    {
    if (targetSeq)
	{
	DNA *iStart = targetSeq->dna + a->tEnd;
	DNA *iEnd = targetSeq->dna + b->tStart;
	if (isRev)
	    {
	    if (iStart[0] == 'c' && iStart[1] == 't' && iEnd[-2] == 'a' && iEnd[-1] == 'c')
		return TRUE;
	    if (iStart[0] == 'c' && iStart[1] == 't' && iEnd[-2] == 'g' && iEnd[-1] == 'c')
		return TRUE;
	    }
	else
	    {
	    if (iStart[0] == 'g' && iStart[1] == 't' && iEnd[-2] == 'a' && iEnd[-1] == 'g')
		return TRUE;
	    if (iStart[0] == 'g' && iStart[1] == 'c' && iEnd[-2] == 'a' && iEnd[-1] == 'g')
		return TRUE;
	    }
	}
    else
        return TRUE;	/* Done all the checking we can */
    }
return FALSE;
}

static struct ggMrnaCluster *finishClusterOfOne(
	struct ggMrnaAli *ma, struct dnaSeq *genoSeq,
	struct ggVertex *vertices, int vCount)
/* Finish making up a ggMrnaCluster from a single alignment
 * once have already calculated the vertices that go into it. */
{
struct ggMrnaCluster *mc;
struct maRef *ref;
struct ggAliInfo *da;

AllocVar(mc);
AllocVar(ref);
ref->ma  = ma;
mc->refList = ref;
AllocVar(da);
AllocArray(da->vertices, vCount);
CopyArray(vertices, da->vertices, vCount);
da->vertexCount = vCount;
da->ma = ma;
mc->mrnaList = da;
mc->tName = cloneString(ma->tName);
safef(mc->strand, sizeof(mc->strand), "%s", ma->strand);
mc->tStart = ma->tStart;
mc->tEnd = ma->tEnd;
mc->genoSeq = genoSeq;
return mc;
}



struct ggMrnaCluster *ggMrnaClusterOfOne(struct ggMrnaAli *ma, struct dnaSeq *genoSeq)
/* Make up a ggMrnaCluster with just one thing on it. Looks at each
 intron to see if it is good before determining which type a vertex
 should be (hard or soft) */
{
static struct ggVertex *vertices = NULL;	/* Resized array. */
static int vAlloc = 0;
int vCount = 0;

struct ggMrnaBlock *blocks = ma->blocks;
int blockCount = ma->blockCount;
int isRev = ma->orientation < 0;
int i;
struct ggVertex *v;

for(i=0; i<blockCount; i++) 
    {
    struct ggMrnaBlock *block = blocks+i;
    if (vCount+2 > vAlloc)
	{
	if (vAlloc == 0)
	    vAlloc = 128;
	else
	    vAlloc <<= 1;
	ExpandArray(vertices, vCount, vAlloc);
	}
    /* Enter first vertex, checking to see if it has consensus splice sites. */
    v = &vertices[vCount++];
    v->position = block->tStart;
    if (i == 0 || (!isGoodIntron(genoSeq, block-1, block, isRev)))
	v->type = ggSoftStart;
    else
	v->type = ggHardStart;

    /* Enter second vertex, checking to see if it has consensus splice sites. */
    v = &vertices[vCount++];
    v->position = block->tEnd;
    if(i == blockCount-1 || (!isGoodIntron(genoSeq, block, block+1, isRev)))
	v->type = ggSoftEnd;
    else
	v->type = ggHardEnd;
    }
/* Allocate and fill in ggMrnaCluster. */
return finishClusterOfOne(ma, genoSeq, vertices, vCount);
}


struct ggMrnaCluster *ggMrnaSoftClusterOfOne(struct ggMrnaAli *ma, 
	struct dnaSeq *genoSeq)
/* Make up a ggMrnaCluster with just one thing on it. 
 * Allow internal soft edges. */
{
static struct ggVertex *vertices = NULL;	/* Resized array. */
static int vAlloc = 0;
int vCount = 0;

struct ggMrnaBlock *blocks = ma->blocks;
int i, blockCount = ma->blockCount;
struct ggVertex *v;
int isRev = ma->orientation < 0;

/* Do initial vertex allocation. */
if (vAlloc == 0)
    {
    vAlloc = 128;
    AllocArray(vertices, vAlloc);
    }

/* Put start of first block in - always soft. */
v = &vertices[vCount++];
v->position = blocks->tStart;
v->type = ggSoftStart;

/* Loop around putting in block end, next block start. */
for (i=1; i<blockCount; ++i)
    {
    /* Expand buffer if need be to handle these two new
     * points, and the one at the end too. */
    if (vCount+3 > vAlloc)
	{
	vAlloc <<= 1;
	ExpandArray(vertices, vCount, vAlloc);
	}

    v = &vertices[vCount];
    v->position = blocks->tEnd;
    if (isGoodIntron(genoSeq, blocks, blocks+1, isRev))
	{
	v->type = ggHardEnd;
	v += 1;
	v->type = ggHardStart;
	}
    else
        {
	v->type = ggSoftEnd;
	v += 1;
	v->type = ggSoftStart;
	}
    blocks += 1;
    v->position = blocks->tStart;
    vCount += 2;
    }

/* Put in end of last block - always soft. */
v = &vertices[vCount++];
v->position = blocks->tEnd;
v->type = ggSoftEnd;

/* Allocate and fill in ggMrnaCluster. */
return finishClusterOfOne(ma, genoSeq, vertices, vCount);
}

struct ggMrnaCluster *ggMrnaSoftFilteredClusterOfOne(struct ggMrnaAli *ma, 
	struct dnaSeq *genoSeq, int minExonSize, int minNonSpliceExon)
/* Make up a ggMrnaCluster with just one thing on it. 
 * All edges here will be soft (not intended for alt-splicing use) */
{
static struct ggVertex *vertices = NULL;	/* Resized array. */
static int vAlloc = 0;
int vCount = 0;
int firstExon = -1, lastExon = -1;
struct ggMrnaBlock *blocks = ma->blocks, *b;
int i, size, blockCount = ma->blockCount;
struct ggVertex *v;
int isRev = ma->orientation < 0;
struct ggMrnaCluster *mc;

/* Do initial vertex allocation. */
if (vAlloc == 0)
    {
    vAlloc = 128;
    AllocArray(vertices, vAlloc);
    }

/* Find first exon which is either large enough or
 * precedes a good intron. */
for (i=0; i<blockCount; ++i)
    {
    size = blocks[i].tEnd - blocks[i].tStart;
    if (size >= minNonSpliceExon)
        {
	firstExon = i;
	break;
	}
    if (i < blockCount-1 && size >= minExonSize && 
	    isGoodIntron(genoSeq, blocks+i, blocks+i+1, isRev))
	{
	firstExon = i;
	break;
	}
    }
if (firstExon < 0)
    return NULL;

/* Find last exon which is either large enough or
 * follows a good intron. */
for (i=blockCount-1; i>= 0; --i)
    {
    size = blocks[i].tEnd - blocks[i].tStart;
    if (size >= minNonSpliceExon)
        {
	lastExon = i;
	break;
	}
    if (i > 0 && size >= minExonSize && 
	    isGoodIntron(genoSeq, blocks+i-1, blocks+i, isRev))
	{
	lastExon = i;
	break;
	}
    }
if (lastExon < 0)
    return NULL;

for (i=firstExon; i<=lastExon; ++i)
    {
    if (vCount+2 > vAlloc)
	{
	vAlloc <<= 1;
	ExpandArray(vertices, vCount, vAlloc);
	}
    b = blocks+i;
    size = b->tEnd - b->tStart;
    if (size >= minExonSize)
        {
	v = vertices + vCount;
	v->position = b->tStart;
	v->type = ggSoftStart;
	v += 1;
	v->position = b->tEnd;
	v->type = ggSoftEnd;
	vCount += 2;
	}
    }
/* Allocate and fill in ggMrnaCluster. */
mc = finishClusterOfOne(ma, genoSeq, vertices, vCount);
mc->tStart = blocks[firstExon].tStart;
mc->tEnd = blocks[lastExon].tEnd;
return mc;
}


static boolean exonOverlaps(int eStart, int eEnd, struct ggMrnaCluster *mc)
/* Returns TRUE if exon defined by eStart/eEnd overlaps with any
 * exon in cluster. */
{
struct ggAliInfo *da;

for (da = mc->mrnaList; da != NULL; da = da->next)
    {
    struct ggVertex *v = da->vertices;
    int vCount = da->vertexCount;
    int i;
    for (i=0; i<vCount; i+=2,v+=2)
	{
	int mStart = v[0].position;
	int mEnd = v[1].position;
	int s = max(eStart, mStart);
	int e = min(eEnd, mEnd);
	if (e - s > 0)
	    return TRUE;
	}
    }
return FALSE;
}

static boolean clustersOverlap(struct ggMrnaCluster *a, struct ggMrnaCluster *b)
/* Return TRUE if a and b have an overlapping exon. */
{
struct ggAliInfo *da;

for (da = a->mrnaList; da != NULL; da = da->next)
    {
    struct ggVertex *v = da->vertices;
    int vCount = da->vertexCount;
    int i;
    for (i=0; i<vCount; i+=2,v+=2)
	{
	if (exonOverlaps(v[0].position, v[1].position, b))
	    return TRUE;
	}
    }
return FALSE;
}

static struct ggMrnaCluster *findMergableClusters(struct ggMrnaCluster **pActiveClusters, 
	struct ggMrnaCluster *newMc)
/* Remove list of clusters that are mergable with cluster from active list
 * and return it as list.  Assumes that only clusters on same strand of same clone
 * are on active list. */
{
struct ggMrnaCluster *mc, *mcNext, *newActive = NULL, *mergeList = NULL;
for (mc = *pActiveClusters; mc != NULL; mc = mcNext)
    {
    mcNext = mc->next;
    if (clustersOverlap(mc, newMc))
	{
	slAddHead(&mergeList, mc);
	}
    else
	{
	slAddHead(&newActive, mc);
	}
    }
slReverse(&newActive);
*pActiveClusters = newActive;
slReverse(&mergeList);
return mergeList;
}


void ggMrnaClusterMerge(struct ggMrnaCluster *aMc, struct ggMrnaCluster *bMc)
/* Merge bMc into aMc.  Afterwords aMc will be bigger and
 * bMc will be gone. */
{
aMc->refList = slCat(aMc->refList, bMc->refList);
aMc->mrnaList = slCat(aMc->mrnaList, bMc->mrnaList);
if (aMc->tStart > bMc->tStart) 
    aMc->tStart = bMc->tStart;
if (aMc->tEnd < bMc->tEnd)
    aMc->tEnd = bMc->tEnd;
freez(&bMc->tName);
freeMem(bMc);
}

static struct ggMrnaCluster *clustersFromMas(struct ggMrnaAli *maList, struct dnaSeq *genoSeq)
/* Given a sorted ggMrnaAli list return list of mRNA clusters. */
{
struct ggMrnaCluster *activeClusters = NULL;	/* List that can still add to. */
struct ggMrnaCluster *finishedClusters = NULL;    /* List that's all done. */
struct ggMrnaAli *ma;

for (ma = maList; ma != NULL; ma = ma->next)
    {
    struct ggMrnaCluster *mergableClusters = NULL, *newMc = NULL;
    updateActiveClusters(&activeClusters, &finishedClusters, ma->strand, ma->tStart);
    newMc = ggMrnaClusterOfOne(ma, genoSeq);    
    if (newMc)
	{
	mergableClusters = findMergableClusters(&activeClusters, newMc);
	if (mergableClusters == NULL)
	    {
	    slAddHead(&activeClusters, newMc);
	    }
	else
	    {
	    struct ggMrnaCluster *mergeHead = mergableClusters;
	    struct ggMrnaCluster *mc, *nextGg;
	    ggMrnaClusterMerge(mergeHead, newMc);
	    for (mc = mergeHead->next; mc != NULL; mc = nextGg)
		{
		nextGg = mc->next;
		ggMrnaClusterMerge(mergableClusters, mc);
		}
	    slAddHead(&activeClusters, mergeHead);
	    }
	}
    }
finishedClusters = slCat(activeClusters, finishedClusters);
slReverse(&finishedClusters);
return finishedClusters;
}


struct ggMrnaCluster *ggClusterMrna(struct ggMrnaInput *ci)
/* Make a list of clusters from ci. */
{
struct ggMrnaCluster *mc = NULL;
slSort(&ci->maList, cmpGgMrnaAliTargetStart);
mc =  clustersFromMas(ci->maList, ci->genoSeq);
return mc;
}

void freeDenseAliInfo(struct ggAliInfo **pDa)
/* Free up a ggAliInfo */
{
struct ggAliInfo *da;
if ((da = *pDa) != NULL)
    {
    freeMem(da->vertices);
    freez(pDa);
    }
}

void freeDenseAliInfoList(struct ggAliInfo **pDaList)
/* Free up a whole ggAliInfo list. */
{
struct ggAliInfo *da, *next;
for (da = *pDaList; da != NULL; da = next)
    {
    next = da->next;
    freeDenseAliInfo(&da);
    }
*pDaList = NULL;
}

void maRefFree(struct maRef **pMaRef)
{
struct maRef *ma;
if ((ma = *pMaRef) != NULL)
    {
    ggMrnaAliFreeList(&ma->ma);
    }
}

void maRefFreeList(struct maRef **pMaRef)
{
struct maRef *ma, *next;
for(ma = *pMaRef; ma != NULL; ma = next)
    {
    next = ma->next;
    maRefFree(&ma);
    }
*pMaRef = NULL;
}

static void freeMrnaCluster(struct ggMrnaCluster **pMc)
/* Free a single mrna cluster */
{
struct ggMrnaCluster *mc;

if ((mc = *pMc) != NULL)
    {
    slFreeList(&mc->refList);
    freeDenseAliInfoList(&mc->mrnaList);
    freez(&mc->tName);
    freez(pMc);
    }
}

void ggFreeMrnaClusterList(struct ggMrnaCluster **pMcList)
/* Free a list of mrna clusters. */
{
struct ggMrnaCluster *mc, *next;

for(mc = *pMcList; mc != NULL; mc = next)
    {
    next = mc->next;
    freeMrnaCluster(&mc);
    }
*pMcList = NULL;
}
