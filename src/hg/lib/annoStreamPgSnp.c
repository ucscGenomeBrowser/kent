/* annoStreamPgSnp -- subclass of annoStreamer for pgSnp data */

#include "annoStreamPgSnp.h"
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
    boolean hasBin;			// 1 if first column is "bin"
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

static struct annoRow *aspsNextRowDb(struct annoStreamer *vSelf, boolean *retFilterFailed)
/* Perform sql query if we haven't already and return a single annoRow, or NULL if
 * there are no more items. */
{
struct annoStreamPgSnpDb *self = (struct annoStreamPgSnpDb *)vSelf;
if (self->sr == NULL)
    {
    struct dyString *query = dyStringCreate("select * from %s", self->table);
    if (!self->streamer.positionIsGenome)
	{
	dyStringPrintf(query, "where chrom='%s' and chromStart < %u and chromEnd > %u",
		       self->streamer.chrom, self->streamer.regionEnd, self->streamer.regionStart);
	self->hasBin = (sqlFieldIndex(self->conn, self->table, "bin") == 0) ? 1 : 0;
	if (self->hasBin)
	    hAddBinToQuery(self->streamer.regionStart, self->streamer.regionEnd, query);
	self->sr = sqlGetResult(self->conn, query->string);
	dyStringFree(&query);
	}
    }
char **row = sqlNextRow(self->sr);
if (row == NULL)
    return NULL;
// Skip bin field if there is one:
row += self->hasBin;

//#*** TODO: implement filtering!

// pgSnp-specific fix: pgSnp autoSql currently includes bin, at the end(!),
// while the sql table bin is at the beginning as it should be.  Anyway,
// the number of meaningful columns is one less than PGSNP_NUL_COLS.
int correctedColCount = PGSNP_NUM_COLS - 1;
return annoRowFromStringArray(row[0], sqlUnsigned(row[1]), sqlUnsigned(row[2]),
			      row, correctedColCount);
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
//#*** currently pgSnpAsObj() includes bin, ugh...
annoStreamerInit(&(self->streamer), pgSnpAsObj());
self->streamer.setRegion = aspsSetRegionDb;
self->streamer.nextRow = aspsNextRowDb;
self->streamer.close = aspsCloseDb;
self->conn = conn;
self->table = cloneString(table);
return (struct annoStreamer *)self;
}
