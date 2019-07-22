/* hgTables - Main and utility functions for table browser. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "memalloc.h"
#include "obscure.h"
#include "htmshell.h"
#include "cheapcgi.h"
#include "cart.h"
#include "cartTrackDb.h"
#include "textOut.h"
#include "jksql.h"
#include "hdb.h"
#include "web.h"
#include "hui.h"
#include "hCommon.h"
#include "hgColors.h"
#include "trackDb.h"
#include "botDelay.h"
#include "grp.h"
#include "customTrack.h"
#include "pipeline.h"
#include "hgFind.h"
#include "hgTables.h"
#include "joiner.h"
#include "bedCart.h"
#include "hgMaf.h"
#include "gvUi.h"
#include "wikiTrack.h"
#include "trackHub.h"
#include "hubConnect.h"
#include "hgConfig.h"
#include "udc.h"
#include "chromInfo.h"
#include "knetUdc.h"
#include "trashDir.h"
#include "genbank.h"
#include "windowsToAscii.h"

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
char *database;		/* Current genome database - hg17, mm5, etc. */
char *freezeName;	/* Date of assembly. */
struct grp *fullGroupList;	/* List of all groups. */
struct grp *curGroup;	/* Currently selected group. */
struct trackDb *fullTrackList;	/* List of all tracks in database. */
struct hash *fullTableToTdbHash;        /* All tracks and subtracks keyed by tdb->table field. */
struct trackDb *curTrack;	/* Currently selected track. */
char *curTable;		/* Currently selected table. */
struct joiner *allJoiner;	/* Info on how to join tables. */

static struct pipeline *compressPipeline = (struct pipeline *)NULL;

char *gsTemp = NULL;
int saveStdout = -1;

/* --------------- HTML Helpers ----------------- */

void hPrintSpaces(int count)
/* Print a number of non-breaking spaces. */
{
int i;
for (i=0; i<count; ++i)
    hPrintf("&nbsp;");
}

static void stripHtmlTags(char *text)
/* remove HTML tags from text string, replacing in place by moving
 * the text up to take their place
 */
{
char *s = text;
char *e = text;
char c = *text;
for ( ; c != 0 ; )
    {
    c = *s++;
    if (c == 0)
	/*	input string is NULL, or it ended with '>' without any
	 *	opening '>'
	 */
	{
	*e = 0;
	break;
	}
    /* stays in the while loop for adjacent tags <TR><TD> ... etc */
    while (c == '<' && c != 0)
	{
	s = strchr(s,'>');
	if (s != NULL)
	    {
	    if (*s == '>') ++s; /* skip closing bracket > */
	    c = *s++;		/* next char after the closing bracket > */
	    }
	else
	    c = 0;	/* no closing bracket > found, end of string */
	}
    *e++ = c;	/*	copies all text outside tags, including ending NULL */
    }
}

void writeHtmlCell(char *text)
/* Write out a cell in an HTML table, making text not too big,
 * and stripping html tags and breaking spaces.... */
{
int maxLen = 128;
int len = strlen(text);
char *extra = "";
if (len > maxLen)
    {
    len = maxLen;
    extra = "&nbsp;...";
    }
char *s = cloneStringZ(text,len);
char *r;
stripHtmlTags(s);
eraseTrailingSpaces(s);
r = replaceChars(s, " ", "&nbsp;");
hPrintf("<TD>%s%s</TD>", r, extra);
freeMem(s);
freeMem(r);
}

static void earlyAbortHandler(char *format, va_list args)
{
// provide more explicit message when we run out of memory (#5147).
popWarnHandler();
if(strstr(format, "needLargeMem:") || strstr(format, "carefulAlloc:"))
    format = "Region selected is too large for calculation. Please specify a smaller region or try limiting to fewer data points.";
vaWarn(format, args);
if(isErrAbortInProgress())
    noWarnAbort();
}

static void errAbortHandler(char *format, va_list args)
{
// provide more explicit message when we run out of memory (#5147).
if(strstr(format, "needLargeMem:") || strstr(format, "carefulAlloc:"))
    htmlVaWarn("Region selected is too large for calculation. Please specify a smaller region or try limiting to fewer data points.", args);
else
    {
    // call previous handler
    popWarnHandler();
    vaWarn(format, args);
    }
if(isErrAbortInProgress())
    noWarnAbort();
}

static void vaHtmlOpen(char *format, va_list args)
/* Start up a page that will be in html format. */
{
puts("Content-Type:text/html\n");
cartVaWebStart(cart, database, format, args);
pushWarnHandler(errAbortHandler);
}

void htmlOpen(char *format, ...)
/* Start up a page that will be in html format. */
{
va_list args;
va_start(args, format);
vaHtmlOpen(format, args);
va_end(args);
hgBotDelay();
}

void htmlClose()
/* Close down html format page. */
{
popWarnHandler();
cartWebEnd();
}


void explainWhyNoResults(FILE *f)
/* Put up a little explanation to user of why they got nothing. */
{
if (f == NULL)
    f = stdout;
fprintf(f, "# No results");
if (identifierFileName() != NULL)
    fprintf(f, " matching identifier list");
if (anyFilter())
    fprintf(f, " passing filter");
if (!fullGenomeRegion())
    fprintf(f, " in given region");
if (anyIntersection())
    fprintf(f, " after intersection");
fprintf(f, ".\n");
}

char *curTableLabel()
/* Return label for current table - track short label if it's a track */
{
if (curTrack != NULL && sameString(curTrack->table, curTable))
    return curTrack->shortLabel;
else
    return curTable;
}

char *getScriptName()
/* returns script name from environment or hardcoded for command line */
{
char *script = cgiScriptName();
if (script != NULL)
    return script;
else
    return hgTablesName();
}


void textOpen()
/* Start up page in text format. (No need to close this).
 *	In case of pipeline output to a compressor, it is closed
 *	at main() exit.
 */
{
hgBotDelayNoWarn();  // delay but suppress warning at 10-20 sec delay level because this is not html output.
char *fileName = textOutSanitizeHttpFileName(cartUsualString(cart, hgtaOutFileName, ""));
char *compressType = cartUsualString(cart, hgtaCompressType,
				     textOutCompressNone);

if (doGenomeSpace())
    {
    char hgsid[64];
    struct tempName tn;
    safef(hgsid, sizeof(hgsid), "%s", cartSessionId(cart));
    trashDirFile(&tn, "genomeSpace", hgsid, ".tmp");
    gsTemp = cloneString(tn.forCgi);
    fileName = gsTemp;
    }

compressPipeline = textOutInit(fileName, compressType, &saveStdout);
}


void dbOverrideFromTable(char buf[256], char **pDb, char **pTable)
/* If *pTable includes database, overrider *pDb with it, using
 * buf to hold string. */
{
char *s;
safef(buf, 256, "%s", *pTable);
s = strchr(buf, '.');
if (s != NULL)
    {
    *pDb = buf;
    *s++ = 0;
    *pTable = s;
    }
}

boolean fullGenomeRegion()
/* Return TRUE if region is full genome. */
{
char *regionType = cartUsualString(cart, hgtaRegionType, "genome");
return sameString(regionType, "genome");
}

boolean isNoGenomeDisabled(char *db, char *table)
/* Return TRUE if table (or a track with which it is associated) has 'tableBrowser noGenome'
 * and region is genome. */
{
return (fullGenomeRegion() && cartTrackDbIsNoGenome(db, table));
}

void checkNoGenomeDisabled(char *db, char *table)
/* Before producing output, make sure that the URL hasn't been hacked to make a
 * genome-wide query on a noGenome table. */
{
if (isNoGenomeDisabled(db, table))
    errAbort("Can't do genome-wide query on %s.  "
             "Please go back and choose a position range.", table);
}

static int regionCmp(const void *va, const void *vb)
/* Compare to sort based on chrom,start */
{
const struct region *a = *((struct region **)va);
const struct region *b = *((struct region **)vb);
int dif;
dif = chrStrippedCmp(a->chrom, b->chrom);
if (dif == 0)
    dif = a->start - b->start;
return dif;
}

static struct region *getRegionsFullGenomeLocal()
/* get all the chrom ranges for a local database */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
struct region *region, *regionList = NULL;

sr = sqlGetResult(conn, NOSQLINJ "select chrom,size from chromInfo");
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(region);
    region->chrom = cloneString(row[0]);
    region->end = sqlUnsigned(row[1]);
    region->fullChrom = TRUE;
    region->name = NULL;		/* unused for full chrom */
    slAddHead(&regionList, region);
    }
slSort(&regionList, regionCmp);
sqlFreeResult(&sr);
hFreeConn(&conn);
return regionList;
}

static struct region *getRegionsFullGenomeHub()
/* get all the chrom ranges for a hub database */
{
struct chromInfo *ci = trackHubAllChromInfo(database);
struct region *region, *regionList = NULL;

AllocVar(region);
for(; ci; ci = ci->next)
    {
    AllocVar(region);
    region->chrom = cloneString(ci->chrom);
    region->end = ci->size;
    region->fullChrom = TRUE;
    region->name = NULL;		/* unused for full chrom */
    slAddHead(&regionList, region);
    }
slSort(&regionList, regionCmp);
return regionList;
}

struct region *getRegionsFullGenome()
/* Get a region list that covers all of each chromosome. */
{
if (trackHubDatabase(database))
    return getRegionsFullGenomeHub();
return getRegionsFullGenomeLocal();
}

struct region *getEncodeRegions()
/* Get encode regions from encodeRegions table. */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
struct region *list = NULL, *region;

sr = sqlGetResult(conn, NOSQLINJ "select chrom,chromStart,chromEnd,name from encodeRegions order by name desc");
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(region);
    region->chrom = cloneString(row[0]);
    region->start = atoi(row[1]);
    region->end = atoi(row[2]);
    region->name = cloneString(row[3]);	/* encode region name	*/
    slAddHead(&list, region);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
return list;
}

struct hgPositions *lookupPosition(struct dyString *dyWarn)
/* Look up position (aka range) if need be.  Return a container of matching tables and positions.
 * Warnings/errors are appended to dyWarn. */
{
char *range = windowsToAscii(cloneString(cartUsualString(cart, hgtaRange, "")));
range = trimSpaces(range);
if (isEmpty(range))
    range = hDefaultPos(database);
struct hgPositions *hgp = hgFindSearch(cart, &range, NULL, NULL, NULL, getScriptName(), dyWarn);
cartSetString(cart, hgtaRange, range);
return hgp;
}

struct region *getRegions()
/* Consult cart to get list of regions to work on. */
{
char *regionType = cartUsualString(cart, hgtaRegionType, "genome");
struct region *regionList = NULL, *region;
if (sameString(regionType, hgtaRegionTypeGenome))
    {
    regionList = getRegionsFullGenome();
    }
else if (sameString(regionType, hgtaRegionTypeRange))
    {
    char *range = cartString(cart, hgtaRange);
    boolean parseOk = FALSE;
    regionList = AllocVar(region);
    if (! strchr(range, ':'))
	parseOk = ((region->chrom = hgOfficialChromName(database, range)) != NULL);
    else
	parseOk = hgParseChromRange(database, range, &region->chrom, &region->start,
				    &region->end);
    if (! parseOk)
	{
	errAbort("Bad position %s, please enter something else in position box",
		 range);
	}
    region->name = NULL;	/*	unused for range	*/
    }
else if (sameString(regionType, hgtaRegionTypeEncode))
    {
    regionList = getEncodeRegions();
    }
else if (sameString(regionType, hgtaRegionTypeUserRegions) &&
	(NULL != userRegionsFileName()))
    {
    regionList = getUserRegions(userRegionsFileName());
    }
else
    {
    regionList = getRegionsFullGenome();
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

struct sqlResult *regionQuery(struct sqlConnection *conn, char *table,
	char *fields, struct region *region, boolean isPositional,
	char *extraWhere)
/* Construct and execute query for table on region. Returns NULL if
 * table doesn't exist (e.g. missing split table for region->chrom). */
{
struct sqlResult *sr;
if (isPositional)
    {
    /* Check for missing split tables before querying: */
    char *db = sqlGetDatabase(conn);
    struct hTableInfo *hti = hFindTableInfo(db, region->chrom, table);
    if (hti == NULL)
	return NULL;
    else if (hti->isSplit)
	{
	char fullTableName[256];
	safef(fullTableName, sizeof(fullTableName),
	      "%s_%s", region->chrom, table);
	if (!sqlTableExists(conn, fullTableName))
	    return NULL;
	}
    if (region->fullChrom) /* Full chromosome. */
	{
	sr = hExtendedChromQuery(conn, table, region->chrom,
		extraWhere, FALSE, fields, NULL);
	}
    else
	{
	sr = hExtendedRangeQuery(conn, table, region->chrom, region->start, region->end,
		extraWhere, TRUE, fields, NULL);
	}
    }
else
    {
    struct dyString *query = dyStringNew(0);
    sqlDyStringPrintf(query, "select %-s from %s", sqlCkIl(fields), table);
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

char *getDbTable(char *db, char *table)
/* If table already contains its real database as a dot-prefix, then
 * return a clone of table; otherwise alloc and return db.table . */
{
if (sameString(table, WIKI_TRACK_TABLE))
    {
    char dbTable[256];
    safef(dbTable, sizeof(dbTable), "%s.%s", wikiDbName(), WIKI_TRACK_TABLE);
    return cloneString(dbTable);
    }
else if (strchr(table, '.') != NULL)
    return cloneString(table);
else
    {
    char dbTable[256];
    safef(dbTable, sizeof(dbTable), "%s.%s", db, table);
    return cloneString(dbTable);
    }
}


char *connectingTableForTrack(char *rawTable)
/* Return table name to use with all.joiner for track.
 * You can freeMem this when done. */
{
return cloneString(rawTable);
}

char *chromTable(struct sqlConnection *conn, char *table)
/* Get chr1_table if it exists, otherwise table.
 * You can freeMem this when done. */
{
if (isHubTrack(table))
    return cloneString(table);
else
    {
    char *chrom = hDefaultChrom(database);
    if (sqlTableExists(conn, table))
	return cloneString(table);
    else
	{
	char buf[256];
	safef(buf, sizeof(buf), "%s_%s", chrom, table);
	return cloneString(buf);
	}
    }
}

struct hTableInfo *hubTrackTableInfo(struct trackDb *tdb)
/* Given trackDb entry for a hub track, wrap table info around it. */
{
struct hTableInfo *hti = NULL;
if (tdb->subtracks == NULL)
    {
    if (startsWithWord("bigBed", tdb->type) || startsWithWord("bigGenePred", tdb->type) ||
        startsWithWord("bigNarrowPeak", tdb->type))
	hti = bigBedToHti(tdb->table, NULL);
    else if (startsWithWord("longTabix", tdb->type))
	hti = longTabixToHti(tdb->table);
    else if (startsWithWord("bam", tdb->type))
	hti = bamToHti(tdb->table);
    else if (startsWithWord("vcfTabix", tdb->type))
	hti = vcfToHti(tdb->table, TRUE);
    else if (sameWord("hic", tdb->type))
	hti = hicToHti(tdb->table);
    }
if (hti == NULL)
    {
    AllocVar(hti);
    hti->rootName = cloneString(tdb->track);
    hti->isPos = TRUE;
    hti->type = cloneString(tdb->type);
    }
return hti;
}

struct hTableInfo *maybeGetHti(char *db, char *table, struct sqlConnection *conn)
/* Return primary table info, but don't abort if table not there. Conn should be open to db. */
{
struct hTableInfo *hti = NULL;
boolean isTabix = FALSE;

if (isHubTrack(table))
    {
    struct trackDb *tdb = hashMustFindVal(fullTableToTdbHash, table);
    hti = hubTrackTableInfo(tdb);
    }
else if (isBigBed(database, table, curTrack, ctLookupName))
    hti = bigBedToHti(table, conn);
else if (isBigWigTable(table))
    hti = bigWigToHti(table);
else if (isLongTabixTable(table))
    hti = longTabixToHti(table);
else if (isBamTable(table))
    hti = bamToHti(table);
else if (isVcfTable(table, &isTabix))
    {
    boolean isTabix = trackIsType(database, table, curTrack, "vcfTabix", ctLookupName);
    hti = vcfToHti(table, isTabix);
    }
else if (isHicTable(table))
    hti = hicToHti(table);
else if (isCustomTrack(table))
    {
    struct customTrack *ct = ctLookupName(table);
    hti = ctToHti(ct);
    }
else if (sameWord(table, WIKI_TRACK_TABLE))
    {
    hti = wikiHti();
    }
else
    {
    char *track;
    if (startsWith("chrN_", table))
	track = table + strlen("chrN_");
    else
	track = table;
    hti = hFindTableInfo(db, NULL, track);
    }
return(hti);
}

struct hTableInfo *getHti(char *db, char *table, struct sqlConnection *conn)
/* Return primary table info. */
{
struct hTableInfo *hti = maybeGetHti(db, table, conn);

if (hti == NULL)
    {
    errAbort("Could not find table info for table %s in db %s",
	     table, db);
    }
return(hti);
}

struct hTableInfo *getHtiOnDb(char *db, char *table)
/* Return primary table info. */
{
struct sqlConnection *conn = hAllocConn(db);
struct hTableInfo *hti = getHti(db, table, conn);
hFreeConn(&conn);
return hti;
}

struct hTableInfo *maybeGetHtiOnDb(char *db, char *table)
/* Return primary table info, but don't abort if table not there. */
{
struct sqlConnection *conn = NULL;

if (!trackHubDatabase(database))
    conn = hAllocConn(db);
struct hTableInfo *hti = maybeGetHti(db, table, conn);
hFreeConn(&conn);
return hti;
}


boolean isPositional(char *db, char *table)
/* Return TRUE if it looks to be a positional table. */
{
boolean result = FALSE;
struct sqlConnection *conn = hAllocConn(db);
if (sqlTableExists(conn, "chromInfo"))
    {
    char chromName[64];
    struct hTableInfo *hti;
    sqlQuickQuery(conn, NOSQLINJ "select chrom from chromInfo limit 1",
	chromName, sizeof(chromName));
    hti = hFindTableInfo(db, chromName, table);
    if (hti != NULL)
	{
	result = htiIsPositional(hti);
	}
    }
hFreeConn(&conn);
return result;
}

boolean isSqlStringType(char *type)
/* Return TRUE if type is a stringish SQL type. */
{
return strstr(type, "char") || strstr(type, "text")
       || strstr(type, "blob");
}

boolean isSqlEnumType(char *type)
/* Return TRUE if type is an enum. */
{
return startsWith("enum", type);
}

boolean isSqlSetType(char *type)
/* Return TRUE if type is a set. */
{
return startsWith("set", type);
}

boolean isSqlNumType(char *type)
/* Return TRUE if it is a numerical SQL type. */
{
return strstr(type, "int") || strstr(type, "float") || strstr(type, "double");
}

boolean isSqlIntType(char *type)
/* Return TRUE if it is an integer SQL type. */
{
return (strstr(type, "int") != NULL);
}

static struct trackDb *findTrackInGroup(char *name, struct trackDb *trackList,
	struct grp *group)
/* Find named track that is in group (NULL for any group).
 * Return NULL if can't find it. */
{
struct trackDb *track;
if (group != NULL && sameString(group->name, "all"))
    group = NULL;
for (track = trackList; track != NULL; track = track->next)
    {
    if (sameString(name, track->track) &&
       (group == NULL || sameString(group->name, track->grp)))
       return track;
    if (track->subtracks)
        {
	struct trackDb *subtrack = findTrackInGroup(name, track->subtracks, group);
	if (subtrack != NULL)
            // Return composite track if given a subtrack name (e.g. hg19 refGene track to
            // hg38 refSeqComposite, #19920)
	    return track;
	}
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
    {
    track = findTrackInGroup(name, trackList, group);
    }
if (track == NULL)
    {
    if (group == NULL || sameString(group->name, "all"))
        track = trackList;
    else
	{
	for (track = trackList; track != NULL; track = track->next)
	    if (sameString(track->grp, group->name))
	         break;
	}
    }
return track;
}

static struct grp *findGroup(struct grp *groupList, char *name)
/* Return named group in list, or NULL if not found. */
{
struct grp *group;
for (group = groupList; group != NULL; group = group->next)
    if (sameString(name, group->name))
        return group;
return NULL;
}

struct grp *findSelectedGroup(struct grp *groupList, char *cgiVar)
/* Find user-selected group if possible.  If not then
 * go to various levels of defaults. */
{
char *defaultGroup = "genes";
char *name = cartUsualString(cart, cgiVar, defaultGroup);
struct grp *group = findGroup(groupList, name);
if (group == NULL)
    group = findGroup(groupList, defaultGroup);
if (group == NULL)
    group = groupList;
return group;
}

static char *findSelectedTable(struct trackDb *track, char *var)
/* Find selected table.  Default to main track table if none
 * found. */
{
if (track == NULL)
    return cartString(cart, var);
else if (isCustomTrack(track->table))
    return track->table;
else
    {
    struct slName *tableList = cartTrackDbTablesForTrack(database, track, TRUE);
    char *table = cartUsualString(cart, var, tableList->name);
    if (slNameInList(tableList, table))
	return table;
    return tableList->name;
    }
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
return isCustomTrack(hti->rootName) || hti->isPos;
#ifdef OLD
    ((hti->startField[0] && hti->endField[0]) &&
	(hti->chromField[0] || sameString(hti->rootName, "gl")));
#endif /* OLD */
}

char *getIdField(char *db, struct trackDb *track, char *table,
	struct hTableInfo *hti)
/* Get ID field for table, or NULL if none.  FreeMem result when done */
{
char *idField = NULL;
if (hti != NULL && hti->nameField[0] != 0)
    idField = cloneString(hti->nameField);
else if (track != NULL && !tdbIsComposite(track))
    {
    struct hTableInfo *trackHti = maybeGetHtiOnDb(db, track->table);
    if (trackHti != NULL && isCustomTrack(table))
        idField = cloneString(trackHti->nameField);
    else if (hti != NULL && trackHti != NULL && trackHti->nameField[0] != 0)
        {
        struct joinerPair *jp, *jpList;
        jpList = joinerRelate(allJoiner, db, track->table, NULL);
        for (jp = jpList; jp != NULL; jp = jp->next)
            {
            if (sameString(jp->a->field, trackHti->nameField))
                {
                if ( sameString(jp->b->database, db)
                && sameString(jp->b->table, table) )
                    {
                    idField = cloneString(jp->b->field);
                    break;
                    }
                }
            }
        joinerPairFreeList(&jpList);
        }
    }
/* If we haven't found the answer but this looks like a non-positional table,
 * use the first field. */
if (idField == NULL && !isCustomTrack(table) && (hti == NULL || !hti->isPos))
    {
    struct sqlConnection *conn = track ? hAllocConnTrack(db, track) : hAllocConn(db);
    struct slName *fieldList = sqlListFields(conn, table);
    if (fieldList == NULL)
        errAbort("getIdField: Can't find fields of table %s", table);
    idField = cloneString(fieldList->name);
    slFreeList(&fieldList);
    hFreeConn(&conn);
    }
return idField;
}

int countTableColumns(struct sqlConnection *conn, char *table)
/* Count columns in table. */
{
char *splitTable = chromTable(conn, table);
int count = sqlCountColumnsInTable(conn, splitTable);
freez(&splitTable);
return count;
}

#if defined(__GNUC__)
static void hOrFPrintf(FILE *f, char *format, ...)
__attribute__((format(printf, 2, 3)));
#endif

static void hOrFPrintf(FILE *f, char *format, ...)
/* fprintf if f is non-null, else hPrintf. */
{
va_list(args);
va_start(args, format);
if (f != NULL)
    {
    vfprintf(f, format, args);
    if (ferror(f))
	noWarnAbort();
    }
else
    hvPrintf(format, args);
va_end(args);
}



static void itemRgbDataOut(FILE *f, char **row, int lastCol, int itemRgbCol)
/*	display bed data, show "reserved" column as r,g,b	*/
{
int colIx;
int rgb;

/*	Print up to itemRgbCol	*/
for (colIx = 0; (colIx < itemRgbCol) && (colIx < lastCol); ++colIx)
    hOrFPrintf(f, "%s\t", row[colIx]);

/*	Print out the itemRgbCol column	*/
rgb = atoi(row[itemRgbCol]);
hOrFPrintf(f, "%d,%d,%d", (rgb & 0xff0000) >> 16,
	(rgb & 0xff00) >> 8, (rgb & 0xff));

/*	Print the rest if there are any	*/
if (itemRgbCol < lastCol)
    {
    hOrFPrintf(f, "\t");
    for (colIx = itemRgbCol+1; colIx < lastCol; ++colIx)
	hOrFPrintf(f, "%s\t", row[colIx]);
    hOrFPrintf(f, "%s\n", row[lastCol]);
    }
else
    hOrFPrintf(f, "\n");	/*	itemRgbCol was the last column	*/
}

static int itemRgbHeader(FILE *f, struct sqlResult *sr, int lastCol)
/*	print out bed header, recognize "reserved" column, return which
 *	column it is, or -1 if not found
 */
{
int colIx;
int ret = -1;
char *field = sqlFieldName(sr);
for (colIx = 0; colIx < lastCol; ++colIx)
    {
    if (sameWord("reserved",field))
	{
	hOrFPrintf(f, "itemRgb\t");
	ret = colIx;
	}
    else
	hOrFPrintf(f, "%s\t", field);
    field = sqlFieldName(sr);
    }
if (sameWord("reserved",field))
    {
    hOrFPrintf(f, "itemRgb\n");
    ret = lastCol;
    }
else
    hOrFPrintf(f, "%s\n", field);

return(ret);
}

void doTabOutDb( char *db, char *dbVarName, char *table, char *tableVarName,
	FILE *f, struct sqlConnection *conn, char *fields)
/* Do tab-separated output on fields of a single table. */
{
struct region *regionList = getRegions();
struct region *region;
struct hTableInfo *hti = NULL;
struct dyString *fieldSpec = newDyString(256);
struct hash *idHash = NULL;
int outCount = 0;
boolean isPositional;
int fieldCount;
char *idField;
boolean showItemRgb = FALSE;
int itemRgbCol = -1;	/*	-1 means not found	*/
boolean printedColumns = FALSE;
struct trackDb *tdb = findTdbForTable(db, curTrack, table, ctLookupName);

hti = getHti(db, table, conn);
idField = getIdField(db, curTrack, table, hti);

showItemRgb=bedItemRgb(tdb);	/* should we expect itemRgb instead of "reserved" */

/* If they didn't pass in a field list assume they want all fields. */
if (fields != NULL)
    {
    dyStringAppend(fieldSpec, sqlCkIl(fields));
    fieldCount = countChars(fields, ',') + 1;
    }
else
    {
    dyStringAppend(fieldSpec, "*");
    fieldCount = countTableColumns(conn, table);
    }

/* If can find id field for table then get
 * uploaded list of identifiers, create identifier hash
 * and add identifier column to end of result set. */
char *identifierFilter = NULL;
if (idField != NULL)
    {
    idHash = identifierHash(db, table);
    if (idHash != NULL)
	{
	identifierFilter = identifierWhereClause(idField, idHash);
	if (isEmpty(identifierFilter))
	    {
	    dyStringAppendC(fieldSpec, ',');
	    dyStringAppend(fieldSpec, sqlCkId(idField));
	    }
	}
    }
isPositional = htiIsPositional(hti);

/* Loop through each region. */
for (region = regionList; region != NULL; region = region->next)
    {
    struct sqlResult *sr;
    char **row;
    int colIx, lastCol = fieldCount-1;
    char *filter = filterClause(dbVarName, tableVarName, region->chrom, identifierFilter);

    sr = regionQuery(conn, table, fieldSpec->string,
    	region, isPositional, filter);
    if (sr == NULL)
	continue;

    /* First time through print column names. */
    if (! printedColumns)
        {
	// Show only the SQL filter built from filter page options, not identifierFilter,
	// because identifierFilter can get enormous (like 126kB for 12,500 rsIDs).
	char *filterNoIds = filterClause(dbVarName, tableVarName, region->chrom, NULL);
	if (filterNoIds != NULL)
	    hOrFPrintf(f, "#filter: %s\n", filterNoIds);
	hOrFPrintf(f, "#");
	if (showItemRgb)
	    {
	    itemRgbCol = itemRgbHeader(f, sr, lastCol);
	    if (itemRgbCol == -1)
		showItemRgb = FALSE;	/*  did not find "reserved" */
	    }
	else
	    {
	    for (colIx = 0; colIx < lastCol; ++colIx)
		hOrFPrintf(f, "%s\t", sqlFieldName(sr));
	    hOrFPrintf(f, "%s\n", sqlFieldName(sr));
	    }
	printedColumns = TRUE;
	}
    while ((row = sqlNextRow(sr)) != NULL)
	{
	if (idHash == NULL || isNotEmpty(identifierFilter) || hashLookup(idHash, row[fieldCount]))
	    {
	    if (showItemRgb)
		itemRgbDataOut(f, row, lastCol, itemRgbCol);
	    else
		{
		for (colIx = 0; colIx < lastCol; ++colIx)
		    hOrFPrintf(f, "%s\t", row[colIx]);
		hOrFPrintf(f, "%s\n", row[lastCol]);
		}
	    ++outCount;
	    }
	}
    sqlFreeResult(&sr);
    if (!isPositional)
        break;	/* No need to iterate across regions in this case. */
    freez(&filter);
    }

/* Do some error diagnostics for user. */
if (outCount == 0)
    explainWhyNoResults(f);
hashFree(&idHash);
}


void doTabOutTable( char *db, char *table, FILE *f, struct sqlConnection *conn, char *fields)
/* Do tab-separated output on fields of a single table. */
{
boolean isTabix = FALSE;
if (isBigBed(database, table, curTrack, ctLookupName))
    bigBedTabOut(db, table, conn, fields, f);
else if (isLongTabixTable(table))
    longTabixTabOut(db, table, conn, fields, f);
else if (isBamTable(table))
    bamTabOut(db, table, conn, fields, f);
else if (isVcfTable(table, &isTabix))
    vcfTabOut(db, table, conn, fields, f, isTabix);
else if (isHicTable(table))
    hicTabOut(db, table, conn, fields, f);
else if (isCustomTrack(table))
    {
    doTabOutCustomTracks(db, table, conn, fields, f);
    }
else
    doTabOutDb(db, db, table, table, f, conn, fields);
}

struct slName *fullTableFields(char *db, char *table)
/* Return list of fields in db.table.field format. */
{
char dtBuf[256];
struct sqlConnection *conn=NULL;
struct slName *fieldList = NULL, *dtfList = NULL, *field, *dtf;
if (isBigBed(database, table, curTrack, ctLookupName))
    {
    if (!trackHubDatabase(database))
	conn = hAllocConn(db);
    fieldList = bigBedGetFields(table, conn);
    hFreeConn(&conn);
    }
else if (isLongTabixTable(table))
    fieldList = getLongTabixFields(6);
else if (isHalTable(table))
    fieldList = getBedFields(6);
else if (isBamTable(table))
    fieldList = bamGetFields(table);
else if (isVcfTable(table, NULL))
    fieldList = vcfGetFields(table);
else if (isHicTable(table))
    fieldList = hicGetFields(table);
else if (isCustomTrack(table))
    {
    struct customTrack *ct = ctLookupName(table);
    char *type = ct->dbTrackType;
    if (type != NULL)
        {
	conn = hAllocConn(CUSTOM_TRASH);
	if (startsWithWord("maf", type) || 
            startsWithWord("makeItems", type) || 
            sameWord("bedDetail", type) || 
            sameWord("barChart", type) || 
            sameWord("interact", type) || 
            sameWord("pgSnp", type))
	        fieldList = sqlListFields(conn, ct->dbTableName);
	hFreeConn(&conn);
	}
    if (fieldList == NULL)
	fieldList = getBedFields(ct->fieldCount);
    }
else
    {
    char *splitTable;
    dbOverrideFromTable(dtBuf, &db, &table);
    conn = hAllocConn(db);
    splitTable = chromTable(conn, table);
    fieldList = sqlListFields(conn, splitTable);
    freez(&splitTable);
    hFreeConn(&conn);
    }
for (field = fieldList; field != NULL; field = field->next)
    {
    char buf[256];
    safef(buf, sizeof(buf), "%s.%s.%s", db, table, field->name);
    dtf = slNameNew(buf);
    slAddHead(&dtfList, dtf);
    }
slReverse(&dtfList);
slFreeList(&fieldList);
return dtfList;
}

void doOutPrimaryTable(char *table, struct sqlConnection *conn)
/* Dump out primary table. */
{
if (anySubtrackMerge(database, table))
    errAbort("Can't do all fields output when subtrack merge is on. "
    "Please go back and select another output type (BED or custom track is good), or clear the subtrack merge.");
if (anyIntersection())
    errAbort("Can't do all fields output when intersection is on. "
    "Please go back and select another output type (BED or custom track is good), or clear the intersection.");
textOpen();
if (sameWord(table, WIKI_TRACK_TABLE))
    tabOutSelectedFields(wikiDbName(), table, NULL, fullTableFields(wikiDbName(), table));
else
    tabOutSelectedFields(database, table, NULL, fullTableFields(database, table));
}

void ensureVisibility(char *db, char *table, struct trackDb *tdb)
/* Check track visibility; if hide, print out CGI to set it to pack or full. */
{
enum trackVisibility vis = tvHide;
char *cartVis = cartOptionalString(cart, table);
if (isNotEmpty(cartVis))
    vis = hTvFromString(cartVis);
else
    {
    if (tdb == NULL)
	tdb = hTrackDbForTrackAndAncestors(db, table);
    if (tdb != NULL)
	{
	cartVis = cartOptionalString(cart, tdb->track);
	if (isNotEmpty(cartVis))
	    vis = hTvFromString(cartVis);
	else
	    vis = tdb->visibility;
	}
    }
if (vis == tvHide)
    {
    enum trackVisibility openVis = tvFull;
    if (tdb != NULL)
	{
	if (tdb->canPack)
	    openVis = tvPack;
	hPrintf("&%s=%s", table, hStringFromTv(openVis));
	if (!sameString(table, tdb->track))
	    hPrintf("&%s_sel=1", table);
	}
    else
	hPrintf("&%s=%s", table, hStringFromTv(openVis));
    }
}

void doOutHyperlinks(char *table, struct sqlConnection *conn)
/* Output as genome browser hyperlinks. */
{
char *table2 = cartOptionalString(cart, hgtaIntersectTrack);
int outputPad = cartUsualInt(cart, hgtaOutputPad,0);
struct region *region, *regionList = getRegions();
char posBuf[64];
int count = 0;

htmlOpen("Hyperlinks to Genome Browser");
for (region = regionList; region != NULL; region = region->next)
    {
    struct lm *lm = lmInit(64*1024);
    struct bed *bedList, *bed;
    bedList = cookedBedList(conn, table, region, lm, NULL);
    for (bed = bedList; bed != NULL; bed = bed->next)
	{
	char *name;
        int start = max(0,bed->chromStart+1-outputPad);
        int end = min(hChromSize(database, bed->chrom),bed->chromEnd+outputPad);
	safef(posBuf, sizeof(posBuf), "%s:%d-%d",
		    bed->chrom, start, end);
	/* Construct browser anchor URL with tracks we're looking at open. */
        if (doGalaxy())
            {
            char *s, *script = hgTracksName();
            s = strstr(script, "cgi-bin");
            hPrintf("<A HREF=\"http://%s/%s?db=%s", cgiServerNamePort(), s, database);
            }
        else
	    hPrintf("<A HREF=\"%s?db=%s", hgTracksName(), database);
	hPrintf("&position=%s", posBuf);
	ensureVisibility(database, table, curTrack);
	if (table2 != NULL)
            {
            struct trackDb *tdb2 = findTrack(table2, fullTrackList);
            ensureVisibility(database, table2, tdb2);
            }
	hPrintf("\" TARGET=_blank>");
	name = bed->name;
	if (bed->name == NULL)
	    name = posBuf;
	if (sameString(name, posBuf))
	    hPrintf("%s", posBuf);
	else
	    {
	    char *tmp = htmlEncode(name);
	    hPrintf("%s at %s", tmp, posBuf);
	    freeMem(tmp);
	    }
	hPrintf("</A><BR>\n");
	++count;
	}
    lmCleanup(&lm);
    }
if (count == 0)
    hPrintf(NO_RESULTS);
htmlClose();
}

/* Remove any meta data variables from the cart. (Copied from above!) */
void removeMetaData()
{
cartRemove(cart, "hgta_metaStatus");
cartRemove(cart, "hgta_metaVersion");
cartRemove(cart, "hgta_metaDatabases");
cartRemove(cart, "hgta_metaTables");
}

void doMetaData(struct sqlConnection *conn)
/* Get meta data for a database. */
{
puts("Content-Type:text/plain\n");
char *query = "";
if (cartVarExists(cart, hgtaMetaStatus))
    {
    printf("Table status for database %s\n", database);
    query = NOSQLINJ "SHOW TABLE STATUS";
    }
else if (cartVarExists(cart, hgtaMetaVersion))
    {
    query = NOSQLINJ "SELECT @@VERSION";
    }
else if (cartVarExists(cart, hgtaMetaDatabases))
    {
    query = NOSQLINJ "SHOW DATABASES";
    }
else if (cartVarExists(cart, hgtaMetaTables))
    {
    query = NOSQLINJ "SHOW TABLES";
    }
struct sqlResult *sr;
char **row;
char *sep="";
int c = 0;
int numCols = 0;
sr = sqlGetResult(conn, query);
numCols = sqlCountColumns(sr);
char *field;
while ((field = sqlFieldName(sr)) != NULL)
    printf("%s \t", field);
printf("\n");
while ((row = sqlNextRow(sr)) != NULL)
    {
    sep="";
    for (c=0;c<numCols;++c)
	{
	printf("%s%s",sep,row[c]);
	sep = "\t";
	}
    fprintf(stdout, "\n");
    }
sqlFreeResult(&sr);
removeMetaData();
}

void dispatch();

void doTopSubmit(struct sqlConnection *conn)
/* Respond to submit button on top level page.
 * This basically just dispatches based on output type. */
{
char *output = cartString(cart, hgtaOutputType);
char *trackName = NULL;
char *table = cartString(cart, hgtaTable);
struct trackDb *track = NULL;

if (!sameString(curGroup->name, "allTables"))
    {
    trackName = cartString(cart, hgtaTrack);
    track = mustFindTrack(trackName, fullTrackList);
    }
else
    {
    struct trackDb *cTdb = NULL;
    track = hTrackDbForTrack(database, table);
    cTdb = hCompositeTrackDbForSubtrack(database, track);
    if (cTdb)
	track = cTdb;
    }
checkNoGenomeDisabled(database, table);
if (track != NULL)
    {
    if (sameString(track->table, "gvPos") &&
	!cartVarExists(cart, "gvDisclaimer"))
	{
	/* display disclaimer and add flag to cart, program exits from here */
	htmlStart("Table Browser");
	gvDisclaimer();
	}
    else if (sameString(track->table, "gvPos") &&
	     sameString(cartString(cart, "gvDisclaimer"), "Disagree"))
	{
	cartRemove(cart, "gvDisclaimer");
	cartRemove(cart, hgtaDoTopSubmit);
	cartSetString(cart, hgtaDoMainPage, "return to table browser");
	dispatch();
	return;
	}
    }
if (doGenomeSpace())
    {
    if (!checkGsReady())
	return;
    }
if (doGreat())
    verifyGreatFormat(output);
if (sameString(output, outPrimaryTable))
    {
    if (doGalaxy() && !cgiOptionalString(hgtaDoGalaxyQuery))
        sendParamsToGalaxy(hgtaDoTopSubmit, "get output");
    else
        doOutPrimaryTable(table, conn);
    }
else if (sameString(output, outSelectedFields))
    doOutSelectedFields(table, conn);
else if (sameString(output, outSequence))
    doOutSequence(conn);
else if (sameString(output, outMicroarrayNames))
    doOutMicroarrayNames(track);
else if (sameString(output, outBed))
    doOutBed(table, conn);
else if (sameString(output, outCustomTrack))
    doOutCustomTrack(table, conn);
else if (sameString(output, outPalOptions))
    doOutPalOptions( conn);
else if (sameString(output, outGff))
    {
    if (doGalaxy() && !cgiOptionalString(hgtaDoGalaxyQuery))
        sendParamsToGalaxy(hgtaDoTopSubmit, "get output");
    else
        doOutGff(table, conn, TRUE);
    }
else if (sameString(output, outHyperlinks))
    {
    if (doGalaxy() && !cgiOptionalString(hgtaDoGalaxyQuery))
        sendParamsToGalaxy(hgtaDoTopSubmit, "get output");
    else
        doOutHyperlinks(table, conn);
    }
else if (sameString(output, outWigData))
    {
    if (doGalaxy() && !cgiOptionalString(hgtaDoGalaxyQuery))
        sendParamsToGalaxy(hgtaDoTopSubmit, "get output");
    else
        doOutWigData(track, table, conn);
    }
else if (sameString(output, outWigBed))
    {
    if (doGalaxy() && !cgiOptionalString(hgtaDoGalaxyQuery))
        sendParamsToGalaxy(hgtaDoTopSubmit, "get output");
    else if (doGreat() && !cgiOptionalString(hgtaDoGreatQuery))
        doGreatTopLevel();
    else
        doOutWigBed(track, table, conn);
    }
else if (sameString(output, outMaf))
    {
    if (doGalaxy() && !cgiOptionalString(hgtaDoGalaxyQuery))
        sendParamsToGalaxy(hgtaDoTopSubmit, "get output");
    else if (isHalTable(table))
        doHalMaf(track, table, conn);
    else
        doOutMaf(track, table, conn);
    }
else if (sameString(output, outChromGraphData))
    {
    if (doGalaxy() && !cgiOptionalString(hgtaDoGalaxyQuery))
        sendParamsToGalaxy(hgtaDoTopSubmit, "get output");
    else
        doOutChromGraphDataCt(track, table);
    }
else
    errAbort("Don't know how to handle %s output yet", output);
}

void dispatch()
/* Scan for 'do' variables and dispatch to appropriate page-generator.
 * By default head to the main page. */
{
struct hashEl *varList;
struct sqlConnection *conn = NULL;
if (!trackHubDatabase(database))
    conn = curTrack ? hAllocConnTrack(database, curTrack) : hAllocConn(database);
pushWarnHandler(earlyAbortHandler);
/* only allows view table schema function for CGB or GSID servers for the time being */
if (hIsCgbServer() || hIsGsidServer())
    {
    if (cartVarExists(cart, hgtaDoSchema))
	{
	doSchema(conn);
	}
    else
	{
        if (cartVarExists(cart, hgtaDoValueRange))
	    {
    	    doValueRange(cartString(cart, hgtaDoValueRange));
	    }
	else
	    {
	    if (cartVarExists(cart, hgtaDoValueHistogram))
    		{
		doValueHistogram(cartString(cart, hgtaDoValueHistogram));
	    	}
	    else
	    	{
	    	errAbort("Sorry, currently only the \"View Table Schema\" function of the Table Browser is available on this server.");
	    	}
	    }
	}

    }
else if (cartVarExists(cart, hgtaDoTest))
    doTest();
else if (cartVarExists(cart, hgtaDoMainPage))
    doMainPage(conn);
else if (cartVarExists(cart, hgtaDoSchema))
    doSchema(conn);
else if (cartVarExists(cart, hgtaDoTopSubmit))
    doTopSubmit(conn);
else if (cartVarExists(cart, hgtaDoSummaryStats))
    doSummaryStats(conn);
else if (cartVarExists(cart, hgtaDoIntersectPage))
    doIntersectPage(conn);
else if (cartVarExists(cart, hgtaDoPalOut))
    doGenePredPal(conn);
else if (cartVarExists(cart, hgtaDoPal))
    doOutPalOptions( conn);
else if (cartVarExists(cart, hgtaDoClearIntersect))
    doClearIntersect(conn);
else if (cartVarExists(cart, hgtaDoIntersectMore))
    doIntersectMore(conn);
else if (cartVarExists(cart, hgtaDoIntersectSubmit))
    doIntersectSubmit(conn);
else if (cartVarExists(cart, hgtaDoCorrelatePage))
    doCorrelatePage(conn);
else if (cartVarExists(cart, hgtaDoClearCorrelate))
    doClearCorrelate(conn);
else if (cartVarExists(cart, hgtaDoClearContinueCorrelate))
    doClearContinueCorrelate(conn);
else if (cartVarExists(cart, hgtaDoCorrelateMore))
    doCorrelateMore(conn);
else if (cartVarExists(cart, hgtaDoCorrelateSubmit))
    doCorrelateSubmit(conn);
else if (cartVarExists(cart, hgtaDoPasteIdentifiers))
    doPasteIdentifiers(conn);
else if (cartVarExists(cart, hgtaDoClearPasteIdentifierText))
    doClearPasteIdentifierText(conn);
/* Respond to clear within paste identifier page. */
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
else if (cartVarExists(cart, hgtaDoGalaxySelectedFields))
    sendParamsToGalaxy(hgtaDoPrintSelectedFields, "get output");
else if ((varList = cartFindPrefix(cart, hgtaDoClearAllFieldPrefix)) != NULL)
    doClearAllField(varList->name + strlen(hgtaDoClearAllFieldPrefix));
else if ((varList = cartFindPrefix(cart, hgtaDoSetAllFieldPrefix)) != NULL)
    doSetAllField(varList->name + strlen(hgtaDoSetAllFieldPrefix));
else if (cartVarExists(cart, hgtaDoGenePredSequence))
    doGenePredSequence(conn);
else if (cartVarExists(cart, hgtaDoGenomicDna))
    if (doGalaxy() && !cgiOptionalString(hgtaDoGalaxyQuery))
        sendParamsToGalaxy(hgtaDoGenomicDna, "submit");
    else
        doGenomicDna(conn);
else if (cartVarExists(cart, hgtaDoGetBed) || cartUsualBoolean(cart, hgtaDoGreatOutput, FALSE))
    doGetBed(conn);
else if (cartVarExists(cart, hgtaDoGetCustomTrackTb))
    doGetCustomTrackTb(conn);
else if (cartVarExists(cart, hgtaDoGetCustomTrackGb))
    doGetCustomTrackGb(conn);
else if (cartVarExists(cart, hgtaDoGetCustomTrackFile))
    doGetCustomTrackFile(conn);
else if (cartVarExists(cart, hgtaDoRemoveCustomTrack))
    doRemoveCustomTrack(conn);
else if (cartVarExists(cart, hgtaDoClearSubtrackMerge))
    doClearSubtrackMerge(conn);
/* Hack but I don't know what else to do now: hCompositeUi makes a hidden
 * var for hgtaDoSubtrackMergePage, so that javascript can submit and it will
 * look like that button was pressed.  However when the real submit button is
 * pressed, it will look like both were pressed... so check the real submit
 * button first (and check doMainPage before this too, for "cancel"!): */
else if (cartVarExists(cart, hgtaDoSubtrackMergeSubmit))
    doSubtrackMergeSubmit(conn);
else if (cartVarExists(cart, hgtaDoSubtrackMergePage))
    doSubtrackMergePage(conn);
else if (cartVarExists(cart, hgtaDoSetUserRegions))
    doSetUserRegions(conn);
else if (cartVarExists(cart, hgtaDoSubmitUserRegions))
    doSubmitUserRegions(conn);
else if (cartVarExists(cart, hgtaDoClearSetUserRegionsText))
    doClearSetUserRegionsText(conn);
else if (cartVarExists(cart, hgtaDoClearUserRegions))
    doClearUserRegions(conn);
else if (cartVarExists(cart, hgtaDoMetaData))
    doMetaData(conn);
else if (cartVarExists(cart, hgtaDoGsLogin))
    {
    doGsLogin(conn);
    dispatch();
    }
else	/* Default - put up initial page. */
    doMainPage(conn);
cartRemovePrefix(cart, hgtaDo);
hFreeConn(&conn);
}

char *excludeVars[] = {"Submit", "submit", NULL};

static void rAddTablesToHash(struct trackDb *tdbList, struct hash *hash)
/* Add tracks in list to hash, keyed by tdb->table*/
{
struct trackDb *tdb;
for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    {
    hashAdd(hash, tdb->table, tdb);
    if (tdb->subtracks)
        rAddTablesToHash(tdb->subtracks, hash);
    }
}

void initGroupsTracksTables()
/* Get list of groups that actually have something in them, prepare hashes
 * containing all tracks and all tables. Set global variables that correspond
 * to the group, track, and table specified in the cart. */
{
cartTrackDbInit(cart, &fullTrackList, &fullGroupList, TRUE);
fullTableToTdbHash = hashNew(0);
rAddTablesToHash(fullTrackList, fullTableToTdbHash);
curTrack = findSelectedTrack(fullTrackList, NULL, hgtaTrack);

// if there isn't a current track, then use the default group
if (curTrack != NULL)
    curGroup = findSelectedGroup(fullGroupList, hgtaGroup);
else
    curGroup = fullGroupList;
if (sameString(curGroup->name, "allTables"))
    curTrack = NULL;
curTable    = findSelectedTable(curTrack, hgtaTable);
if (curTrack == NULL)
    {
    struct trackDb *tdb  = hTrackDbForTrack(database, curTable);
    struct trackDb *cTdb = hCompositeTrackDbForSubtrack(database, tdb);
    if (cTdb)
        curTrack = cTdb;
    else
        curTrack = tdb;
    }
}


void hgTables()
/* hgTables - Get table data associated with tracks and intersect tracks.
 * Here we set up cart and some global variables, dispatch the command,
 * and put away the cart when it is done. */
{

char *clade = NULL;

oldVars = hashNew(10);

/* Sometimes we output HTML and sometimes plain text; let each outputter
 * take care of headers instead of using a fixed cart*Shell(). */
cart = cartAndCookieNoContent(hUserCookie(), excludeVars, oldVars);

// Try to deal with virt chrom position used by hgTracks.
if (startsWith("virt:", cartUsualString(cart, "position", "")))
    cartSetString(cart, "position", cartUsualString(cart, "nonVirtPosition", ""));

/* Set up global variables. */
allJoiner = joinerRead("all.joiner");
getDbGenomeClade(cart, &database, &genome, &clade, oldVars);
freezeName = hFreezeFromDb(database);

initGenbankTableNames(database);

int timeout = cartUsualInt(cart, "udcTimeout", 300);
if (udcCacheTimeout() < timeout)
    udcSetCacheTimeout(timeout);
knetUdcInstall();

char *backgroundStatus = cartUsualString(cart, "backgroundStatus", NULL);
if (backgroundStatus)
    {
    getBackgroundStatus(backgroundStatus);
    exit(0);
    }

char *backgroundExec = cgiUsualString("backgroundExec", NULL);
if (sameOk(backgroundExec,"gsSendToDM"))
    {
    gsSendToDM();
    exit(0);
    }

/* Init track and group lists and figure out what page to put up. */
initGroupsTracksTables();
struct dyString *dyWarn = dyStringNew(0);
struct hgPositions *hgp = lookupPosition(dyWarn);
if (hgp->singlePos && isEmpty(dyWarn->string))
    {
    if (cartUsualBoolean(cart, hgtaDoGreatOutput, FALSE))
	doGetGreatOutput(dispatch);
    else
	dispatch();
    }
else
    {
    cartWebStartHeader(cart, database, "Table Browser");
    if (isNotEmpty(dyWarn->string))
        warn("%s", dyWarn->string);
    if (hgp->posCount > 1)
        hgPositionsHtml(database, hgp, hgTablesName(), cart);
    else
        {
        struct sqlConnection *conn = NULL;
        if (!trackHubDatabase(database))
            conn = curTrack ? hAllocConnTrack(database, curTrack) : hAllocConn(database);
        mainPageAfterOpen(conn);
        hFreeConn(&conn);
        }
    cartWebEnd();
    }

textOutClose(&compressPipeline, &saveStdout);

if (doGenomeSpace())
    {
    if (gsTemp)
	{
	cartSetString(cart, "gsTemp", gsTemp);
	char *workUrl = NULL;
	startBackgroundWork("./hgTables backgroundExec=gsSendToDM", &workUrl);

	htmlOpen("Uploading Output to GenomeSpace");

	jsInlineF(
	    "setTimeout(function(){location = 'hgTables?backgroundStatus=%s';},2000);\n", // was 10000?
	    cgiEncode(workUrl));
	htmlClose();
	fflush(stdout);

	gsTemp = NULL;
	}	
    }


/* Save variables. */
cartCheckout(&cart);
}

int main(int argc, char *argv[])
/* Process command line. */
{

long enteredMainTime = clock1000();

pushCarefulMemHandler(LIMIT_2or6GB);
htmlPushEarlyHandlers(); /* Make errors legible during initialization. */
cgiSpoof(&argc, argv);

hgTables();

cgiExitTime("hgTables", enteredMainTime);

return 0;
}
