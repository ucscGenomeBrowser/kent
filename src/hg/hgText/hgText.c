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
#include "hash.h"
#include "fa.h"
#include "psl.h"
#include "nib.h"
#include "web.h"
#include "dbDb.h"
#include "kxTok.h"
#include "hgSeq.h"
#include "featureBits.h"

static char *hgFixed = "hgFixed";

/* main() sets these */
struct cart *cart = NULL;

/* main() sets this and calls hSetDb() before calling execute(): */
char *database = NULL;

/* execute() sets these */
char *freezeName;
boolean tableIsPositional;
boolean tableIsSplit;
char parsedTableName[256];
char *position;
char *chrom;
int winStart;
int winEnd;
boolean allGenome;

/* Droplist menu for selecting output type: */
#define allFieldsPhase      "Tab-separated, All fields"
#define chooseFieldsPhase   "Tab-separated, Choose fields..."
#define seqOptionsPhase     "FASTA (DNA sequence)..."
#define gffPhase            "GFF (not implemented)"
#define bedOptionsPhase     "BED/Custom Track..."
#define linksPhase          "Hyperlinks to Genome Browser"
char *outputTypePosMenu[] =
{
    allFieldsPhase,
    chooseFieldsPhase,
    seqOptionsPhase,
//#***    gffPhase,
    bedOptionsPhase,
    linksPhase,
};
//#***int outputTypePosMenuSize = 6;
int outputTypePosMenuSize = 5;
char *outputTypeNonPosMenu[] =
{
    allFieldsPhase,
    chooseFieldsPhase,
};
int outputTypeNonPosMenuSize = 2;
/* Other values that the "phase" var can take on: */
#define chooseTablePhase    "table"
#define outputOptionsPhase  "Specify output..."
#define getSomeFieldsPhase  "Get these fields"
#define getSequencePhase    "Get sequence"
#define getBedPhase         "Get BED"
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
cgiMakeHiddenVar("phase", phase);
cgiMakeButton("submit", "Look up");
}

void positionLookupSamePhase()
/* print the location and a jump button if the table is positional */
{
if (tableIsPositional)
    {
    fputs("position: ", stdout);
    positionLookup(cgiString("phase"));
    }
else
    {
    cgiMakeHiddenVar("position", position);
    }
}


void doGateway(char *errorMsg)
/* Table Browser gateway page: select organism, db, position */
{
char *organism = hOrganism(database);
char *assemblyList[128];
char *values[128];
int numAssemblies = 0;
struct dbDb *dbList = hGetIndexedDatabases();
struct dbDb *cur = NULL;
char *assembly = NULL;

webStart(cart, "Table Browser: %s", freezeName);

puts(
     "<TABLE BGCOLOR=\"fffee8\" WIDTH=\"100%\" CELLPADDING=0>\n"
     "<TR><TH HEIGHT=10></TH></TR><TR><TD WIDTH=10></TD>\n"
     "<TD><P>This tool allows you to download portions of the database used 
	by the genome browser in a simple tab-delimited text format.
	Please enter a position in the genome and press the submit button:\n"
     );

printf("(Use <A HREF=\"/cgi-bin/hgBlat?db=%s\">BLAT Search</A> to locate a 
        particular sequence in the genome.)	
	<P><FORM ACTION=\"%s\" METHOD=\"GET\">Freeze:\n",
       database, hgTextName());

/* Find all the assemblies that pertain to the selected genome */
for (cur = dbList; cur != NULL; cur = cur->next)
    {
    /* If we are looking at a zoo database then show the zoo database list */
    if ((strstrNoCase(database, "zoo") || strstrNoCase(organism, "zoo")) &&
        strstrNoCase(cur->description, "zoo"))
        {
        assemblyList[numAssemblies] = cur->description;
        values[numAssemblies] = cur->name;
        numAssemblies++;
        }
    else if (strstrNoCase(organism, cur->organism) && 
             !strstrNoCase(cur->description, "zoo") &&
             (cur->active || strstrNoCase(cur->name, database)))
        {
        assemblyList[numAssemblies] = cur->description;
        values[numAssemblies] = cur->name;
        numAssemblies++;
        }

    /* Save a pointer to the current assembly */
    if (strstrNoCase(database, cur->name))
	{
	assembly = cur->description;
	}
    }

cgiMakeDropListFull("db", assemblyList, values, numAssemblies, assembly, NULL);
printf(" &nbsp; Genome position:\n");
position = cgiUsualString("position", hDefaultPos(database));
positionLookup(chooseTablePhase);

puts(
     "<P>A genome position can be specified by the accession number of a
	sequenced genomic clone, an mRNA or EST or STS marker, or a
	cytological band, a chromosomal coordinate range, or keywords
	from the Genbank description of an mRNA. The following list
	provides examples of various types of position queries for the
	human genome. Analogous queries can be made for many of these
	in the mouse genome. See the 
	<A HREF=\"/goldenPath/help/hgTracksHelp.html\" TARGET=_blank>
	User Guide</A> for more help.
<P>
<TABLE  border=0 CELLPADDING=0 CELLSPACING=0>
<TR><TD VALIGN=Top NOWRAP><B>Request:</B><br></TD>
<TD VALIGN=Top COLSPAN=2><B>&nbsp;&nbsp; Genome Browser Response:</B><br></TD>
</TR>
<TR><TD VALIGN=Top><br></TD></TR>
<TR><TD VALIGN=Top NOWRAP>chr7</TD>
	<TD WIDTH=14></TD>
	<TD VALIGN=Top>Displays all of chromosome 7</TD></TR>
<TR><TD VALIGN=Top NOWRAP>20p13</TD>
	<TD WIDTH=14></TD>
	<TD VALIGN=Top>Displays region for band p13 on chr 20</TD></TR>
<TR><TD VALIGN=Top NOWRAP>chr3:1-1000000</TD>
	<TD WIDTH=14></TD>
	<TD VALIGN=Top>Displays first million bases of chr 3, counting from p arm telomere</TD></TR>

<TR><TD VALIGN=Top><br></TD></TR>

<TR><TD VALIGN=Top NOWRAP>D16S3046</TD>
	<TD WIDTH=14></TD>
	<TD VALIGN=Top>Displays region around STS marker D16S3046 from the Genethon/Marshfield maps
	(open \"STS Markers\" track by clicking to see this marker)</TD></TR>
<TR><TD VALIGN=Top NOWRAP>D22S586;D22S43</TD>
	<TD WIDTH=14></TD>
	<TD VALIGN=Top>Displays region between STS markers D22S586 and D22S43.
	Includes 250,000 bases to either side as well.
<TR><TD VALIGN=Top NOWRAP>AA205474</TD>
	<TD WIDTH=14></TD>
	<TD VALIGN=Top>Displays region of EST with GenBank accession AA205474 in BRCA1 cancer gene on chr 17
	(open \"spliced ESTs\" track by clicking to see this EST)</TD></TR>
<TR><TD VALIGN=Top NOWRAP>AC008101</TD>
	<TD WIDTH=14></TD>
	<TD VALIGN=Top>Displays region of clone with GenBank accession number AC008101
	(open \"coverage\" track by clicking to see this clone)</TD></TR>
<TR><TD VALIGN=Top><br></TD></TR>
<TR><TD VALIGN=Top NOWRAP>AF083811</TD>
	<TD WIDTH=14></TD>
	<TD VALIGN=Top>Displays region of mRNA with GenBank accession number
	AF083811</TD></TR>
<TR><TD VALIGN=Top NOWRAP>PRNP</TD>
	<TD WIDTH=14></TD>
	<TD>Displays region of genome with HUGO identifier PRNP</TD></TR>
<TR><TD VALIGN=Top><br></TD></TR>
<TR><TD VALIGN=Top NOWRAP>pseudogene mRNA</TD>
	<TD WIDTH=14></TD>
	<TD VALIGN=Top>Lists transcribed pseudogenes but not cDNAs</TD></TR>
<TR><TD VALIGN=Top NOWRAP>homeobox caudal</TD>
	<TD WIDTH=14></TD>
	<TD VALIGN=Top>Lists mRNAs for caudal homeobox genes</TD></TR>
<TR><TD VALIGN=Top NOWRAP>zinc finger</TD>
	<TD WIDTH=14></TD>
	<TD VALIGN=Top>Lists many zinc finger mRNAs</TD></TR>
<TR><TD VALIGN=Top NOWRAP>kruppel zinc finger</TD>
	<TD WIDTH=14></TD>
	<TD VALIGN=Top>Lists only kruppel-like zinc fingers</TD></TR>
<TR><TD VALIGN=Top NOWRAP>huntington</TD>
	<TD WIDTH=14></TD>
	<TD VALIGN=Top>Lists candidate genes associated with Huntington's disease</TD></TR>
<TR><TD VALIGN=Top NOWRAP>zahler</TD>
	<TD WIDTH=14></TD>
	<TD VALIGN=Top>Lists mRNAs deposited by scientist named Zahler</TD></TR>
<TR><TD VALIGN=Top NOWRAP>Evans,J.E.</TD>
	<TD WIDTH=14></TD>
	<TD VALIGN=Top>Lists mRNAs deposited by co-author J.E. Evans</TD></TR>

<TR><TD VALIGN=Top><br></TD></TR>
<TR><TD COLSPAN=\"3\" >
Use this last format for entry authors -- even though Genbank
searches require Evans JE format, GenBank entries themselves use
Evans,J.E. internally.
</TABLE>
	</TD><TD WIDTH=15></TD></TR></TABLE>
	<BR></TD></TR></TABLE>
</FORM>
<BR></TD></TR></TABLE>
</BODY></HTML>"
     );
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
    webAbort("Error", "Malformatted %s \"%s\".", desc, word);
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
struct dyString *ui;

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
    webStart(cart,  "Table Browser");
    hgPositionsHtml(hgp, stdout, FALSE, NULL);
    hgPositionsFree(&hgp);
    webEnd();
    return FALSE;
    }
freeDyString(&ui);
}


char *getPosition(char **retChrom, int *retStart, int *retEnd)
{
char *pos = cgiOptionalString("position");

/* if the position information is not given, get it */
if (pos == NULL)
    pos = "";
if (pos[0] == '\0')
    {
    doGateway("Missing position: Please enter a position");
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

void doChooseTable()
/* get the a table selection from the user */
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
char buf[256];

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

printf("<FORM ACTION=\"%s\">\n\n", hgTextName());
cgiMakeHiddenVar("db", database);
puts("<TABLE CELLPADDING=\"8\">");
puts("<TR><TD>");

if (! sameString(position, "genome"))
    {
    sprintf(buf, "%s:%d-%d", chrom, winStart+1, winEnd);
    position = cloneString(buf);
    }

puts("Choose a position: ");
puts("</TD><TD>");
positionLookup(chooseTablePhase);
puts("</TD></TR>");

puts("<TR><TD>");

/* get the list of tables */
puts("Choose a table:");
puts("</TD><TD>");
puts("<SELECT NAME=table0 SIZE=1>");
printf("<OPTION VALUE=\"Choose table\">Positional tables</OPTION>\n");
printSelectOptions(posTableList, "table0", database);
printSelectOptions(fixedPosTableList, "table0", hgFixed);
puts("</SELECT>");

puts("<SELECT NAME=table1 SIZE=1>");
printf("<OPTION VALUE=\"Choose table\">Non-positional tables</OPTION>\n");
printSelectOptions(nonposTableList, "table1", database);
printSelectOptions(fixedNonposTableList, "table1", hgFixed);
puts("</SELECT>");

puts("</TD></TR>");

puts("<TR><TD>");
puts("Choose an action: ");
puts("</TD><TD>");
cgiMakeButton("phase", allFieldsPhase);
cgiMakeButton("phase", outputOptionsPhase);
puts("</TD></TR>");
puts("</TABLE>");

puts("</FORM>");

hashElFreeList(&posTableList);

webNewSection("Getting started on the Table Browser");

puts(
     "This web tool allows convenient and precise access to the primary
database tables containing the genome sequence and associated
annotation tracks. By specifying chromosomal range and table type, the
exact data set of interest can be viewed.  This tool thus makes it
unnecessary to download and manipulate the genome itself and its
massive data tracks (though that option is will always remain <A
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
     "Familiarize yourself with the table browser tool by a <B>few minutes
of exploration</B>. Starting at the top of the form, use the default
position (or enter a cytological band, eg 20p12, or type in a keyword
from a GenBank entry to look up). Next select a positional table from
the menu, for example, chrN_mrna.  Choosing an action will then
display -- relative to the chromosomal range in the text box -- a
option to view and refine displayed data fields, all the mRNA
coordinate data, or the genomic DNA sequences of the RNAs.  If the
chromosomal range is too narrow, it may happen that no mRNA occurs
there (meaning no data will be returned)." "\n"
     "<P>" "\n"
     "Notice that if you look up a gene or accession number, the tool
typically returns a choice of several mRNAs.  Selecting one of these
returns the table browser to its starting place but with the
chromosomal location automaticallyentered in the text box." "\n"
     "<P>" "\n"
     /*"The non-positional tables display all entries associated with
       the selected assembly -- there is no way to restrict them to
       chromosomes.  However there is a provision in the text box to
       add a single word restriction.  Thus, to see which species of
       macaque monkeys have contributed sequence data, selected the
       organism table, view all fields, and restrict the text box to
       the genus Macaca." "\n" "<P>" "\n"*/
     "For either type of table, a user not familiar with what the table
offers should select \"Filter fields\".  Some tables offer many
columns of data of which only a few might be relevent. The fewer
fields selected, the simpler the next stage of data return.  If all
the fields are wanted, shortcut this step by selecting \"Get all
fields\".  If a chromosome and range are in the text box, \"Get DNA\"
will retrieve genomic sequences in fasta format without any table
attributes other than chromosome, freeze date, and start-stop genomic
coordinates." "\n"
     );
}

void parseTableName(char *dest, char *chrom)
/* given a chrom return the table name of the table selected by the user */
{
char *table;
char post[64];

table = getTableName();

if (chrom == NULL)
    chrom = "chr1";
if (sscanf(table, "chrN_%64s", post) == 1)
    snprintf(dest, 256, "%s_%s", chrom, post);
else
    strncpy(dest, table, 256);
}


void doOutputOptions()
{
struct sqlConnection *conn;
struct sqlResult *sr;
char **row;
char query[256];
char name[128];
char *newVal;
boolean gotFirst;

if (sameString(database, getTableDb()))
    conn = hAllocConn();
else
    conn = hAllocConn2();

webStart(cart, "Table Browser: %s", freezeName);

checkTableExists(parsedTableName);

printf("<FORM ACTION=\"%s\">\n\n", hgTextName());
cgiMakeHiddenVar("db", database);
cgiMakeHiddenVar("table", getTableVar());
positionLookupSamePhase();

printf("<H3> Select Output Format for Table %s </H3>\n", getTableName());
if (tableIsPositional)
    cgiMakeDropList("phase", outputTypePosMenu, outputTypePosMenuSize,
		    outputTypePosMenu[0]);
else
    cgiMakeDropList("phase", outputTypeNonPosMenu, outputTypeNonPosMenuSize,
		    outputTypeNonPosMenu[0]);
cgiMakeButton("submit", "Submit");

puts("<H3> (Optional) Filter Table Records by Field Values </H3>");

snprintf(query, 256, "DESCRIBE %s", parsedTableName);
sr = sqlGetResult(conn, query);

puts("<TABLE><TR><TD>\n");
puts("Free-form query: ");
cgiMakeTextVar("rawQuery", "", 50);
strncpy(name, "log_rawQuery", sizeof(name));
cgiMakeDropList(name, logOpMenu, logOpMenuSize, logOpMenu[0]);
puts(" </TD></TR><TR><TD>");
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
	printf("<TR><TD> %s </TD><TD>\n", row[0]);
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
puts("</TD></TR></TABLE>\n");
if (sameString(database, getTableDb()))
    hFreeConn(&conn);
else
    hFreeConn2(&conn);
	
cgiMakeButton("submit", "Submit");
puts("</FORM>");

puts(
     "<P>\n"
     "By default, all lines in the specified position range will be returned. \n"
     "To restrict the number of lines that are returned, you may enter \n"
     "patterns with wildcards for each text field, and/or enter range \n"
     "restrictions for numeric fields. \n"
     "</P><P>\n"
     "Wildcards are \"*\" (to match \n"
     "0 or more characters) and \"?\" (to match a single character).  \n"
     "Each word in a text field box will be treated as a pattern to match \n"
     "against that field (implicit OR).  If any pattern matches, that field \n"
     "matches. \n"
     "</P><P>\n"
     "For numeric fields, enter a single number for operators such as &lt;, \n"
     "&gt;, != etc.  Enter two numbers (start and end) separated by a \"-\" or \n"
     "\",\" for \"in range\". \n"
     "</P>\n"
     );

puts("</TD></TR></TABLE>");
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

if ((rawQuery == NULL) || (rawQuery[0] == 0))
    return;

/* tokenize (do allow wildcards, and include quotes.) */
kxTokIncludeQuotes(TRUE);
tokList = kxTokenize(rawQuery, TRUE);

/* to be extra conservative, wrap the whole expression in parens. */
dyStringAppend(clause, "(");
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
	(tokPtr->type == kxtOpenParen) ||
	(tokPtr->type == kxtCloseParen) ||
	(tokPtr->type == kxtAdd) ||
	(tokPtr->type == kxtSub) ||
	(tokPtr->type == kxtDiv))
	{
	dyStringAppend(clause, tokPtr->string);
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

slFreeList(&tokList);
}

char *constrainFields()
/* If the user specified constraints, append SQL conditions (suitable
 * for a WHERE clause) to q. */
{
struct cgiVar *current;
struct dyString *freeClause = newDyString(512);
struct dyString *andClause = newDyString(512);
struct dyString *clause;
char *fieldName;
char *rawQuery = cgiOptionalString("rawQuery");
char *rQLogOp  = cgiOptionalString("log_rawQuery");
char *dd, *cmp, *pat;
char varName[128];
char *ret;

dyStringClear(andClause);
for (current = cgiVarList();  current != NULL;  current = current->next)
    {
    /* Look for pattern variable associated with each field. */
    if (startsWith("pat_", current->name))
	{	
	fieldName = current->name + strlen("pat_");
	/* make sure that the field name doesn't have anything "weird" in it */
	checkIsAlpha("field name", fieldName);
	pat = current->val;
	snprintf(varName, sizeof(varName), "dd_%s", fieldName);
	dd = cgiOptionalString(varName);
	snprintf(varName, sizeof(varName), "cmp_%s", fieldName);
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
constrainFreeForm(rawQuery, freeClause);
// force rQLogOp to a legit value:
if (! sameString("AND", rQLogOp))
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


void preserveConstraints()
{
struct cgiVar *current;

for (current = cgiVarList();  current != NULL;  current = current->next)
    {
    /* Look for pattern variable associated with each field. */
    if (startsWith("pat_", current->name) ||
	startsWith("dd_", current->name) ||
	startsWith("cmp_", current->name) ||
	startsWith("log_", current->name) ||
	sameString("rawQuery", current->name))
	{
	cgiMakeHiddenVar(current->name, current->val);
	}
    }
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
    printf("Content-Type: text/plain\n\n");
    webStartText();
    initialized = TRUE;
    }

/* print the columns names */
printf("#");
while((field = sqlFieldName(sr)) != NULL)
    printf("%s\t", field);
printf("\n");

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
int numberColumns;
char chromFieldName[32];
char startName[32];
char endName[32];
char *constraints = constrainFields();
boolean gotResults;
boolean gotFirst = FALSE;

checkTableExists(parsedTableName);

if (allGenome)
    chromList = hAllChromNames();
else
    chromList = newSlName(chrom);

if (sameString(database, getTableDb()))
    conn = hAllocConn();
else
    conn = hAllocConn2();

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

gotResults = FALSE;
if (tableIsSplit)
    {
    for (chromPtr=chromList;  chromPtr != NULL;  chromPtr = chromPtr->next)
	{
	parseTableName(parsedTableName, chromPtr->name);
	dyStringClear(query);
	dyStringPrintf(query, "SELECT %s FROM %s",
		       fieldSpec->string, parsedTableName);
	if ((! allGenome) &&
	    hFindChromStartEndFieldsDb(getTableDb(), parsedTableName,
				       chromFieldName, startName, endName))
	    {
	    dyStringPrintf(query, " WHERE %s < %d AND %s > %d",
			   startName, winEnd, endName, winStart);
	    if ((constraints != NULL) && (constraints[0] != 0))
		dyStringPrintf(query, " AND %s", constraints);
	    }
	else if ((constraints != NULL) && (constraints[0] != 0))
	    dyStringPrintf(query, " WHERE %s", constraints);
	sr = sqlGetResult(conn, query->string);
	gotResults = printTabbedResults(sr, gotResults);
	}
    }
else
    {
    dyStringClear(query);
    dyStringPrintf(query, "SELECT %s FROM %s",
		   fieldSpec->string, parsedTableName);
    if ((! allGenome) &&
	hFindChromStartEndFieldsDb(getTableDb(), parsedTableName,
				   chromFieldName, startName, endName))
	{
	dyStringPrintf(query, " WHERE %s < %d AND %s > %d",
		       startName, winEnd, endName, winStart);
	if (! sameString("", chromFieldName))
	    dyStringPrintf(query, " AND %s = \'%s\'",
			   chromFieldName, chrom);
	if ((constraints != NULL) && (constraints[0] != 0))
	    dyStringPrintf(query, " AND %s", constraints);
	}
    else if ((constraints != NULL) && (constraints[0] != 0))
	dyStringPrintf(query, " WHERE %s", constraints);
    sr = sqlGetResult(conn, query->string);
    gotResults = printTabbedResults(sr, gotResults);
    }
if (! gotResults)
    webAbort("Empty Result", "Your query produced no results.");

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
char query[256];
char name[128];
char *newVal;
boolean checkAll;
boolean clearAll;

if (sameString(database, getTableDb()))
    conn = hAllocConn();
else
    conn = hAllocConn2();

checkTableExists(parsedTableName);

webStart(cart, "Table Browser: %s", freezeName);
printf("<FORM ACTION=\"%s\">\n\n", hgTextName());
cgiMakeHiddenVar("db", database);
cgiMakeHiddenVar("table", getTableVar());
positionLookupSamePhase();
preserveConstraints();

snprintf(query, sizeof(query), "DESCRIBE %s", parsedTableName);
sr = sqlGetResult(conn, query);


printf("<H3> Select Fields of Table %s: </H3>\n", getTableName());
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
webStart(cart, "Table Browser: %s", freezeName);
printf("<FORM ACTION=\"%s\">\n\n", hgTextName());
cgiMakeHiddenVar("db", database);
cgiMakeHiddenVar("table", getTableVar());
preserveConstraints();
positionLookupSamePhase();
printf("<H3>Table: %s</H3>\n", getTableName());
hgSeqOptionsDb(getTableDb(), parsedTableName);
cgiMakeButton("phase", getSequencePhase);
puts("</FORM>");
webEnd();
}


void doGetSequence()
{
struct slName *chromList, *chromPtr;
char *constraints = constrainFields();
int itemCount;

if (allGenome)
    chromList = hAllChromNames();
else
    chromList = newSlName(chrom);

printf("Content-Type: text/plain\n\n");
webStartText();
itemCount = 0;
for (chromPtr=chromList;  chromPtr != NULL;  chromPtr = chromPtr->next)
    {
    parseTableName(parsedTableName, chromPtr->name);
    if (allGenome)
	{
	winStart = 0;
	winEnd   = hChromSize(chromPtr->name);
	}
    itemCount += hgSeqItemsInRangeDb(getTableDb(), parsedTableName,
				     chromPtr->name, winStart, winEnd,
				     constraints);
    }
if (itemCount == 0)
    printf("\n# No results returned from query.\n\n");
}


void doGetGFF()
{
char *constraints = constrainFields();
webStart(cart, "Table Browser: %s", freezeName);
puts("<H3> Not implemented yet </H3>");
webEnd();
}


void doBedOptions()
{
char *table = getTableName();
char *track = getTrackName();
webStart(cart, "Table Browser: %s", freezeName);

if (! fbUnderstandTrack(track))
    webAbort("Not yet supported",
	     "Sorry, BED output not supported yet for table %s.",
	     table);

puts("<FORM ACTION=\"/cgi-bin/hgText\" METHOD=\"GET\">\n");
cgiMakeHiddenVar("db", database);
cgiMakeHiddenVar("table", getTableVar());
preserveConstraints();
positionLookupSamePhase();
printf("<H3> Select BED Options for Table %s: </H3>\n", table);
puts("<TABLE><TR><TD>");
cgiMakeCheckBox("hgt.doCustomTrack", TRUE);
puts("</TD><TD> <B> Print custom track header: </B>");
puts("</TD></TR><TR><TD></TD><TD>name=");
cgiMakeTextVar("hgt.ctName", "", 16);
puts("</TD></TR><TR><TD></TD><TD>description=");
cgiMakeTextVar("hgt.ctDesc", "", 50);
puts("</TD></TR><TR><TD></TD><TD>visibility=");
cgiMakeDropList("hgt.ctVis", ctVisMenu, ctVisMenuSize, ctVisMenu[1]);
puts("</TD></TR><TR><TD></TD><TD>url=");
cgiMakeTextVar("hgt.ctUrl", "", 50);
puts("</TABLE>");
puts("<P> <B> Create one BED record per: </B>");
fbOptions(track);
cgiMakeButton("phase", getBedPhase);
puts("</FORM>");
webEnd();
}


void doGetBed()
{
struct featureBits *fbList = NULL, *fbPtr;
struct slName *chromList, *chromPtr;
char *constraints = constrainFields();
char *table = getTableName();
char *track = getTrackName();
boolean doCT = cgiBoolean("hgt.doCustomTrack");
char *ctName = cgiUsualString("hgt.ctName", table);
char *ctDesc = cgiUsualString("hgt.ctDesc", table);
char *ctVis  = cgiUsualString("hgt.ctVis", "dense");
char *ctUrl  = cgiUsualString("hgt.ctUrl", "");
char *fbQual = fbOptionsToQualifier();
char fbTQ[128];
int totalCount;

if ((fbQual == NULL) || (fbQual[0] == 0))
    strncpy(fbTQ, track, sizeof(fbTQ));
else
    snprintf(fbTQ, sizeof(fbTQ), "%s:%s", track, fbQual);

if (! fbUnderstandTrack(track))
    {
    webStart(cart, "Table Browser: %s", freezeName);
    webAbort("Not yet supported",
	     "Sorry, BED output not supported yet for table %s.",
	     table);
    }

if (allGenome)
    chromList = hAllChromNames();
else
    chromList = newSlName(chrom);

printf("Content-Type: text/plain\n\n");
webStartText();

//#*** HACK in constraints!!!
if ((constraints != NULL) && (constraints[0] != 0))
    printf("# Note: filtering on fields not yet supported for BED output!\n");

if (doCT)
    {
    int visNum = (sameString("hide", ctVis) ? 0 :
		  sameString("full", ctVis) ? 2 : 1);
    printf("track name=\"%s\" description=\"%s\" visibility=%d url=%s \n",
	   ctName, ctDesc, visNum, ctUrl);
    }

totalCount = 0;
for (chromPtr=chromList;  chromPtr != NULL;  chromPtr = chromPtr->next)
    {
    parseTableName(parsedTableName, chromPtr->name);
    if (allGenome)
	{
	winStart = 0;
	winEnd   = hChromSize(chromPtr->name);
	}
    fbList = fbGetRange(fbTQ, chromPtr->name, winStart, winEnd);
    for (fbPtr=fbList;  fbPtr != NULL;  fbPtr=fbPtr->next)
	{
	char *ptr = strchr(fbPtr->name, ' ');
	if (ptr != NULL)
	    *ptr = 0;
	printf("%s\t%d\t%d\t%s\t%d\t%c\n",
	       fbPtr->chrom, fbPtr->start, fbPtr->end, fbPtr->name,
	       0, fbPtr->strand);
	totalCount++;
	}
    featureBitsFreeList(&fbList);
    }
if (totalCount == 0)
    printf("\n# No results returned from query.\n\n");
}


int printLinkResults(struct sqlResult *sr, char *trackName, char *chromName,
		      char *chromField, char *nameField)
{
char **row;
char *chrom, *name, *strand;
int start, end;
int rowCount = 0;

while ((row = sqlNextRow(sr)) != NULL)
    {
    start  = atoi(row[0]) + 1;
    end    = atoi(row[1]);
    chrom  = chromField[0] ? row[2] : chromName;
    name   = nameField[0] ? row[3] : "";
    strand = row[4];
    printf("<A HREF=\"%s?db=%s&position=%s:%d-%d&%s=full\"",
	   hgTracksName(), hGetDb(), chrom, start, end, trackName);
    printf(" TARGET=_blank> %s %s:%d-%d <BR>\n", name, chrom, start, end);
    rowCount++;
    }
return(rowCount);
}


void doGetBrowserLinks()
{
struct dyString *fieldSpec = newDyString(256);
struct dyString *query = newDyString(512);
struct slName *chromList, *chromPtr;
struct sqlConnection *conn;
struct sqlResult *sr;
char *constraints = constrainFields();
char *trackName;
char chromField[32], startField[32], endField[32], nameField[32],
    strandField[32];
int rowCount;

trackName = getTrackName();

webStart(cart, "Table Browser: %s", freezeName);
checkTableExists(parsedTableName);
if (sameString(database, getTableDb()))
    conn = hAllocConn();
else
    conn = hAllocConn2();
if (allGenome)
    chromList = hAllChromNames();
else
    chromList = newSlName(chrom);


printf("<H3> Links to Genome Browser from Table %s </H3>\n", getTableName());
hFindBed6FieldsDb(getTableDb(), parsedTableName, chromField, startField,
		  endField, nameField, strandField);

dyStringClear(fieldSpec);
dyStringPrintf(fieldSpec, "%s,%s", startField, endField);
if (chromField[0] != 0)
    dyStringPrintf(fieldSpec, ",%s", chromField);
else
    dyStringPrintf(fieldSpec, ",%s", startField);  // keep same #fields!
if (nameField[0] != 0)
    dyStringPrintf(fieldSpec, ",%s", nameField);
else
    dyStringPrintf(fieldSpec, ",%s", startField);  // keep same #fields!
if (strandField[0] != 0)
    dyStringPrintf(fieldSpec, ",%s", strandField);

rowCount = 0;
if (tableIsSplit)
    {
    for (chromPtr=chromList;  chromPtr != NULL;  chromPtr = chromPtr->next)
	{
	parseTableName(parsedTableName, chromPtr->name);
	dyStringClear(query);
	dyStringPrintf(query, "SELECT %s FROM %s",
		       fieldSpec->string, parsedTableName);
	if (! allGenome)
	    {
	    dyStringPrintf(query, " WHERE %s < %d AND %s > %d",
			   startField, winEnd, endField, winStart);
	    if ((constraints != NULL) && (constraints[0] != 0))
		dyStringPrintf(query, " AND %s", constraints);
	    }
	else if ((constraints != NULL) && (constraints[0] != 0))
	    dyStringPrintf(query, " WHERE %s", constraints);
	sr = sqlGetResult(conn, query->string);
	rowCount += printLinkResults(sr, trackName, chromPtr->name,
				     chromField, nameField);
	}
    }
else
    {
    dyStringClear(query);
    dyStringPrintf(query, "SELECT %s FROM %s",
		   fieldSpec->string, parsedTableName);
    if (! allGenome)
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
    rowCount += printLinkResults(sr, trackName, chrom, chromField, nameField);
    }

if (rowCount == 0)
    puts("No results returned from query.");
webEnd();

dyStringFree(&fieldSpec);
dyStringFree(&query);
if (sameString(database, getTableDb()))
    hFreeConn(&conn);
else
    hFreeConn2(&conn);
}


void execute()
/* the main body of the program */
{
char *table = getTableName();
char trash[32];

freezeName = hFreezeFromDb(database);
if (freezeName == NULL)
    freezeName = "Unknown";

tableIsPositional = FALSE;
tableIsSplit = FALSE;
parsedTableName[0] = 0;
chrom = NULL;
winStart = winEnd = 0;
position = getPosition(&chrom, &winStart, &winEnd);
if (position == NULL)
    return;
allGenome = sameString("genome", position);

if (table == NULL || existsAndEqual("phase", chooseTablePhase))
    {
    if (existsAndEqual("table0", "Choose table") &&
	existsAndEqual("table1", "Choose table") &&
	!existsAndEqual("submit", "Look up"))
	webAbort("Missing table selection",
		 "Please choose a table and try again.");
    else
	{
	webStart(cart, "Table Browser: %s", freezeName);
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
	parseTableName(parsedTableName, "chr1");
    else
	parseTableName(parsedTableName, chrom);
    /* make sure that the table name doesn't have anything "weird" in it */
    checkIsAlpha("table name", parsedTableName);

    tableIsPositional = hFindChromStartEndFieldsDb(getTableDb(),
						   parsedTableName,
						   trash, trash, trash);
    if (strstr(table, "chrN_") == table)
	tableIsSplit = TRUE;

    if (existsAndEqual("phase", outputOptionsPhase))
	doOutputOptions();
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
    else
	webAbort("Table Browser: CGI option error",
		 "Error: unrecognized value of CGI var phase: %s",
		 cgiUsualString("phase", "(Undefined)"));
    }
}


int main(int argc, char *argv[])
/* Process command line. */
{
struct hash *oldVars = hashNew(8);
char *excludeVars[] = {NULL};
char *cookieName = "hguid";

if (!cgiIsOnWeb())
    {
    warn("This is a CGI script - attempting to fake environment from command line");
    warn("Reading configuration options from the user's home directory.");
    cgiSpoof(&argc, argv);
    }

cart = cartAndCookieWithHtml(cookieName, excludeVars, oldVars, FALSE);

/* select the database */
database = cartUsualString(cart, "db", hGetDb());
hSetDb(database);
hDefaultConnect();

/* call the main function */
execute();

cartCheckout(&cart);
return 0;
}
