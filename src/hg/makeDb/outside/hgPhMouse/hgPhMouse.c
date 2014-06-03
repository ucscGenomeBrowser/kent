/* hgPhMouse - Load phMouse track. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "dystring.h"
#include "hash.h"
#include "cheapcgi.h"
#include "hdb.h"
#include "bed.h"
#include "jksql.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgPhMouse - Load phMouse track\n"
  "usage:\n"
  "   hgPhMouse database track chromFile(s)\n"
  "options:\n"
  "   -add - add data, don't overwrite old track\n"
  );
}

char *createString = 
"CREATE TABLE %s (\n"
"    bin smallint unsigned not null,    # Bin index field\n"
"    chrom varchar(255) not null,	# Human chromosome or FPC contig\n"
"    chromStart int unsigned not null,	# Start position in chromosome\n"
"    chromEnd int unsigned not null,	# End position in chromosome\n"
"    name varchar(255) not null,	# Name of item\n"
"    score int unsigned not null,	# Score from 0-1000\n"
"    strand char(1) not null,	# + or -\n"
"              #Indices\n"
"    INDEX(chrom(8),bin),\n"
"    INDEX(chrom(8),chromStart),\n"
"    INDEX(chrom(8),chromEnd)\n"
")\n";

void loadDatabase(char *database, char *track, char *tabName)
/* Load up database from tab-file. */
{
struct sqlConnection *conn = sqlConnect(database);
struct dyString *dy = newDyString(1024);


if (!cgiBoolean("add"))
    {
    sqlDyStringPrintf(dy, createString, track);
    sqlRemakeTable(conn, track, dy->string);
    dyStringClear(dy);
    }
sqlDyStringPrintf(dy, "load data local infile '%s' into table %s",
	tabName, track);
sqlUpdate(conn, dy->string);
dyStringFree(&dy);
sqlDisconnect(&conn);
}


void hgPhMouse(char *database, char *track, int fileCount, char *fileNames[])
/* hgPhMouse - Load phMouse track. */
{
int i;
char *fileName;
char *tabName = "phMouse.tab";
FILE *f = mustOpen(tabName, "w");
struct lineFile *lf;
char *words[32], *s, c;
int wordCount;
int oneSize, totalSize = 0;

for (i=0; i<fileCount; ++i)
    {
    struct bed *bedList = NULL, *bed;
    fileName = fileNames[i];
    lf = lineFileOpen(fileName, TRUE);
    printf("Reading %s ", fileName);
    fflush(stdout);
    while ((wordCount = lineFileChop(lf, words)) > 0)
        {
	if (wordCount < 7)
	   errAbort("Expecting at least 7 words line %d of %s", 
	   	lf->lineIx, fileName);
	AllocVar(bed);
	bed->chrom = cloneString(words[0]);
	bed->chromStart = lineFileNeedNum(lf, words, 1);
	bed->chromEnd = lineFileNeedNum(lf, words, 2);
	bed->score = lineFileNeedNum(lf, words, 6);
	s = strrchr(words[3], '|');
	c = s[1];
	s[0] = 0;
	if (c != '+' && c != '-')
	    errAbort("Misformed strandless trace name line %d of %s",
	    	lf->lineIx, lf->fileName);
	bed->name = cloneString(words[3]);
	bed->strand[0] = c;
	slAddHead(&bedList, bed);
	}
    oneSize = slCount(bedList);
    printf("%d alignments ", oneSize);
    totalSize += oneSize;
    fflush(stdout);
    slSort(&bedList, bedCmp);
    printf("sorted ");
    fflush(stdout);
    for (bed = bedList; bed != NULL; bed = bed->next)
        {
	int bin = hFindBin(bed->chromStart, bed->chromEnd);
	fprintf(f, "%d\t", bin);
	bedTabOutN(bed, 6, f);
	}
    printf("tabbed out\n");
    bedFreeList(&bedList);
    }
carefulClose(&f);
printf("Loading %d items into %s.%s\n", totalSize, database, track);
loadDatabase(database, track, tabName);
remove(tabName);
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc < 4)
    usage();
hgPhMouse(argv[1], argv[2], argc-3, argv+3);
return 0;
}
