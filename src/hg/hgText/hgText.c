#include "common.h"
#include "hCommon.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "jksql.h"
#include "knownInfo.h"
#include "hdb.h"
#include "hgConfig.h"
#include "hgFind.h"
#include "hash.h"
#include "hdb.h"
#include "fa.h"

static boolean existsAndEqual(char* var, char* value)
/* returns true is the given CGI var exists and equals value */
{
	if(cgiOptionalString(var) != 0 && sameString(cgiOptionalString(var), value))
		return TRUE;
	else
		return FALSE;
}

static boolean fitFields(struct hash *hash, char *chrom, char *start, char *end,
        char retChrom[32], char retStart[32], char retEnd[32])
/* Return TRUE if chrom/start/end are in hash.
 * If so copy them to retChrom, retStart, retEnd, if they are all not null
 * Helper routine for findChromStartEndFields below. */
{
if (hashLookup(hash, chrom) && hashLookup(hash, start) && hashLookup(hash, end))
    {
	if(retChrom != 0 && retStart != 0 && retEnd != 0)
		{
    	strcpy(retChrom, chrom);
    	strcpy(retStart, start);
    	strcpy(retEnd, end);
	}
    return TRUE;
    }
else
    return FALSE;
}

static boolean hFindChromStartEndFields(char *table,
        char retChrom[32], char retStart[32], char retEnd[32])
/* Given a table return the fields for selecting chromosome, start, and end.
 *  If no such fields returns FALSE. */
{
char query[256];
struct sqlResult *sr;
char **row;
struct hash *hash = newHash(6);
struct sqlConnection *conn = hAllocConn();
boolean isPos = TRUE;

/* Read table description into hash. */
sprintf(query, "describe %s", table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    hashAdd(hash, row[0], NULL);
sqlFreeResult(&sr);

/* Look for bed-style names. */
if (fitFields(hash, "chrom", "chromStart", "chromEnd", retChrom, retStart,
retEnd))
    ;
/* Look for psl-style names. */
else if (fitFields(hash, "tName", "tStart", "tEnd", retChrom, retStart, retEnd))
    ;
/* Look for gene prediction names. */
else if (fitFields(hash, "chrom", "txStart", "txEnd", retChrom, retStart,
retEnd))
    ;
/* Look for repeatMasker names. */
else if (fitFields(hash, "genoName", "genoStart", "genoEnd", retChrom, retStart,
retEnd))
    ;
else
    isPos = FALSE;
freeHash(&hash);
hFreeConn(&conn);
return isPos;
}

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
    errAbort("Sorry, couldn't locate %s in genome database\n", spec);
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
    hgPositionsHtml(hgp, stdout, FALSE);
    hgPositionsFree(&hgp);
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
/* this function iterates over a hash table and prints out the name in a input form */
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
/* get the a talbe selection from the user */
{
char* table;
char* database;
struct cgiVar* current;

char *chromName;        /* Name of chromosome sequence . */
int winStart;           /* Start of window in sequence. */
int winEnd;         /* End of window in sequence. */

char* position;
char* freezeName;

struct sqlConnection *conn;
struct sqlResult *sr;
char **row;
char query[256];

struct hash *tableHash = newHash(7);
struct hashEl *tableList;
struct hashEl *currentListEl;

current = cgiVarList();
position = cgiOptionalString("position");

/* select the database */
database = cgiOptionalString("db");
if (database == NULL)
    database = "hg6";
hSetDb(database);
hDefaultConnect();
conn = hAllocConn();

/* if the position information is not given, get it */
if(position == NULL)
	position = "";
if(strcmp(position, "genome"))
	{
	if(position != NULL && position[0] != 0)
		{
		if (!findGenomePos(position, &chromName, &winStart, &winEnd))
			return;
		}
	findGenomePos(position, &chromName, &winStart, &winEnd);
	}

/* print the form */
puts("<CENTER>");
printf("<FORM ACTION=\"%s\">\n\n", hgTextName());

/* output the freexe name and informaion */
freezeName = hFreezeFromDb(database);
if(freezeName == NULL)
	freezeName = "Unknown";
printf("<H2>UCSC Genome Text Browser on %s Freeze</H2>\n",freezeName);

/* print the location and a jump button */
fputs("position ", stdout);
cgiMakeTextVar("position", position, 30);
cgiMakeButton("submit", " jump ");
cgiMakeHiddenVar("db", database);
cgiMakeHiddenVar("phase", "table");

puts("<P>");

/* iterate through all the tables and store the positional ones in a list */
strcpy(query, "SHOW TABLES");
sr = sqlGetResult(conn, query);
while((row = sqlNextRow(sr)) != NULL)
	{
	/* if they are positional, print them */
	if(hFindChromStartEndFields(row[0], 0, 0, 0))
		{
		char chrom[32];
		char post[32];
		char key[32];

		/* if table name is of the form, chr*_random_* or chr*_* */
		if(sscanf(row[0], "chr%[^_]_random_%s", chrom, post) == 2 ||
			sscanf(row[0], "chr%[^_]_%s", chrom, post) == 2)
			{
			/* parse the name into chrN_* */
			sprintf(key, "chrN_%s", post);
			/* and insert it into the hash table if it is not already there */
			hashStoreName(tableHash, key);
			}
		else
			hashStoreName(tableHash, row[0]);
		}
	}
sqlFreeResult(&sr);
sqlDisconnect(&conn);

/* get the list of tables */
puts("<SELECT NAME=table SIZE=1>");
puts("<OPTION>Choose table</OPTION>");

tableList = hashElListHash(tableHash);
slSort(&tableList, compareTable);

currentListEl = tableList;
while(currentListEl != 0)
	{
	printf("<OPTION>%s\n", currentListEl->name);

	currentListEl = currentListEl->next;
	}
puts("</SELECT>");

puts("<INPUT TYPE=\"submit\" VALUE=\"Choose fields\" NAME=\"phase\">");
puts("<INPUT TYPE=\"submit\" VALUE=\"Get all fields\" NAME=\"phase\">");
puts("<INPUT TYPE=\"submit\" VALUE=\"Get DNA\" NAME=\"phase\">");

puts("</FORM>");

/* some debuging information */
/*pnts-uts("<TABLE>");
while(current != 0)
	{
	puts("\t<TR>");
	printf("\t\t<TD>%s</TD><TD>%s</TD>\n", current->name, current->val);
	puts("\t</TR>");

	current = current->next;
	}
printf("%s from %d to %d\n", chromName, winStart, winEnd);
puts("</TABLE>");*/

hashElFreeList(&tableList);

puts("</CENTER>");
}

void parseTableName(char* dest, char* chrom_name)
/* given a chrom name (such as one produced by findGenomePos) return the
 * table name of the table selected by the user */
{
char* table = cgiString("table");
char selectChro[64];
char chro[64];
char post[64];

sscanf(chrom_name, "chr%s", selectChro);

if(sscanf(table, "chrN_%s", post) == 1)
	{
	sprintf(dest, "chr%s_%s", selectChro, post);
	}
else
	strcpy(dest, table);
}


void getAllFields()
/* downloads and all the fields of the selected table */
{
struct cgiVar* current = cgiVarList();
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
if(strcmp(position, "genome"))
	{
	if(position != NULL && position[0] != 0)
		{
		if (!findGenomePos(position, &choosenChromName, &winStart, &winEnd))
			return;
		}
	findGenomePos(position, &choosenChromName, &winStart, &winEnd);
	}

/* if they haven't choosen a table tell them */
if(existsAndEqual("table", "Choose table"))
	{
	printf("Content-type: text/plain\n\n");
	printf("\n\nPlease choose a table.");
	}

/* get the real name of the table */
parseTableName(table, choosenChromName);

/* make sure that the table name doesn't have anything "weird" in it */
if(!allLetters(table))
	errAbort("Malformated table name.");
	
/* get the name of the start and end fields */
hFindChromStartEndFields(table, chromFieldName, startName, endName);
sprintf(query, "SELECT * FROM %s WHERE %s = \"%s\" AND %s >= %d AND %s <= %d",
		table, chromFieldName, choosenChromName, startName, winStart, endName, winEnd);

conn = hAllocConn();
sr = sqlGetResult(conn, query);
numberColumns = sqlCountColumns(sr);

printf("Content-Type: text/plain\n\n");

/* print the columns names */
printf("#");
while((field = sqlFieldName(sr)) != 0)
	    printf("%s\t", field);

printf("\n");

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

void getChoosenFields()
/* output a form that allows the user to choose what fields they want to download */
{
struct cgiVar* current = cgiVarList();
char table[256];
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

/* select the database */
database = cgiOptionalString("db");
if (database == NULL)
    database = "hg6";
hSetDb(database);
hDefaultConnect();
conn = hAllocConn();

/* if the position information is not given, get it */
if(position == NULL)
	position = "";
if(strcmp(position, "genome"))
	{
	if(position != NULL && position[0] != 0)
		{
		if (!findGenomePos(position, &chromName, &winStart, &winEnd))
			return;
		}
	findGenomePos(position, &chromName, &winStart, &winEnd);
	}
else
	allGenome = TRUE;	/* read all chrom info */

/* print the form */
puts("<CENTER>");
printf("<FORM ACTION=\"%s\">\n\n", hgTextName());

/* output the freexe name and informaion */
freezeName = hFreezeFromDb(database);
if(freezeName == NULL)
	freezeName = "Unknown";
printf("<H2>UCSC Genome Text Browser on %s Freeze</H2>\n",freezeName);

/* if they haven't choosen a table tell them */
if(existsAndEqual("table", "Choose table"))
	errAbort("Please choose a table.");

if(!allGenome)
	{
	/* get the real name of the table */
	parseTableName(table, chromName);
	}
else	/* if all the genome */
	{
	strcpy(table, cgiString("table"));
	/* take table of the form chrN_* to chr1_* and uses chr1_* as prototypical table */
	if(table[0] == 'c' && table[1] == 'h' && table[2] == 'r' && table[3] == 'N' && table[4] == '_')
		table[3] = '1';
	}

puts(table);

/* make sure that the table name doesn't have anything "weird" in it */
if(!allLetters(table))
	errAbort("Malformated table name.");

/* print the location and a jump button */
fputs("position ", stdout);
cgiMakeTextVar("position", position, 30);
cgiMakeButton("submit", "jump");
cgiMakeHiddenVar("db", database);
cgiMakeHiddenVar("phase", "Choose fields");

/* print the name of the selected table */
printf("<H3>Track: %s</H3>\n", cgiString("table"));

sprintf(query, "DESCRIBE %s", table);
sr = sqlGetResult(conn, query);

printf("<INPUT TYPE=\"hidden\" NAME=\"table\" VALUE=\"%s\">\n", cgiString("table"));
puts("<TABLE><TR><TD>");
while((row = sqlNextRow(sr)) != NULL)
	printf("<INPUT TYPE=\"checkbox\" NAME=\"field_%s\"> %s<BR>\n", row[0], row[0]);

puts("<INPUT TYPE=\"submit\" NAME=\"phase\" VALUE=\"Get these fields\">");

puts("</TD></TR></TABLE>");
puts("</FORM>");
puts("</CENTER>");
sqlDisconnect(&conn);
}

void getSomeFields()
/* get the prints the fields choosen by the user */
{
struct cgiVar* current = cgiVarList();
char table[1024];
char query[256];
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
if(strcmp(position, "genome"))
	{
	if(position != NULL && position[0] != 0)
		{
		if (!findGenomePos(position, &choosenChromName, &winStart, &winEnd))
			return;
		}
	findGenomePos(position, &choosenChromName, &winStart, &winEnd);
	}

/* get the real name of the table */
parseTableName(table, choosenChromName);

/* make sure that the table name doesn't have anything "weird" in it */
if(!allLetters(table))
	errAbort("Malformated table name.");

/* get the name of the start and end fields */
hFindChromStartEndFields(table, chromFieldName, startName, endName);

strcpy(query, "SELECT");

/* build the SQL command */

/* the first field selected is special */
while(current != 0)
	{
	if(strstr(current->name, "field_") == current->name && strcmp(current->val, "on") == 0)
		{	
		/* make sure that the field names don't have anything "weird" in them */
		if(!allLetters(current->name + strlen("field_")))
			errAbort("Malformated field name.");

		sprintf(query, "%s %s", query, current->name + strlen("field_"));
		break; /* only process the first field this way */
		}

	current = current->next;
	}

/* if there are no fields sellected, say so */
if(current == 0)
	{
	printf("Content-Type: text/plain\n\n");
	printf("\n\nNo fields selected.\n");
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
			errAbort("Malformated field name.");

		sprintf(query, "%s, %s", query, current->name + strlen("field_"));
		}

	current = current->next;
	}

/* build the rest of the query */
sprintf(query, "%s FROM %s WHERE %s = \"%s\" AND %s >= %d AND %s <= %d", query, table, chromFieldName, choosenChromName, startName, winStart, endName, winEnd);

conn = hAllocConn();
sr = sqlGetResult(conn, query);
numberColumns = sqlCountColumns(sr);

printf("Content-Type: text/plain\n\n");
/*puts(query);*/
/* print the field names */
printf("#");
while((field = sqlFieldName(sr)) != 0)
	    printf("%s\t", field);

printf("\n");

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

char* table = cgiString("table");

/* select the database */
database = cgiOptionalString("db");
if (database == NULL)
    database = "hg6";
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
			errAbort("Malformated field name.");

		sprintf(query, "%s %s", query, current->name + strlen("field_"));
		break; /* only process the first field this way */
		}

	current = current->next;
	}

/* if there are no fields sellected, say so */
if(current == 0)
	{
	printf("Content-Type: text/plain\n\n");
	printf("\n\nNo fields selected.\n");
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
			errAbort("Malformated field name.");

		sprintf(query, "%s, %s", query, current->name + strlen("field_"));
		}

	current = current->next;
	}

/* build the rest of the query */
sprintf(query, "%s FROM %s", query, table);

conn = hAllocConn();
sr = sqlGetResult(conn, query);
numberColumns = sqlCountColumns(sr);

printf("Content-Type: text/plain\n\n");
puts(query);
/* print the field names */
printf("#");
while((field = sqlFieldName(sr)) != 0)
		printf("%s\t", field);

printf("\n");

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

char* table = cgiString("table");
char parsedTableName[256];

/* select the database */
database = cgiOptionalString("db");
if (database == NULL)
    database = "hg6";
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
			errAbort("Malformated field name.");

		sprintf(fields, "%s %s", fields, current->name + strlen("field_"));
		break; /* only process the first field this way */
		}

	current = current->next;
	}

/* if there are no fields sellected, say so */
if(current == 0)
	{
	printf("Content-Type: text/plain\n\n");
	printf("\n\nNo fields selected.\n");
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
			errAbort("Malformated field name.");

		sprintf(fields, "%s, %s", fields, current->name + strlen("field_"));
		}

	current = current->next;
	}

/* build the rest of the query */
table = strstr(table, "_");

printf("Content-type: text/plain\n\n");

sprintf(parsedTableName, "chr%d%s", 1, table);
sprintf(query, "SELECT%s FROM %s", fields, parsedTableName);
outputTabData(query, parsedTableName, conn, TRUE);

sprintf(parsedTableName, "chr%d_random%s", 1, table);
sprintf(query, "SELECT%s FROM %s", fields, parsedTableName);
outputTabData(query, parsedTableName, conn, FALSE);

for(c  = 2; c <= 22; c++)
	{
	sprintf(parsedTableName, "chr%d%s", c, table); 
	sprintf(query, "SELECT%s FROM %s", fields, parsedTableName); 
	outputTabData(query, parsedTableName, conn, FALSE);
	sprintf(parsedTableName, "chr%d_random%s", c, table); 
	sprintf(query, "SELECT%s FROM %s", fields, parsedTableName); 
	outputTabData(query, parsedTableName, conn, FALSE);
	}

sprintf(parsedTableName, "chr%s%s", "X", table);
sprintf(query, "SELECT%s FROM %s", fields, parsedTableName);
outputTabData(query, parsedTableName, conn, FALSE);
sprintf(parsedTableName, "chr%s_random%s", "X", table);
sprintf(query, "SELECT%s FROM %s", fields, parsedTableName);
outputTabData(query, parsedTableName, conn, FALSE);

sprintf(parsedTableName, "chr%s%s", "Y", table);
sprintf(query, "SELECT%s FROM %s", fields, parsedTableName);
outputTabData(query, parsedTableName, conn, FALSE);
sprintf(parsedTableName, "chr%s_random%s", "Y", table);
sprintf(query, "SELECT%s FROM %s", fields, parsedTableName);
outputTabData(query, parsedTableName, conn, FALSE);

sprintf(parsedTableName, "chr%s%s", "NA", table);
sprintf(query, "SELECT%s FROM %s", fields, parsedTableName);
outputTabData(query, parsedTableName, conn, FALSE);
sprintf(parsedTableName, "chr%s_random%s", "NA", table);
sprintf(query, "SELECT%s FROM %s", fields, parsedTableName);
outputTabData(query, parsedTableName, conn, FALSE);

sprintf(parsedTableName, "chr%s%s", "UL", table);
sprintf(query, "SELECT%s FROM %s", fields, parsedTableName);
outputTabData(query, parsedTableName, conn, FALSE);
sprintf(parsedTableName, "chr%s_random%s", "UL", table);
sprintf(query, "SELECT%s FROM %s", fields, parsedTableName);
outputTabData(query, parsedTableName, conn, FALSE);
}

void getSomeFieldsGenome()
/* get the fields that the user choose for the genome-wide informations */
{
char* table = cgiString("table");
	
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

table = cgiString("table");

/* make sure that the table name doesn't have anything "weird" in it */
if(!allLetters(table))
	{
	printf("Content-Type: text/plain\n\n");
	printf("Malformated table name.");
	return;
	}

/* get the name of the start and end fields */
sprintf(query, "SELECT * FROM %s", table);

conn = hAllocConn();

printf("Content-Type: text/plain\n\n");
outputTabData(query, table, conn, TRUE);
}


void getAllGenomeWideSplit()
{
char query[256];
struct sqlConnection *conn;
char* database;
int c;

char* table = cgiString("table");
char parsedTableName[256];

/* select the database */
database = cgiOptionalString("db");
if (database == NULL)
    database = "hg6";
hSetDb(database);
hDefaultConnect();
conn = hAllocConn();

/* build the rest of the query */
table = strstr(table, "_");

printf("Content-type: text/plain\n\n");

sprintf(parsedTableName, "chr%d%s", 1, table);
sprintf(query, "SELECT * FROM %s", parsedTableName);
outputTabData(query, parsedTableName, conn, TRUE);

sprintf(parsedTableName, "chr%d_random%s", 1, table);
sprintf(query, "SELECT * FROM %s", parsedTableName);
outputTabData(query, parsedTableName, conn, FALSE);

for(c  = 2; c <= 22; c++)
	{
	sprintf(parsedTableName, "chr%d%s", c, table); 
	sprintf(query, "SELECT * FROM %s", parsedTableName); 
	outputTabData(query, parsedTableName, conn, FALSE);
	sprintf(parsedTableName, "chr%d_random%s", c, table); 
	sprintf(query, "SELECT * FROM %s", parsedTableName); 
	outputTabData(query, parsedTableName, conn, FALSE);
	}

sprintf(parsedTableName, "chr%s%s", "X", table);
sprintf(query, "SELECT * FROM %s", parsedTableName);
outputTabData(query, parsedTableName, conn, FALSE);
sprintf(parsedTableName, "chr%s_random%s", "X", table);
sprintf(query, "SELECT * FROM %s", parsedTableName);
outputTabData(query, parsedTableName, conn, FALSE);

sprintf(parsedTableName, "chr%s%s", "Y", table);
sprintf(query, "SELECT * FROM %s", parsedTableName);
outputTabData(query, parsedTableName, conn, FALSE);
sprintf(parsedTableName, "chr%s_random%s", "Y", table);
sprintf(query, "SELECT * FROM %s", parsedTableName);
outputTabData(query, parsedTableName, conn, FALSE);

sprintf(parsedTableName, "chr%s%s", "NA", table);
sprintf(query, "SELECT * FROM %s", parsedTableName);
outputTabData(query, parsedTableName, conn, FALSE);
sprintf(parsedTableName, "chr%s_random%s", "NA", table);
sprintf(query, "SELECT * FROM %s", parsedTableName);
outputTabData(query, parsedTableName, conn, FALSE);

sprintf(parsedTableName, "chr%s%s", "UL", table);
sprintf(query, "SELECT * FROM %s", parsedTableName);
outputTabData(query, parsedTableName, conn, FALSE);
sprintf(parsedTableName, "chr%s_random%s", "UL", table);
sprintf(query, "SELECT * FROM %s", parsedTableName);
outputTabData(query, parsedTableName, conn, FALSE);
















}


void getAllFieldsGenome()
{
char* table = cgiString("table");
	
/* if they haven't choosen a table tell them */
if(existsAndEqual("table", "Choose table"))
	{
	printf("Content-type: text/plain\n\n");
	printf("\n\nPlease choose a table.");
	exit(0);
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



void getDNA()
/* get the DNA for the given position */
{
char *chromName;        /* Name of chromosome sequence . */
int winStart;           /* Start of window in sequence. */
int winEnd;         /* End of window in sequence. */
char* position;
struct dnaSeq *seq;
char dnaName[256];

/* if the position information is not given, get it */
position = cgiOptionalString("position");
if(position == NULL)
	position = "";
if(position != NULL && position[0] != 0)
	{
	if (!findGenomePos(position, &chromName, &winStart, &winEnd))
		return;
	}
findGenomePos(position, &chromName, &winStart, &winEnd);



sprintf(dnaName, "%s:%d-%d", chromName, winStart, winEnd);
puts(dnaName);
seq = hDnaFromSeq(chromName, winStart, winEnd, dnaMixed);
printf("<TT><PRE>");
faWriteNext(stdout, dnaName, seq->dna, seq->size);
printf("</TT></PRE>");
freeDnaSeq(&seq);
}



/* the main body of the program */
void execute()
{
char* table = cgiOptionalString("table");

hDefaultConnect();	/* read in the default connection options */

/* if there is no table chosen, ask for one. If the user pushed the "jump"
 * of the choose table form, we ask for the table again */
if(table == NULL || existsAndEqual("phase", "table"))
	{
	htmlSetBackground("../images/floret.jpg");
	htmShell("Genome Text Browser", getTable, NULL);
	}
else
	{
	if(table != 0 && existsAndEqual("phase", "Choose fields"))
		{
		htmlSetBackground("../images/floret.jpg");
		htmShell("Genome Text Browser", getChoosenFields, NULL);
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
		{
		htmlSetBackground("../images/floret.jpg");
		htmShell("DNA Sequence", getDNA, NULL);
		}
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
database = cgiOptionalString("db");
if (database == NULL)
    database = "hg6";
hSetDb(database);
hDefaultConnect();

execute(); /* call the main function */

return 0;
}
