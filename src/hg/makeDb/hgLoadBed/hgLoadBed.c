/* hgLoadBed - Load a generic bed file into database. */
#include "common.h"
#include "options.h"
#include "linefile.h"
#include "obscure.h"
#include "hash.h"
#include "jksql.h"
#include "dystring.h"
#include "bed.h"
#include "hdb.h"
#include "hgRelate.h"

static char const rcsid[] = "$Id: hgLoadBed.c,v 1.32 2004/12/16 21:23:43 jsp Exp $";

/* Command line switches. */
boolean noSort = FALSE;		/* don't sort */
boolean noBin = FALSE;		/* Suppress bin field. */
boolean hasBin = FALSE;		/* Input bed file includes bin. */
boolean strictTab = FALSE;	/* Separate on tabs. */
boolean oldTable = FALSE;	/* Don't redo table. */
boolean noLoad = FALSE;		/* Do not load DB or remove tab file */
char *sqlTable = NULL;		/* Read table from this .sql if non-NULL. */

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"noSort", OPTION_BOOLEAN},
    {"noBin", OPTION_BOOLEAN},
    {"nobin", OPTION_BOOLEAN},
    {"oldTable", OPTION_BOOLEAN},
    {"onServer", OPTION_BOOLEAN},
    {"sqlTable", OPTION_STRING},
    {"tab", OPTION_BOOLEAN},
    {"hasBin", OPTION_BOOLEAN},
    {"noLoad", OPTION_BOOLEAN},
    {NULL, 0}
};

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgLoadBed - Load a generic bed file into database\n"
  "usage:\n"
  "   hgLoadBed database track files(s).bed\n"
  "options:\n"
  "   -noSort  don't sort (you better be sorting before this)\n"
  "   -noBin   suppress bin field\n"
  "   -oldTable add to existing table\n"
  "   -onServer This will speed things up if you're running in a directory that\n"
  "             the mysql server can access.\n"
  "   -sqlTable=table.sql Create table from .sql file\n"
  "   -tab  Separate by tabs rather than space\n"
  "   -hasBin   Input bed file starts with a bin field.\n"
  "   -noLoad  - Do not load database and do not clean up tab files\n"
  );
}

int findBedSize(char *fileName, struct lineFile **retLf)
/* Read first line of file and figure out how many words in it. */
/* Input file could be stdin, in which case we really don't want to open,
 * read, and close it here.  So if retLf is non-NULL, return the open 
 * linefile (having told it to reuse the line we just read). */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[64], *line;
int wordCount;
lineFileNeedNext(lf, &line, NULL);
line = cloneString(line);
if (strictTab)
    wordCount = chopTabs(line, words);
else
    wordCount = chopLine(line, words);
if (wordCount == 0)
    errAbort("%s appears to be empty", fileName);
if (retLf != NULL)
    {
    lineFileReuse(lf);
    *retLf = lf;
    }
else
    lineFileClose(&lf);
freeMem(line);
return wordCount;
}

struct bedStub
/* A line in a bed file with chromosome, start, end position parsed out. */
    {
    struct bedStub *next;	/* Next in list. */
    char *chrom;                /* Chromosome . */
    int chromStart;             /* Start position. */
    int chromEnd;		/* End position. */
    char *line;                 /* Line. */
    };

int bedStubCmp(const void *va, const void *vb)
/* Compare to sort based on query. */
{
const struct bedStub *a = *((struct bedStub **)va);
const struct bedStub *b = *((struct bedStub **)vb);
int dif;
dif = strcmp(a->chrom, b->chrom);
if (dif == 0)
    dif = a->chromStart - b->chromStart;
return dif;
}

void loadOneBed(struct lineFile *lf, int bedSize, struct bedStub **pList)
/* Load one bed file.  Make sure all lines have bedSize fields.
 * Put results in *pList. */
{
char *words[64], *line, *dupe;
int wordCount;
struct bedStub *bed;

verbose(1, "Reading %s\n", lf->fileName);
while (lineFileNext(lf, &line, NULL))
    {
    if (hasBin)
	nextWord(&line);
    dupe = cloneString(line);
    if (strictTab)
	wordCount = chopTabs(line, words);
    else
	wordCount = chopLine(line, words);
    lineFileExpectWords(lf, bedSize, wordCount);
    AllocVar(bed);
    bed->chrom = cloneString(words[0]);
    bed->chromStart = lineFileNeedNum(lf, words, 1);
    bed->chromEnd = lineFileNeedNum(lf, words, 2);
    bed->line = dupe;
    slAddHead(pList, bed);
    }
}

void writeBedTab(char *fileName, struct bedStub *bedList, int bedSize)
/* Write out bed list to tab-separated file. */
{
struct bedStub *bed;
FILE *f = mustOpen(fileName, "w");
char *words[64];
int i, wordCount;
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    if (!noBin)
        fprintf(f, "%u\t", hFindBin(bed->chromStart, bed->chromEnd));
    if (strictTab)
	wordCount = chopTabs(bed->line, words);
    else
	wordCount = chopLine(bed->line, words);
    for (i=0; i<wordCount; ++i)
        {
	/*	new definition for old "reserved" field, now itemRgb */
	/*	and when itemRgb, it is a comma separated string r,g,b */
	if (i == 8 && sqlTable == NULL)
	    {
	    char *comma;
	    /*  Allow comma separated list of rgb values here   */
	    comma = strchr(words[8], ',');
	    if (comma)
		{
		int itemRgb = 0;
		if (-1 == (itemRgb = bedParseRgb(words[8])))
		    errAbort("ERROR: expecting r,g,b specification, "
				"found: '%s'", words[8]);
		else
		    fprintf(f,"%d", itemRgb);
		verbose(2, "itemRgb: %s, rgb: %#x\n", words[8], itemRgb);
		}
	    else
		fputs(words[i], f);
	    }
	else
	    fputs(words[i], f);

	if (i == wordCount-1)
	    fputc('\n', f);
	else
	    fputc('\t', f);
	}
    }
fclose(f);
}

void loadDatabase(char *database, char *track, int bedSize, struct bedStub *bedList)
/* Load database from bedList. */
{
struct sqlConnection *conn;
struct dyString *dy = newDyString(1024);
char *tab = "bed.tab";
int loadOptions = (optionExists("onServer") ? SQL_TAB_FILE_ON_SERVER : 0);
char comment[256];

if ( ! noLoad )
    conn = sqlConnect(database);

/* First make table definition. */
if (sqlTable != NULL)
    {
    /* Read from file. */
    char *sql, *s;
    readInGulp(sqlTable, &sql, NULL);

    /* Chop of end-of-statement semicolon if need be. */
    s = strchr(sql, ';');
    if (s != NULL) *s = 0;
    
    if ( ! noLoad )
	sqlRemakeTable(conn, track, sql);

    freez(&sql);
    }
else if (!oldTable)
    {
    int minLength;

    hSetDb(database);
    minLength = hGetMinIndexLength();

    /* Create definition statement. */
    verbose(1, "Creating table definition for \n");
    dyStringPrintf(dy, "CREATE TABLE %s (\n", track);
    if (!noBin)
       dyStringAppend(dy, "  bin smallint unsigned not null,\n");
    dyStringAppend(dy, "  chrom varchar(255) not null,\n");
    dyStringAppend(dy, "  chromStart int unsigned not null,\n");
    dyStringAppend(dy, "  chromEnd int unsigned not null,\n");
    if (bedSize >= 4)
       dyStringAppend(dy, "  name varchar(255) not null,\n");
    if (bedSize >= 5)
       dyStringAppend(dy, "  score int unsigned not null,\n");
    if (bedSize >= 6)
       dyStringAppend(dy, "  strand char(1) not null,\n");
    if (bedSize >= 7)
       dyStringAppend(dy, "  thickStart int unsigned not null,\n");
    if (bedSize >= 8)
       dyStringAppend(dy, "  thickEnd int unsigned not null,\n");
    /*	As of 2004-11-22 the reserved field is used as itemRgb in code */
    if (bedSize >= 9)
       dyStringAppend(dy, "  reserved int unsigned  not null,\n");
    if (bedSize >= 10)
       dyStringAppend(dy, "  blockCount int unsigned not null,\n");
    if (bedSize >= 11)
       dyStringAppend(dy, "  blockSizes longblob not null,\n");
    if (bedSize >= 12)
       dyStringAppend(dy, "  chromStarts longblob not null,\n");
    if (bedSize >= 13)
       dyStringAppend(dy, "  expCount int unsigned not null,\n");
    if (bedSize >= 14)
       dyStringAppend(dy, "  expIds longblob not null,\n");
    if (bedSize >= 15)
       dyStringAppend(dy, "  expScores longblob not null,\n");
    dyStringAppend(dy, "#Indices\n");
    if (!noBin)
       dyStringPrintf(dy, "  INDEX(chrom(%d),bin),\n", minLength);
    if (bedSize >= 4)
       dyStringAppend(dy, "  INDEX(name(16)),\n");
    dyStringPrintf(dy, "  INDEX(chrom(%d),chromStart),\n", minLength);
    dyStringPrintf(dy, "  INDEX(chrom(%d),chromEnd)\n", minLength);
    dyStringAppend(dy, ")\n");
    if ( ! noLoad )
	sqlRemakeTable(conn, track, dy->string);
    }

verbose(1, "Saving %s\n", tab);
writeBedTab(tab, bedList, bedSize);

if ( ! noLoad )
    {
    verbose(1, "Loading %s\n", database);
    sqlLoadTabFile(conn, tab, track, loadOptions);

    /* add a comment to the history table and finish up connection */
    safef(comment, sizeof(comment),
	"Add %d element(s) from bed list to %s table",
	    slCount(bedList), track);
    hgHistoryComment(conn, comment);
    sqlDisconnect(&conn);
    }
else
    verbose(1, "No load option selected, see file: %s\n", tab);
}

void hgLoadBed(char *database, char *track, int bedCount, char *bedFiles[])
/* hgLoadBed - Load a generic bed file into database. */
{
struct lineFile *lf = NULL;
int bedSize = findBedSize(bedFiles[0], &lf);
struct bedStub *bedList = NULL;
int i;

if (hasBin)
    bedSize--;
for (i=0; i<bedCount; ++i)
    {
    /* bedFiles[0] was opened by findBedSize above -- since it might be stdin,
     * it's left open and reused by loadOneBed.  After that, open here: */
    if (i > 0)
	lf = lineFileOpen(bedFiles[i], TRUE);
    loadOneBed(lf, bedSize, &bedList);
    lineFileClose(&lf);
    }
verbose(1, "Loaded %d elements of size %d\n", slCount(bedList), bedSize);
if (!noSort)
    {
    slSort(&bedList, bedStubCmp);
    verbose(1, "Sorted\n");
    }
else
    {
    verbose(1, "Not Sorting\n");
    slReverse(&bedList);
    }

loadDatabase(database, track, bedSize, bedList);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc < 4)
    usage();
noBin = optionExists("noBin") || optionExists("nobin");
noSort = optionExists("noSort");
strictTab = optionExists("tab");
oldTable = optionExists("oldTable");
sqlTable = optionVal("sqlTable", sqlTable);
hasBin = optionExists("hasBin");
noLoad = optionExists("noLoad");
hgLoadBed(argv[1], argv[2], argc-3, argv+3);
return 0;
}
