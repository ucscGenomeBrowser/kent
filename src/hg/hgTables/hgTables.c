/* hgTables - Get table data associated with tracks and intersect tracks. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "memalloc.h"
#include "htmshell.h"
#include "cheapcgi.h"
#include "cart.h"
#include "jksql.h"
#include "hdb.h"
#include "web.h"
#include "hui.h"
#include "hgColors.h"
#include "trackDb.h"
#include "grp.h"
#include "customTrack.h"
#include "hgTables.h"

static char const rcsid[] = "$Id: hgTables.c,v 1.30 2004/07/18 18:41:10 kent Exp $";


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgTables - Get table data associated with tracks and intersect tracks\n"
  "usage:\n"
  "   hgTables XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Global variables. */
struct cart *cart;	/* This holds cgi and other variables between clicks. */
struct hash *oldVars;	/* The cart before new cgi stuff added. */
char *genome;		/* Name of genome - mouse, human, etc. */
char *database;		/* Name of genome database - hg15, mm3, or the like. */
struct trackDb *fullTrackList;	/* List of all tracks in database. */
struct trackDb *curTrack;	/* Currently selected track. */
struct customTrack *theCtList = NULL;	/* List of custom tracks. */
struct slName *browserLines = NULL;	/* Browser lines in custom tracks. */
char *customTrackPseudoDb = "customTrack"; /* Fake database for custom tracks */

/* --------------- HTML Helpers ----------------- */

void hvPrintf(char *format, va_list args)
/* Print out some html.  Check for write error so we can
 * terminate if http connection breaks. */
{
vprintf(format, args);
if (ferror(stdout))
    noWarnAbort();
}

void hPrintf(char *format, ...)
/* Print out some html.  Check for write error so we can
 * terminate if http connection breaks. */
{
va_list(args);
va_start(args, format);
hvPrintf(format, args);
va_end(args);
}

void hPrintSpaces(int count)
/* Print a number of non-breaking spaces. */
{
int i;
for (i=0; i<count; ++i)
    hPrintf("&nbsp;");
}

static void vaHtmlOpen(char *format, va_list args)
/* Start up a page that will be in html format. */
{
puts("Content-Type:text/html\n");
cartVaWebStart(cart, format, args);
}

void htmlOpen(char *format, ...)
/* Start up a page that will be in html format. */
{
va_list args;
va_start(args, format);
vaHtmlOpen(format, args);
}

void htmlClose()
/* Close down html format page. */
{
cartWebEnd();
}

void hTableStart()
/* For some reason BORDER=1 does not work in our web.c nested table scheme.
 * So use web.c's trick of using an enclosing table to provide a border.   */
{
puts("<!--outer table is for border purposes-->" "\n"
     "<TABLE BGCOLOR=\"#"HG_COL_BORDER"\" BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"1\"><TR><TD>");
puts("<TABLE BORDER=\"1\" BGCOLOR=\""HG_COL_INSIDE"\" CELLSPACING=\"0\">");
}

void hTableEnd()
/* Close out table started with hTableStart() */
{
puts("</TABLE>");
puts("</TR></TD></TABLE>");
}

/* --------------- Text Mode Helpers ----------------- */

static void textWarnHandler(char *format, va_list args)
/* Text mode error message handler. */
{
char *hLine =
"---------------------------------------------------------------------------\n";
if (format != NULL) {
    fflush(stdout);
    fprintf(stdout, "%s", hLine);
    vfprintf(stdout, format, args);
    fprintf(stdout, "\n");
    fprintf(stdout, "%s", hLine);
    }
}

static void textAbortHandler()
/* Text mode abort handler. */
{
exit(-1);
}

void textOpen()
/* Start up page in text format. (No need to close this). */
{
printf("Content-Type: text/plain\n\n");
pushWarnHandler(textWarnHandler);
pushAbortHandler(textAbortHandler);
}

/* --------- Utility functions --------------------- */

static struct region *getRegionsFullGenome()
/* Get a region list that covers all of each chromosome. */
{
struct slName *chrom, *chromList = hAllChromNames();
struct region *region, *regionList = NULL;
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    AllocVar(region);
    region->chrom = cloneString(chrom->name);
    slAddHead(&regionList, region);
    }
slReverse(&regionList);
slFreeList(&chromList);
return regionList;
}

struct region *getEncodeRegions()
/* Get encode regions from encodeRegions table. */
{
struct sqlConnection *conn = sqlConnect(database);
struct sqlResult *sr;
char **row;
struct region *list = NULL, *region;
sr = sqlGetResult(conn, "select chrom,chromStart,chromEnd from encodeRegions");
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(region);
    region->chrom = cloneString(row[0]);
    region->start = atoi(row[1]);
    region->end = atoi(row[2]);
    slAddHead(&list, region);
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
slReverse(&list);
return list;
}

struct region *getRegions()
/* Consult cart to get list of regions to work on. */
{
char *regionType = cartUsualString(cart, hgtaRegionType, "genome");
struct region *regionList = NULL, *region;
if (sameString(regionType, "genome"))
    {
    regionList = getRegionsFullGenome();
    }
else if (sameString(regionType, "range"))
    {
    char *range = cartString(cart, hgtaRange);
    regionList = AllocVar(region);
    if (!hgParseChromRange(range, &region->chrom, &region->start, &region->end))
        {
	errAbort("%s is not a chromosome range.  "
	         "Please go back and enter something like chrX:1000000-1100000 "
		 "in the range control.", range);
	}
    }
else if (sameString(regionType, "encode"))
    {
    regionList = getEncodeRegions();
    }
else
    {
    errAbort("Unrecognized region type %s", regionType);
    }
// uglyf("regionTYpe %s\n", regionType);
// for (region=regionList;region!=NULL;region=region->next) uglyf("%s:%d-%d\n", region->chrom, region->start, region->end);
return regionList;
}

struct sqlResult *regionQuery(struct sqlConnection *conn, char *table,
	char *fields, struct region *region, boolean isPositional)
/* Construct and execute query for table on region. */
{
struct sqlResult *sr;
if (isPositional)
    {
    if (region->end == 0) /* Full chromosome. */
	{
	sr = hExtendedChromQuery(conn, table, region->chrom, 
		NULL, FALSE, fields, NULL);
	}
    else
	{
	sr = hExtendedRangeQuery(conn, table, region->chrom, 
		region->start, region->end, NULL, 
		FALSE, fields, NULL);
	}
    }
else
    {
    char query[256];
    safef(query, sizeof(query), 
	    "select %s from %s", fields, table);
    sr = sqlGetResult(conn, query);
    }
return sr;
}


char *connectingTableForTrack(struct trackDb *track)
/* Return table name to use with all.joiner for track. 
 * You can freeMem this when done. */
{
if (sameString(track->tableName, "mrna"))
    return cloneString("all_mrna");
else if (sameString(track->tableName, "intronEst"))
    return cloneString("all_est");
else if (sameString(track->tableName, "est"))
    return cloneString("all_est");
else 
    return cloneString(track->tableName);
}

char *chromTable(struct sqlConnection *conn, char *table)
/* Get chr1_table if it exists, otherwise table. 
 * You can freeMem this when done. */
{
char *chrom = hDefaultChrom();
if (sameString("mrna", table))
    return cloneString(table);
else if (sqlTableExists(conn, table))
    return cloneString(table);
else
    {
    char buf[256];
    safef(buf, sizeof(buf), "%s_%s", chrom, table);
    return cloneString(buf);
    }
}

char *chrnTable(struct sqlConnection *conn, char *table)
/* Return chrN_table if table is split, otherwise table. 
 * You can freeMem this when done. */
{
char buf[256];
char *splitTable = chromTable(conn, table);
if (!sameString(splitTable, table))
     {
     safef(buf, sizeof(buf), "chrN_%s", table);
     freez(&splitTable);
     return cloneString(buf);
     }
else
     return splitTable;
}

void checkTableExists(struct sqlConnection *conn, char *table)
/* Check that table exists, or put up an error message. */
{
char *splitTable = chromTable(conn, table);
if (!sqlTableExists(conn, table))
    errAbort("Table %s doesn't exist", table);
freeMem(splitTable);
}

struct customTrack *getCustomTracks()
/* Get custom track list. */
{
if (theCtList == NULL)
    theCtList = customTracksParseCart(cart, &browserLines, NULL);
return(theCtList);
}

struct customTrack *lookupCt(char *name)
/* Find named custom track. */
{
struct customTrack *ctList = getCustomTracks();
struct customTrack *ct;
for (ct=ctList;  ct != NULL;  ct=ct->next)
    {
    if (sameString(ct->tdb->tableName, name))
	return ct;
    }
return NULL;
}

struct hTableInfo *ctToHti(struct customTrack *ct)
/* Create an hTableInfo from a customTrack. */
{
struct hTableInfo *hti;

if (ct == NULL)
    return(NULL);

AllocVar(hti);
hti->rootName = cloneString(ct->tdb->tableName);
hti->isPos = TRUE;
hti->isSplit = FALSE;
hti->hasBin = FALSE;
hti->type = cloneString(ct->tdb->type);
if (ct->fieldCount >= 3)
    {
    strncpy(hti->chromField, "chrom", 32);
    strncpy(hti->startField, "chromStart", 32);
    strncpy(hti->endField, "chromEnd", 32);
    }
if (ct->fieldCount >= 4)
    {
    strncpy(hti->nameField, "name", 32);
    }
if (ct->fieldCount >= 5)
    {
    strncpy(hti->scoreField, "score", 32);
    }
if (ct->fieldCount >= 6)
    {
    strncpy(hti->strandField, "strand", 32);
    }
if (ct->fieldCount >= 8)
    {
    strncpy(hti->cdsStartField, "thickStart", 32);
    strncpy(hti->cdsEndField, "thickEnd", 32);
    hti->hasCDS = TRUE;
    }
if (ct->fieldCount >= 12)
    {
    strncpy(hti->countField, "blockCount", 32);
    strncpy(hti->startsField, "chromStarts", 32);
    strncpy(hti->endsSizesField, "blockSizes", 32);
    hti->hasBlocks = TRUE;
    }

return(hti);
}

struct hTableInfo *maybeGetHti(char *db, char *table)
/* Return primary table info. */
{
struct hTableInfo *hti = NULL;

if (sameString(customTrackPseudoDb, db))
    {
    struct customTrack *ct = lookupCt(table);
    hti = ctToHti(ct);
    }
else
    {
    char *track;
    if (startsWith("chrN_", table))
	track = table + strlen("chrN_");
    else
	track = table;
    hti = hFindTableInfoDb(db, NULL, track);
    }
return(hti);
}

struct hTableInfo *getHti(char *db, char *table)
/* Return primary table info. */
{
struct hTableInfo *hti = maybeGetHti(db, table);

if (hti == NULL)
    {
    errAbort("Could not find table info for table %s in db %s",
	     table, db);
    }
return(hti);
}

boolean isPositional(char *db, char *table)
/* Return TRUE if it looks to be a positional table. */
{
boolean result = FALSE;
struct sqlConnection *conn = sqlConnect(db);
if (!sameString(table, "mrna"))
    {
    if (sqlTableExists(conn, "chromInfo"))
	{
	char chromName[64];
	struct hTableInfo *hti;
	sqlQuickQuery(conn, "select chrom from chromInfo limit 1", 
	    chromName, sizeof(chromName));
	hti = hFindTableInfoDb(db, chromName, table);
	if (hti != NULL)
	    {
	    result = htiIsPositional(hti);
	    }
	}
    }
sqlDisconnect(&conn);
return result;
}

boolean isSqlStringType(char *type)
/* Return TRUE if it a a stringish SQL type. */
{
return strstr(type, "char") || strstr(type, "text") 
	|| strstr(type, "blob") || startsWith("enum", type);
}

boolean isSqlNumType(char *type)
/* Return TRUE if it is a numerical SQL type. */
{
return strstr(type, "int") || strstr(type, "float") || strstr(type, "double");
}


struct trackDb *findTrackInGroup(char *name, struct trackDb *trackList,
    struct grp *group)
/* Find named track that is in group (NULL for any group).
 * Return NULL if can't find it. */
{
struct trackDb *track;
for (track = trackList; track != NULL; track = track->next)
    {
    if (sameString(name, track->tableName) &&
       (group == NULL || sameString(group->name, track->grp)))
       return track;
    }
return NULL;
}

struct trackDb *findTrack(char *name, struct trackDb *trackList)
/* Find track, or return NULL if can't find it. */
{
return findTrackInGroup(name, trackList, NULL);
}

struct trackDb *findSelectedTrack(struct trackDb *trackList, 
    struct grp *group)
/* Find selected track - from CGI variable if possible, else
 * via various defaults. */
{
char *name = cartOptionalString(cart, hgtaTrack);
struct trackDb *track = NULL;
if (name != NULL)
    track = findTrackInGroup(name, trackList, group);
if (track == NULL)
    {
    if (group == NULL)
        track = trackList;
    else
	{
	for (track = trackList; track != NULL; track = track->next)
	    if (sameString(track->grp, group->name))
	         break;
	if (track == NULL)
	    internalErr();
	}
    }
return track;
}



void addWhereClause(struct dyString *query, boolean *gotWhere)
/* Add where clause to query.  If already have a where clause
 * add 'and' to it. */
{
if (*gotWhere)
    {
    dyStringAppend(query, " and ");
    }
else
    {
    dyStringAppend(query, " where ");
    *gotWhere = TRUE;
    }
}

boolean htiIsPositional(struct hTableInfo *hti)
/* Return TRUE if hti looks like it's from a positional table. */
{
return hti->chromField[0] && hti->startField[0] && hti->endField[0];
}


void doTabOutTable(
	char *database, 
	char *table, 
	struct sqlConnection *conn, 
	char *fields)
/* Do tab-separated output on fields of a single table. */
{
struct region *region, *regionList = NULL;
struct hTableInfo *hti = NULL;
struct dyString *fieldSpec = newDyString(256);
struct hash *idHash = NULL;
int outCount = 0;
boolean isPositional;

regionList = getRegions();
hti = getHti(database, table);

/* If they didn't pass in a field list assume they want all fields. */
if (fields != NULL)
    dyStringAppend(fieldSpec, fields);
else
    dyStringAppend(fieldSpec, "*");

/* If table has and identity (name) field, and user has
 * uploaded list of identifiers, create identifier hash
 * and add identifier column to end of result set. */
if (hti->nameField[0] != 0)
    {
    idHash = identifierHash();
    if (idHash != NULL)
	{
	dyStringAppend(fieldSpec, ",");
	dyStringAppend(fieldSpec, hti->nameField);
	}
    }
isPositional = htiIsPositional(hti);

/* Loop through each region. */
for (region = regionList; region != NULL; region = region->next)
    {
    struct sqlResult *sr;
    char **row;
    int colIx, colCount, lastCol;
    char chromTable[256];
    boolean gotWhere = FALSE;

    sr = regionQuery(conn, table, fieldSpec->string, region, isPositional);
    colCount = sqlCountColumns(sr);
    if (idHash != NULL)
        colCount -= 1;
    lastCol = colCount - 1;

    /* First time through print column names. */
    if (region == regionList)
        {
	hPrintf("#");
	for (colIx = 0; colIx < lastCol; ++colIx)
	    hPrintf("%s\t", sqlFieldName(sr));
	hPrintf("%s\n", sqlFieldName(sr));
	}
    while ((row = sqlNextRow(sr)) != NULL)
	{
	if (idHash == NULL || hashLookup(idHash, row[colCount]))
	    {
	    for (colIx = 0; colIx < lastCol; ++colIx)
		hPrintf("%s\t", row[colIx]);
	    hPrintf("%s\n", row[lastCol]);
	    ++outCount;
	    }
	}
    sqlFreeResult(&sr);
    if (!isPositional)
        break;	/* No need to iterate across regions in this case. */
    }

/* Do some error diagnostics for user. */
if (outCount == 0)
    {
    int regionCount = slCount(regionList);
    if (idHash != NULL)
	{
	if (regionCount <= 1)
	    hPrintf("No items in selected region matched identifier list.\n");
	else
	    hPrintf("No items matched identifier list");
	}
    else if (regionCount <= 1)
        {
	hPrintf("No items in selected region");
	}
    }
hashFree(&idHash);
}


void doOutPrimaryTable(struct trackDb *track, 
	struct sqlConnection *conn)
/* Dump out primary table. */
{
textOpen();
doTabOutTable(database, track->tableName, conn, NULL);
}

void doTopSubmit(struct sqlConnection *conn)
/* Respond to submit button on top level page.
 * This basically just dispatches based on output type. */
{
char *output = cartString(cart, hgtaOutputType);
char *trackName = cartString(cart, hgtaTrack);
struct trackDb *track = findTrack(trackName, fullTrackList);
if (track == NULL)
    errAbort("track %s doesn't exist in %s", trackName, database);
if (sameString(output, outPrimaryTable))
    doOutPrimaryTable(track, conn);
else if (sameString(output, outSchema))
    doTrackSchema(track, conn);
else if (sameString(output, outSelectedFields))
    doOutSelectedFields(track, conn);
else
    errAbort("Don't know how to handle %s output yet", output);
}

void doIntersect(struct sqlConnection *conn)
/* Respond to intersection button. */
{
htmlOpen("Table Browser Intersect");
hPrintf("Processing intersect button... not yet doing anything real...");
htmlClose();
}

void dispatch(struct sqlConnection *conn)
/* Scan for 'Do' variables and dispatch to appropriate page-generator.
 * By default head to the main page. */
{
struct hashEl *varList;
if (cartVarExists(cart, hgtaDoTest))
    doTest();
else if (cartVarExists(cart, hgtaDoTopSubmit))
    doTopSubmit(conn);
else if (cartVarExists(cart, hgtaDoIntersect))
    doIntersect(conn);
else if (cartVarExists(cart, hgtaDoPasteIdentifiers))
    doPasteIdentifiers(conn);
else if (cartVarExists(cart, hgtaDoPastedIdentifiers))
    doPastedIdentifiers(conn);
else if (cartVarExists(cart, hgtaDoUploadIdentifiers))
    doUploadIdentifiers(conn);
else if (cartVarExists(cart, hgtaDoClearIdentifiers))
    doClearIdentifiers(conn);
else if (cartVarExists(cart, hgtaDoFilterPage))
    doFilterPage(conn);
else if (cartVarExists(cart, hgtaDoFilterMore))
    doFilterMore(conn);
else if (cartVarExists(cart, hgtaDoFilterSubmit))
    doFilterSubmit(conn);
else if (cartVarExists(cart, hgtaDoClearFilter))
     doClearFilter(conn);
else if (cartVarExists(cart, hgtaDoSchema))
    {
    doTableSchema( cartString(cart, hgtaDoSchemaDb), 
    	cartString(cart, hgtaDoSchema), conn);
    }
else if (cartVarExists(cart, hgtaDoValueHistogram))
    doValueHistogram(cartString(cart, hgtaDoValueHistogram));
else if (cartVarExists(cart, hgtaDoValueRange))
    doValueRange(cartString(cart, hgtaDoValueRange));
else if (cartVarExists(cart, hgtaDoSelectFieldsMore))
    doSelectFieldsMore();
else if (cartVarExists(cart, hgtaDoPrintSelectedFields))
    doPrintSelectedFields();
else if ((varList = cartFindPrefix(cart, hgtaDoClearAllFieldPrefix)) != NULL)
    doClearAllField(varList->name + strlen(hgtaDoClearAllFieldPrefix));
else if ((varList = cartFindPrefix(cart, hgtaDoSetAllFieldPrefix)) != NULL)
    doSetAllField(varList->name + strlen(hgtaDoSetAllFieldPrefix));
else if (cartVarExists(cart, hgtaDoMainPage))
    doMainPage(conn);
else	/* Default - put up initial page. */
    doMainPage(conn);
cartRemovePrefix(cart, hgtaDo);
}

char *excludeVars[] = {"Submit", "submit", NULL};

void hgTables()
/* hgTables - Get table data associated with tracks and intersect tracks. */
{
struct sqlConnection *conn = NULL;
oldVars = hashNew(10);

/* Sometimes we output HTML and sometimes plain text; let each outputter 
 * take care of headers instead of using a fixed cart*Shell(). */
cart = cartAndCookieNoContent(hUserCookie(), excludeVars, oldVars);

/* Set up global variables. */
getDbAndGenome(cart, &database, &genome);
hSetDb(database);
conn = hAllocConn();
fullTrackList = hTrackDb(NULL);
curTrack = findSelectedTrack(fullTrackList, NULL);

/* Go figure out what page to put up. */
dispatch(conn);

/* Save variables. */
cartCheckout(&cart);
}

int main(int argc, char *argv[])
/* Process command line. */
{
htmlPushEarlyHandlers(); /* Make errors legible during initialization. */
pushCarefulMemHandler(100000000);
cgiSpoof(&argc, argv);
hgTables();
return 0;
}
