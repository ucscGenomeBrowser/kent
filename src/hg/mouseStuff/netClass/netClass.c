/* netClass - Add classification info to net. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "netClass - Add classification info to net\n"
  "usage:\n"
  "   netClass in.net tDb qDb out.net\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

struct chromNet
/* A net on one chromosome. */
    {
    struct chromNet *next;
    char *name;			/* Chromosome name. */
    int size;			/* Chromosome size. */
    struct fill *fillList;	/* Top level fills. */
    struct hash *nameHash; 	/* Hash of all fill->qChrom names. */
    };

struct fill
/* Filling sequence or a gap. */
    {
    /* Required fields */
    struct fill *next;	   /* Next in list. */
    int tStart, tSize;	   /* Range in target chromosome. */
    char *qChrom;	   /* Other chromosome (not allocated here) */
    char qStrand;	   /* Orientation + or - in other chrom. */
    int qStart,	qSize;	   /* Range in query chromosome. */
    struct fill *children; /* List of child gaps. */
    /* Optional fields. */
    int chainId;	   /* Chain id.  0 for a gap. */
    int tN;	   /* Count of N's in target chromosome or -1 */
    int qN;	   /* Count of N's in query chromosome or -1 */
    int tR;	   /* Count of repeats in target chromosome or -1 */
    int qR;	   /* Count of repeats in query chromosome or -1 */
    };

struct fill *fillNew()
/* Return fill structure with some basic stuff filled in */
{
struct fill *fill;
AllocVar(fill);
fill->tN = fill->qN = fill->tR = fill->qR = -1;
return fill;
}

void fillFreeList(struct fill **pList);

void fillFree(struct fill **pFill)
/* Free up a fill structure and all of it's children. */
{
struct fill *fill = *pFill;
if (fill != NULL)
    {
    if (fill->children != NULL)
        fillFreeList(&fill->children);
    freez(pFill);
    }
}

void fillFreeList(struct fill **pList)
/* Free up a list of fills. */
{
struct fill *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    fillFree(&el);
    }
*pList = NULL;
}

void chromNetFree(struct chromNet **pNet)
/* Free up a chromosome net. */
{
struct chromNet *net = *pNet;
if (net != NULL)
    {
    freeMem(net->name);
    fillFreeList(&net->fillList);
    hashFree(&net->nameHash);
    freez(pNet);
    }
}

void chromNetFreeList(struct chromNet **pList)
/* Free up a list of chromNet. */
{
struct chromNet *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    chromNetFree(&el);
    }
*pList = NULL;
}


struct fill *rNetRead(struct chromNet *net, struct lineFile *lf)
/* Recursively read in file. */
{
static char *words[64];
int i, wordCount;
char *line;
int d, depth = 0;
struct fill *fillList = NULL;
struct fill *fill = NULL;
enum {basicFields = 7};

for (;;)
    {
    if (!lineFileNextReal(lf, &line))
	break;
    d = countLeadingChars(line, ' ');
    if (fill == NULL)
        depth = d;
    if (d < depth)
	{
	lineFileReuse(lf);
	break;
	}
    if (d > depth)
        {
	lineFileReuse(lf);
	fill->children = rNetRead(net, lf);
	}
    else
        {
	wordCount = chopLine(line, words);
	lineFileExpectAtLeast(lf, basicFields, wordCount);
	fill = fillNew();
	slAddHead(&fillList, fill);
	fill->tStart = lineFileNeedNum(lf, words, 1);
	fill->tSize = lineFileNeedNum(lf, words, 2);
	fill->qChrom = hashStoreName(net->nameHash, words[3]);
	fill->qStrand = words[4][0];
	fill->qStart = lineFileNeedNum(lf, words, 5);
	fill->qSize = lineFileNeedNum(lf, words, 6);
	for (i=basicFields; i<wordCount; i += 2)
	    {
	    char *name = words[i];
	    int val = lineFileNeedNum(lf, words, i+1);
	    if (sameString(name, "id"))
	        fill->chainId = val;
	    else if (sameString(name, "tN"))
	        fill->tN = val;
	    else if (sameString(name, "qN"))
	        fill->qN = val;
	    else if (sameString(name, "tR"))
	        fill->tR = val;
	    else if (sameString(name, "qR"))
	        fill->qR = val;
	    }
	}
    }
slReverse(&fillList);
return fillList;
}

static void rChromNetWrite(struct fill *fillList, FILE *f, int depth)
/* Recursively write out fill list. */
{
struct fill *fill;
for (fill = fillList; fill != NULL; fill = fill->next)
    {
    char *type = (fill->chainId ? "fill" : "gap");
    spaceOut(f, depth);
    fprintf(f, "%s %d %d %s %c %d %d", type, fill->tStart, fill->tSize,
    	fill->qChrom, fill->qStrand, fill->qStart, fill->qSize);
    if (fill->chainId)
        fprintf(f, " id %d", fill->chainId);
    if (fill->tN >= 0)
        fprintf(f, " tN %d", fill->tN);
    if (fill->qN >= 0)
        fprintf(f, " qN %d", fill->qN);
    if (fill->tR >= 0)
        fprintf(f, " tR %d", fill->tR);
    if (fill->qR >= 0)
        fprintf(f, " qR %d", fill->qR);
    fputc('\n', f);
    if (fill->children)
        rChromNetWrite(fill->children, f, depth+1);
    }
}

void chromNetWrite(struct chromNet *net, FILE *f)
/* Write out chain net. */
{
fprintf(f, "net %s %d\n", net->name, net->size);
rChromNetWrite(net->fillList, f, 1);
}

int fillForestSize(struct fill *fillList)
/* Count elements in fill forest (list of fills) */
{
struct fill *fill;
int count = 0;
for (fill = fillList; fill != NULL; fill = fill->next)
    {
    ++count;
    if (fill->children != NULL)
        count += fillForestSize(fill->children);
    }
return count;
}

struct chromNet *chromNetRead(struct lineFile *lf)
/* Read next net from file. Return NULL at end of file.*/
{
char *line, *words[3];
struct chromNet *net;
int wordCount;

if (!lineFileNextReal(lf, &line))
   return NULL;
if (!startsWith("net ", line))
   errAbort("Expecting 'net' first word of line %d of %s", 
   	lf->lineIx, lf->fileName);
AllocVar(net);
wordCount = chopLine(line, words);
lineFileExpectAtLeast(lf, 3, wordCount);
net->name = cloneString(words[1]);
net->size = lineFileNeedNum(lf, words, 2);
net->nameHash = hashNew(6);
net->fillList = rNetRead(net, lf);
uglyf("Read %d top level elements, %d total in %s\n", 
	slCount(net->fillList), fillForestSize(net->fillList), net->name);
return net;
}

void netClass(char *inName, char *tDb, char *qDb, char *outName)
/* netClass - Add classification info to net. */
{
struct chromNet *net;
struct lineFile *lf = lineFileOpen(inName, TRUE);
FILE *f = mustOpen(outName, "w");
while ((net = chromNetRead(lf)) != NULL)
    {
    chromNetWrite(net, f);
    chromNetFree(&net);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 5)
    usage();
netClass(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
