/* hgLoadWiggle - Load a Wiggle track "bed" file into database. */
#include "common.h"
#include "options.h"
#include "linefile.h"
#include "obscure.h"
#include "hash.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "dystring.h"
#include "wiggle.h"
#include "hdb.h"

static char const rcsid[] = "$Id: hgLoadWiggle.c,v 1.1 2003/09/16 22:46:07 hiram Exp $";

/* Command line switches. */
boolean noBin = FALSE;		/* Suppress bin field. */
boolean strictTab = FALSE;	/* Separate on tabs. */
boolean oldTable = FALSE;	/* Don't redo table. */
boolean verbose = FALSE;	/* Explain what is happening */
char *sqlTable = NULL;		/* Read table from this .sql if non-NULL. */

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"sqlTable", OPTION_BOOLEAN},
    {"smallInsertSize", OPTION_INT},
    {"tab", OPTION_BOOLEAN},
    {"noBin", OPTION_BOOLEAN},
    {"oldTable", OPTION_BOOLEAN},
    {"verbose", OPTION_BOOLEAN},
    {NULL, 0}
};

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgLoadWiggle - Load a wiggle track definition into database\n"
  "usage:\n"
  "   hgLoadWiggle [options] database track files(s).wig\n"
  "options:\n"
  "   -noBin\t suppress bin field\n"
  "   -oldTable\t add to existing table\n"
  "   -sqlTable=table.sql\t Create table from .sql file\n"
  "   -tab\t\tSeparate by tabs rather than space\n"
  );
}

int findWiggleSize(char *fileName)
/* Read first line of file and figure out how many words in it. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[64], *line;
int wordCount;
lineFileNeedNext(lf, &line, NULL);
if (strictTab)
    wordCount = chopTabs(line, words);
else
    wordCount = chopLine(line, words);
if (wordCount == 0)
    errAbort("%s appears to be empty", fileName);
lineFileClose(&lf);
return wordCount;
}

struct wiggleStub
/* A line in a wiggle file with chromosome, start, end position parsed out. */
    {
    struct wiggleStub *next;	/* Next in list. */
    char *chrom;                /* Chromosome . */
    int chromStart;             /* Start position. */
    int chromEnd;		/* End position. */
    char *line;                 /* Line. */
    };

int wiggleStubCmp(const void *va, const void *vb)
/* Compare to sort based on query. */
{
const struct wiggleStub *a = *((struct wiggleStub **)va);
const struct wiggleStub *b = *((struct wiggleStub **)vb);
int dif;
dif = strcmp(a->chrom, b->chrom);
if (dif == 0)
    dif = a->chromStart - b->chromStart;
return dif;
}


void loadOneWiggle(char *fileName, int wiggleSize, struct wiggleStub **pList)
/* Load one wiggle file.  Make sure all lines have wiggleSize fields.
 * Put results in *pList. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[64], *line, *dupe;
int wordCount;
struct wiggleStub *wiggle;
int lineCount = 0;

while (lineFileNext(lf, &line, NULL))
    {
    ++lineCount;
    dupe = cloneString(line);
    if (strictTab)
	wordCount = chopTabs(line, words);
    else
	wordCount = chopLine(line, words);
    lineFileExpectWords(lf, wiggleSize, wordCount);
    AllocVar(wiggle);
    wiggle->chrom = cloneString(words[0]);
    wiggle->chromStart = lineFileNeedNum(lf, words, 1);
    wiggle->chromEnd = lineFileNeedNum(lf, words, 2);
    wiggle->line = dupe;
    slAddHead(pList, wiggle);
    }
lineFileClose(&lf);
if( verbose )
    printf("Read %d lines from %s\n", lineCount, fileName);
}

void writeWiggleTab(char *fileName, struct wiggleStub *wiggleList, int wiggleSize)
/* Write out wiggle list to tab-separated file. */
{
struct wiggleStub *wiggle;
FILE *f = mustOpen(fileName, "w");
char *words[64];
int i, j, wordCount;
unsigned int wiggleCount;
char **wiggleValueStrings;
unsigned char *wiggleValues;

for (wiggle = wiggleList; wiggle != NULL; wiggle = wiggle->next)
    {
    if (!noBin)
        fprintf(f, "%u\t", hFindBin(wiggle->chromStart, wiggle->chromEnd));
    if (strictTab)
	wordCount = chopTabs(wiggle->line, words);
    else
	wordCount = chopLine(wiggle->line, words);
    for (i=0; i<wordCount; ++i)
        {
	fputs(words[i], f);
	if (i == wordCount-1)
	    fputc('\n', f);
	else
	    fputc('\t', f);
	}
    	(void) sscanf(words[7], "%d", &wiggleCount);
    }
fclose(f);
}

void loadDatabase(char *database, char *track, int wiggleSize, struct wiggleStub *wiggleList)
/* Load database from wiggleList. */
{
struct sqlConnection *conn = sqlConnect(database);
struct dyString *dy = newDyString(1024);
char *tab = "wiggle.tab";

if( verbose )
    uglyf("Connected to database %s for track %s\n", database, track);
/* First make table definition. */
if (sqlTable != NULL)
    {
    /* Read from file. */
    char *sql, *s;
    readInGulp(sqlTable, &sql, NULL);

    /* Chop of end-of-statement semicolon if need be. */
    s = strchr(sql, ';');
    if (s != NULL) *s = 0;
    
    sqlRemakeTable(conn, track, sql);
    freez(&sql);
    }
else if (!oldTable)
    {
    /* Create definition statement. */
    printf("Creating table definition with %d columns in %s.%s\n",
	    wiggleSize, database, track);
    dyStringPrintf(dy, "CREATE TABLE %s (\n", track);
    if (!noBin)
       dyStringAppend(dy, "  bin smallint unsigned not null,\n");
    dyStringAppend(dy, "  chrom varchar(255) not null,\n");
    dyStringAppend(dy, "  chromStart int unsigned not null,\n");
    dyStringAppend(dy, "  chromEnd int unsigned not null,\n");
    if (wiggleSize >= 4)
       dyStringAppend(dy, "  name varchar(255) not null,\n");
    if (wiggleSize >= 5)
       dyStringAppend(dy, "  score int unsigned not null,\n");
    if (wiggleSize >= 6)
       dyStringAppend(dy, "  strand char(1) not null,\n");
    if (wiggleSize >= 7)
       dyStringAppend(dy, "  Span int unsigned not null,\n");
    if (wiggleSize >= 8)
       dyStringAppend(dy, "  Count int unsigned not null,\n");
    if (wiggleSize >= 9)
       dyStringAppend(dy, "  Offset int unsigned not null,\n");
    if (wiggleSize >= 10)
       dyStringAppend(dy, "  File varchar(255) not null,\n");
    dyStringAppend(dy, "#Indices\n");
    if (!noBin)
       dyStringAppend(dy, "  INDEX(chrom(8),bin),\n");
    if (wiggleSize >= 4)
       dyStringAppend(dy, "  INDEX(name(16)),\n");
    dyStringAppend(dy, "  INDEX(chrom(8),chromStart),\n");
    dyStringAppend(dy, "  INDEX(chrom(8),chromEnd)\n");
    dyStringAppend(dy, ")\n");
    sqlRemakeTable(conn, track, dy->string);
    }

printf("Saving %s\n", tab);
writeWiggleTab(tab, wiggleList, wiggleSize);

printf("Loading %s\n", database);
dyStringClear(dy);
dyStringPrintf(dy, "load data local infile '%s' into table %s", tab, track);
sqlUpdate(conn, dy->string);
sqlDisconnect(&conn);
}

void hgLoadWiggle(char *database, char *track, int wiggleCount, char *wiggleFiles[])
/* hgLoadWiggle - Load a generic wiggle file into database. */
{
int wiggleSize = findWiggleSize(wiggleFiles[0]);
struct wiggleStub *wiggleList = NULL, *wiggle;
int i;

for (i=0; i<wiggleCount; ++i)
    loadOneWiggle(wiggleFiles[i], wiggleSize, &wiggleList);
slSort(&wiggleList, wiggleStubCmp);
loadDatabase(database, track, wiggleSize, wiggleList);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);

if (argc < 4)
    usage();
noBin = optionExists("noBin");
strictTab = optionExists("tab");
oldTable = optionExists("oldTable");
sqlTable = optionVal("sqlTable", NULL);
verbose = optionExists("verbose");
if( verbose )
    {
	printf("noBin: %s, tab: %s, oldTable: %s\n",
		noBin ? "TRUE" : "FALSE",
		strictTab ? "TRUE" : "FALSE",
		oldTable ? "TRUE" : "FALSE");
	if( sqlTable )
	    printf("using sql definition file: %s\n", sqlTable);
    }
hgLoadWiggle(argv[1], argv[2], argc-3, argv+3);
return 0;
}
