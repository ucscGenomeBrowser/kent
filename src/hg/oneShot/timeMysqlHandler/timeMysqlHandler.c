/* timeMysqlHandler - Compare performance of mysql HANDLER vs. SELECT for big & small queries. */
#include "common.h"
#include "options.h"
#include "annoRow.h"
#include "binRange.h"
#include "jksql.h"
#include "hdb.h"

/* The annoGrator infrastructure depends on two things for speed:
 * - data sources provide rows strictly sorted by (chrom, chromStart)
 * - data sources are streaming, i.e. they start providing rows more or less
 *   immediately instead of reading everything into memory and then providing it.
 *
 * Our database tables are sorted by (chrom, chromStart) and QA checks for that.
 * However, when an index is used to get rows only for a certain position range,
 * rows are returned by the server in index order, and for tables that use the
 * (chrom,bin) index, rows are returned in increasing bin order which destroys
 * the natural order's chromStart sorting.
 *
 * To restore the sorting, one can add "ORDER BY chromStart" to the SELECT,
 * but then since the server must sort all results, it has to load all the
 * results first.  For very large tables like snp137 with 56M rows, that takes
 * too long.  And snp137 in particular is critical for variant annoGrator.
 *
 * But we can use some heuristics to start returning sorted results a lot sooner.
 * Especially for snp137, most items are in the finest-sized bins, which are the
 * highest numerically.  Very large items, or items that happen to straddle a
 * bin boundary, are in coarser bins which are lowest numerically.  So results
 * from a position-range query typically include a few coarse-bin items followed
 * by many finest-bin items.  Once we start getting finest-bin items, we can be
 * sure that the results from that point onward will be sorted by chromStart.
 * So we can accumulate coarse-bin items, and as soon as we get a finest-bin
 * item, we can sort the accumulated coarse-bin item list, and then it's just
 * a linear-time merge-sort to inject those back in at the right points between
 * finest-bin items.
 *
 * Meanwhile, I was looking for the speediest streaming interface to MySQL and
 * came across a blog page
 *   http://marksverbiage.blogspot.com/2010/04/streaming-data-from-mysql.html
 * about MySQL's HANDLER interface
 *   http://dev.mysql.com/doc/refman/5.1/en/handler.html
 * which sounded intriguing so I wrote this program to compare both time to first
 * result and total time for three styles of query:
 * - SELECT ... ORDER BY chromStart
 * - SELECT ... --> merge-sorting scheme
 * - HANDLER ... --> merge-sorting scheme
 * for three types of position range:
 * - genome-wide [using natural table order, no post-SELECT sorting required]
 * - chr2
 * - an arbitrary 1M region: chr8, 98735498, 99735498
 * for three tables in hg19:
 * - knownGene (small and important)
 * - wgEncodeRegDnaseClustered (almost exactly 1M rows)
 * - snp137 (big and important)
 *
 * Turns out that HANDLER is not blazingly faster than SELECT since we're
 * using mysql_use_result.  Inexplicably, it can be slower for wgEncodeRegDnaseClustered.
 * The big win is the merge-sorting scheme that avoids ORDER BY.
 */

// Global var for option:
boolean writeFiles = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "timeMysqlHandler - Compare performance of mysql's HANDLER vs. SELECT for big & small queries\n"
  "usage:\n"
  "   timeMysqlHandler db\n"
  "       db must be hg19 because table names are hardcoded.\n"
  "options:\n"
  "   -writeFiles       Write output files from each query for testing correctness.\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
    {"writeFiles", OPTION_BOOLEAN},
    {NULL, 0},
};

struct region
    {
    boolean isGenome;
    boolean isWholeChrom;
    char *chrom;
    uint start;
    uint end;
    };

// Hardcoded tables and regions:
static char *tables[] = {
    "knownGene",
    "wgEncodeRegDnaseClustered",
    "snp137",
    NULL
};

static struct region regions[] = {
    { FALSE, FALSE, "chr8", 98735498, 99735498 },  // completely arbitrary 1Mb region
    { FALSE, TRUE,  "chr2", 0, 0 },                // all of chr2
    { TRUE, FALSE, NULL, 0, 0 },                   // whole genome
};
static int regionCount = ArraySize(regions);

long logNewQuery(char *query)
/* Report the start of a query and return a clock time for measuring duration. */
{
printf("----------------------- Beginning new query------------------\n");
printf("-- %s\n", query);
return clock1000();
}

void logFirstResult(long startTime)
{
printf("First result: %ld ms\n", clock1000() - startTime);
}

void logQueryFinish(long startTime, int rowCount)
{
printf("Done:         %ld ms\n", clock1000() - startTime);
printf("Rows:         %d\n\n", rowCount);
}

INLINE void getChromStartEnd(char **row, struct hTableInfo *hti, char **retChrom,
			     uint *retStart, uint *retEnd)
/* Use hti to tell whether this is bed or genePred, and parse coords from row accordingly. */
{
if (sameString(hti->startField, "chromStart"))
    {
    *retChrom = row[0+hti->hasBin];
    *retStart = atoll(row[1+hti->hasBin]);
    *retEnd = atoll(row[2+hti->hasBin]);
    }
else if (sameString(hti->startField, "txStart"))
    {
    *retChrom = row[1+hti->hasBin];
    *retStart = atoll(row[3+hti->hasBin]);
    *retEnd = atoll(row[4+hti->hasBin]);
    }
else
    errAbort("fix getChromStartEnd to support table types other than bed and genePred");
}

int cmpAnnoRowToPos(struct annoRow *aRow, char *chrom, uint start, uint end)
/* Compare an annoRow's position to {chrom, start, end} (for merging). */
{
int dif = strcmp(aRow->chrom, chrom);
if (dif == 0)
    {
    dif = aRow->start - start;
    if (dif == 0)
	dif = aRow->end - end;
    }
return dif;
}

INLINE void dumpRow(FILE *f, char **row, int colCount)
/* If f is non-NULL, write out tab-sep row to f. */
{
if (f != NULL)
    {
    int i;
    for (i = 0;  i < colCount-1;  i++)
	fprintf(f, "%s\t", row[i]);
    fprintf(f, "%s\n", row[i]);
    }
}

INLINE void dumpAnnoRow(FILE *f, struct annoRow *aRow, int colCount)
/* If f is non-NULL, write out tab-sep annoRow data to f. */
{
dumpRow(f, (char **)(aRow->data), colCount);
}

int mergeOneRow(char **row, struct hTableInfo *hti, int colCount, FILE *f, long startTime,
		int rowCount, struct annoRow **pBigItemQueue, boolean *pGotSmallestItemBin)
/* If row is from a big-item bin, add it to bigItemQueue; if it's the first
 * we've seen from a smallest-item bin, sort the bigItemQueue by position and
 * start using it to merge items in with subsequent rows which we expect to
 * be sorted by position. Return the updated rowCount; when we're queueing up
 * a big item for later rowCount does not change, but when we de-queue it
 * rowCount increments by 1 in addition to 1 for each smallest-item bin row. */
{
int bin = atoi(row[0]);
char *chrom = NULL;
uint start = 0, end = 0;
getChromStartEnd(row, hti, &chrom, &start, &end);
if (bin < binOffset(0))
    {
    // big item -- store aside in queue for merging later; rowCount doesn't increment
    if (*pGotSmallestItemBin)
	errAbort("Row %d has large-item bin %d after seeing a smallest-item bin!",
		 rowCount, bin);
    struct annoRow *aRow = annoRowFromStringArray(chrom, start, end, FALSE, row, colCount);
    slAddHead(pBigItemQueue, aRow);
    }
else
    {
    if (!*pGotSmallestItemBin)
	{
	// First smallest item!  Sort *pBigItemQueue in preparation for merging:
	*pGotSmallestItemBin = TRUE;
	slSort(pBigItemQueue, annoRowCmp);
	printf("Large item queue length = %d; first result of merge-sort at %ld ms\n",
	       slCount(*pBigItemQueue), clock1000() - startTime);
	}
    while (*pBigItemQueue && cmpAnnoRowToPos(*pBigItemQueue, chrom, start, end) <= 0)
	{
	// For each item we can merge in, pop it from the queue and increment rowCount:
	struct annoRow *bigItem = *pBigItemQueue;
	dumpAnnoRow(f, bigItem, colCount);
	*pBigItemQueue = bigItem->next;
	rowCount++;
	}
    // Now for the current row:
    dumpRow(f, row, colCount);
    rowCount++;
    }
return rowCount;
}

int closeOutQueue(int colCount, FILE *f, long startTime,
		  struct annoRow **pBigItemQueue, boolean gotSmallestItemBin)
/* Account for any big items remaining to merge after all small items have been processed. */
{
int rowCount = 0;
if (*pBigItemQueue)
    {
    if (!gotSmallestItemBin)
	{
	slSort(pBigItemQueue, annoRowCmp);
	printf("Large item queue length = %d; no small items; first result at %ld ms\n",
	       slCount(*pBigItemQueue), clock1000() - startTime);
	}
    struct annoRow *aRow;
    for (aRow = *pBigItemQueue;  aRow != NULL;  aRow = aRow->next)
	{
	dumpAnnoRow(f, aRow, colCount);
	rowCount++;
	}
    }
return rowCount;
}

int mergeSort(char **row, struct sqlResult *sr, struct hTableInfo *hti,
	      int colCount, FILE *f, long startTime)
/* Expect sr to contain a few items from large-item bins followed by a bunch of items
 * from smallest-item bins.  Save the large items aside, and when we start getting items
 * from a smallest-item bin, merge-sort the large items in to produce output sorted by
 * {chrom, start}.
 * NOTE: this works with standard bin scheme only.  Don't use on opossum (without extending). */
{
if (!hti->hasBin)
    errAbort("Don't call me unless table has bin.");
struct annoRow *bigItemQueue = NULL;
boolean gotSmallestItemBin = FALSE;
int rowCount = 0;
for (; row != NULL; row = sqlNextRow(sr))
    rowCount = mergeOneRow(row, hti, colCount, f, startTime, rowCount,
			   &bigItemQueue, &gotSmallestItemBin);
rowCount += closeOutQueue(colCount, f, startTime, &bigItemQueue, gotSmallestItemBin);
return rowCount;
}

void timeQueryConn(struct sqlConnection *conn, char *query, struct hTableInfo *hti, FILE *f)
/* Measure how long it takes to get first result and how long it takes
 * to get all results; if hti is non-NULL and hasBin, perform merge-sorting of
 * large items that appear first with small items that appear later
 * (smallest-item bins are highest numerically). */
{
int rowCount = 0;
long startTime = logNewQuery(query);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row = sqlNextRow(sr);
logFirstResult(startTime);
int colCount = sqlCountColumns(sr);
if (hti == NULL || !hti->hasBin)
    {
    // no need to sort output, just count rows
    for (; row != NULL; row = sqlNextRow(sr))
	{
	dumpRow(f, row, colCount);
	rowCount++;
	}
    }
else
    rowCount = mergeSort(row, sr, hti, colCount, f, startTime);
logQueryFinish(startTime, rowCount);
sqlFreeResult(&sr);
}

void timeQuery(char *db, char *query, struct hTableInfo *hti, FILE *f)
/* Measure how long it takes to get first result and how long it takes
 * to get all results; if hti is non-NULL and hasBin, perform merge-sorting of
 * large items that appear first with small items that appear later
 * (smallest-item bins are highest numerically). */
{
struct sqlConnection *conn = hAllocConn(db);
timeQueryConn(conn, query, hti, f);
hFreeConn(&conn);
}

struct dyString *regionSelect(struct region *region, struct hTableInfo *hti)
/* Return a dyString with a SELECT SQL_NO_CACHE query for table in region. */
{
char *table = hti->rootName; // not supporting split tables
struct dyString *dy = sqlDyStringCreate("select SQL_NO_CACHE * from %s ", table);
if (! region->isGenome)
    {
    if (sameString(table, "knownGene"))
//#*** Presence of chromEnd index for knownGene screws up sorting for region=chr2, ugh!
	dyStringAppend(dy, "IGNORE INDEX (chrom_2) ");
    dyStringPrintf(dy, "where %s = '%s'", hti->chromField, region->chrom);
    if (! region->isWholeChrom)
	{
	dyStringAppend(dy, " and ");
	if (hti->hasBin)
	    hAddBinToQuery(region->start, region->end, dy);
	dyStringPrintf(dy, "%s > %u and %s < %u ",
		       hti->endField, region->start, hti->startField, region->end);
	}
    }
return dy;
}

FILE *outFileForQuery(char *db, struct hTableInfo *hti, struct region *region, char *type)
/* Return a file for writing whose name reflects the query that produced its contents. */
{
struct dyString *dy = dyStringCreate("%s.%s.", db, hti->rootName);
if (region->isGenome)
    dyStringAppend(dy, "genome.");
else if (region->isWholeChrom)
    dyStringPrintf(dy, "%s.", region->chrom);
else
    dyStringPrintf(dy, "%s:%u-%u.", region->chrom, region->start, region->end);
dyStringAppend(dy, type);
dyStringAppend(dy, ".tab");
return mustOpen(dy->string, "w");
}

void selectOrderQuery(char *db, struct hTableInfo *hti, struct region *region)
/* Time a query using 'SELECT SQL_NO_CACHE ... ORDER BY chromStart'.
 * If region->isGenome, skip they "order by" since tables' "natural order"
 * (as opposed to index order) should be sorted by position already. */
{
FILE *f = NULL;
if (writeFiles)
    f = outFileForQuery(db, hti, region, region->isGenome ? "selectRaw" : "selectOrderBy");
struct dyString *dy = regionSelect(region, hti);
if (!region->isGenome)
    dyStringPrintf(dy, " order by %s", hti->startField);
timeQuery(db, dy->string, NULL, f);
carefulClose(&f);
}

void selectQueryThenMergeBins(char *db, struct hTableInfo *hti, struct region *region)
/* Time a query using 'SELECT SQL_NO_CACHE ...', followed by merge-sorting
 * large-bin items with small-bin items to get {chrom, chromStart} order. */
{
FILE *f = NULL;
if (writeFiles)
    f = outFileForQuery(db, hti, region, "selectMerge");
struct dyString *dy = regionSelect(region, hti);
timeQuery(db, dy->string, hti, f);
carefulClose(&f);
}

void calcBinsStandard(uint start, uint end, uint *startBins, uint *endBins,
		      boolean selfContained)
/* Given a range's start and end, use the standard binning scheme to
 * populate arrays containing each level's bin that overlaps start or end,
 * respectively, in increasing bin order. */
{
int bFirstShift = binFirstShift(), bNextShift = binNextShift();
int startBin = (start>>bFirstShift), endBin = ((end-1)>>bFirstShift);
int i, levels = binLevels();

// Unless this is going to be added by calcBinsExtended, we also need
// the extended scheme's analog of bin 0:
if (selfContained)
    {
    printf("startBins[0] = %d\n", _binOffsetOldToExtended);
    printf("  endBins[0] = %d\n", _binOffsetOldToExtended);
    startBins[0] = endBins[0] = _binOffsetOldToExtended;
    }

// Result is in increasing numeric order, so work backwards from highest
// bin numbers to lowest:
for (i=levels-1; i >= 0; i--)
    {
    int offset = binOffset(levels-1-i);
    int ix = i;
    if (selfContained)
	ix++;
    startBins[ix] = offset + startBin;
    endBins[ix] = offset + endBin;
    printf("startBins[%d] = %d + %u = %u\n", ix, offset, startBin, startBins[ix]);
    printf("  endBins[%d] = %d + %u = %u\n", ix, offset, endBin, endBins[ix]);
    startBin >>= bNextShift;
    endBin >>= bNextShift;
    }
}

void calcBinsExtended(uint start, uint end, uint *startBins, uint *endBins)
/* Given a range's start and end, use the extended binning scheme to
 * populate arrays containing each level's bin that overlaps start or end,
 * respectively, in increasing bin order. */
{
int bFirstShift = binFirstShift(), bNextShift = binNextShift();
int startBin = (start>>bFirstShift), endBin = ((end-1)>>bFirstShift);
int i, levels = binLevelsExtended();
int startLevel = 0;
if (start < BINRANGE_MAXEND_512M)
    {
    calcBinsStandard(start, BINRANGE_MAXEND_512M, startBins, endBins, FALSE);
    startLevel = binLevels();
    }

int totalLevels = startLevel + levels;
for (i=totalLevels-1; i >= startLevel; i--)
    {
    int offset = binOffsetExtended(totalLevels-1-i);
    startBins[i] = offset + startBin;
    endBins[i] = offset + endBin;
    printf("startBins[%d] = %d + %u = %u\n", i, offset, startBin, startBins[i]);
    printf("  endBins[%d] = %d + %u = %u\n", i, offset, endBin, endBins[i]);
    startBin >>= bNextShift;
    endBin >>= bNextShift;
    }
}

void calcBins(uint start, uint end,
	      uint **retStartBins, uint **retEndBins, uint *retNumLevels)
/* Given a range's start and end, figure out how many bin levels apply
 * (standard or extended) and create arrays of that length containing each level's
 * bin that overlaps start or end, respectively, in increasing bin order. */
{
int numLevels = 0;
uint *startBins = NULL, *endBins = NULL;
if (end <= BINRANGE_MAXEND_512M)
    {
    numLevels = binLevels() + 1;
    AllocArray(startBins, numLevels);
    AllocArray(endBins, numLevels);
    calcBinsStandard(start, end, startBins, endBins, TRUE);
    }
else
    {
    numLevels = binLevelsExtended();
    if (start < BINRANGE_MAXEND_512M)
	numLevels += binLevels();
    AllocArray(startBins, numLevels);
    AllocArray(endBins, numLevels);
    calcBinsExtended(start, end, startBins, endBins);
    }
*retNumLevels = numLevels;
*retStartBins = startBins;
*retEndBins = endBins;
}

// It would be nice to have a struct sqlHandler that remembers its table
// so caller doesn't have to keep passing in correct table name.
// Would also be smart to know about indices, e.g. do we have (chrom,bin),
// or (chrom, chromStart), or (tName,..) etc
// Ultimately instead of returning sqlResults, handlerNextRow would be
// a nicer interface, to smooth over the succession of queries required
// to get multi-bin results for one region.  Merge-sorting could still
// happen on top of that -- just track the bins in the returned rows.

struct sqlConnection *handlerOpen(char *db, char *table)
/* Return a mysql connection devoted to being the HANDLER connection for table;
 * close connection with handlerClose. */
{
struct sqlConnection *conn = hAllocConn(db);
char query[512];
sqlSafef(query, sizeof(query), "handler %s open", table);
long startTime = clock1000();
sqlUpdate(conn, query);
printf("Handler open: %ld ms\n", clock1000() - startTime);
return conn;
}

void handlerClose(struct sqlConnection **pConn, char *table)
/* Check and close a connection created by handlerOpen. */
{
if (pConn == NULL)
    return;
char query[512];
sqlSafef(query, sizeof(query), "handler %s close", table);
long startTime = clock1000();
sqlUpdate(*pConn, query);
printf("Handler close: %ld ms\n", clock1000() - startTime);
hFreeConn(pConn);
}

void timeHandlerChromBin(struct sqlConnection *conn, struct hTableInfo *hti,
			 char *chrom, uint start, uint end, FILE *f)
/* Like timeQueryConn for a sequence of (chrom,bin) handler queries, but
 * also checks to see if we have run past the end of the region. */
{
char query[512];
// get arrays of start and end bins.
uint *startBins = NULL, *endBins = NULL, numLevels = 0;
calcBins(start, end, &startBins, &endBins, &numLevels);
int totalRowCount = 0;
long overallStartTime = clock1000();
struct annoRow *bigItemQueue = NULL;
boolean gotSmallestItemBin = FALSE;
int colCount = 0;
// for each level, fire off a 'read (chrom, bin)'
uint i;
for (i = 0;  i < numLevels;  i++)
    {
    int limit = 100;
    boolean first = TRUE, doneWithLevel = FALSE;
    while (!doneWithLevel)
	{
	if (first)
	    sqlSafef(query, sizeof(query), "handler %s read %s >= ('%s', %u) limit %d",
		  hti->rootName, hti->chromField, chrom, startBins[i], limit);
	else
	    {
	    if (limit < 10000)
		limit *= 10;
	    sqlSafef(query, sizeof(query), "handler %s read %s next limit %d",
		  hti->rootName, hti->chromField, limit);
	    }
	int rowCount = 0;
	long startTime = logNewQuery(query);
	struct sqlResult *sr = sqlGetResult(conn, query);
	colCount = sqlCountColumns(sr);
	char **row = sqlNextRow(sr);
	logFirstResult(startTime);
	// as we keep getting rows, watch for changes in chrom, bin, and also trim to [start, end).
	// When we get to last level (highest bin), use larger limit and do merge-sort
	// instead of saving rows up for merge-sort.
	for (; row != NULL; row = sqlNextRow(sr))
	    {
	    if (doneWithLevel)
		continue; // still need to keep calling sqlNextRow until it returns null
	    int rowBin = atoi(row[0]);
	    char *rowChrom = NULL;
	    uint rowStart = 0, rowEnd = 0;
	    getChromStartEnd(row, hti, &rowChrom, &rowStart, &rowEnd);
	    if (differentString(rowChrom, chrom) || rowBin > endBins[i] || rowStart >= end)
		doneWithLevel = TRUE;
	    else if (rowEnd > start)
		rowCount = mergeOneRow(row, hti, colCount, f, overallStartTime, rowCount,
				       &bigItemQueue, &gotSmallestItemBin);
	    }
	logQueryFinish(startTime, rowCount);
	first = FALSE;
	totalRowCount += rowCount;
	}
    }
totalRowCount += closeOutQueue(colCount, f, overallStartTime, &bigItemQueue, gotSmallestItemBin);
logQueryFinish(overallStartTime, totalRowCount);
}

void timeHandlerChromStart(struct sqlConnection *conn, struct hTableInfo *hti,
			   char *chrom, uint start, uint end, FILE *f)
/* Like timeQueryConn for a (chrom,chromStart) hander query but also
 * checks to see if we have run past the end of the region. */
{
char query[512];
sqlSafef(query, sizeof(query), "handler %s read %s = ('%s') where %s > %d and %s < %d limit %d",
      hti->rootName, hti->chromField, chrom, hti->endField, start, hti->startField, end, BIGNUM);
int rowCount = 0;
long startTime = logNewQuery(query);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row = sqlNextRow(sr);
logFirstResult(startTime);
int colCount = sqlCountColumns(sr);
for (; row != NULL; row = sqlNextRow(sr))
    {
    char *rowChrom = NULL;
    uint rowStart = 0, rowEnd = 0;
    getChromStartEnd(row, hti, &rowChrom, &rowStart, &rowEnd);
    if (sameString(rowChrom, chrom) && rowStart < end && rowEnd > start)
	{
	dumpRow(f, row, colCount);
	rowCount++;
	}
    }
logQueryFinish(startTime, rowCount);
sqlFreeResult(&sr);
}

void handlerQuery(char *db, struct hTableInfo *hti, struct region *region)
/* Time a query using a sequence of 'HANDLER' commands and a bin-merge list. */
{
char *table = hti->rootName;
long overallStart = clock1000();
struct sqlConnection *conn = handlerOpen(db, table);
char query[512];
if (region->isGenome)
    {
    FILE *f = NULL;
    if (writeFiles)
	f = outFileForQuery(db, hti, region, "handlerRaw");
    sqlSafef(query, sizeof(query), "handler %s read next limit %d", table, BIGNUM);
    timeQueryConn(conn, query, NULL, f);
    carefulClose(&f);
    }
else if (region->isWholeChrom)
    {
    FILE *f = NULL;
    if (writeFiles)
	f = outFileForQuery(db, hti, region, "handlerMerge");
    sqlSafef(query, sizeof(query), "handler %s read chrom = ('%s') limit %d",
	  table, region->chrom, BIGNUM);
    timeQueryConn(conn, query, hti, f);
    carefulClose(&f);
    }
else if (hti->hasBin)
    {
    FILE *f = NULL;
    if (writeFiles)
	f = outFileForQuery(db, hti, region, "handlerMerge");
    timeHandlerChromBin(conn, hti, region->chrom, region->start, region->end, f);
    carefulClose(&f);
    }
else
    {
    FILE *f = NULL;
    if (writeFiles)
	f = outFileForQuery(db, hti, region, "handlerWhere");
    timeHandlerChromStart(conn, hti, region->chrom, region->start, region->end, f);
    carefulClose(&f);
    }
handlerClose(&conn, table);
printf("\nOverall connect + handler queries + close time: %ld ms\n\n\n",
       clock1000() - overallStart);
}


void timesForTable(char *db, char *table, struct region *region)
/* Given a db, table and region, run the different types of queries. */
{
char *chrom = region->chrom;
if (region->chrom == NULL)
    chrom = hDefaultChrom(db);
struct hTableInfo *hti = hFindTableInfo(db, chrom, table);
selectOrderQuery(db, hti, region);
if (! region->isGenome)
    selectQueryThenMergeBins(db, hti, region);
handlerQuery(db, hti, region);
}

void timeMysqlHandler(char *db)
/* timeMysqlHandler - Compare performance of mysql HANDLER vs. SELECT for big & small queries. */
{
int i, j;
for (j = 0;  j < regionCount;  j++)
    for (i = 0;  tables[i] != NULL;  i++)
	timesForTable(db, tables[i], &regions[j]);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
writeFiles = optionExists("writeFiles");
if (argc != 2)
    usage();
if (differentString(argv[1], "hg19"))
    {
    warn("\nSorry, I need tables that are only in hg19. "
	 "The db arg is there only to avoid the help message.\n");
    usage();
    }
timeMysqlHandler(argv[1]);
return 0;
}
