/* cacheMergedRanges - compare speeds of different ways to cache position 
 * ranges from a SQL table, removing overlaps. */
#include "common.h"
#include "options.h"
#include "jksql.h"
#include "hash.h"
#include "dystring.h"
#include "portable.h"
#include "hdb.h"
#include "rbTree.h"
#include "rbmTree.h"


/* Need to get a cart in order to use hgFind. */
struct cart *cart = NULL;

/* Command line option specifications */
static struct optionSpec optionSpecs[] = {
    {NULL, 0}
};


void usage()
{
errAbort(
"cacheMergedRanges - compare speeds of 3 query/sort/merge/store methods.\n"
"usage:\n"
"  cacheMergedRanges database bedTable\n"
/*
"options:\n"
*/
);
}


//** BEGIN STUFF COPIED FROM NETCLASS AND MODIFIED
struct range
/* A part of a chromosome. */
    {
    struct range *next;
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

//** END STUFF COPIED FROM NETCLASS AND MODIFIED

int rangeCmpStart(const void *va, const void *vb)
/* Return -1 if a->start before b->start,  0 if a and b equal,
 * and 1 if a after b. */
{
struct range *a = *(struct range **)va;
struct range *b = *(struct range **)vb;
return a->start - b->start;
}

rbmTreeCompareResult rangeCmpMerge(void *va, void *vb)
/* Comparison function for use by rbTreeAddMerge, with distinct return 
 * values for complete vs. partial overlap of values:
 * Return RBMT_LESS if a is strictly less than b, 
 * RBMT_GREATER if a is strictly greater than b,
 * RBMT_COMPLETE if a is completely contained/encompassed by b 
 *   (not nec. vice versa!  mnemonic: b may be bigger),
 * RBMT_PARTIAL if a and b have a partial overlap requiring a merge. */
{
struct range *a = (struct range *)va;
struct range *b = (struct range *)vb;
if (a->end < b->start)
    return RBMT_LESS;
else if (a->start > b->end)
    return RBMT_GREATER;
else if (a->start >= b->start && a->end <= b->end)
    return RBMT_COMPLETE;
else
    return RBMT_PARTIAL;
}


void rangeMerge(void *va, void *vb)
/* Item value-merging function for use by rbTreeAddMerge.  
 * Change b to encompass both a and b (again, b may be bigger). */
{
struct range *a = (struct range *)va;
struct range *b = (struct range *)vb;

if (a->start < b->start)
    b->start = a->start;
if (a->end > b->end)
    b->end = a->end;
}


char *makeQuery(char *table, boolean sortByChromStart)
/* Make a query to get chrom,chromStart,chromEnd (or whatever the appropriate 
 * field names are for table), ordered by chrom and possibly by start. */
{
char query[512];
char *db = hGetDb();
struct hTableInfo *hti = hFindTableInfo(NULL, table);
if (hti == NULL)
    errAbort("Can't find table info for %s.%s", db, table);
if (! hti->isPos)
    errAbort("Table must be positional, but looks like %s.%s isn't",
	     db, table);
if (sortByChromStart)
    sqlSafef(query, sizeof(query), "select %s,%s,%s from %s order by %s,%s",
	  hti->chromField, hti->startField, hti->endField, table,
	  hti->chromField, hti->startField);
else
    sqlSafef(query, sizeof(query), "select %s,%s,%s from %s order by %s",
	  hti->chromField, hti->startField, hti->endField, table,
	  hti->chromField);
return(cloneString(query));
}


void addRbTree(char *chrom, struct hash *chromHash, struct rbTree *tree)
/* Add tree to chromHash. */
{
struct hashEl *hel = hashLookup(chromHash, chrom);
if (hel != NULL)
    errAbort("resultsToMergedTreesMergeInline: need results ordered by chrom, "
	     "but looks like they weren't for %s.", chrom);
hashAdd(chromHash, chrom, tree);
}


struct hash *resultsToTreesMergeInline(struct sqlResult *sr)
/* Given results of a sorted query on chrom,chromStart,chromEnd, store results 
 * as rbTrees hashed by chrom. */
{
struct hash *chromHash = newHash(18);
char **row = NULL;
struct rbTree *t = rbTreeNew(rangeCmp);
char *prevChrom = NULL;
struct range *prevR = NULL;
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct range *r = NULL;
    AllocVar(r);
    r->start = sqlUnsigned(row[1]);
    r->end   = sqlUnsigned(row[2]);
    if (prevChrom == NULL)
	prevChrom = cloneString(row[0]);
    else if (! sameString(prevChrom, row[0]))
	{
	rbTreeAdd(t, prevR);
	addRbTree(prevChrom, chromHash, t);
	prevR = NULL;
	freeMem(prevChrom);
	prevChrom = cloneString(row[0]);
	t = rbTreeNew(rangeCmp);
	}
    if (prevR == NULL)
	prevR = r;
    else if (r->start <= prevR->end && prevR->start <= r->end)
	{
	/* Overlap: merge r into prevR & discard; prevR gets passed forward. */
	if (r->end > prevR->end)
	    prevR->end = r->end;
	if (r->start < prevR->start)
	    prevR->start = r->start;
	freez(&r);
	}
    else
	{
	rbTreeAdd(t, prevR);
	prevR = r;
	}
    }
if (prevChrom != NULL)
    {
    rbTreeAdd(t, prevR);
    addRbTree(prevChrom, chromHash, t);
    freeMem(prevChrom);
    }
return chromHash;
}

void mergeOverlaps(struct range **pRangeList)
/* Given a pointer to an *already sorted by start* list of ranges, 
 * merge overlapping ranges together and set the pointer to the new 
 * *reverse sorted* list. */
{
struct range *r = *pRangeList, *nextR = NULL, *newList = NULL;
while (r != NULL)
    {
    nextR = r->next;
    if (nextR != NULL && (nextR->start <= r->end) && (r->start <= nextR->end))
	{
	/* Overlap: merge r into nextR & discard; nextR gets passed forward. */
	if (r->end > nextR->end)
	    nextR->end = r->end;
	if (r->start < nextR->start)
	    nextR->start = r->start;
	freez(&r);
	}
    else
	slAddHead(&newList, r);
    r = nextR;
    }
*pRangeList = newList;
}


void addRangeListAsRbTree(char *chrom, struct hash *chromHash,
			  struct range *rangeList, boolean doSort)
/* Given a list of ranges for a chrom, sort if specified, merge overlapping
 * ranges, create a non-merging rbTree of ranges and store it in chromHash. */
{
struct rbTree *t = rbTreeNew(rangeCmp);
struct range *r = NULL;
struct hashEl *hel = hashLookup(chromHash, chrom);

if (hel != NULL)
    errAbort("resultsToTrees: need results ordered by chrom, "
	     "but looks like they weren't for %s.", chrom);
if (doSort)
    slSort(&rangeList, rangeCmpStart);
mergeOverlaps(&rangeList);
for (r = rangeList;  r != NULL;  r = r->next)
    {
    rbTreeAdd(t, r);
    }
hashAdd(chromHash, chrom, t);
}


struct hash *resultsToTrees(struct sqlResult *sr, boolean doSort)
/* Given results of a query on chrom,chromStart,chromEnd, store results 
 * as range lists (reversed!) hashed by chrom. */
{
struct hash *chromHash = newHash(18);
char **row = NULL;
struct range *rangeList = NULL;
char *prevChrom = NULL;
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct range *r = NULL;
    AllocVar(r);
    r->start = sqlUnsigned(row[1]);
    r->end   = sqlUnsigned(row[2]);
    /* Merging overlaps from a sorted query here would be more efficient. */
    if (prevChrom == NULL)
	prevChrom = cloneString(row[0]);
    else if (! sameString(prevChrom, row[0]))
	{
	addRangeListAsRbTree(prevChrom, chromHash, rangeList, doSort);
	freeMem(prevChrom);
	prevChrom = cloneString(row[0]);
	rangeList = NULL;
	}
    slAddHead(&rangeList, r);
    }
if (prevChrom != NULL)
    {
    addRangeListAsRbTree(prevChrom, chromHash, rangeList, doSort);
    freeMem(prevChrom);
    }
return chromHash;
}

struct hash *sqlSorting(struct sqlConnection *conn, char *table)
/* Grab contents of bed table into chrom-hashed range lists, 
 * using SQL to sort by chromStart; then merge ranges and 
 * convert to chrom-hashed rbTrees with normal rbTreeAdd. */
{
char *query = makeQuery(table, TRUE);
struct sqlResult *sr = NULL;
struct hash *chromTrees = NULL;
int startMs = 0, endMs = 0, totalMs = 0;

sqlUpdate(conn, "NOSQLINJ reset query cache");
startMs = clock1000();
sr = sqlGetResult(conn, query);
endMs = clock1000();
verbose(1, "sqlSorting: Took %dms to do sorting query.\n", endMs - startMs);
printf("%d\t", endMs - startMs);
totalMs += (endMs - startMs);

startMs = clock1000();
chromTrees = resultsToTreesMergeInline(sr);
endMs = clock1000();
verbose(1, "sqlSorting: Took %dms to build, merge and convert.\n",
       endMs - startMs);
printf("%d\t", endMs - startMs);
totalMs += (endMs - startMs);

verbose(1, "sqlSorting: Took %dms total.\n\n", totalMs);
printf("%d\t", totalMs);
freeMem(query);
sqlFreeResult(&sr);
return chromTrees;
}


struct hash *jkSorting(struct sqlConnection *conn, char *table)
/* Grab contents of bed table into chrom-hashed range lists, 
 * using slSort() to sort by start; then merge ranges and 
 * convert to chrom-hashed rbTrees with normal rbTreeAdd. */
{
char *query = makeQuery(table, FALSE);
struct sqlResult *sr = NULL;
struct hash *chromTrees = NULL;
int startMs = 0, endMs = 0, totalMs = 0;

sqlUpdate(conn, "NOSQLINJ reset query cache");
startMs = clock1000();
sr = sqlGetResult(conn, query);
endMs = clock1000();
verbose(1, "jkSorting: Took %dms to do plain query.\n", endMs - startMs);
printf("%d\t", endMs - startMs);
totalMs += (endMs - startMs);

startMs = clock1000();
chromTrees = resultsToTrees(sr, TRUE);
endMs = clock1000();
verbose(1, "jkSorting: Took %dms to build, sort, merge, and convert.\n",
       endMs - startMs);
printf("%d\t", endMs - startMs);
totalMs += (endMs - startMs);

verbose(1, "jkSorting: Took %dms total.\n\n", totalMs);
printf("%d\t", totalMs);
freeMem(query);
sqlFreeResult(&sr);
return chromTrees;
}


void addRbmTree(char *chrom, struct hash *chromHash, struct rbmTree *tree)
/* Add tree to chromHash. */
{
struct hashEl *hel = hashLookup(chromHash, chrom);
if (hel != NULL)
    errAbort("resultsToMergedTrees: need results ordered by chrom, "
	     "but looks like they weren't for %s.", chrom);
hashAdd(chromHash, chrom, tree);
}


struct hash *resultsToMergedTrees(struct sqlResult *sr)
/* Given results of a query on chrom,chromStart,chromEnd, store results 
 * in chrom-hashed rbmTrees of non-overlapping (merged) ranges. */
{
struct hash *chromHash = newHash(18);
struct rbmTree *t = rbmTreeNew(rangeCmpMerge, rangeMerge, NULL, freeMem);
char **row = NULL;
char *prevChrom = NULL;
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct range *r = NULL;
    AllocVar(r);
    r->start = sqlUnsigned(row[1]);
    r->end   = sqlUnsigned(row[2]);
    if (prevChrom == NULL)
	prevChrom = cloneString(row[0]);
    else if (! sameString(prevChrom, row[0]))
	{
	addRbmTree(prevChrom, chromHash, t);
	freeMem(prevChrom);
	t = rbmTreeNew(rangeCmpMerge, rangeMerge, NULL, freeMem);
	prevChrom = cloneString(row[0]);
	}
    rbmTreeAdd(t, r);
    }
if (prevChrom != NULL)
    {
    addRbmTree(prevChrom, chromHash, t);
    freeMem(prevChrom);
    }
return chromHash;
}


struct hash *rbMerging(struct sqlConnection *conn, char *table)
/* Grab contents of bed table ordered by chrom but not sorted, 
 * then convert to chrom-hashed rbmTrees to handle merging of 
 * overlapping ranges. */
{
char *query = makeQuery(table, FALSE);
struct sqlResult *sr = NULL;
struct hash *chromTrees = NULL;
int startMs = 0, endMs = 0, totalMs = 0;

sqlUpdate(conn, "NOSQLINJ reset query cache");
startMs = clock1000();
sr = sqlGetResult(conn, query);
endMs = clock1000();
verbose(1, "rbMerging: Took %dms to do plain query.\n", endMs - startMs);
printf("%d\t", endMs - startMs);
totalMs += (endMs - startMs);

startMs = clock1000();
chromTrees = resultsToMergedTrees(sr);
endMs = clock1000();
verbose(1, "rbMerging: Took %dms to make merged trees.\n", endMs - startMs);
printf("%d\t", endMs - startMs);
totalMs += (endMs - startMs);

verbose(1, "rbMerging: Took %dms total.\n\n", totalMs);
printf("%d\n", totalMs);
freeMem(query);
sqlFreeResult(&sr);
return chromTrees;
}


/* Globals for use by hash-traversed rbTree-dumping function: */
FILE *dumpTreesF = NULL;
char *dumpChrom = NULL;
int dumpSeq = 0;

void dumpRange(void *item)
/* Dump one range to f as bed 4. */
{
struct range *r = (struct range *)(item);
fprintf(dumpTreesF, "%s\t%d\t%d\t%s.%d\n",
	dumpChrom, r->start, r->end, dumpChrom, ++dumpSeq);
}

void dumpOneRbTree(struct hashEl *hel)
/* Given a hel that associates a chrom name with an rbTree of non-overlapping 
 * ranges, dump out bed 4 like featureBits -bed. */
{
struct rbTree *t = (struct rbTree *)hel->val;
dumpChrom = hel->name;
dumpSeq = 0;
rbTreeTraverse(t, dumpRange);
}

void dumpOneRbmTree(struct hashEl *hel)
/* Given a hel that associates a chrom name with an rbmTree of non-overlapping 
 * ranges, dump out bed 4 like featureBits -bed. */
{
struct rbmTree *t = (struct rbmTree *)hel->val;
dumpChrom = hel->name;
dumpSeq = 0;
rbmTreeTraverse(t, dumpRange);
}

void dumpRbTrees(struct hash *chromTrees, char *table, char *suffix)
/* Dump out chrom-hashed rbTrees of non-overlapping ranges as bed 4 
 * for comparison with featureBits -bed output. */
{
char fname[512];
safef(fname, sizeof(fname), "%s.%s.%s.bed", hGetDb(), table, suffix);
dumpTreesF = mustOpen(fname, "w");
hashTraverseEls(chromTrees, dumpOneRbTree);
carefulClose(&dumpTreesF);
}


void dumpRbmTrees(struct hash *chromTrees, char *table, char *suffix)
/* Dump out chrom-hashed rbmTrees of non-overlapping ranges as bed 4 
 * for comparison with featureBits -bed output. */
{
char fname[512];
safef(fname, sizeof(fname), "%s.%s.%s.bed", hGetDb(), table, suffix);
dumpTreesF = mustOpen(fname, "w");
hashTraverseEls(chromTrees, dumpOneRbmTree);
carefulClose(&dumpTreesF);
}


void hashElFreeRbTree(struct hashEl *hel)
/* Free up an rbTree's contents and the tree itself. */
{
rbTreeFreeAll((struct rbTree **)(&(hel->val)), freeMem);
}

void hashElFreeRbmTree(struct hashEl *hel)
/* Free up an rbmTree's contents and the tree itself. */
{
rbmTreeFreeAll((struct rbmTree **)(&(hel->val)));
}

void freeRbTreeHash(struct hash **pTreeHash)
/* Free up a whole hash of rbTrees of ranges. */
{
hashTraverseEls(*pTreeHash, hashElFreeRbTree);
hashFree(pTreeHash);
}

void freeRbmTreeHash(struct hash **pTreeHash)
/* Free up a whole hash of rbmTrees of ranges. */
{
hashTraverseEls(*pTreeHash, hashElFreeRbmTree);
hashFree(pTreeHash);
}


void cacheMergedRanges(char *db, char *table)
/* cacheMergedRanges - compare speeds of different ways to cache position 
 * ranges from a SQL table, removing overlaps. */
{
struct sqlConnection *conn = NULL;
struct hash *chromTrees = NULL;

hSetDb(db);
conn = hAllocConn();

chromTrees = sqlSorting(conn, table);
dumpRbTrees(chromTrees, table, "sqlSorting");
freeRbTreeHash(&chromTrees);

chromTrees = jkSorting(conn, table);
dumpRbTrees(chromTrees, table, "jkSorting");
freeRbTreeHash(&chromTrees);

chromTrees = rbMerging(conn, table);
dumpRbmTrees(chromTrees, table, "rbMerging");
freeRbmTreeHash(&chromTrees);
}


int main(int argc, char *argv[])
{
optionInit(&argc, argv, optionSpecs);
if (argc != 3)
    usage();

cacheMergedRanges(argv[1], argv[2]);
return 0;
}
