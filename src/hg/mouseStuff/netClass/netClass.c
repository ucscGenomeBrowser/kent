/* netClass - Add classification info to net. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "rbTree.h"
#include "jksql.h"
#include "hdb.h"
#include "localmem.h"
#include "agpGap.h"

char *tNewR = NULL;
char *qNewR = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "netClass - Add classification info to net\n"
  "usage:\n"
  "   netClass in.net tDb qDb out.net\n"
  "options:\n"
  "   -tNewR=dir - Dir of chrN.out.spec files, with RepeatMasker .out format\n"
  "                lines describing lineage specific repeats in target\n"
  "   -qNewR=dir - Dir of chrN.out.spec files for query\n"
  );
}

struct chainNet
/* A net on one chromosome. */
    {
    struct chainNet *next;
    char *name;			/* Chromosome name. */
    int size;			/* Chromosome size. */
    struct fill *fillList;	/* Top level fills. */
    struct hash *nameHash; 	/* Hash of all fill->qName names. */
    };

struct fill
/* Filling sequence or a gap. */
    {
    /* Required fields */
    struct fill *next;	   /* Next in list. */
    int tStart, tSize;	   /* Range in target chromosome. */
    char *qName;	   /* Other chromosome (not allocated here) */
    char qStrand;	   /* Orientation + or - in other chrom. */
    int qStart,	qSize;	   /* Range in query chromosome. */
    struct fill *children; /* List of child gaps. */
    /* Optional fields. */
    int chainId;	   /* Chain id.  0 for a gap. */
    int tN;	   /* Count of N's in target chromosome or -1 */
    int qN;	   /* Count of N's in query chromosome or -1 */
    int tR;	   /* Count of repeats in target chromosome or -1 */
    int qR;	   /* Count of repeats in query chromosome or -1 */
    int tNewR;	   /* Count of new (lineage specific) repeats in target */
    int qNewR;	   /* Count of new (lineage specific) repeats in query */
    };

struct fill *fillNew()
/* Return fill structure with some basic stuff filled in */
{
struct fill *fill;
AllocVar(fill);
fill->tN = fill->qN = fill->tR = fill->qR = fill->tNewR = fill->qNewR = -1;
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

void chromNetFree(struct chainNet **pNet)
/* Free up a chromosome net. */
{
struct chainNet *net = *pNet;
if (net != NULL)
    {
    freeMem(net->name);
    fillFreeList(&net->fillList);
    hashFree(&net->nameHash);
    freez(pNet);
    }
}

void chromNetFreeList(struct chainNet **pList)
/* Free up a list of chainNet. */
{
struct chainNet *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    chromNetFree(&el);
    }
*pList = NULL;
}


struct fill *rNetRead(struct chainNet *net, struct lineFile *lf)
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
	fill->qName = hashStoreName(net->nameHash, words[3]);
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
	    else if (sameString(name, "tNewR"))
	        fill->tNewR = val;
	    else if (sameString(name, "qNewR"))
	        fill->qNewR = val;
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
    	fill->qName, fill->qStrand, fill->qStart, fill->qSize);
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
    if (fill->tNewR >= 0)
        fprintf(f, " tNewR %d", fill->tNewR);
    if (fill->qNewR >= 0)
        fprintf(f, " qNewR %d", fill->qNewR);
    fputc('\n', f);
    if (fill->children)
        rChromNetWrite(fill->children, f, depth+1);
    }
}

void chromNetWrite(struct chainNet *net, FILE *f)
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

struct chainNet *chromNetRead(struct lineFile *lf)
/* Read next net from file. Return NULL at end of file.*/
{
char *line, *words[3];
struct chainNet *net;
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
return net;
}

struct chrom
/* Basic information on a chromosome. */
    {
    struct chrom *next;	  /* Next in list */
    char *name;		  /* Chromosome name, allocated in hash. */
    int size;		  /* Chromosome size. */
    struct rbTree *nGaps; /* Gaps in sequence (Ns) */
    struct rbTree *repeats; /* Repeats in sequence */
    struct rbTree *newRepeats; /* New (lineage specific) repeats. */
    };

struct range
/* A part of a chromosome. */
    {
    int start, end;	/* Half open zero based coordinates. */
    };

int rangeCmp(void *va, void *vb)
/* Return -1 if a before b,  0 if a and b overlap,
 * and 1 if a after b. */
{
struct range *a = va;
struct range *b = vb;
if (a->end <= b->start)
    return -1;
else if (b->end <= a->start)
    return 1;
else
    return 0;
}

static int interSize;	/* Size of intersection. */
static struct range interRange; /* Range to intersect with. */

void addInterSize(void *item)
/* Add range to interSize. */
{
struct range *r = item;
int size;
size = rangeIntersection(r->start, r->end, interRange.start, interRange.end);
interSize += size;
}

int intersectionSize(struct rbTree *tree, int start, int end)
/* Return total size of things intersecting range start-end. */
{
interRange.start = start;
interRange.end = end;
interSize = 0;
rbTreeTraverseRange(tree, &interRange, &interRange, addInterSize);
return interSize;
}

struct rbTree *getSeqGaps(char *db, char *chrom)
/* Return a tree of ranges for sequence gaps in chromosome */
{
struct sqlConnection *conn = sqlConnect(db);
struct rbTree *tree = rbTreeNew(rangeCmp);
int rowOffset;
struct sqlResult *sr = hChromQuery(conn, "gap", chrom, NULL, &rowOffset);
char **row;

while ((row = sqlNextRow(sr)) != NULL)
    {
    struct agpGap gap;
    struct range *range;
    agpGapStaticLoad(row+rowOffset, &gap);
    lmAllocVar(tree->lm, range);
    range->start = gap.chromStart;
    range->end = gap.chromEnd;
    rbTreeAdd(tree, range);
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
return tree;
}

struct rbTree *getRepeats(char *db, char *chrom)
/* Return a tree of ranges for sequence gaps in chromosome */
{
struct sqlConnection *conn = sqlConnect(db);
struct sqlResult *sr;
char **row;
struct rbTree *tree = rbTreeNew(rangeCmp);
char tableName[64];
char query[256];
boolean hasBin;

if (!hFindSplitTable(chrom, "rmsk", tableName, &hasBin))
    errAbort("Can't find rmsk table for %s\n", chrom);
sprintf(query, "select genoStart,genoEnd from %s", tableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct range *range;
    lmAllocVar(tree->lm, range);
    range->start = sqlUnsigned(row[0]);
    range->end = sqlUnsigned(row[1]);
    rbTreeAdd(tree, range);
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
return tree;
}

struct rbTree *getNewRepeats(char *dirName, char *chrom)
/* Read in repeatMasker .out line format file into a tree of ranges. */
{
struct rbTree *tree = rbTreeNew(rangeCmp);
struct range *range;
char fileName[512];
struct lineFile *lf;
char *row[7];

sprintf(fileName, "%s/%s.out.spec", dirName, chrom);
lf = lineFileOpen(fileName, TRUE);
while (lineFileRow(lf, row))
    {
    if (!sameString(chrom, row[4]))
        errAbort("Expecting %s word 5, line %d of %s\n", 
		chrom, lf->lineIx, lf->fileName);
    lmAllocVar(tree->lm, range);
    range->start = lineFileNeedNum(lf, row, 5) - 1;
    range->end = lineFileNeedNum(lf, row, 6);
    rbTreeAdd(tree, range);
    }
lineFileClose(&lf);
return tree;
}

void getChroms(char *db, struct hash **retHash, struct chrom **retList)
/* Get hash of chromosomes from database. */
{
struct sqlConnection *conn = sqlConnect(db);
struct sqlResult *sr;
char **row;
struct chrom *chromList = NULL, *chrom;
struct hash *hash = hashNew(8);

sr = sqlGetResult(conn, "select chrom,size from chromInfo");
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(chrom);
    hashAddSaveName(hash, row[0], chrom, &chrom->name);
    chrom->size = atoi(row[1]);
    slAddHead(&chromList, chrom);
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
slReverse(&chromList);
*retHash = hash;
*retList = chromList;
}

void addTn(struct chainNet *net, struct fill *fillList, struct rbTree *tree)
/* Add tN's to all gaps underneath fillList. */
{
struct fill *fill;
for (fill = fillList; fill != NULL; fill = fill->next)
    {
    int s = fill->tStart;
    fill->tN = intersectionSize(tree, s, s + fill->tSize);
    if (fill->children)
	addTn(net, fill->children, tree);
    }
}

void addQn(struct chainNet *net, struct fill *fillList, struct hash *qChromHash)
/* Add qN's to all gaps underneath fillList. */
{
struct fill *fill;
for (fill = fillList; fill != NULL; fill = fill->next)
    {
    struct chrom *qChrom = hashMustFindVal(qChromHash, fill->qName);
    struct rbTree *tree = qChrom->nGaps;
    int s = fill->qStart;
    fill->qN = intersectionSize(tree, s, s + fill->qSize);
    if (fill->children)
	addQn(net, fill->children, qChromHash);
    }
}

void addTr(struct chainNet *net, struct fill *fillList, struct rbTree *tree)
/* Add t repeats's to all things underneath fillList. */
{
struct fill *fill;
for (fill = fillList; fill != NULL; fill = fill->next)
    {
    int s = fill->tStart;
    fill->tR = intersectionSize(tree, s, s + fill->tSize);
    if (fill->children)
	addTr(net, fill->children, tree);
    }
}

void addQr(struct chainNet *net, struct fill *fillList, struct hash *qChromHash)
/* Add q repeats to all things underneath fillList. */
{
struct fill *fill;
for (fill = fillList; fill != NULL; fill = fill->next)
    {
    struct chrom *qChrom = hashMustFindVal(qChromHash, fill->qName);
    int s = fill->qStart;
    fill->qR = intersectionSize(qChrom->repeats, s, s + fill->qSize);
    if (fill->children)
	addQr(net, fill->children, qChromHash);
    }
}

void tAddNewR(struct chainNet *net, struct fill *fillList, struct rbTree *tree)
/* Add t new repeats's to all things underneath fillList. */
{
struct fill *fill;
for (fill = fillList; fill != NULL; fill = fill->next)
    {
    int s = fill->tStart;
    fill->tNewR = intersectionSize(tree, s, s + fill->tSize);
    if (fill->children)
	tAddNewR(net, fill->children, tree);
    }
}

void qAddNewR(struct chainNet *net, struct fill *fillList, struct hash *qChromHash)
/* Add q new repeats to all things underneath fillList. */
{
struct fill *fill;
for (fill = fillList; fill != NULL; fill = fill->next)
    {
    struct chrom *qChrom = hashMustFindVal(qChromHash, fill->qName);
    int s = fill->qStart;
    fill->qNewR = intersectionSize(qChrom->newRepeats, s, s + fill->qSize);
    if (fill->children)
	qAddNewR(net, fill->children, qChromHash);
    }
}



void netClass(char *inName, char *tDb, char *qDb, char *outName)
/* netClass - Add classification info to net. */
{
struct chainNet *net;
struct lineFile *lf = lineFileOpen(inName, TRUE);
FILE *f = mustOpen(outName, "w");
struct chrom *qChromList, *chrom;
struct hash *qChromHash;

getChroms(qDb, &qChromHash, &qChromList);

printf("Reading gaps in %s\n", qDb);
for (chrom = qChromList; chrom != NULL; chrom = chrom->next)
    chrom->nGaps = getSeqGaps(qDb, chrom->name);
printf("Reading repeats in %s\n", qDb);
for (chrom = qChromList; chrom != NULL; chrom = chrom->next)
    chrom->repeats = getRepeats(qDb, chrom->name);
if (qNewR)
    {
    printf("Reading new repeats from %s\n", qNewR);
    for (chrom = qChromList; chrom != NULL; chrom = chrom->next)
        chrom->newRepeats = getNewRepeats(qNewR, chrom->name);
    }


while ((net = chromNetRead(lf)) != NULL)
    {
    struct rbTree *tnTree, *trTree;
    printf("Processing %s.%s\n", tDb, net->name);
    tnTree = getSeqGaps(tDb, net->name);
    addTn(net, net->fillList, tnTree);
    rbTreeFree(&tnTree);
    addQn(net, net->fillList, qChromHash);

    trTree = getRepeats(tDb, net->name);
    addTr(net, net->fillList, trTree);
    rbTreeFree(&trTree);
    addQr(net, net->fillList, qChromHash);

    if (tNewR)
        {
	struct rbTree *tree = getNewRepeats(tNewR, net->name);
	tAddNewR(net, net->fillList, tree);
	rbTreeFree(&tree);
	}
    if (qNewR)
        qAddNewR(net, net->fillList, qChromHash);
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
tNewR = optionVal("tNewR", tNewR);
qNewR = optionVal("qNewR", qNewR);
netClass(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
