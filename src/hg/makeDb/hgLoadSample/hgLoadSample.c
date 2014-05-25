/* hgLoadBed - Load a generic bed file into database. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "obscure.h"
#include "hash.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "dystring.h"
#include "sample.h"
#include "hdb.h"


/* Command line switches. */
boolean noBin = FALSE;		/* Suppress bin field. */
boolean strictTab = FALSE;	/* Separate on tabs. */
boolean oldTable = FALSE;	/* Don't redo table. */
char *sqlTable = NULL;		/* Read table from this .sql if non-NULL. */


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgLoadSample - Load a sample 9 (wiggle) file into database\n"
  "usage:\n"
  "   hgLoadSample database track files(s).smp\n"
  "options:\n"
  "   -noBin   suppress bin field\n"
  "   -oldTable add to existing table\n"
  "   -sqlTable=table.sql Create table from .sql file\n"
  "   -tab  Separate by tabs rather than space\n"
  );
}

int findBedSize(char *fileName)
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


void loadOneBed(char *fileName, int bedSize, struct bedStub **pList)
/* Load one bed file.  Make sure all lines have bedSize fields.
 * Put results in *pList. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[64], *line, *dupe;
int wordCount;
struct bedStub *bed;

verbose(1, "Reading %s\n", fileName);
while (lineFileNext(lf, &line, NULL))
    {
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
lineFileClose(&lf);
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
struct sqlConnection *conn = sqlConnect(database);
struct dyString *dy = newDyString(1024);
char *tab = "sample.tab";

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
    verbose(1, "Creating table definition for \n");
    sqlDyStringPrintf(dy, "CREATE TABLE %s (\n", track);
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
       dyStringAppend(dy, "  sampleCount int unsigned not null,\n");
    if (bedSize >= 8)
       dyStringAppend(dy, "  samplePosition longblob not null,\n");
    if (bedSize >= 9)
       dyStringAppend(dy, "  sampleHeight longblob not null,\n");
    dyStringAppend(dy, "#Indices\n");
    if (!noBin)
       dyStringAppend(dy, "  INDEX(chrom(8),bin),\n");
    if (bedSize >= 4)
       dyStringAppend(dy, "  INDEX(name(16)),\n");
    dyStringAppend(dy, "  INDEX(chrom(8),chromStart)\n");
    dyStringAppend(dy, ")\n");
    sqlRemakeTable(conn, track, dy->string);
    }

verbose(1, "Saving %s\n", tab);
writeBedTab(tab, bedList, bedSize);

verbose(1, "Loading %s\n", database);
dyStringClear(dy);
sqlDyStringPrintf(dy, "load data local infile '%s' into table %s", tab, track);
sqlUpdate(conn, dy->string);
sqlDisconnect(&conn);
}

void hgLoadBed(char *database, char *track, int bedCount, char *bedFiles[])
/* hgLoadBed - Load a generic bed file into database. */
{
int bedSize = findBedSize(bedFiles[0]);
struct bedStub *bedList = NULL;
int i;

for (i=0; i<bedCount; ++i)
    loadOneBed(bedFiles[i], bedSize, &bedList);
verbose(1, "Loaded %d elements\n", slCount(bedList));
slSort(&bedList, bedStubCmp);
verbose(1, "Sorted\n");
loadDatabase(database, track, bedSize, bedList);
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc < 4)
    usage();
noBin = cgiBoolean("noBin");
strictTab = cgiBoolean("tab");
uglyf("stringTab = %d\n", strictTab);
oldTable = cgiBoolean("oldTable");
sqlTable = cgiOptionalString("sqlTable");
hgLoadBed(argv[1], argv[2], argc-3, argv+3);
return 0;
}
