/* hgTables - Get table data associated with tracks and intersect tracks. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "htmshell.h"
#include "cheapcgi.h"
#include "cart.h"
#include "jksql.h"
#include "hdb.h"
#include "web.h"
#include "hui.h"
#include "trackDb.h"
#include "grp.h"
#include "hgTables.h"

static char const rcsid[] = "$Id: hgTables.c,v 1.5 2004/07/13 09:41:45 kent Exp $";


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

static void vaHtmlOpen(char *format, va_list args)
/* Start up a page that will be in html format. */
{
puts("Content-Type:text/html\n");
cartVaWebStart(cart, format, args);
}

static void htmlOpen(char *format, ...)
/* Start up a page that will be in html format. */
{
va_list args;
va_start(args, format);
vaHtmlOpen(format, args);
}

static void htmlClose()
/* Close down html format page. */
{
cartWebEnd();
}

/* --------------- Text Mode Helpers ----------------- */

static void textWarnHandler(char *format, va_list args)
/* Text mode error message handler. */
{
char *hLine =
"---------------------------------------------------------------------------\n";
if (format != NULL) {
    fflush(stdout);
    fprintf(stderr, "%s", hLine);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    fprintf(stderr, "%s", hLine);
    }
}

static void textAbortHandler()
/* Text mode abort handler. */
{
exit(-1);
}

static void textOpen()
/* Start up page in text format. (No need to close this). */
{
printf("Content-Type: text/plain\n\n");
pushWarnHandler(textWarnHandler);
pushAbortHandler(textAbortHandler);
}

/* --------- Test Page --------------------- */

void doTest(struct sqlConnection *conn)
/* Put up a page to see what happens. */
{
textOpen();
hPrintf("%s\n", cartUsualString(cart, "test", "n/a"));
hPrintf("All for now\n");
}

/* --------- Initial Page ------------------ */

static struct grp *makeGroupList(struct sqlConnection *conn, 
	struct trackDb *trackList)
/* Get list of groups that actually have something in them. */
{
struct sqlResult *sr;
char **row;
struct grp *groupList = NULL, *group;
struct hash *groupsInTrackList = newHash(0);
struct hash *groupsInDatabase = newHash(0);
struct trackDb *track;

/* Stream throught track list building up hash of active groups. */
for (track = trackList; track != NULL; track = track->next)
    {
    if (!hashLookup(groupsInTrackList,track->grp))
        hashAdd(groupsInTrackList, track->grp, NULL);
    }

/* Scan through group table, putting in ones where we have data. */
sr = sqlGetResult(conn,
    "select * from grp order by priority");
while ((row = sqlNextRow(sr)) != NULL)
    {
    group = grpLoad(row);
    if (hashLookup(groupsInTrackList, group->name))
	{
	slAddTail(&groupList, group);
	hashAdd(groupsInDatabase, group->name, group);
	}
    else
        grpFree(&group);
    }
sqlFreeResult(&sr);

/* Do some error checking for tracks with group names that are
 * not in database.  Just warn about them. */
for (track = trackList; track != NULL; track = track->next)
    {
    if (!hashLookup(groupsInDatabase, track->grp))
         warn("Track %s has group %s, which isn't in grp table",
	 	track->tableName, track->grp);
    }
hashFree(&groupsInTrackList);
hashFree(&groupsInDatabase);
return groupList;
}

struct grp *findGroup(struct grp *groupList, char *name)
/* Return named group in list, or NULL if not found. */
{
struct grp *group;
for (group = groupList; group != NULL; group = group->next)
    if (sameString(name, group->name))
        return group;
return NULL;
}

struct grp *findSelectedGroup(struct grp *groupList)
/* Find user-selected group if possible.  If not then
 * go to various levels of defaults. */
{
char *defaultGroup = "genes";
char *name = cartUsualString(cart, hgtaGroup, defaultGroup);
struct grp *group = findGroup(groupList, name);
if (group == NULL)
    group = findGroup(groupList, defaultGroup);
if (group == NULL)
    group = groupList;
return group;
}

int trackDbCmpShortLabel(const void *va, const void *vb)
/* Sort track by shortLabel. */
{
const struct trackDb *a = *((struct trackDb **)va);
const struct trackDb *b = *((struct trackDb **)vb);
return strcmp(a->shortLabel, b->shortLabel);
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

void jsDropDownCarryOver(struct dyString *dy, char *var)
/* Add statement to carry-over drop-down item to dy. */
{
dyStringPrintf(dy, "document.hiddenForm.%s.value=", var);
dyStringPrintf(dy, "document.mainForm.%s.options", var);
dyStringPrintf(dy, "[document.mainForm.%s.selectedIndex].value;", var);
}

void jsTextCarryOver(struct dyString *dy, char *var)
/* Add statement to carry-over text item to dy. */
{
dyStringPrintf(dy, 
    " document.hiddenForm.%s.value=document.mainForm.%s.value;",
    var, var);
}

struct dyString *onChangeStart()
/* Start up a javascript onChange command */
{
struct dyString *dy = dyStringNew(1024);
dyStringAppend(dy, "onChange=\"");
jsDropDownCarryOver(dy, hgtaTrack);
jsDropDownCarryOver(dy, hgtaGroup);
dyStringAppend(dy, 
	" document.hiddenForm.hgta_regionType.value=regionType;");
jsTextCarryOver(dy, hgtaRange);
jsTextCarryOver(dy, hgtaOffsetStart);
jsTextCarryOver(dy, hgtaOffsetEnd);
jsDropDownCarryOver(dy, hgtaOffsetRelativeTo);
jsDropDownCarryOver(dy, hgtaOutputType);
return dy;
}

char *onChangeEnd(struct dyString **pDy)
/* Finish up javascript onChange command. */
{
dyStringAppend(*pDy, " document.hiddenForm.submit();\"");
return dyStringCannibalize(pDy);
}

char *onChangeOrg()
/* Return javascript executed when they change organism. */
{
struct dyString *dy = onChangeStart();
jsDropDownCarryOver(dy, "org");
dyStringAppend(dy, " document.hiddenForm.db.value=0;");
return onChangeEnd(&dy);
}

char *onChangeDb()
/* Return javascript executed when they change database. */
{
struct dyString *dy = onChangeStart();
jsDropDownCarryOver(dy, "db");
return onChangeEnd(&dy);
}

char *onChangeGroup()
/* Return javascript executed when they change group. */
{
struct dyString *dy = onChangeStart();
jsDropDownCarryOver(dy, "db");
jsDropDownCarryOver(dy, "org");
return onChangeEnd(&dy);
}

void makeRegionButton(char *val, char *selVal)
/* Make region radio button including a little Javascript
 * to save selection state. */
{
hPrintf("<INPUT TYPE=RADIO NAME=\"hgtaRegionType\"");
hPrintf(" VALUE=\"%s\"", val);
hPrintf(" onClick=\"regionType='%s';\"", val);
if (sameString(val, selVal))
    hPrintf(" CHECKED");
hPrintf(">");
}

void showMainControlTable(struct sqlConnection *conn)
/* Put up table with main controls for main page. */
{
hPrintf("<TABLE BORDER=0>\n");

/* Print genome and assembly line. */
    {
    hPrintf("<TR><TD><B>genome:</B>\n");
    printGenomeListHtml(database, onChangeOrg());
    hPrintf("<B>assembly:</B>\n");
    printAssemblyListHtml(database, onChangeDb());
    hPrintf("</TD></TR>\n");
    }

/* Print group and track line. */
    {
    struct grp *group, *groupList = makeGroupList(conn, fullTrackList);
    struct grp *selGroup = findSelectedGroup(groupList);
    struct trackDb *track, *selTrack;

    hPrintf("<TR><TD><B>group:</B>\n");
    hPrintf("<SELECT NAME=%s %s>\n", hgtaGroup, onChangeGroup());
    for (group = groupList; group != NULL; group = group->next)
        {
	hPrintf(" <OPTION VALUE=%s%s>%s\n", group->name,
	    (group == selGroup ? " SELECTED" : ""),
	    group->label);
	}
    hPrintf(" <OPTION VALUE=all%s>All Tracks\n",
	    (group == selGroup ? " SELECTED" : ""));
    hPrintf("</SELECT>\n");

    hPrintf("<B>track:</B>\n");
    if (selGroup == NULL)   /* All Tracks */
	slSort(&fullTrackList, trackDbCmpShortLabel);
    selTrack = findSelectedTrack(fullTrackList, selGroup);
    hPrintf("<SELECT NAME=%s>\n", hgtaTrack);
    for (track = fullTrackList; track != NULL; track = track->next)
        {
	if (selGroup == NULL || sameString(selGroup->name, track->grp))
	    hPrintf(" <OPTION VALUE=%s%s>%s\n", track->tableName,
		(track == selTrack ? " SELECTED" : ""),
		track->shortLabel);
	}
    hPrintf("</SELECT>\n");
    hPrintf("</TD></TR>\n");
    }

/* Region line */
    {
    char *regionType = cartUsualString(cart, hgtaRegionType, "genome");
    boolean doEncode = sameWord(genome, "human");
    char *range = cartUsualString(cart, hgtaRange, "");

    hPrintf("<TR><TD><B>region:</B>\n");

    /* If regionType not allowed force it to "genome". */
    if (!(sameString(regionType, "genome") ||
        sameString(regionType, "range") ||
	(doEncode && sameString(regionType, "encode") ) ) )
	regionType = "genome";
    hPrintf("<SCRIPT>\n");
    hPrintf("var regionType='%s';\n", regionType);
    hPrintf("</SCRIPT>\n");
    makeRegionButton("genome", regionType);
    hPrintf(" genome ");
    if (doEncode)
        {
	makeRegionButton("encode", regionType);
	hPrintf(" ENCODE ");
	}
    makeRegionButton("range", regionType);
    hPrintf(" range ");
    cgiMakeTextVar(hgtaRange, range, 29);
    hPrintf("</TD></TR>\n");
    }

/* Select identifiers line. */
    {
    hPrintf("<TR><TD><B>select identifiers:</B>\n");
    cgiMakeButton(hgtaDoPasteIdentifiers, "Paste List");
    hPrintf(" ");
    cgiMakeButton(hgtaDoUploadIdentifiers, "Upload List");
    hPrintf("</TD></TR>\n");
    }

/* Filter line. */
    {
    hPrintf("<TR><TD><B>filter:</B>\n");
    cgiMakeButton(hgtaDoFilterPage, "Create");
    hPrintf("</TD></TR>\n");
    }

/* Offset line. */
    {
    char *start = cartUsualString(cart, hgtaOffsetStart, "");
    char *end = cartUsualString(cart, hgtaOffsetEnd, "");
    char *rel = cartUsualString(cart, hgtaOffsetRelativeTo, "both");
    static char *relMenu[3] = {"both", "start", "end"};
    hPrintf("<TR><TD><B>offset:</B>\n");
    hPrintf(" start: ");
    cgiMakeTextVar(hgtaOffsetStart, start, 6);
    hPrintf("\n");
    hPrintf(" end: ");
    cgiMakeTextVar(hgtaOffsetEnd, end, 6);
    hPrintf("\n");
    hPrintf(" relative to: ");
    cgiMakeDropListFull(hgtaOffsetRelativeTo, relMenu, relMenu,
    	ArraySize(relMenu), rel, NULL);
    hPrintf("</TD></TR>\n");
    }

/* Output line. */
    {
    char *outputType = cartUsualString(cart, hgtaOutputType, outPrimaryTable);
    static char *symbols[] = 
        {outPrimaryTable, outSequence, outSelectedFields, outSchema,
	 outStats, outBed, outGtf, outCustomTrack };
    static char *labels[] =
        {"primary table", "sequence", "selected fields", "schema",
	 "statistics", "BED", "GFF", "custom track"};
    hPrintf("<TR><TD><B>output:</B>\n");
    cgiMakeDropListFull(hgtaOutputType, labels, symbols, 
    	ArraySize(symbols), outputType, NULL);
    hPrintf("</TD></TR>\n");
    }

/* Submit buttons. */
    {
    hPrintf("<TR><TD>\n");
    cgiMakeButton(hgtaDoTopSubmit, "Submit");
    hPrintf(" ");
    cgiMakeButton(hgtaDoIntersect, "Intersect");
    hPrintf("</TD></TR>\n");
    }

hPrintf("</TABLE>\n");
}

void beginPage(struct sqlConnection *conn)
/* Put up the first page user sees. */
{
htmlOpen("Table Browser");
hPrintf("%s", 
  "Use this program to get the data associated with a track in text "
  "format, to calculate intersections between tracks, and to fetch "
  "DNA sequence covered by a track. Please <A HREF=\"#Help\">see "
  "below</A> for a description of the controls in this form. ");

/* Main form. */
hPrintf("<FORM ACTION=\"../cgi-bin/hgTables\" NAME=\"mainForm\" METHOD=GET>\n");
cartSaveSession(cart);
showMainControlTable(conn);
hPrintf("</FORM>\n");

/* Hidden form - for benefit of javascript. */
    {
    static char *saveVars[] = {
      "org", "db", hgtaGroup, hgtaTrack, hgtaRegionType,
      hgtaRange, hgtaOffsetStart, hgtaOffsetEnd, hgtaOffsetRelativeTo,
      hgtaOutputType, };
    int i;

    hPrintf("<FORM ACTION=\"../cgi-bin/hgTables\" METHOD=\"GET\" NAME=\"hiddenForm\">\n");
    cartSaveSession(cart);
    for (i=0; i<ArraySize(saveVars); ++i)
	hPrintf("<input type=\"hidden\" name=\"%s\" value=\"\">\n", saveVars[i]);
    puts("</FORM>");
    }

webNewSection("<A NAME=\"Help\"></A>Using the Table Browser\n");
hPrintf("%s",
"Here is a line-by-line description of the controls:\n"
"<UL>\n"
"   <LI><B>organism: </B>Which organism to work with.</LI>\n"
"   \n"
"   <LI><B>assembly: </B>Which version of that organism's genome\n"
"   sequence to use.</LI>\n"
"   \n"
"   <LI><B>group: </B>Selects which group the track is in. This\n"
"   corresponds to the grouping of tracks as separated by the blue\n"
"   bars in the genome browser.</LI>\n"
"   \n"
"   <LI><B>track: </B>Which track to work with. (Hint: there is a lot\n"
"   of data associated with the known genes, RefSeq, and Ensembl\n"
"   tracks, all of which are in the Gene and Gene Prediction\n"
"   group).</LI>\n"
"   \n"
"   <LI><B>region: </B>With this one can restrict the query to a\n"
"   particular chromosome or region. Select genome to apply the query\n"
"   to the entire genome.  Alternatively select range and type in either \n"
"   the chromosome name (such as chrX) or something like chrX:100000-200000 \n"
"   in the text box.</LI>\n"
"   \n"
"   <LI><B>select identifiers: </B>You can restrict your attention to\n"
"   a items in the track that match a list of identifiers (for\n"
"   instance RefSeq accessions for the RefSeq track) here. Ignore this\n"
"   line if you wish to consider all items in the track.</LI>\n"
"   \n"
"   <LI><B>filter: </B>You can restrict the query to only items that\n"
"   match certain criteria by creating a filter. For instance you\n"
"   could restrict your attention only to genes of a single exon.</LI>\n"
"   \n"
"   <LI><B>offset: </B>In many cases the output of the track\n"
"   intersector is a list of regions in the genome or DNA&nbsp;from\n"
"   these regions. The offset control lets you adjust the final output\n"
"   relative to these regions. The 'start' field specifies the\n"
"   relative offset of the starting position. A negative number here\n"
"   will make the region bigger, a positive number will make it\n"
"   smaller. A positive number in the end field makes the region\n"
"   bigger. The 'relative to' control has three values. If it is\n"
"   'start' then both start and end offsets are relative to the start\n"
"   position in the original region. Similarly if it is 'end' then the\n"
"   offsets are relative to the end position in the original region.\n"
"   If the 'relative to' control is 'both' (the default)&nbsp;then the\n"
"   start is relative to the original start position, and the end is\n"
"   relative to the original end position. Here are some examples of\n"
"   common usages:\n"
"   \n"
"   <UL>\n"
"      <LI>Promoters: start: -1000 end: 50 relative to: start</LI>\n"
"      \n"
"      <LI>10,000 bases on either side of a gene: start: -10000 end\n"
"      10000 relative to:&nbsp;both</LI>\n"
"      \n"
"      <LI>The last 50 bases of a gene: start: -50 end: 0 relative to:\n"
"      end</LI>\n"
"   </UL>\n"
"   </LI>\n"
"   \n"
"   <LI><B>output: </B>This controls the output format. Options\n"
"   include:\n"
"   \n"
"   <UL>\n"
"      <LI>Primary Table - the primary table associated with the track\n"
"      in a tab-separated format suitable for import into spreadsheets\n"
"      and relational databases.  This format is reasonably human readable \n"
"      as well.\n"
"      <LI>Sequence - get DNA or for some tracks protein sequence\n"
"      associated with track.</LI>\n"
"      <LI>Selected Fields - select fields from the various tables associated\n"
"      with the track.  The result is tab-separated\n"
"      <LI>Schema - output description of tables associated with track, but\n"
"      not the actual data in the tables\n"
"      <LI>Statistics - this calculates summary information such as\n"
"      number of items and how many bases are coverd by the results of\n"
"      a query.</LI>\n"
"      <LI>BED - saves the positions of all the items in a standard\n"
"      UCSC Browser format.</LI>\n"
"      \n"
"      <LI>GTF - saves the positions of all the items in a standard\n"
"      format for gene predictions. (Both BED&nbsp;and&nbsp;GTF can be\n"
"      used as the basis for custom tracks).</LI>\n"
"      \n"
"      <LI>Custom Track - this creates a custom track in the browser\n"
"      based on the results of the query.</LI>\n"
"      \n"
"   </UL>\n"
"   </LI>\n"
"   \n"
"   <LI><B>submit: </B>Press this button to have the server compute\n"
"   and send the output.</LI>\n"
"   \n"
"   <LI><B>intersection: </B>Press this button if you wish to combine\n"
"   this query with another query. This can be useful for such things\n"
"   as finding all SNPs which intersect RefSeq coding regions.</LI>\n"
"</UL>\n");
htmlClose();
}

void doPasteIdentifiers(struct sqlConnection *conn)
/* Respond to paste identifiers button. */
{
htmlOpen("Paste Identifiers");
uglyf("Theoretically pasting identifiers.");
htmlClose();
}

void doUploadIdentifiers(struct sqlConnection *conn)
/* Respond to upload identifiers button. */
{
htmlOpen("Upload Identifiers");
uglyf("Theoretically uploading identifiers.");
htmlClose();
}

void doFilterPage(struct sqlConnection *conn)
/* Respond to filter create/edit button */
{
htmlOpen("Table Browser Filter");
uglyf("Theoretically making filter.");
htmlClose();
}

void doOutPrimaryTable(struct sqlConnection *conn)
/* Dump out primary table. */
{
textOpen();
uglyf("The primary table will be dumped in a text mode.");
}

void doOutSchema(struct sqlConnection *conn)
/* Dump schema. */
{
char *track = cartString(cart, hgtaTrack);
htmlOpen("Local Schema for %s", track);
uglyf("Not much here yet");
htmlClose();
}

void doTopSubmit(struct sqlConnection *conn)
/* Respond to submit button on top level page.
 * This basically just dispatches based on output type. */
{
char *output = cartString(cart, hgtaOutputType);
if (sameString(output, outPrimaryTable))
    doOutPrimaryTable(conn);
else if (sameString(output, outSchema))
    doOutSchema(conn);
else
    errAbort("Don't know how to handle %s output yet", output);
}

void doIntersect(struct sqlConnection *conn)
/* Respond to intersection button. */
{
htmlOpen("Table Browser Intersect");
uglyf("Processing intersect button... too stupid to do anything real...");
htmlClose();
}

void dispatch()
/* Check phase variable and based on that dispatch to 
 * appropriate page-generator. */
{
struct sqlConnection *conn = NULL;
getDbAndGenome(cart, &database, &genome);
hSetDb(database);
conn = hAllocConn();
fullTrackList = hTrackDb(NULL);

if (cartVarExists(cart, hgtaDoTest))
    doTest(conn);
else if (cartVarExists(cart, hgtaDoTopSubmit))
    doTopSubmit(conn);
else if (cartVarExists(cart, hgtaDoIntersect))
    doIntersect(conn);
else if (cartVarExists(cart, hgtaDoPasteIdentifiers))
    doPasteIdentifiers(conn);
else if (cartVarExists(cart, hgtaDoUploadIdentifiers))
    doUploadIdentifiers(conn);
else if (cartVarExists(cart, hgtaDoFilterPage))
    doFilterPage(conn);
else	/* Default - put up initial page. */
    beginPage(conn);
cartRemovePrefix(cart, hgtaDo);
}

char *excludeVars[] = {"Submit", "submit", NULL};

void hgTables()
/* hgTables - Get table data associated with tracks and intersect tracks. */
{
oldVars = hashNew(8);

/* Sometimes we output HTML and sometimes plain text; let each outputter 
 * take care of headers instead of using a fixed cart*Shell(). */
cart = cartAndCookieNoContent(hUserCookie(), excludeVars, oldVars);
dispatch();
cartCheckout(&cart);
}

int main(int argc, char *argv[])
/* Process command line. */
{
htmlPushEarlyHandlers(); /* Make errors legible during initialization. */
cgiSpoof(&argc, argv);
hgTables();
return 0;
}
