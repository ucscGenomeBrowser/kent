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

static char const rcsid[] = "$Id: hgLoadWiggle.c,v 1.9 2004/03/30 00:47:06 hiram Exp $";

/* Command line switches. */
boolean noBin = FALSE;		/* Suppress bin field. */
boolean strictTab = FALSE;	/* Separate on tabs. */
boolean oldTable = FALSE;	/* Don't redo table. */
char *pathPrefix = NULL;	/* path prefix instead of /gbdb/hg16/wib */

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"smallInsertSize", OPTION_INT},
    {"tab", OPTION_BOOLEAN},
    {"noBin", OPTION_BOOLEAN},
    {"oldTable", OPTION_BOOLEAN},
    {"pathPrefix", OPTION_STRING},
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
  "   -tab\tSeparate by tabs rather than space\n"
  "   -pathPrefix=<path>\t.wib file path prefix to use (default /gbdb/<DB>/wib)"
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
verbose(2, "Read %d lines from %s\n", lineCount, fileName);
}

void writeWiggleTab(char *fileName, struct wiggleStub *wiggleList,
	int wiggleSize, char *database)
/* Write out wiggle list to tab-separated file. */
{
struct wiggleStub *wiggle;
FILE *f = mustOpen(fileName, "w");
char *words[64];
int i, wordCount;
unsigned int wiggleCount;

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
	if (i==7)
	    {
	    if (pathPrefix )
		fprintf(f,"%s/", pathPrefix );
	    else
		fprintf(f,"/gbdb/%s/wib/", database );
	    }
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

verbose(1, "Connected to database %s for track %s\n", database, track);
/* First make table definition. */
if (!oldTable)
    {
    /* Create definition statement. */
    verbose(1, "Creating table definition with %d columns in %s.%s\n",
	    wiggleSize, database, track);
    dyStringPrintf(dy, "CREATE TABLE %s (\n", track);
    if (!noBin)
       dyStringAppend(dy, "  bin smallint unsigned not null,\n");
    dyStringAppend(dy, "  chrom varchar(255) not null,\n");
    dyStringAppend(dy, "  chromStart int unsigned not null,\n");
    dyStringAppend(dy, "  chromEnd int unsigned not null,\n");
    dyStringAppend(dy, "  name varchar(255) not null,\n");
    dyStringAppend(dy, "  span int unsigned not null,\n");
    dyStringAppend(dy, "  count int unsigned not null,\n");
    dyStringAppend(dy, "  offset int unsigned not null,\n");
    dyStringAppend(dy, "  file varchar(255) not null,\n");
    dyStringAppend(dy, "  lowerLimit double not null,\n");
    dyStringAppend(dy, "  dataRange double not null,\n");
    dyStringAppend(dy, "  validCount int unsigned not null,\n");
    dyStringAppend(dy, "  sumData double not null,\n");
    dyStringAppend(dy, "  sumSquares double not null,\n");
    dyStringAppend(dy, "#Indices\n");
    if (!noBin)
       dyStringAppend(dy, "  INDEX(chrom(8),bin),\n");
    else
	{
	dyStringAppend(dy, "  INDEX(chrom(8),chromStart),\n");
	dyStringAppend(dy, "  INDEX(chrom(8),chromEnd)\n");
	}
    dyStringAppend(dy, ")\n");
    sqlRemakeTable(conn, track, dy->string);
    }

verbose(1, "Saving %s\n", tab);
writeWiggleTab(tab, wiggleList, wiggleSize, database);

verbose(1, "Loading %s\n", database);
dyStringClear(dy);
dyStringPrintf(dy, "load data local infile '%s' into table %s", tab, track);
sqlUpdate(conn, dy->string);
sqlDisconnect(&conn);
}

void hgLoadWiggle(char *database, char *track, int wiggleCount, char *wiggleFiles[])
/* hgLoadWiggle - Load a generic wiggle file into database. */
{
int wiggleSize = findWiggleSize(wiggleFiles[0]);
struct wiggleStub *wiggleList = NULL;
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
pathPrefix = optionVal("pathPrefix",NULL);
verbose(2, "noBin: %s, tab: %s, oldTable: %s\n",
	noBin ? "TRUE" : "FALSE",
	strictTab ? "TRUE" : "FALSE",
	oldTable ? "TRUE" : "FALSE");
if (pathPrefix)
    verbose(2, "pathPrefix: %s\n", pathPrefix);
hgLoadWiggle(argv[1], argv[2], argc-3, argv+3);
return 0;
}
