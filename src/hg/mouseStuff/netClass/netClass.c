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
#include "simpleRepeat.h"

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
    struct cnFill *fillList;	/* Top level fills. */
    struct hash *nameHash; 	/* Hash of all fill->qName names. */
    };

struct cnFill
/* Filling sequence or a gap. */
    {
	/* Required fields */
    struct cnFill *next;	   /* Next in list. */
    int tStart, tSize;	   /* Range in target chromosome. */
    char *qName;	   /* Other chromosome (not allocated here) */
    char qStrand;	   /* Orientation + or - in other chrom. */
    int qStart,	qSize;	   /* Range in query chromosome. */
    struct cnFill *children; /* List of child gaps. */
	/* Optional fields. */
    int chainId;	   /* Chain id.  0 for a gap. */
    double score;  /* Score of associated chain. */
    int tN;	   /* Count of N's in target chromosome or -1 */
    int qN;	   /* Count of N's in query chromosome or -1 */
    int tR;	   /* Count of repeats in target chromosome or -1 */
    int qR;	   /* Count of repeats in query chromosome or -1 */
    int tNewR;	   /* Count of new (lineage specific) repeats in target */
    int qNewR;	   /* Count of new (lineage specific) repeats in query */
    int tOldR;	   /* Count of ancient repeats (pre-split) in target */
    int qOldR;	   /* Count of ancient repeats (pre-split) in query */
    int qTrf;	   /* Count of simple repeats, period 12 or less. */
    int tTrf;	   /* Count of simple repeats, period 12 or less. */
    };

struct cnFill *cnFillNew()
/* Return fill structure with some basic stuff filled in */
{
struct cnFill *fill;
AllocVar(fill);
fill->tN = fill->qN = fill->tR = fill->qR = 
	fill->tNewR = fill->qNewR = fill->tOldR = fill->tNewR = 
	fill->qTrf = fill->tTrf = -1;
return fill;
}

void cnFillFreeList(struct cnFill **pList);

void cnFillFree(struct cnFill **pFill)
/* Free up a fill structure and all of it's children. */
{
struct cnFill *fill = *pFill;
if (fill != NULL)
    {
    if (fill->children != NULL)
        cnFillFreeList(&fill->children);
    freez(pFill);
    }
}

void cnFillFreeList(struct cnFill **pList)
/* Free up a list of fills. */
{
struct cnFill *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    cnFillFree(&el);
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
    cnFillFreeList(&net->fillList);
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


struct cnFill *cnFillRead(struct chainNet *net, struct lineFile *lf)
/* Recursively read in file. */
{
static char *words[64];
int i, wordCount;
char *line;
int d, depth = 0;
struct cnFill *fillList = NULL;
struct cnFill *fill = NULL;
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
	fill->children = cnFillRead(net, lf);
	}
    else
        {
	wordCount = chopLine(line, words);
	lineFileExpectAtLeast(lf, basicFields, wordCount);
	fill = cnFillNew();
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

	    if (sameString(name, "score"))
	        fill->score = atof(words[i+1]);
	    else
		{
		/* Cope with integer values. */
		int iVal = lineFileNeedNum(lf, words, i+1);
		if (sameString(name, "id"))
		    fill->chainId = iVal;
		else if (sameString(name, "tN"))
		    fill->tN = iVal;
		else if (sameString(name, "qN"))
		    fill->qN = iVal;
		else if (sameString(name, "tR"))
		    fill->tR = iVal;
		else if (sameString(name, "qR"))
		    fill->qR = iVal;
		else if (sameString(name, "tNewR"))
		    fill->tNewR = iVal;
		else if (sameString(name, "qNewR"))
		    fill->qNewR = iVal;
		else if (sameString(name, "tOldR"))
		    fill->tOldR = iVal;
		else if (sameString(name, "qOldR"))
		    fill->qOldR = iVal;
		else if (sameString(name, "tTrf"))
		    fill->tTrf = iVal;
		else if (sameString(name, "qTrf"))
		    fill->qTrf = iVal;
		}
	    }
	}
    }
slReverse(&fillList);
return fillList;
}

void cnFillWrite(struct cnFill *fillList, FILE *f, int depth)
/* Recursively write out fill list. */
{
struct cnFill *fill;
for (fill = fillList; fill != NULL; fill = fill->next)
    {
    char *type = (fill->chainId ? "fill" : "gap");
    spaceOut(f, depth);
    fprintf(f, "%s %d %d %s %c %d %d", type, fill->tStart, fill->tSize,
    	fill->qName, fill->qStrand, fill->qStart, fill->qSize);
    if (fill->chainId)
        fprintf(f, " id %d", fill->chainId);
    if (fill->score > 0)
        fprintf(f, " score %1.0f", fill->score);
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
    if (fill->tOldR >= 0)
        fprintf(f, " tOldR %d", fill->tOldR);
    if (fill->qOldR >= 0)
        fprintf(f, " qOldR %d", fill->qOldR);
    if (fill->tTrf >= 0)
        fprintf(f, " tTrf %d", fill->tTrf);
    if (fill->qTrf >= 0)
        fprintf(f, " qTrf %d", fill->qTrf);
    fputc('\n', f);
    if (fill->children)
        cnFillWrite(fill->children, f, depth+1);
    }
}

void chromNetWrite(struct chainNet *net, FILE *f)
/* Write out chain net. */
{
fprintf(f, "net %s %d\n", net->name, net->size);
cnFillWrite(net->fillList, f, 1);
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
net->fillList = cnFillRead(net, lf);
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
    struct rbTree *oldRepeats; /* Old (pre-split) repeats. */
    struct rbTree *trf;	       /* Simple repeats. */
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

struct rbTree *getTrf(char *db, char *chrom)
/* Return a tree of ranges for simple repeats in chromosome. */
{
struct rbTree *tree = rbTreeNew(rangeCmp);
struct range *range;
struct sqlConnection *conn = sqlConnect(db);
char query[256];
struct sqlResult *sr;
char **row;

sprintf(query, "select chromStart,chromEnd from simpleRepeat "
               "where chrom = '%s'",
	       chrom);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    lmAllocVar(tree->lm, range);
    range->start = sqlUnsigned(row[0]);
    range->end = sqlUnsigned(row[1]);
    rbTreeAdd(tree, range);
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
return tree;
}

void getRepeats(char *db, struct hash *arHash, char *chrom,
	struct rbTree **retAllRepeats,  struct rbTree **retNewRepeats)
/* Return a tree of ranges for sequence gaps in chromosome */
{
struct sqlConnection *conn = sqlConnect(db);
struct sqlResult *sr;
char **row;
struct rbTree *allTree = rbTreeNew(rangeCmp);
struct rbTree *newTree = rbTreeNew(rangeCmp);
char tableName[64];
char query[256];
boolean hasBin;

if (!hFindSplitTable(chrom, "rmsk", tableName, &hasBin))
    errAbort("Can't find rmsk table for %s\n", chrom);
sprintf(query, "select genoStart,genoEnd,repName,repClass,repFamily from %s", tableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct range *range;
    char arKey[512];
    lmAllocVar(allTree->lm, range);
    range->start = sqlUnsigned(row[0]);
    range->end = sqlUnsigned(row[1]);
    rbTreeAdd(allTree, range);
    sprintf(arKey, "%s.%s.%s", row[2], row[3], row[4]);
    if (hashLookup(arHash, arKey))
        {
	lmAllocVar(newTree->lm, range);
	range->start = sqlUnsigned(row[0]);
	range->end = sqlUnsigned(row[1]);
	rbTreeAdd(newTree, range);
	}
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
*retAllRepeats = allTree;
*retNewRepeats = newTree;
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

boolean tableExists(char *db, char *table)
/* Return TRUE if table exists in database. */
{
struct sqlConnection *conn = sqlConnect(db);
boolean exists = sqlTableExists(conn, table);
sqlDisconnect(&conn);
return exists;
}

struct hash *getAncientRepeats(char *tDb, char *qDb)
/* Get hash of ancient repeats.  This keyed by name.family.class. */
{
char *db = NULL;
struct sqlConnection *conn;
struct sqlResult *sr;
char **row;
char key[512];
struct hash *hash = newHash(10);

if (tableExists(tDb, "ancientRepeat"))
    db = tDb;
else if (tableExists(qDb, "ancientRepeat"))
    db = qDb;
else
    errAbort("Can't find ancientRepeat table in %s or %s", tDb, qDb);
conn = sqlConnect(db);
sr = sqlGetResult(conn, "select name,family,class from ancientRepeat");
while ((row = sqlNextRow(sr)) != NULL)
    {
    sprintf(key, "%s.%s.%s", row[0], row[1], row[2]);
    hashAdd(hash, key, NULL);
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
return hash;
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

void tAddN(struct chainNet *net, struct cnFill *fillList, struct rbTree *tree)
/* Add tN's to all gaps underneath fillList. */
{
struct cnFill *fill;
for (fill = fillList; fill != NULL; fill = fill->next)
    {
    int s = fill->tStart;
    fill->tN = intersectionSize(tree, s, s + fill->tSize);
    if (fill->children)
	tAddN(net, fill->children, tree);
    }
}

void qAddN(struct chainNet *net, struct cnFill *fillList, struct hash *qChromHash)
/* Add qN's to all gaps underneath fillList. */
{
struct cnFill *fill;
for (fill = fillList; fill != NULL; fill = fill->next)
    {
    struct chrom *qChrom = hashMustFindVal(qChromHash, fill->qName);
    struct rbTree *tree = qChrom->nGaps;
    int s = fill->qStart;
    fill->qN = intersectionSize(tree, s, s + fill->qSize);
    if (fill->children)
	qAddN(net, fill->children, qChromHash);
    }
}

void tAddR(struct chainNet *net, struct cnFill *fillList, struct rbTree *tree)
/* Add t repeats's to all things underneath fillList. */
{
struct cnFill *fill;
for (fill = fillList; fill != NULL; fill = fill->next)
    {
    int s = fill->tStart;
    fill->tR = intersectionSize(tree, s, s + fill->tSize);
    if (fill->children)
	tAddR(net, fill->children, tree);
    }
}

void qAddR(struct chainNet *net, struct cnFill *fillList, struct hash *qChromHash)
/* Add q repeats to all things underneath fillList. */
{
struct cnFill *fill;
for (fill = fillList; fill != NULL; fill = fill->next)
    {
    struct chrom *qChrom = hashMustFindVal(qChromHash, fill->qName);
    int s = fill->qStart;
    fill->qR = intersectionSize(qChrom->repeats, s, s + fill->qSize);
    if (fill->children)
	qAddR(net, fill->children, qChromHash);
    }
}

void tAddNewR(struct chainNet *net, struct cnFill *fillList, struct rbTree *tree)
/* Add t new repeats's to all things underneath fillList. */
{
struct cnFill *fill;
for (fill = fillList; fill != NULL; fill = fill->next)
    {
    int s = fill->tStart;
    fill->tNewR = intersectionSize(tree, s, s + fill->tSize);
    if (fill->children)
	tAddNewR(net, fill->children, tree);
    }
}

void qAddNewR(struct chainNet *net, struct cnFill *fillList, struct hash *qChromHash)
/* Add q new repeats to all things underneath fillList. */
{
struct cnFill *fill;
for (fill = fillList; fill != NULL; fill = fill->next)
    {
    struct chrom *qChrom = hashMustFindVal(qChromHash, fill->qName);
    int s = fill->qStart;
    fill->qNewR = intersectionSize(qChrom->newRepeats, s, s + fill->qSize);
    if (fill->children)
	qAddNewR(net, fill->children, qChromHash);
    }
}

void tAddOldR(struct chainNet *net, struct cnFill *fillList, struct rbTree *tree)
/* Add t new repeats's to all things underneath fillList. */
{
struct cnFill *fill;
for (fill = fillList; fill != NULL; fill = fill->next)
    {
    int s = fill->tStart;
    fill->tOldR = intersectionSize(tree, s, s + fill->tSize);
    if (fill->children)
	tAddOldR(net, fill->children, tree);
    }
}

void qAddOldR(struct chainNet *net, struct cnFill *fillList, struct hash *qChromHash)
/* Add q new repeats to all things underneath fillList. */
{
struct cnFill *fill;
for (fill = fillList; fill != NULL; fill = fill->next)
    {
    struct chrom *qChrom = hashMustFindVal(qChromHash, fill->qName);
    int s = fill->qStart;
    fill->qOldR = intersectionSize(qChrom->oldRepeats, s, s + fill->qSize);
    if (fill->children)
	qAddOldR(net, fill->children, qChromHash);
    }
}

void tAddTrf(struct chainNet *net, struct cnFill *fillList, struct rbTree *tree)
/* Add t simple repeats's to all things underneath fillList. */
{
struct cnFill *fill;
for (fill = fillList; fill != NULL; fill = fill->next)
    {
    int s = fill->tStart;
    fill->tTrf = intersectionSize(tree, s, s + fill->tSize);
    if (fill->children)
	tAddTrf(net, fill->children, tree);
    }
}

void qAddTrf(struct chainNet *net, struct cnFill *fillList, struct hash *qChromHash)
/* Add q new repeats to all things underneath fillList. */
{
struct cnFill *fill;
for (fill = fillList; fill != NULL; fill = fill->next)
    {
    struct chrom *qChrom = hashMustFindVal(qChromHash, fill->qName);
    int s = fill->qStart;
    fill->qTrf = intersectionSize(qChrom->trf, s, s + fill->qSize);
    if (fill->children)
	qAddTrf(net, fill->children, qChromHash);
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
struct hash *arHash = getAncientRepeats(tDb, qDb);

getChroms(qDb, &qChromHash, &qChromList);

printf("Reading gaps in %s\n", qDb);
for (chrom = qChromList; chrom != NULL; chrom = chrom->next)
    chrom->nGaps = getSeqGaps(qDb, chrom->name);

printf("Reading simpleRepeats in %s\n", qDb);
for (chrom = qChromList; chrom != NULL; chrom = chrom->next)
    chrom->trf = getTrf(qDb, chrom->name);

printf("Reading repeats in %s\n", qDb);
for (chrom = qChromList; chrom != NULL; chrom = chrom->next)
    getRepeats(qDb, arHash, chrom->name, &chrom->repeats, &chrom->oldRepeats);

if (qNewR)
    {
    printf("Reading new repeats from %s\n", qNewR);
    for (chrom = qChromList; chrom != NULL; chrom = chrom->next)
        chrom->newRepeats = getNewRepeats(qNewR, chrom->name);
    }


while ((net = chromNetRead(lf)) != NULL)
    {
    struct rbTree *tN, *tRepeats, *tOldRepeats, *tTrf;

    printf("Processing %s.%s\n", tDb, net->name);
    tN = getSeqGaps(tDb, net->name);
    tAddN(net, net->fillList, tN);
    rbTreeFree(&tN);
    qAddN(net, net->fillList, qChromHash);

    getRepeats(tDb, arHash, net->name, &tRepeats, &tOldRepeats);
    tAddR(net, net->fillList, tRepeats);
    tAddOldR(net, net->fillList, tOldRepeats);
    rbTreeFree(&tRepeats);
    rbTreeFree(&tOldRepeats);
    qAddR(net, net->fillList, qChromHash);
    qAddOldR(net, net->fillList, qChromHash);

    tTrf = getTrf(tDb, net->name);
    tAddTrf(net, net->fillList, tTrf);
    rbTreeFree(&tTrf);
    qAddTrf(net, net->fillList, qChromHash);

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
