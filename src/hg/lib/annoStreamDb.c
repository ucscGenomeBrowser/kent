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
    boolean hasBin;			// 1 if SQL table's first column is bin
    boolean omitBin;			// 1 if table hasBin and autoSql doesn't have bin
    int numCols;			// Number of columns in autoSql
    char *chromField;			// Name of chrom-ish column in table
    char *startField;			// Name of chromStart-ish column in table
    char *endField;			// Name of chromEnd-ish column in table
    int chromIx;			// Index of chrom-ish col in autoSql or bin-less table
    int startIx;			// Index of chromStart-ish col in autoSql or bin-less table
    int endIx;				// Index of chromEnd-ish col in autoSql or bin-less table
    };

static void asdSetRegionDb(struct annoStreamer *vSelf,
			    char *chrom, uint regionStart, uint regionEnd)
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
    dyStringPrintf(query, " where %s='%s'", self->chromField, streamer->chrom);
    int chromSize = hashIntVal(streamer->query->chromSizes, streamer->chrom);
    if (streamer->regionStart != 0 || streamer->regionEnd != chromSize)
	{
	dyStringAppend(query, " and ");
	if (self->hasBin)
	    hAddBinToQuery(streamer->regionStart, streamer->regionEnd, query);
	dyStringPrintf(query, "%s < %u and %s > %u", self->startField, streamer->regionEnd,
		       self->endField, streamer->regionStart);
	}
    }
verbose(2, "mysql query: '%s'\n", query->string);
struct sqlResult *sr = sqlStoreResult(self->conn, query->string);
dyStringFree(&query);
self->sr = sr;
}

static char **nextRowUnfiltered(struct annoStreamDb *self)
/* Fetch the next row from the sqlResult, and skip past table's bin field if necessary. */
{
char **row = sqlNextRow(self->sr);
if (row == NULL)
    return NULL;
// Skip table's bin field if not in autoSql:
row += self->omitBin;
return row;
}

static struct annoRow *asdNextRowDb(struct annoStreamer *vSelf)
/* Perform sql query if we haven't already and return a single
 * annoRow, or NULL if there are no more items. */
{
struct annoStreamDb *self = (struct annoStreamDb *)vSelf;
if (self->sr == NULL)
    asdDoQuery(self);
if (self->sr == NULL)
    // This is necessary only if we're using sqlStoreResult in asdDoQuery, harmless otherwise:
    return NULL;
char **row = nextRowUnfiltered(self);
if (row == NULL)
    return NULL;
// Skip past any left-join failures until we get a right-join failure, a passing row, or EOF.
boolean rightFail = FALSE;
while (annoFilterRowFails(vSelf->filters, row, self->numCols, &rightFail))
    {
    if (rightFail)
	break;
    row = nextRowUnfiltered(self);
    if (row == NULL)
	return NULL;
    }
char *chrom = row[self->omitBin+self->chromIx];
uint chromStart = sqlUnsigned(row[self->omitBin+self->startIx]);
uint chromEnd = sqlUnsigned(row[self->omitBin+self->endIx]);
return annoRowFromStringArray(chrom, chromStart, chromEnd, rightFail, row, self->numCols);
}

static void asdCloseDb(struct annoStreamer **pVSelf)
/* Close db connection and free self. */
{
if (pVSelf == NULL)
    return;
struct annoStreamDb *self = *(struct annoStreamDb **)pVSelf;
// Let the caller close conn; it might be from a cache.
freeMem(self->table);
sqlFreeResult(&(self->sr));
annoStreamerFree(pVSelf);
}

static int asColumnFindIx(struct asColumn *list, char *string)
/* Return index of first element of asColumn list that matches string.
 * Return -1 if not found. */
{
struct asColumn *ac;
int ix = 0;
for (ac = list; ac != NULL; ac = ac->next, ix++)
    if (sameString(string, ac->name))
        return ix;
return -1;
}

static boolean asHasFields(struct annoStreamDb *self, char *chromField, char *startField,
			   char *endField)
/* If autoSql def has all three columns, remember their names and column indexes and 
 * return TRUE. */
{
struct asColumn *columns = self->streamer.asObj->columnList;
int chromIx = asColumnFindIx(columns, chromField);
int startIx = asColumnFindIx(columns, startField);
int endIx = asColumnFindIx(columns, endField);
if (chromIx >= 0 && startIx >= 0 && endIx >= 0)
    {
    self->chromField = cloneString(chromField);
    self->startField = cloneString(startField);
    self->endField = cloneString(endField);
    self->chromIx = chromIx;
    self->startIx = startIx;
    self->endIx = endIx;
    return TRUE;
    }
return FALSE;
}

static boolean asdInitBed3Fields(struct annoStreamDb *self)
/* Use autoSql to figure out which table fields correspond to {chrom, chromStart, chromEnd}. */
{
if (asHasFields(self, "chrom", "chromStart", "chromEnd"))
    return TRUE;
if (asHasFields(self, "chrom", "txStart", "txEnd"))
    return TRUE;
if (asHasFields(self, "tName", "tStart", "tEnd"))
    return TRUE;
if (asHasFields(self, "genoName", "genoStart", "genoEnd"))
    return TRUE;
return FALSE;
}

struct annoStreamer *annoStreamDbNew(struct sqlConnection *conn, char *table,
				     struct asObject *asObj)
/* Create an annoStreamer (subclass) object from a database table described by asObj.
 * conn must not be shared. */
{
if (!sqlTableExists(conn, table))
    errAbort("annoStreamNewDb: table %s doesn't exist", table);
struct annoStreamDb *self = NULL;
AllocVar(self);
struct annoStreamer *streamer = &(self->streamer);
annoStreamerInit(streamer, asObj);
streamer->setRegion = asdSetRegionDb;
streamer->nextRow = asdNextRowDb;
streamer->close = asdCloseDb;
self->conn = conn;
self->table = cloneString(table);
char *asFirstColumnName = streamer->asObj->columnList->name;
if (sqlFieldIndex(self->conn, self->table, "bin") == 0)
    self->hasBin = 1;
if (self->hasBin && !sameString(asFirstColumnName, "bin"))
    self->omitBin = 1;
self->numCols = slCount(streamer->asObj->columnList);
if (!asdInitBed3Fields(self))
    errAbort("annoStreamDbNew: can't figure out which fields of %s to use as "
	     "{chrom, chromStart, chromEnd}.", table);
return (struct annoStreamer *)self;
}
