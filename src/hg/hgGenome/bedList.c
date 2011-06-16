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

static char const rcsid[] = "$Id: bedList.c,v 1.7 2010/05/11 01:43:24 kent Exp $";

boolean htiIsPsl(struct hTableInfo *hti)
/* Return TRUE if table looks to be in psl format. */
{
return sameString("tStarts", hti->startsField);
}


char *getBedGraphType(char *table)
/* Return bedgraph track type if table is a bedGraph in the current database's
 * trackDb. */
{
if (curTrack && startsWith("bedGraph", curTrack->type))
    {
    if (sameString(curTrack->table, table))
	return curTrack->type;
    }
else
    {
    struct trackDb *tdb = hTrackDbForTrack(database, table);
    if (tdb && startsWith("bedGraph", tdb->type))
	return tdb->type;
    }
return NULL;
}

boolean isBedGraph(char *table)
/* Return TRUE if table is specified as a bedGraph in the current database's
 * trackDb. */
{
char *bgt = getBedGraphType(table);
return (bgt && startsWith("bedGraph", bgt));
}


int  getBedGraphColumnNum(char *table)
/* get the bedGraph dataValue column num from the track type */
{
char *typeLine = cloneString(getBedGraphType(table));
int wordCount;
char *words[8];
wordCount = chopLine(typeLine,words);
if (wordCount > 1)
    return sqlUnsigned(words[1]);
return 0;
}

char *getBedGraphField(char *table)
/* get the bedGraph dataValue field name from the track type */
{
char query[256];
struct sqlResult *sr;
char **row;
int colCount = 0;
char *bedGraphField = NULL;
int bedGraphColumnNum = getBedGraphColumnNum(table);
struct sqlConnection *conn = NULL;
if (isCustomTrack(curTable))
    {
    conn = hAllocConn(CUSTOM_TRASH);
    struct customTrack *ct = lookupCt(table);
    table = ct->dbTableName;
    }
else
    conn = curTrack ? hAllocConnTrack(database, curTrack) : hAllocConn(database);

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
hFreeConn(&conn);
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
	char *bedGraphField = getBedGraphField(hti->rootName);
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
bed->name=lmCloneString(lm,row[2]);  /* for lack of a better place */
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
struct sqlConnection *conn = curTrack ? hAllocConnTrack(database, curTrack) : hAllocConn(database);
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

hFreeConn(&conn);
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


