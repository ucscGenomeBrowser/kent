/* hgStsMarkers - Load STS markers into database. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "jksql.h"
#include "hash.h"
#include "bed.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgStsMarkers - Load STS markers into database\n"
  "usage:\n"
  "   hgStsMarkers database markerDir\n");
}

#define numMaps 6

char *markerFiles[numMaps] = {
    "genethon.BED",
    "marshfield.BED",
    "gm99_gb4.BED",
    "shgc_g3.BED",
    "wi_yac.BED",
    "shgc_tng.BED",
};
 
#define maxWords 16
typedef char *WordArray[maxWords];

void makeTabFile(char *markerDir, char *tabName)
/* Create tab-delimiter file synthesizing all maps. */
{
FILE *f = NULL;
struct lineFile *allLf[numMaps], *lf;
char *allWords[numMaps][maxWords];
int wordCount, i,j;
char fileName[512];
static int sharedFields[] = {0,1,2,3,4,5,8,9};
int lineCount = 0;

/* Open all files. */
f = mustOpen(tabName, "w");
for (i=0; i<numMaps; ++i)
    {
    sprintf(fileName, "%s/%s", markerDir, markerFiles[i]);
    printf("Opening %s\n", fileName);
    allLf[i] = lineFileOpen(fileName, TRUE);
    }

/* Combine a line of each input into a single line of output. */
for (;;)
    {
    boolean gotZero = FALSE;
    int totalWords = 0;

    /* Read next line in each file, and check for end of file. */
    for (i=0; i<numMaps; ++i)
        {
	lf = allLf[i];
	wordCount = lineFileChopNext(lf, allWords[i], maxWords);
	if (wordCount == 0)
	    gotZero = TRUE;
	else
	    {
	    lineFileExpectWords(lf, 10, wordCount);
	    totalWords += wordCount;
	    }
	}
    if (gotZero)
        {
	if (totalWords != 0)
	    errAbort("Not all input maps have same number of lines");
	break;
	}
    ++lineCount;
    if (lineCount%10000 == 0)
        printf("processing line %d\n", lineCount);

    /* Check that shared fields are consistent between maps. */
    for (i=1; i<numMaps; ++i)
        {
	for (j=0; j<ArraySize(sharedFields); ++j)
	    {
	    int field = sharedFields[j];
	    if (!sameString(allWords[0][field], allWords[i][field]))
	        {
		errAbort("Inconsistent field %d line %d of %s and %s: %s vs %s",
		    field, allLf[0]->lineIx, allLf[0]->fileName, allLf[i]->fileName,
		    allWords[0][field], allWords[i][field]);
		}
	    }
	}

    /* Write out chromosome and chrom start. */
    if (sameString(allWords[0][0], "chr"))
	fprintf(f, "unknown\t");
    else
        fprintf(f, "%s\t", allWords[0][0]);

    /* Write out other shared data fields. */
    for (i=1; i<ArraySize(sharedFields); ++i)
	{
	int field = sharedFields[i];
	fprintf(f, "%s\t", allWords[0][field]);
	}

    /* Write out unique data. */
    for (i=0; i<numMaps; ++i)
	{
	char end = (i == numMaps-1 ? '\n' : '\t');
	fprintf(f, "%s\t%s%c", allWords[i][6], allWords[i][7], end);
	}
    }

/* Close all files. */
for (i=0; i<numMaps; ++i)
    lineFileClose(&allLf[i]);
carefulClose(&f);
}
void hgStsMarkers(char *database, char *markerDir)
/* hgStsMarkers - Load STS markers into database. */
{
char *table = "stsMarker";
char *tabName = "stsMarkers.tab";
struct sqlConnection *conn = NULL;
char query[256];

makeTabFile(markerDir, tabName);
bedSortFile(tabName, tabName);

conn = sqlConnect(database);
if (!sqlTableExists(conn, table))
    errAbort("You need to create the table first (with %s.sql)", table);
printf("Loading %s database %s table\n", database, table);
sqlSafef(query, sizeof query, "delete from %s", table);
sqlUpdate(conn, query);
sqlSafef(query, sizeof query, "load data local infile '%s' into table %s", tabName, table);
sqlUpdate(conn, query);

sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
hgStsMarkers(argv[1], argv[2]);
return 0;
}
