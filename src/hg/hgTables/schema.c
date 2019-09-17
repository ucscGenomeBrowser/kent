/* schema - display info about database organization. */

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
#include "jksql.h"
#include "hdb.h"
#include "web.h"
#include "trackDb.h"
#include "joiner.h"
#include "tableDescriptions.h"
#include "asParse.h"
#include "customTrack.h"
#include "bedCart.h"
#include "hgMaf.h"
#include "hgTables.h"
#include "wikiTrack.h"
#include "makeItemsItem.h"
#include "bedDetail.h"
#include "pgSnp.h"
#include "barChartBed.h"
#include "interact.h"
#include "hubConnect.h"
#include "errCatch.h"

static char *nbForNothing(char *val)
/* substitute &nbsp; for empty strings to keep table formating sane */
{
char *s = skipLeadingSpaces(val);
if ((s == NULL) || (s[0] == '\0'))
    return "&nbsp;";
else
    return val;
}

static char *abbreviateInPlace(char *val, int len)
/* Abbreviate a string to len characters.  */
{
int vlen = strlen(val);
if (vlen > len)
    strcpy(val+len-3, "...");
return val;
}

static char *cleanExample(char *val)
/* Abbreviate example if necessary and add non-breaking space if need be */
{
val = abbreviateInPlace(val, 30);
val = nbForNothing(val);
return val;
}

static struct slName *storeRow(struct sqlConnection *conn, char *query)
/* Just save the results of a single row query in a string list. */
{
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
struct slName *list = NULL, *el;
int i, colCount = sqlCountColumns(sr);
if ((row = sqlNextRow(sr)) != NULL)
     {
     for (i=0; i<colCount; ++i)
         {
	 el = slNameNew(row[i]);
	 slAddTail(&list, el);
	 }
     }
sqlFreeResult(&sr);
return list;
}

void describeFields(char *db, char *table,
                    struct asObject *asObj, struct sqlConnection *conn)
/* Print out an HTML table showing table fields and types, and optionally
 * offering histograms for the text/enum fields. */
{
struct sqlResult *sr;
char **row;
#define TOO_BIG_FOR_HISTO 500000
boolean tooBig = (sqlTableSize(conn, table) > TOO_BIG_FOR_HISTO);
char query[256];
struct slName *exampleList, *example;
boolean showItemRgb = FALSE;

showItemRgb=bedItemRgb(findTdbForTable(db, curTrack, table, ctLookupName));
// should we expect itemRgb instead of "reserved"

sqlSafef(query, sizeof(query), "select * from %s limit 1", table);
exampleList = storeRow(conn, query);
sqlSafef(query, sizeof(query), "describe %s", table);
sr = sqlGetResult(conn, query);

hTableStart();
hPrintf("<TR><TH>field</TH>");
if (exampleList != NULL)
    hPrintf("<TH>example</TH>");
hPrintf("<TH>SQL type</TH> ");
if (!tooBig)
    hPrintf("<TH>info</TH> ");
if (asObj != NULL)
    hPrintf("<TH>description</TH> ");
puts("</TR>\n");
example = exampleList;
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (showItemRgb && (sameWord(row[0],"reserved")))
	hPrintf("<TR><TD><TT>itemRgb</TT></TD> ");
    else
	hPrintf("<TR><TD><TT>%s</TT></TD> ", row[0]);
    if (exampleList != NULL)
        {
	hPrintf("<TD>");
	if (example != NULL)
	     hPrintf("%s", cleanExample(example->name));
	else
	     hPrintf("n/a");
	hPrintf("</TD>");
	}
    // enums/sets with many items can make for painfully wide rows in the table --
    // add spaces between quoted list values:
    if (stringIn("','", row[1]))
	{
	struct dyString *spaced = dyStringSub(row[1], "','", "', '");
	hPrintf("<TD><TT>%s</TT></TD>", spaced->string);
	}
    else
	hPrintf("<TD><TT>%s</TT></TD>", row[1]);
    if (!tooBig)
	{
	hPrintf(" <TD>");
	if ((isSqlStringType(row[1]) && !sameString(row[1], "longblob")) ||
	    isSqlEnumType(row[1]) || isSqlSetType(row[1]))
	    {
	    hPrintf("<A HREF=\"%s", getScriptName());
	    hPrintf("?%s", cartSidUrlString(cart));
	    hPrintf("&amp;%s=%s", hgtaDatabase, db);
	    hPrintf("&amp;%s=%s", hgtaHistoTable, table);
	    hPrintf("&amp;%s=%s", hgtaDoValueHistogram, row[0]);
	    hPrintf("\">");
	    hPrintf("values");
	    hPrintf("</A>");
	    }
	else if (isSqlNumType(row[1]))
	    {
	    hPrintf("<A HREF=\"%s", getScriptName());
	    hPrintf("?%s", cartSidUrlString(cart));
	    hPrintf("&amp;%s=%s", hgtaDatabase, db);
	    hPrintf("&amp;%s=%s", hgtaHistoTable, table);
	    hPrintf("&amp;%s=%s", hgtaDoValueRange, row[0]);
	    hPrintf("\">");
	    hPrintf("range");
	    hPrintf("</A>");
	    }
	else
	    {
	    hPrintf("&nbsp;");
	    }
	hPrintf("</TD>");
	}
    if (asObj != NULL)
        {
	struct asColumn *asCol = asColumnFind(asObj, row[0]);
	hPrintf(" <TD>");
	if (asCol != NULL)
	    hPrintf("%s", asCol->comment);
	else
	    {
	    if (sameString("bin", row[0]))
	       hPrintf("Indexing field to speed chromosome range queries.");
	    else
		hPrintf("&nbsp;");
	    }
	hPrintf("</TD>");
	}
    puts("</TR>");
    if (example != NULL)
	example = example->next;
    }
hTableEnd();
sqlFreeResult(&sr);
}

static void explainCoordSystem()
/* Our coord system is counter-intuitive to users.  Warn them in advance to
 * reduce the frequency with which they find this "bug" on their own and
 * we have to explain it on the genome list. */
{
if (!hIsGsidServer())
    {
    puts("<BR><I>Note: all start coordinates in our database are 0-based, not \n"
     "1-based.  See explanation \n"
     "<A HREF=\"http://genome.ucsc.edu/FAQ/FAQtracks#tracks1\">"
         "here</A>.</I>");
    }
else
    {
    puts("<BR><I>Note: all start coordinates in our database are 0-based, not \n"
         "1-based.\n</I>");
    }
}


static void printSampleRows(int sampleCount, struct sqlConnection *conn, char *table)
/* Put up sample values. */
{
char query[256];
struct sqlResult *sr;
char **row;
int i, columnCount = 0;
int itemRgbCol = -1;
boolean showItemRgb = FALSE;

showItemRgb=bedItemRgb(findTdbForTable(database, curTrack, table, ctLookupName));
// should we expect itemRgb     instead of "reserved"

/* Make table with header row containing name of fields. */
sqlSafef(query, sizeof(query), "describe %s", table);
sr = sqlGetResult(conn, query);
hTableStart();
hPrintf("<TR>");
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (showItemRgb && sameWord(row[0],"reserved"))
	{
	hPrintf("<TH>itemRgb</TH>");
	itemRgbCol = columnCount;
	}
    else
	hPrintf("<TH>%s</TH>", row[0]);
    ++columnCount;
    }
hPrintf("</TR>");
sqlFreeResult(&sr);

/* Get some sample fields. */
sqlSafef(query, sizeof(query), "select * from %s limit %d", table, sampleCount);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    hPrintf("<TR>");
    for (i=0; i<columnCount; ++i)
	{
	if (showItemRgb && (i == itemRgbCol))
	    {
	    int rgb = atoi(row[i]);
	    hPrintf("<TD>%d,%d,%d</TD>", (rgb & 0xff0000) >> 16,
		(rgb & 0xff00) >> 8, (rgb & 0xff));
	    }
	else
	    {
	    if (row[i] == NULL)
		{
		hPrintf("<TD></TD>");
		}
	    else
	        {
		writeHtmlCell(row[i]);
		}
	    }
	}
    hPrintf("</TR>\n");
    }
sqlFreeResult(&sr);
hTableEnd();
explainCoordSystem();
}

static int joinerPairCmpOnAandB (const void *va, const void *vb)
/* Compare two joinerPair based on the a and b element of pair. */
{
const struct joinerPair *jpA = *((struct joinerPair **)va);
const struct joinerPair *jpB = *((struct joinerPair **)vb);
struct joinerDtf *a1 = jpA->a;
struct joinerDtf *a2 = jpA->b;
struct joinerDtf *b1 = jpB->a;
struct joinerDtf *b2 = jpB->b;
int diff;

diff = strcmp(a1->database, b1->database);
if (diff == 0)
   {
   diff = strcmp(a1->table, b1->table);
   if (diff == 0)
       {
       diff = strcmp(a1->field, b1->field);
       if (diff == 0)
           {
           diff = strcmp(a2->database, b2->database);
           if (diff == 0)
               {
               diff = strcmp(a2->table, b2->table);
               if (diff == 0)
                   diff = strcmp(a2->field, b2->field);
               }
           }
       }
   }
return diff;
}

static boolean isViaIndex(struct joinerSet *jsPrimary, struct joinerDtf *dtf)
/* Return's TRUE if dtf is part of identifier only by an array index. */
{
struct joinerField *jf;
struct slRef *chain, *link;
struct joinerSet *js;
boolean retVal = FALSE;
boolean gotRetVal = FALSE;

chain = joinerSetInheritanceChain(jsPrimary);
for (link = chain; link != NULL; link = link->next)
    {
    js = link->val;
    for (jf = js->fieldList; jf != NULL; jf = jf->next)
	{
	if (sameString(jf->table, dtf->table))
	    if (sameString(jf->field, dtf->field))
		if (slNameInList(jf->dbList, dtf->database))
		    {
		    retVal = jf->indexOf;
		    gotRetVal = TRUE;
		    break;
		    }
	}
    if (gotRetVal)
        break;
    }
slFreeList(&chain);
return retVal;
}

void printTrackHtml(struct trackDb *tdb)
/* If trackDb has html for table, print it out in a new section. */
{
if (tdb != NULL && isNotEmpty(tdb->html))
    {
    webNewSection("%s (%s) Track Description", tdb->shortLabel, tdb->track);
    char *browserVersion;
    if (btIE == cgiClientBrowser(&browserVersion, NULL, NULL) && *browserVersion < '8')
        puts(tdb->html);
    else
	{
	// H2 (as in "<H2>Description</H2>") has a big top margin, which adds to
	// the 10px start-of-web-section <tr> (except for IE < 8, above).
	// Tim's trick for moving the text back up in this case, to look like more
	// like details pages in which HR's bottom margin melts into H2's top margin:
	char *s = skipLeadingSpaces(tdb->html);
	if (startsWith("<H2>", s) || startsWith("<h2>", s))
	    printf("<div style='position:relative; top:-1.2em; margin-bottom:0em;'>%s\n</div>",
		   tdb->html);
	else
	    puts(tdb->html);
	}
    }
}


static void showSchemaDb(char *db, struct trackDb *tdb, char *table)
/* Show schema to open html page. */
{
struct trackDb *tdbForConn = tdb ? tdb : curTrack;
struct sqlConnection *conn;
if (tdbForConn == NULL)
    conn = hAllocConn(db);
else
    conn = hAllocConnTrack(db, tdbForConn);
struct joiner *joiner = allJoiner;
struct joinerPair *jpList, *jp;
struct asObject *asObj = asForTable(conn, table);
char *splitTable = chromTable(conn, table);
hPrintf("<B>Database:</B> %s", db);
hPrintf("&nbsp;&nbsp;&nbsp;&nbsp;<B>Primary Table:</B> %s", table);
if (!sameString(splitTable, table))
    hPrintf(" (%s)", splitTable);
hPrintf("&nbsp;&nbsp;&nbsp;&nbsp;<B>Row Count: </B>  ");
printLongWithCommas(stdout, sqlTableSize(conn, splitTable));
char *date = firstWordInLine(sqlTableUpdate(conn, splitTable));
if (date != NULL)
    printf("&nbsp&nbsp<B> Data last updated:&nbsp;</B>%s<BR>\n", date);
if (asObj != NULL)
    hPrintf("<B>Format description:</B> %s<BR>", asObj->comment);
if (cartTrackDbIsNoGenome(db, table))
    hPrintf(" Note: genome-wide queries are not available for this table.");
describeFields(db, splitTable, asObj, conn);
if (tdbForConn != NULL)
    {
    char *type = tdbForConn->type;
    if (startsWithWord("bigWig", type))
	printf("<BR>This table points to a file in "
	       "<A HREF=\"/goldenPath/help/bigWig.html\" TARGET=_BLANK>"
	       "BigWig</A> format.<BR>\n");
    }
jpList = joinerRelate(joiner, db, table, NULL);

/* sort and unique list */
slUniqify(&jpList, joinerPairCmpOnAandB, joinerPairFree);

if (jpList != NULL)
    {
    webNewSection("Connected Tables and Joining Fields");
    for (jp = jpList; jp != NULL; jp = jp->next)
	{
	if (cartTrackDbIsAccessDenied(jp->b->database, jp->b->table))
	    continue;
	struct joinerSet *js = jp->identifier;
	boolean aViaIndex, bViaIndex;
	hPrintSpaces(6);
	hPrintf("%s.", jp->b->database);
	hPrintf("<A HREF=\"%s?", cgiScriptName());
	hPrintf("%s&", cartSidUrlString(cart));
	hPrintf("%s=%s&", hgtaDoSchemaDb, jp->b->database);
	hPrintf("%s=%s", hgtaDoSchemaTable, jp->b->table);
	hPrintf("\">");
	hPrintf("%s", jp->b->table);
	hPrintf("</A>");
	aViaIndex = isViaIndex(js, jp->a);
	bViaIndex = isViaIndex(js, jp->b);
	hPrintf(".%s ", jp->b->field);
	if (aViaIndex && bViaIndex)
	    {
	    hPrintf("(%s.%s and %s.%s are arrays sharing an index)",
	        jp->a->table, jp->a->field,
	        jp->b->table, jp->b->field);

	    }
	else if (aViaIndex)
	    {
            hPrintf("(which is an array index into %s.%s)", jp->a->table, jp->a->field);
	    }
	else if (bViaIndex)
	    {
            hPrintf("(%s.%s is an array index into %s.%s)", jp->a->table, jp->a->field,
	        jp->b->table, jp->b->field);
	    }
	else
	    {
	    hPrintf("(via %s.%s)", jp->a->table, jp->a->field);
	    }
        if (cartTrackDbIsNoGenome(jp->b->database, jp->b->table))
            hPrintf(" Note: genome-wide queries are not available for this table.");
	hPrintf("<BR>\n");
	}
    }
webNewSection("Sample Rows");
printSampleRows(10, conn, splitTable);
printTrackHtml(tdb);
hFreeConn(&conn);
}

static void showSchemaCtWiggle(char *table, struct customTrack *ct)
/* Show schema on wiggle format custom track. */
{
hPrintf("<B>Wiggle Custom Track ID:</B> %s<BR>\n", table);
hPrintf("Wiggle custom tracks are stored in a dense binary format.");
printTrackHtml(ct->tdb);
}

static void showSchemaCtChromGraph(char *table, struct customTrack *ct)
/* Show schema on wiggle format custom track. */
{
hPrintf("<B>ChromGraph Custom Track ID:</B> %s<BR>\n", table);
hPrintf("ChromGraph custom tracks are stored in a dense binary format.");
printTrackHtml(ct->tdb);
}

static void showSchemaCtMaf(char *table, struct customTrack *ct)
/* Show schema on maf format custom track. */
{
hPrintf("<B>MAF Custom Track ID:</B> %s<BR>\n", table);
hPrintf("For formatting information see: ");
hPrintf("<A HREF=\"../FAQ/FAQformat.html#format5\">MAF</A> ");
hPrintf("format.");

struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
webNewSection("Sample Rows");
printSampleRows(10, conn, ct->dbTableName);
printTrackHtml(ct->tdb);
hFreeConn(&conn);
}


static void showSchemaCtBed(char *table, struct customTrack *ct)
/* Show schema on bed format custom track. */
{
struct bed *bed;
int count = 0;
boolean showItemRgb = FALSE;

showItemRgb=bedItemRgb(ct->tdb);	/* should we expect itemRgb */
					/*	instead of "reserved" */

/* Find named custom track. */
hPrintf("<B>Custom Track ID:</B> %s ", table);
hPrintf("<B>Field Count:</B> %d<BR>", ct->fieldCount);
hPrintf("For formatting information see: ");
hPrintf("<A HREF=\"../FAQ/FAQformat.html#format1\">BED</A> ");
hPrintf("format.");

if (ct->dbTrack)
    {
    struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
    webNewSection("Sample Rows");
    printSampleRows(10, conn, ct->dbTableName);
    printTrackHtml(ct->tdb);
    hFreeConn(&conn);
    }
else
    {
    webNewSection("Sample Rows");
    hPrintf("<TT><PRE>");
    if (showItemRgb)
	{
	for(bed = ct->bedList;bed != NULL && count < 10;bed = bed->next,++count)
	    bedTabOutNitemRgb(bed, ct->fieldCount, stdout);
	}
    else
	{
	for(bed = ct->bedList;bed != NULL && count < 10;bed = bed->next,++count)
	    bedTabOutN(bed, ct->fieldCount, stdout);
	}
    hPrintf("</PRE></TT>\n");
    }
}

static void showSchemaCtArray(char *table, struct customTrack *ct)
/* Show schema on bed format custom track. */
{
struct bed *bed;
int count = 0;
/* Find named custom track. */
hPrintf("<B>Custom Track ID:</B> %s ", table);
hPrintf("<B>Field Count:</B> %d<BR>", ct->fieldCount);
hPrintf("For formatting information see: ");
hPrintf("<A HREF=\"../FAQ/FAQformat.html#format6.5\">Microarray</A> ");
hPrintf("format.");

if (ct->dbTrack)
    {
    struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
    webNewSection("Sample Rows");
    printSampleRows(10, conn, ct->dbTableName);
    printTrackHtml(ct->tdb);
    hFreeConn(&conn);
    }
else
    {
    webNewSection("Sample Rows");
    hPrintf("<TT><PRE>");
    for(bed = ct->bedList;bed != NULL && count < 10;bed = bed->next,++count)
	bedTabOutN(bed, ct->fieldCount, stdout);
    hPrintf("</PRE></TT>\n");
    }
}

static void showSchemaWithAsObj(char *db, char *trackId, struct customTrack *ct,
                                struct asObject *asObj)
/* Show schema on custom track using autoSqlString defined for this track type. */
{
struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
char *table = ct->dbTableName;

hPrintf("<B>Genome Database:</B> %s ", db);
hPrintf("<B>Track ID:</B> %s ", trackId);
hPrintf("<B>MySQL table:</B> %s", table);
hPrintf("&nbsp;&nbsp;&nbsp;&nbsp;<B>Row Count:</B> ");
printLongWithCommas(stdout, sqlTableSize(conn, table));
hPrintf("<BR>\n");
if (asObj != NULL)
    hPrintf("<B>Format description:</B> %s<BR>", asObj->comment);
describeFields(CUSTOM_TRASH, table, asObj, conn);

webNewSection("Sample Rows");
printSampleRows(10, conn, ct->dbTableName);
printTrackHtml(ct->tdb);
hFreeConn(&conn);
}

static void showSchemaCt(char *db, char *table)
/* Show schema on custom track. */
{
struct customTrack *ct = ctLookupName(table);
char *type = ct->tdb->type;
if (startsWithWord("wig", type) || startsWithWord("bigWig", type))
    showSchemaCtWiggle(table, ct);
else if (startsWithWord("chromGraph", type))
    showSchemaCtChromGraph(table, ct);
else if (startsWithWord("bed", type) || startsWithWord("bedGraph", type))
    showSchemaCtBed(table, ct);
else if (startsWithWord("maf", type))
    showSchemaCtMaf(table, ct);
else if (startsWithWord("array", type))
    showSchemaCtArray(table, ct);
else if (startsWithWord("makeItems", type))
    {
    struct asObject *asObj = makeItemsItemAsObj();
    showSchemaWithAsObj(db, table, ct, asObj);
    asObjectFree(&asObj);
    }
else if (sameWord("bedDetail", type))
    {
    struct asObject *asObj = bedDetailAsObj();
    showSchemaWithAsObj(db, table, ct, asObj);
    asObjectFree(&asObj);
    }
else if (sameWord("pgSnp", type))
    {
    struct asObject *asObj = pgSnpAsObj();
    showSchemaWithAsObj(db, table, ct, asObj);
    asObjectFree(&asObj);
    }
else if (sameWord("barChart", type))
    {
    struct asObject *asObj = barChartAsObj();
    showSchemaWithAsObj(db, table, ct, asObj);
    asObjectFree(&asObj);
    }
else if (sameWord("interact", type))
    {
    struct asObject *asObj = interactAsObj();
    showSchemaWithAsObj(db, table, ct, asObj);
    asObjectFree(&asObj);
    }
else
    errAbort("Unrecognized customTrack type %s", type);
}

static void showSchemaHub(char *db, char *table)
/* Show schema on a hub track. */
{
struct trackDb *tdb = hashMustFindVal(fullTableToTdbHash, table);
hubConnectAddDescription(db, tdb);
char *type = cloneFirstWord(tdb->type);
if (tdbIsBigBed(tdb))
    showSchemaBigBed(table, tdb);
else if (sameString(type, "longTabix"))
    showSchemaLongTabix(table, tdb);
else if (sameString(type, "bam"))
    showSchemaBam(table, tdb);
else if (sameString(type, "vcfTabix"))
    showSchemaVcf(table, tdb, TRUE);
else if (sameString(type, "hic"))
    showSchemaHic(table, tdb);
else
    {
    hPrintf("Binary file of type %s stored at %s<BR>\n",
	    type, trackDbSetting(tdb, "bigDataUrl"));
    printTrackHtml(tdb);
    }
}

static void showSchemaWiki(struct trackDb *tdb, char *table)
/* Show schema for the wikiTrack. */
{
hPrintf("<B>User annotations to UCSC genes or genome regions</B><BR>\n");
showSchemaDb(wikiDbName(), tdb, table);
printTrackHtml(tdb);
}

static void showSchema(char *db, struct trackDb *tdb, char *table)
/* Show schema to open html page. */
{
boolean isTabix = FALSE;
if (isHubTrack(table))
    showSchemaHub(db, table);
else if (isBigBed(database, table, curTrack, ctLookupName))
    showSchemaBigBed(table, tdb);
else if (isLongTabixTable(table))
    showSchemaLongTabix(table, tdb);
else if (isBamTable(table))
    showSchemaBam(table, tdb);
else if (isVcfTable(table, &isTabix))
    showSchemaVcf(table, tdb, isTabix);
else if (isHicTable(table))
    showSchemaHic(table, tdb);
else if (isCustomTrack(table))
    showSchemaCt(db, table);
else if (sameWord(table, WIKI_TRACK_TABLE))
    showSchemaWiki(tdb, table);
else if (isBigWig(database, table, curTrack, ctLookupName) && !hTableExists(db, table))
	showSchemaBigWigNoTable(db, table, tdb);
else
    showSchemaDb(db, tdb, table);
}

void doTableSchema(char *db, char *table, struct sqlConnection *conn)
/* Show schema around table (which is not described by curTrack). */
{
struct trackDb *tdb = NULL;
char parseBuf[256];
dbOverrideFromTable(parseBuf, &db, &table);
htmlOpen("Schema for %s", table);
tdb = hTrackDbForTrack(database, table);
showSchema(db, tdb, table);
htmlClose();
}

static boolean curTrackDescribesCurTable()
/* Return TRUE if curTable is curTrack or its subtrack. */
{
if (curTrack == NULL)
    return FALSE;
if (sameString(curTrack->table, curTable))
    return TRUE;
else if (startsWith("wigMaf", curTrack->type))
    {
    struct consWiggle *wig, *wiggles = wigMafWiggles(database, curTrack);
    for (wig = wiggles; wig != NULL; wig = wig->next)
        if (sameString(curTable, wig->table))
            return TRUE;
    }
else if (curTrack->subtracks != NULL)
    {
    struct slRef *tdbRefList = trackDbListGetRefsToDescendantLeaves(curTrack->subtracks);
    struct slRef *tdbRef;
    for (tdbRef = tdbRefList; tdbRef != NULL; tdbRef = tdbRef->next)
        {
	struct trackDb *sTdb = tdbRef->val;
        if (sameString(sTdb->table, curTable))
            return TRUE;
        }
    }
return FALSE;
}

void doSchema(struct sqlConnection *conn)
/* Show schema around current track. */
{
if (curTrackDescribesCurTable())
    {
    char *table = connectingTableForTrack(curTable);
    if (!isCustomTrack(table) && !hashFindVal(fullTableToTdbHash, table))
        hashAdd(fullTableToTdbHash, table, curTrack);
    htmlOpen("Schema for %s - %s", curTrack->shortLabel, curTrack->longLabel);
    showSchema(database, curTrack, table);
    htmlClose();
    }
else
    doTableSchema(database, curTable, conn);
}

struct asObject *asForTable(struct sqlConnection *conn, char *table)
/* Get autoSQL description if any associated with table. */
/* Wrap some error catching around asForTable. */
{
struct trackDb *tdb = hashFindVal(fullTableToTdbHash, table);
if (tdb != NULL)
    return asForTdb(conn,tdb);

// Some cases are for tables with no tdb!
struct asObject *asObj = NULL;
if (sqlTableExists(conn, "tableDescriptions"))
    {
    struct errCatch *errCatch = errCatchNew();
    if (errCatchStart(errCatch))
        {
        char query[256];

        sqlSafef(query, sizeof(query),
              "select autoSqlDef from tableDescriptions where tableName='%s'", table);
        char *asText = asText = sqlQuickString(conn, query);

        // If no result try split table. (not likely)
        if (asText == NULL)
            {
            sqlSafef(query, sizeof(query),
                  "select autoSqlDef from tableDescriptions where tableName='chrN_%s'", table);
            asText = sqlQuickString(conn, query);
            }
        if (asText != NULL && asText[0] != 0)
            {
            asObj = asParseText(asText);
            }
        freez(&asText);
        }
    errCatchEnd(errCatch);
    errCatchFree(&errCatch);
    }
return asObj;
}
