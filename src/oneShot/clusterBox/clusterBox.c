/* clusterBox - Cluster together overlapping boxes,  or technically,
 * overlapping axt's. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "dlist.h"
#include "portable.h"
#include "options.h"
#include "localmem.h"
#include "dlist.h"
#include "fuzzyFind.h"
#include "axt.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "clusterBox - Experiments with new ways of stitching together alignments\n"
  "usage:\n"
  "   clusterBox input.axt output\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

struct seqPair
/* Pair of sequences. */
    {
    struct seqPair *next;
    char *name;	/* Allocated in hash */
    struct axt *axtList; /* List of alignments. */
    };

struct aBoxCluster
/* A cluster of boxes. */
    {
    struct aBox *boxList;	/* List of boxes. */
    struct dlNode *node;	/* Position in doubly-linked list. */
    };

struct aBox
/* A box in an alignment.  Input to cluster2d. */
    {
    struct aBox *next;		 /* Next in list (usually on cluster). */
    int qStart, qEnd;		 /* Range covered in query. */
    int tStart, tEnd;		 /* Range covered in target. */
    struct aBoxCluster *cluster; /* Cluster this is part of. */
    void *data;			 /* Some associated data. */
    };

struct aBoxRef
/* Reference to a box. */
    {
    struct aBoxRef *next;
    struct aBox *box;
    };

struct aBoxJoiner
/* A structure that contains all you need to
 * cluster boxes. */
    {
    struct dlList *clusterList;  /* List of all clusters. */
    struct lm *lm;               /* Memory for aBoxRef's. */
    };

static void mergeClusters(struct aBoxCluster *a, struct aBoxCluster *b)
/* Merge cluster b into cluster a, and free remnants of b. */
{
struct aBox *box;

/* Merging with yourself is always problematic. */
if (a == b)
    return;

/* Point all boxes in b to a. */
for (box = b->boxList; box != NULL; box = box->next)
    box->cluster = a;

/* Add b's boxList to end of a's. */
a->boxList = slCat(a->boxList, b->boxList);

/* Remove b from master cluster list. */
dlRemove(b->node);

/* Free up remnants of b. */
freeMem(b->node);
freeMem(b);
}

static boolean allStartBy(struct aBoxRef *refList, int qStart, int tStart)
/* Return TRUE if qStart/qEnd of all boxes less than or equal to qStart/tStart 
 * This is an end condition for recursion along with only having two or
 * less boxes in the refList.  It handles the case where you have many
 * boxes stacked on top of each other. */
{
struct aBoxRef *ref;
for (ref = refList->next; ref != NULL; ref = ref->next)
    {
    struct aBox *box = ref->box;
    if (box->qStart > qStart || box->tStart > tStart)
        return FALSE;
    }
return TRUE;
}

static boolean allSameCluster(struct aBoxRef *refList)
/* Return TRUE if qStart/qEnd of all boxes is the same */
{
struct aBoxRef *ref;
struct aBoxCluster *cluster = refList->box->cluster;
for (ref = refList->next; ref != NULL; ref = ref->next)
    {
    if (ref->box->cluster != cluster)
        return FALSE;
    }
return TRUE;
}

static int rCalls = 0;
static int maxDepth = 0;

static struct aBoxJoiner *joiner;
/* Input data shared by all invocations of rBoxJoin.
 * Must be set before call to rBoxJoin. */

static void rBoxJoin(int depth, struct aBoxRef *refList, 
	int qStart, int qEnd, int tStart, int tEnd)
/* Recursively cluster boxes. */
{
int boxCount = slCount(refList);

++rCalls;

    {
    int i;
    if (depth > maxDepth)
        maxDepth = depth;
#ifdef DEBUG
    for (i=0; i<depth; ++i)
       uglyf("  ");
    uglyf("rBoxJoin(%d,%d %d,%d) boxCount %d, ",
        qStart, qEnd, tStart, tEnd, boxCount);
#endif /* DEBUG */
    }

if (boxCount <= 1)
    {
    /* Easy: no merging required. */
    // uglyf("easy\n"); fflush(uglyOut);
    }
else if (boxCount == 2)
    {
    /* Decide if pair overlaps and if so merge. */
    struct aBox *a = refList->box;
    struct aBox *b = refList->next->box;
    if (rangeIntersection(a->qStart, a->qEnd, b->qStart, b->qEnd) > 0 &&
        rangeIntersection(a->tStart, a->tEnd, b->tStart, b->tEnd) > 0 )
	{
	// uglyf("merge2\n"); fflush(uglyOut);
	mergeClusters(a->cluster, b->cluster);
	}
    else
        {
	// uglyf("distinct2\n"); fflush(uglyOut);
	}
    }
else if (allStartBy(refList, qStart, tStart))
    {
    /* If everybody contains the upper left corner, then they all need merging. */
    struct aBoxCluster *aCluster = refList->box->cluster;
    struct aBoxRef *ref;
    for (ref = refList->next; ref != NULL; ref = ref->next)
        {
	struct aBoxCluster *bCluster = ref->box->cluster;
	mergeClusters(aCluster, bCluster);
	// uglyf("multiMerge\n"); fflush(uglyOut);
	}
    }
else if (allSameCluster(refList))
    {
    // uglyf("allOne\n"); fflush(uglyOut);
    }
else
    {
    struct aBoxRef *list1 = NULL, *list2 = NULL, *ref, *next;

    /* Break up box along larger dimension. */
    if (qEnd - qStart > tEnd - tStart)
        {
	int mid = (qStart + qEnd)>>1;
	// uglyf("qSplit\n"); fflush(uglyOut);
	for (ref = refList; ref != NULL; ref = next)
	    {
	    struct aBox *box = ref->box;
	    next = ref->next;
	    if (box->qEnd <= mid)
	        {
		slAddHead(&list1, ref);
		}
	    else if (box->qStart >= mid)
	        {
		slAddHead(&list2, ref);
		}
	    else
	        {
		/* Box crosses boundary, have to put it on both lists. */
		slAddHead(&list1, ref);
		lmAllocVar(joiner->lm, ref);
		ref->box = box;
		slAddHead(&list2, ref);
		}
	    }
	rBoxJoin(depth+1, list1, qStart, mid, tStart, tEnd);
	rBoxJoin(depth+1, list2, mid, qEnd, tStart, tEnd);
	}
    else
        {
	int mid = (tStart + tEnd)>>1;
	// uglyf("tSplit\n"); fflush(uglyOut);
	for (ref = refList; ref != NULL; ref = next)
	    {
	    struct aBox *box = ref->box;
	    next = ref->next;
	    if (box->tEnd <= mid)
	        {
		slAddHead(&list1, ref);
		}
	    else if (box->tStart >= mid)
	        {
		slAddHead(&list2, ref);
		}
	    else
	        {
		/* Box crosses boundary, have to put it on both lists. */
		slAddHead(&list1, ref);
		lmAllocVar(joiner->lm, ref);
		ref->box = box;
		slAddHead(&list2, ref);
		}
	    }
	rBoxJoin(depth+1, list1, qStart, qEnd, tStart, mid);
	rBoxJoin(depth+1, list2, qStart, qEnd, mid, tEnd);
	}
    }
}

void printClusters(struct dlList *clusterList, FILE *f)
/* Print out all clusters. */
{
struct dlNode *node;
for (node = clusterList->head; !dlEnd(node); node = node->next)
    {
    struct aBoxCluster *cluster = node->val;
    struct aBox *box;
    fprintf(f, "Cluster of %d\n", slCount(cluster->boxList));
    for (box = cluster->boxList; box != NULL; box = box->next)
        fprintf(f, "  %d,%d %d,%d\n", 
		box->qStart, box->qEnd, box->tStart, box->tEnd);
    }
}


void clusterPair(struct seqPair *sp, FILE *f)
/* Make chains for all alignments in sp. */
{
struct aBoxJoiner *join;
struct aBoxRef *refList = NULL, *ref;
struct aBox *box;
struct aBoxCluster *cluster;
struct axt *axt;
int qStart = BIGNUM, qEnd = -BIGNUM;
int tStart = BIGNUM, tEnd = -BIGNUM;

/* Make initial list containing all axt's. */
fprintf(f, "Pairing %s - %d axt's initially\n", sp->name, slCount(sp->axtList));

AllocVar(join);
join->lm = lmInit(0);
join->clusterList = newDlList();

/* Make a box for each axt, a cluster for each box, and
 * a reference for each box.  Calculate overall bounds. */
for (axt = sp->axtList; axt != NULL; axt = axt->next)
    {
    int x;
    AllocVar(box);
    box->qStart = x = axt->qStart;
    if (x < qStart) qStart = x;
    box->qEnd = x = axt->qEnd;
    if (x > qEnd) qEnd = x;
    box->tStart = x = axt->tStart;
    if (x < tStart) tStart = x;
    box->tEnd = x = axt->tEnd;
    if (x > tEnd) tEnd = x;
    AllocVar(cluster);
    cluster->node = dlAddValTail(join->clusterList, cluster);
    cluster->boxList = box;
    box->cluster = cluster;
    lmAllocVar(join->lm, ref);
    ref->box = box;
    slAddHead(&refList, ref);
    }

/* Call to recursive joiner. */
joiner = join;
rBoxJoin(0, refList, qStart, qEnd, tStart, tEnd);

/* Print out cluster list. */
printClusters(join->clusterList, f);
fprintf(f, "\n");
}


void clusterBox(char *axtFile, char *output)
/* clusterBox - Experiments with new ways of stitching together alignments. */
{
struct hash *pairHash = newHash(0);  /* Hash keyed by qSeq<strand>tSeq */
struct dyString *dy = newDyString(512);
struct lineFile *lf = lineFileOpen(axtFile, TRUE);
struct axt *axt;
struct seqPair *spList = NULL, *sp;
FILE *f = mustOpen(output, "w");
int axtCount = 0;
long t1,t2,t3;

/* Read input file and divide alignments into various parts. */
t1 = clock1000();
while ((axt = axtRead(lf)) != NULL)
    {
    if (axt->score < 500)
        {
	axtFree(&axt);
	continue;
	}
    dyStringClear(dy);
    dyStringPrintf(dy, "%s%c%s", axt->qName, axt->qStrand, axt->tName);
    sp = hashFindVal(pairHash, dy->string);
    if (sp == NULL)
        {
	AllocVar(sp);
	slAddHead(&spList, sp);
	hashAddSaveName(pairHash, dy->string, sp, &sp->name);
	}
    slAddHead(&sp->axtList, axt);
    ++axtCount;
    }
uglyf("%d axt's\n", axtCount);
t2 = clock1000();
for (sp = spList; sp != NULL; sp = sp->next)
    {
    slReverse(&sp->axtList);
    clusterPair(sp, f);
    }
uglyf("maxDepth = %d, rCalls = %d\n", maxDepth, rCalls);
t3 = clock1000();
dyStringFree(&dy);
uglyf("axtRead %fs, clusterPair %fs\n", 0.001 * (t2-t1), 0.001 * (t3-t2) );
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3)
    usage();
clusterBox(argv[1], argv[2]);
return 0;
}
