#include "common.h"
#include "hCommon.h"
#include "linefile.h"
#include "cheapcgi.h"
#include "genePred.h"
#include "dnaseq.h"
#include "dystring.h"
#include "htmshell.h"
#include "jksql.h"
#include "knownInfo.h"
#include "hdb.h"
#include "hgConfig.h"
#include "hgFind.h"
#include "hash.h"
#include "hdb.h"
#include "fa.h"
#include "psl.h"
#include "nib.h"
#include "web.h"

/* constants used to refer to non-numerical chroms */
const int GENOME  = 0;
const int CHR_X   = 23;
const int CHR_Y   = 24;
const int CHR_NA  = 25;
const int CHR_UL  = 26;

static boolean wholeGenome = FALSE;

static struct sqlConnection *conn = 0;
char *database;

static char *chromName;        /* Name of chromosome sequence . */
static int winStart;           /* Start of window in sequence. */
static int winEnd;             /* End of window in sequence. */

static boolean findGenomePos(char *spec, char **retChromName, 
		    int *retWinStart, int *retWinEnd)
/* process the position information given in spec and gives a chrom name
 * as well as a start and end values on that chrom */
{
struct hgPositions *hgp;
struct hgPos *pos;
struct dyString *ui;

hgp = hgPositionsFind(spec, "", FALSE);

if (hgp == NULL || hgp->posCount == 0)
    {
    hgPositionsFree(&hgp);
    webAbort("Not found", "Sorry, couldn't locate %s in genome database\n", spec);
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
    webStart("Genome Table Browser");
    hgPositionsHtml(hgp, stdout, FALSE);
    hgPositionsFree(&hgp);
    webEnd();
    return FALSE;
    }
freeDyString(&ui);
}


void getFieldNames(struct slName** fieldNames, char* tableName)
/* returns a list of field names in the given database */
{
struct sqlResult *sr;
char **row;
char query[256] = "DESCRIBE ";

struct slName* fieldList = 0;
struct slName* el = 0; 

strncat(query, tableName, 240);
sr = sqlGetResult(conn, query);

while((row = sqlNextRow(sr)) != NULL)
    {
    el = newSlName(row[0]);
    
    slAddHead(&fieldList, el);
    }

slReverse(&fieldList);
*fieldNames = fieldList;
}

void getTblList(struct slName** positionalList, struct slName** nonPositionalList)
/* returns a list of positional and non-postional tables in the database
 * uses the global SQL connection, conn */
{
struct sqlResult *sr;
char **row;
char query[32] = "SHOW TABLES";

struct slName* posList = 0;
struct slName* nonPosList = 0;

struct slName* el = 0; 

sr = sqlGetResult(conn, query);

while((row = sqlNextRow(sr)) != NULL)
    {
    el = newSlName(row[0]);

    /* if the table is positional */
    if(hFindChromStartEndFields(row[0], query, query, query))
    	{
	slAddHead(&posList, el);
	}
    else
    	{
	slAddHead(&nonPosList, el);
	}
    }

*positionalList = posList;
*nonPositionalList = nonPosList;
}


void getPseudoTblList(struct slName** positionalList, struct slName** nonPositionalList)
/* returns a list of tables in the database (using the global connection conn). Tables
 * of the form chr*_* are maped into pseudo-table names like chrN_* */
{
struct slName* posList = 0;
struct slName* nonPosList = 0;
struct slName* current = 0;

/* the list of pseudo-table names */
struct slName* posPseudoList = 0;

struct slName* el = 0;

/* how the real table name will be brokendown */
char chromName[32];
char postfix[32];
/* and reassembled into the pseudo-table name */
char pseudoName[64];

/* get the list of tables */
getTblList(&posList, nonPositionalList);

slNameSort(nonPositionalList);

/* change the positional tables names into pseudo-names */
current = posList;
while(current != 0)
    {
    /* if the table name is of the form chr*_* write it as pseudo table chrN_* */
    if(sscanf(current->name, "chr%32[^_]_random_%32s", chromName, postfix) == 2 ||
	sscanf(current->name, "chr%32[^_]_%32s", chromName, postfix) == 2)
	{
	/* parse the name into the form chrN_* */
	snprintf(pseudoName, 64, "chrN_%s", postfix);

    	el = newSlName(pseudoName);
	}
    else
	el = newSlName(current->name);
	
    slAddHead(&posPseudoList, el);

    current = current->next;
    }

slFreeList(&posList);

slUniqify(&posPseudoList, slNameCmp, 0);
*positionalList = posPseudoList;
}


void selectTable()
{
char* freezeName;

struct slName* posList = 0;
struct slName* nonPosList = 0;
struct slName* current = 0;

/* output the freeze name and informaion */
freezeName = hFreezeFromDb(database);
if(freezeName == NULL)
        freezeName = "Unknown";

getPseudoTblList(&posList, &nonPosList);

webStart("Genome Table Browser on %s Freeze", freezeName);

//printf("<FORM ACTION=\"%s\">\n\n", hgTextName());
printf("<FORM ACTION=\"%s\">\n\n", "/cgi-bin/hgText2");
cgiMakeHiddenVar("db", cgiString("db"));
puts(
	"<TABLE BORDER=1 CELLPADDING=\"8\">" "\n"
	"	<TR>" "\n"
	"		<TD>Table Access</TD><TD>Positional Tables</TD><TD>Non-Positional Tables</TD>" "\n"
	"	</TR>" "\n"
	"	<TR>" "\n"
	"		<TD>Change position</TD><TD BGCOLOR=\"#CCFFCC\">"
);

if(wholeGenome)
    puts("<INPUT TYPE=TEXT NAME=\"position\" SIZE=30 VALUE=\"genome\">");
else
    printf("<INPUT TYPE=TEXT NAME=\"position\" SIZE=30 VALUE=\"%s:%d-%d\">\n", chromName, winStart + 1, winEnd);

//cgiMakeTextVar("position", "chr11:2572514-2601517", 30);
cgiMakeButton("look", "Look up");

puts(
	"		</TD>" "\n"
	"		<TD BGCOLOR=\"#C0EBFF\">&nbsp;</TD>" "\n"
	"	</TR>" "\n"
	"	<TR>" "\n"
	"		<TD>Choose a table</TD>" "\n"
	"		<TD BGCOLOR=\"#CCFFCC\">" "\n"
	"			<SELECT NAME=\"pos_table\" SIZE=1>" "\n"
	"				<OPTION VALUE=\"\">Positional tables</OPTION>"
);

current = posList;
while(current != 0)
    {
    /* if a table has already been selected, select it again */
    if(cgiOptionalString("pos_table") != 0 && strcmp(current->name, cgiOptionalString("pos_table")) == 0)
	printf("<OPTION SELECTED>%s</OPTION>\n", current->name);
    else
	printf("<OPTION>%s</OPTION>\n", current->name);

    current = current->next;
    }

puts(
	"		</SELECT>" "\n"
	"		</TD>" "\n"
	"		<TD BGCOLOR=\"#C0EBFF\">"
	"			<SELECT NAME=\"non_table\" SIZE=1>" "\n"
	"				<OPTION VALUE=\"\">Non-Positional tables</OPTION>"
);

current = nonPosList;
while(current != 0)
    {
    /* if a table has already been selected, select it again */
    if(cgiOptionalString("pos_table") != 0 && strcmp(current->name, cgiOptionalString("non_table")) == 0)
	printf("<OPTION SELECTED>%s</OPTION>\n", current->name);
    else
	printf("<OPTION>%s</OPTION>\n", current->name);

    current = current->next;
    }

puts(
	"		</SELECT>" "\n"
	"		</TD>" "\n"
	"	</TR>" "\n"
	"	<TR>" "\n"
	"		<TD>Enter a filter</TD><TD BGCOLOR=\"#CCFFCC\">"
);

/* if a filter has alread been given, remember it */
if(cgiOptionalString("pos_filter") == 0)
    cgiMakeTextVar("pos_filter", "", 30);
else
    cgiMakeTextVar("pos_filter", cgiOptionalString("pos_filter"), 30);

puts("</TD><TD BGCOLOR=\"#C0EBFF\">");

/* if a filter has alread been given, remember it */
if(cgiOptionalString("non_filter") == 0)
    cgiMakeTextVar("non_filter", "", 30);
else
    cgiMakeTextVar("non_filter", cgiOptionalString("non_filter"), 30);

puts(
	"		</TD>" "\n"
	"	</TR>" "\n"
	"	<TR>" "\n"
	"		<TD>Take an action</TD><TD BGCOLOR=\"#CCFFCC\">" "\n"
	"			<INPUT TYPE=\"submit\" VALUE=\"Choose fields\" NAME=\"pPhase\">" "\n"
	"			<INPUT TYPE=\"submit\" VALUE=\"Get all fields\" NAME=\"pPhase\">" "\n"
	"			<INPUT TYPE=\"submit\" VALUE=\"Get DNA\" NAME=\"phase\">" "\n"
	"		</TD><TD BGCOLOR=\"#C0EBFF\">" "\n"
	"			<INPUT TYPE=\"submit\" VALUE=\"Choose fields\" NAME=\"nPhase\">" "\n"
	"			<INPUT TYPE=\"submit\" VALUE=\"Get all fields\" NAME=\"nPhase\">" "\n"
	"		</TD>" "\n"
	"</TABLE>" "\n"
	"</FORM>"
);

webEnd();
}


boolean isPseudoTbl(char* table)
{
    if(table[0] == 'c' && table[1] == 'h' && table[2] == 'r' && table[3] == 'N' && table[4] == '_')
	return TRUE;
    else
	return FALSE;
}


char* pseudoTbl2RealTbl(char* realTbl, char* pseudoTbl, int chromNum)
{
    if(isPseudoTbl(pseudoTbl))
	{
	if(chromNum > 0)
	    {
	    if(chromNum == CHR_X)
		snprintf(realTbl, 64, "chrX_%s", pseudoTbl + 5);
	    else if(chromNum == CHR_Y)
		snprintf(realTbl, 64, "chrY_%s", pseudoTbl + 5);
	    else if(chromNum == CHR_NA)
		snprintf(realTbl, 64, "chrNA_%s", pseudoTbl + 5);
	    else if(chromNum == CHR_UL)
		snprintf(realTbl, 64, "chrUL_%s", pseudoTbl + 5);
	    else
		snprintf(realTbl, 64, "chr%d_%s", chromNum, pseudoTbl + 5);
	    }
	else
	    if(chromNum == -CHR_X)
		snprintf(realTbl, 64, "chrX_random_%s", pseudoTbl + 5);
	    else if(chromNum == -CHR_Y)
		snprintf(realTbl, 64, "chrY_random_%s", pseudoTbl + 5);
	    else if(chromNum == -CHR_NA)
		snprintf(realTbl, 64, "chrNA_random_%s", pseudoTbl + 5);
	    else if(chromNum == -CHR_UL)
		snprintf(realTbl, 64, "chrUL_random_%s", pseudoTbl + 5);
	    else
		snprintf(realTbl, 64, "chr%d_random_%s", -chromNum, pseudoTbl + 5);
	}
   else
       strncpy(realTbl, pseudoTbl, 64);

   return realTbl;
}


int getChrNumber()
{
if(strcmp(chromName, "genome") == 0)
    return GENOME;
else if(isdigit(chromName[3]))
    return atoi(chromName + 3);
else if(chromName[3] == 'X')
    return CHR_X;
else if(chromName[3] == 'Y')
    return CHR_Y;
else if(chromName[3] == 'N')
    return CHR_NA;
else
    return CHR_UL;
}

void printTabQuery(char* query, boolean outputFields)
{
struct sqlResult *sr;
char **row;
char* field;
int numColumns;
int i;
    
sr = sqlGetResult(conn, query);
numColumns = sqlCountColumns(sr);
    
/* print the field names */
if(outputFields)
    {
    printf("#");
    while((field = sqlFieldName(sr)) != 0)
	printf("%s\t", field);

    printf("\n");
    }

/* print the data */
while((row = sqlNextRow(sr)) != NULL)
    {
    for(i = 0; i < numColumns; i++)
	{
	printf("%s\t", row[i]);
	}
    printf("\n");
    }
}


void getAllFieldsPos()
/* get all the fields of the selected table */
{
char query[1024];
char realTbl[64];

char chromFieldName[32];	/* the name of the chrom field */
char startName[32];		/* the start and end names of the positional fields */ 
char endName[32];		/* in the table */

char* where;

where = trimSpaces(cloneString(cgiString("pos_filter")));

webStartText();

if(wholeGenome)
    {
    int chr;
    boolean printHeader = TRUE;

    if(where != 0)
	{
	for(chr = 1; chr <= CHR_UL; chr++) 
	    {
	    pseudoTbl2RealTbl(realTbl, cgiString("pos_table"), chr);
	    snprintf(query, 1024, "SELECT * FROM %s WHERE %s", realTbl, where);

	    if(sqlTableExists(conn, realTbl))
	    	{
		fprintf(stderr, "%s\n", query);
	        printTabQuery(query, printHeader);
	    	printHeader = FALSE;
	    	}

	    pseudoTbl2RealTbl(realTbl, cgiString("pos_table"), -chr);
	    snprintf(query, 1024, "SELECT * FROM %s WHERE %s", realTbl, where);

	    if(sqlTableExists(conn, realTbl))
	    	{
		fprintf(stderr, "%s\n", query);
	        printTabQuery(query, printHeader);
	    	printHeader = FALSE;
	    	}
	    }
	}
    else
	{
	for(chr = 1; chr <= CHR_UL; chr++) 
	    {
	    pseudoTbl2RealTbl(realTbl, cgiString("pos_table"), chr);
	    snprintf(query, 1024, "SELECT * FROM %s", realTbl);

	    if(sqlTableExists(conn, realTbl))
	    	{
		fprintf(stderr, "%s\n", query);
	        printTabQuery(query, printHeader);
	    	printHeader = FALSE;
	    	}

	    pseudoTbl2RealTbl(realTbl, cgiString("pos_table"), -chr);
	    snprintf(query, 1024, "SELECT * FROM %s", realTbl);

	    if(sqlTableExists(conn, realTbl))
	    	{
		fprintf(stderr, "%s\n", query);
	        printTabQuery(query, printHeader);
	    	printHeader = FALSE;
	    	}
	    }
	}
    }
else
    {
    pseudoTbl2RealTbl(realTbl, cgiString("pos_table"), getChrNumber());

    /* if there is no table, do nothing */
    if(!sqlTableExists(conn, realTbl))
	{
	webEnd();
	return;
	}

    /* get the name of the start and end fields */
    if(!hFindChromStartEndFields(realTbl, chromFieldName, startName, endName))
        webAbort("Internal Error #01", "Internal Error #01");

    if(where != 0)
	snprintf(query, 1024, "SELECT * FROM %s WHERE %s = \"%s\" AND %s >= %d AND %s <= %d AND %s",
		 realTbl, chromFieldName, chromName, startName, winStart, endName, winEnd, where);
    else
	snprintf(query, 1024, "SELECT * FROM %s WHERE %s = \"%s\" AND %s >= %d AND %s <= %d",
		 realTbl, chromFieldName, chromName, startName, winStart, endName, winEnd);

	
    printTabQuery(query, TRUE);
    }

webEnd();

freeMem(where);
}


int main(int argc, char *argv[])
{
struct slName* posList = 0;
struct slName* nonPosList = 0;
struct slName* current = 0;
char* position;

if(!cgiIsOnWeb())
    {
    warn("This is a CGI script - attempting to fake environment from command line");
    warn("Reading configuration options from the user's home directory.");
    cgiSpoof(&argc, argv);
    }

/* select the database, and allocate the global connection */
database = cgiString("db");
hSetDb(database);
hDefaultConnect();
conn = hAllocConn();

position = cgiOptionalString("position");
if(position == 0)
    position = "chr11:2572514-2601517";
if(position[0] == '\0')
    webAbort("Missing position", "Please enter a position");
else if(strcasecmp(position, "genome") == 0)
    wholeGenome = TRUE;
else
    {
    findGenomePos(position, &chromName, &winStart, &winEnd);
    }
    
if(cgiOptionalString("pPhase") != 0)
{
    if(strcmp(cgiOptionalString("pPhase"), "Get all fields") == 0)
	getAllFieldsPos();
    else if(strcmp(cgiOptionalString("pPhase"), "Choose fields") == 0)
	{
	webStart("Choose fields");
	webEnd();
	}
}
else if(cgiOptionalString("nPhase") != 0)
{


} else    
selectTable();

return 0;
}
