/* xenbig.c - Routines to do big xeno alignments by breaking them into
 * pieces, calling the small aligner, and then stitching them back
 * together again. */
/* Copyright 2000-2003 Jim Kent.  All rights reserved. */

#include "common.h"
#include "dlist.h"
#include "hash.h"
#include "portable.h"
#include "dnautil.h"
#include "nt4.h"
#include "crudeali.h"
#include "wormdna.h"
#include "xenalign.h"


struct contig
/* A contiguously aligned sequence pair. */
    {
    char *name;
    char *queryFile;
    char *query;
    int qStart, qEnd;
    int qOffset, qEndOffset;
    char qStrand;
    char *target;
    int tStart, tEnd;
    int tOffset, tEndOffset;
    char tStrand;
    int score;
    char *qSym;     /* Query symbols (nucleotides and insert char) */
    char *tSym;     /* Target symbols (nucleotides and insert char) */
    char *hSym;     /* "Hidden" state symbols. */
    int symCount;
    boolean isComplete;
    };

static void freeContig(struct contig **pContig)
/* Free up a contig. */
{
struct contig *contig = *pContig;
if (contig != NULL)
    {
    freeMem(contig->name);
    freeMem(contig->target);
    freeMem(contig->qSym);
    freeMem(contig->tSym);
    freeMem(contig->hSym);
    freez(pContig);
    }
}

static void freeContigList(struct dlList **pContigList)
/* Free up list of contigs. */
{
struct dlNode *node;
struct contig *contig;

for (node = (*pContigList)->head; node->next != NULL; node = node->next)
    {
    contig = node->val;
    freeContig(&contig);
    }
freeDlList(pContigList);
}

static void checkComplete(struct dlList *contigList)
/* Make sure all the contigs on the list are complete. */
{
struct dlNode *node;
struct contig *contig;

for (node = contigList->head; node->next != NULL; node = node->next)
    {
    contig = node->val;
    if (!contig->isComplete)
        {
        errAbort("Contig %s:%d-%d %c %s:%d-%d %c isn't complete",
            contig->query, contig->qStart, contig->qEnd, contig->qStrand,
            contig->target, contig->tStart, contig->tEnd, contig->tStrand);
        }
    }
}

static int cmpContigQueryStart(const void *va, const void *vb)
/* Compare two contigs by start position and then by inverse
 * stop position. (This ensures that if one contig completely
 * encompasses another it will appear on list first.) */
{
struct contig **pA = (struct contig **)va;
struct contig **pB = (struct contig **)vb;
struct contig *a = *pA, *b = *pB;
int dif;

dif = a->qStart - b->qStart;
if (dif == 0)
   //dif = a->tStart - b->tStart;
   dif = b->qEnd - a->qEnd;
return dif;
}

static void writeContig(struct contig *contig, FILE *out, 
	boolean compactOutput, boolean newFormat)
/* Write out one contig to file */
{
int i;
int size = contig->symCount;
#define maxLineSize 50
char buf[maxLineSize+1];
int lineSize;
int j;
int qIx = 0, tIx = 0;
char *qSyms = contig->qSym;
char *tSyms = contig->tSym;

if (newFormat)
    {
    fprintf(out, "%s align %d.%d%% of %d %s %s:%d-%d %c %s:%d-%d %c\n",
	contig->name, contig->score/10, contig->score%10, size, 
	contig->queryFile, contig->query, contig->qStart, contig->qEnd, contig->qStrand,
	contig->target, contig->tStart, contig->tEnd, contig->tStrand);
    }
else
    {
    fprintf(out, "%s align %d.%d%% of %d %s:%d-%d %c %s:%d-%d %c\n",
	contig->name, contig->score/10, contig->score%10, size, 
	contig->query, contig->qStart, contig->qEnd, contig->qStrand,
	contig->target, contig->tStart, contig->tEnd, contig->tStrand);
    }
if (compactOutput)
    {
    mustWrite(out, contig->qSym, size);
    fputc('\n', out);
    mustWrite(out, contig->tSym, size);
    fputc('\n', out);
    mustWrite(out, contig->hSym, size);
    fputc('\n', out);
    }
else
    {
    for (i=0; i<size; i += lineSize)
        {
        /* Figure out how long this line is going to be. */
        lineSize = size - i;
        if (lineSize > maxLineSize)
            lineSize = maxLineSize;
        buf[lineSize] = 0;
    
        /* Print query symbols */
        memcpy(buf, contig->qSym + i, lineSize);
        fprintf(out, "%5d %s\n", qIx, buf);

        /* Print matching symbols */
        fprintf(out, "%5s ", "");
        for (j=0; j<lineSize; ++j)
            {
            int off = j+i;
            char q = qSyms[off];
            char t = tSyms[off];
            char c = (q == t ? '|' : ' ');
            fputc(c, out);
            }
        fputc('\n', out);

        /* Print target symbols */
        memcpy(buf, contig->tSym + i, lineSize);
        fprintf(out, "%5d %s\n", tIx, buf);

        /* Print hidden symbols */
        memcpy(buf, contig->hSym + i, lineSize);
        fprintf(out, "%5s %s\n", "", buf);

        /* Figure out how far we have advanced */
        for (j=0; j<lineSize; ++j)
            {
            int off = j+i;
            char q = qSyms[off];
            char t = tSyms[off];
            if (q != '-')
                ++qIx;
            if (t != '-')
                ++tIx;
            }
        /* Print blank line */
        fputc('\n', out);
        }
    }
#undef lineSize
}

struct cluster
/* A set of compatable contigs - one that might eventually be merged. */
    {
    struct dlList *contigs;
    char *query;
    int qStart, qEnd;
    int qOffset, qEndOffset;
    char qStrand;
    char *target;
    int tStart, tEnd;
    int tOffset, tEndOffset;
    char tStrand;
    int score;
    };

#ifdef UNUSED
static int cmpCluster(const void *va, const void *vb)
/* Compare two clusters. */
{
struct cluster **pA = (struct cluster **)va;
struct cluster **pB = (struct cluster **)vb;
struct cluster *a = *pA, *b = *pB;

return b->score - a->score;
}
#endif /* UNUSED */

static boolean contigsOverlap(struct contig *a, struct contig *b, 
    int *retQueryOverlap, int *retTargetOverlap)
/* Returns TRUE is contigs overlap. */
{
int overlap;
int start, end;

/* Check overlap in query. */
start = max(a->qStart, b->qStart);
end = min(a->qEnd, b->qEnd);
overlap = end - start;
if (overlap > 0)
    *retQueryOverlap = overlap;
else
    return FALSE;

/* Check overlap in target. */
start = max(a->tStart, b->tStart);
end = min(a->tEnd, b->tEnd);
overlap = end - start;
if (overlap > 0)
    *retTargetOverlap = overlap;
else
    return FALSE;
return TRUE;
}

static int findSymIx(char *qSym, int qIx, int *retSymIx)
/* Convert from query to symbol index, dealing with insert chars. */
{
char q;
int symIx;
int qCount = 0;

for (symIx = 0; ; ++symIx)
    {
    if (qCount == qIx)
        {
        *retSymIx = symIx;
        return TRUE;
        }
    if ((q = *qSym++) == 0)
        {
        warn("findSymIx gone past end");
        return FALSE;
        }
    if (q != '-')
        ++qCount;
    }
}

static boolean mergeIntoAdjacentClusters(struct dlList *clusterList, struct dlNode *contigNode)
/* If can merge contig into cluster list, do it and return TRUE.
 * else return FALSE. */
{
struct contig *contig = contigNode->val;  /* The contig we're trying to fold in. */
struct dlNode *clusterNode;               /* Node of a single cluster. */
struct cluster *cluster;                  /* A cluster. */

for (clusterNode = clusterList->head; clusterNode->next != NULL; clusterNode = clusterNode->next)
    {
    cluster = clusterNode->val;
    if ((contig->qOffset == cluster->qEndOffset - 1000 ||
        (contig->qEndOffset == cluster->qEndOffset &&
         contig->tEndOffset == cluster->tEndOffset)) &&
        contig->qStrand == cluster->qStrand &&
        contig->tStrand == cluster->tStrand &&
        sameString(contig->target, cluster->target) &&
        sameString(contig->query, cluster->query))
        {
        if (contig->qStrand == contig->tStrand)
            {
            if (contig->tOffset < cluster->tEndOffset + 50000
                && contig->tOffset >= cluster->tOffset-300)
                {
                cluster->qEndOffset = contig->qEndOffset;
                cluster->tEndOffset = contig->tEndOffset;
                cluster->score += contig->score;
                dlAddTail(cluster->contigs, contigNode);
                return TRUE;
                }
            }
        else
            {
            if ( contig->tEndOffset > cluster->tOffset - 50000
                && contig->tEndOffset <= cluster->tEndOffset+300)
                {
                cluster->qEndOffset = contig->qEndOffset;
                cluster->tOffset = contig->tOffset;
                cluster->score += contig->score;
                dlAddTail(cluster->contigs, contigNode);
                return TRUE;
                }
            }
        }
    }
return FALSE;
}

static void maxAlignedOverlap(char *a1, char *a2, char *b1, char *b2, int size,
    int *retStart, int *retOverlapSize)
/* Find maximum overlap between a and b without shifting a or
 * b. */
{
int maxStart = -1;
int maxLen = -1;
int curStart = 0;
int curLen = 0;
int i;

for (i=0; i<size; ++i)
    {
    if (a1[i] == b1[i] && a2[i] == b2[i])
        {
        curLen += 1;
        if (curLen > maxLen)
            {
            maxLen = curLen;
            maxStart = curStart;
            }
        }
    else
        {
        curLen = 0;
        curStart = i+1;
        }
    }
*retStart = maxStart;
*retOverlapSize = maxLen;
}
    
static void findMaxOverlap(char *a1, char *a2, int aSize, 
    char *b1, char *b2, int bSize,
    int *retAstart, int *retBstart, int *retOverlapSize)
/* Find maximum overlap between a and b and return it. */
{
int i;
int maxOverlap = -1;
int maxAstart = 0;
int maxBstart = 0;
int overlap;
int minSize = min(aSize, bSize);
int goodEnough = minSize-1;
int start;

for (i=0; i<bSize; ++i)
    {
    int size = bSize-i;
    if (size > aSize)
        size = aSize;
    maxAlignedOverlap(a1, a2, b1+i, b2+i, size, &start, &overlap);
    if (overlap > maxOverlap)
        {
        maxOverlap = overlap;
        maxAstart = start;
        maxBstart = i+start;
        if (maxOverlap >= goodEnough)
            goto RETURN_VALS;
        }
    }
for (i=1; i<aSize; ++i)
    {
    int size = aSize-i;
    if (size > bSize)
        size = bSize;
    maxAlignedOverlap(a1+i, a2+i, b1, b2, size, &start, &overlap);
    if (overlap > maxOverlap)
        {
        maxOverlap = overlap;
        maxAstart = i+start;
        maxBstart = start;
        if (maxOverlap >= goodEnough)
            goto RETURN_VALS;
        }
    }
RETURN_VALS:
    {
    *retAstart = maxAstart;
    *retBstart = maxBstart;
    *retOverlapSize = maxOverlap;
    }
}

static struct contig *forwardMergeTwo(struct contig *a, struct contig *b)
/* Make a new contig that's a merger of a and b. 
 * Assumes that a and b overlap and are on + strand. */
{
int aSymOverlapStart, aSymOverlapEnd, aSymOverlapSize;
int bSymOverlapStart, bSymOverlapEnd, bSymOverlapSize;
int aqoStart, bqoStart;
int overlapSize, halfOverlapSize;
int aCopyStart, aCopyEnd, bCopyStart, bCopyEnd;
int newSymCount;
struct contig *c;
int aCopySize, bCopySize;
int qOverlap;

/* Find overlapping area in symbol coordinates. */
qOverlap = b->qStart - a->qStart;
if (qOverlap < 0 || qOverlap > a->qEnd - a->qStart)
    return NULL;
if (!findSymIx(a->qSym, b->qStart - a->qStart, &aSymOverlapStart))
    return NULL;
aSymOverlapEnd = a->symCount;
aSymOverlapSize = aSymOverlapEnd - aSymOverlapStart;
bSymOverlapStart = 0;
if (!findSymIx(b->qSym, a->qEnd - b->qStart, &bSymOverlapEnd))
    return NULL;
bSymOverlapSize = bSymOverlapEnd - bSymOverlapStart;

/* Find subset of overlapping area that is identical
 * in source and query symbols. */
findMaxOverlap(a->qSym + aSymOverlapStart, 
    a->tSym + aSymOverlapStart, aSymOverlapSize, 
    b->qSym + bSymOverlapStart,
    b->tSym + bSymOverlapStart, bSymOverlapSize, 
    &aqoStart, &bqoStart, &overlapSize);
if (overlapSize < 15)
    return NULL;
aSymOverlapStart += aqoStart;
bSymOverlapStart += bqoStart;
halfOverlapSize = overlapSize/2;

/* Figure out what areas will go into new contig, and how big
 * it will be. */
aCopyStart = 0;
aCopyEnd = aSymOverlapStart + halfOverlapSize;
bCopyStart = bSymOverlapStart + halfOverlapSize;
bCopyEnd = b->symCount;
newSymCount = aCopyEnd - aCopyStart + bCopyEnd - bCopyStart;

/* Allocate new contig with set of symbols big enough to hold
 * merged contig. */
AllocVar(c);
c->query = a->query;
c->queryFile = a->queryFile;
c->qStart = min(a->qStart, b->qStart);
c->qEnd = max(a->qEnd, b->qEnd);
c->qStrand = a->qStrand;
c->target = cloneString(a->target);
c->tStart = a->tStart;
c->tEnd = b->tEnd;
c->tStrand = a->tStrand;
c->qSym = needMem(newSymCount);
c->tSym = needMem(newSymCount);
c->hSym = needMem(newSymCount);
c->symCount = newSymCount;

/* Copy symbols from contig a up to half way through
 * overlapping area */
aCopySize = aCopyEnd - aCopyStart;
bCopySize = bCopyEnd - bCopyStart;
memcpy(c->qSym, a->qSym+aCopyStart, aCopySize);
memcpy(c->tSym, a->tSym+aCopyStart, aCopySize);
memcpy(c->hSym, a->hSym+aCopyStart, aCopySize);

/* Append symbols from contig b from half way through
 * overlapping area to end. */
memcpy(c->qSym+aCopySize, b->qSym+bCopyStart, bCopySize);
memcpy(c->tSym+aCopySize, b->tSym+bCopyStart, bCopySize);
memcpy(c->hSym+aCopySize, b->hSym+bCopyStart, bCopySize);

return c;
}

static void rcContigOffsets(struct contig *a, int revOff)
/* Subtract each coordinate in contig from revOff */
{
int temp;

temp = a->qStart;
a->qStart = revOff - a->qEnd;
a->qEnd = revOff - temp;
temp = a->tStart;
a->tStart = revOff - a->tEnd;
a->tEnd = revOff - temp;
}

static void rcQueryOffsetsAroundSeg(struct contig *a, int segStart, int segEnd)
/* Reverse complement offsets relative to segStart and segEnd */
{
int segSize = segEnd - segStart;
int qEnd = reverseOffset(a->qStart - segStart, segSize) + segStart + 1;
int qStart = reverseOffset(a->qEnd - segStart, segSize) + segStart + 1;
a->qStart = qStart;
a->qEnd = qEnd;
}

static struct contig *mergeTwo(struct contig *a, struct contig *b)
/* Make a new contig that's a merger of a and b. 
 * Assumes that a and b overlap and are on either strand. */
{
int aStart, bEnd, revOff;
struct contig *c;
int tStart, tEnd;
int qStart, qEnd;

/* Take care of easy forward case first. */
if (a->qStrand == a->tStrand)
    return forwardMergeTwo(a,b);

/* Figure out a good position to do reverse complementing of
 * offsets about.  */
aStart = a->qStart;
bEnd = b->qEnd;
revOff = aStart + bEnd;

/* Save target beginning and end. */
tStart = b->tStart;
tEnd = a->tEnd;

/* Save target begin and end. */
qStart = a->qStart;
qEnd = b->qEnd;

/* Reverse complement offsets, do merge, and reverse complement
 * offsets back. */
rcContigOffsets(a, revOff);
rcContigOffsets(b, revOff);
c = forwardMergeTwo(b,a);
rcContigOffsets(a, revOff);
rcContigOffsets(b, revOff);
if (c != NULL)
    {
    rcContigOffsets(c, revOff);
    c->tStart = tStart;
    c->tEnd = tEnd;
    assert(c->qStart == qStart && c->qEnd == qEnd);
    }
return c;
}


static void mergeWithinCluster(struct cluster *cluster)
/* Merge all the contigs that you can merge within cluster. */
{
int clusterSize = dlCount(cluster->contigs);
struct dlNode *prevNode, *curNode, *nextNode;
struct contig *prev, *cur, *merged;
int tOverlap, qOverlap;

/* uglyf("M %d %s:%d-%d %c (%d-%d) %s:%d-%d %c (%d-%d)\n",
    clusterSize, 
    cluster->query, cluster->qStart, cluster->qEnd, cluster->qStrand,
    cluster->qOffset, cluster->qEndOffset,
    cluster->target, cluster->tStart, cluster->tEnd, cluster->tStrand,
    cluster->tOffset, cluster->tEndOffset);
 */

if (clusterSize < 2)
    return;     /* Wow was that easy! */
curNode = cluster->contigs->head;
cur = curNode->val;
nextNode = curNode->next;

for (;;)
    {
    /* Start of loop logic - store current position as previous, and
     * move to next, stop if at end of list. */
    prevNode = curNode;
    curNode = nextNode;
    if ((nextNode = curNode->next) == NULL)
        break;
    prev = cur;
    cur = curNode->val;    

    /* Figure out if current node overlaps with previous. */
    if (contigsOverlap(prev, cur, &qOverlap, &tOverlap))
        {
        if ((merged = mergeTwo(prev, cur)) == NULL)
            warn("Couldn't merge two");
        else
            {
            dlRemove(prevNode);
            freeMem(prevNode);
            freeContig(&cur);
            freeContig(&prev);
            curNode->val = cur = merged;
            }
        }
    }
}

static boolean clusterEncompassedBefore(struct dlList *clusterList, struct cluster *cluster)
/* Return TRUE if clusterList up to cluster has a member that encompasses
 * cluster. */
{
struct dlNode *node;
struct cluster *c;

for (node = clusterList->head; node->next != NULL; node = node->next)
    {
    c = node->val;
    if (c == cluster)
        return FALSE;
    if (c->qStart <= cluster->qStart && c->qEnd >= cluster->qEnd &&
        c->tStart <= cluster->tStart && c->tEnd >= cluster->tEnd &&
        c->qStrand == cluster->qStrand &&
        c->tStrand == cluster->tStrand &&
        sameString(c->query, cluster->query) &&
        sameString(c->target, cluster->target) )
        {
        return TRUE;
        }            
    }
assert(FALSE);
return FALSE;
}

static void clusterRemoveEncompassed(struct dlList *clusterList)
/* Remove elements from list that are completely encompassed by
 * another. */
{
struct dlNode *node, *next;
struct cluster *cluster;

for (node = clusterList->head; node->next != NULL; node = next)
    {
    next = node->next;
    cluster = node->val;
    if (clusterEncompassedBefore(clusterList, cluster))
        {
        dlRemove(node);
        freeMem(node);
        freeContigList(&cluster->contigs);
        freeMem(cluster);
        }
    }
}

static boolean contigEncompassedBefore(struct dlList *contigList, struct contig *contig)
/* Return TRUE if contigList up to contig has a member that encompasses
 * contig. */
{
struct dlNode *node;
struct contig *c;

for (node = contigList->head; node->next != NULL; node = node->next)
    {
    c = node->val;
    if (c == contig)
        return FALSE;
    if (c->qStart <= contig->qStart && c->qEnd >= contig->qEnd &&
        c->tStart <= contig->tStart && c->tEnd >= contig->tEnd &&
        c->qStrand == contig->qStrand &&
        c->tStrand == contig->tStrand &&
        sameString(c->query, contig->query) &&
        sameString(c->target, contig->target) )
        {
        return TRUE;
        }            
    }
assert(FALSE);
return FALSE;
}

static void contigRemoveEncompassed(struct dlList *contigList)
/* Remove elements from list that are completely encompassed by
 * another. */
{
struct dlNode *node, *next;
struct contig *contig;

for (node = contigList->head; node->next != NULL; node = next)
    {
    next = node->next;
    contig = node->val;
    if (contigEncompassedBefore(contigList, contig))
        {
        dlRemove(node);
        freeMem(node);
        freeContig(&contig);
        }
    }
}

static int milliMatchScore(struct contig *contig)
/* Return score that's fraction of symbols that match over 1000 */
{
int symCount = contig->symCount;
char *qSym = contig->qSym;
char *tSym = contig->tSym;
int matchCount = 0;
int i;

for (i=0; i<symCount; ++i)
    {
    if (qSym[i] == tSym[i])
        ++matchCount;
    }
return round( (1000.0 * matchCount) /  symCount);
} 

static void mergeContigs(struct dlList *contigList, int stitchMinScore)
/* Merge together contigs where possible. Assumes query sequence for
 * all in list is the same (though the query subsequence differs). */
{
struct dlList *clusterList;
struct dlNode *contigNode, *clusterNode;
struct dlNode *nextClusterNode;
struct cluster *cluster;
struct contig *contig;
int contigCount;
int startCount = 0;

/* Make sure there's really something to do, so we don't have
 * to NULL check so much in the rest of this routine. */
contigCount = dlCount(contigList);
if (contigCount < 1)
    return;

clusterList = newDlList();

/* Group together contigs that overlap in both query and target
   into clusters. */
while ((contigNode = dlPopHead(contigList)) != NULL)
    {
    if (!mergeIntoAdjacentClusters(clusterList, contigNode))
        {
        AllocVar(cluster);
        contig = contigNode->val;
        cluster->contigs = newDlList();
        dlAddTail(cluster->contigs, contigNode);
        cluster->query = contig->query;
        cluster->qStart = contig->qStart;
        cluster->qEnd = contig->qEnd;
        cluster->qOffset = contig->qOffset;
        cluster->qEndOffset = contig->qEndOffset;
        cluster->qStrand = contig->qStrand;
        cluster->target = contig->target;
        cluster->tStart = contig->tStart;
        cluster->tEnd = contig->tEnd;
        cluster->tOffset = contig->tOffset;
        cluster->tEndOffset = contig->tEndOffset;
        cluster->tStrand = contig->tStrand;
        cluster->score = contig->score;
        dlAddValTail(clusterList, cluster);
        }
    }

/* Weed out clusters that are too weak. */
for (clusterNode = clusterList->head; clusterNode->next != NULL; 
    clusterNode = nextClusterNode)
    {
    nextClusterNode = clusterNode->next;
    cluster = clusterNode->val;
    if (cluster->score < stitchMinScore)
        {
        dlRemove(clusterNode);
        freeContigList(&cluster->contigs);
        freeMem(cluster);
        freeMem(clusterNode);
        }
    }

clusterRemoveEncompassed(clusterList);

/* Merge contigs within cluster into a single contig, and move that contig
 * back onto cluster list. */
for (clusterNode = clusterList->head; clusterNode->next != NULL; clusterNode = clusterNode->next)
    {
    cluster = clusterNode->val;
    mergeWithinCluster(cluster);
    while ((contigNode = dlPopHead(cluster->contigs)) != NULL)
        dlAddTail(contigList, contigNode);
    freeDlList(&cluster->contigs);
    }

dlSort(contigList, cmpContigQueryStart);
contigRemoveEncompassed(contigList);

for (contigNode = contigList->head; contigNode->next != NULL; contigNode = contigNode->next)
    {
    char nameBuf[128];
    char *noDir;
    char *noDot;
    contig = contigNode->val;
    /* Get rid of directory and suffix parts of file name to use as
     * base for contig name. */
    noDir = strrchr(contig->query, '\\');
    if (noDir == NULL)
        noDir = strrchr(contig->query, '/');
    if (noDir == NULL)
        noDir = contig->query;
    else
        noDir += 1;
    noDot = strrchr(noDir, '.');
    if (noDot != NULL)
        *noDot = 0;
    sprintf(nameBuf, "%s.c%d", noDir, ++startCount);    
    contig->name = cloneString(nameBuf);
    contig->score = milliMatchScore(contig);
    }

/* Free up now gutted cluster list. */
freeDlListAndVals(&clusterList);
}

static void finishContigs( struct dlList **pContigList, 
    FILE *out, int stitchMinScore, boolean compactOutput, boolean newFormat)
/* Merge together contigs that go together, write the good
 * ones to output, and remove contigs from list.  */
{
struct contig *contig;
struct dlNode *node;

checkComplete(*pContigList);
mergeContigs(*pContigList, stitchMinScore);
for (node = (*pContigList)->head; node->next != NULL; node = node->next)
    {
    contig = node->val;
    writeContig(contig, out, compactOutput, newFormat);
    }
freeContigList(pContigList);
*pContigList = newDlList();
}

static int countNonGap(char *s, int size)
/* Count number of non-gap characters in string s of given size. */
{
int count = 0;
while (--size >= 0)
    if (*s++ != '-') ++count;
return count;
}

static void xStitch(char *inName, FILE *out, int stitchMinScore, boolean compactOutput, 
   boolean newFormat)
/* Do the big old stitching run putting together contents of inName
 * into contigs in out. */
{
FILE *in;
char line[512];
int lineCount = 0;
char *words[64];
int wordCount;
char *firstWord;
char *queryName = "";
char *queryFile = "";
struct hash *queryFileHash = newHash(0);
struct contig *contig = NULL;
struct dlList *contigList = newDlList();
int lineState = 0;   /* Keeps track of groups of four lines. */
struct slName *queryNameList = NULL;
int maxSymCount = 64*1024;
char *qSymBuf = needMem(maxSymCount+1);
char *tSymBuf = needMem(maxSymCount+1);
char *hSymBuf = needMem(maxSymCount+1);
int symCount = 0;
int qSymLen = 0, tSymLen = 0, hSymLen = 0;

in = mustOpen(inName, "r");
while (fgets(line, sizeof(line), in))
    {
    ++lineCount;
    if (++lineState == 5)
        lineState = 0;
    wordCount = chopLine(line, words);
    if (wordCount <= 0)
        continue;
    firstWord = words[0];
    if (sameString(firstWord, "Aligning"))
        {
        char *queryString;
        char *targetString;
        char queryStrand, targetStrand;
        char *parts[8];

        /* Do some preliminary checking of this line. */
	if (newFormat)
	    {
	    if (wordCount < 7)
		errAbort("Short line %d of %s", lineCount, inName);
	    queryFile = hashStoreName(queryFileHash, words[1]);
	    queryString = words[2];
	    queryStrand = words[3][0];
	    targetString = words[5];
	    targetStrand = words[6][0];
	    }
	else
	    {
	    if (wordCount < 6)
		errAbort("Short line %d of %s", lineCount, inName);
	    queryString = words[1];
	    queryStrand = words[2][0];
	    targetString = words[4];
	    targetStrand = words[5][0];
	    }

        /* Extract the name of the query sequence.  If it's new,
         * then write out contigs on previous query we've accumulated
         * so far and start a new list.  Ignore partCount return
         * from chopString. */
        chopString(queryString, ":-", parts, ArraySize(parts));
        if (!sameString(parts[0], queryName))
            {
            /* Allocate new name and keep track of it. */
            struct slName *newName = newSlName(parts[0]);
            slAddHead(&queryNameList, newName);

            /* Write out old contigs and empty out contig list. */
            finishContigs(&contigList, out, stitchMinScore, compactOutput, newFormat);
            queryName = newName->name;
            }
        
        /* Make up a new contig, and fill it in with the data we
         * have so far about query. */
        AllocVar(contig);
        dlAddValTail(contigList, contig);
	contig->queryFile = queryFile;
        contig->query = queryName;
        contig->qOffset = atoi(parts[1]);
        contig->qEndOffset = atoi(parts[2]);
        contig->qStrand = queryStrand;

        /* Parse target string and fill in contig with it's info. */
        chopString(targetString, ":-", parts, ArraySize(parts));
        contig->target = cloneString(parts[0]);
        contig->tOffset = atoi(parts[1]);
        contig->tEndOffset = atoi(parts[2]);
        contig->tStrand = targetStrand;

        /* We don't know start and end yet - set them to values
         * that will get easily replace by max/min. */
        contig->qStart = contig->tStart = 0x3fffffff;

        lineState = -1;
        symCount = 0;
        }
    else if (sameString(firstWord, "best"))
        {
        if (wordCount < 3)
            errAbort("Short line %d of %s", lineCount, inName);
        contig->score = atoi(words[2]);
        contig->isComplete = TRUE;
        contig->qSym = cloneStringZ(qSymBuf, symCount);
        contig->tSym = cloneStringZ(tSymBuf, symCount);
        contig->hSym = cloneStringZ(hSymBuf, symCount);
        contig->symCount = symCount;
        contig->qEnd = contig->qStart + countNonGap(qSymBuf, symCount);
        contig->tEnd = contig->tStart + countNonGap(tSymBuf, symCount);
        if (contig->qStrand != contig->tStrand) 
            rcQueryOffsetsAroundSeg(contig, contig->qOffset, contig->qEndOffset);
        }
    else if (wordCount > 1 && (isdigit(firstWord[0]) || firstWord[0] == '-'))
        {
        int start, end;
        char *sym = words[1];
        int symLen = strlen(sym);
        char firstChar = firstWord[0];
        if (lineState != 0 && lineState != 2)
            errAbort("Bummer - phasing mismatch on lineState line %d of %s!\n", lineCount, inName);
        assert(lineState == 0 || lineState == 2);
        start = atoi(firstWord);
        end = start + symLen;
        if (symCount + symLen > maxSymCount)
            {
            errAbort("Single contig too long line %d of %s, can only handle up to %d symbols\n",
                lineCount, inName, maxSymCount);
            }
        if (lineState == 0) /* query symbols */
            {
            qSymLen = symLen;
            if (isdigit(firstChar))
                {
                start += contig->qOffset;
                end += contig->qOffset;
                contig->qStart = min(contig->qStart, start);
                contig->qEnd = max(contig->qEnd, end);
                }
            memcpy(qSymBuf+symCount, sym, symLen);
            }
        else               /* target symbols */
            {
            tSymLen = symLen;
            if (tSymLen != qSymLen)
                {
                errAbort("Target symbol size not same as query line %d of %s",
                    lineCount, inName);
                }            
            if (isdigit(firstChar))
                {
                start += contig->tOffset;
                end += contig->tOffset;
                contig->tStart = min(contig->tStart, start);
                }
            memcpy(tSymBuf+symCount, sym, symLen);
            }
        }
    else if (firstWord[0] == '(')
        {
        lineState = -1;
        }
    else
        {
        assert(lineState == 1 || lineState == 3);
        if (lineState == 3)  /* Hidden symbols. */
            {
            char *sym = firstWord;
            int symLen = strlen(sym);
            hSymLen = symLen;
            if (hSymLen != qSymLen)
                {
                errAbort("Hidden symbol size not same as query line %d of %s",
                    lineCount, inName);
                }
            memcpy(hSymBuf+symCount, sym, symLen);
            symCount += symLen;
            }        
        }
    } 
finishContigs(&contigList, out, stitchMinScore, compactOutput, newFormat);
fclose(in);

slFreeList(&queryNameList);
freeHash(&queryFileHash);
freeMem(qSymBuf);
freeMem(tSymBuf);
freeMem(hSymBuf);
}

void xenStitch(char *inName, FILE *out, int stitchMinScore, boolean compactOutput)
/* Do the big old stitching run putting together contents of inName
 * into contigs in out.  Create output in newer format. */
{
xStitch(inName, out, stitchMinScore, compactOutput, TRUE);
}

void xenStitcher(char *inName, FILE *out, int stitchMinScore, boolean compactOutput)
/* Do the big old stitching run putting together contents of inName
 * into contigs in out.  Create output in older format. */
{
xStitch(inName, out, stitchMinScore, compactOutput, FALSE);
}


void xenAlignBig(DNA *query, int qSize, DNA *target, int tSize, FILE *f, boolean forHtml)
/* Do a big alignment - one that has to be stitched together. */
{
struct crudeAli *caList, *ca;
struct nt4Seq *nt4 = newNt4(target, tSize, "target");
struct tempName dynTn;
int i;
int lqSize;
int qMaxSize = 2000;
int tMaxSize = 2*qMaxSize;
int lastQsection = qSize - qMaxSize/2;
FILE *dynFile;
int tMin, tMax, tMid;
int score;
int totalAli = 0;
double totalSize;

totalSize = (double)qSize * tSize;
if (totalSize < 10000000.0)
    {
    xenAlignSmall(query, qSize, target, tSize, f, forHtml);
    return;
    }
makeTempName(&dynTn, "xen", ".dyn");
dynFile = mustOpen(dynTn.forCgi, "w");

for (i = 0; i<lastQsection || i == 0; i += qMaxSize/2)
    {
    lqSize = qSize - i;
    if (lqSize > qMaxSize)
        lqSize = qMaxSize;
    caList = crudeAliFind(query+i, lqSize, &nt4, 1, 8, 12);
    if (forHtml)
        {
        fputc('.', stdout);
        fflush(stdout);
        }
    for (ca = caList; ca != NULL; ca = ca->next)
        {
        tMid = (ca->start + ca->end)/2;
        tMin = tMid-tMaxSize/2;
        tMax = tMin + tMaxSize;
        if (tMin < 0)
            tMin = 0;
        if (tMax > tSize)
            tMax = tSize;
        
        fprintf(dynFile, "Aligning query:%d-%d %c to target:%d-%d +\n",
            i, i+lqSize, ca->strand, tMin, tMax);
        if (ca->strand == '-')
            {
            reverseComplement(query+i, lqSize);
            }
        score = xenAlignSmall(query+i, lqSize, target+tMin, tMax-tMin, dynFile, FALSE);        
        if (ca->strand == '-')
            reverseComplement(query+i, lqSize);
        fprintf(dynFile, "best score %d\n", score);
        if (forHtml)
            {
            fputc('.', stdout);
            fflush(stdout);
            }
        ++totalAli;
        }
    slFreeList(&caList);
    }

freeNt4(&nt4);
fclose(dynFile);
if (forHtml)
    {
    printf("\n %d alignments to stitch.\n", totalAli);
    }
xenStitcher(dynTn.forCgi, f, 150000, FALSE);
remove(dynTn.forCgi);
}


void xenAlignWorm(DNA *query, int qSize, FILE *f, boolean forHtml)
/* Do alignment against worm genome. */
 {
struct crudeAli *caList = NULL, *ca, *newCa;
struct tempName dynTn;
int i;
int lqSize;
int qMaxSize = 2000;
int tMaxSize = 2*qMaxSize;
int lastQsection = qSize - qMaxSize/2;
FILE *dynFile;
int tMin, tMax, tMid, tSize;
int score;
int chromCount;
char **chromNames;
struct nt4Seq **chrom;
DNA *target;
int sectionCount = 0;

/* Get C. elegans genome and do crude alignments. */
wormLoadNt4Genome(&chrom, &chromCount);
wormChromNames(&chromNames, &chromCount);
for (i = 0; i<lastQsection || i == 0; i += qMaxSize/2)
    {
    if (forHtml)
        {
        fputc('.', stdout);
        fflush(stdout);
        }
    lqSize = qSize - i;
    if (lqSize > qMaxSize)
        lqSize = qMaxSize;
    newCa = crudeAliFind(query+i, lqSize, chrom, chromCount, 8, 45);
    for (ca = newCa; ca != NULL; ca = ca->next)
        {
        ca->qStart = i;
        ca->qEnd = i+lqSize;
        }
    caList = slCat(caList,newCa);
    ++sectionCount;
    }
wormFreeNt4Genome(&chrom);

/* Make temporary output file. */
makeTempName(&dynTn, "xen", ".dyn");
dynFile = mustOpen(dynTn.forCgi, "w");

/* Do dynamic programming alignment on each crude alignment. */
if (forHtml)
    {
    int crudeCount = slCount(caList);
    if (crudeCount > 2*sectionCount)
        {
        printf("\nThis is a difficult alignment.  It looks like the query aligns with\n"
               "the <I>C. elegans</I> genome in %d places.  It will take about %1.0f more\n"
               "minutes to finish.\n", (crudeCount+sectionCount/2)/sectionCount, crudeCount*0.5);
        }
    fflush(stdout);
    }
for (ca = caList; ca != NULL; ca = ca->next)
    {
    if (forHtml)
        {
        fputc('.', stdout);
        fflush(stdout);
        }
    tMid = (ca->start + ca->end)/2;
    tMin = tMid-tMaxSize/2;
    tMax = tMin + tMaxSize;
    wormClipRangeToChrom(chromNames[ca->chromIx], &tMin, &tMax);
    fprintf(dynFile, "Aligning query:%d-%d %c to %s:%d-%d +\n",
        ca->qStart, ca->qEnd, ca->strand, chromNames[ca->chromIx], tMin, tMax);
    lqSize = ca->qEnd - ca->qStart;
    if (ca->strand == '-')
        reverseComplement(query+ca->qStart, lqSize);
    tSize = tMax - tMin;
    target = wormChromPart(chromNames[ca->chromIx], tMin, tSize);
    score = xenAlignSmall(query+ca->qStart, lqSize, target, tSize, dynFile, FALSE);        
    freez(&target);
    if (ca->strand == '-')
        reverseComplement(query+ca->qStart, lqSize);
    fprintf(dynFile, "best score %d\n", score);
    }
slFreeList(&caList);
fclose(dynFile);

if (forHtml)
    printf("\n");
xenStitcher(dynTn.forCgi, f, 150000, FALSE);
remove(dynTn.forCgi);
}

