/* schema - display info about database organization. */

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
#include "trackDb.h"
#include "joiner.h"
#include "tableDescriptions.h"
#include "asParse.h"
#include "customTrack.h"
#include "bedCart.h"
#include "hgMaf.h"
#include "hgTables.h"
#include "wikiTrack.h"

static char const rcsid[] = "$Id: schema.c,v 1.49.6.2 2008/08/07 16:02:38 markd Exp $";

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

showItemRgb=bedItemRgb(curTrack);	/* should we expect itemRgb */
					/*	instead of "reserved" */

safef(query, sizeof(query), "select * from %s limit 1", table);
exampleList = storeRow(conn, query);
safef(query, sizeof(query), "describe %s", table);
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
    hPrintf("<TD><TT>%s</TT></TD>", row[1]);
    if (!tooBig)
	{
	hPrintf(" <TD>");
	if ((isSqlStringType(row[1]) && !sameString(row[1], "longblob")) ||
	    isSqlEnumType(row[1]) || isSqlSetType(row[1]))
	    {
	    hPrintf("<A HREF=\"%s", getScriptName());
	    hPrintf("?%s", cartSidUrlString(cart));
	    hPrintf("&%s=%s", hgtaDatabase, db);
	    hPrintf("&%s=%s", hgtaHistoTable, table);
	    hPrintf("&%s=%s", hgtaDoValueHistogram, row[0]);
	    hPrintf("\">");
	    hPrintf("values");
	    hPrintf("</A>");
	    }
	else if (isSqlNumType(row[1]))
	    {
	    hPrintf("<A HREF=\"%s", getScriptName());
	    hPrintf("?%s", cartSidUrlString(cart));
	    hPrintf("&%s=%s", hgtaDatabase, db);
	    hPrintf("&%s=%s", hgtaHistoTable, table);
	    hPrintf("&%s=%s", hgtaDoValueRange, row[0]);
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
    puts("<P><I>Note: all start coordinates in our database are 0-based, not \n"
     "1-based.  See explanation \n"
     "<A HREF=\"http://genome.ucsc.edu/FAQ/FAQtracks#tracks1\">"
     "here</A>.</I></P>");
    }
else
    {
    puts("<P><I>Note: all start coordinates in our database are 0-based, not \n"
     "1-based.\n</I></P>");
    }
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


static void printSampleRows(int sampleCount, struct sqlConnection *conn, char *table)
/* Put up sample values. */
{
char query[256];
struct sqlResult *sr;
char **row;
int i, columnCount = 0;
int itemRgbCol = -1;
boolean showItemRgb = FALSE;

showItemRgb=bedItemRgb(curTrack);	/* should we expect itemRgb */
					/*	instead of "reserved" */

/* Make table with header row containing name of fields. */
safef(query, sizeof(query), "describe %s", table);
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
safef(query, sizeof(query), "select * from %s limit %d", table, sampleCount);
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
	    else if (strlen(row[i]) > 128)
		{
		char *s = cloneStringZ(row[i],128);
		char *r;
		stripHtmlTags(s);
		eraseTrailingSpaces(s);
		r = replaceChars(s, " ", "&nbsp;");
		hPrintf("<TD>%s&nbsp;...</TD>", r);
		freeMem(s);
		freeMem(r);
		}
	    else
		{
		char *r;
		stripHtmlTags(row[i]);
		eraseTrailingSpaces(row[i]);
		r = replaceChars(row[i], " ", "&nbsp;");
		hPrintf("<TD>%s</TD>", r);
		freeMem(r);
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

static void printTrackHtml(struct trackDb *tdb)
/* If trackDb has html for table, print it out in a new section. */
{
if (tdb != NULL && isNotEmpty(tdb->html))
    {
    webNewSection("%s (%s) Track Description", tdb->shortLabel, tdb->tableName);
    puts(tdb->html);
    }
}


static void showSchemaDb(char *db, struct trackDb *tdb, char *table)
/* Show schema to open html page. */
{
struct sqlConnection *conn = sqlConnect(db);
struct joiner *joiner = allJoiner;
struct joinerPair *jpList, *jp;
struct asObject *asObj = asForTable(conn, table);
char *splitTable = chromTable(conn, table);

hPrintf("<B>Database:</B> %s", db);
hPrintf("&nbsp;&nbsp;&nbsp;&nbsp;<B>Primary Table:</B> %s", table);
if (!sameString(splitTable, table))
    hPrintf(" (%s)", splitTable);
hPrintf("&nbsp;&nbsp;&nbsp;&nbsp;<B>Row Count:</B> ");
printLongWithCommas(stdout, sqlTableSize(conn, splitTable));
hPrintf("<BR>\n");
if (asObj != NULL)
    hPrintf("<B>Description:</B> %s<BR>", asObj->comment);
describeFields(db, splitTable, asObj, conn);

jpList = joinerRelate(joiner, db, table);

/* sort and unique list */
slUniqify(&jpList, joinerPairCmpOnAandB, joinerPairFree);

if (jpList != NULL)
    {
    webNewSection("Connected Tables and Joining Fields");
    for (jp = jpList; jp != NULL; jp = jp->next)
	{
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
	    hPrintf("(which is an array index into %s.%s)", 
	    	jp->a->table, jp->a->field);
	    }
	else if (bViaIndex)
	    {
	    hPrintf("(%s.%s is an array index into %s.%s)", 
		jp->a->table, jp->a->field,
	    	jp->b->table, jp->b->field);
	    }
	else
	    {
	    hPrintf("(via %s.%s)", jp->a->table, jp->a->field);
	    }
	hPrintf("<BR>\n");
	}
    }
webNewSection("Sample Rows");
printSampleRows(10, conn, splitTable);
printTrackHtml(tdb);
sqlDisconnect(&conn);
}

static void showSchemaCtWiggle(char *table, struct customTrack *ct)
/* Show schema on wiggle format custom track. */
{
hPrintf("<B>Wiggle Custom Track ID:</B> %s<BR>\n", table);
hPrintf("Wiggle custom tracks are stored in a dense binary format.");
}

static void showSchemaCtChromGraph(char *table, struct customTrack *ct)
/* Show schema on wiggle format custom track. */
{
hPrintf("<B>ChromGraph Custom Track ID:</B> %s<BR>\n", table);
hPrintf("ChromGraph custom tracks are stored in a dense binary format.");
}

static void showSchemaCtMaf(char *table, struct customTrack *ct)
/* Show schema on maf format custom track. */
{
hPrintf("<B>MAF Custom Track ID:</B> %s<BR>\n", table);
hPrintf("For formatting information see: ");
hPrintf("<A HREF=\"../goldenPath/help/customTrack.html#MAF\">MAF</A> ");
hPrintf("format.");

struct sqlConnection *conn = hAllocConnProfile(CUSTOM_TRACKS_PROFILE, CUSTOM_TRASH);
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
hPrintf("<A HREF=\"../goldenPath/help/customTrack.html#BED\">BED</A> ");
hPrintf("format.");

if (ct->dbTrack)
    {
    struct sqlConnection *conn = hAllocConnProfile(CUSTOM_TRACKS_PROFILE, CUSTOM_TRASH);
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

static void showSchemaCt(char *table)
/* Show schema on custom track. */
{
struct customTrack *ct = lookupCt(table);
char *type = ct->tdb->type;
if (startsWithWord("wig", type))
    showSchemaCtWiggle(table, ct);
else if (startsWithWord("chromGraph", type))
    showSchemaCtChromGraph(table, ct);
else if (startsWithWord("bed", type) || startsWithWord("bedGraph", type))
    showSchemaCtBed(table, ct);
else if (startsWithWord("maf", type))
    showSchemaCtMaf(table, ct);
else
    errAbort("Unrecognized customTrack type %s", type);
}

static void showSchemaWiki(struct trackDb *tdb, char *table)
/* Show schema for the wikiTrack. */
{
hPrintf("<B>User annotations to UCSC genes or genome regions</B><BR>\n");
showSchemaDb(wikiDbName(), tdb, table);
}

static void showSchema(char *db, struct trackDb *tdb, char *table)
/* Show schema to open html page. */
{
if (isCustomTrack(table))
    showSchemaCt(table);
else if (sameWord(table, WIKI_TRACK_TABLE))
    showSchemaWiki(tdb, table);
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
if (sameString(curTrack->tableName, curTable))
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
    struct trackDb *sTdb = NULL;
    for (sTdb = curTrack->subtracks;  sTdb != NULL;  sTdb = sTdb->next)
	{
	if (sameString(sTdb->tableName, curTable))
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
    struct trackDb *track = curTrack;
    char *table = connectingTableForTrack(curTable);
    htmlOpen("Schema for %s - %s", track->shortLabel, track->longLabel);
    showSchema(database, curTrack, table);
    htmlClose();
    }
else
    doTableSchema(database, curTable, conn);
}

