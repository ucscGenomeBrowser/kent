/* annoStreamDb -- subclass of annoStreamer for database tables */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "annoStreamDb.h"
#include "annoGratorQuery.h"
#include "binRange.h"
#include "hAnno.h"
#include "joinMixer.h"
#include "hdb.h"
#include "obscure.h"
#include "sqlNum.h"

struct annoStreamDb
    {
    struct annoStreamer streamer;	// Parent class members & methods
    // Private members
    char *db;                           // Database name (e.g. hg19 or customTrash)
    struct sqlConnection *conn;	        // Database connection
    struct sqlResult *sr;		// SQL query result from which we grab rows
    char *trackTable;			// Name of database table (or root name if split tables)
    char *table;			// If split, chr..._trackTable; otherwise same as trackTable
    char *baselineQuery;                // SQL query without position constraints
    boolean baselineQueryHasWhere;      // True if baselineQuery contains filter or join clauses

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

    // Info for joining in related tables/fields
    struct joinerDtf *mainTableDtfList; // Fields from the main table to include in output
    struct joinerDtf *relatedDtfList;	// Fields from related tables to include in output
    struct joiner *joiner;		// Parsed all.joiner schema
    struct joinMixer *joinMixer;	// Plan for joining related tables using sql and/or hash
					// (NULL if no joining is necessary)
    uint sqlRowSize;			// Number of columns from sql query (may include related)
    uint bigRowSize;			// Number of columns from sql + joinMixer->hashJoins
    boolean hasLeftJoin;		// If we have to use 'left join' we'll have to 'order by'.
    boolean naForMissing;		// If true, insert "n/a" for missing related table values
					// to match hgTables.

    struct rowBuf
    // Temporary storage for rows from chunked query
        {
	struct lm *lm;			// storage for rows
	char ***buf;			// array of pointers to rows
	int size;			// number of rows
	int ix;				// offset in buffer, [0..size]
        } rowBuf;

    char **(*nextRowRaw)(struct annoStreamDb *self);
    // Depending on query style, use either sqlNextRow or temporary row storage to get next row.
    // This may return NULL but set self->needQuery; asdNextRow watches for that.

    void (*doQuery)(struct annoStreamDb *self, char *minChrom, uint minEnd);
    // Depending on query style, perform either a single query or (series of) chunked query
    };

//#*** TODO: make a way to pass the filter with dtf into annoStreamDb.

struct annoFilterDb
    // annoFilter has columnIx which works fine for all fields of main table,
    // but for joining filters we will need dtf.
    {
    struct annoFilter filter;            // parent class
    struct joinerDtf *dtf;               // {database, table, field} in case this is from
                                         // some table to be joined with the main table
    };

// For performance reasons, even if !useMaxItems (no limit), we need to limit the
// number of rows that are returned from a query, so we can slurp them into memory and
// close the sqlResult before mysql gets unhappy about the result being open so long.
#define ASD_CHUNK_SIZE 100000

#define JOINER_FILE "all.joiner"

static const boolean asdDebug = FALSE;

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

static void startMerging(struct annoStreamDb *self)
/* Set self->mergeBins flag and create self->qLm if necessary. */
{
self->mergeBins = TRUE;
self->gotFinestBin = FALSE;
if (self->qLm == NULL)
    self->qLm = lmInit(0);
}

static void resetQueryState(struct annoStreamDb *self)
/* Free sqlResult if there is one, and reset state associated with the current query. */
{
sqlFreeResult(&(self->sr));
resetMergeState(self);
resetChunkState(self);
}

// Forward declaration in order to avoid moving lots of code:
static void asdUpdateBaselineQuery(struct annoStreamDb *self);
/* Build a dy SQL query with no position constraints (select ... from ...)
 * possibly including joins and filters if specified (where ...), using the current splitTable. */

static void asdSetRegion(struct annoStreamer *vSelf, char *chrom, uint regionStart, uint regionEnd)
/* Set region -- and free current sqlResult if there is one. */
{
annoStreamerSetRegion(vSelf, chrom, regionStart, regionEnd);
struct annoStreamDb *self = (struct annoStreamDb *)vSelf;
// If splitTable differs from table, use new chrom in splitTable:
if (differentString(self->table, self->trackTable))
    {
    char newSplitTable[PATH_LEN];
    safef(newSplitTable, sizeof(newSplitTable), "%s_%s", chrom, self->trackTable);
    freeMem(self->table);
    self->table = cloneString(newSplitTable);
    }
resetQueryState(self);
asdUpdateBaselineQuery(self);
}

static char **nextRowFromSqlResult(struct annoStreamDb *self)
/* Stream rows directly from self->sr. */
{
return sqlNextRow(self->sr);
}

INLINE boolean useSplitTable(struct annoStreamDb *self, struct joinerDtf *dtf)
/* Return TRUE if dtf matches self->{db,table} and table is split. */
{
return (sameString(dtf->database, self->db) &&
        sameString(dtf->table, self->trackTable) &&
        differentString(self->table, self->trackTable));
}

static void appendFieldList(struct annoStreamDb *self, struct dyString *query)
/* Append SQL field list to query. */
{
struct joinerDtf *fieldList = self->joinMixer ? self->joinMixer->sqlFieldList :
                                                self->mainTableDtfList;
struct joinerDtf *dtf;
for (dtf = fieldList;  dtf != NULL;  dtf = dtf->next)
    {
    if (dtf != fieldList)
        sqlDyStringPrintf(query, ",");
    if (useSplitTable(self, dtf))
        sqlDyStringPrintf(query, "%s.%s", self->table, dtf->field);
    else
        {
        char dtfString[PATH_LEN];
        joinerDtfToSqlFieldString(dtf, self->db, dtfString, sizeof(dtfString));
        sqlDyStringPrintf(query, "%s", dtfString);
        }
    }
}

static void ignoreEndIndexIfNecessary(struct annoStreamDb *self, char *dbTable,
                                      struct dyString *query)
/* Don't let mysql use a (chrom, chromEnd) index because that messes up sorting by chromStart. */
{
if (sameString(dbTable, self->trackTable) && self->endFieldIndexName != NULL)
    sqlDyStringPrintf(query, " IGNORE INDEX (%s) ", self->endFieldIndexName);
}

static void appendOneTable(struct annoStreamDb *self, struct joinerDtf *dt, struct dyString *query)
/* Add the (db.)table string from dt to query; if dt is NULL or table is split then
 * use self->table. */
{
char dbTable[PATH_LEN];
if (dt == NULL || useSplitTable(self, dt))
    safecpy(dbTable, sizeof(dbTable), self->table);
else
    joinerDtfToSqlTableString(dt, self->db, dbTable, sizeof(dbTable));
dyStringAppend(query, dbTable);
ignoreEndIndexIfNecessary(self, dbTable, query);
}

INLINE void splitOrDtfToSqlField(struct annoStreamDb *self, struct joinerDtf *dtf,
                                 char *fieldName, size_t fieldNameSize)
/* Write [db].table.field into fieldName, where table may be split. */
{
if (useSplitTable(self, dtf))
    safef(fieldName, fieldNameSize, "%s.%s", self->table, dtf->field);
else
    joinerDtfToSqlFieldString(dtf, self->db, fieldName, fieldNameSize);
}

static void appendJoin(struct annoStreamDb *self, struct joinerPair *routeList,
                       struct dyString *query)
/* Append join statement(s) for a possibly tree-structured routeList. */
{
struct joinerPair *jp;
for (jp = routeList;  jp != NULL;  jp = jp->next)
    {
    struct joinerField *jfB = joinerSetFindField(jp->identifier, jp->b);
    if (! jfB->full)
        dyStringAppend(query, " left");
    dyStringAppend(query, " join ");
    if (jp->child)
        {
        dyStringAppendC(query, '(');
        appendOneTable(self, jp->child->a, query);
        appendJoin(self, jp->child, query);
        dyStringAppendC(query, ')');
        }
    else
        appendOneTable(self, jp->b, query);
    char fieldA[PATH_LEN], fieldB[PATH_LEN];
    splitOrDtfToSqlField(self, jp->a, fieldA, sizeof(fieldA));
    splitOrDtfToSqlField(self, jp->b, fieldB, sizeof(fieldB));
    struct joinerField *jfA = joinerSetFindField(jp->identifier, jp->a);
    if (sameOk(jfA->separator, ","))
        dyStringPrintf(query, " on find_in_set(%s, %s)", fieldB, fieldA);
    else
        dyStringPrintf(query, " on %s = %s", fieldA, fieldB);
    }
}

static boolean appendTableList(struct annoStreamDb *self, struct dyString *query)
/* Append SQL table list to query, including tables used for output, filtering and joining. */
{
boolean hasLeftJoin = FALSE;
if (self->joinMixer == NULL || self->joinMixer->sqlRouteList == NULL)
    appendOneTable(self, NULL, query);
else
    {
    // Use both a and b of the first pair and only b of each subsequent pair
    appendOneTable(self, self->joinMixer->sqlRouteList->a, query);
    appendJoin(self, self->joinMixer->sqlRouteList, query);
    hasLeftJoin = TRUE;
    }
return hasLeftJoin;
}

// libify?
static struct joinerDtf *joinerDtfCloneList(struct joinerDtf *listIn)
/* Return a list with cloned items of listIn. */
{
struct joinerDtf *listOut = NULL, *item;
for (item = listIn;  item != NULL;  item = item->next)
    slAddHead(&listOut, joinerDtfClone(item));
slReverse(&listOut);
return listOut;
}

static char *joinerFilePath()
/* Return the location of all.joiner - default is ./all.joiner but a different file
 * can be substituted using environment variable ALL_JOINER_FILE */
{
char *joinerFile = getenv("ALL_JOINER_FILE");
if (isEmpty(joinerFile))
    joinerFile = JOINER_FILE;
return joinerFile;
}

static void asdInitBaselineQuery(struct annoStreamDb *self)
/* Build a dy SQL query with no position constraints (select ... from ...)
 * possibly including joins and filters if specified (where ...). */
{
if (self->relatedDtfList)
    {
    struct joinerDtf *outputFieldList = slCat(joinerDtfCloneList(self->mainTableDtfList),
                                              joinerDtfCloneList(self->relatedDtfList));
    if (self->joiner == NULL)
        self->joiner = joinerRead(joinerFilePath());
    int expectedRows = sqlRowCount(self->conn, self->table);
    self->joinMixer = joinMixerNew(self->joiner, self->db, self->table, outputFieldList,
                                   expectedRows, self->naForMissing);
    joinerPairListToTree(self->joinMixer->sqlRouteList);
    self->sqlRowSize = slCount(self->joinMixer->sqlFieldList);
    self->bigRowSize = self->joinMixer->bigRowSize;
    joinerDtfFreeList(&outputFieldList);
    }
else
    {
    self->sqlRowSize = slCount(self->mainTableDtfList);
    self->bigRowSize = self->sqlRowSize;
    }
}

static void asdUpdateBaselineQuery(struct annoStreamDb *self)
/* Build a dy SQL query with no position constraints (select ... from ...)
 * possibly including joins and filters if specified (where ...), using the current splitTable. */
{
struct dyString *query = sqlDyStringCreate("select ");
appendFieldList(self, query);
sqlDyStringPrintf(query, " from ");
self->hasLeftJoin = appendTableList(self, query);
boolean hasWhere = FALSE;
self->baselineQuery = dyStringCannibalize(&query);
self->baselineQueryHasWhere = hasWhere;
// Don't free joiner; we need its storage of joinerFields.
}

static void addBinToQuery(struct annoStreamDb *self, uint start, uint end, struct dyString *query)
/* If applicable, add bin range constraints to query with explicit table name, in case we're
 * joining with another table that has a bin column. */
{
if (self->hasBin)
    {
    // Get the bin constraints with no table specification:
    struct dyString *binConstraints = dyStringNew(0);
    hAddBinToQuery(start, end, binConstraints);
    // Swap in explicit table name for bin field:
    char tableDotBin[PATH_LEN];
    safef(tableDotBin, sizeof(tableDotBin), "%s.bin", self->table);
    struct dyString *explicitBinConstraints = dyStringSub(binConstraints->string,
                                                          "bin", tableDotBin);
    dyStringAppend(query, explicitBinConstraints->string);
    dyStringFree(&explicitBinConstraints);
    dyStringFree(&binConstraints);
    }
}

static void addRangeToQuery(struct annoStreamDb *self, struct dyString *query,
                            char *chrom, uint start, uint end, boolean hasWhere)
/* Add position constraints to query. */
{
if (hasWhere)
    sqlDyStringPrintf(query, " and ");
else
    sqlDyStringPrintf(query, " where ");
sqlDyStringPrintf(query, "%s.%s='%s'", self->table, self->chromField, chrom);
uint chromSize = annoAssemblySeqSize(self->streamer.assembly, chrom);
boolean addStartConstraint = (start > 0);
boolean addEndConstraint = (end < chromSize);
if (addStartConstraint || addEndConstraint)
    {
    sqlDyStringPrintf(query, "and ");
    if (self->hasBin)
        addBinToQuery(self, start, end, query);
    if (addStartConstraint)
        {
        if (self->doNextChunk)
            sqlDyStringPrintf(query, "%s.%s >= %u ", self->table, self->startField, start);
        else
            // Make sure to include insertions at start:
            sqlDyStringPrintf(query, "(%s.%s > %u or (%s.%s = %s.%s and %s.%s = %u)) ",
                              self->table, self->endField, start,
                              self->table, self->endField, self->table, self->startField,
                              self->table, self->startField, start);
        }
    if (addEndConstraint)
        {
        if (addStartConstraint)
            sqlDyStringPrintf(query, "and ");
        // Make sure to include insertions at end:
        sqlDyStringPrintf(query, "(%s.%s < %u or (%s.%s = %s.%s and %s.%s = %u)) ",
                          self->table, self->startField, end,
                          self->table, self->startField, self->table, self->endField,
                          self->table, self->endField, end);
        }
    }
}

static void asdDoQuerySimple(struct annoStreamDb *self, char *minChrom, uint minEnd)
/* Return a sqlResult for a query on table items in position range.
 * If doing a whole genome query. just select all rows from table. */
// NOTE: it would be possible to implement filters at this level, as in hgTables.
{
struct annoStreamer *streamer = &(self->streamer);
boolean hasWhere = self->baselineQueryHasWhere;
struct dyString *query = dyStringCreate("%s", self->baselineQuery);
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
        startMerging(self);
	}
    addRangeToQuery(self, query, streamer->chrom, streamer->regionStart, streamer->regionEnd,
                    hasWhere);
    if (self->notSorted || self->hasLeftJoin)
	sqlDyStringPrintf(query, " order by %s.%s", self->table, self->startField);
    }
else if (self->notSorted || self->hasLeftJoin)
    sqlDyStringPrintf(query, " order by %s.%s,%s.%s",
                      self->table, self->chromField, self->table, self->startField);
if (self->maxOutRows > 0)
    dyStringPrintf(query, " limit %d", self->maxOutRows);
struct sqlResult *sr = sqlGetResult(self->conn, query->string);
dyStringFree(&query);
self->sr = sr;
self->needQuery = FALSE;
}

static void rowBufInit(struct rowBuf *rowBuf, int size)
/* Clean up rowBuf and give it a new lm and buffer[size]. */
{
resetRowBuf(rowBuf);
rowBuf->lm = lmInit(0);
rowBuf->size = size;
lmAllocArray(rowBuf->lm, rowBuf->buf, size);
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
    int startIx = self->startIx + self->omitBin;
    uint lastStart = atoll(words[startIx]);
    for (ix = rowBuf->size - 2;  ix >= 0;  ix--)
	{
	words = rowBuf->buf[ix];
	uint thisStart = atoll(words[startIx]);
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

static boolean glomSqlDup(char **oldRow, char **newRow, int mainColCount, int sqlColCount,
                          struct lm *lm)
/* If newRow's contents are identical to oldRow's for all fields from the main table,
 * then comma-glom new values of joined related fields onto oldRow's values and return TRUE;
 * otherwise leave oldRow alone and return FALSE. */
{
boolean isDup = TRUE;
int i;
for (i = 0;  i < mainColCount;  i++)
    if (differentStringNullOk(oldRow[i], newRow[i]))
        {
        isDup = FALSE;
        break;
        }
if (isDup)
    {
    // Glom related column values produced by mysql, collapsing consecutive duplicate values
    // and appending comma to match hgTables
    for (i = mainColCount;  i < sqlColCount;  i++)
        {
        char *oldVal = oldRow[i];
        char *newVal = newRow[i];
        if (newVal != NULL)
            {
            int newValLen = strlen(newVal);
            char newValComma[newValLen+2];
            safef(newValComma, sizeof(newValComma), "%s,", newVal);
            if (oldVal != NULL)
                {
                int oldValLen = strlen(oldVal);
                if (! (endsWithWordComma(oldVal, newVal)))
                    {
                    char *comma = (oldVal[oldValLen-1] == ',') ? "" : ",";
                    int glommedSize = oldValLen + 1 + newValLen + 2;
                    char *glommedVal = lmAlloc(lm, glommedSize);
                    safef(glommedVal, glommedSize, "%s%s%s", oldVal, comma, newValComma);
                    oldRow[i] = glommedVal;
                    }
                }
            else
                {
                oldRow[i] = lmCloneString(lm, newValComma);
                }
            }
        }
    }
return isDup;
}

static void bufferRowsFromSqlQuery(struct annoStreamDb *self, char *query, int queryMaxItems)
/* Store all rows from query in rowBuf. */
{
struct sqlResult *sr = sqlGetResult(self->conn, query);
struct rowBuf *rowBuf = &(self->rowBuf);
rowBufInit(rowBuf, ASD_CHUNK_SIZE);
struct annoStreamer *sSelf = &(self->streamer);
boolean didSqlJoin = (self->joinMixer && self->joinMixer->sqlRouteList);
int mainColCount = slCount(self->mainTableDtfList);
int sqlColCount = self->sqlRowSize;
char **row = NULL;
int ix = 0;
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (ix >= rowBuf->size)
	errAbort("annoStreamDb %s: rowBuf overflow, got more than %d rows",
		 sSelf->name, rowBuf->size);
    // SQL join outputs separate rows for multiple matches. Accumulate multiple matches as
    // comma-sep lists to match hgTables and hashJoin (and prevent rowBuf overflow).
    boolean didGlom = FALSE;
    if (ix != 0 && didSqlJoin)
        didGlom = glomSqlDup(rowBuf->buf[ix-1], row, mainColCount, sqlColCount, rowBuf->lm);
    if (! didGlom)
        rowBuf->buf[ix++] = lmCloneRowExt(rowBuf->lm, row, self->bigRowSize, self->sqlRowSize);
    }
// Set rowBuf->size to the number of rows we actually stored.
rowBuf->size = ix;
sqlFreeResult(&sr);
updateNextChunkState(self, queryMaxItems);
}

static void updateQueryChrom(struct annoStreamDb *self, char *minChrom)
/* Figure out whether we need to query the next chunk on the current chromosome
 * or move on to the next chromosome. */
{
if (self->queryChrom == NULL)
    self->queryChrom = self->chromList;
else if (!self->doNextChunk)
    {
    self->queryChrom = self->queryChrom->next;
    if (self->hasBin)
        {
        resetMergeState(self);
        startMerging(self);
        }
    }
// -- don't resetMergeState if doNextChunk.
if (minChrom != NULL)
    {
    // Skip chroms that precede minChrom
    while (self->queryChrom != NULL && strcmp(self->queryChrom->name, minChrom) < 0)
        {
        self->queryChrom = self->queryChrom->next;
        self->doNextChunk = FALSE;
        }
    if (self->hasBin)
        {
        resetMergeState(self);
        startMerging(self);
        }
    }
}

static void doOneChunkQuery(struct annoStreamDb *self, struct dyString *query,
                         char *chrom, uint start, uint end,
                         boolean hasWhere, int maxItems)
/* Add range constraints to query, perform query and buffer the results. */
{
addRangeToQuery(self, query, chrom, start, end, hasWhere);
if (self->notSorted || self->hasLeftJoin)
    sqlDyStringPrintf(query, " order by %s.%s", self->table, self->startField);
sqlDyStringPrintf(query, " limit %d", maxItems);
bufferRowsFromSqlQuery(self, query->string, maxItems);
}

static void asdDoQueryChunking(struct annoStreamDb *self, char *minChrom, uint minEnd)
/* Get rows from mysql with a limit on the number of rows returned at one time (ASD_CHUNK_SIZE),
 * to avoid long delays for very large tables.  This will be called multiple times if
 * the number of rows in region is more than ASD_CHUNK_SIZE.  If doing a genome-wide query,
 * break it up into chrom-by-chrom queries because the code that merges large bin items
 * in with small bin items assumes that all rows are on the same chrom. */
{
struct annoStreamer *sSelf = &(self->streamer);
boolean hasWhere = self->baselineQueryHasWhere;
struct dyString *query = dyStringCreate("%s", self->baselineQuery);
if (sSelf->chrom != NULL && self->rowBuf.size > 0 && !self->doNextChunk)
    {
    // We're doing a region query, we already got some rows, and don't need another chunk:
    resetRowBuf(&self->rowBuf);
    self->eof = TRUE;
    }
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
    if (self->doNextChunk && self->mergeBins && !self->gotFinestBin)
	errAbort("annoStreamDb %s: can't continue merge in chunking query; "
		 "increase ASD_CHUNK_SIZE", sSelf->name);
    // Don't reset merge state here in case bigItemQueue has a large-bin item
    // at the end of the chrom, past all smallest-bin items.
    startMerging(self);
    }
if (sSelf->chrom != NULL)
    {
    // Region query (but might end up as multiple chunked queries)
    char *chrom = sSelf->chrom;
    uint start = sSelf->regionStart;
    uint end = sSelf->regionEnd;
    if (minChrom)
	{
	if (differentString(minChrom, chrom))
	    errAbort("annoStreamDb %s: nextRow minChrom='%s' but region chrom='%s'",
		     sSelf->name, minChrom, chrom);
	if (start < minEnd)
	    start = minEnd;
	}
    if (self->doNextChunk && start < self->nextChunkStart)
	start = self->nextChunkStart;
    doOneChunkQuery(self, query, chrom, start, end, hasWhere, queryMaxItems);
    if (self->rowBuf.size == 0)
	self->eof = TRUE;
    }
else
    {
    // Genome-wide query: break it into chrom-by-chrom queries (that might be chunked)
    // because the mergeBins stuff assumes that all rows are from the same chrom.
    updateQueryChrom(self, minChrom);
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
        uint end = annoAssemblySeqSize(self->streamer.assembly, chrom);
        doOneChunkQuery(self, query, chrom, start, end, hasWhere, queryMaxItems);
	// If there happens to be no items on chrom, try again with the next chrom:
	if (! self->eof && self->rowBuf.size == 0)
	    asdDoQueryChunking(self, minChrom, minEnd);
	}
    }
dyStringFree(&query);
}

static char **nextRowFromBuffer(struct annoStreamDb *self)
/* Instead of streaming directly from self->sr, we have buffered up the results
 * of a chunked query; return the head of that queue. */
{
struct rowBuf *rowBuf = &self->rowBuf;
if (rowBuf->ix > rowBuf->size)
    errAbort("annoStreamDb %s: rowBuf overflow (%d > %d)", self->streamer.name,
	     rowBuf->ix, rowBuf->size);
if (rowBuf->ix == rowBuf->size)
    {
    // Last row in buffer -- we'll need another query to get subsequent rows (if any).
    // But first, see if we need to update gotFinestBin, since getFinestBin might be
    // one of our callers.
    if (rowBuf->size > 0)
	{
	char **lastRow = rowBuf->buf[rowBuf->size-1];
	int lastBin = atoi(lastRow[0]);
	if (lastBin >= self->minFinestBin)
	    self->gotFinestBin = TRUE;
	}
    if (self->bigItemQueue == NULL && self->smallItemQueue == NULL)
        self->needQuery = TRUE;
    // Bounce back out -- asdNextRow or nextRowMergeBins will need to do another query.
    return NULL;
    }
if (rowBuf->size == 0)
    return NULL;
else
    return rowBuf->buf[rowBuf->ix++];
}

static char **nextRowUnfiltered(struct annoStreamDb *self)
/* Call self->nextRowRaw to get the next row from the sql query on the main table.
 * Then, if applicable, add columns added from hashes of related tables. */
{
char **row = self->nextRowRaw(self);
if (self->joinMixer && self->joinMixer->hashJoins && row)
    {
    // Add columns from hashedJoins to row
    struct hashJoin *hj;
    for (hj = self->joinMixer->hashJoins;  hj != NULL;  hj = hashJoinNext(hj))
        hashJoinOneRow(hj, row);
    }
return row;
}

static char **nextRowFiltered(struct annoStreamDb *self, boolean *retRightFail,
			      char *minChrom, uint minEnd)
/* Skip past any left-join failures until we get a right-join failure, a passing row,
 * or end of data.  Return row or NULL, and return right-join fail status via retRightFail. */
{
int numCols = self->streamer.numCols;
char **row = nextRowUnfiltered(self);
if (minChrom != NULL && row != NULL)
    {
    // Ignore rows that fall completely before (minChrom, minEnd) - save annoGrator's time
    int chromIx = self->omitBin+self->chromIx;
    int endIx = self->omitBin+self->endIx;
    int chromCmp;
    while (row &&
	   ((chromCmp = strcmp(row[chromIx], minChrom)) < 0 || // this chrom precedes minChrom
	    (chromCmp == 0 && atoll(row[endIx]) < minEnd)))    // on minChrom, but before minEnd
	row = nextRowUnfiltered(self);
    }
boolean rightFail = FALSE;
struct annoFilter *filterList = self->streamer.filters;
while (row && annoFilterRowFails(filterList, row+self->omitBin, numCols, &rightFail))
    {
    if (rightFail)
	break;
    row = nextRowUnfiltered(self);
    }
*retRightFail = rightFail;
return row;
}

static struct annoRow *rowToAnnoRow(struct annoStreamDb *self, char **row, boolean rightFail,
				    struct lm *lm)
/* Extract coords from row and return an annoRow including right-fail status. */
{
char **finalRow = row + self->omitBin;
uint numCols = self->streamer.numCols;
char *swizzleRow[numCols];
if (self->joinMixer)
    {
    uint i;
    for (i = 0;  i < numCols;  i++)
        {
        uint outIx = self->joinMixer->outIxs[i+self->omitBin];
        if (row[outIx] == NULL)
            swizzleRow[i] = self->naForMissing ? "n/a" : "";
        else
            swizzleRow[i] = row[outIx];
        }
    finalRow = swizzleRow;
    }
char *chrom = finalRow[self->chromIx];
uint chromStart = sqlUnsigned(finalRow[self->startIx]);
uint chromEnd = sqlUnsigned(finalRow[self->endIx]);
return annoRowFromStringArray(chrom, chromStart, chromEnd, rightFail, finalRow, numCols, lm);
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
    // big item -- store aside in queue for merging later (unless it falls off the end of
    // the current chunk), move on to next item
    struct annoRow *aRow = rowToAnnoRow(self, row, *pRightFail, self->qLm);
    if (! (self->doNextChunk && self->nextChunkStart <= aRow->start))
        slAddHead(&(self->bigItemQueue), aRow);
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
if (self->bigItemQueue != NULL && annoRowCmp(&(self->bigItemQueue), &aRow) < 0)
    {
    // Big item gets to go now, so save aside small item for next time.
    outRow = slPopHead(&(self->bigItemQueue));
    slAddHead(&(self->smallItemQueue), aRow);
    }
// Clone outRow using callerLm
enum annoRowType rowType = self->streamer.rowType;
int numCols = self->streamer.numCols;
outRow = annoRowClone(outRow, rowType, numCols, callerLm);
if (self->bigItemQueue == NULL && self->smallItemQueue == NULL)
    {
    // No coarse-bin items to merge-sort, just stream finest-bin items from here on out.
    // This needs to be done after cloning outRow because it was allocated in self->qLm.
    resetMergeState(self);
    }
return outRow;
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
    {
    struct annoRow *aRow = nextRowMergeBins(self, minChrom, minEnd, callerLm);
    if (aRow == NULL && self->needQuery && !self->eof)
	// Recurse: query, then get next merged/filtered row:
	return asdNextRow(vSelf, minChrom, minEnd, callerLm);
    else
	return aRow;
    }
boolean rightFail = FALSE;
char **row = nextRowFiltered(self, &rightFail, minChrom, minEnd);
if (row == NULL)
    {
    if (self->needQuery && !self->eof)
	// Recurse: query, then get next merged/filtered row:
	return asdNextRow(vSelf, minChrom, minEnd, callerLm);
    else
	return NULL;
    }
return rowToAnnoRow(self, row, rightFail, callerLm);
}


static void makeMainTableDtfList(struct annoStreamDb *self, struct asObject *mainAsObj)
/* Make a list of mainTable columns. */
{
struct joinerDtf mainDtf;
mainDtf.database = self->db;
mainDtf.table = self->trackTable;
struct asColumn *col;
for (col = mainAsObj->columnList;  col != NULL;  col = col->next)
    {
    mainDtf.field = col->name;
    slAddHead(&self->mainTableDtfList, joinerDtfClone(&mainDtf));
    }
slReverse(&self->mainTableDtfList);
// If table has bin but asObj does not, add bin to head of mainTableDtfList.
if (self->hasBin && differentString("bin", self->mainTableDtfList->field))
    {
    mainDtf.field = "bin";
    slAddHead(&self->mainTableDtfList, joinerDtfClone(&mainDtf));
    }
}

static struct asObject *asObjForDtf(struct hash *hash, struct joinerDtf *dtf)
/* Get asObj for dtf, either from hash if we've seen it before, or make one. */
{
struct asObject *asObj = NULL;
char dbTable[PATH_LEN];
joinerDtfToSqlTableString(dtf, NULL, dbTable, sizeof(dbTable));
struct hashEl *hel = hashLookup(hash, dbTable);
if (hel == NULL)
    {
    asObj = hAnnoGetAutoSqlForDbTable(dtf->database, dtf->table, NULL, TRUE);
    if (asObj == NULL)
        errAbort("annoStreamDb: No autoSql for %s.%s.%s",
                 dtf->database, dtf->table, dtf->field);
    hel = hashAdd(hash, dbTable, asObj);
    }
else
    asObj = hel->val;
return asObj;
}

static void makeDottedTriple(char *dtfString, size_t dtfStringSize,
                             char *db, char *table, char *field)
/* In case we don't have a struct joinerDtf for a field that we want to look up,
 * but we do have the db, table and field name, concat with dots into dtfString.
 * Unlike joinerDtfToSqlFieldString, don't bother checking whether db is the main db. */
{
safef(dtfString, dtfStringSize, "%s.%s.%s", db, table, field);
}

char *annoStreamDbColumnNameFromDtf(char *db, char *mainTable, struct joinerDtf *dtf)
/* Return a string with the autoSql column name that would be assigned according to dtf's
 * db, table and field. */
{
char colName[PATH_LEN*2];
if (differentString(dtf->table, mainTable) || differentString(dtf->database, db))
    {
    joinerDtfToSqlFieldString(dtf, db, colName, sizeof(colName));
    // asParse rejects names that have '.' in them, which makes sense because it's for SQL,
    // so replace the '.'s with '_'s.
    subChar(colName, '.', '_');
    }
else
    safecpy(colName, sizeof(colName), dtf->field);
return cloneString(colName);
}

static void addOneColumn(struct dyString *dy, struct joinerDtf *dtf, char *db, char *mainTable,
                         struct asColumn *col, struct hash *dtfNames)
/* Append an autoSql text line describing col to dy.
 * If col is an array whose size is some other column that has not yet been added,
 * coerce its type to string to avoid asParseText errAbort. */
{
// First see if this col depends on a linked size column that hasn't been added yet.
boolean sizeColIsMissing = FALSE;
if (col->isArray && !col->fixedSize && isNotEmpty(col->linkedSizeName))
    {
    // col's size comes from another column -- has that column already been added?
    char linkedDtfString[PATH_LEN];
    makeDottedTriple(linkedDtfString, sizeof(linkedDtfString),
                     dtf->database, dtf->table, col->linkedSizeName);
    if (!hashLookup(dtfNames, linkedDtfString))
        sizeColIsMissing = TRUE;
    }
if (col->isArray && sizeColIsMissing)
    {
    // The size column is missing, so this can't be a valid array in autoSql --
    // ignore col->lowType and call it a (comma-separated) string.
    dyStringAppend(dy, "    lstring");
    }
else
    {
    dyStringPrintf(dy, "    %s", col->lowType->name);
    if (col->isArray)
        {
        dyStringAppendC(dy, '[');
        if (col->fixedSize)
            dyStringPrintf(dy, "%d", col->fixedSize);
        else
            dyStringAppend(dy, col->linkedSizeName);
        dyStringAppendC(dy, ']');
        }
    }
char *colName = annoStreamDbColumnNameFromDtf(db, mainTable, dtf);
dyStringPrintf(dy, "  %s; \"%s\"\n", colName, col->comment);
// Store plain old dotted triple in dtfNames in case we need to look it up later.
char dtfString[PATH_LEN];
makeDottedTriple(dtfString, sizeof(dtfString), dtf->database, dtf->table, dtf->field);
hashAdd(dtfNames, dtfString, NULL);
}

static struct asObject *asdAutoSqlFromTableFields(struct annoStreamDb *self,
                                                  struct asObject *mainAsObj)
/* Get autoSql for each table in self->relatedDtfList and append the columns
 * included in self->relatedDtfList to the main table asObj columns. */
{
struct dyString *newAsText = dyStringCreate("table %sCustom\n"
                                            "\"query based on %s with customized fields.\"\n"
                                            "    (",
                                            self->trackTable, self->trackTable);
// Use a hash of table to asObject so we fetch autoSql only once per table.
struct hash *asObjCache = hashNew(0);
// Use a hash of dtf strings to test whether or not one has been added already.
struct hash *dtfNames = hashNew(0);
// Start with all columns of main table:
struct joinerDtf mainDtf;
mainDtf.database = self->db;
mainDtf.table = self->trackTable;
struct asColumn *col;
for (col = mainAsObj->columnList;  col != NULL;  col = col->next)
    {
    mainDtf.field = col->name;
    addOneColumn(newAsText, &mainDtf, self->db, self->trackTable, col, dtfNames);
    }
// Append fields from related tables:
struct joinerDtf *dtf;
for (dtf = self->relatedDtfList;  dtf != NULL;  dtf = dtf->next)
    {
    struct asObject *asObj = asObjForDtf(asObjCache, dtf);
    struct asColumn *col = asColumnFind(asObj, dtf->field);
    if (col == NULL)
        errAbort("annoStreamDb: Can't find column %s in autoSql for table %s.%s",
                 dtf->field, dtf->database, dtf->table);
    addOneColumn(newAsText, dtf, self->db, self->trackTable, col, dtfNames);
    }
dyStringAppendC(newAsText, ')');
struct asObject *newAsObj = asParseText(newAsText->string);
hashFreeWithVals(&asObjCache, asObjectFree);
dyStringFree(&newAsText);
freeHashAndVals(&dtfNames);
return newAsObj;
}

static void asdClose(struct annoStreamer **pVSelf)
/* Close db connection and free self. */
{
if (pVSelf == NULL)
    return;
struct annoStreamDb *self = *(struct annoStreamDb **)pVSelf;
lmCleanup(&(self->qLm));
freeMem(self->trackTable);
freeMem(self->table);
slNameFreeList(&self->chromList);
joinerDtfFreeList(&self->mainTableDtfList);
joinerDtfFreeList(&self->relatedDtfList);
joinerFree(&self->joiner);
joinMixerFree(&self->joinMixer);
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

char *sqlTableIndexOnFieldANotB(struct sqlConnection *conn, char *table, char *fieldA, char *fieldB)
/* If table has an index that includes fieldA but not fieldB, return the index name, else NULL. */
{
char *indexNameA = NULL, *indexNameB = NULL;
char query[512];
sqlSafef(query, sizeof(query), "show index from %s", table);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (sameString(row[4], fieldA))
	indexNameA = cloneString(row[2]);
    else if (sameString(row[4], fieldB))
	indexNameB = cloneString(row[2]);
    }
if (sameOk(indexNameA, indexNameB))
    indexNameA = NULL;
sqlFreeResult(&sr);
return indexNameA;
}

static boolean isIncrementallyUpdated(char *table)
// Tables that have rows added to them after initial creation are not completely sorted
// because of new rows at end, so we have to 'order by'.
{
return (sameString(table, "refGene") || sameString(table, "refFlat") ||
	sameString(table, "xenoRefGene") || sameString(table, "xenoRefFlat") ||
	sameString(table, "all_mrna") || sameString(table, "xenoMrna") ||
	sameString(table, "all_est") || sameString(table, "xenoEst") ||
        sameString(table, "intronEst") ||
	sameString(table, "refSeqAli") || sameString(table, "xenoRefSeqAli"));
}

static boolean isPubsTable(char *table)
// Not absolutely every pubs* table is unsorted, but most of them are.
{
return startsWith("pubs", table);
}

static struct asObject *asdParseConfig(struct annoStreamDb *self, struct jsonElement *configEl)
/* Extract the autoSql for self->trackTable from the database.
 * If configEl is not NULL, expect it to be a description of related tables and fields like this:
 * config = { "relatedTables": [ { "table": "hg19.kgXref",
 *                                 "fields": ["geneSymbol", "description"] },
 *                               { "table": "hg19.knownCanonical",
 *                                 "fields": ["clusterId"] }
 *                             ] }
 * If so, unpack the [db.]tables and fields into self->relatedDtfList and append autoSql
 * column descriptions for each field to the autoSql object that describes our output.
 * It might also have "naForMissing": true/false; if so, set self->naForMissing. */
{
struct asObject *asObj = hAnnoGetAutoSqlForDbTable(self->db, self->trackTable, NULL, TRUE);
makeMainTableDtfList(self, asObj);
if (configEl != NULL)
    {
    struct hash *config = jsonObjectVal(configEl, "config");
    struct jsonElement *relatedTablesEl = hashFindVal(config, "relatedTables");
    if (relatedTablesEl)
        {
        // relatedTables is a list of objects like { table: <[db.]table name>,
        //                                           fields: [ <field1>, <field2>, ...] }
        struct slRef *relatedTables = jsonListVal(relatedTablesEl, "relatedTables");
        struct slRef *tfRef;
        for (tfRef = relatedTables;  tfRef != NULL;  tfRef = tfRef->next)
            {
            struct jsonElement *dbTableFieldEl = tfRef->val;
            struct hash *tfObj = jsonObjectVal(dbTableFieldEl,
                                               "{table,fields} object in relatedTables");
            struct jsonElement *dbTableEl = hashMustFindVal(tfObj, "table");
            char *dbTable = jsonStringVal(dbTableEl, "[db.]table in relatedTables");
            char tfDb[PATH_LEN], tfTable[PATH_LEN];
            hParseDbDotTable(self->db, dbTable, tfDb, sizeof(tfDb), tfTable, sizeof(tfTable));
            if (isEmpty(tfDb))
                safecpy(tfDb, sizeof(tfDb), self->db);
            if (hTableExists(tfDb, tfTable))
                {
                struct jsonElement *fieldListEl = hashMustFindVal(tfObj, "fields");
                struct slRef *fieldList = jsonListVal(fieldListEl, "fieldList");
                struct slRef *fieldRef;
                for (fieldRef = fieldList;  fieldRef != NULL;  fieldRef = fieldRef->next)
                    {
                    struct jsonElement *fieldEl = fieldRef->val;
                    char *tfField = jsonStringVal(fieldEl, "field");
                    slAddHead(&self->relatedDtfList, joinerDtfNew(tfDb, tfTable, tfField));
                    }
                }
            }
        if (self->relatedDtfList)
            {
            slReverse(&self->relatedDtfList);
            asObj = asdAutoSqlFromTableFields(self, asObj);
            }
        }
    struct jsonElement *naForMissingEl = hashFindVal(config, "naForMissing");
    if (naForMissingEl != NULL)
        self->naForMissing = jsonBooleanVal(naForMissingEl, "naForMissing");
    }
return asObj;
}

static char *sqlExplain(struct sqlConnection *conn, char *query)
/* For now, just turn the values back into a multi-line "#"-comment string. */
{
char *trimmedQuery = query;
if (startsWith(NOSQLINJ, trimmedQuery))
    trimmedQuery = trimmedQuery + strlen(NOSQLINJ);
struct dyString *dy = dyStringCreate("# Output of 'explain %s':\n", trimmedQuery);
char explainQuery[PATH_LEN*8];
safef(explainQuery, sizeof(explainQuery), NOSQLINJ "explain %s", trimmedQuery);
struct sqlResult *sr = sqlGetResult(conn, explainQuery);
struct slName *fieldList = sqlResultFieldList(sr);
int nColumns = slCount(fieldList);
// Header:
dyStringPrintf(dy, "# %s\n", slNameListToString(fieldList, '\t'));
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    dyStringAppend(dy, "# ");
    int i;
    for (i = 0;  i < nColumns;  i++)
        {
        if (i > 0)
            dyStringAppend(dy, "\t");
        if (row[i] == NULL)
            dyStringAppend(dy, "NULL");
        else
            dyStringAppend(dy, row[i]);
        }
    dyStringAppendC(dy, '\n');
    }
return dyStringCannibalize(&dy);
}

static char *asdGetHeader(struct annoStreamer *sSelf)
/* Return header with debug info. */
{
struct annoStreamDb *self = (struct annoStreamDb *)sSelf;
// Add a fake constraint on chromField because a real one is added to baselineQuery.
char queryWithChrom[PATH_LEN*4];
safef(queryWithChrom, sizeof(queryWithChrom), "%s %s %s.%s = 'someValue'", self->baselineQuery,
      (strstr(self->baselineQuery, "where") ? "and" : "where"), self->table, self->chromField);
char *explanation = sqlExplain(self->conn, queryWithChrom);
return explanation;
}

struct annoStreamer *annoStreamDbNew(char *db, char *table, struct annoAssembly *aa,
				     int maxOutRows, struct jsonElement *configEl)
/* Create an annoStreamer (subclass) object from a database table.
 * If config is NULL, then the streamer produces output from all fields
 * (except bin, unless table's autoSql includes bin).
 * Otherwise, config is a json object with a member 'relatedTables' that specifies
 * related tables and fields to join with table, for example:
 * config = { "relatedTables": [ { "table": "hg19.kgXref",
 *                                 "fields": ["geneSymbol", "description"] },
 *                               { "table": "hg19.knownCanonical",
 *                                 "fields": ["clusterId"] }
 *                             ] }
 * -- the streamer's autoSql will be constructed by appending autoSql column
 * descriptions to the columns of table.
 * Caller may free db, and table when done with them, but must keep the
 * annoAssembly aa alive for the lifetime of the returned annoStreamer. */
{
struct sqlConnection *conn = hAllocConn(db);
char splitTable[HDB_MAX_TABLE_STRING];
if (!hFindSplitTable(db, NULL, table, splitTable, sizeof splitTable, NULL))
    errAbort("annoStreamDbNew: can't find table (or split table) for '%s.%s'", db, table);
struct annoStreamDb *self = NULL;
AllocVar(self);
self->conn = conn;
self->db = cloneString(db);
self->trackTable = cloneString(table);
self->table = cloneString(splitTable);
if (sqlFieldIndex(self->conn, self->table, "bin") == 0)
    {
    self->hasBin = 1;
    self->minFinestBin = binFromRange(0, 1);
    }
struct asObject *asObj = asdParseConfig(self, configEl);
struct annoStreamer *streamer = &(self->streamer);
int dbtLen = strlen(db) + strlen(table) + 2;
char streamerName[dbtLen];
safef(streamerName, sizeof(streamerName), "%s.%s", db, table);
annoStreamerInit(streamer, aa, asObj, streamerName);
streamer->rowType = arWords;
streamer->setRegion = asdSetRegion;
streamer->nextRow = asdNextRow;
streamer->close = asdClose;
char *asFirstColumnName = streamer->asObj->columnList->name;
if (self->hasBin && !sameString(asFirstColumnName, "bin"))
    self->omitBin = 1;
if (!asdInitBed3Fields(self))
    errAbort("annoStreamDbNew: can't figure out which fields of %s.%s to use as "
	     "{chrom, chromStart, chromEnd}.", db, self->table);
// When a table has an index on endField (not startField), sometimes the query optimizer uses it
// and that ruins the sorting.  Fortunately most tables don't anymore.
self->endFieldIndexName = sqlTableIndexOnFieldANotB(self->conn, self->table, self->endField,
                                                    self->startField);
self->notSorted = FALSE;
// Special case: genbank-updated tables are not sorted because new mappings are
// tacked on at the end.  Max didn't sort the pubs* tables but I hope he will
// sort the tables for any future tracks.  :)
if (isIncrementallyUpdated(table) || isPubsTable(table))
    self->notSorted = TRUE;
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
asdInitBaselineQuery(self);
asdUpdateBaselineQuery(self);
struct annoStreamer *sSelf = (struct annoStreamer *)self;
if (asdDebug)
    sSelf->getHeader = asdGetHeader;
return sSelf;
}
