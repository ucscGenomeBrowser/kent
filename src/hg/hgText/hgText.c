#include "common.h"
#include "hCommon.h"
#include "linefile.h"
#include "cheapcgi.h"
#include "genePred.h"
#include "dnaseq.h"
#include "dystring.h"
#include "htmshell.h"
#include "cart.h"
#include "jksql.h"
#include "knownInfo.h"
#include "hdb.h"
#include "hgConfig.h"
#include "hgFind.h"
#include "hui.h"
#include "hash.h"
#include "fa.h"
#include "psl.h"
#include "nib.h"
#include "web.h"
#include "dbDb.h"
#include "kxTok.h"
#include "hgSeq.h"
#include "featureBits.h"
#include "bed.h"
#include "portable.h"
#include "customTrack.h"

static char *hgFixed = "hgFixed";

/* doMiddle() sets these: */
struct cart *cart = NULL;
char *database = NULL;
char *freezeName;
boolean tableIsPositional;
boolean tableIsSplit;
char fullTableName[256];
char *position;
char *chrom;
int winStart;
int winEnd;
boolean allGenome;

/* copied from hgGateway: */
static char * const onChangeText = "onchange=\"document.orgForm.org.value = document.mainForm.org.options[document.mainForm.org.selectedIndex].value; document.orgForm.submit();\"";

/* Droplist menu for selecting output type: */
#define allFieldsPhase      "Tab-separated, All fields"
#define chooseFieldsPhase   "Tab-separated, Choose fields..."
#define seqOptionsPhase     "FASTA (DNA sequence)..."
#define gffPhase            "GTF"
#define bedOptionsPhase     "BED/Custom Track..."
#define linksPhase          "Hyperlinks to Genome Browser"
char *outputTypePosMenu[] =
{
    allFieldsPhase,
    chooseFieldsPhase,
    seqOptionsPhase,
    bedOptionsPhase,
    linksPhase,
    gffPhase,
};
int outputTypePosMenuSize = 6;
char *outputTypeNonPosMenu[] =
{
    allFieldsPhase,
    chooseFieldsPhase,
};
int outputTypeNonPosMenuSize = 2;
/* Other values that the "phase" var can take on: */
#define chooseTablePhase    "table"
#define outputOptionsPhase  "Advanced query..."
#define getOutputPhase      "Get results"
#define getSomeFieldsPhase  "Get these fields"
#define getSequencePhase    "Get sequence"
#define getBedPhase         "Get BED"
#define intersectOptionsPhase "Intersect Results..."
/* Old "phase" values handled for backwards compatibility: */
#define oldAllFieldsPhase   "Get all fields"

/* Droplist menus for filtering on fields: */
char *ddOpMenu[] =
{
    "does",
    "doesn't"
};
int ddOpMenuSize = 2;

char *logOpMenu[] =
{
    "AND",
    "OR"
};
int logOpMenuSize = 2;

char *cmpOpMenu[] =
{
    "ignored",
    "in range",
    "<",
    "<=",
    "=",
    "!=",
    ">=",
    ">"
};
int cmpOpMenuSize = 8;

/* Droplist menu for custom track visibility: */
char *ctVisMenu[] =
{
    "hide",
    "dense",
    "full",
};
int ctVisMenuSize = 3;


void positionLookup(char *phase)
/* print the location and a jump button */
{
cgiMakeTextVar("position", position, 30);
cgiMakeHiddenVar("origPhase", phase);
cgiMakeButton("submit", "Look up");
}

void positionLookupSamePhase()
/* print the location and a jump button if the table is positional */
{
if (tableIsPositional)
    {
    puts("position: ");
    positionLookup(cgiString("phase"));
    puts("<P>");
    }
else
    {
    cgiMakeHiddenVar("position", position);
    }
}


void doGateway()
/* Table Browser gateway page: select organism, db, position */
{
char *organism;
char *defaultPosition;
char *position;

getDbAndOrganism(cart, &database, &organism);
freezeName = hFreezeFromDb(database);
defaultPosition = hDefaultPos(database);
position = cartUsualString(cart, "position", defaultPosition);
webStart(cart, "Table Browser: %s: Choose Organism &amp; Assembly",
	 freezeName);

puts(
     "<P>This tool allows you to download portions of the database used 
	by the genome browser in several output formats.
	Please enter a position in the genome (or enter \"genome\" to 
        search all chromosomes) and press the submit button:\n"
     );

printf("(Use <A HREF=\"/cgi-bin/hgBlat?db=%s&hgsid=%d\">BLAT Search</A> to 
        locate a particular sequence in the genome.)\n",
       database, cartSessionId(cart));
cgiMakeHiddenVar("org", organism);

puts(
"<center>\n"
"<table bgcolor=\"cccc99\" border=\"0\" CELLPADDING=1 CELLSPACING=0>\n"
"<tr><td>\n"
"<table BGCOLOR=\"FEFDEF\" BORDERCOLOR=\"CCCC99\" BORDER=0 CELLPADDING=0 CELLSPACING=0>\n"  
"<tr><td>\n"
);

puts(
"<table bgcolor=\"FFFEF3\" border=0>\n"
"<tr>\n"
"<td>\n"
);
printf("<FORM ACTION=\"%s\" NAME=\"mainForm\" METHOD=\"GET\" ENCTYPE=\"multipart/form-data\">\n", hgTextName());
cartSaveSession(cart);
puts(
"<input TYPE=\"IMAGE\" BORDER=\"0\" NAME=\"hgt.dummyEnterButton\" src=\"/images/DOT.gif\">\n"
"<table><tr>\n"
"<td align=center valign=baseline>genome</td>\n"
"<td align=center valign=baseline>assembly</td>\n"
"<td align=center valign=baseline>position</td>\n"
);

puts("<tr><td align=center>\n");
printOrgListHtml(database, onChangeText);
puts("</td>\n");

puts("<td align=center>\n");
printAssemblyListHtml(database);
puts("</td>\n");

puts("<td align=center>\n");
cgiMakeTextVar("position", position, 30);
cgiMakeHiddenVar("phase", chooseTablePhase);
puts("</td><td>");
cgiMakeButton("submit", "Submit");
puts(
"</td></tr></table>\n"
"</FORM>"
"</td></tr>\n"
"</table>\n"
"</td></tr></table>\n"
"</td></tr></table>\n"
"</center>\n"
);

printf("<FORM ACTION=\"%s\" METHOD=\"GET\" NAME=\"orgForm\"><input type=\"hidden\" name=\"org\" value=\"%s\">\n", hgTextName(), organism);
cartSaveSession(cart);
puts("</FORM>");

hgPositionsHelpHtml(organism);
webEnd();

freez(&defaultPosition);
position = NULL;
}

static boolean allLetters(char *s)
/* returns true if the string only has letters number and underscores */
{
int i;
for (i = 0; i < strlen(s); i++)
    if (!isalnum(s[i]) && s[i] != '_')
	return FALSE;

return TRUE;
}

void checkIsAlpha(char *desc, char *word)
/* make sure that the table name doesn't have anything "weird" in it */
{
if (!allLetters(word))
    webAbort("Error", "Invalid %s \"%s\".", desc, word);
}


static boolean findPositionInGenome(char *spec, char **retChromName, 
				    int *retWinStart, int *retWinEnd)
/* process the position information given in spec and gives a chrom name
 * as well as a start and end values on that chrom */
/* NOTE: this is very close to hgFind's findGenomePos, but it handles 
 * the web headers and says hgText instead of hgTracks.
 */
{ 
struct hgPositions *hgp;
struct hgPos *pos;

hgp = hgPositionsFind(spec, "", FALSE, NULL);

if (hgp == NULL || hgp->posCount == 0)
    {
    hgPositionsFree(&hgp);
    webAbort("Not found", "Sorry, couldn't locate %s in genome database\n",
	     spec);
    return TRUE;
    }
if ((pos = hgp->singlePos) != NULL)
    {
    *retChromName = pos->chrom;
    *retWinStart = pos->chromStart;
    *retWinEnd = pos->chromEnd;
    hgPositionsFree(&hgp);
    return TRUE;
    }
else
    {
    webStart(cart, "Table Browser: %s: Select Position",
	     freezeName);
    hgPositionsHtml(hgp, stdout, FALSE, NULL);
    hgPositionsFree(&hgp);
    webEnd();
    return FALSE;
    }
}


char *getPosition(char **retChrom, int *retStart, int *retEnd)
{
char *pos = cgiOptionalString("position");

/* if the position information is not given, get it */
if (pos == NULL)
    pos = "";
if (pos[0] == '\0')
    {
    doGateway();
    return NULL;
    }
if (strcmp(pos, "genome"))
    {
    if (! findPositionInGenome(pos, retChrom, retStart, retEnd))
	return NULL;
    }
return(pos);
}


char *getTableVar()
{
char *table  = cgiOptionalString("table");
char *table0 = cgiOptionalString("table0");
char *table1 = cgiOptionalString("table1");
	
if (table != 0 && strcmp(table, "Choose table") == 0)
    table = 0;

if (table0 != 0 && strcmp(table0, "Choose table") == 0)
    table0 = 0;
	
if (table1 != 0 && strcmp(table1, "Choose table") == 0)
    table1 = 0;

if (table != 0)
    return table;
else if (table0 != 0)
    return table0;
else
    return table1;
}

char *getTableName()
{
char *val, *ptr;
	
val = getTableVar();
if (val == NULL)
    return val;
else if ((ptr = strchr(val, '.')) != NULL)
    return ptr + 1;
else
    return val;
}

char *getTableDb()
{
char *val, *ptr;
	
val = cloneString(getTableVar());
if ((ptr = strchr(val, '.')) != NULL)
    *ptr = 0;
return(val);
}

char *getTrackName()
{
char *trackName = cloneString(getTableName());
if (startsWith("chrN_", trackName))
    strcpy(trackName, trackName+strlen("chrN_"));
return trackName;
}

char *getTable2Var()
{
char *table2  = cgiOptionalString("table2");
if ((table2 != NULL) && sameString(table2, "Choose table"))
    table2 = NULL;
return table2;
}

char *getTable2Name()
{
char *val, *ptr;
	
val = getTable2Var();
if (val == NULL)
    return val;
else if ((ptr = strchr(val, '.')) != NULL)
    return ptr + 1;
else
    return val;
}

char *getTable2Db()
{
char *val, *ptr;
	
val = cloneString(getTable2Var());
if (val == NULL)
    return(val);
if ((ptr = strchr(val, '.')) != NULL)
    *ptr = 0;
return(val);
}

char *getTrack2Name()
{
char *trackName = cloneString(getTable2Name());
if (startsWith("chrN_", trackName))
    strcpy(trackName, trackName+strlen("chrN_"));
return trackName;
}

boolean tableExists(char *table)
{
return ((sameString(database, getTableDb()) && hTableExists(table)) ||
	(sameString(hGetDb2(), getTableDb()) && hTableExists2(table)));
}

void checkTableExists(char *table)
{
if (! tableExists(table))
    webAbort("No data", "Table %s (%s) does not exist in database %s.",
	     getTableName(), table, getTableDb());
}

void checkTable2Exists(char *table)
{
if (! tableExists(table))
    webAbort("No data", "Table %s (%s) does not exist in database %s.",
	     getTable2Name(), table, getTable2Db());
}

static boolean existsAndEqual(char *var, char *value)
/* returns true is the given CGI var exists and equals value */
{
if (cgiOptionalString(var) != 0 && sameString(cgiOptionalString(var), value))
    return TRUE;
else
    return FALSE;
}


static void printSelectOptions(struct hashEl *optList, char *name,
			       char *valPrefix)
/* this function is used to iterates over a hash el list and prints out
 * the name in a select input form */
{
struct hashEl *cur;
for (cur = optList;  cur != NULL;  cur = cur->next)
    {
    if (existsAndEqual(name, cur->name))
	printf("<OPTION SELECTED VALUE=\"%s.%s\">%s</OPTION>\n",
	       valPrefix, cur->name, cur->name);
    else
	printf("<OPTION VALUE=\"%s.%s\">%s</OPTION>\n",
	       valPrefix, cur->name, cur->name);
    }
}


int compareTable(const void *elem1,  const void *elem2)
/* compairs two hash element by name */
{
struct hashEl* a = *((struct hashEl **)elem1);
struct hashEl* b = *((struct hashEl **)elem2);
	
return strcmp(a->name, b->name);
}

void getTableNames(char *dbName, struct sqlConnection *conn,
		   struct hash *posTableHash, struct hash *nonposTableHash)
/* iterate through all the tables and store the positional ones in a list */
{
struct sqlResult *sr;
char **row;
char query[256];
char name[128];
char chrom[32];
char post[64];

strcpy(query, "SHOW TABLES");
sr = sqlGetResult(conn, query);
while((row = sqlNextRow(sr)) != NULL)
    {
    if (strcmp(row[0], "all_est") == 0 || strcmp(row[0], "all_mrna") == 0)
	continue;

    /* if table name is of the form, chr*_random_* or chr*_*: */
    if (sscanf(row[0], "chr%32[^_]_random_%64s", chrom, post) == 2 ||
	sscanf(row[0], "chr%32[^_]_%64s", chrom, post) == 2)
	{
	snprintf(name, sizeof(name), "chrN_%s", post);
	// If a chrN_ table is already in the (positional) hash, 
	// don't bother looking up its fields.
	if (hashLookup(posTableHash, name))
	    continue;
	}
    else
	{
	strncpy(name, row[0], sizeof(name));
	}

    if (hFindChromStartEndFieldsDb(dbName, row[0], query, query, query))
	hashStoreName(posTableHash, cloneString(name));
    else
	hashStoreName(nonposTableHash, cloneString(name));
    }
sqlFreeResult(&sr);
}

void categorizeTables(struct hashEl **retPosTableList,
		      struct hashEl **retNonposTableList)
/* Return sorted lists of positional and non-positional table names 
 * from the current database and hgFixed. */
{
struct sqlConnection *conn;
struct hash *posTableHash = newHash(7);
struct hashEl *posTableList;
struct hash *nonposTableHash = newHash(7);
struct hashEl *nonposTableList;
struct hash *fixedPosTableHash = newHash(7);
struct hashEl *fixedPosTableList;
struct hash *fixedNonposTableHash = newHash(7);
struct hashEl *fixedNonposTableList;

/* get table names from the database */
conn = hAllocConn();
getTableNames(database, conn, posTableHash, nonposTableHash);
hFreeConn(&conn);
/* get table names from hgFixed too */
conn = sqlConnect(hgFixed);
getTableNames(hgFixed, conn, fixedPosTableHash, fixedNonposTableHash);
sqlDisconnect(&conn);
/* get lists of names from the hashes and sort them */
posTableList = hashElListHash(posTableHash);
slSort(&posTableList, compareTable);
nonposTableList = hashElListHash(nonposTableHash);
slSort(&nonposTableList, compareTable);
fixedPosTableList = hashElListHash(fixedPosTableHash);
slSort(&fixedPosTableList, compareTable);
fixedNonposTableList = hashElListHash(fixedNonposTableHash);
slSort(&fixedNonposTableList, compareTable);
/* append hgFixed db lists onto default db lists and return */
*retPosTableList = slCat(posTableList, fixedPosTableList);
*retNonposTableList = slCat(nonposTableList, fixedNonposTableList);
}

void doChooseTable()
/* get the table selection from the user */
{
struct hashEl *posTableList;
struct hashEl *nonposTableList;
char buf[256];

if (! sameString(position, "genome"))
    {
    sprintf(buf, "%s:%d-%d", chrom, winStart+1, winEnd);
    position = cloneString(buf);
    }

printf("<FORM ACTION=\"%s\" NAME=\"mainForm\">\n\n", hgTextName());
cartSaveSession(cart);
cgiMakeHiddenVar("db", database);
puts("<TABLE CELLPADDING=\"8\">");
puts("<TR><TD>");

puts("Choose a position: ");
puts("</TD><TD>");
positionLookup(chooseTablePhase);
puts("</TD></TR>");

puts("<TR><TD>");

categorizeTables(&posTableList, &nonposTableList);
puts("Choose a table:");
puts("</TD><TD>");
puts("<SELECT NAME=table0 SIZE=1>");
printf("<OPTION VALUE=\"Choose table\">Positional tables</OPTION>\n");
printSelectOptions(posTableList, "table0", database);
puts("</SELECT>");

puts("<SELECT NAME=table1 SIZE=1>");
printf("<OPTION VALUE=\"Choose table\">Non-positional tables</OPTION>\n");
printSelectOptions(nonposTableList, "table1", database);
puts("</SELECT>");

puts("</TD></TR>");

puts("<TR><TD>");
puts("Choose an action: ");
puts("</TD><TD>");
cgiMakeButton("phase", oldAllFieldsPhase);
cgiMakeButton("phase", outputOptionsPhase);
puts("</TD></TR>");
puts("</TABLE>");

puts("</FORM>");

hashElFreeList(&posTableList);
hashElFreeList(&nonposTableList);

webNewSection("Getting started on the Table Browser");

puts(
     "This web tool allows convenient and precise access to the primary
database tables containing the genome sequence and associated
annotation tracks. By specifying chromosomal range and table type, the
exact data set of interest can be viewed.  This tool thus makes it
unnecessary to download and manipulate the genome itself and its
massive data tracks (although that option will always remain <A
HREF=\"/\">available</A>.)" "\n"
     "<P>" "\n"
     "After each round of genome assembly, features such as mRNAs are
located within the genome by alignment. This process generates
<B>positional</B> stop-start coordinates and other descriptive data
which are then stored within MySQL relational database tables. It is
these tables that drive the graphical tracks in the Genome Browser,
meaning that the two views of the data are always in
agreement. Chromosomal coordinates usually change with each build
because of newly filled gaps or assembly procedure refinements." "\n"
     "<P>" "\n"
     "Other data is inherently <B>non-positional</B> (not tied to genome
coordinates), such as submitting author of a GenBank EST, tissue of
origin , or link to a RefSeq.  These data types are accessed
separately." "\n"
     "<P>" "\n"
     "The \"Positional tables\" selection takes precedence over the 
\"Non-positional tables\" selection.  In other words, if both a 
positional and a non-positional table are selected, the positional 
table is used and the non-positional table is ignored. " "\n"
     "<P>" "\n");
printf("Familiarize yourself with the table browser tool by a <B>few minutes
of exploration</B>. Starting at the top of the form, use the default
position (or enter a cytological band, eg 20p12, or type in a keyword
from a GenBank entry to look up). Next select a positional table from
the menu, for example, chrN_mrna.  Then choose an action: 
\"Get all fields\" will return all fields of the chrN_mrna table in 
the given position coordinate range.  \"%s\" ", outputOptionsPhase);
printf("will offer several output format choices, as well as optional
filtering constraints to refine the search.  If the chromosomal range
is too narrow, it may happen that no mRNA occurs there (meaning no
data will be returned)." "\n"
     "<P>" "\n"
     "Notice that if you look up a gene or accession number, the tool
typically returns a choice of several mRNAs.  Selecting one of these
returns the table browser to its starting place but with the
chromosomal location automatically entered in the text box." "\n"
     "<P>" "\n"
     "To search all chromosomes, enter <B>\"genome\"</B> as the position. \n"
     "<P>" "\n"
     "For either type of table, a user not familiar with what the table
offers should select \"%s\".  ", outputOptionsPhase);
puts("The fields of the table will be listed in the optional filtering
constraints section." "\n"
     );
}

void getFullTableName(char *dest, char *newChrom, char *table)
/* given a chrom return the table name of the table selected by the user */
{
char post[64];

if (newChrom == NULL)
    newChrom = "chr1";
chrom = newChrom;
if (allGenome)
    {
    winStart = 0;
    winEnd   = hChromSize(chrom);
    }
if (sscanf(table, "chrN_%64s", post) == 1)
    snprintf(dest, 256, "%s_%s", chrom, post);
else
    strncpy(dest, table, 256);

/* make sure that the table name doesn't have anything "weird" in it */
checkIsAlpha("table name", dest);
}


void doOutputOptions()
/* print out a form with output table format & filtering options */
{
struct sqlConnection *conn;
struct sqlResult *sr;
struct hashEl *posTableList;
struct hashEl *nonposTableList;
char **row;
char *table = getTableName();
char query[256];
char name[128];
char *newVal;
boolean gotFirst;

webStart(cart, "Table Browser: %s: %s", freezeName, outputOptionsPhase);
checkTableExists(fullTableName);
printf("<FORM ACTION=\"%s\" NAME=\"mainForm\">\n\n", hgTextName());
cartSaveSession(cart);
cgiMakeHiddenVar("db", database);
cgiMakeHiddenVar("table", getTableVar());
positionLookupSamePhase();

printf("<P><HR><H3> Select Output Format for Table %s </H3>\n", table);
if (tableIsPositional)
    cgiMakeDropList("outputType", outputTypePosMenu, outputTypePosMenuSize,
		    outputTypePosMenu[0]);
else
    cgiMakeDropList("outputType", outputTypeNonPosMenu,
		    outputTypeNonPosMenuSize, outputTypeNonPosMenu[0]);
cgiMakeButton("phase", getOutputPhase);

printf("<P><HR><H3> (Optional) Filter %s Records by Field Values </H3>",
       table);

snprintf(query, 256, "DESCRIBE %s", fullTableName);
if (sameString(database, getTableDb()))
    conn = hAllocConn();
else
    conn = hAllocConn2();
sr = sqlGetResult(conn, query);

puts("<TABLE><TR><TD>\n");
puts("<TABLE>\n");
gotFirst = FALSE;
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (! strstr(row[1], "blob"))
	{
	if (! gotFirst)
	    gotFirst = TRUE;
	else
	    puts(" AND </TD></TR>\n");
	printf("<TR VALIGN=BOTTOM><TD> %s </TD><TD>\n", row[0]);
	if (strstr(row[1], "char"))
	    {
	    snprintf(name, sizeof(name), "dd_%s", row[0]);
	    cgiMakeDropList(name, ddOpMenu, ddOpMenuSize, ddOpMenu[0]);
	    puts(" match </TD><TD>\n");
	    newVal = "*";
	    }
	else
	    {
	    puts(" is \n");
	    snprintf(name, sizeof(name), "cmp_%s", row[0]);
	    cgiMakeDropList(name, cmpOpMenu, cmpOpMenuSize, cmpOpMenu[0]);
	    puts("</TD><TD>\n");
	    newVal = "";
	    }
	snprintf(name, sizeof(name), "pat_%s", row[0]);
	cgiMakeTextVar(name, newVal, 20);
	}
    }
sqlFreeResult(&sr);
if (sameString(database, getTableDb()))
    hFreeConn(&conn);
else
    hFreeConn2(&conn);
puts("</TD></TR></TABLE>\n");
puts(" </TD></TR><TR VALIGN=BOTTOM><TD>");
strncpy(name, "log_rawQuery", sizeof(name));
cgiMakeDropList(name, logOpMenu, logOpMenuSize, logOpMenu[0]);
puts("Free-form query: ");
cgiMakeTextVar("rawQuery", "", 50);
puts("</TD></TR></TABLE>");
cgiMakeButton("phase", getOutputPhase);

if (tableIsPositional)
    {
    puts("<P><HR><H3> (Optional) Intersect Results with Another Table </H3>");
    puts("Note: Output Format must be FASTA, BED, Hyperlinks or GTF \n"
	 "for this feature.  (Tab-separated dumps are not supported.) <P>");
    categorizeTables(&posTableList, &nonposTableList);
    puts("Choose a positional table:");
    puts("<SELECT NAME=table2 SIZE=1>");
    printf("<OPTION VALUE=\"Choose table\">Positional tables</OPTION>\n");
    printSelectOptions(posTableList, "table2", database);
    puts("</SELECT>");
    hashElFreeList(&posTableList);
    hashElFreeList(&nonposTableList);
    
    puts("<P>");
    cgiMakeButton("phase", intersectOptionsPhase);
    }

puts("</FORM>");

webNewSection("Output Format and Query/Filtering Options");
puts(
     "<P>\n"
     "By default, the table browser retrieves all records for the specified \n"
     "coordinate range (use \"genome\" to search all chromosomes). \n"
     "To restrict the set of records retrieved from this range, you may \n"
     "enter constraints on the values of fields. \n"
     "</P>\n"
     "<H4> Individual Field Constraints </H4>"
     "<P>\n"
     "Text fields are compared to words or patterns containing wildcard \n"
     "characters.  \n"
     "Wildcards are \"*\" (to match \n"
     "0 or more characters) and \"?\" (to match a single character).  \n"
     "Each space-separated word/pattern in a text field box will be matched \n"
     "against the value of that field in each record.  \n"
     "If any word/pattern matches the value, then the record meets the \n"
     "constraint on that field. \n"
     "</P><P>\n"
     "Numeric fields are compared to table data using an operator \n"
     "such as &lt;, &gt;, != (not equal) followed by a number.  \n"
     "To specify a range, enter two numbers (start and end) separated by a \n"
     "\"-\". \n"
     "</P><P>\n"
     "The individual field constraints are combined with AND.  In other \n"
     "words, a record must meet all individual field constraints in order \n"
     "to be retrieved.  The default values will match all records, \n"
     "so the only constraints that apply are the ones that you edit.  \n"
     "</P>\n"
     "<H4> Free-form Query </H4>"
     "<P>\n"
     "When the filters above aren\'t enough, free-form queries allow \n"
     "fancier constraints that typically relate two or more field names of \n"
     "the selected table. If you are familiar with \n"
     "<A HREF=\"http://www.w3schools.com/sql/sql_where.asp\" TARGET=_blank>"
     "SQL syntax</A>, \n"
     "queries here are just \"where\" clauses (with wildcards as above). \n"
     "Queries can combine simple constraints with AND, OR, and NOT using \n"
     "parentheses as needed for clarity. \n"
     "<P> A simple constraint consists of a field name listed above, \n"
     "     a comparison operator (see below), \n"
     "     and a value: a number, string, wildcard value (see below), \n"
     "     or another field name. \n"
     "     In place of a field name, you may use \n"
     "     an arithmetic expression of numeric field names.  </P>\n"
     "<P> String or wildcard values for text comparisons must be quoted.  \n"
     "     Single or double quotes may be used.  \n"
     "     If comparing to a literal string value, \n"
     "     you may use the \"=\" (or \"!=\") operator.  \n"
     "     If comparing to a wildcard \n"
     "     value, you must use the \"LIKE\" (or \"NOT LIKE\") operator.  \n"
     "     </P> \n"
     "<P> Numeric comparison operators include "
     "&lt;, &lt;=, =, != (not equal), &gt;=, and &gt; .  \n"
     "     Arithmetic operators include +, -, *, and /.  \n"
     "     Other "
     "<A HREF=\"http://www.w3schools.com/sql/sql_where.asp\" TARGET=_blank>"
     "SQL</A> \n"
     "     comparison keywords may be used.  \n"
     "     </P> \n"
     "</P><P>\n"
     "Free-form query examples (taken from the refGene table):\n"
     "<UL>\n"
     "<LI> <FONT COLOR=\"#0000FF\">txStart = cdsStart</FONT> \n"
     "searches for gene models missing expected 5\' UTR upstream sequence \n"
     "(if strand is \'+\'; 3\' UTR downstream if strand is \'-\'). </LI>\n"
     "<LI> <FONT COLOR=\"#0000FF\">chrom NOT LIKE \"chr??\"</FONT> \n"
     "filters search to chromosomes 1-9 plus X and Y. </LI> \n"
     "<LI> <FONT COLOR=\"#0000FF\">(cdsEnd - cdsStart) &gt; 10000</FONT> \n"
     "selects genes whose coding regions span over 10 kbp. </LI> \n"
     "<LI> <FONT COLOR=\"#0000FF\">(txStart != cdsStart) AND (txEnd != cdsEnd)"
     " AND exonCount = 1</FONT> \n"
     "finds single exon genes with both 3\' and 5\' flanking UTR.</LI> \n"
     "<LI><FONT COLOR=\"#0000FF\">((cdsEnd - cdsStart) &gt; 30000) AND "
     "(exonCount=2 OR exonCount=3)</FONT> \n"
     "finds genes with long spans but only 2-3 exons.</LI>"
     "</UL>\n"
     "</P>\n"
     );

webEnd();
}


void parseNum(char *fieldName, struct kxTok **tok, struct dyString *q)
{
if (*tok == NULL)
    webAbort("Error", "Parse error when reading number for field %s.",
	     fieldName);

if ((*tok)->type == kxtString)
    {
    if (! isdigit((*tok)->string[0]))
	webAbort("Error", "Parse error when reading number for field %s.",
		 fieldName);
    dyStringAppend(q, (*tok)->string);
    *tok = (*tok)->next;
    }
else
    {
    webAbort("Error", "Parse error when reading number for field %s: Incorrect token type %d for token \"%s\"",
	     fieldName, (*tok)->type, (*tok)->string);
    }
}

void constrainNumber(char *fieldName, char *op, char *pat, char *log,
		     struct dyString *clause)
{
struct kxTok *tokList, *tokPtr;
int i;
boolean legit;

if (fieldName == NULL || op == NULL || pat == NULL || log == NULL)
    webAbort("Error", "CGI var error: not all required vars were defined for field %s.", fieldName);

/* complain if op is not a legitimate value */
legit = FALSE;
for (i=0;  i < cmpOpMenuSize;  i++)
    {
    if (sameString(cmpOpMenu[i], op))
	{
	legit = TRUE;
	break;
	}
    }
if (! legit)
    webAbort("Error", "Illegal comparison operator \"%s\"", op);

/* tokenize (don't expect wildcards) */
tokPtr = tokList = kxTokenize(pat, FALSE);

if (clause->stringSize > 0)
    dyStringPrintf(clause, " %s ", log);
else
    dyStringAppend(clause, "(");
dyStringPrintf(clause, "(%s %s ", fieldName, op);
parseNum(fieldName, &tokPtr, clause);
dyStringAppend(clause, ")");

slFreeList(&tokList);
}

void constrainRange(char *fieldName, char *pat, char *log,
		    struct dyString *clause)
{
struct kxTok *tokList, *tokPtr;

if (fieldName == NULL || pat == NULL || log == NULL)
    webAbort("Error", "CGI var error: not all required vars were defined for field %s.", fieldName);

/* tokenize (don't expect wildcards) */
tokPtr = tokList = kxTokenize(pat, FALSE);

if (clause->stringSize > 0)
    dyStringPrintf(clause, " %s ", log);
else
    dyStringAppend(clause, "(");
dyStringPrintf(clause, "((%s >= ", fieldName);
parseNum(fieldName, &tokPtr, clause);
dyStringPrintf(clause, ") && (%s <= ", fieldName);
while (tokPtr != NULL && (tokPtr->type == kxtSub ||
			  tokPtr->type == kxtPunct))
    tokPtr = tokPtr->next;
parseNum(fieldName, &tokPtr, clause);
dyStringAppend(clause, "))");

slFreeList(&tokList);
}

void constrainPattern(char *fieldName, char *dd, char *pat, char *log,
		      struct dyString *clause)
{
struct kxTok *tokList, *tokPtr;
boolean needOr = FALSE;
char *cmp, *or, *ptr;
int i;
boolean legit;

if (fieldName == NULL || dd == NULL || pat == NULL || log == NULL)
    webAbort("Error", "CGI var error: not all required vars were defined for field %s.", fieldName);

/* complain if dd is not a legitimate value */
legit = FALSE;
for (i=0;  i < ddOpMenuSize;  i++)
    {
    if (sameString(ddOpMenu[i], dd))
	{
	legit = TRUE;
	break;
	}
    }
if (! legit)
    webAbort("Error", "Illegal does/doesn't value \"%s\"", dd);

/* tokenize (do allow wildcards) */
tokList = kxTokenize(pat, TRUE);

/* The subterms are joined by OR if dd="does", AND if dd="doesn't" */
or  = sameString(dd, "does") ? " OR " : " AND ";
cmp = sameString(dd, "does") ? "LIKE"   : "NOT LIKE";

if (clause->stringSize > 0)
    dyStringPrintf(clause, " %s ", log);
else
    dyStringAppend(clause, "(");
dyStringAppend(clause, "(");
for (tokPtr = tokList;  tokPtr != NULL;  tokPtr = tokPtr->next)
    {
    if (tokPtr->type == kxtWildString || tokPtr->type == kxtString ||
	/* Allow these types for strand matches too: */
	tokPtr->type == kxtSub || tokPtr->type == kxtAdd)
	{
	if (needOr)
	    dyStringAppend(clause, or);
	/* Replace normal wildcard characters with SQL: */
	while ((ptr = strchr(tokPtr->string, '?')) != NULL)
	    *ptr = '_';
	while ((ptr = strchr(tokPtr->string, '*')) != NULL)
	    *ptr = '%';
	dyStringPrintf(clause, "(%s %s \"%s\")",
		       fieldName, cmp, tokPtr->string);
	needOr = TRUE;
	}
    else if (tokPtr->type == kxtEnd)
	break;
    else
	{
	webAbort("Error", "Match pattern parse error for field %s: bad token type (%d) for this word: \"%s\".",
		 fieldName, tokPtr->type, tokPtr->string);
	}
    }
dyStringAppend(clause, ")");

slFreeList(&tokList);
}

void constrainFreeForm(char *rawQuery, struct dyString *clause)
/* Let the user type in an expression that may contain
 * - field names
 * - parentheses
 * - comparison/arithmetic/logical operators
 * - numbers
 * - patterns with wildcards
 * Make sure they don't use any SQL reserved words, ;'s, etc.
 * Let SQL handle the actual parsing of nested expressions etc. - 
 * this is just a token cop. */
{
struct kxTok *tokList, *tokPtr;
struct slName *fnPtr;
char *ptr;
int numLeftParen, numRightParen;

if ((rawQuery == NULL) || (rawQuery[0] == 0))
    return;

/* tokenize (do allow wildcards, and include quotes.) */
kxTokIncludeQuotes(TRUE);
tokList = kxTokenize(rawQuery, TRUE);

/* to be extra conservative, wrap the whole expression in parens. */
dyStringAppend(clause, "(");
numLeftParen = numRightParen = 0;
for (tokPtr = tokList;  tokPtr != NULL;  tokPtr = tokPtr->next)
    {
    if ((tokPtr->type == kxtEquals) ||
	(tokPtr->type == kxtGT) ||
	(tokPtr->type == kxtGE) ||
	(tokPtr->type == kxtLT) ||
	(tokPtr->type == kxtLE) ||
	(tokPtr->type == kxtAnd) ||
	(tokPtr->type == kxtOr) ||
	(tokPtr->type == kxtNot) ||
	(tokPtr->type == kxtAdd) ||
	(tokPtr->type == kxtSub) ||
	(tokPtr->type == kxtDiv))
	{
	dyStringAppend(clause, tokPtr->string);
	}
    else if (tokPtr->type == kxtOpenParen)
	{
	dyStringAppend(clause, tokPtr->string);
	numLeftParen++;
	}
    else if (tokPtr->type == kxtCloseParen)
	{
	dyStringAppend(clause, tokPtr->string);
	numRightParen++;
	}
    else if ((tokPtr->type == kxtWildString) ||
	     (tokPtr->type == kxtString))
	{
	char *word = cloneString(tokPtr->string);
	toUpperN(word, strlen(word));
	if (startsWith("SQL_", word) || 
	    startsWith("MYSQL_", word) || 
	    sameString("ALTER", word) || 
	    sameString("BENCHMARK", word) || 
	    sameString("CHANGE", word) || 
	    sameString("CREATE", word) || 
	    sameString("DELAY", word) || 
	    sameString("DELETE", word) || 
	    sameString("DROP", word) || 
	    sameString("FLUSH", word) || 
	    sameString("GET_LOCK", word) || 
	    sameString("GRANT", word) || 
	    sameString("INSERT", word) || 
	    sameString("KILL", word) || 
	    sameString("LOAD", word) || 
	    sameString("LOAD_FILE", word) || 
	    sameString("LOCK", word) || 
	    sameString("MODIFY", word) || 
	    sameString("PROCESS", word) || 
	    sameString("QUIT", word) || 
	    sameString("RELEASE_LOCK", word) || 
	    sameString("RELOAD", word) || 
	    sameString("REPLACE", word) || 
	    sameString("REVOKE", word) || 
	    sameString("SELECT", word) || 
	    sameString("SESSION_USER", word) || 
	    sameString("SHOW", word) || 
	    sameString("SYSTEM_USER", word) || 
	    sameString("UNLOCK", word) || 
	    sameString("UPDATE", word) || 
	    sameString("USE", word) || 
	    sameString("USER", word) || 
	    sameString("VERSION", word))
	    {
	    webAbort("Error", "Illegal SQL word \"%s\" in free-form query string",
		     tokPtr->string);
	    }
	else if (sameString("*", tokPtr->string))
	    {
	    // special case for multiplication in a wildcard world
	    dyStringPrintf(clause, " %s ", tokPtr->string);
	    }
	else
	    {
	    /* Replace normal wildcard characters with SQL: */
	    while ((ptr = strchr(tokPtr->string, '?')) != NULL)
		*ptr = '_';
	    while ((ptr = strchr(tokPtr->string, '*')) != NULL)
	    *ptr = '%';
	    dyStringPrintf(clause, " %s ", tokPtr->string);
	    }
	}
    else if (tokPtr->type == kxtEnd)
	{
	break;
	}
    else
	{
	webAbort("Error", "Unrecognized token \"%s\" in free-form query string",
		 tokPtr->string);
	}
    }
dyStringAppend(clause, ")");

if (numLeftParen != numRightParen)
    webAbort("Error", "Unequal number of left parentheses (%d) and right parentheses (%d) in free-form query expression",
	     numLeftParen, numRightParen);

slFreeList(&tokList);
}

char *constrainFields(char *tableId)
/* If the user specified constraints, append SQL conditions (suitable
 * for a WHERE clause) to q. */
{
struct cgiVar *current;
struct dyString *freeClause = newDyString(512);
struct dyString *andClause = newDyString(512);
struct dyString *clause;
char *fieldName;
char *rawQuery;
char *rQLogOp;
char *dd, *cmp, *pat;
char varName[128];
char *ret;

if (tableId == NULL)
    tableId = "";

dyStringClear(andClause);
for (current = cgiVarList();  current != NULL;  current = current->next)
    {
    /* Look for pattern variable associated with each field. */
    snprintf(varName, sizeof(varName), "pat%s_", tableId);
    if (startsWith(varName, current->name))
	{	
	fieldName = current->name + strlen(varName);
	/* make sure that the field name doesn't have anything "weird" in it */
	checkIsAlpha("field name", fieldName);
	pat = current->val;
	snprintf(varName, sizeof(varName), "dd%s_%s", tableId, fieldName);
	dd = cgiOptionalString(varName);
	snprintf(varName, sizeof(varName), "cmp%s_%s", tableId, fieldName);
	cmp = cgiOptionalString(varName);
	/* If it's a null constraint, skip it. */
	if ( (dd != NULL &&
	      (pat == NULL || pat[0] == 0 ||
	       sameString(trimSpaces(pat), "*"))) ||
	     (cmp != NULL && sameString(cmp, "ignored")) )
	    continue;
	/* Otherwise, expect it to be a well-formed constraint and tack 
	 * it on to the clause. */
	clause = andClause;
	if (cmp != NULL && sameString(cmp, "in range"))
	    constrainRange(fieldName, pat, "AND", clause);
	else if (cmp != NULL)
	    constrainNumber(fieldName, cmp, pat, "AND", clause);
	else
	    constrainPattern(fieldName, dd, pat, "AND", clause);
	}
    }
if (andClause->stringSize > 0)
    dyStringAppend(andClause, ")");

dyStringClear(freeClause);
snprintf(varName, sizeof(varName), "rawQuery%s", tableId);
rawQuery = cgiOptionalString(varName);
snprintf(varName, sizeof(varName), "log_rawQuery%s", tableId);
rQLogOp  = cgiOptionalString(varName);
constrainFreeForm(rawQuery, freeClause);
// force rQLogOp to a legit value:
if ((rQLogOp != NULL) && (! sameString("AND", rQLogOp)))
    rQLogOp = "OR";

if (freeClause->stringSize > 0 && andClause->stringSize > 0)
    dyStringPrintf(freeClause, " %s ", rQLogOp);

if (freeClause->stringSize > 0)
    {
    dyStringAppend(freeClause, andClause->string);
    }
else
    {
    dyStringAppend(freeClause, andClause->string);
    }
ret = cloneString(freeClause->string);
freeDyString(&freeClause);
freeDyString(&andClause);
return ret;
}


void preserveConstraints(char *fullTblName, char *db, char *tableId)
/* Add CGI variables for filtering constraints, so they will be passed to 
 * the next page.  Also parse the constraints and do a null query with them 
 * in order to catch any syntax errors sooner rather than later. */
{
struct sqlConnection *conn;
struct sqlResult *sr;
struct cgiVar *current;
struct dyString *query = newDyString(512);
char *constraints = constrainFields(tableId);
char varName[128];

if ((constraints != NULL) && (constraints[0] != 0))
    {
    // Null query will cause errAbort if there's a syntax error, no-op if OK.
    dyStringPrintf(query, "SELECT 1 FROM %s WHERE 0 AND %s",
		   fullTblName, constraints);
    if (sameString(database, db))
	conn = hAllocConn();
    else
	conn = hAllocConn2();
    sr = sqlGetResult(conn, query->string);
    dyStringFree(&query);
    sqlFreeResult(&sr);
    if (sameString(database, db))
	hFreeConn(&conn);
    else
	hFreeConn2(&conn);
    }

if (tableId == NULL)
    tableId = "";
for (current = cgiVarList();  current != NULL;  current = current->next)
    {
    /* Look for pattern variable associated with each field. */
    snprintf(varName, sizeof(varName), "pat%s_", tableId);
    if (startsWith(varName, current->name))
	cgiMakeHiddenVar(current->name, current->val);
    snprintf(varName, sizeof(varName), "dd%s_", tableId);
    if (startsWith(varName, current->name))
	cgiMakeHiddenVar(current->name, current->val);
    snprintf(varName, sizeof(varName), "cmp%s_", tableId);
    if (startsWith(varName, current->name))
	cgiMakeHiddenVar(current->name, current->val);
    snprintf(varName, sizeof(varName), "log_rawQuery%s", tableId);
    if (sameString(varName, current->name))
	cgiMakeHiddenVar(current->name, current->val);
    snprintf(varName, sizeof(varName), "rawQuery%s", tableId);
    if (sameString(varName, current->name))
	cgiMakeHiddenVar(current->name, current->val);
    }
}


void preserveTable2()
{
char *table2 = getTable2Name();
char *op = cgiOptionalString("hgt.intersectOp");
if ((table2 != NULL) && (table2[0] != 0) && (op != NULL))
    {
    char *db2 = getTable2Db();
    char fullTableName2[256];
    cgiContinueHiddenVar("table2");
    cgiContinueHiddenVar("hgt.intersectOp");
    cgiMakeHiddenVar("hgt.moreThresh", cgiUsualString("hgt.moreThresh", "0"));
    cgiMakeHiddenVar("hgt.lessThresh", cgiUsualString("hgt.lessThresh", "100"));
    cgiContinueHiddenVar("hgt.invertTable");
    cgiContinueHiddenVar("hgt.invertTable2");
    getFullTableName(fullTableName2, chrom, table2);
    preserveConstraints(fullTableName2, db2, "2");
    }
}

struct hTableInfo *getHti()
/* Return primary table info. */
{
char *db = getTableDb();
char *track = getTrackName();
struct hTableInfo *hti = hFindTableInfoDb(db, chrom, track);

if (hti == NULL)
    webAbort("Error", "Could not find table info for table %s",
	     track);
return(hti);
}


struct hTableInfo *getOutputHti()
/* Return effective table info for the output.  If we're doing a 
 * base-pair-wise intersection/union of 2 tables, this will be a 
 * bed4 that isn't a track (unless user makes it a custom track).  
 * Otherwise this will just be the primary table info. */
{
struct hTableInfo *hti = getHti();
char *table = getTableName();
char *table2 = getTable2Name();
char *op = cgiOptionalString("hgt.intersectOp");

if ((table2 != NULL) && (table2[0] != 0) && (op != NULL))
    {
    if (sameString("and", op) || sameString("or", op))
	{
	struct hTableInfo *effHti;
	char buf[128];
	AllocVar(effHti);
	snprintf(buf, sizeof(buf), "%s_%s_%s", table, op, table2);
	effHti->rootName = cloneString(buf);
	effHti->isPos = TRUE;
	effHti->isSplit = FALSE;
	effHti->hasBin = FALSE;
	snprintf(effHti->chromField, 32, "N/A");
	snprintf(effHti->startField, 32, "N/A");
	snprintf(effHti->endField, 32, "N/A");
	snprintf(effHti->nameField, 32, "N/A");
	effHti->scoreField[0] = 0;
	effHti->strandField[0] = 0;
	effHti->cdsStartField[0] = 0;
	effHti->cdsEndField[0] = 0;
	effHti->countField[0] = 0;
	effHti->startsField[0] = 0;
	effHti->endsSizesField[0] = 0;
	effHti->hasCDS = FALSE;
	effHti->hasBlocks = FALSE;
	strcpy(buf, "bed 4");
	effHti->type = cloneString(buf);
	return(effHti);
	}
    }
return(hti);
}

struct bed *bitsToBed4List(Bits *bits, int bitSize, char *chrom, int minSize,
			   int rangeStart, int rangeEnd)
{
struct bed *bedList = NULL, *bed;
int i;
boolean thisBit, lastBit;
int start = 0;
int end;
int id = 0;
char name[128];

if (rangeStart < 0)
    rangeStart = 0;
if (rangeEnd > bitSize)
    rangeEnd = bitSize;

/* We depend on extra zero BYTE at end in case bitNot was used on bits. */
thisBit = FALSE;
for (i=0;  i < bitSize+8;  ++i)
    {
    lastBit = thisBit;
    thisBit = bitReadOne(bits, i);
    if (thisBit)
	{
	if (!lastBit)
	    start = i;
	}
    else
        {
	end = i;
	if (end >= bitSize)
	    end = bitSize - 1;
	// Lop off elements that go all the way to the beginning/end of the 
	// chrom... unless our range actually includes the beginning/end.
	// (That can happen with the AND/OR of two NOT's...)
	if (lastBit &&
	    ((end - start) >= minSize) &&
	    ((rangeStart == 0) || (start > 0)) &&
	    ((rangeEnd == bitSize) || (end < bitSize)))
	    {
	    AllocVar(bed);
	    bed->chrom = cloneString(chrom);
	    bed->chromStart = start;
	    bed->chromEnd = end;
	    snprintf(name, sizeof(name), "%s.%d", chrom, ++id);
	    bed->name = cloneString(name);
	    slAddHead(&bedList, bed);
	    }
	}
    }

slReverse(&bedList);
return(bedList);
}


int countBasesOverlap(struct bed *bedItem, Bits *bits, boolean hasBlocks)
/* Return the number of bases belonging to bedItem covered by bits. */
{
int count = 0;
int i, j;
if (hasBlocks)
    {
    for (i=0;  i < bedItem->blockCount;  i++)
	{
	int start = bedItem->chromStart + bedItem->chromStarts[i];
	int end   = start + bedItem->blockSizes[i];
	for (j=start;  j < end;  j++)
	    if (bitReadOne(bits, j))
		count++;
	}
    }
else
    {
    for (i=bedItem->chromStart;  i < bedItem->chromEnd;  i++)
	if (bitReadOne(bits, i))
	    count++;
    }
    return(count);
}


static struct bed *cloneBed(struct bed *bedItem)
/* Make an all-newly-allocated copy of a single bed record (<= 12 fields). */
{
struct bed *newBed;
if (bedItem == NULL)
    return NULL;
AllocVar(newBed);
newBed->chrom = cloneString(bedItem->chrom);
newBed->chromStart = bedItem->chromStart;
newBed->chromEnd = bedItem->chromEnd;
newBed->name = cloneString(bedItem->name);
newBed->score = bedItem->score;
strcpy(newBed->strand, bedItem->strand);
newBed->thickStart = bedItem->thickStart;
newBed->thickEnd = bedItem->thickEnd;
newBed->reserved = bedItem->reserved;
newBed->blockCount = bedItem->blockCount;
if (bedItem->blockCount > 0)
    {
    newBed->blockSizes = needMem(sizeof(int) * bedItem->blockCount);
    memcpy(newBed->blockSizes, bedItem->blockSizes,
	   sizeof(int) * bedItem->blockCount);
    newBed->chromStarts = needMem(sizeof(int) * bedItem->blockCount);
    memcpy(newBed->chromStarts, bedItem->chromStarts,
	   sizeof(int) * bedItem->blockCount);
    }
return(newBed);
}


struct bed *getBedList()
/* For any positional table output: get the features selected by the user 
 * and return them as a bed list.  This is where table intersection happens. */
{
struct slName *chromList, *chromPtr;
struct bed *bedList = NULL, *bedListT1 = NULL, *bedListChrom = NULL;
struct hTableInfo *hti = getHti();
char *constraints;
char *table = getTableName();
char *table2 = getTable2Name();
char *op = cgiOptionalString("hgt.intersectOp");
char *db = getTableDb();
char *track = getTrackName();
char *outputType = cgiString("outputType");
int fields;
int i, totalCount;

checkTableExists(fullTableName);
constraints = constrainFields(NULL);

if (allGenome)
    chromList = hAllChromNames();
else
    chromList = newSlName(chrom);
for (chromPtr=chromList;  chromPtr != NULL;  chromPtr = chromPtr->next)
    {
    bedListChrom = NULL;
    getFullTableName(fullTableName, chromPtr->name, table);
    bedListT1 =  hGetBedRangeDb(db, fullTableName, chrom, winStart,
				winEnd, constraints);
    /* If 2 tables are named, get their intersection. */
    if ((table2 != NULL) && (table2[0] != 0) && (op != NULL))
	{
	struct featureBits *fbListT2 = NULL;
	struct bed *bed;
	Bits *bitsT2;
	int moreThresh = cgiOptionalInt("hgt.moreThresh", 0);
	int lessThresh = cgiOptionalInt("hgt.lessThresh", 100);
	boolean invTable = cgiBoolean("hgt.invertTable");
	boolean invTable2 = cgiBoolean("hgt.invertTable2");
	char *track2 = getTrack2Name();
	char *db2 = getTable2Db();
	char *constraints2 = constrainFields("2");
	char fullTableName2[256];
	int chromSize = hChromSize(chromPtr->name);
	if ((!sameString("any", op)) &&
	    (!sameString("none", op)) &&
	    (!sameString("more", op)) &&
	    (!sameString("less", op)) &&
	    (!sameString("and", op)) &&
	    (!sameString("or", op)))
	    {
	    webAbort("Error", "Invalid value \"%s\" of CGI variable hgt.intersectOp", op);
	    }
	getFullTableName(fullTableName2, chromPtr->name, table2);
	checkTable2Exists(fullTableName2);
	fbListT2 = fbGetRangeQueryDb(db2, track2, chrom, winStart, winEnd,
				     constraints2, FALSE, FALSE);
	bitsT2 = bitAlloc(chromSize+8);
	fbOrBits(bitsT2, chromSize, fbListT2, 0);
	if (sameString("and", op) || sameString("or", op))
	    {
	    // Base-pair-wise operation: get featureBits for primary table too
	    struct featureBits *fbListT1;
	    Bits *bitsT1;
	    fbListT1 = fbGetRangeQueryDb(db, track, chrom, winStart, winEnd,
					 constraints, FALSE, FALSE);
	    bitsT1 = bitAlloc(chromSize+8);
	    fbOrBits(bitsT1, chromSize, fbListT1, 0);
	    // invert inputs if necessary
	    if (invTable)
		bitNot(bitsT1, chromSize);
	    if (invTable2)
		bitNot(bitsT2, chromSize);
	    // do the intersection/union
	    if (sameString("and", op))
		bitAnd(bitsT1, bitsT2, chromSize);
	    else
		bitOr(bitsT1, bitsT2, chromSize);
	    // translate back to bed
	    bedListChrom = bitsToBed4List(bitsT1, chromSize, chrom, 1,
					  winStart, winEnd);
	    bitFree(&bitsT1);
	    featureBitsFreeList(&fbListT1);
	    }
	else
	    {
	    for (bed = bedListT1;  bed != NULL;  bed = bed->next)
		{
		struct bed *newBed;
		int numBasesOverlap = countBasesOverlap(bed, bitsT2,
							hti->hasBlocks);
		int length = 0;
		double pctBasesOverlap;
		if (hti->hasBlocks)
		    for (i=0;  i < bed->blockCount;  i++)
			length += bed->blockSizes[i];
		else
		    length = (bed->chromEnd - bed->chromStart);
		if (length == 0)
		    length = 1;
		pctBasesOverlap = ((numBasesOverlap * 100.0) / length);
		if ((sameString("any", op) && (numBasesOverlap > 0)) ||
		    (sameString("none", op) && (numBasesOverlap == 0)) ||
		    (sameString("more", op) &&
		     (pctBasesOverlap >= moreThresh)) ||
		    (sameString("less", op) &&
		     (pctBasesOverlap <= lessThresh)))
		    {
		    newBed = cloneBed(bed);		    
		    slAddHead(&bedListChrom, newBed);
		    }
		}
	    slReverse(&bedListChrom);
	    } // end foreach primary table bed item
	bedFreeList(&bedListT1);
	bitFree(&bitsT2);
	featureBitsFreeList(&fbListT2);
	}
    else
	{
	/* Only one table was given, no intersection necessary. */
	bedListChrom = bedListT1;
	}
    bedList = slCat(bedList, bedListChrom);
    } // end foreach chromosome in position range
return(bedList);
}


boolean printTabbedResults(struct sqlResult *sr, boolean initialized)
{
char **row;
char *field;
int i;
int numberColumns = sqlCountColumns(sr);

row = sqlNextRow(sr);
if (row == NULL)
    return(initialized);

if (! initialized)
    {
    initialized = TRUE;
    /* print the columns names */
    printf("#");
    while((field = sqlFieldName(sr)) != NULL)
	printf("%s\t", field);
    printf("\n");
    }

/* print the data */
do
    {
    for (i = 0; i < numberColumns; i++)
	printf("%s\t", row[i]);
    printf("\n");
    }
while((row = sqlNextRow(sr)) != NULL);
return(initialized);
}


void doTabSeparated(boolean allFields)
{
struct slName *chromList, *chromPtr;
struct sqlConnection *conn;
struct sqlResult *sr;
struct dyString *query = newDyString(512);
struct dyString *fieldSpec = newDyString(256);
struct cgiVar* varPtr;
char *table = getTableName();
char chromField[32];
char startField[32];
char endField[32];
char *constraints;
boolean gotResults;
boolean gotFirst = FALSE;

if (allGenome)
    chromList = hAllChromNames();
else
    chromList = newSlName(chrom);

if (sameString(database, getTableDb()))
    conn = hAllocConn();
else
    conn = hAllocConn2();

printf("Content-Type: text/plain\n\n");
webStartText();
checkTableExists(fullTableName);
constraints = constrainFields(NULL);

dyStringClear(fieldSpec);
if (allFields)
    dyStringAppend(fieldSpec, "*");
else
    for (varPtr = cgiVarList();  varPtr != NULL;  varPtr = varPtr->next)
	{
	if ((strstr(varPtr->name, "field_") == varPtr->name) &&
	    sameString("on", varPtr->val))
	    {	
	    checkIsAlpha("field name", varPtr->name + strlen("field_"));
	    if (! gotFirst)
		gotFirst = TRUE;
	    else
		dyStringAppend(fieldSpec, ",");
	    dyStringAppend(fieldSpec, varPtr->name + strlen("field_"));
	    }
	}

if (strlen(fieldSpec->string) < 1)
    {
    printf("\n# Error: at least one field must be selected.\n\n");
    return;
    }

hFindChromStartEndFieldsDb(getTableDb(), fullTableName, chromField,
			   startField, endField);
gotResults = FALSE;
if (tableIsSplit)
    {
    for (chromPtr=chromList;  chromPtr != NULL;  chromPtr = chromPtr->next)
	{
	getFullTableName(fullTableName, chromPtr->name, table);
	dyStringClear(query);
	dyStringPrintf(query, "SELECT %s FROM %s",
		       fieldSpec->string, fullTableName);
	if ((! allGenome) && tableIsPositional)
	    {
	    dyStringPrintf(query, " WHERE %s < %d AND %s > %d",
			   startField, winEnd, endField, winStart);
	    if ((constraints != NULL) && (constraints[0] != 0))
		dyStringPrintf(query, " AND %s", constraints);
	    }
	else if ((constraints != NULL) && (constraints[0] != 0))
	    dyStringPrintf(query, " WHERE %s", constraints);
	sr = sqlGetResult(conn, query->string);
	gotResults = printTabbedResults(sr, gotResults);
	sqlFreeResult(&sr);
	}
    }
else
    {
    dyStringClear(query);
    dyStringPrintf(query, "SELECT %s FROM %s",
		   fieldSpec->string, fullTableName);
    if ((! allGenome) && tableIsPositional)
	{
	dyStringPrintf(query, " WHERE %s < %d AND %s > %d",
		       startField, winEnd, endField, winStart);
	if (! sameString("", chromField))
	    dyStringPrintf(query, " AND %s = \'%s\'",
			   chromField, chrom);
	if ((constraints != NULL) && (constraints[0] != 0))
	    dyStringPrintf(query, " AND %s", constraints);
	}
    else if ((constraints != NULL) && (constraints[0] != 0))
	dyStringPrintf(query, " WHERE %s", constraints);
    sr = sqlGetResult(conn, query->string);
    gotResults = printTabbedResults(sr, gotResults);
    sqlFreeResult(&sr);
    }
if (! gotResults)
    printf("\n# No results returned from query.\n\n");

dyStringFree(&query);
if (sameString(database, getTableDb()))
    hFreeConn(&conn);
else
    hFreeConn2(&conn);
}


void doChooseFields()
{
struct sqlConnection *conn;
struct sqlResult *sr;
char **row;
char *db = getTableDb();
char *outputType = cgiUsualString("outputType", cgiString("phase"));
char query[256];
char name[128];
char *newVal;
boolean checkAll;
boolean clearAll;

if (sameString(database, db))
    conn = hAllocConn();
else
    conn = hAllocConn2();

webStart(cart, "Table Browser: %s: Choose Fields", freezeName);
checkTableExists(fullTableName);
printf("<FORM ACTION=\"%s\" NAME=\"mainForm\">\n\n", hgTextName());
cartSaveSession(cart);
cgiMakeHiddenVar("db", database);
cgiMakeHiddenVar("table", getTableVar());
cgiMakeHiddenVar("outputType", outputType);
positionLookupSamePhase();
preserveConstraints(fullTableName, db, NULL);
preserveTable2();

snprintf(query, sizeof(query), "DESCRIBE %s", fullTableName);
sr = sqlGetResult(conn, query);

printf("<H3> Select Fields of Table %s: </H3>\n", getTableName());
cgiMakeHiddenVar("origPhase", cgiString("phase"));
cgiMakeButton("submit", "Check All");
cgiMakeButton("submit", "Clear All");

puts("<TABLE><TR><TD>");
checkAll = existsAndEqual("submit", "Check All") ? TRUE : FALSE;
clearAll = existsAndEqual("submit", "Clear All") ? TRUE : FALSE;
while((row = sqlNextRow(sr)) != NULL)
    {
    snprintf(name, sizeof(name), "field_%s", row[0]);
    if (checkAll || (existsAndEqual(name, "on") && !clearAll))
	printf("<INPUT TYPE=\"checkbox\" NAME=\"%s\" CHECKED> %s\n",
	       name, row[0]);
    else
	printf("<INPUT TYPE=\"checkbox\" NAME=\"%s\"> %s\n", name, row[0]);
    puts("</TD></TR><TR><TD>");
    }
puts("</TABLE>\n");
sqlFreeResult(&sr);
if (sameString(database, getTableDb()))
    hFreeConn(&conn);
else
    hFreeConn2(&conn);
	
cgiMakeButton("phase", getSomeFieldsPhase);
puts("</FORM>");

webEnd();
}


void doSequenceOptions()
{
struct hTableInfo *hti = getOutputHti();
char *outputType = cgiUsualString("outputType", cgiString("phase"));

webStart(cart, "Table Browser: %s: %s", freezeName, seqOptionsPhase);
checkTableExists(fullTableName);
printf("<FORM ACTION=\"%s\" NAME=\"mainForm\">\n\n", hgTextName());
cartSaveSession(cart);
cgiMakeHiddenVar("db", database);
cgiMakeHiddenVar("table", getTableVar());
cgiMakeHiddenVar("outputType", outputType);
preserveConstraints(fullTableName, getTableDb(), NULL);
preserveTable2();
positionLookupSamePhase();
printf("<H3>Table: %s</H3>\n", hti->rootName);
hgSeqOptionsHti(hti);
cgiMakeButton("phase", getSequencePhase);
puts("</FORM>");
webEnd();
}


void doGetSequence()
{
struct hTableInfo *hti = getOutputHti();
struct bed *bedList = getBedList();
char *db = getTableDb();
int itemCount;

printf("Content-Type: text/plain\n\n");
webStartText();

itemCount = hgSeqBedDb(db, hti, bedList);
bedFreeList(&bedList);
if (itemCount == 0)
    printf("\n# No results returned from query.\n\n");
}

static void addGffLineFromBed(struct gffLine **pGffList, struct bed *bed,
			      char *source, char *feature,
			      int start, int end, char frame)
/* Create a gffLine from a bed and line-specific parameters, add to list. */
{
struct gffLine *gff;
AllocVar(gff);
gff->seq = cloneString(bed->chrom);
gff->source = cloneString(source);
gff->feature = cloneString(feature);
gff->start = start;
gff->end = end;
gff->score = bed->score;
gff->strand = bed->strand[0];
gff->frame = frame;
gff->group = gff->geneId = cloneString(bed->name);
slAddHead(pGffList, gff);
}


static void addCdsStartStop(struct gffLine **pGffList, struct bed *bed,
			    char *source, int s, int e, char *frames,
			    int i, int startIndx, int stopIndx,
			    boolean gtf2StopCodons)
{
// start_codon (goes first for + strand) overlaps with CDS
if ((i == startIndx) && (bed->strand[0] != '-'))
    {
    addGffLineFromBed(pGffList, bed, source, "start_codon",
		      s, s+3, '.');
    }
// stop codon does not overlap with CDS as of GTF2
if ((i == stopIndx) && gtf2StopCodons)
    {
    if (bed->strand[0] == '-')
	{
	addGffLineFromBed(pGffList, bed, source, "stop_codon",
			  s, s+3, '.');
	addGffLineFromBed(pGffList, bed, source, "CDS", s+3, e,
			  frames[i]);
	}
    else
	{
	addGffLineFromBed(pGffList, bed, source, "CDS", s, e-3,
			  frames[i]);
	addGffLineFromBed(pGffList, bed, source, "stop_codon",
			  e-3, e, '.');
	}
    }
else
    {
    addGffLineFromBed(pGffList, bed, source, "CDS", s, e,
		      frames[i]);
    }
// start_codon (goes last for - strand) overlaps with CDS
if ((i == startIndx) && (bed->strand[0] == '-'))
    {
    addGffLineFromBed(pGffList, bed, source, "start_codon",
		      e-3, e, '.');
    }
}


struct gffLine *bedToGffLines(struct bed *bedList, struct hTableInfo *hti,
			      char *source)
/* Translate a (list of) bed into list of gffLine elements. */
{
struct gffLine *gffList = NULL, *gff;
struct bed *bed;
int i, j, s, e;

for (bed = bedList;  bed != NULL;  bed = bed->next)
    {
    if (hti->hasBlocks && hti->hasCDS)
	{
	char *frames = needMem(bed->blockCount);
	boolean gotFirstCds = FALSE;
	int nextPhase = 0;
	int startIndx = 0;
	int stopIndx = 0;
	// need a separate pass to compute frames, in case strand is '-'
	for (i=0;  i < bed->blockCount;  i++)
	    {
	    if (bed->strand[0] == '-')
		j = bed->blockCount-i-1;
	    else
		j = i;
	    s = bed->chromStart + bed->chromStarts[j];
	    e = s + bed->blockSizes[j];
	    if ((s < bed->thickEnd) && (e > bed->thickStart))
		{
		int cdsSize = e - s;
		if (e > bed->thickEnd)
		    cdsSize = bed->thickEnd - s;
		else if (s < bed->thickStart)
		    cdsSize = e - bed->thickStart;
		if (! gotFirstCds)
		    {
		    gotFirstCds = TRUE;
		    startIndx = j;
		    }
		frames[j] = '0' + nextPhase;
		nextPhase = (3 + ((nextPhase - cdsSize) % 3)) % 3;
		stopIndx = j;
		}
	    else
		{
		frames[j] = '.';
		}
	    }
	for (i=0;  i < bed->blockCount;  i++)
	    {
	    s = bed->chromStart + bed->chromStarts[i];
	    e = s + bed->blockSizes[i];
	    if ((s >= bed->thickStart) && (e <= bed->thickEnd))
		{
		addCdsStartStop(&gffList, bed, source, s, e, frames,
				i, startIndx, stopIndx, FALSE);
		}
	    else if ((s < bed->thickStart) && (e > bed->thickEnd))
		{
		addCdsStartStop(&gffList, bed, source,
				bed->thickStart, bed->thickEnd,
				frames, i, startIndx, stopIndx, FALSE);
		}
	    else if ((s < bed->thickStart) && (e > bed->thickStart))
		{
		addCdsStartStop(&gffList, bed, source, bed->thickStart, e,
				frames, i, startIndx, stopIndx, FALSE);
		}
	    else if ((s < bed->thickEnd) && (e > bed->thickEnd))
		{
		addCdsStartStop(&gffList, bed, source, s, bed->thickEnd,
				frames, i, startIndx, stopIndx, FALSE);
		}
	    addGffLineFromBed(&gffList, bed, source, "exon", s, e, '.');
	    }
	freeMem(frames);
	}
    else if (hti->hasBlocks)
	{
	for (i=0;  i < bed->blockCount;  i++)
	    {
	    s = bed->chromStart + bed->chromStarts[i];
	    e = s + bed->blockSizes[i];
	    addGffLineFromBed(&gffList, bed, source, "exon", s, e, '.');
	    }
	}
    else if (hti->hasCDS)
	{
	if (bed->thickStart > bed->chromStart)
	    {
	    addGffLineFromBed(&gffList, bed, source, "exon", bed->chromStart,
			      bed->thickStart, '.');
	    }
	addGffLineFromBed(&gffList, bed, source, "CDS", bed->thickStart,
			  bed->thickEnd, '0');
	if (bed->thickEnd < bed->chromEnd)
	    {
	    addGffLineFromBed(&gffList, bed, source, "exon", bed->thickEnd,
			      bed->chromEnd, '.');
	    }
	}
    else
	{
	addGffLineFromBed(&gffList, bed, source, "exon", bed->chromStart,
			  bed->chromEnd, '.');
	}
    }
slReverse(&gffList);
return(gffList);
}

void doGetGFF()
{
struct hTableInfo *hti = getOutputHti();
struct bed *bedList = getBedList();
struct gffLine *gffList, *gffPtr;
char source[64];
char *db = getTableDb();
char *track = getTrackName();
int itemCount;

printf("Content-Type: text/plain\n\n");
webStartText();

snprintf(source, sizeof(source), "%s_%s", db, track);
itemCount = 0;
gffList = bedToGffLines(bedList, hti, source);
bedFreeList(&bedList);
for (gffPtr = gffList;  gffPtr != NULL;  gffPtr = gffPtr->next)
    {
    gffTabOut(gffPtr, stdout);
    itemCount++;
    }
slFreeList(&gffList);
if (itemCount == 0)
    printf("\n# No results returned from query.\n\n");
}


void doBedOptions()
{
struct hTableInfo *hti = getOutputHti();
char *table = getTableName();
char *table2 = getTable2Name();
char *track = getTrackName();
char *db = getTableDb();
char *outputType = cgiUsualString("outputType", cgiString("phase"));
char *val;
char buf[256];

webStart(cart, "Table Browser: %s: %s", freezeName, bedOptionsPhase);
checkTableExists(fullTableName);
puts("<FORM ACTION=\"/cgi-bin/hgText\" NAME=\"mainForm\" METHOD=\"GET\">\n");
cartSaveSession(cart);
cgiMakeHiddenVar("db", database);
cgiMakeHiddenVar("table", getTableVar());
cgiMakeHiddenVar("outputType", outputType);
preserveConstraints(fullTableName, db, NULL);
preserveTable2();
positionLookupSamePhase();
printf("<H3> Select BED Options for Table %s: </H3>\n", hti->rootName);
puts("<TABLE><TR><TD>");
cgiMakeCheckBox("hgt.doCustomTrack", TRUE);
puts("</TD><TD> <B> Print "
     "<A HREF=\"/goldenPath/help/customTrack.html\" TARGET=_blank>"
     "custom track</A> header: </B>");
puts("</TD></TR><TR><TD></TD><TD>name=");
snprintf(buf, sizeof(buf), "tb_%s%s%s", hti->rootName,
	 (table2 ? "_" : ""),
	 (table2 ? table2 : ""));
val = cgiUsualString("hgt.ctName", buf);
cgiMakeTextVar("hgt.ctName", val, 16);
puts("</TD></TR><TR><TD></TD><TD>description=");
snprintf(buf, sizeof(buf), "table browser query on %s%s%s",
	 hti->rootName,
	 (table2 ? ", " : ""),
	 (table2 ? table2 : ""));
val = cgiUsualString("hgt.ctDesc", buf);
cgiMakeTextVar("hgt.ctDesc", val, 50);
puts("</TD></TR><TR><TD></TD><TD>visibility=");
val = cgiUsualString("hgt.ctVis", ctVisMenu[1]);
cgiMakeDropList("hgt.ctVis", ctVisMenu, ctVisMenuSize, val);
puts("</TD></TR><TR><TD></TD><TD>url=");
val = cgiUsualString("hgt.ctUrl", "");
cgiMakeTextVar("hgt.ctUrl", val, 50);
puts("</TD></TR><TR><TD></TD><TD>");
cgiMakeCheckBox("hgt.loadCustomTrack", FALSE);
puts("Load this custom track into my session");
puts("</TD></TR></TABLE>");
puts("<P> <B> Create one BED record per: </B>");
fbOptionsHti(hti);
cgiMakeButton("phase", getBedPhase);
puts("</FORM>");
webEnd();
}


struct customTrack *newCT(char *ctName, char *ctDesc, int visNum, char *ctUrl,
			  int fields)
/* Make a new custom track record for the query results. */
{
struct customTrack *ct;
struct trackDb *tdb;
char buf[256];
AllocVar(ct);
AllocVar(tdb);
snprintf(buf, sizeof(buf), "ct_%s", ctName);
tdb->tableName = cloneString(buf);
tdb->shortLabel = ctName;
tdb->longLabel = ctDesc;
snprintf(buf, sizeof(buf), "bed %d .", fields);
tdb->type = cloneString(buf);
tdb->visibility = visNum;
tdb->url = ctUrl;
ct->tdb = tdb;
ct->fieldCount = fields;
ct->needsLift = FALSE;
ct->fromPsl = FALSE;
return(ct);
}

void doGetBed()
{
struct hTableInfo *hti = getOutputHti();
struct bed *bedList = getBedList();
struct bed *bedPtr;
struct featureBits *fbList = NULL, *fbPtr;
struct customTrack *ctList = NULL, *ctNew = NULL;
struct tempName tn;
char *table = getTableName();
char *track = getTrackName();
char *db = getTableDb();
boolean doCT = cgiBoolean("hgt.doCustomTrack");
char *ctName = cgiUsualString("hgt.ctName", table);
char *ctDesc = cgiUsualString("hgt.ctDesc", table);
char *ctVis  = cgiUsualString("hgt.ctVis", "dense");
char *ctUrl  = cgiUsualString("hgt.ctUrl", "");
boolean loadCT = cgiBoolean("hgt.loadCustomTrack");
char *ctFileName = NULL;
char *fbQual = fbOptionsToQualifier();
char fbTQ[128];
int fields;
boolean gotResults = FALSE;

printf("Content-Type: text/plain\n\n");
webStartText();

if (hti->hasBlocks)
    fields = 12;
else if (hti->hasCDS)
    fields = 8;
else if (hti->strandField[0] != 0)
    fields = 6;
else if (hti->scoreField[0] != 0)
    fields = 5;
else
    fields = 4;

if (doCT && (bedList != NULL))
    {
    int visNum = (sameString("hide", ctVis) ? 0 :
		  sameString("full", ctVis) ? 2 : 1);
    if (loadCT)
	ctNew = newCT(ctName, ctDesc, visNum, ctUrl, fields);
    printf("track name=\"%s\" description=\"%s\" visibility=%d url=%s \n",
	   ctName, ctDesc, visNum, ctUrl);
    }

if ((fbQual == NULL) || (fbQual[0] == 0))
    {
    for (bedPtr = bedList;  bedPtr != NULL;  bedPtr = bedPtr->next)
	{
	char *ptr = strchr(bedPtr->name, ' ');
	if (ptr != NULL)
	    *ptr = 0;
	bedTabOutN(bedPtr, fields, stdout);
	gotResults = TRUE;
	}
    if (ctNew != NULL)
	ctNew->bedList = bedList;
    }
else
    {
    snprintf(fbTQ, sizeof(fbTQ), "%s:%s", hti->rootName, fbQual);
    fbList = fbFromBed(fbTQ, hti, bedList, winStart, winEnd, FALSE, FALSE);
    for (fbPtr=fbList;  fbPtr != NULL;  fbPtr=fbPtr->next)
	{
	char *ptr = strchr(fbPtr->name, ' ');
	if (ptr != NULL)
	    *ptr = 0;
	printf("%s\t%d\t%d\t%s\t%d\t%c\n",
	       fbPtr->chrom, fbPtr->start, fbPtr->end, fbPtr->name,
	       0, fbPtr->strand);
	gotResults = TRUE;
	}
    if (ctNew != NULL)
	ctNew->bedList = fbToBed(fbList);
    featureBitsFreeList(&fbList);
    }

if ((ctNew != NULL) && (ctNew->bedList != NULL))
    {
    /* Load existing custom tracks and add this new one: */
    ctList = customTracksParseCart(cart, NULL, NULL);
    slAddHead(&ctList, ctNew);
    /* Save the custom tracks out to file (overwrite the old file): */
    ctFileName = cartOptionalString(cart, "ct");
    if (ctFileName == NULL)
	{
	makeTempName(&tn, "hgtct", ".bed");
	ctFileName = cloneString(tn.forCgi);
	}
    customTrackSave(ctList, ctFileName);
    cartSetString(cart, "ct", ctFileName);
    }

if (! gotResults)
    printf("\n# No results returned from query.\n\n");
bedFreeList(&bedList);
}


void doGetBrowserLinks()
{
struct hTableInfo *hti = getOutputHti();
struct bed *bedList = getBedList();
struct bed *bedPtr;
char *table = getTableName();
char *track = getTrackName();
char *track2 = getTrack2Name();
char track2CGI[128];
int itemCount;

webStart(cart, "Table Browser: %s: %s", freezeName, linksPhase);
puts("<FORM ACTION=\"/cgi-bin/hgText\" NAME=\"mainForm\" METHOD=\"GET\">\n");
cartSaveSession(cart);
puts("</FORM>");
printf("<H3> Links to Genome Browser from Table %s </H3>\n", getTableName());

if ((track2 != NULL) && (track2[0] != 0))
    snprintf(track2CGI, sizeof(track2CGI), "&%s=full", track2);
else
    track2CGI[0] = 0;

itemCount = 0;
for (bedPtr = bedList;  bedPtr != NULL;  bedPtr = bedPtr->next)
    {
    printf("<A HREF=\"%s?db=%s&position=%s:%d-%d&%s=full%s&hgsid=%d\"",
	   hgTracksName(), hGetDb(), bedPtr->chrom, bedPtr->chromStart+1,
	   bedPtr->chromEnd, track, track2CGI, cartSessionId(cart));
    printf(" TARGET=_blank> %s %s:%d-%d <BR>\n", bedPtr->name, bedPtr->chrom,
	   bedPtr->chromStart+1, bedPtr->chromEnd);
    itemCount++;
    }
bedFreeList(&bedList);
if (itemCount == 0)
    puts("No results returned from query.");
webEnd();
}


void doIntersectOptions()
{
struct bed *bedList, *bedPtr;
struct sqlConnection *conn;
struct sqlResult *sr;
char **row;
char *outputType = cgiString("outputType");
char *table = getTableName();
char *table2 = getTable2Name();
char *track = getTrackName();
char *db = getTableDb();
char *db2 = getTable2Db();
char *newVal;
char fullTableName2[256];
char query[256];
char name[128];
boolean gotFirst;

webStart(cart, "Table Browser: %s: %s", freezeName, intersectOptionsPhase);
checkTableExists(fullTableName);
if (sameString("Choose table", table2))
    webAbort("Missing table selection",
	     "Please choose a table and try again.");
getFullTableName(fullTableName2, chrom, table2);
checkTable2Exists(fullTableName2);
if (! tableIsPositional)
    webAbort("Error", "Table %s is not a positional table.", table);
if (! sameString(outputType, seqOptionsPhase) &&
    ! sameString(outputType, bedOptionsPhase) &&
    ! sameString(outputType, linksPhase) &&
    ! sameString(outputType, gffPhase))
    webAbort("Error", "Output format \"%s\" is not supported for intersection of tables.", outputType);

puts("<FORM ACTION=\"/cgi-bin/hgText\" NAME=\"mainForm\" METHOD=\"GET\">\n");
cartSaveSession(cart);
cgiMakeHiddenVar("db", database);
cgiMakeHiddenVar("table", getTableVar());
cgiMakeHiddenVar("outputType", outputType);
positionLookupSamePhase();
cgiMakeHiddenVar("table2", getTable2Var());
preserveConstraints(fullTableName, db, NULL);
preserveConstraints(fullTableName2, db2, "2");
printf("<H3> Specify how to combine tables %s and %s: </H3>\n",
       table, table2);

printf("These combinations will maintain the gene/alignment structure (if any) of %s: <P>\n",
       table);
cgiMakeRadioButton("hgt.intersectOp", "any", TRUE);
printf("All %s records that have any overlap with %s <P>\n",
       table, table2);
cgiMakeRadioButton("hgt.intersectOp", "none", TRUE);
printf("All %s records that have no overlap with %s <P>\n",
       table, table2);
cgiMakeRadioButton("hgt.intersectOp", "more", TRUE);
printf("All %s records that have at least ",
       table);
cgiMakeIntVar("hgt.moreThresh", 80, 3);
printf(" %% overlap with %s <P>\n", table2);
cgiMakeRadioButton("hgt.intersectOp", "less", TRUE);
printf("All %s records that have at most ",
       table);
cgiMakeIntVar("hgt.lessThresh", 80, 3);
printf(" %% overlap with %s <P>\n", table2);

printf("These combinations will discard the gene/alignment structure (if any) of %s and produce a simple list of position ranges.\n",
       table);
puts("To complement a table means to consider a position included if it \n"
     "is <I>not</I> included in the table. <P>");
cgiMakeRadioButton("hgt.intersectOp", "and", TRUE);
printf("Base-pair-wise intersection (AND) of %s and %s <P>\n",
       table, table2);
cgiMakeRadioButton("hgt.intersectOp", "or", TRUE);
printf("Base-pair-wise union (OR) of %s and %s <P>\n",
       table, table2);
cgiMakeCheckBox("hgt.invertTable", FALSE);
printf("Complement %s before intersection/union <P>\n", table);
cgiMakeCheckBox("hgt.invertTable2", FALSE);
printf("Complement %s before intersection/union <P>\n", table2);

cgiMakeButton("phase", outputType);

printf("<P><HR><H3> (Optional) Filter Table %s Records by Field Values </H3>\n",
       table2);
if (sameString(database, getTable2Db()))
    conn = hAllocConn();
else
    conn = hAllocConn2();
snprintf(query, sizeof(query), "DESCRIBE %s", fullTableName2);
sr = sqlGetResult(conn, query);

puts("<TABLE><TR><TD>\n");
puts("<TABLE>\n");
gotFirst = FALSE;
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (! strstr(row[1], "blob"))
	{
	if (! gotFirst)
	    gotFirst = TRUE;
	else
	    puts(" AND </TD></TR>\n");
	printf("<TR VALIGN=BOTTOM><TD> %s </TD><TD>\n", row[0]);
	if (strstr(row[1], "char"))
	    {
	    snprintf(name, sizeof(name), "dd2_%s", row[0]);
	    cgiMakeDropList(name, ddOpMenu, ddOpMenuSize, ddOpMenu[0]);
	    puts(" match </TD><TD>\n");
	    newVal = "*";
	    }
	else
	    {
	    puts(" is \n");
	    snprintf(name, sizeof(name), "cmp2_%s", row[0]);
	    cgiMakeDropList(name, cmpOpMenu, cmpOpMenuSize, cmpOpMenu[0]);
	    puts("</TD><TD>\n");
	    newVal = "";
	    }
	snprintf(name, sizeof(name), "pat2_%s", row[0]);
	cgiMakeTextVar(name, newVal, 20);
	}
    }
sqlFreeResult(&sr);
if (sameString(database, getTable2Db()))
    hFreeConn(&conn);
else
    hFreeConn2(&conn);
puts("</TD></TR></TABLE>\n");
puts(" </TD></TR><TR VALIGN=BOTTOM><TD>");
strncpy(name, "log_rawQuery2", sizeof(name));
cgiMakeDropList(name, logOpMenu, logOpMenuSize, logOpMenu[0]);
puts("Free-form query: ");
cgiMakeTextVar("rawQuery2", "", 50);
puts("</TD></TR></TABLE>");
cgiMakeButton("phase", outputType);
puts("</FORM>");
webEnd();
}


void doMiddle(struct cart *theCart)
/* the main body of the program */
{
char *table = getTableName();
char trash[32];

cart = theCart;
database = cartUsualString(cart, "db", hGetDb());
hSetDb(database);
hDefaultConnect();

freezeName = hFreezeFromDb(database);
if (freezeName == NULL)
    freezeName = "Unknown";

tableIsPositional = FALSE;
tableIsSplit = FALSE;
fullTableName[0] = 0;
chrom = NULL;
winStart = winEnd = 0;
position = getPosition(&chrom, &winStart, &winEnd);
if (position == NULL)
    return;
allGenome = sameString("genome", position);

if (existsAndEqual("submit", "Look up") ||
    (cgiOptionalString("phase") == NULL))
    {
    // Stay in same phase if we're just looking up position.
    char *origPhase = cgiOptionalString("origPhase");
    if (origPhase != NULL)
	cgiVarSet("phase", origPhase);
    }

if (table == NULL || existsAndEqual("phase", chooseTablePhase))
    {
    if (existsAndEqual("table0", "Choose table") &&
	existsAndEqual("table1", "Choose table") &&
	!existsAndEqual("submit", "Look up"))
	webAbort("Missing table selection",
		 "Please choose a table and try again.");
    else
	{
	webStart(cart, "Table Browser: %s: Choose a table", freezeName);
	doChooseTable();
	webEnd();
	}
    }
else
    {
    if ((! sameString(database, getTableDb())) &&
	(! sameString(hGetDb2(), getTableDb())))
	hSetDb2(getTableDb());

    if (existsAndEqual("table0", "Choose table") &&
	existsAndEqual("table1", "Choose table"))
	webAbort("Missing table selection", "Please choose a table.");

    if (allGenome)
	getFullTableName(fullTableName, "chr1", table);
    else
	getFullTableName(fullTableName, chrom, table);

    tableIsPositional = hFindChromStartEndFieldsDb(getTableDb(),
						   fullTableName,
						   trash, trash, trash);
    if (strstr(table, "chrN_") == table)
	tableIsSplit = TRUE;

    if (existsAndEqual("phase", outputOptionsPhase))
	doOutputOptions();
    else if (existsAndEqual("phase", getOutputPhase))
	{
	if (existsAndEqual("outputType", allFieldsPhase) ||
	    existsAndEqual("outputType", oldAllFieldsPhase))
	    doTabSeparated(TRUE);
	else if (existsAndEqual("outputType", chooseFieldsPhase))
	    doChooseFields();
	else if (existsAndEqual("outputType", seqOptionsPhase))
	    doSequenceOptions();
	else if (existsAndEqual("outputType", gffPhase))
	    doGetGFF();
	else if (existsAndEqual("outputType", bedOptionsPhase))
	    doBedOptions();
	else if (existsAndEqual("outputType", linksPhase))
	    doGetBrowserLinks();
	else
	    webAbort("Table Browser: CGI option error",
		     "Error: unrecognized value of CGI var outputType: %s",
		     cgiUsualString("outputType", "(Undefined)"));
	}
    else if (existsAndEqual("phase", allFieldsPhase) ||
	     existsAndEqual("phase", oldAllFieldsPhase))
	doTabSeparated(TRUE);
    else if (existsAndEqual("phase", chooseFieldsPhase))
	doChooseFields();
    else if (existsAndEqual("phase", getSomeFieldsPhase))
	doTabSeparated(FALSE);
    else if (existsAndEqual("phase", seqOptionsPhase))
	doSequenceOptions();
    else if (existsAndEqual("phase", getSequencePhase))
	doGetSequence();
    else if (existsAndEqual("phase", gffPhase))
	doGetGFF();
    else if (existsAndEqual("phase", bedOptionsPhase))
	doBedOptions();
    else if (existsAndEqual("phase", getBedPhase))
	doGetBed();
    else if (existsAndEqual("phase", linksPhase))
	doGetBrowserLinks();
    else if (existsAndEqual("phase", intersectOptionsPhase))
	doIntersectOptions();
    else
	webAbort("Table Browser: CGI option error",
		 "Error: unrecognized value of CGI var phase: %s",
		 cgiUsualString("phase", "(Undefined)"));
    }
}


int main(int argc, char *argv[])
/* Process command line. */
{
struct cart *theCart;
char *excludeVars[] = {NULL};

cgiSpoof(&argc, argv);
// Sometimes we output HTML and sometimes plain text; let each outputter 
// take care of headers instead of using a fixed cart*Shell().
theCart = cartAndCookieWithHtml(hUserCookie(), excludeVars, NULL, FALSE);
doMiddle(theCart);
cartCheckout(&theCart);
return 0;
}
