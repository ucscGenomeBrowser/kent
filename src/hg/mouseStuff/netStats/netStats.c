/* netStats - Gather statistics on net. */

/* Copyright (C) 2012 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "chainNet.h"
#include "localmem.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "netStats - Gather statistics on net\n"
  "usage:\n"
  "   netStats summary.out inNet(s)\n"
  "options:\n"
  "   -gap=gapFile\n"
  "   -fill=fillFile\n"
  "   -top=topFile\n"
  "   -nonSyn=topFile\n"
  "   -syn=synFile\n"
  "   -inv=invFile\n"
  "   -dupe=dupeFile\n"
  );
}

FILE *logFile;

#if defined(__GNUC__)
void logIt(char *format, ...)
__attribute__((format(printf, 1, 2)));
#endif

void logIt(char *format, ...)
/* Record something to log. */
{
va_list args;
va_start(args, format);
vfprintf(logFile, format, args);
va_end(args);
}

FILE *optionalFile(char *optionName)
/* Return open file or NULL depending if optional filename
 * exists. */
{
char *name = optionVal(optionName, NULL);
if (name == NULL)
    return NULL;
FILE *f = mustOpen(name, "w");
fprintf(f, "#tSize\tqSize\tali\tqDup\tqFar\n");
return f;
}


void (*rtApply)(struct cnFill *fill, int level, FILE *optFile);
/* Function rTraversal below applies. */
FILE *rtOptFile;

void rTraversal(struct cnFill *fillList, int level)
/* Recursively traverse net. */
{
struct cnFill *fill;
for (fill = fillList; fill != NULL; fill = fill->next)
    {
    rtApply(fill, level, rtOptFile);
    if (fill->children)
        rTraversal(fill->children, level+1);
    }
}

void traverseNet(struct chainNet *netList, 
	FILE *optFile,
	void (*apply)(struct cnFill *fill, int level, FILE *optFile))
/* Traverse all nets and apply function. */
{
struct chainNet *net;
rtApply = apply;
rtOptFile = optFile;
for (net = netList; net != NULL; net = net->next)
    rTraversal(net->fillList, 0);
}


int depthMax;

void depthGather(struct cnFill *fill, int level, FILE *f)
/* Gather depth info. */
{
if (depthMax < level)
     depthMax = level;
}


long long gapSizeT, gapSizeQ;
int gapCount;

void gapGather(struct cnFill *fill, int level, FILE *f)
/* Gather stats on gaps */
{
if (fill->chainId == 0)
    {
    gapCount += 1;
    gapSizeT += fill->tSize;
    gapSizeQ += fill->qSize;
    if (f)
	fprintf(f, "%d\t%d\n", fill->tSize, fill->qSize);
    }
}

struct lm *intLm;	/* Local memory for intLists . */

struct intList
/* A list of ints. */
    {
    struct intList *next;	/* Next in list. */
    int val;			/* Value. */
    };

int intListCmp(const void *va, const void *vb)
/* Compare to sort based on value. */
{
const struct intList *a = *((struct intList **)va);
const struct intList *b = *((struct intList **)vb);
return a->val - b->val;
}

struct fillStats
/* Information on fills. */
    {
    int count;
    long long totalAli;			/* Total alignments. */
    struct intList *spanT, *spanQ;	/* Coverage with gaps. */
    struct intList *ali;		/* Coverage no gaps. */
    struct intList *qFar;		/* Total farness. */
    struct intList *qDup;		/* Total dupness. */
    };

struct fillStats fillStats;   /* Stats on all fills. */
struct fillStats topStats;    /* Stats on top fills. */
struct fillStats nonSynStats; /* Stats on nonSyn fills. */
struct fillStats invStats;    /* Stats on inv fills. */
struct fillStats synStats;    /* Stats on fills in same chrom same dir. */
struct fillStats dupeStats;   /* Stats on dupes. */

void addInt(struct intList **pList, int val)
/* Add value to int list. */
{
struct intList *el;
lmAllocVar(intLm, el);
el->val = val;
slAddHead(pList, el);
}

void addFillStats(struct fillStats *stats, struct cnFill *fill, FILE *f)
/* Add info from one fill to stats. */
{
stats->count += 1;

addInt(&stats->spanT, fill->tSize);
addInt(&stats->spanQ, fill->qSize);
addInt(&stats->ali, fill->ali);
stats->totalAli += fill->ali;
if (fill->qDup >= 0)
    addInt(&stats->qDup, fill->qDup);
if (fill->qFar >= 0)
    addInt(&stats->qFar, fill->qFar);
if (f)
    fprintf(f, "%d\t%d\t%d\t%d\t%d\n", fill->tSize, fill->qSize, fill->ali,
    	fill->qDup, fill->qFar);
}

void fillGather(struct cnFill *fill, int level, FILE *f)
/* Gather stats on fill */
{
if (fill->chainId != 0)
    {
    addFillStats(&fillStats, fill, f);
    }
}

void topGather(struct cnFill *fill, int level, FILE *f)
/* Gather stats on top level fill */
{
if (fill->type != NULL && sameString(fill->type, "top"))
    {
    addFillStats(&topStats, fill, f);
    }
}

void nonSynGather(struct cnFill *fill, int level, FILE *f)
/* Gather stats on non-syntenic fill. */
{
if (fill->type != NULL && sameString(fill->type, "nonSyn"))
    {
    addFillStats(&nonSynStats, fill, f);
    }
}

void invGather(struct cnFill *fill, int level, FILE *f)
/* Gather stats on inv level fill */
{
if (fill->type != NULL && sameString(fill->type, "inv"))
    {
    addFillStats(&invStats, fill, f);
    }
}

void dupeGather(struct cnFill *fill, int level, FILE *f)
/* Gather stats on things mostly duplicated. */
{
if (fill->qDup > 0 && fill->qDup * 2 >= fill->ali)
    addFillStats(&dupeStats, fill, f);
}

void synGather(struct cnFill *fill, int level, FILE *f)
/* Gather stats on things mostly duplicated. */
{
if (fill->type != NULL && sameString(fill->type, "syn"))
    {
    addFillStats(&synStats, fill, f);
    }
}


void logListStats(struct intList **pList)
/* Write out some stats to log file. */
{
struct intList *el;
long long total = 0;
int minVal = 0, medVal0 = 0, medVal1 = 0, maxVal = 0;
int count = slCount(*pList);
int i, middle = count/2;
float medVal;


if (count != 0)
    {
    slSort(pList, intListCmp);
    minVal = (*pList)->val;
    for (i=0, el = *pList; el != NULL; el = el->next, ++i)
	{
	int val = el->val;
	total += val;
	maxVal = val;
	if (i == middle)
	    medVal0 = val;
	else if (i == middle+1)
	    medVal1 = val;
	}
    if (((count % 2) == 0) && (count > 1))
        medVal = (medVal0+medVal1)/2.0;
    else
        medVal = medVal0;
    logIt("ave: %0.1f  min: %d  max: %d  median: %0.1f  total: %lld", 
          total/(double)count, minVal, maxVal, medVal, total);
    }
logIt("\n");
}

void logFillStats(char *name, struct fillStats *stats)
/* Write out info on stats */
{
logIt("%s count: %d\n", name, stats->count);
logIt("%s aligned: %lld\n", name, stats->totalAli);
logIt("%s percent of total: %3.1f%%\n", name, 
	100.0*stats->totalAli/fillStats.totalAli);
logIt("%s span-T: ", name);
logListStats(&stats->spanT);
logIt("%s span-Q: ", name);
logListStats(&stats->spanQ);
logIt("%s aligning: ", name);
logListStats(&stats->ali);
if (stats->qDup != NULL)
    {
    logIt("%s ave-qDup: ", name);
    logListStats(&stats->qDup);
    }
if (stats->qFar != NULL)
    {
    logIt("%s ave-qFar: ", name);
    logListStats(&stats->qFar);
    }
}


void netStats(char *summaryFile, int inCount, char *inFiles[])
/* netStats - Gather statistics on net. */
{
int i;
int netCount = 0;
FILE *gapFile = optionalFile("gap");
FILE *fillFile = optionalFile("fill");
FILE *topFile = optionalFile("top");
FILE *nonSynFile = optionalFile("nonSyn");
FILE *invFile = optionalFile("inv");
FILE *synFile = optionalFile("syn");
FILE *dupeFile = optionalFile("dupe");

intLm = lmInit(0);
logFile = mustOpen(summaryFile, "w");
logIt("net files: %d\n", inCount);
for (i=0; i<inCount; ++i)
    {
    struct lineFile *lf = lineFileOpen(inFiles[i], TRUE);
    struct chainNet *net;
    while ((net = chainNetRead(lf)) != NULL)
	{
	++netCount;
	traverseNet(net, NULL, depthGather);
	traverseNet(net, gapFile, gapGather);
	traverseNet(net, fillFile, fillGather);
	traverseNet(net, topFile, topGather);
	traverseNet(net, nonSynFile, nonSynGather);
	traverseNet(net, synFile, synGather);
	traverseNet(net, invFile, invGather);
	traverseNet(net, dupeFile, dupeGather);
	chainNetFree(&net);
	}
    lineFileClose(&lf);
    }

logIt("net chromosomes: %d\n", netCount);
logIt("max depth: %d\n", depthMax);
logIt("gap count: %d\n",  gapCount);
logIt("gap average size T: %4.1f\n", gapSizeT/(double)gapCount);
logIt("gap average size Q: %4.1f\n", gapSizeQ/(double)gapCount);
logFillStats("fill", &fillStats);
logFillStats("top", &topStats);
logFillStats("nonSyn", &nonSynStats);
logFillStats("syn", &synStats);
logFillStats("inv", &invStats);
logFillStats("dupe", &dupeStats);
}

void testList()
{
struct intList *list = NULL;
intLm = lmInit(0);
addInt(&list, 1);
addInt(&list, 2);
addInt(&list, 3);
addInt(&list, 4);
addInt(&list, 5);
addInt(&list, 6);
addInt(&list, 7);
addInt(&list, 8);
addInt(&list, 9);
addInt(&list, 10);
addInt(&list, 11);
logListStats(&list);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc < 3)
    usage();
netStats(argv[1], argc-2, argv+2);
return 0;
}
