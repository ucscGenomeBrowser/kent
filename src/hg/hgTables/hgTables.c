/* hgTables - Main and utility functions for table browser. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "memalloc.h"
#include "obscure.h"
#include "htmshell.h"
#include "cheapcgi.h"
#include "cart.h"
#include "jksql.h"
#include "hdb.h"
#include "web.h"
#include "hui.h"
#include "hCommon.h"
#include "hgColors.h"
#include "trackDb.h"
#include "grp.h"
#include "customTrack.h"
#include "hgTables.h"
#include "joiner.h"

static char const rcsid[] = "$Id: hgTables.c,v 1.58 2004/09/03 16:57:18 hiram Exp $";


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
char *freezeName;	/* Date of assembly. */
struct trackDb *fullTrackList;	/* List of all tracks in database. */
struct trackDb *curTrack;	/* Currently selected track. */
char *curTable;		/* Currently selected table. */
struct joiner *allJoiner;	/* Info on how to join tables. */

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

void explainWhyNoResults()
/* Put up a little explanation to user of why they got nothing. */
{
hPrintf("# No results");
if (identifierFileName() != NULL)
    hPrintf(" matching identifier list");
if (anyFilter())
    hPrintf(" passing filter");
if (!fullGenomeRegion())
    hPrintf(" in given region");
if (anyIntersection())
    hPrintf(" after intersection");
hPrintf(".");
}

char *curTableLabel()
/* Return label for current table - track short label if it's a track */
{
if (sameString(curTrack->tableName, curTable))
    return curTrack->shortLabel;
else
    return curTable;
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

static struct trackDb *getFullTrackList()
/* Get all tracks including custom tracks if any. */
{
struct trackDb *list = hTrackDb(NULL);
struct customTrack *ctList, *ct;

/* Create dummy group for custom tracks if any */
ctList = getCustomTracks();
for (ct = ctList; ct != NULL; ct = ct->next)
    {
    slAddHead(&list, ct->tdb);
    }
return list;
}

boolean fullGenomeRegion()
/* Return TRUE if region is full genome. */
{
char *regionType = cartUsualString(cart, hgtaRegionType, "genome");
return sameString(regionType, "genome");
}

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
    if ((region->chrom = hgOfficialChromName(range)) == NULL)
	{
	if (!hgParseChromRange(range, &region->chrom, &region->start, &region->end))
	    {
	    errAbort("%s is not a chromosome range.  "
		     "Please go back and enter something like chrX:1000000-1100000 "
		     "in the range control.", range);
	    }
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
return regionList;
}

char *getRegionName()
/* Get a name for selected region.  Don't free this. */
{
char *region = cartUsualString(cart, hgtaRegionType, "genome");
if (sameString(region, "range"))
    region = cartUsualString(cart, hgtaRange, "n/a");
return region;
}

void regionFillInChromEnds(struct region *regionList)
/* Fill in end fields if set to zero to be whole chrom. */
{
struct region *region;
for (region = regionList; region != NULL; region = region->next)
    {
    if (region->end == 0)
        region->end = hChromSize(region->chrom);
    }
}


struct region *getRegionsWithChromEnds()
/* Get list of regions.  End field is set to chrom size rather
 * than zero for full chromosomes. */
{
struct region *regionList = getRegions();
regionFillInChromEnds(regionList);
return regionList;
}

struct sqlResult *regionQuery(struct sqlConnection *conn, char *table,
	char *fields, struct region *region, boolean isPositional,
	char *extraWhere)
/* Construct and execute query for table on region. */
{
struct sqlResult *sr;
if (isPositional)
    {
    if (region->end == 0) /* Full chromosome. */
	{
	sr = hExtendedChromQuery(conn, table, region->chrom, 
		extraWhere, FALSE, fields, NULL);
	}
    else
	{
	sr = hExtendedRangeQuery(conn, table, region->chrom, 
		region->start, region->end, 
		extraWhere, TRUE, fields, NULL);
	}
    }
else
    {
    struct dyString *query = dyStringNew(0);
    dyStringPrintf(query, "select %s from %s", fields, table);
    if (extraWhere)
         {
	 dyStringAppend(query, " where ");
	 dyStringAppend(query, extraWhere);
	 }
    sr = sqlGetResult(conn, query->string);
    dyStringFree(&query);
    }
return sr;
}

char *trackTable(char *rawTable)
/* Return table name for track, substituting all_mrna
 * for mRNA if need be. */
{
char *table = rawTable;
if (sameString(table, "mrna"))
    return "all_mrna";
else
    return table;
}

char *connectingTableForTrack(char *rawTable)
/* Return table name to use with all.joiner for track. 
 * You can freeMem this when done. */
{
if (sameString(rawTable, "mrna"))
    return cloneString("all_mrna");
else if (sameString(rawTable, "intronEst"))
    return cloneString("all_est");
else if (sameString(rawTable, "est"))
    return cloneString("all_est");
else 
    return cloneString(rawTable);
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

struct hTableInfo *maybeGetHti(char *db, char *table)
/* Return primary table info. */
{
struct hTableInfo *hti = NULL;

if (isCustomTrack(table))
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
if (group != NULL && sameString(group->name, "all"))
    group = NULL;
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

struct trackDb *mustFindTrack(char *name, struct trackDb *trackList)
/* Find track or squawk and die. */
{
struct trackDb *track = findTrack(name, trackList);
if (track == NULL)
    {
    if (isCustomTrack(name))
        errAbort("Can't find custom track %s. "
	         "If it's been 8 hours since you accessed this track you "
		 "may just need to upload it again.", name);
    else
	errAbort("Track %s doesn't exist in database %s.", name, database);
    }
return track;
}

struct trackDb *findSelectedTrack(struct trackDb *trackList, 
	struct grp *group, char *varName)
/* Find selected track - from CGI variable if possible, else
 * via various defaults. */
{
char *name = cartOptionalString(cart, varName);
struct trackDb *track = NULL;

if (name != NULL)
    track = findTrackInGroup(name, trackList, group);
if (track == NULL)
    {
    if (group == NULL || sameString(group->name, "all"))
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

char *findSelectedTable(struct trackDb *track, char *var)
/* Find selected table.  Default to main track table if none
 * found. */
{
if (isCustomTrack(track->tableName))
    return track->tableName;
else
    {
    char *table = cartUsualString(cart, var, track->tableName);
    struct joinerPair *jpList, *jp;
    if (sameString(track->tableName, table))
        return track->tableName;
    jpList = joinerRelate(allJoiner, database, track->tableName);
    for (jp = jpList; jp != NULL; jp = jp->next)
        {
	if (sameString(database, jp->b->database))
	    {
	    if (sameString(jp->b->table, table))
	        return table;
	    }
	else
	    {
	    char buf[256];
	    safef(buf, sizeof(buf), "%s.%s", jp->b->database, jp->b->table);
	    if (sameString(buf, table))
	        return table;
	    }
	}
    }
return track->tableName;
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

int countTableColumns(struct sqlConnection *conn, char *table)
/* Count columns in table. */
{
char *splitTable = chromTable(conn, table);
int count = sqlCountColumnsInTable(conn, splitTable);
freez(&splitTable);
return count;
}

static void doTabOutDb( char *db, char *table, 
	struct sqlConnection *conn, char *fields)
/* Do tab-separated output on fields of a single table. */
{
struct region *regionList = getRegions();
struct region *region;
struct hTableInfo *hti = NULL;
struct dyString *fieldSpec = newDyString(256);
struct hash *idHash = NULL;
int outCount = 0;
boolean isPositional;
boolean doIntersection;
char *filter = filterClause(db, table);
int fieldCount;
int bedFieldsOffset, bedFieldCount;

hti = getHti(db, table);

/* If they didn't pass in a field list assume they want all fields. */
if (fields != NULL)
    {
    dyStringAppend(fieldSpec, fields);
    fieldCount = countChars(fields, ',') + 1;
    }
else
    {
    dyStringAppend(fieldSpec, "*");
    fieldCount = countTableColumns(conn, table);
    }
bedFieldsOffset = fieldCount;

/* If table has and identity (name) field, and user has
 * uploaded list of identifiers, create identifier hash
 * and add identifier column to end of result set. */
if (hti->nameField[0] != 0)
    {
    idHash = identifierHash();
    if (idHash != NULL)
	{
	dyStringAppendC(fieldSpec, ',');
	dyStringAppend(fieldSpec, hti->nameField);
	bedFieldsOffset += 1;
	}
    }
isPositional = htiIsPositional(hti);

/* If intersecting add fields needed to calculate bed as well. */
doIntersection = (anyIntersection() && isPositional);
if (doIntersection)
    {
    char *bedFields;
    bedSqlFieldsExceptForChrom(hti, &bedFieldCount, &bedFields);
    dyStringAppendC(fieldSpec, ',');
    dyStringAppend(fieldSpec, bedFields);
    freez(&bedFields);
    }

/* Loop through each region. */
for (region = regionList; region != NULL; region = region->next)
    {
    struct sqlResult *sr;
    char **row;
    int colIx, lastCol = fieldCount-1;
    char chromTable[256];
    boolean gotWhere = FALSE;

    sr = regionQuery(conn, table, fieldSpec->string, 
    	region, isPositional, filter);

    /* First time through print column names. */
    if (region == regionList)
        {
	if (filter != NULL)
	    hPrintf("#filter: %s\n", filter);
	hPrintf("#");
	for (colIx = 0; colIx < lastCol; ++colIx)
	    hPrintf("%s\t", sqlFieldName(sr));
	hPrintf("%s\n", sqlFieldName(sr));
	}
    while ((row = sqlNextRow(sr)) != NULL)
	{
	if (idHash == NULL || hashLookup(idHash, row[fieldCount]))
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
    explainWhyNoResults();
freez(&filter);
hashFree(&idHash);
}

void doTabOutTable( char *db, char *table, struct sqlConnection *conn, char *fields)
/* Do tab-separated output on fields of a single table. */
{
if (isCustomTrack(table))
    {
    struct trackDb *track = findTrack(table, fullTrackList);
    doTabOutCustomTracks(track, conn, fields);
    }
else
    {
    doTabOutDb(db, table, conn, fields);
    }
}

void doOutPrimaryTable(char *table, 
	struct sqlConnection *conn)
/* Dump out primary table. */
{
textOpen();
doTabOutTable(database, trackTable(table), conn, NULL);
}

void doOutHyperlinks(char *table, struct sqlConnection *conn)
/* Output as genome browser hyperlinks. */
{
char *table2 = cartOptionalString(cart, hgtaIntersectTrack);
struct region *region, *regionList = getRegions();
char posBuf[64];
int count = 0;

htmlOpen("Hyperlinks to Genome Browser");
for (region = regionList; region != NULL; region = region->next)
    {
    struct lm *lm = lmInit(64*1024);
    struct bed *bedList, *bed;
    bedList = cookedBedList(conn, table, region, lm);
    for (bed = bedList; bed != NULL; bed = bed->next)
	{
	char *name;
	safef(posBuf, sizeof(posBuf), "%s:%d-%d",
		    bed->chrom, bed->chromStart+1, bed->chromEnd);
	/* Construct browser anchor URL with tracks we're looking at open. */
	hPrintf("<A HREF=\"%s?%s", hgTracksName(), cartSidUrlString(cart));
	hPrintf("&db=%s", database);
	hPrintf("&position=%s", posBuf);
	hPrintf("&%s=%s", table, hTrackOpenVis(table));
	if (table2 != NULL)
	    hPrintf("&%s=%s", table2, hTrackOpenVis(table2));
	hPrintf("\" TARGET=_blank>");
	name = bed->name;
	if (bed->name == NULL)
	    name = posBuf;
	if (sameString(name, posBuf))
	    hPrintf("%s", posBuf);
	else
	    hPrintf("%s at %s", name, posBuf);
	hPrintf("</A><BR>\n");
	++count;
	}
    lmCleanup(&lm);
    }
if (count == 0)
    hPrintf("\n# No results returned from query.\n\n");
htmlClose();
}

void doTopSubmit(struct sqlConnection *conn)
/* Respond to submit button on top level page.
 * This basically just dispatches based on output type. */
{
char *output = cartString(cart, hgtaOutputType);
char *trackName = cartString(cart, hgtaTrack);
char *table = cartString(cart, hgtaTable);
struct trackDb *track = mustFindTrack(trackName, fullTrackList);
if (sameString(output, outPrimaryTable))
    doOutPrimaryTable(table, conn);
else if (sameString(output, outSelectedFields))
    doOutSelectedFields(table, conn);
else if (sameString(output, outSequence))
    doOutSequence(conn);
else if (sameString(output, outBed))
    doOutBed(table, conn);
else if (sameString(output, outCustomTrack))
    doOutCustomTrack(table, conn);
else if (sameString(output, outGff))
    doOutGff(table, conn);
else if (sameString(output, outHyperlinks))
    doOutHyperlinks(table, conn);
else if (sameString(output, outWigData))
    doOutWigData(track, conn);
else if (sameString(output, outWigBed))
    doOutWigBed(track, conn);
else
    errAbort("Don't know how to handle %s output yet", output);
}

void dispatch(struct sqlConnection *conn)
/* Scan for 'do' variables and dispatch to appropriate page-generator.
 * By default head to the main page. */
{
struct hashEl *varList;
if (cartVarExists(cart, hgtaDoTest))
    doTest();
else if (cartVarExists(cart, hgtaDoTopSubmit))
    doTopSubmit(conn);
else if (cartVarExists(cart, hgtaDoSummaryStats))
    doSummaryStats(conn);
else if (cartVarExists(cart, hgtaDoSchema))
    doSchema(conn);
else if (cartVarExists(cart, hgtaDoIntersectPage))
    doIntersectPage(conn);
else if (cartVarExists(cart, hgtaDoClearIntersect))
    doClearIntersect(conn);
else if (cartVarExists(cart, hgtaDoIntersectMore))
    doIntersectMore(conn);
else if (cartVarExists(cart, hgtaDoIntersectSubmit))
    doIntersectSubmit(conn);
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
else if (cartVarExists(cart, hgtaDoSchemaTable))
    {
    doTableSchema( cartString(cart, hgtaDoSchemaDb), 
    	cartString(cart, hgtaDoSchemaTable), conn);
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
else if (cartVarExists(cart, hgtaDoGenePredSequence))
    doGenePredSequence(conn);
else if (cartVarExists(cart, hgtaDoGenomicDna))
    doGenomicDna(conn);
else if (cartVarExists(cart, hgtaDoGetBed))
    doGetBed(conn);
else if (cartVarExists(cart, hgtaDoGetCustomTrack))
    doGetCustomTrack(conn);
else if (cartVarExists(cart, hgtaDoGetCustomTrackFile))
    doGetCustomTrackFile(conn);
else if (cartVarExists(cart, hgtaDoMainPage))
    doMainPage(conn);
else	/* Default - put up initial page. */
    doMainPage(conn);
cartRemovePrefix(cart, hgtaDo);
}

char *excludeVars[] = {"Submit", "submit", NULL};

void hgTables()
/* hgTables - Get table data associated with tracks and intersect tracks. 
 * Here we set up cart and some global variables, dispatch the command,
 * and put away the cart when it is done. */
{
struct sqlConnection *conn = NULL;
oldVars = hashNew(10);

/* Sometimes we output HTML and sometimes plain text; let each outputter 
 * take care of headers instead of using a fixed cart*Shell(). */
cart = cartAndCookieNoContent(hUserCookie(), excludeVars, oldVars);

/* Set up global variables. */
allJoiner = joinerRead("all.joiner");
getDbAndGenome(cart, &database, &genome);
hSetDb(database);
freezeName = hFreezeFromDb(database);
conn = hAllocConn();
fullTrackList = getFullTrackList();
curTrack = findSelectedTrack(fullTrackList, NULL, hgtaTrack);
curTable = findSelectedTable(curTrack, hgtaTable);

/* Go figure out what page to put up. */
dispatch(conn);

/* Save variables. */
cartCheckout(&cart);
}

int main(int argc, char *argv[])
/* Process command line. */
{
htmlPushEarlyHandlers(); /* Make errors legible during initialization. */
pushCarefulMemHandler(500000000);
cgiSpoof(&argc, argv);
hgTables();
return 0;
}
