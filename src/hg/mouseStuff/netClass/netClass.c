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
#include "liftUp.h"
#include "chainNet.h"

static char const rcsid[] = "$Id: netClass.c,v 1.22 2008/09/03 19:20:38 markd Exp $";

/* Command line switches. */
char *tNewR = NULL;
char *qNewR = NULL;
boolean noAr = FALSE;
char *qRepeatTable = NULL;
char *tRepeatTable = NULL;
struct hash *liftHashT = NULL;
struct hash *liftHashQ = NULL;

/* Localmem obj shared by cached query rbTrees. */
struct lm *qLm = NULL;

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"tNewR", OPTION_STRING},
    {"qNewR", OPTION_STRING},
    {"noAr", OPTION_BOOLEAN},
    {"qRepeats", OPTION_STRING},
    {"tRepeats", OPTION_STRING},
    {"liftT", OPTION_STRING},
    {"liftQ", OPTION_STRING},
    {NULL, 0}
};

static void usage()
/* Explain usage and exit. */
{
errAbort(
  "netClass - Add classification info to net\n"
  "usage:\n"
  "   netClass [options] in.net tDb qDb out.net\n"
  "       tDb - database to fetch target repeat masker table information\n"
  "       qDb - database to fetch query repeat masker table information\n"
  "options:\n"
  "   -tNewR=dir - Dir of chrN.out.spec files, with RepeatMasker .out format\n"
  "                lines describing lineage specific repeats in target\n"
  "   -qNewR=dir - Dir of chrN.out.spec files for query\n"
  "   -noAr - Don't look for ancient repeats\n"
  "   -qRepeats=table - table name for query repeats in place of rmsk\n"
  "   -tRepeats=table - table name for target repeats in place of rmsk\n"
  "                   - for example: -tRepeats=windowmaskerSdust\n"
  "   -liftQ=file.lft - Lift in.net's query coords to chrom-level using\n"
  "                     file.lft (for accessing chrom-level coords in qDb)\n"
  "   -liftT=file.lft - Lift in.net's target coords to chrom-level using\n"
  "                     file.lft (for accessing chrom-level coords in tDb)\n"
  );
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

struct simpleRange
/* A part of a chromosome. */
    {
    int start, end;	/* Half open zero based coordinates. */
    };

#define overlap(r1, r2) ((r1)->start <= (r2)->end && (r2)->start <= (r1)->end)

int simpleRangeCmp(void *va, void *vb)
/* Return -1 if a before b,  0 if a and b overlap,
 * and 1 if a after b. */
{
struct simpleRange *a = va;
struct simpleRange *b = vb;
if (a->end <= b->start)
    return -1;
else if (b->end <= a->start)
    return 1;
else
    return 0;
}

static int interSize;	/* Size of intersection. */
static struct simpleRange interRange; /* Range to intersect with. */

void addInterSize(void *item)
/* Add range to interSize. */
{
struct simpleRange *r = item;
int size;
size = rangeIntersection(r->start, r->end, interRange.start, interRange.end);
interSize += size;
}

int intersectionSize(struct rbTree *tree, int start, int end)
/* Return total size of things intersecting range start-end. */
{
if (tree == NULL)
    return 0;
interRange.start = start;
interRange.end = end;
interSize = 0;
rbTreeTraverseRange(tree, &interRange, &interRange, addInterSize);
return interSize;
}

void setNGap(char *chr, struct hash *chromHash, struct rbTree *tree)
/* Set the appropriate chrom->nGaps to tree -- make sure it's NULL first. */
{
struct chrom *chrom = hashMustFindVal(chromHash, chr);
if (chrom->nGaps != NULL)
    errAbort("getSeqGapsUnsplit: query results must be orderd by chrom, but "
	     "looks like they weren't for %s", chr);
chrom->nGaps = tree;
}

void getSeqGapsUnsplit(struct sqlConnection *conn, struct hash *chromHash)
/* Return a tree of ranges for sequence gaps in all chromosomes, 
 * assuming an unsplit gap table -- when the table is unsplit, it's 
 * probably for a scaffold assembly where we *really* don't want 
 * to do one query per scaffold! */
{
struct rbTreeNode **stack = lmAlloc(qLm, 256 * sizeof(stack[0]));
struct rbTree *tree = rbTreeNewDetailed(simpleRangeCmp, qLm, stack);
int rowOffset = hOffsetPastBin(sqlGetDatabase(conn), NULL, "gap");
struct sqlResult *sr;
char **row;
char *prevChrom = NULL;

sr = sqlGetResult(conn, "select * from gap order by chrom");
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct agpGap gap;
    struct simpleRange *range;
    agpGapStaticLoad(row+rowOffset, &gap);
    if (prevChrom == NULL)
	prevChrom = cloneString(gap.chrom);
    else if (! sameString(prevChrom, gap.chrom))
	{
	setNGap(prevChrom, chromHash, tree);
	freeMem(prevChrom);
	stack = lmAlloc(qLm, 256 * sizeof(stack[0]));
	tree = rbTreeNewDetailed(simpleRangeCmp, qLm, stack);
	prevChrom = cloneString(gap.chrom);
	}
    lmAllocVar(tree->lm, range);
    range->start = gap.chromStart;
    range->end = gap.chromEnd;
    rbTreeAdd(tree, range);
    }
if (prevChrom != NULL)
    {
    setNGap(prevChrom, chromHash, tree);
    freeMem(prevChrom);
    }
sqlFreeResult(&sr);
}

struct rbTree *getSeqGaps(struct sqlConnection *conn, char *chrom)
/* Return a tree of ranges for sequence gaps in chromosome */
{
struct rbTree *tree = rbTreeNew(simpleRangeCmp);
int rowOffset;
struct sqlResult *sr = hChromQuery(conn, "gap", chrom, NULL, &rowOffset);
char **row;

while ((row = sqlNextRow(sr)) != NULL)
    {
    struct agpGap gap;
    struct simpleRange *range;
    agpGapStaticLoad(row+rowOffset, &gap);
    lmAllocVar(tree->lm, range);
    range->start = gap.chromStart;
    range->end = gap.chromEnd;
    rbTreeAdd(tree, range);
    }
sqlFreeResult(&sr);
return tree;
}

void setTrf(char *chr, struct hash *chromHash, struct rbTree *tree)
/* Set the appropriate chrom->trf to tree -- make sure it's NULL first. */
{
struct chrom *chrom = hashMustFindVal(chromHash, chr);
if (chrom->trf != NULL)
    errAbort("getTrfUnsplit: query results must be orderd by chrom, but "
	     "looks like they weren't for %s", chr);
chrom->trf = tree;
}

void getTrfUnsplit(struct sqlConnection *conn, struct hash *chromHash)
/* Return a tree of ranges for simple repeats in all chromosomes, 
 * from a single query on the whole (unsplit) simpleRepeat table. */
{
struct rbTreeNode **stack = lmAlloc(qLm, 256 * sizeof(stack[0]));
struct rbTree *tree = rbTreeNewDetailed(simpleRangeCmp, qLm, stack);
struct simpleRange *range, *prevRange = NULL;
struct sqlResult *sr;
char **row;
char *prevChrom = NULL;

sr = sqlGetResult(conn, "select chrom,chromStart,chromEnd from simpleRepeat"
		  " order by chrom,chromStart");
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (prevChrom == NULL)
	prevChrom = cloneString(row[0]);
    else if (! sameString(prevChrom, row[0]))
	{
	rbTreeAdd(tree, prevRange);
	setTrf(prevChrom, chromHash, tree);
	prevRange = NULL;
	freeMem(prevChrom);
	stack = lmAlloc(qLm, 256 * sizeof(stack[0]));
	tree = rbTreeNewDetailed(simpleRangeCmp, qLm, stack);
	prevChrom = cloneString(row[0]);
	}
    lmAllocVar(tree->lm, range);
    range->start = sqlUnsigned(row[1]);
    range->end = sqlUnsigned(row[2]);
    if (prevRange == NULL)
	prevRange = range;
    else if (overlap(range, prevRange))
	{
	/* merge r into prevR & discard; prevR gets passed forward. */
	if (range->end > prevRange->end)
	    prevRange->end = range->end;
	if (range->start < prevRange->start)
	    prevRange->start = range->start;
	}
    else
	{
	rbTreeAdd(tree, prevRange);
	prevRange = range;
	}
    }
if (prevChrom != NULL)
    {
    rbTreeAdd(tree, prevRange);
    setTrf(prevChrom, chromHash, tree);
    freeMem(prevChrom);
    }
sqlFreeResult(&sr);
}

struct rbTree *getTrf(struct sqlConnection *conn, char *chrom)
/* Return a tree of ranges for simple repeats in chromosome. */
{
struct rbTree *tree = rbTreeNew(simpleRangeCmp);
struct simpleRange *range, *prevRange = NULL;
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
    if (prevRange == NULL)
	prevRange = range;
    else if (overlap(range, prevRange))
	{
	/* merge r into prevR & discard; prevR gets passed forward. */
	if (range->end > prevRange->end)
	    prevRange->end = range->end;
	if (range->start < prevRange->start)
	    prevRange->start = range->start;
	}
    else
	{
	rbTreeAdd(tree, prevRange);
	prevRange = range;
	}
    }
if (prevRange != NULL)
    rbTreeAdd(tree, prevRange);
sqlFreeResult(&sr);
return tree;
}

static void setRepeats(char *chr, struct hash *chromHash,
	struct rbTree *allTree, struct rbTree *newTree)
/* Set the appropriate chrom->repeats to tree -- make sure it's NULL first. */
{
struct chrom *chrom = hashMustFindVal(chromHash, chr);
if (chrom->repeats != NULL)
    errAbort("getRepeatsUnsplit: query results must be orderd by genoName "
	     "(chrom), but looks like they weren't for %s", chr);
chrom->repeats = allTree;
chrom->newRepeats = newTree;
}

static void getRepeatsUnsplit(struct sqlConnection *conn,
	struct hash *chromHash, struct hash *arHash)
/* Return a tree of ranges for sequence gaps all chromosomes, 
 * assuming an unsplit table -- when the table is unsplit, it's 
 * probably for a scaffold assembly where we *really* don't want 
 * to do one query per scaffold! */
{
struct sqlResult *sr;
char **row;
struct rbTreeNode **stack = lmAlloc(qLm, 256 * sizeof(stack[0]));
struct rbTree *allTree = rbTreeNewDetailed(simpleRangeCmp, qLm, stack);
struct rbTreeNode **newstack = lmAlloc(qLm, 256 * sizeof(newstack[0]));
struct rbTree *newTree = rbTreeNewDetailed(simpleRangeCmp, qLm, newstack);
char *prevChrom = NULL;
struct simpleRange *prevRange = NULL, *prevNewRange = NULL;

sr = sqlGetResult(conn,
    "select genoName,genoStart,genoEnd,repName,repClass,repFamily from rmsk "
    "order by genoName,genoStart");
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct simpleRange *range;
    char arKey[512];
    if (prevChrom == NULL)
	prevChrom = cloneString(row[0]);
    else if (! sameString(prevChrom, row[0]))
	{
	rbTreeAdd(allTree, prevRange);
	if (prevNewRange != NULL)
	    rbTreeAdd(newTree, prevNewRange);
	setRepeats(prevChrom, chromHash, allTree, newTree);
	freeMem(prevChrom);
	prevRange = prevNewRange = NULL;
	stack = lmAlloc(qLm, 256 * sizeof(stack[0]));
	allTree = rbTreeNewDetailed(simpleRangeCmp, qLm, stack);
	if (arHash != NULL)
	    {
	    stack = lmAlloc(qLm, 256 * sizeof(stack[0]));
	    newTree = rbTreeNewDetailed(simpleRangeCmp, qLm, stack);
	    }
	prevChrom = cloneString(row[0]);
	}
    lmAllocVar(allTree->lm, range);
    range->start = sqlUnsigned(row[1]);
    range->end = sqlUnsigned(row[2]);
    if (prevRange == NULL)
	prevRange = range;
    else if (overlap(range, prevRange))
	{
	/* merge r into prevR & discard; prevR gets passed forward. */
	if (range->end > prevRange->end)
	    prevRange->end = range->end;
	if (range->start < prevRange->start)
	    prevRange->start = range->start;
	}
    else
	{
	rbTreeAdd(allTree, prevRange);
	prevRange = range;
	}
    sprintf(arKey, "%s.%s.%s", row[3], row[4], row[5]);
    if (arHash != NULL && hashLookup(arHash, arKey))
        {
	lmAllocVar(newTree->lm, range);
	range->start = sqlUnsigned(row[1]);
	range->end = sqlUnsigned(row[2]);
	if (prevNewRange == NULL)
	    prevNewRange = range;
	else if (overlap(range, prevNewRange))
	    {
	    /* merge r into prevR & discard; prevR gets passed forward. */
	    if (range->end > prevNewRange->end)
		prevNewRange->end = range->end;
	    if (range->start < prevNewRange->start)
		prevNewRange->start = range->start;
	    }
	else
	    {
	    rbTreeAdd(newTree, prevNewRange);
	    prevNewRange = range;
	    }
	}
    }
if (prevChrom != NULL)
    {
    rbTreeAdd(allTree, prevRange);
    if (prevNewRange != NULL)
	rbTreeAdd(newTree, prevNewRange);
    setRepeats(prevChrom, chromHash, allTree, newTree);
    freeMem(prevChrom);
    }
sqlFreeResult(&sr);
}

static void getRepeatsUnsplitTable(struct sqlConnection *conn,
	struct hash *chromHash, char *table)
/* Return a tree of ranges for sequence gaps all chromosomes, 
 *	from specified table
 */
{
struct sqlResult *sr;
char **row;
struct rbTreeNode **stack = lmAlloc(qLm, 256 * sizeof(stack[0]));
struct rbTree *allTree = rbTreeNewDetailed(simpleRangeCmp, qLm, stack);
struct rbTreeNode **newstack = lmAlloc(qLm, 256 * sizeof(newstack[0]));
struct rbTree *newTree = rbTreeNewDetailed(simpleRangeCmp, qLm, newstack);
char *prevChrom = NULL;
struct simpleRange *prevRange = NULL, *prevNewRange = NULL;
char query[256];


safef(query, ArraySize(query), "select chrom,chromStart,chromEnd from %s "
    "order by chrom,chromStart", table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct simpleRange *range;
    if (prevChrom == NULL)
	prevChrom = cloneString(row[0]);
    else if (! sameString(prevChrom, row[0]))
	{
	rbTreeAdd(allTree, prevRange);
	if (prevNewRange != NULL)
	    rbTreeAdd(newTree, prevNewRange);
	setRepeats(prevChrom, chromHash, allTree, newTree);
	freeMem(prevChrom);
	prevRange = prevNewRange = NULL;
	stack = lmAlloc(qLm, 256 * sizeof(stack[0]));
	allTree = rbTreeNewDetailed(simpleRangeCmp, qLm, stack);
	prevChrom = cloneString(row[0]);
	}
    lmAllocVar(allTree->lm, range);
    range->start = sqlUnsigned(row[1]);
    range->end = sqlUnsigned(row[2]);
    if (prevRange == NULL)
	prevRange = range;
    else if (overlap(range, prevRange))
	{
	/* merge r into prevR & discard; prevR gets passed forward. */
	if (range->end > prevRange->end)
	    prevRange->end = range->end;
	if (range->start < prevRange->start)
	    prevRange->start = range->start;
	}
    else
	{
	rbTreeAdd(allTree, prevRange);
	prevRange = range;
	}
    }
if (prevChrom != NULL)
    {
    rbTreeAdd(allTree, prevRange);
    if (prevNewRange != NULL)
	rbTreeAdd(newTree, prevNewRange);
    setRepeats(prevChrom, chromHash, allTree, newTree);
    freeMem(prevChrom);
    }
sqlFreeResult(&sr);
}	/*	void getRepeatsUnsplitTable()	*/

static void getRepeats(struct sqlConnection *conn, struct hash *arHash,
    char *chrom, struct rbTree **retAllRepeats,
	struct rbTree **retNewRepeats)
/* Return a tree of ranges for sequence gaps in chromosome */
{
char *db = sqlGetDatabase(conn);
struct sqlResult *sr;
char **row;
struct rbTree *allTree = rbTreeNew(simpleRangeCmp);
struct rbTree *newTree = rbTreeNew(simpleRangeCmp);
char tableName[64];
char query[256];
boolean splitRmsk = TRUE;
struct simpleRange *prevRange = NULL, *prevNewRange = NULL;

safef(tableName, sizeof(tableName), "%s_rmsk", chrom);
if (! sqlTableExists(conn, tableName))
    {
    safef(tableName, sizeof(tableName), "rmsk");
    if (! sqlTableExists(conn, tableName))
	errAbort("Can't find rmsk table for %s (%s.%s_rmsk or %s.rmsk)\n",
		 chrom, db, chrom, db);
    splitRmsk = FALSE;
    }
if (splitRmsk)
    sprintf(query,
	    "select genoStart,genoEnd,repName,repClass,repFamily from %s",
	    tableName);
else
    sprintf(query,
	    "select genoStart,genoEnd,repName,repClass,repFamily from %s "
	    "where genoName = \"%s\"",
	    tableName, chrom);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct simpleRange *range;
    char arKey[512];
    lmAllocVar(allTree->lm, range);
    range->start = sqlUnsigned(row[0]);
    range->end = sqlUnsigned(row[1]);
    if (prevRange == NULL)
	prevRange = range;
    else if (overlap(range, prevRange))
	{
	/* merge r into prevR & discard; prevR gets passed forward. */
	if (range->end > prevRange->end)
	    prevRange->end = range->end;
	if (range->start < prevRange->start)
	    prevRange->start = range->start;
	}
    else
	{
	rbTreeAdd(allTree, prevRange);
	prevRange = range;
	}
    sprintf(arKey, "%s.%s.%s", row[2], row[3], row[4]);
    if (arHash != NULL && hashLookup(arHash, arKey))
        {
	lmAllocVar(newTree->lm, range);
	range->start = sqlUnsigned(row[0]);
	range->end = sqlUnsigned(row[1]);
	if (prevNewRange == NULL)
	    prevNewRange = range;
	else if (overlap(range, prevNewRange))
	    {
	    /* merge r into prevR & discard; prevR gets passed forward. */
	    if (range->end > prevNewRange->end)
		prevNewRange->end = range->end;
	    if (range->start < prevNewRange->start)
		prevNewRange->start = range->start;
	    }
	else
	    {
	    rbTreeAdd(allTree, prevNewRange);
	    prevNewRange = range;
	    }
	}
    }
if (prevRange != NULL)
    rbTreeAdd(allTree, prevRange);
if (prevNewRange != NULL)
    rbTreeAdd(newTree, prevNewRange);
sqlFreeResult(&sr);
*retAllRepeats = allTree;
*retNewRepeats = newTree;
}

static void getRepeatsTable(struct sqlConnection *conn, char *table,
    char *chrom, struct rbTree **retAllRepeats,
	struct rbTree **retNewRepeats)
/* Return a tree of ranges for sequence gaps in chromosome from
 *	specified table */
{
struct sqlResult *sr;
char **row;
struct rbTree *allTree = rbTreeNew(simpleRangeCmp);
struct rbTree *newTree = rbTreeNew(simpleRangeCmp);
char query[256];
struct simpleRange *prevRange = NULL, *prevNewRange = NULL;

safef(query, ArraySize(query), "select chromStart,chromEnd from %s "
	    "where chrom = \"%s\"", table, chrom);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct simpleRange *range;
    lmAllocVar(allTree->lm, range);
    range->start = sqlUnsigned(row[0]);
    range->end = sqlUnsigned(row[1]);
    if (prevRange == NULL)
	prevRange = range;
    else if (overlap(range, prevRange))
	{
	/* merge r into prevR & discard; prevR gets passed forward. */
	if (range->end > prevRange->end)
	    prevRange->end = range->end;
	if (range->start < prevRange->start)
	    prevRange->start = range->start;
	}
    else
	{
	rbTreeAdd(allTree, prevRange);
	prevRange = range;
	}
    }
if (prevRange != NULL)
    rbTreeAdd(allTree, prevRange);
if (prevNewRange != NULL)
    rbTreeAdd(newTree, prevNewRange);
sqlFreeResult(&sr);
*retAllRepeats = allTree;
*retNewRepeats = newTree;
}	/*	static void getRepeatsTable()	*/

static struct rbTree *getNewRepeats(char *dirName, char *chrom)
/* Read in repeatMasker .out line format file into a tree of ranges. */
/* Handles lineage-specific files that preserve header */
{
struct rbTree *tree = rbTreeNew(simpleRangeCmp);
struct simpleRange *range;
char fileName[512];
struct lineFile *lf;
char *row[7];
boolean headerDone = FALSE;

sprintf(fileName, "%s/%s.out.spec", dirName, chrom);
lf = lineFileOpen(fileName, TRUE);
while (lineFileRow(lf, row))
    {
    /* skip header lines (don't contain numeric first field) */
    if (!headerDone && atoi(row[0]) == 0)
        continue;
    if (!sameString(chrom, row[4]))
        errAbort("Expecting %s word 5, line %d of %s\n", 
		chrom, lf->lineIx, lf->fileName);
    headerDone = TRUE;
    lmAllocVar(tree->lm, range);
    range->start = lineFileNeedNum(lf, row, 5) - 1;
    range->end = lineFileNeedNum(lf, row, 6);
    rbTreeAdd(tree, range);
    }
lineFileClose(&lf);
return tree;
}

struct hash *getAncientRepeats(struct sqlConnection *tConn,
			       struct sqlConnection *qConn)
/* Get hash of ancient repeats.  This keyed by name.family.class. */
{
struct sqlConnection *conn = NULL;
struct sqlResult *sr;
char **row;
char key[512];
struct hash *hash = newHash(10);

if (sqlTableExists(tConn, "ancientRepeat"))
    conn = tConn;
else if (sqlTableExists(qConn, "ancientRepeat"))
    conn = qConn;
else
    errAbort("Can't find ancientRepeat table in %s or %s",
	     sqlGetDatabase(tConn), sqlGetDatabase(qConn));
sr = sqlGetResult(conn, "select name,family,class from ancientRepeat");
while ((row = sqlNextRow(sr)) != NULL)
    {
    sprintf(key, "%s.%s.%s", row[0], row[1], row[2]);
    hashAdd(hash, key, NULL);
    }
sqlFreeResult(&sr);
return hash;
}

void getChroms(struct sqlConnection *conn, struct hash **retHash,
	       struct chrom **retList)
/* Get hash of chromosomes from database. */
{
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
slReverse(&chromList);
*retHash = hash;
*retList = chromList;
}

int liftStart(char *name, int start, struct hash *liftHash)
/* Lift start if necessary. */
{
int s = start;
if (liftHash != NULL)
    {
    struct liftSpec *lft = hashMustFindVal(liftHash, name);
    s += lft->offset;
    }
return s;
}

void tAddN(struct chainNet *net, struct cnFill *fillList, struct rbTree *tree)
/* Add tN's to all gaps underneath fillList. */
{
struct cnFill *fill;
for (fill = fillList; fill != NULL; fill = fill->next)
    {
    int s = liftStart(net->name, fill->tStart, liftHashT);
    fill->tN = intersectionSize(tree, s, s + fill->tSize);
    if (fill->children)
	tAddN(net, fill->children, tree);
    }
}

struct chrom *getQChrom(char *qName, struct hash *qChromHash)
/* Lift qName to chrom if necessary and dig up from qChromHash. */
{
struct chrom *qChrom = NULL;
if (liftHashQ != NULL)
    {
    struct liftSpec *lft = hashMustFindVal(liftHashQ, qName);
    qChrom = hashMustFindVal(qChromHash, lft->newName);
    }
else
    qChrom = hashMustFindVal(qChromHash, qName);
return(qChrom);
}

void qAddN(struct chainNet *net, struct cnFill *fillList, struct hash *qChromHash)
/* Add qN's to all gaps underneath fillList. */
{
struct cnFill *fill;
for (fill = fillList; fill != NULL; fill = fill->next)
    {
    struct chrom *qChrom = getQChrom(fill->qName, qChromHash);
    struct rbTree *tree = qChrom->nGaps;
    int s = liftStart(fill->qName, fill->qStart, liftHashQ);
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
    int s = liftStart(net->name, fill->tStart, liftHashT);
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
    struct chrom *qChrom = getQChrom(fill->qName, qChromHash);
    int s = liftStart(fill->qName, fill->qStart, liftHashQ);
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
    int s = liftStart(net->name, fill->tStart, liftHashT);
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
    struct chrom *qChrom = getQChrom(fill->qName, qChromHash);
    int s = liftStart(fill->qName, fill->qStart, liftHashQ);
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
    int s = liftStart(net->name, fill->tStart, liftHashT);
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
    struct chrom *qChrom = getQChrom(fill->qName, qChromHash);
    int s = liftStart(fill->qName, fill->qStart, liftHashQ);
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
    int s = liftStart(net->name, fill->tStart, liftHashT);
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
    struct chrom *qChrom = getQChrom(fill->qName, qChromHash);
    int s = liftStart(fill->qName, fill->qStart, liftHashQ);
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
struct hash *arHash = NULL;
struct sqlConnection *tConn = sqlConnect(tDb);
struct sqlConnection *qConn = sqlConnect(qDb);

qLm = lmInit(0);

if (!noAr)
    arHash = getAncientRepeats(tConn, qConn);

getChroms(qConn, &qChromHash, &qChromList);

verbose(1, "Reading gaps in %s\n", qDb);
if (sqlTableExists(qConn, "gap"))
    {
    getSeqGapsUnsplit(qConn, qChromHash);
    }
else
    {
    for (chrom = qChromList; chrom != NULL; chrom = chrom->next)
	chrom->nGaps = getSeqGaps(qConn, chrom->name);
    }

if (qNewR)
    {
    verbose(1, "Reading new repeats from %s\n", qNewR);
    for (chrom = qChromList; chrom != NULL; chrom = chrom->next)
        chrom->newRepeats = getNewRepeats(qNewR, chrom->name);
    }

verbose(1, "Reading simpleRepeats in %s\n", qDb);
getTrfUnsplit(qConn, qChromHash);

if (qRepeatTable)
    {
    verbose(1, "Reading repeats in %s from table %s\n", qDb, qRepeatTable);
    getRepeatsUnsplitTable(qConn, qChromHash, qRepeatTable);
    }
else
    {
    verbose(1, "Reading repeats in %s\n", qDb);
    if (sqlTableExists(qConn, "rmsk"))
	getRepeatsUnsplit(qConn, qChromHash, arHash);
    else
	{
	for (chrom = qChromList; chrom != NULL; chrom = chrom->next)
	    getRepeats(qConn, arHash, chrom->name, &chrom->repeats,
		       &chrom->oldRepeats);
	}
    }

while ((net = chainNetRead(lf)) != NULL)
    {
    struct rbTree *tN, *tRepeats, *tOldRepeats, *tTrf;
    char *tName = net->name;
    if (liftHashT != NULL)
	{
	struct liftSpec *lft = hashMustFindVal(liftHashT, net->name);
	tName = lft->newName;
	}

    verbose(1, "Processing %s.%s\n", tDb, net->name);
    tN = getSeqGaps(tConn, tName);
    tAddN(net, net->fillList, tN);
    rbTreeFree(&tN);
    qAddN(net, net->fillList, qChromHash);

    if (tRepeatTable)
	getRepeatsTable(tConn, tRepeatTable, tName, &tRepeats, &tOldRepeats);
    else
	getRepeats(tConn, arHash, tName, &tRepeats, &tOldRepeats);
    tAddR(net, net->fillList, tRepeats);
    if (!noAr)
	tAddOldR(net, net->fillList, tOldRepeats);
    rbTreeFree(&tRepeats);
    rbTreeFree(&tOldRepeats);
    qAddR(net, net->fillList, qChromHash);
    if (!noAr)
	qAddOldR(net, net->fillList, qChromHash);

    tTrf = getTrf(tConn, tName);
    tAddTrf(net, net->fillList, tTrf);
    rbTreeFree(&tTrf);
    qAddTrf(net, net->fillList, qChromHash);

    if (tNewR)
        {
	struct rbTree *tree = getNewRepeats(tNewR, tName);
	tAddNewR(net, net->fillList, tree);
	rbTreeFree(&tree);
	}
    if (qNewR)
        qAddNewR(net, net->fillList, qChromHash);
    chainNetWrite(net, f);
    chainNetFree(&net);
    }
sqlDisconnect(&tConn);
sqlDisconnect(&qConn);
}


int main(int argc, char *argv[])
/* Process command line. */
{
char *liftFileQ = NULL, *liftFileT = NULL;
optionInit(&argc, argv, optionSpecs);
if (argc != 5)
    usage();
tNewR = optionVal("tNewR", tNewR);
qNewR = optionVal("qNewR", qNewR);
noAr = optionExists("noAr");
qRepeatTable = optionVal("qRepeats", qRepeatTable);
tRepeatTable = optionVal("tRepeats", tRepeatTable);
liftFileQ = optionVal("liftQ", liftFileQ);
liftFileT = optionVal("liftT", liftFileT);
if (liftFileQ != NULL)
    {
    struct liftSpec *lifts = readLifts(liftFileQ);
    liftHashQ = hashLift(lifts, TRUE);
    }
if (liftFileT != NULL)
    {
    struct liftSpec *lifts = readLifts(liftFileT);
    liftHashT = hashLift(lifts, TRUE);
    }
netClass(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
