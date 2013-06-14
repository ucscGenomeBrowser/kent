/* annoStreamDb -- subclass of annoStreamer for database tables */

#include "annoStreamDb.h"
#include "annoGratorQuery.h"
#include "binRange.h"
#include "hdb.h"
#include "sqlNum.h"

struct annoStreamDb
    {
    struct annoStreamer streamer;	// Parent class members & methods
    // Private members
    struct sqlConnection *conn;		// Database connection (e.g. hg19 or customTrash)
    struct sqlResult *sr;		// SQL query result from which we grab rows
    char *table;			// Table name, must exist in database

    // These members enable us to extract coords from the otherwise unknown row:
    char *chromField;			// Name of chrom-ish column in table
    char *startField;			// Name of chromStart-ish column in table
    char *endField;			// Name of chromEnd-ish column in table
    int chromIx;			// Index of chrom-ish col in autoSql or bin-less table
    int startIx;			// Index of chromStart-ish col in autoSql or bin-less table
    int endIx;				// Index of chromEnd-ish col in autoSql or bin-less table

    // These members enable us to produce {chrom, start}-sorted output:
    char *endFieldIndexName;		// SQL index on end field, if any (can mess up sorting)
    boolean notSorted;			// TRUE if table is not sorted (e.g. genbank-updated)
    boolean hasBin;			// 1 if SQL table's first column is bin
    boolean omitBin;			// 1 if table hasBin and autoSql doesn't have bin
    boolean mergeBins;			// TRUE if query results will be in bin order
    struct annoRow *bigItemQueue;	// If mergeBins, accumulate coarse-bin items here
    struct annoRow *smallItemQueue;	// Max 1 item for merge-sorting with bigItemQueue
    struct lm *qLm;			// localmem for merge-sorting queues
    int minFinestBin;			// Smallest bin number for finest bin level
    boolean gotFinestBin;		// Flag that it's time to merge-sort with bigItemQueue

    // Limit (or not) number of rows processed:
    boolean useMaxOutRows;		// TRUE if maxOutRows passed to annoStreamDbNew is > 0
    int maxOutRows;			// Maximum number of rows we can output.

    // Process large tables in manageable chunks:
    struct slName *chromList;		// list of chromosomes/sequences in assembly
    struct slName *queryChrom;		// most recently queried chrom for whole-genome (or NULL)
    boolean eof;			// TRUE when we are done (maxItems or no more items)
    boolean needQuery;			// TRUE when we haven't yet queried, or need to query again
    boolean doNextChunk;		// TRUE if rowBuf ends before end of chrom/region
    uint nextChunkStart;		// Start coord for next chunk of rows to query

    struct rowBuf
    // Temporary storage for rows from chunked query
        {
	struct lm *lm;			// storage for rows
	char ***buf;			// array of pointers to rows
	int size;			// number of rows
	int ix;				// offset in buffer, [0..size]
        } rowBuf;

    char **(*nextRowRaw)(struct annoStreamDb *self, char *minChrom, uint minEnd);
    // Depending on query style, use either sqlNextRow or temporary row storage to get next row.

    void (*doQuery)(struct annoStreamDb *self, char *minChrom, uint minEnd);
    // Depending on query style, perform either a single query or (series of) chunked query
    };

// For performance reasons, even if !useMaxItems (no limit), we need to limit the
// number of rows that are returned from a query, so we can slurp them into memory and
// close the sqlResult before mysql gets unhappy about the result being open so long.
#define ASD_CHUNK_SIZE 100000

static void resetMergeState(struct annoStreamDb *self)
/* Reset or initialize members that track merging of coarse-bin items with fine-bin items. */
{
self->mergeBins = FALSE;
self->bigItemQueue = self->smallItemQueue = NULL;
lmCleanup(&(self->qLm));
self->gotFinestBin = FALSE;
}

static void resetRowBuf(struct rowBuf *rowBuf)
/* Reset temporary storage for chunked query rows. */
{
rowBuf->buf = NULL;
rowBuf->size = 0;
rowBuf->ix = 0;
lmCleanup(&(rowBuf->lm));
}

static void resetChunkState(struct annoStreamDb *self)
/* Reset members that track chunked queries. */
{
self->queryChrom = NULL;
self->eof = FALSE;
self->doNextChunk = FALSE;
self->needQuery = TRUE;
resetRowBuf(&self->rowBuf);
}

static void asdSetRegion(struct annoStreamer *vSelf, char *chrom, uint regionStart, uint regionEnd)
/* Set region -- and free current sqlResult if there is one. */
{
annoStreamerSetRegion(vSelf, chrom, regionStart, regionEnd);
struct annoStreamDb *self = (struct annoStreamDb *)vSelf;
sqlFreeResult(&(self->sr));
resetMergeState(self);
resetChunkState(self);
}

static char **nextRowFromSqlResult(struct annoStreamDb *self, char *minChrom, uint minEnd)
/* Stream rows directly from self->sr. */
{
return sqlNextRow(self->sr);
}

static void asdDoQuerySimple(struct annoStreamDb *self, char *minChrom, uint minEnd)
/* Return a sqlResult for a query on table items in position range.
 * If doing a whole genome query. just 'select * from' table. */
// NOTE: it would be possible to implement filters at this level, as in hgTables.
{
struct annoStreamer *streamer = &(self->streamer);
struct dyString *query = sqlDyStringCreate("select * from %s", self->table);
if (!streamer->positionIsGenome)
    {
    if (minChrom && differentString(minChrom, streamer->chrom))
	errAbort("annoStreamDb %s: nextRow minChrom='%s' but region chrom='%s'",
		 streamer->name, minChrom, streamer->chrom);
    if (self->hasBin)
	{
	// Results will be in bin order, but we can restore chromStart order by
	// accumulating initial coarse-bin items and merge-sorting them with
	// subsequent finest-bin items which will be in chromStart order.
	resetMergeState(self);
	self->mergeBins = TRUE;
	self->qLm = lmInit(0);
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
if (self->maxOutRows > 0)
    dyStringPrintf(query, " limit %d", self->maxOutRows);
struct sqlResult *sr = sqlGetResult(self->conn, query->string);
dyStringFree(&query);
self->sr = sr;
}

static void rowBufInit(struct rowBuf *rowBuf, int size)
/* Clean up rowBuf and give it a new lm and buffer[size]. */
{
resetRowBuf(rowBuf);
rowBuf->lm = lmInit(0);
rowBuf->size = size;
lmAllocArray(rowBuf->lm, rowBuf->buf, size);
}

static char **lmCloneRow(struct lm *lm, char **row, int colCount)
/* Use lm to allocate an array of strings and its contents copied from row. */
{
char **cloneRow = NULL;
lmAllocArray(lm, cloneRow, colCount);
int i;
for (i = 0;  i < colCount;  i++)
    cloneRow[i] = lmCloneString(lm, row[i]);
return cloneRow;
}

static void updateNextChunkState(struct annoStreamDb *self, int queryMaxItems)
/* If the just-fetched interval list was limited to ASD_CHUNK_SIZE, set doNextChunk
 * and trim the last row(s) so that when we query the next chunk, we don't get
 * repeat rows due to querying a start coord that was already returned. */
{
struct rowBuf *rowBuf = &self->rowBuf;
if (queryMaxItems == ASD_CHUNK_SIZE && rowBuf->size == ASD_CHUNK_SIZE)
    {
    self->doNextChunk = TRUE;
    // Starting at the last row in rowBuf, work back to find a value with a different start.
    int ix = rowBuf->size - 1;
    char **words = rowBuf->buf[ix];
    uint lastStart = atoll(words[self->startIx]);
    for (ix = rowBuf->size - 2;  ix >= 0;  ix--)
	{
	words = rowBuf->buf[ix];
	uint thisStart = atoll(words[self->startIx]);
	if (thisStart != lastStart)
	    {
	    rowBuf->size = ix+1;
	    self->nextChunkStart = lastStart;
	    break;
	    }
	}
    }
else
    self->doNextChunk = FALSE;
self->needQuery = FALSE;
}

static void bufferRowsFromSqlQuery(struct annoStreamDb *self, char *query, int queryMaxItems)
/* Store all rows from query in rowBuf. */
{
struct sqlResult *sr = sqlGetResult(self->conn, query);
struct rowBuf *rowBuf = &(self->rowBuf);
rowBufInit(rowBuf, ASD_CHUNK_SIZE);
struct annoStreamer *sSelf = &(self->streamer);
char **row = NULL;
int ix = 0;
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (ix >= rowBuf->size)
	errAbort("annoStreamDb %s: rowBuf overflow, got more than %d rows",
		 sSelf->name, rowBuf->size);
    rowBuf->buf[ix++] = lmCloneRow(rowBuf->lm, row, sSelf->numCols+self->omitBin);
    }
// Set rowBuf->size to the number of rows we actually stored.
rowBuf->size = ix;
sqlFreeResult(&sr);
updateNextChunkState(self, queryMaxItems);
}

static void asdDoQueryChunking(struct annoStreamDb *self, char *minChrom, uint minEnd)
/* Return a sqlResult for a query on table items in position range.
 * If doing a whole genome query. just 'select * from' table. */
{
struct annoStreamer *sSelf = &(self->streamer);
struct dyString *query = sqlDyStringCreate("select * from %s ", self->table);
if (sSelf->chrom != NULL && self->rowBuf.size > 0 && !self->doNextChunk)
    // We're doing a region query, we already got some rows, and don't need another chunk:
    self->eof = TRUE;
if (self->useMaxOutRows)
    {
    self->maxOutRows -= self->rowBuf.size;
    if (self->maxOutRows <= 0)
	self->eof = TRUE;
    }
if (self->eof)
    return;
int queryMaxItems = ASD_CHUNK_SIZE;
if (self->useMaxOutRows && self->maxOutRows < queryMaxItems)
    queryMaxItems = self->maxOutRows;
if (self->hasBin)
    {
    // Results will be in bin order, but we can restore chromStart order by
    // accumulating initial coarse-bin items and merge-sorting them with
    // subsequent finest-bin items which will be in chromStart order.
    resetMergeState(self);
    self->mergeBins = TRUE;
    self->qLm = lmInit(0);
    }
if (self->endFieldIndexName != NULL)
    // Don't let mysql use a (chrom, chromEnd) index because that messes up
    // sorting by chromStart.
    dyStringPrintf(query, "IGNORE INDEX (%s) ", self->endFieldIndexName);
if (sSelf->chrom != NULL)
    {
    uint start = sSelf->regionStart;
    if (minChrom)
	{
	if (differentString(minChrom, sSelf->chrom))
	    errAbort("annoStreamDb %s: nextRow minChrom='%s' but region chrom='%s'",
		     sSelf->name, minChrom, sSelf->chrom);
	if (start < minEnd)
	    start = minEnd;
	}
    if (self->doNextChunk && start < self->nextChunkStart)
	start = self->nextChunkStart;
    dyStringPrintf(query, "where %s = '%s' and ", self->chromField, sSelf->chrom);
    if (self->hasBin)
	hAddBinToQuery(start, sSelf->regionEnd, query);
    if (self->doNextChunk)
	dyStringPrintf(query, "%s >= %u and ", self->startField, self->nextChunkStart);
    dyStringPrintf(query, "%s < %u and %s > %u limit %d", self->startField, sSelf->regionEnd,
		   self->endField, start, queryMaxItems);
    bufferRowsFromSqlQuery(self, query->string, queryMaxItems);
    }
else
    {
    // Genome-wide query: break it into chrom-by-chrom queries.
    if (self->queryChrom == NULL)
	self->queryChrom = self->chromList;
    else if (!self->doNextChunk)
	self->queryChrom = self->queryChrom->next;
    if (minChrom != NULL)
	{
	// Skip chroms that precede minChrom
	while (self->queryChrom != NULL && strcmp(self->queryChrom->name, minChrom) < 0)
	    {
	    self->queryChrom = self->queryChrom->next;
	    self->doNextChunk = FALSE;
	    }
	}
    if (self->queryChrom == NULL)
	self->eof = TRUE;
    else
	{
	char *chrom = self->queryChrom->name;
	int start = 0;
	if (minChrom != NULL && sameString(chrom, minChrom))
	    start = minEnd;
	if (self->doNextChunk && start < self->nextChunkStart)
	    start = self->nextChunkStart;
	uint end = annoAssemblySeqSize(self->streamer.assembly, self->queryChrom->name);
	dyStringPrintf(query, "where %s = '%s' ", self->chromField, chrom);
	if (start > 0 || self->doNextChunk)
	    {
	    dyStringAppend(query, "and ");
	    if (self->hasBin)
		hAddBinToQuery(start, end, query);
	    if (self->doNextChunk)
		dyStringPrintf(query, "%s >= %u and ", self->startField, self->nextChunkStart);
	    // region end is chromSize, so no need to constrain startField here:
	    dyStringPrintf(query, "%s > %u ", self->endField, start);
	    }
	dyStringPrintf(query, "limit %d", queryMaxItems);
	bufferRowsFromSqlQuery(self, query->string, queryMaxItems);
	// If there happens to be no items on chrom, try again with the next chrom:
	if (! self->eof && self->rowBuf.size == 0)
	    asdDoQueryChunking(self, minChrom, minEnd);
	}
    }
dyStringFree(&query);
}

static char **nextRowFromBuffer(struct annoStreamDb *self, char *minChrom, uint minEnd)
/* Instead of streaming directly from self->sr, we have buffered up the results
 * of a chunked query; return the head of that queue. */
{
struct rowBuf *rowBuf = &self->rowBuf;
if (rowBuf->ix > rowBuf->size)
    errAbort("annoStreamDb %s: rowBuf overflow (%d > %d)", self->streamer.name,
	     rowBuf->ix, rowBuf->size);
if (rowBuf->ix == rowBuf->size)
    // Last row in buffer -- we'll need another query to get subsequent rows (if any).
    asdDoQueryChunking(self, minChrom, minEnd);
return rowBuf->buf[rowBuf->ix++];
}

static char **nextRowFiltered(struct annoStreamDb *self, boolean *retRightFail,
			      char *minChrom, uint minEnd)
/* Skip past any left-join failures until we get a right-join failure, a passing row,
 * or end of data.  Return row or NULL, and return right-join fail status via retRightFail. */
{
int numCols = self->streamer.numCols;
char **row = self->nextRowRaw(self, minChrom, minEnd);
if (minChrom != NULL && row != NULL)
    {
    // Ignore rows that fall completely before (minChrom, minEnd) - save annoGrator's time
    int chromIx = self->omitBin+self->chromIx;
    int endIx = self->omitBin+self->endIx;
    int chromCmp;
    while (row &&
	   ((chromCmp = strcmp(row[chromIx], minChrom)) < 0 || // this chrom precedes minChrom
	    (chromCmp == 0 && atoll(row[endIx]) < minEnd)))    // on minChrom, but before minEnd
	row = self->nextRowRaw(self, minChrom, minEnd);
    }
boolean rightFail = FALSE;
struct annoFilter *filterList = self->streamer.filters;
while (row && annoFilterRowFails(filterList, row+self->omitBin, numCols, &rightFail))
    {
    if (rightFail)
	break;
    row = self->nextRowRaw(self, minChrom, minEnd);
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

static char **getFinestBinItem(struct annoStreamDb *self, char **row, boolean *pRightFail,
			       char *minChrom, uint minEnd)
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
    row = nextRowFiltered(self, pRightFail, minChrom, minEnd);
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

static struct annoRow *nextRowMergeBins(struct annoStreamDb *self, char *minChrom, uint minEnd,
					struct lm *callerLm)
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
    char **row = nextRowFiltered(self, &rightFail, minChrom, minEnd);
    if (row && !self->gotFinestBin)
	{
	// We are just starting -- queue up coarse-bin items, if any, until we get the first
	// finest-bin item.
	row = getFinestBinItem(self, row, &rightFail, minChrom, minEnd);
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

static struct annoRow *asdNextRow(struct annoStreamer *vSelf, char *minChrom, uint minEnd,
				  struct lm *callerLm)
/* Perform sql query if we haven't already and return a single
 * annoRow, or NULL if there are no more items. */
{
struct annoStreamDb *self = (struct annoStreamDb *)vSelf;
if (self->needQuery)
    self->doQuery(self, minChrom, minEnd);
if (self->mergeBins)
    return nextRowMergeBins(self, minChrom, minEnd, callerLm);
boolean rightFail = FALSE;
char **row = nextRowFiltered(self, &rightFail, minChrom, minEnd);
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
sqlSafef(query, sizeof(query), "show index from %s", table);
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
				     struct asObject *asObj, int maxOutRows)
/* Create an annoStreamer (subclass) object from a database table described by asObj. */
{
struct sqlConnection *conn = hAllocConn(db);
if (!sqlTableExists(conn, table))
    errAbort("annoStreamDbNew: table '%s' doesn't exist in database '%s'", table, db);
struct annoStreamDb *self = NULL;
AllocVar(self);
struct annoStreamer *streamer = &(self->streamer);
int dbtLen = strlen(db) + strlen(table) + 2;
char dbTable[dbtLen];
safef(dbTable, dbtLen, "%s.%s", db, table);
annoStreamerInit(streamer, aa, asObj, dbTable);
streamer->rowType = arWords;
streamer->setRegion = asdSetRegion;
streamer->nextRow = asdNextRow;
streamer->close = asdClose;
self->conn = conn;
self->table = cloneString(table);
char *asFirstColumnName = streamer->asObj->columnList->name;
if (sqlFieldIndex(self->conn, self->table, "bin") == 0)
    {
    self->hasBin = 1;
    self->minFinestBin = binFromRange(0, 1);
    }
if (self->hasBin && !sameString(asFirstColumnName, "bin"))
    self->omitBin = 1;
if (!asdInitBed3Fields(self))
    errAbort("annoStreamDbNew: can't figure out which fields of %s.%s to use as "
	     "{chrom, chromStart, chromEnd}.", db, table);
// When a table has an index on endField, sometimes the query optimizer uses it
// and that ruins the sorting.  Fortunately most tables don't anymore.
self->endFieldIndexName = sqlTableIndexOnField(self->conn, self->table, self->endField);
self->notSorted = FALSE;
self->mergeBins = FALSE;
self->maxOutRows = maxOutRows;
self->useMaxOutRows = (maxOutRows > 0);
self->needQuery = TRUE;
self->chromList = annoAssemblySeqNames(aa);
if (slCount(self->chromList) > 1000)
    {
    // Assembly has many sequences (e.g. scaffold-based assembly) --
    // don't break up into per-sequence queries.  Take our chances
    // with mysql being unhappy about the sqlResult being open too long.
    self->doQuery = asdDoQuerySimple;
    self->nextRowRaw = nextRowFromSqlResult;
    }
else
    {
    // All-chromosome assembly -- if table is large, perform a series of
    // chunked queries.
    self->doQuery = asdDoQueryChunking;
    self->nextRowRaw = nextRowFromBuffer;
    }
return (struct annoStreamer *)self;
}
