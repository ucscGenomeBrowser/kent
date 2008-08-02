/* bedList - get list of beds in region that pass filtering. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "localmem.h"
#include "dystring.h"
#include "jksql.h"
#include "cheapcgi.h"
#include "trackDb.h"
#include "customTrack.h"
#include "hdb.h"
#include "hui.h"
#include "hCommon.h"
#include "web.h"
#include "featureBits.h"
#include "portable.h"
#include "wiggle.h"
#include "correlate.h"
#include "bedCart.h"
#include "trashDir.h"

#include "hgGenome.h"

static char const rcsid[] = "$Id: bedList.c,v 1.2.44.2 2008/08/02 04:06:21 markd Exp $";

boolean htiIsPsl(struct hTableInfo *hti)
/* Return TRUE if table looks to be in psl format. */
{
return sameString("tStarts", hti->startsField);
}


boolean isBedGraph(char *table)
/* Return TRUE if table is specified as a bedGraph in the current database's 
 * trackDb. */
{
if (curTrack && sameString(curTrack->tableName, table))
    return startsWith("bedGraph", curTrack->type);
else
    {
    struct trackDb *tdb = hTrackDbForTrack(database, table);
    return (tdb && startsWith("bedGraph", tdb->type));
    }
return FALSE;
}



char *getBedGraphField(char *table, char *type)
/* get the bedGraph dataValue field name from the track type */
{
char query[256];
struct sqlResult *sr;
char **row;
int wordCount;
char *words[8];
char *typeLine = cloneString(type);
int colCount = 0;
char *bedGraphField = NULL;
int bedGraphColumnNum = 0;
struct sqlConnection *conn = sqlConnect(database);

wordCount = chopLine(typeLine,words);
if (wordCount > 1)
    bedGraphColumnNum = sqlUnsigned(words[1]);
freez(&typeLine);
safef(query, ArraySize(query), "describe %s", table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    ++colCount;
    if ((1 == colCount) && sameString(row[0], "bin"))
	colCount = 0;
    if (colCount == bedGraphColumnNum)
	bedGraphField = cloneString(row[0]);
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
return bedGraphField;
}


void bedSqlFieldsExceptForChrom(char *chrom,
	int *retFieldCount, char **retFields)
/* Given tableInfo figure out what fields are needed to
 * add to a database query to have information to create
 * a bed. The chromosome is not one of these fields - we
 * assume that is already known since we're processing one
 * chromosome at a time.   Return a comma separated (no last
 * comma) list of fields that can be freeMem'd, and the count
 * of fields (this *including* the chromosome). */
{
struct dyString *fields = dyStringNew(128);
struct hTableInfo *hti = hFindTableInfo(database, chrom, curTable);
if (hti == NULL)
    errAbort("Could not find table info for table %s", curTable);

int fieldCount = 3;
dyStringPrintf(fields, "%s,%s", hti->startField, hti->endField);

if (curTrack) /* some tables from all tables list have no curTrack */
    {
    if (startsWith("bedGraph", curTrack->type))
	{
	char fullTableName[256];
	++fieldCount;
	if (hti->isSplit)
	    safef(fullTableName,sizeof(fullTableName),"%s_%s", chrom, hti->rootName);
	else
	    safef(fullTableName,sizeof(fullTableName),"%s", hti->rootName);
	char *bedGraphField = getBedGraphField(hti->rootName,curTrack->type);
	dyStringPrintf(fields, ",%s", bedGraphField);
	}
    else if (isMafTable(database, curTrack, curTable))
	{
	++fieldCount;
	dyStringPrintf(fields, ",%s", "score");
	}
    }
*retFieldCount = fieldCount;
*retFields = dyStringCannibalize(&fields);
}


struct bed *bedFromRow(
	char *chrom, 		  /* Chromosome bed is on. */
	char **row,  		  /* Row with other data for bed. */
	int fieldCount,		  /* Number of fields in final bed. */
	struct lm *lm)		  /* Local memory pool */
/* Create bed from a database row when we already understand
 * the format pretty well.  The bed is allocated inside of
 * the local memory pool lm.  Generally use this in conjunction
 * with the results of a SQL query constructed with the aid
 * of the bedSqlFieldsExceptForChrom function. */
{
struct bed *bed;

lmAllocVar(lm, bed);
bed->chrom = chrom;
bed->chromStart = sqlUnsigned(row[0]);
bed->chromEnd = sqlUnsigned(row[1]);
if (fieldCount < 4)
    return bed;
/* for bedGraph */
lmAllocVar(lm,bed->expScores);
bed->expScores[0] = sqlFloat(row[2]);
return bed;
}

struct bed *getChromAsBed(
	char *chrom,         /* Region to get data for. */
	struct lm *lm,	     /* Where to allocate memory. */
	int *retFieldCount)  /* Number of fields. */
/* Return a bed list of all items in the given range in table.
 * Cleanup result via lmCleanup(&lm) rather than bedFreeList.  */
{
char *fields = NULL;
struct sqlConnection *conn = sqlConnect(database);
struct sqlResult *sr;
struct bed *bedList=NULL, *bed;
char **row;
int fieldCount;

bedSqlFieldsExceptForChrom(chrom, &fieldCount, &fields);

/* All beds have at least chrom,start,end.  We omit the chrom
 * from the query since we already know it. */
sr = chromQuery(conn, curTable, fields, chrom, TRUE, NULL);
while (sr != NULL && (row = sqlNextRow(sr)) != NULL)
    {
    bed = bedFromRow(chrom, row, fieldCount, lm);
    slAddHead(&bedList, bed);
    }
freez(&fields);
sqlFreeResult(&sr);
slReverse(&bedList);

sqlDisconnect(&conn);
if (retFieldCount)
    *retFieldCount = fieldCount;
return(bedList);
}

struct bed *getBeds(char *chrom, struct lm *lm, int *retFieldCount)
/* Get list of beds on single region. */
{
struct bed *bedList = NULL;
if (isCustomTrack(curTable))
    bedList = customTrackGetBedsForChrom(curTable, chrom, lm, retFieldCount);
else
    bedList = getChromAsBed(chrom, lm, retFieldCount);
return bedList;
}


