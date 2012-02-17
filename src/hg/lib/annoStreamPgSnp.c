/* annoStreamPgSnp -- subclass of annoStreamer for pgSnp data */

#include "annoStreamPgSnp.h"
#include "annoGratorQuery.h"
#include "hdb.h"
#include "pgSnp.h"
#include "sqlNum.h"

struct annoStreamPgSnpDb
    {
    struct annoStreamer streamer;	// Parent class members & methods
    // Private members
    struct sqlConnection *conn;		// Database connection (e.g. hg19 or customTrash)
    char *table;			// Table name, must exist in database
    struct sqlResult *sr;		// SQL query result from which we grab rows
    boolean hasBin;			// 1 if SQL table's first column is bin
    boolean skipBin;			// 1 if table hasBin and autoSql doesn't have bin
    };

static void aspsSetRegionDb(struct annoStreamer *vSelf,
			    char *chrom, uint regionStart, uint regionEnd)
/* Set region -- and free current sqlResult if there is one. */
{
annoStreamerSetRegion(vSelf, chrom, regionStart, regionEnd);
struct annoStreamPgSnpDb *self = (struct annoStreamPgSnpDb *)vSelf;
if (self->sr != NULL)
    sqlFreeResult(&(self->sr));
}

struct sqlResult *aspsDoQuery(struct annoStreamPgSnpDb *self)
/* Return a sqlResult for a query on table items in position range. */
// NOTE: it would be possible to implement filters at this level, as in hgTables.
{
struct annoStreamer *streamer = &(self->streamer);
struct dyString *query = dyStringCreate("select * from %s", self->table);
if (!streamer->positionIsGenome)
    {
    dyStringPrintf(query, "where chrom='%s'", streamer->chrom);
    int chromSize = hashIntVal(streamer->query->chromSizes, streamer->chrom);
    if (streamer->regionStart != 0 || streamer->regionEnd != chromSize)
	{
	dyStringPrintf(query, " and chromStart < %u and chromEnd > %u",
		       streamer->regionEnd, streamer->regionStart);
	if (self->hasBin)
	    hAddBinToQuery(streamer->regionStart, streamer->regionEnd, query);
	}
    }
struct sqlResult *sr = sqlGetResult(self->conn, query->string);
dyStringFree(&query);
return sr;
}

static struct annoRow *aspsNextRowDb(struct annoStreamer *vSelf, boolean *retFilterFailed)
/* Perform sql query if we haven't already and return a single annoRow, or NULL if
 * there are no more items. */
{
struct annoStreamPgSnpDb *self = (struct annoStreamPgSnpDb *)vSelf;
if (self->sr == NULL)
    aspsDoQuery(self);
char **row = sqlNextRow(self->sr);
if (row == NULL)
    return NULL;
// Skip bin field if necessary:
row += self->skipBin;
boolean fail = annoFilterTestRow(self->streamer.filters, row, PGSNP_NUM_COLS);
if (retFilterFailed != NULL)
    *retFilterFailed = fail;
return annoRowFromStringArray(row[0], sqlUnsigned(row[1]), sqlUnsigned(row[2]),
			      row, PGSNP_NUM_COLS);
}

static void aspsCloseDb(struct annoStreamer **pVSelf)
/* Close db connection and free self. */
{
if (pVSelf == NULL)
    return;
struct annoStreamPgSnpDb *self = *(struct annoStreamPgSnpDb **)pVSelf;
// Let the caller close conn; it might be from a cache.
freez(&(self->table));
sqlFreeResult(&(self->sr));
annoStreamerFree(pVSelf);
}

struct annoStreamer *annoStreamPgSnpNewDb(struct sqlConnection *conn, char *table)
/* Create an annoStreamer (subclass) object from a pgSnp database table. */
{
if (!sqlTableExists(conn, table))
    errAbort("annoStreamPgSnpNewDb: table %s doesn't exist", table);
struct annoStreamPgSnpDb *self = NULL;
AllocVar(self);
struct annoStreamer *streamer = &(self->streamer);
annoStreamerInit(streamer, pgSnpAsObj());
streamer->setRegion = aspsSetRegionDb;
streamer->nextRow = aspsNextRowDb;
streamer->close = aspsCloseDb;
self->conn = conn;
self->table = cloneString(table);
char *asFirstColumnName = streamer->asObj->columnList->name;
if (sqlFieldIndex(self->conn, self->table, "bin") == 0)
    self->hasBin = 1;
if (self->hasBin && !sameString(asFirstColumnName, "bin"))
    self->skipBin = 1;
return (struct annoStreamer *)self;
}
