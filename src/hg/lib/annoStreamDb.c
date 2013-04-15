/* annoStreamDb -- subclass of annoStreamer for database tables */

#include "annoStreamDb.h"
#include "annoGratorQuery.h"
#include "hdb.h"
#include "sqlNum.h"

struct annoStreamDb
    {
    struct annoStreamer streamer;	// Parent class members & methods
    // Private members
    struct sqlConnection *conn;		// Database connection (e.g. hg19 or customTrash)
    char *table;			// Table name, must exist in database
    struct sqlResult *sr;		// SQL query result from which we grab rows
    char *chromField;			// Name of chrom-ish column in table
    char *startField;			// Name of chromStart-ish column in table
    char *endField;			// Name of chromEnd-ish column in table
    char *endFieldIndexName;		// SQL index on end field, if any (can mess up sorting)
    int chromIx;			// Index of chrom-ish col in autoSql or bin-less table
    int startIx;			// Index of chromStart-ish col in autoSql or bin-less table
    int endIx;				// Index of chromEnd-ish col in autoSql or bin-less table
    boolean notSorted;			// TRUE if table is not sorted (e.g. genbank-updated)
    boolean hasBin;			// 1 if SQL table's first column is bin
    boolean omitBin;			// 1 if table hasBin and autoSql doesn't have bin
    boolean mergeBins;			// TRUE if query results will be in bin order
    struct annoRow *bigItemQueue;	// If mergeBins, accumulate coarse-bin items here
    struct annoRow *smallItemQueue;	// Max 1 item for merge-sorting with bigItemQueue
    struct lm *qLm;			// localmem for merge-sorting queues
    int minFinestBin;			// Smallest bin number for finest bin level
    boolean gotFinestBin;		// Flag that it's time to merge-sort with bigItemQueue
    };

static void asdSetRegion(struct annoStreamer *vSelf, char *chrom, uint regionStart, uint regionEnd)
/* Set region -- and free current sqlResult if there is one. */
{
annoStreamerSetRegion(vSelf, chrom, regionStart, regionEnd);
struct annoStreamDb *self = (struct annoStreamDb *)vSelf;
if (self->sr != NULL)
    sqlFreeResult(&(self->sr));
}

static void asdDoQuery(struct annoStreamDb *self)
/* Return a sqlResult for a query on table items in position range. */
// NOTE: it would be possible to implement filters at this level, as in hgTables.
{
struct annoStreamer *streamer = &(self->streamer);
struct dyString *query = dyStringCreate("select * from %s", self->table);
if (!streamer->positionIsGenome)
    {
    if (self->hasBin)
	{
	// Results will be in bin order, but we can restore chromStart order by
	// accumulating initial coarse-bin items and merge-sorting them with
	// subsequent finest-bin items which will be in chromStart order.
	self->mergeBins = TRUE;
	self->bigItemQueue = self->smallItemQueue = NULL;
	lmCleanup(&(self->qLm));
	self->qLm = lmInit(0);
	self->gotFinestBin = FALSE;
	}
    if (self->endFieldIndexName != NULL)
	// Don't let mysql use a (chrom, chromEnd) index because that messes up
	// sorting by chromStart.
	dyStringPrintf(query, " IGNORE INDEX (%s)", self->endFieldIndexName);
    dyStringPrintf(query, " where %s='%s'", self->chromField, streamer->chrom);
    int chromSize = annoAssemblySeqSize(streamer->assembly, streamer->chrom);
    if (streamer->regionStart != 0 || streamer->regionEnd != chromSize)
	{
	dyStringAppend(query, " and ");
	if (self->hasBin)
	    hAddBinToQuery(streamer->regionStart, streamer->regionEnd, query);
	dyStringPrintf(query, "%s < %u and %s > %u", self->startField, streamer->regionEnd,
		       self->endField, streamer->regionStart);
	}
    if (self->notSorted)
	dyStringPrintf(query, " order by %s", self->startField);
    }
else if (self->notSorted)
    dyStringPrintf(query, " order by %s,%s", self->chromField, self->startField);
struct sqlResult *sr = sqlGetResult(self->conn, query->string);
dyStringFree(&query);
self->sr = sr;
}

static char **nextRowFiltered(struct annoStreamDb *self, boolean *retRightFail)
/* Skip past any left-join failures until we get a right-join failure, a passing row,
 * or end of data.  Return row or NULL, and return right-join fail status via retRightFail. */
{
int numCols = self->streamer.numCols;
char **row = sqlNextRow(self->sr);
boolean rightFail = FALSE;
struct annoFilter *filterList = self->streamer.filters;
while (row && annoFilterRowFails(filterList, row+self->omitBin, numCols, &rightFail))
    {
    if (rightFail)
	break;
    row = sqlNextRow(self->sr);
    }
*retRightFail = rightFail;
return row;
}

static struct annoRow *rowToAnnoRow(struct annoStreamDb *self, char **row, boolean rightFail,
				    struct lm *lm)
/* Extract coords from row and return an annoRow including right-fail status. */
{
row += self->omitBin;
char *chrom = row[self->chromIx];
uint chromStart = sqlUnsigned(row[self->startIx]);
uint chromEnd = sqlUnsigned(row[self->endIx]);
return annoRowFromStringArray(chrom, chromStart, chromEnd, rightFail, row,
			      self->streamer.numCols, lm);
}

static char **getFinestBinItem(struct annoStreamDb *self, char **row, boolean *pRightFail)
/* If row is a coarse-bin item, add it to bigItemQueue, get the next row(s) and
 * add any subsequent coarse-bin items to bigItemQueue.  As soon as we get an item from a
 * finest-level bin (or NULL), sort the bigItemQueue and return the finest-bin item/row. */
{
int bin = atoi(row[0]);
while (bin < self->minFinestBin)
    {
    // big item -- store aside in queue for merging later, move on to next item
    slAddHead(&(self->bigItemQueue), rowToAnnoRow(self, row, *pRightFail, self->qLm));
    *pRightFail = FALSE;
    row = nextRowFiltered(self, pRightFail);
    if (row == NULL)
	break;
    bin = atoi(row[0]);
    }
// First finest-bin item!  Sort bigItemQueue in preparation for merging:
self->gotFinestBin = TRUE;
slReverse(&(self->bigItemQueue));
slSort(&(self->bigItemQueue), annoRowCmp);
return row;
}

static struct annoRow *mergeRow(struct annoStreamDb *self, struct annoRow *aRow,
				struct lm *callerLm)
/* Compare head of bigItemQueue with (finest-bin) aRow; return the one with
 * lower chromStart and save the other for later.  */
{
struct annoRow *outRow = aRow;
if (self->bigItemQueue == NULL)
    {
    // No coarse-bin items to merge-sort, just stream finest-bin items from here on out.
    self->mergeBins = FALSE;
    }
else if (annoRowCmp(&(self->bigItemQueue), &aRow) < 0)
    {
    // Big item gets to go now, so save aside small item for next time.
    outRow = slPopHead(&(self->bigItemQueue));
    slAddHead(&(self->smallItemQueue), aRow);
    }
enum annoRowType rowType = self->streamer.rowType;
int numCols = self->streamer.numCols;
return annoRowClone(outRow, rowType, numCols, callerLm);
}

static struct annoRow *nextQueuedRow(struct annoStreamDb *self, struct lm *callerLm)
// Return the head of either bigItemQueue or smallItemQueue, depending on which has
// the lower chromStart.
{
struct annoRow *row = NULL;
if (self->bigItemQueue && annoRowCmp(&(self->bigItemQueue), &(self->smallItemQueue)) < 0)
    row = slPopHead(&(self->bigItemQueue));
else
    row = slPopHead(&(self->smallItemQueue));
if (self->bigItemQueue == NULL && self->smallItemQueue == NULL)
    // All done merge-sorting, just stream finest-bin items from here on out.
    self->mergeBins = FALSE;
enum annoRowType rowType = self->streamer.rowType;
int numCols = self->streamer.numCols;
return annoRowClone(row, rowType, numCols, callerLm);
}

static struct annoRow *nextRowMergeBins(struct annoStreamDb *self, struct lm *callerLm)
/* Fetch the next filtered row from mysql, merge-sorting coarse-bin items into finest-bin
 * items to maintain chromStart ordering. */
{
assert(self->mergeBins && self->hasBin);
if (self->smallItemQueue)
    // In this case we have already begun merge-sorting; don't pull a new row from mysql,
    // use the queues.  This should keep smallItemQueue's max depth at 1.
    return nextQueuedRow(self, callerLm);
else
    {
    // We might need to collect initial coarse-bin items, or might already be merge-sorting.
    boolean rightFail = FALSE;
    char **row = nextRowFiltered(self, &rightFail);
    if (row && !self->gotFinestBin)
	{
	// We are just starting -- queue up coarse-bin items, if any, until we get the first
	// finest-bin item.
	row = getFinestBinItem(self, row, &rightFail);
	}
    // Time to merge-sort finest-bin items from mysql with coarse-bin items from queue.
    if (row != NULL)
	{
	struct annoRow *aRow = rowToAnnoRow(self, row, rightFail, self->qLm);
	return mergeRow(self, aRow, callerLm);
	}
    else
	{
	struct annoRow *qRow = slPopHead(&(self->bigItemQueue));
	enum annoRowType rowType = self->streamer.rowType;
	int numCols = self->streamer.numCols;
	return annoRowClone(qRow, rowType, numCols, callerLm);
	}
    }
}

static struct annoRow *asdNextRow(struct annoStreamer *vSelf, struct lm *callerLm)
/* Perform sql query if we haven't already and return a single
 * annoRow, or NULL if there are no more items. */
{
struct annoStreamDb *self = (struct annoStreamDb *)vSelf;
if (self->sr == NULL)
    asdDoQuery(self);
if (self->sr == NULL)
    // This is necessary only if we're using sqlStoreResult in asdDoQuery, harmless otherwise:
    return NULL;
if (self->mergeBins)
    return nextRowMergeBins(self, callerLm);
boolean rightFail = FALSE;
char **row = nextRowFiltered(self, &rightFail);
if (row == NULL)
    return NULL;
return rowToAnnoRow(self, row, rightFail, callerLm);
}

static void asdClose(struct annoStreamer **pVSelf)
/* Close db connection and free self. */
{
if (pVSelf == NULL)
    return;
struct annoStreamDb *self = *(struct annoStreamDb **)pVSelf;
lmCleanup(&(self->qLm));
freeMem(self->table);
sqlFreeResult(&(self->sr));
hFreeConn(&(self->conn));
annoStreamerFree(pVSelf);
}

static boolean asdInitBed3Fields(struct annoStreamDb *self)
/* Use autoSql to figure out which table fields correspond to {chrom, chromStart, chromEnd}. */
{
struct annoStreamer *vSelf = &(self->streamer);
return annoStreamerFindBed3Columns(vSelf, &(self->chromIx), &(self->startIx), &(self->endIx),
				   &(self->chromField), &(self->startField), &(self->endField));
}

char *sqlTableIndexOnField(struct sqlConnection *conn, char *table, char *field)
/* If table has an index that includes field, return the index name, else NULL. */
{
char *indexName = NULL;
char query[512];
safef(query, sizeof(query), "show index from %s", table);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (sameString(row[4], field))
	{
	indexName = cloneString(row[2]);
	break;
	}
    }
sqlFreeResult(&sr);
return indexName;
}

struct annoStreamer *annoStreamDbNew(char *db, char *table, struct annoAssembly *aa,
				     struct asObject *asObj)
/* Create an annoStreamer (subclass) object from a database table described by asObj. */
{
struct sqlConnection *conn = hAllocConn(db);
if (!sqlTableExists(conn, table))
    errAbort("annoStreamDbNew: table '%s' doesn't exist in database '%s'", table, db);
struct annoStreamDb *self = NULL;
AllocVar(self);
struct annoStreamer *streamer = &(self->streamer);
annoStreamerInit(streamer, aa, asObj);
streamer->rowType = arWords;
streamer->setRegion = asdSetRegion;
streamer->nextRow = asdNextRow;
streamer->close = asdClose;
self->conn = conn;
self->table = cloneString(table);
char *asFirstColumnName = streamer->asObj->columnList->name;
if (sqlFieldIndex(self->conn, self->table, "bin") == 0)
    self->hasBin = 1;
if (self->hasBin && !sameString(asFirstColumnName, "bin"))
    self->omitBin = 1;
if (!asdInitBed3Fields(self))
    errAbort("annoStreamDbNew: can't figure out which fields of %s to use as "
	     "{chrom, chromStart, chromEnd}.", table);
// When a table has an index on endField, sometimes the query optimizer uses it
// and that ruins the sorting.  Fortunately most tables don't anymore.
self->endFieldIndexName = sqlTableIndexOnField(self->conn, self->table, self->endField);
self->notSorted = FALSE;
self->mergeBins = FALSE;
return (struct annoStreamer *)self;
}
