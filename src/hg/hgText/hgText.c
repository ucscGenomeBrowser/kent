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
#include "hdb.h"
#include "fa.h"
#include "psl.h"
#include "nib.h"
#include "web.h"

/* Variables used by getFeatDna code */
char *database;			/* Which database? */
int chromStart = 0;		/* Start of range to select from. */
int chromEnd = BIGNUM;          /* End of range. */
char *where = NULL;		/* Extra selection info. */
boolean breakUp = FALSE;	/* Break up things? */
int merge = -1;			/* Merge close blocks? */
char *outputType = "fasta";	/* Type of output. */

static int blockIx = 0;	/* Index of block written. */


char* getTableVar()
{
char* table  = cgiOptionalString("table");
char* table0 = cgiOptionalString("table0");
char* table1 = cgiOptionalString("table1");
	
if(table != 0 && strcmp(table, "Choose table") == 0)
	table = 0;

if(table0 != 0 && strcmp(table0, "Choose table") == 0)
	table0 = 0;
	
if(table1 != 0 && strcmp(table1, "Choose table") == 0)
	table1 = 0;

if(table != 0)
	return table;
else if(table0 != 0)
	return table0;
else
	return table1;
}


static boolean existsAndEqual(char* var, char* value)
/* returns true is the given CGI var exists and equals value */
{
	if(cgiOptionalString(var) != 0 && sameString(cgiOptionalString(var), value))
		return TRUE;
	else
		return FALSE;
}


static boolean findPositionInGenome(char *spec, char **retChromName, 
		    int *retWinStart, int *retWinEnd)
/* process the position information given in spec and gives a chrom name
 * as well as a start and end values on that chrom */
{ 
struct hgPositions *hgp;
struct hgPos *pos;
struct dyString *ui;

hgp = hgPositionsFind(spec, "", FALSE, NULL);

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
    hgPositionsHtml(hgp, stdout, FALSE, NULL);
    hgPositionsFree(&hgp);
    webEnd();
    return FALSE;
    }
freeDyString(&ui);
}


static boolean allLetters(char* s)
/* returns true if the string only has letters number and underscores */
{
int i;
for(i = 0; i < strlen(s); i++)
	if(!isalnum(s[i]) && s[i] != '_')
		return FALSE;

return TRUE;
}


static void printTables(struct hashEl *hel)
/* this function is used to iterates over a hash table and prints out
 * the name in a input form */
{
	printf("<OPTION>%s\n", hel->name);
}


int compareTable(const void *elem1,  const void *elem2)
/* compairs two hash element by name */
{
struct hashEl* a = *((struct hashEl **)elem1);
struct hashEl* b = *((struct hashEl **)elem2);
	
return strcmp(a->name, b->name);
}


void getTable()
/* get the a table selection from the user */
{
char* database;

char *chromName;        /* Name of chromosome sequence . */
int winStart;           /* Start of window in sequence. */
int winEnd;         /* End of window in sequence. */

char* position;
char* freezeName;

struct sqlConnection *conn;
struct sqlResult *sr;
char **row;
char query[256];

struct hash *posTableHash = newHash(7);
struct hashEl *posTableList;
struct hash *nonposTableHash = newHash(7);
struct hashEl *nonposTableList;

struct hashEl *currentListEl;

position = cgiOptionalString("position");

/* select the database */
database = cgiUsualString("db", hGetDb());
hSetDb(database);
hDefaultConnect();
conn = hAllocConn();

/* if the position information is not given, get it */
if(position == NULL)
	position = "";
if(position[0] == '\0')
    webAbort("Missing position", "Please enter a position");
if(strcmp(position, "genome"))
    {
    if(position != NULL && position[0] != 0)
	{
	if (!findPositionInGenome(position, &chromName, &winStart, &winEnd))
		return;
	}
    findPositionInGenome(position, &chromName, &winStart, &winEnd);
    }

/* iterate through all the tables and store the positional ones in a list */
strcpy(query, "SHOW TABLES");
sr = sqlGetResult(conn, query);
while((row = sqlNextRow(sr)) != NULL)
	{
	if(strcmp(row[0], "all_est") == 0 || strcmp(row[0], "all_mrna") == 0)
		continue;

	/* if they are positional, print them */
	if(hFindChromStartEndFields(row[0], query, query, query))
		{
		char chrom[32];
		char post[32];
		char key[32];

		/* if table name is of the form, chr*_random_* or chr*_* */
		if(sscanf(row[0], "chr%32[^_]_random_%32s", chrom, post) == 2 ||
			sscanf(row[0], "chr%32[^_]_%32s", chrom, post) == 2)
			{
			/* parse the name into chrN_* */
			snprintf(key, 32, "chrN_%s", post);
			/* and insert it into the hash table if it is not already there */
			hashStoreName(posTableHash, key);
			}
		else
			hashStoreName(posTableHash, row[0]);
		}
	else
		{
		hashStoreName(nonposTableHash, row[0]);
		}
	}
sqlFreeResult(&sr);
sqlDisconnect(&conn);

/* print the form */
printf("<FORM ACTION=\"%s\">\n\n", hgTextName());
puts("<TABLE CELLPADDING=\"8\">");
puts("<TR><TD>");

/* print the location and a jump button */
{
    char buf[256];
    sprintf(buf, "%s:%d-%d", chromName, winStart+1, winEnd);

    puts("Choose a position: ");
    puts("</TD><TD>");
    cgiMakeTextVar("position", buf, 30);
}
cgiMakeButton("submit", "Look up");
cgiMakeHiddenVar("db", database);
cgiMakeHiddenVar("phase", "table");
puts("</TD></TR>");

puts("<TR><TD>");
/* get the list of tables */
puts("Choose a table:");
puts("</TD><TD>");
puts("<SELECT NAME=table0 SIZE=1>");
printf("<OPTION VALUE=\"Choose table\">Positional tables</OPTION>\n");

posTableList = hashElListHash(posTableHash);
slSort(&posTableList, compareTable);

nonposTableList = hashElListHash(nonposTableHash);
slSort(&nonposTableList, compareTable);

currentListEl = posTableList;
while(currentListEl != 0)
	{
	if(existsAndEqual("table0", currentListEl->name))
		printf("<OPTION SELECTED>%s</OPTION>\n", currentListEl->name);
	else
		printf("<OPTION>%s</OPTION>\n", currentListEl->name);

	currentListEl = currentListEl->next;
	}
puts("</SELECT>");

puts("<SELECT NAME=table1 SIZE=1>");
printf("<OPTION VALUE=\"Choose table\">Non-positional tables</OPTION>\n");

currentListEl = nonposTableList;
while(currentListEl != 0)
	{
	if(existsAndEqual("table1", currentListEl->name))
		printf("<OPTION SELECTED>%s</OPTION>\n", currentListEl->name);
	else
		printf("<OPTION>%s</OPTION>\n", currentListEl->name);

	currentListEl = currentListEl->next;
	}

puts("</SELECT>");

puts("</TD></TR>");

puts("<TR><TD>");
puts("Choose an action: ");
puts("</TD><TD>");
puts("<INPUT TYPE=\"submit\" VALUE=\"Choose fields\" NAME=\"phase\">");
puts("<INPUT TYPE=\"submit\" VALUE=\"Get all fields\" NAME=\"phase\">");
puts("<INPUT TYPE=\"submit\" VALUE=\"Get DNA\" NAME=\"phase\">");
puts("</TD></TR>");
puts("</TABLE>");

puts("</FORM>");

hashElFreeList(&posTableList);

webNewSection("Getting started on the Table Browser");

puts(
	"This web tool allows convenient and precise access to the primary database tables containing the genome sequence and associated annotation tracks. By specifying chromosomal range and table type, the exact data set of interest can be viewed.  This tool thus makes it unnecessary to download and manipulate the genome itself and its massive data tracks (though that option is will always remain <A HREF=\"/\">available</A>.)" "\n"
	"<P>" "\n"
	"After each round of genome assembly, features such as mRNAs are located within the genome by alignment. This process generates <B>positional</B> stop-start coordinates and other descriptive data which are then stored within MySQL relational database tables. It is these tables that drive the graphical tracks in the Genome Browser, meaning that the two views of the data are always in agreement. Chromosomal coordinates usually change with each  build because of newly filled gaps or assembly procedure refinements." "\n"
	"<P>" "\n"
	"Other data is inherently <B>non-positional</B> (not tied to genome coordinates), such as submitting author of a GenBank EST, tissue of origin , or link to a RefSeq.  These data types are accessed separately." "\n"
	"<P>" "\n"
	"Familiarize yourself with the table browser tool by a <B>few minutes of exploration</B>. Starting at the top of the form, use the default position (or enter a cytological band, eg 20p12, or type in a keyword from a GenBank entry to look up). Next select a positional table from the menu, for example, chrN_mrna.  Choosing an action will then display -- relative to the chromosomal range in the text box -- a option to view and refine displayed data fields, all the mRNA coordinate data, or the genomic DNA sequences of the RNAs.  If the chromosomal range is too narrow, it may happen that no mRNA occurs there (meaning no data will be returned)." "\n"
	"<P>" "\n"
	"Notice that if you look up a gene or accession number, the tool typically returns a choice of several mRNAs.  Selecting one of these returns the table browser to its starting place but with the chromosomal location automaticallyentered in the text box." "\n"
	"<P>" "\n"
	/*"The non-positional tables display all entries associated with the selected assembly -- there is no way to restrict them to chromosomes.  However there is a provision in the text box to add a single word restriction.  Thus, to see which species of macaque monkeys have contributed sequence data, selected the organism table, view all fields, and restrict the text box to the genus Macaca." "\n"
	"<P>" "\n"*/
	"For either type of table, a user not familiar with what the table offers should select \"Choose fields\".   Some tables offer many columns of data of which only a few might be relevent. The fewer fields selected, the simpler the next stage of data return.  If all the fields are wanted, shortcut this step by selecting \"Get all fields\".  If a chromosome and range are in the text box, \"Get DNA\" will retrieve genomic sequences in fasta format without any table attributes other than chromosome, freeze date, and start-stop genomic coordinates." "\n"
);
}

void parseTableName(char* dest, char* chrom_name)
/* given a chrom name (such as one produced by findPositionInGenome) return the
 * table name of the table selected by the user */
{
char* table;
char selectChro[64];
char chro[64];
char post[64];

table = getTableVar();

sscanf(chrom_name, "chr%64s", selectChro);

if(sscanf(table, "chrN_%64s", post) == 1)
	{
	snprintf(dest, 256, "chr%s_%s", selectChro, post);
	}
else
	snprintf(dest, 256, "%s", table);
}


void getAllFields()
/* downloads and all the fields of the selected table */
{
char table[256];
char query[256];
int i;
int numberColumns;
struct sqlConnection *conn;
struct sqlResult *sr;
char **row;
char* field;

char chromFieldName[32];	/* the name of the chrom field */
char startName[32];	/* the start and end names of the positional fields */
char endName[32];	/* in the table */

char *choosenChromName;        /* Name of chromosome sequence . */
int winStart;           /* Start of window in sequence. */
int winEnd;         /* End of window in sequence. */
char* position;

/* if the position information is not given, get it */
position = cgiOptionalString("position");
if(position == NULL)
	position = "";
if(position[0] == '\0')
    webAbort("Missing position", "Please enter a position");
if(strcmp(position, "genome"))
	{
	if(position != NULL && position[0] != 0)
		{
		if (!findPositionInGenome(position, &choosenChromName, &winStart, &winEnd))
			return;
		}
	findPositionInGenome(position, &choosenChromName, &winStart, &winEnd);
	}

/* if they haven't choosen a table tell them */
if(existsAndEqual("table", "Choose table"))
	{
	webAbort("Missing table selection", "Please choose a table");
	}

/* get the real name of the table */
parseTableName(table, choosenChromName);

if(!hTableExists(table))
	webAbort("No data", "%s There is no information in table %s for chromosome %s.", table, getTableVar(), choosenChromName);

/* make sure that the table name doesn't have anything "weird" in it */
if(!allLetters(table))
	webAbort("Error", "Malformated table name.");
	
/* get the name of the start and end fields */
if(hFindChromStartEndFields(table, chromFieldName, startName, endName))
	{
	/* build the rest of the query */
        if(sameString(chromFieldName, ""))
            snprintf(query, 256, "SELECT * FROM %s WHERE %s >= %d AND %s <= %d",
                    table, startName, winStart, endName, winEnd);
        else
            snprintf(query, 256, "SELECT * FROM %s WHERE %s = \"%s\" AND %s >= %d AND %s <= %d",
                    table, chromFieldName, choosenChromName, startName, winStart, endName, winEnd);
	}
else
	snprintf(query, 256, "SELECT * FROM %s ", table);

conn = hAllocConn();
//puts(query);
sr = sqlGetResult(conn, query);
numberColumns = sqlCountColumns(sr);

row = sqlNextRow(sr);
if(row == 0)
    webAbort("Empty Result", "Your query produced no results.");


printf("Content-Type: text/plain\n\n");
webStartText();

/* print the columns names */
printf("#");
while((field = sqlFieldName(sr)) != 0)
	    printf("%s\t", field);

printf("\n");

/* print the data */
do
	{
	for(i = 0; i < numberColumns; i++)
		{
			printf("%s\t", row[i]);
		}
	printf("\n");
	}
while((row = sqlNextRow(sr)) != NULL);
}


void getChoosenFields()
/* output a form that allows the user to choose what fields they want to download */
{
struct cgiVar* current = cgiVarList();
char table[256];
char post[64];
char query[256];
int i;
int numberColumns;
struct sqlConnection *conn;
struct sqlResult *sr;
char **row;
char* field;
char* database;
char* freezeName;

char startName[32];	/* the start and end names of the positional fields */
char endName[32];	/* in the table */

char *chromName;        /* Name of chromosome sequence . */
int winStart;           /* Start of window in sequence. */
int winEnd;         /* End of window in sequence. */
char* position;

boolean allGenome = FALSE;	/* this flag is true if we are fetching the whole genome
							   not just chrom by chrom */

position = cgiOptionalString("position");

/* if they haven't choosen a table tell them */
if(existsAndEqual("table0", "Choose table") && existsAndEqual("table1", "Choose table"))
	webAbort("Missing table selection", "Please choose a table.");

/* select the database */
database = cgiUsualString("db", hGetDb());
hSetDb(database);
hDefaultConnect();
conn = hAllocConn();

/* if the position information is not given, get it */
if(position == NULL)
	position = "";
if(position[0] == '\0')
    webAbort("Missing position", "Please enter a position");
if(strcmp(position, "genome"))
	{
	if(position != NULL && position[0] != 0)
		{
		if (!findPositionInGenome(position, &chromName, &winStart, &winEnd))
			return;
		}
	findPositionInGenome(position, &chromName, &winStart, &winEnd);
	}
else
	allGenome = TRUE;	/* read all chrom info */

/* print the form */
printf("<FORM ACTION=\"%s\">\n\n", hgTextName());

/* take a first stab */
//if(allGenome)
	parseTableName(table, "chr22");
//else
//	parseTableName(table, chromName);

if(!hTableExists(table))
	webAbort("No data", "%s There is no information in table %s for chromosome %s.", table, getTableVar(), chromName);

/* make sure that the table name doesn't have anything "weird" in it */
if(!allLetters(table))
	webAbort("Error", "Malformated table name.");

/* print the location and a jump button if the table is positional */
if(hFindChromStartEndFields(table, query, query, query))
	{
	fputs("position ", stdout);
	cgiMakeTextVar("position", position, 30);
	cgiMakeButton("submit", "Look up");
	}
else
	{
	cgiMakeHiddenVar("position", position);
	}
cgiMakeHiddenVar("db", database);
cgiMakeHiddenVar("phase", "Choose fields");

/* print the name of the selected table */
printf("<H3>Track: %s</H3>\n", getTableVar());

snprintf(query, 256, "DESCRIBE %s", table);
//puts(query);
sr = sqlGetResult(conn, query);

printf("<INPUT TYPE=\"hidden\" NAME=\"table\" VALUE=\"%s\">\n", getTableVar());
puts("<TABLE><TR><TD>");
while((row = sqlNextRow(sr)) != NULL)
	{
	char name[126];
	snprintf(name, 126, "field_%s", row[0]);
	if(existsAndEqual(name, "on"))
		printf("<INPUT TYPE=\"checkbox\" NAME=\"%s\" CHECKED> %s<BR>\n", name, row[0]);
	else
		printf("<INPUT TYPE=\"checkbox\" NAME=\"%s\"> %s<BR>\n", name, row[0]);
	}

	
puts("<INPUT TYPE=\"submit\" NAME=\"phase\" VALUE=\"Get these fields\">");

puts("</TD></TR></TABLE>");
puts("</FORM>");
sqlDisconnect(&conn);

webEnd();
}


void getSomeFields()
/* get the prints the fields choosen by the user */
{
struct cgiVar* current = cgiVarList();
char table[1024];
char query[4096];
int i;
int numberColumns;
struct sqlConnection *conn;
struct sqlResult *sr;
char **row;
char* field;

char chromFieldName[32];	/* the name of the chrom field name */
char startName[32];	/* the start and end names of the positional fields */
char endName[32];	/* in the table */

char *choosenChromName;        /* Name of chromosome sequence . */
int winStart;           /* Start of window in sequence. */
int winEnd;         /* End of window in sequence. */
char* position;

/* if the position information is not given, get it */
position = cgiOptionalString("position");
if(position == NULL)
	position = "";
if(position[0] == '\0')
    webAbort("Missing position", "Please enter a position");
if(strcmp(position, "genome"))
	{
	if(position != NULL && position[0] != 0)
		{
		if (!findPositionInGenome(position, &choosenChromName, &winStart, &winEnd))
			return;
		}
	findPositionInGenome(position, &choosenChromName, &winStart, &winEnd);
	}

/* get the real name of the table */
parseTableName(table, choosenChromName);

/* make sure that the table name doesn't have anything "weird" in it */
if(!allLetters(table))
	webAbort("Error", "Malformated table name.");

strcpy(query, "SELECT");

/* the first field selected is special */
while(current != 0)
	{
	if(strstr(current->name, "field_") == current->name && strcmp(current->val, "on") == 0)
		{	
		/* make sure that the field names don't have anything "weird" in them */
		if(!allLetters(current->name + strlen("field_")))
			webAbort("Error", "Malformated field name.");

		sprintf(query, "%s %s", query, current->name + strlen("field_"));
		break; /* only process the first field this way */
		}

	current = current->next;
	}

/* if there are no fields sellected, say so */
if(current == 0)
	{
	webAbort("Missing fields selection", "No fields selected.");
	//printf("Content-Type: text/plain\n\n");
	//printf("\n\nNo fields selected.\n");
	return;
	}
else
	current = current->next;	/* skip past the first field */

/* process the rest of the selected fields */
while(current != 0)
	{
	if(strstr(current->name, "field_") == current->name && strcmp(current->val, "on") == 0)
		{	
		/* make sure that the field names don't have anything "weird" in them */
		if(!allLetters(current->name + strlen("field_")))
			webAbort("Error", "Malformated field name.");

		sprintf(query, "%s, %s", query, current->name + strlen("field_"));
		}

	current = current->next;
	}

sprintf(query, "%s FROM %s", query, table);

/* get the name of the start and end fields */
if(hFindChromStartEndFields(table, chromFieldName, startName, endName))
	{
	/* build the rest of the query */
        if(sameString(chromFieldName, ""))
            sprintf(query, "%s WHERE %s >= %d AND %s <= %d",
                            query, startName, winStart, endName, winEnd);
        else
            sprintf(query, "%s WHERE %s = \"%s\" AND %s >= %d AND %s <= %d",
                            query, chromFieldName, choosenChromName, startName, winStart, endName, winEnd);
	}

conn = hAllocConn();
sr = sqlGetResult(conn, query);
numberColumns = sqlCountColumns(sr);

row = sqlNextRow(sr);
if(row == 0)
    webAbort("empty result", "your query produced no results.");

printf("Content-Type: text/plain\n\n");
webStartText();
//puts(query);
/* print the field names */
printf("#");
while((field = sqlFieldName(sr)) != 0)
    printf("%s\t", field);

printf("\n");

/* print the data */
do
	{
	for(i = 0; i < numberColumns; i++)
		{
			printf("%s\t", row[i]);
		}
	printf("\n");
	}
while((row = sqlNextRow(sr)) != NULL);
}


void getGenomeWideNonSplit()
/* output the info. for a non-split table (i.e. not chrN_* tables) */
{
struct cgiVar* current = cgiVarList();
char query[256];
int i;
int numberColumns;
struct sqlConnection *conn;
struct sqlResult *sr;
char **row;
char* field;
char* database;

char* table = getTableVar();

/* select the database */
database = cgiUsualString("db", hGetDb());
hSetDb(database);
hDefaultConnect();
conn = hAllocConn();

strcpy(query, "SELECT");

/* the first field selected is special */
while(current != 0)
	{
	if(strstr(current->name, "field_") == current->name && strcmp(current->val, "on") == 0)
		{	
		/* make sure that the field names don't have anything "weird" in them */
		if(!allLetters(current->name + strlen("field_")))
			webAbort("Error", "Malformated field name.");

		sprintf(query, "%s %s", query, current->name + strlen("field_"));
		break; /* only process the first field this way */
		}

	current = current->next;
	}

/* if there are no fields sellected, say so */
if(current == 0)
	{
	webAbort("Missing field selection", "No fields selected.\n");
	//printf("Content-Type: text/plain\n\n");
	//printf("\n\nNo fields selected.\n");
	return;
	}
else
	current = current->next;	/* skip past the first field */

/* process the rest of the selected fields */
while(current != 0)
	{
	if(strstr(current->name, "field_") == current->name && strcmp(current->val, "on") == 0)
		{	
		/* make sure that the field names don't have anything "weird" in them */
		if(!allLetters(current->name + strlen("field_")))
			webAbort("Error", "Malformated field name.");

		sprintf(query, "%s, %s", query, current->name + strlen("field_"));
		}

	current = current->next;
	}

/* build the rest of the query */
sprintf(query, "%s FROM %s", query, table);

conn = hAllocConn();
//puts(query);
sr = sqlGetResult(conn, query);
numberColumns = sqlCountColumns(sr);

row = sqlNextRow(sr);
if(row == 0)
    webAbort("Empty result", "You query found no results.");

printf("Content-Type: text/plain\n\n");
webStartText();
//puts(query);
/* print the field names */
printf("#");
while((field = sqlFieldName(sr)) != 0)
		printf("%s\t", field);

printf("\n");

/* print the data */
do
	{
	for(i = 0; i < numberColumns; i++)
		{
			printf("%s\t", row[i]);
		}
	printf("\n");
	}
while((row = sqlNextRow(sr)) != NULL);
}



void outputTabData(char* query, char* tableName, struct sqlConnection *conn, boolean outputFields)
/* execute the given query and outputs the data in a tab seperated form */
{
int i;
int numberColumns;
struct sqlResult *sr;
char **row;
char* field;
char* database;

/* if there is no table, do nothing */
if(!sqlTableExists(conn, tableName))
	return;

sr = sqlGetResult(conn, query);
numberColumns = sqlCountColumns(sr);

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
	for(i = 0; i < numberColumns; i++)
		{
			printf("%s\t", row[i]);
		}
	printf("\n");
	}
}


void getGenomeWideSplit()
/* output the info. for a split table (i.e. chrN_* tables) */
{
struct cgiVar* current;
char fields[256] = "";	/* the fields part of the query */
char query[256];
int i;
int c;
int numberColumns;
struct sqlConnection *conn;
struct sqlResult *sr;
char **row;
char* field;
char* database;

char* table = getTableVar();
char parsedTableName[256];

/* select the database */
database = cgiUsualString("db", hGetDb());
hSetDb(database);
hDefaultConnect();
conn = hAllocConn();

/* the first field selected is special */
current = cgiVarList();
while(current != 0)
	{
	if(strstr(current->name, "field_") == current->name && strcmp(current->val, "on") == 0)
		{	
		/* make sure that the field names don't have anything "weird" in them */
		if(!allLetters(current->name + strlen("field_")))
			webAbort("Error", "Malformated field name.");

		sprintf(fields, "%s %s", fields, current->name + strlen("field_"));
		break; /* only process the first field this way */
		}

	current = current->next;
	}

/* if there are no fields sellected, say so */
if(current == 0)
	{
	webAbort("Missing field selection", "No fields selected.");
	//printf("Content-Type: text/plain\n\n");
	//printf("\n\nNo fields selected.\n");
	return;
	}
else
	current = current->next;	/* skip past the first field */

/* process the rest of the selected fields */
while(current != 0)
	{
	if(strstr(current->name, "field_") == current->name && strcmp(current->val, "on") == 0)
		{	
		/* make sure that the field names don't have anything "weird" in them */
		if(!allLetters(current->name + strlen("field_")))
			webAbort("Error", "Malformated field name.");

		sprintf(fields, "%s, %s", fields, current->name + strlen("field_"));
		}

	current = current->next;
	}

/* build the rest of the query */
table = strstr(table, "_");

printf("Content-Type: text/plain\n\n");
webStartText();

snprintf(parsedTableName, 256, "chr%d%s", 1, table);
snprintf(query, 256, "SELECT%s FROM %s", fields, parsedTableName);
outputTabData(query, parsedTableName, conn, TRUE);

snprintf(parsedTableName, 256, "chr%d_random%s", 1, table);
snprintf(query, 256, "SELECT%s FROM %s", fields, parsedTableName);
outputTabData(query, parsedTableName, conn, FALSE);

for(c  = 2; c <= 22; c++)
	{
	snprintf(parsedTableName, 256, "chr%d%s", c, table); 
	snprintf(query, 256, "SELECT%s FROM %s", fields, parsedTableName); 
	outputTabData(query, parsedTableName, conn, FALSE);
	snprintf(parsedTableName, 256, "chr%d_random%s", c, table); 
	snprintf(query, 256, "SELECT%s FROM %s", fields, parsedTableName); 
	outputTabData(query, parsedTableName, conn, FALSE);
	}

snprintf(parsedTableName, 256, "chr%s%s", "X", table);
snprintf(query, 256, "SELECT%s FROM %s", fields, parsedTableName);
outputTabData(query, parsedTableName, conn, FALSE);
snprintf(parsedTableName, 256, "chr%s_random%s", "X", table);
snprintf(query, 256, "SELECT%s FROM %s", fields, parsedTableName);
outputTabData(query, parsedTableName, conn, FALSE);

snprintf(parsedTableName, 256, "chr%s%s", "Y", table);
snprintf(query, 256, "SELECT%s FROM %s", fields, parsedTableName);
outputTabData(query, parsedTableName, conn, FALSE);
snprintf(parsedTableName, 256, "chr%s_random%s", "Y", table);
snprintf(query, 256, "SELECT%s FROM %s", fields, parsedTableName);
outputTabData(query, parsedTableName, conn, FALSE);

snprintf(parsedTableName, 256, "chr%s%s", "NA", table);
snprintf(query, 256, "SELECT%s FROM %s", fields, parsedTableName);
outputTabData(query, parsedTableName, conn, FALSE);
snprintf(parsedTableName, 256, "chr%s_random%s", "NA", table);
snprintf(query, 256, "SELECT%s FROM %s", fields, parsedTableName);
outputTabData(query, parsedTableName, conn, FALSE);

snprintf(parsedTableName, 256, "chr%s%s", "UL", table);
snprintf(query, 256, "SELECT%s FROM %s", fields, parsedTableName);
outputTabData(query, parsedTableName, conn, FALSE);
snprintf(parsedTableName, 256, "chr%s_random%s", "UL", table);
snprintf(query, 256, "SELECT%s FROM %s", fields, parsedTableName);
outputTabData(query, parsedTableName, conn, FALSE);
}

void getSomeFieldsGenome()
/* get the fields that the user choose for the genome-wide informations */
{
char* table = getTableVar();
	
if(strstr(table, "chrN_") == table)
	{
	/* have to search through a split table */
	getGenomeWideSplit();
	}
else
	{
	/* don't have t search for a split table */
	getGenomeWideNonSplit();
	}
}	
			

void getAllGenomeWideNonSplit()
{
struct cgiVar* current = cgiVarList();
char* table;
char query[256];
struct sqlConnection *conn;

table = getTableVar();

/* make sure that the table name doesn't have anything "weird" in it */
if(!allLetters(table))
	{
	webAbort("Error", "Malformated table name.");
	//printf("Content-Type: text/plain\n\n");
	//printf("Malformated table name.");
	return;
	}

/* get the name of the start and end fields */
snprintf(query, 256, "SELECT * FROM %s", table);

conn = hAllocConn();

printf("Content-Type: text/plain\n\n");
webStartText();
outputTabData(query, table, conn, TRUE);
}


void getAllGenomeWideSplit()
{
char query[256];
struct sqlConnection *conn;
char* database;
int c;

char* table = getTableVar();
char parsedTableName[256];

/* select the database */
database = cgiUsualString("db", hGetDb());
hSetDb(database);
hDefaultConnect();
conn = hAllocConn();

/* build the rest of the query */
table = strstr(table, "_");

printf("Content-Type: text/plain\n\n");
webStartText();

snprintf(parsedTableName, 256, "chr%d%s", 1, table);
snprintf(query, 256, "SELECT * FROM %s", parsedTableName);
outputTabData(query, parsedTableName, conn, TRUE);

snprintf(parsedTableName, 256, "chr%d_random%s", 1, table);
snprintf(query, 256, "SELECT * FROM %s", parsedTableName);
outputTabData(query, parsedTableName, conn, FALSE);

for(c  = 2; c <= 22; c++)
	{
	snprintf(parsedTableName, 256, "chr%d%s", c, table); 
	snprintf(query, 256, "SELECT * FROM %s", parsedTableName); 
	outputTabData(query, parsedTableName, conn, FALSE);
	snprintf(parsedTableName, 256, "chr%d_random%s", c, table); 
	snprintf(query, 256, "SELECT * FROM %s", parsedTableName); 
	outputTabData(query, parsedTableName, conn, FALSE);
	}

snprintf(parsedTableName, 256, "chr%s%s", "X", table);
snprintf(query, 256, "SELECT * FROM %s", parsedTableName);
outputTabData(query, parsedTableName, conn, FALSE);
snprintf(parsedTableName, 256, "chr%s_random%s", "X", table);
snprintf(query, 256, "SELECT * FROM %s", parsedTableName);
outputTabData(query, parsedTableName, conn, FALSE);

snprintf(parsedTableName, 256, "chr%s%s", "Y", table);
snprintf(query, 256, "SELECT * FROM %s", parsedTableName);
outputTabData(query, parsedTableName, conn, FALSE);
snprintf(parsedTableName, 256, "chr%s_random%s", "Y", table);
snprintf(query, 256, "SELECT * FROM %s", parsedTableName);
outputTabData(query, parsedTableName, conn, FALSE);

snprintf(parsedTableName, 256, "chr%s%s", "NA", table);
snprintf(query, 256, "SELECT * FROM %s", parsedTableName);
outputTabData(query, parsedTableName, conn, FALSE);
snprintf(parsedTableName, 256, "chr%s_random%s", "NA", table);
snprintf(query, 256, "SELECT * FROM %s", parsedTableName);
outputTabData(query, parsedTableName, conn, FALSE);

snprintf(parsedTableName, 256, "chr%s%s", "UL", table);
snprintf(query, 256, "SELECT * FROM %s", parsedTableName);
outputTabData(query, parsedTableName, conn, FALSE);
snprintf(parsedTableName, 256, "chr%s_random%s", "UL", table);
snprintf(query, 256, "SELECT * FROM %s", parsedTableName);
outputTabData(query, parsedTableName, conn, FALSE);
}

void getAllFieldsGenome()
{
char* table = getTableVar();
	
/* if they haven't choosen a table tell them */
if(existsAndEqual("table", "Choose table"))
	{
	webAbort("Missing table selection", "Please choose a table.");
	exit(1);
	}

if(strstr(table, "chrN_") == table)
	{
	/* have to search through a split table */
	getAllGenomeWideSplit();
	}
else
	{
	/* don't have t search for a split table */
	getAllGenomeWideNonSplit();
	}
}


char *makeFaStartLine(char *chrom, char *table, int start, int size)
/* Create fa start line. */
{
char faName[128];
static char faStartLine[512];

++blockIx;
if (startsWith("chr", table))
    sprintf(faName, "%s_%d", table, blockIx);
else
    sprintf(faName, "%s_%s_%d", chrom, table, blockIx);
sprintf(faStartLine, "%s %s %s %d %d", 
	faName, database, chrom, start, start+size);
return faStartLine;
}

void writeOut(FILE *f, char *chrom, int start, int size, char strand, DNA *dna, char *faHeader)
/* Write output to file */
{
if (sameWord(outputType, "pos"))
    fprintf(f, "%s\t%d\t%d\t%c\n", chrom, start, start+size, strand);
else if (sameWord(outputType, "fasta"))
    faWriteNext(f, faHeader, dna, size);
else
    webAbort("Error", "Unknown output type %s\n", outputType);
}


void outputDna(FILE *f, char *chrom, char *table, int start, int size, char *dna, 
	char *nibFileName, FILE *nibFile, int nibSize, char strand)
/* Output DNA either directly from nib or by upper-casing DNA. */
{
if (merge >= 0)
    toUpperN(dna + start, size);
else
    {
    struct dnaSeq *seq = nibLdPart(nibFileName, nibFile, nibSize, start, size);
    if (strand == '-')
        reverseComplement(seq->dna, size);
    writeOut(f, chrom, start, size, strand, seq->dna, makeFaStartLine(chrom, table, start, size));
    freeDnaSeq(&seq);
    }
}


void chromFeatDna(char *table, char *chrom, FILE *f)
/* chromFeatDna - Get dna for a type of feature on one chromosome. */
{
/* Get chromosome in lower case case.  If merging set bits that 
 * are part of feature of interest to upper case, and then scan 
 * for blocks of lower upper case and output them. If not merging
 * then just output dna for features directly. */
struct dyString *query = newDyString(512);
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char chromField[32], startField[32], endField[32];
struct dnaSeq *seq = NULL;
DNA *dna = NULL;
int s,e,sz,i,size;
boolean lastIsUpper = FALSE, isUpper = FALSE;
int last;
char nibFileName[512];
FILE *nibFile = NULL;
int nibSize;

s = size = 0;

if (!hFindChromStartEndFields(table, chromField, startField, endField))
    webAbort("Missing field in table", "Couldn't find chrom/start/end fields in table");

if (merge >= 0)
    {
    seq = hLoadChrom(chrom);
    dna = seq->dna;
    size = seq->size;
    }
else
    {
    hNibForChrom(chrom, nibFileName);
    nibOpenVerify(nibFileName, &nibFile, &nibSize);
    }

if (breakUp)
    {
    if (sameString(startField, "tStart"))
	{
	dyStringPrintf(query, "select * from %s where tStart >= %d and tEnd < %d",
	    table, chromStart, chromEnd);
	dyStringPrintf(query, " and %s = '%s'", chromField, chrom);
	if (where != NULL)
	    dyStringPrintf(query, " and %s", where);
	sr = sqlGetResult(conn, query->string);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
		char trash[32];
		boolean isBinned;
		struct psl *psl;
		
		hFindFieldsAndBin(table, trash, trash, trash, &isBinned);
		
	    psl = pslLoad(row + isBinned);
	    if (psl->strand[1] == '-')	/* Minus strand on target */
		{
		int tSize = psl->tSize;
		for (i=0; i<psl->blockCount; ++i)
		     {
		     sz = psl->blockSizes[i];
		     s = tSize - (psl->tStarts[i] + sz);
		     outputDna(f, chrom, table, s, sz, dna, nibFileName, nibFile, nibSize, '-');
		     }
		}
	    else
		{
		for (i=0; i<psl->blockCount; ++i)
		     outputDna(f, chrom, table, psl->tStarts[i], psl->blockSizes[i], 
			    dna, nibFileName, nibFile, nibSize, '+');
		}
	    pslFree(&psl);
	    }
	}
    else if (sameString(startField, "txStart"))
        {
	dyStringPrintf(query, "select * from %s where txStart >= %d and txEnd < %d",
	    table, chromStart, chromEnd);
	dyStringPrintf(query, " and %s = '%s'", chromField, chrom);
	if (where != NULL)
	    dyStringPrintf(query, " and %s", where);
	sr = sqlGetResult(conn, query->string);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    struct genePred *gp = genePredLoad(row);
	    for (i=0; i<gp->exonCount; ++i)
		 {
		 s = gp->exonStarts[i];
		 sz = gp->exonEnds[i] - s;
		 outputDna(f, chrom, table, 
		    s, sz, dna, nibFileName, nibFile, nibSize, gp->strand[0]);
		 }
	    genePredFree(&gp);
	    }
	}
    else
        {
        webAbort("Error", "Can only use breakUp parameter with psl or genePred formatted tables");
	}
    }
else
    {
    dyStringPrintf(query, "select %s,%s from %s where %s >= %d and %s < %d", 
	    startField, endField, table,
	    startField, chromStart, endField, chromEnd);
    dyStringPrintf(query, " and %s = '%s'", chromField, chrom);
    if (where != NULL)
	dyStringPrintf(query, " and %s", where);
    sr = sqlGetResult(conn, query->string);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	s = sqlUnsigned(row[0]);
	e = sqlUnsigned(row[1]);
	sz = e - s;
	if (seq != NULL && (sz < 0 || e >= size))
	    webAbort("Out of range", "Coordinates out of range %d %d (%s size is %d)", s, e, chrom, size);
	outputDna(f, chrom, table, s, sz, dna, nibFileName, nibFile, nibSize, '+');
	}
    }

/* Merge nearby upper case blocks. */
if (merge > 0)
    {
    e = -merge-10;		/* Don't merge initial block. */
    for (i=0; i<=size; ++i)
	{
	isUpper = isupper(dna[i]);
	if (isUpper && !lastIsUpper)
	    {
	    s = i;
	    if (s - e <= merge)
	        {
		toUpperN(dna+e, s-e);
		}
	    }
	else if (!isUpper && lastIsUpper)
	    {
	    e = i;
	    }
	lastIsUpper = isUpper;
	}
    }

if (merge >= 0)
    {
    for (i=0; i<=size; ++i)
	{
	isUpper = isupper(dna[i]);
	if (isUpper && !lastIsUpper)
	    s = i;
	else if (!isUpper && lastIsUpper)
	    {
	    e = i;
	    sz = e - s;
	    writeOut(f, chrom, s, sz, '+', dna+s, makeFaStartLine(chrom, table, s, sz));
	    }
	lastIsUpper = isUpper;
	}
    }

hFreeConn(&conn);
freeDnaSeq(&seq);
carefulClose(&nibFile);
}


void getFeatDna(char *table, char *chrom, char *outName)
/* getFeatDna - Get dna for a type of feature an all relevant chromosomes. */
{
char chrTableBuf[256];
struct slName *chromList = NULL, *chromEl;
boolean chromSpecificTable = !hTableExists(table);
char *chrTable = (chromSpecificTable ? chrTableBuf : table);
FILE *f = mustOpen(outName, "w");
boolean toStdout = (sameString(outName, "stdout"));

if (sameWord(chrom, "all"))
    chromList = hAllChromNames();
else
    chromList = newSlName(chrom);

for (chromEl = chromList; chromEl != NULL; chromEl = chromEl->next)
    {
    chrom = chromEl->name;
    if (chromSpecificTable)
        {
	sprintf(chrTable, "%s_%s", chrom, table);
	if (!hTableExists(table))
	    webAbort("Cannot find table", "table %s (and %s) don't exist in %s", table, 
	         chrTable, database);
	}
    if (!toStdout)
        printf("Processing %s %s\n", chrTable, chrom);
    chromFeatDna(chrTable, chrom, f);
    }
carefulClose(&f);
if (!toStdout)
    printf("%d features extracted to %s\n", blockIx, outName);
}


void getDNA()
/* get the DNA for the given position */
{
char* table = getTableVar();
char* position = cgiString("position");
char *choosenChromName;		/* Name of chromosome sequence . */
int winStart;				/* Start of window in sequence. */
int winEnd;					/* End of window in sequence. */
char parsedTableName[256];
int c;

/* if they haven't choosen a positional table, tell them */
if(existsAndEqual("table0", "Choose table"))
    webAbort("Missing table selection", "The Get DNA function requires that you select a positional table.");

/* if they haven't choosen a table, tell them */
if(existsAndEqual("table", "Choose table"))
	{
	webAbort("Missing table selection", "Please choose a table.");
	}
	
/* select the database */
database = cgiUsualString("db", hGetDb());

/* if the position information is not given, get it */
if(position == NULL)
	position = "";
if(position[0] == '\0')
    webAbort("Missing position", "Please enter a position");
if(strcmp(position, "genome"))
	{
	if(position != NULL && position[0] != 0)
		{
		if (!findPositionInGenome(position, &choosenChromName, &winStart, &winEnd))
			return;
		}
	findPositionInGenome(position, &choosenChromName, &winStart, &winEnd);
	}

/* make sure that the table name doesn't have anything "weird" in it */
if(!allLetters(table))
	webAbort("Error", "Malformated table name.");

printf("Content-Type: text/plain\n\n");
webStartText();

if(existsAndEqual("position", "genome"))
	{
	char post[64];

	chromStart = 0;		/* Start of range to select from. */
	chromEnd = BIGNUM;          /* End of range. */
		
	if(sscanf(table, "chrN_%64s", post) == 1)
		{
		for(c  = 1; c <= 22; c++)
			{
			snprintf(parsedTableName, 256, "chr%d_%s", c, post); 
			getFeatDna(parsedTableName, "all", "stdout");
			snprintf(parsedTableName, 256, "chr%d_random_%s", c, post); 
			getFeatDna(parsedTableName, "all", "stdout");
			}

		snprintf(parsedTableName, 256, "chr_%s_%s", "X", post);
		getFeatDna(parsedTableName, "all", "stdout");
		snprintf(parsedTableName, 256, "chr%s_random_%s", "X", post);
		getFeatDna(parsedTableName, "all", "stdout");

		snprintf(parsedTableName, 256, "chr%s_%s", "Y", post);
		getFeatDna(parsedTableName, "all", "stdout");
		snprintf(parsedTableName, 256, "chr%s_random_%s", "Y", post);
		getFeatDna(parsedTableName, "all", "stdout");

		snprintf(parsedTableName, 256, "chr%s_%s", "NA", post);
		getFeatDna(parsedTableName, "all", "stdout");
		snprintf(parsedTableName, 256, "chr%s_random_%s", "NA", post);
		getFeatDna(parsedTableName, "all", "stdout");

		snprintf(parsedTableName, 256, "chr%s_%s", "UL", post);
		getFeatDna(parsedTableName, "all", "stdout");
		snprintf(parsedTableName, 256, "chr%s_random_%s", "UL", post);
		getFeatDna(parsedTableName, "all", "stdout");
		}
	else
		getFeatDna(table, "all", "stdout");
	}
else
	{
	/* get the real name of the table */
	parseTableName(parsedTableName, choosenChromName);

	chromStart = winStart;
	chromEnd = winEnd;

	getFeatDna(parsedTableName, choosenChromName, "stdout");
	}
}


/* the main body of the program */
void execute()
{
char* table = getTableVar("table");

char* database;
char* freezeName;

/* select the database */
database = cgiUsualString("db", hGetDb());
hSetDb(database);

/* output the freexe name and informaion */
freezeName = hFreezeFromDb(database);
if(freezeName == NULL)
    freezeName = "Unknown";

hDefaultConnect();	/* read in the default connection options */

/*if(cgiOptionalString("position") == 0)
	webAbort("No position given", "Please choose a position");*/

/* if there is no table chosen, ask for one. */
if(table == NULL || existsAndEqual("phase", "table"))
	{
	if(existsAndEqual("table0", "Choose table") && existsAndEqual("table1", "Choose table") && !existsAndEqual("submit", "Look up"))
	    webAbort("Missing table selection", "Please choose a table and try again.");
	else
	    {
		webStart("Genome Table Browser on %s Freeze", freezeName);
		getTable();
		webEnd();
	    }
	}
else
	{
	if(table != 0 && existsAndEqual("phase", "Choose fields"))
		{
		webStart("Genome Table Browser on %s Freeze: Select Fields", freezeName);
		getChoosenFields();
		webEnd();
		}
	else if(table != 0 && existsAndEqual("phase", "Get all fields"))
		{	
		if(existsAndEqual("position", "genome"))
			getAllFieldsGenome();
		else
			getAllFields();
		}
	else if(table != 0 && existsAndEqual("phase", "Get these fields"))
		{
		if(existsAndEqual("position", "genome"))
			getSomeFieldsGenome();
		else
			getSomeFields();
		}
	else if(existsAndEqual("phase", "Get DNA"))
		getDNA();
	}
}


int main(int argc, char *argv[])
/* Process command line. */
{
char* database;

if (!cgiIsOnWeb())
   {
   warn("This is a CGI script - attempting to fake environment from command line");
   warn("Reading configuration options from the user's home directory.");
   cgiSpoof(&argc, argv);
   }

/* select the database */
database = cgiUsualString("db", hGetDb());
hSetDb(database);
hDefaultConnect();

execute(); /* call the main function */

return 0;
}
