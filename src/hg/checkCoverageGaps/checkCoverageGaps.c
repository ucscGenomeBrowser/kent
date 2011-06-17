/* checkCoverageGaps - Check for biggest gap in coverage for a list of tracks.. */
#include "common.h"
#include "localmem.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "trackDb.h"
#include "hdb.h"
#include "obscure.h"
#include "rangeTree.h"
#include "bigWig.h"
#include "bigBed.h"

static char const rcsid[] = "$Id: newProg.c,v 1.30 2010/03/24 21:18:33 hiram Exp $";

boolean allParts = FALSE;
boolean female = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "checkCoverageGaps - Check for biggest gap in coverage for a list of tracks.\n"
  "usage:\n"
  "   checkCoverageGaps database track1 ... trackN\n"
  "Note: for bigWig and bigBeds, the biggest gap is rounded to the nearest 10,000 or so\n"
  "options:\n"
  "   -allParts  If set then include _hap and _random and other wierd chroms\n"
  "   -female If set then don't check chrY\n"
  );
}

static struct optionSpec options[] = {
   {"allParts", OPTION_BOOLEAN},
   {"female", OPTION_BOOLEAN},
   {NULL, 0},
};

void bigWigBiggestGap(struct trackDb *tdb, struct sqlConnection *conn, 
	char *chrom, int chromSize, struct rbTree *rt)
/* Find biggest gap in given chromosome in bigWig */
{
char *fileName = bbiNameFromSettingOrTable(tdb, conn, tdb->table);
struct bbiFile *bbi = bigWigFileOpen(fileName);
int sampleSize = 10000;
int sampleCount = chromSize/sampleSize;
if (sampleCount > 1)
    {
    int evenEnd = sampleCount * sampleSize;
    double *summaryVals;
    AllocArray(summaryVals, sampleCount);
    if (bigWigSummaryArray(bbi, chrom, 0, evenEnd, bbiSumCoverage, sampleCount, summaryVals))
	{
	int s=0, e, i;
	for (i=0; i<sampleCount; ++i)
	    {
	    e = s + sampleSize;
	    if (summaryVals[i] > 0.0)
	        rangeTreeAdd(rt, s, e);
	    s = e;
	    }
	}
    }
bigWigFileClose(&bbi);
}

void bigBedBiggestGap(struct trackDb *tdb, struct sqlConnection *conn, 
	char *chrom, int chromSize, struct rbTree *rt)
/* Find biggest gap in given chromosome in bigBed */
{
uglyAbort("bigBed not implemented");
}

void tableBiggestGap(struct hTableInfo *hti, struct trackDb *tdb, struct sqlConnection *conn, 
	char *chrom, int chromSize, struct rbTree *rt)
/* Find biggest gap in given chromosome in bigBed */
{
char fields[512];
safef(fields, sizeof(fields), "%s,%s", hti->startField, hti->endField);
struct sqlResult *sr = hExtendedChromQuery(conn, hti->rootName, chrom, NULL, FALSE,
	fields, NULL);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    rangeTreeAdd(rt, sqlUnsigned(row[0]), sqlUnsigned(row[1]));
    }
sqlFreeResult(&sr);
}

void biggestGapFromRangeTree(struct rbTree *rt, int chromSize, 
	int *retStart, int *retEnd, int *retSize)
/* Given a range tree filled with data for given chromosome, figure out
 * location and size of biggest gap in data. */
{
struct range *range, *rangeList = rangeTreeList(rt);
if (rangeList == NULL)
    {
    *retStart = 0;
    *retEnd = *retSize = chromSize;
    }
else
    {
    int lastEnd = 0;
    int biggestSize = 0, biggestStart = 0, biggestEnd = 0;
    for (range = rangeList; range != NULL; range = range->next)
        {
	int size = range->start - lastEnd;
	if (size > biggestSize)
	    {
	    biggestSize = size;
	    biggestStart = lastEnd;
	    biggestEnd = range->start;
	    }
	lastEnd = range->end;
	if (range->next == NULL)
	    {
	    size = chromSize - lastEnd;
	    if (size > biggestSize)
	        {
		biggestSize = size;
		biggestStart = lastEnd;
		biggestEnd = chromSize;
		}
	    }
	}
    *retStart = biggestStart;
    *retEnd = biggestEnd;
    *retSize = biggestSize;
    }
}

void addGaps(struct sqlConnection *conn, char *chrom, struct rbTree *rt)
/* Add gaps to range tree. */
{
char query[256];
safef(query, sizeof(query), "select chromStart, chromEnd from gap where chrom='%s'", chrom);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    rangeTreeAdd(rt, sqlUnsigned(row[0]), sqlUnsigned(row[1]));
    }
sqlFreeResult(&sr);
}

void printBiggestGap(char *database, struct sqlConnection *conn, 
	struct slName *chromList, struct hash *chromHash, char *track)
/* Look up track in database, figure out which type it is, call
 * appropriate biggest gap finder, and then print result. */
{
struct trackDb *tdb = hTrackInfo(conn, track);
struct hTableInfo *hti = hFindTableInfo(database, chromList->name, tdb->table);
char *typeWord = cloneFirstWord(tdb->type);
char *biggestChrom = NULL;
int biggestSize = 0, biggestStart = 0, biggestEnd = 0;
struct slName *chrom;
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    if (!allParts && strchr(chrom->name, '_'))	// Generally skip weird chroms
        continue;
    if (female && sameString(chrom->name, "chrY"))
        continue;
    int chromSize = hashIntVal(chromHash, chrom->name);
    struct rbTree *rt = rangeTreeNew();
    int start = 0, end = 0, size = 0;
    if (sameString("bigWig", typeWord))
	bigWigBiggestGap(tdb, conn, chrom->name, chromSize, rt);
    else if (sameString("bigBed", typeWord))
	bigBedBiggestGap(tdb, conn, chrom->name, chromSize, rt);
    else
        tableBiggestGap(hti, tdb, conn, chrom->name, chromSize, rt);
    if (rt->n > 0)	// Want to keep completely uncovered chromosome uncovered
	addGaps(conn, chrom->name, rt);
    biggestGapFromRangeTree(rt, chromSize, &start, &end, &size);
    if (size > biggestSize)
        {
	biggestSize = size;
	biggestStart = start;
	biggestEnd = end;
	biggestChrom = chrom->name;
	}
    rangeTreeFree(&rt);
    }
printf("%s\t%s:%d-%d\t", track, biggestChrom, biggestStart+1, biggestEnd);
printLongWithCommas(stdout, biggestSize);
putchar('\n');
freez(&typeWord);
}

void checkCoverageGaps(char *database, int trackCount, char *tracks[])
/* checkCoverageGaps - Check for biggest gap in coverage for a list of tracks.. */
{
struct slName *chromList = hChromList(database);
struct hash *chromHash = hChromSizeHash(database);
struct sqlConnection *conn = sqlConnect(database);
int i;
for (i=0; i<trackCount; ++i)
    {
    char *track = tracks[i];
    printBiggestGap(database, conn, chromList, chromHash, track);
    }
sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
allParts = optionExists("allParts");
female = optionExists("female");
if (argc < 3)
    usage();
checkCoverageGaps(argv[1], argc-2, argv+2);
return 0;
}
