/* sumStats - show summary statistics output. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "obscure.h"
#include "htmshell.h"
#include "cheapcgi.h"
#include "cart.h"
#include "jksql.h"
#include "hdb.h"
#include "web.h"
#include "hui.h"
#include "hdb.h"
#include "bits.h"
#include "trackDb.h"
#include "customTrack.h"
#include "hgSeq.h"
#include "agpGap.h"
#include "portable.h"
#include "botDelay.h"
#include "hgTables.h"
#include "wikiTrack.h"

static char const rcsid[] = "$Id: sumStats.c,v 1.22 2008/05/27 23:48:28 hiram Exp $";

long long basesInRegion(struct region *regionList, int limit)
/* Count up all bases in regions to limit number of regions, 0 == no limit */
{
long long total = 0;
struct region *region;
int regionCount = 0;

for (region = regionList;
	(region != NULL) && (!(limit && (regionCount >= limit)));
	    region = region->next, ++regionCount)
    {
    total += region->end - region->start;
    }
return total;
}

long long gapsInRegion(struct sqlConnection *conn, struct region *regionList,
	int limit)
/* Return count of gaps in all regions to limit number of regions,
 *	limit=0 == no limit, do them all
 */
{
long long gapBases = 0;
char *splitTable = chromTable(conn, "gap");
int regionCount = 0;

if (sqlTableExists(conn, splitTable))
    {
    struct region *region;
    for (region = regionList;
	    (region != NULL) && (!(limit && (regionCount >= limit)));
		region = region->next, ++regionCount)
	{
	int rowOffset;
	char **row;
	struct agpGap gap;
	struct sqlResult *sr = hRangeQuery(conn, "gap", 
		region->chrom, region->start, region->end, 
		NULL, &rowOffset);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    agpGapStaticLoad(row + rowOffset, &gap);
	    if (gap.chromStart < region->start) gap.chromStart = region->start;
	    if (gap.chromEnd > region->end) gap.chromEnd = region->end;
	    gapBases += gap.chromEnd - gap.chromStart;
	    }
	sqlFreeResult(&sr);
	}
    }
freez(&splitTable);
return gapBases;
}

struct covStats
/* Coverage stats on one region. */
    {
    struct covStats *next;
    struct region *region;  /* Region this refers to. */
    Bits *bits;	    	    /* Bitmap for region. */
    long long basesCovered; /* No double counting of bases. */
    long long sumBases;	    /* Sum of bases in items. */
    int itemCount;	    /* Number of items. */
    int minBases, maxBases; /* Min/max size of items. */
    };

struct covStats *covStatsNew(struct region *region)
/* Get new covStats. */
{
struct covStats *cov;
AllocVar(cov);
cov->region = region;
cov->minBases = BIGNUM;
if (region != NULL)
    {
    cov->bits = bitAlloc(region->end - region->start);
    }
return cov;
}

static void covAddRange(struct covStats *cov, int start, int end)
/* Add range to stats. */
{
struct region *r = cov->region;
if (end > r->start && start < r->end)
    {
    int unclippedSize, size;
    unclippedSize = end - start;
    if (start < r->start) start = r->start;
    if (end > r->end) end = r->end;
    size = end-start;
    bitSetRange(cov->bits, start - r->start, size);
    cov->sumBases += size;
    cov->itemCount += 1;
    if (unclippedSize > cov->maxBases) cov->maxBases = unclippedSize;
    if (unclippedSize < cov->minBases) cov->minBases = unclippedSize;
    }
}

void covStatsFree(struct covStats **pCov)
/* Free up memory associated with covStats. */
{
struct covStats *cov = *pCov;
if (cov != NULL)
    {
    bitFree(&cov->bits);
    freez(pCov);
    }
}

void covStatsFreeList(struct covStats **pList)
/* Free a list of dynamically allocated covStats's */
{
struct covStats *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    covStatsFree(&el);
    }
*pList = NULL;
}

struct covStats *covStatsSum(struct covStats *list)
/* Add together list of covStats. */
{
struct covStats *tot, *cov;
tot = covStatsNew(NULL);
for (cov = list; cov != NULL; cov = cov->next)
    {
    if (cov->itemCount > 0)
        {
	tot->basesCovered += cov->basesCovered;
	tot->sumBases += cov->sumBases;
	tot->itemCount += cov->itemCount;
	if (tot->minBases > cov->minBases) tot->minBases = cov->minBases;
	if (tot->maxBases < cov->maxBases) tot->maxBases = cov->maxBases;
	}
    }
return tot;
}

static void basesCoveredFromBits(struct covStats *cov)
/* Calculate basesCovered from bits for each item on list. */
{
int regionSize = cov->region->end - cov->region->start;
cov->basesCovered = bitCountRange(cov->bits, 0, regionSize);
bitFree(&cov->bits);
}

static struct covStats *calcSpanOverRegion(struct region *region,
	struct bed *bedList)
/* Get stats calculated over regions. */
{
struct covStats *cov = covStatsNew(region);
struct bed *bed;

for (bed = bedList; bed != NULL; bed = bed->next)
    covAddRange(cov, bed->chromStart, bed->chromEnd);
basesCoveredFromBits(cov);
return cov;
}

static struct covStats *calcBlocksOverRegion(struct region *region,
	struct bed *bedList)
/* Get stats calculated over blocks in regions. */
{
struct covStats *cov = covStatsNew(region);
struct bed *bed;

for (bed = bedList; bed != NULL; bed = bed->next)
    {
    int i, blockCount = bed->blockCount;
    for (i=0; i<blockCount; ++i)
	{
	int start = bed->chromStarts[i] + bed->chromStart;
	covAddRange(cov, start, start + bed->blockSizes[i]);
	}
    }
basesCoveredFromBits(cov);
return cov;
}

void percentStatRow(char *label, long long p, long long q)
/* Print label, p, and p/q */
{
hPrintf("<TR><TD>%s</TD><TD ALIGN=RIGHT>", label);
printLongWithCommas(stdout, p);
if (q != 0)
    hPrintf(" (%3.2f%%)", 100.0 * p/q);
hPrintf("</TD></TR>\n");
}

void numberStatRow(char *label, long long x)
/* Print label, x in table. */
{
hPrintf("<TR><TD>%s</TD><TD ALIGN=RIGHT>", label);
printLongWithCommas(stdout, x);
hPrintf("</TD></TR>\n");
}

void floatStatRow(char *label, double x)
/* Print label, x in table. */
{
hPrintf("<TR><TD>%s</TD><TD ALIGN=RIGHT>", label);
hPrintf("%3.2f", x);
hPrintf("</TD></TR>\n");
}


void stringStatRow(char *label, char *val)
/* Print label, string value. */
{
hPrintf("<TR><TD>%s</TD><TD ALIGN=RIGHT>%s</TD></TR>\n", label, val);
}


void doSummaryStatsBed(struct sqlConnection *conn)
/* Put up page showing summary stats for track that is in database
 * or that is bed-format custom. */
{
struct bed *bedList = NULL;
struct region *regionList = getRegions(), *region;
char *regionName = getRegionName();
long long regionSize = 0, gapTotal = 0, realSize = 0;
long startTime, midTime, endTime;
long loadTime = 0, calcTime = 0, freeTime = 0;
struct covStats *itemCovList = NULL, *blockCovList = NULL, *cov;
int itemCount = 0;
struct hTableInfo *hti = getHti(database, curTable);
int minScore = BIGNUM, maxScore = -BIGNUM;
long long sumScores = 0;
boolean hasBlocks = hti->hasBlocks;
boolean hasScore = (hti->scoreField[0] != 0);
int fieldCount;

htmlOpen("%s (%s) Summary Statistics", curTableLabel(), curTable);

for (region = regionList; region != NULL; region = region->next)
    {
    struct lm *lm = lmInit(64*1024);
    startTime = clock1000();
    bedList = cookedBedList(conn, curTable, region, lm, &fieldCount);
    if (fieldCount < 12)
         hasBlocks = FALSE;
    if (fieldCount < 5)
         hasScore = FALSE;
    midTime = clock1000();
    loadTime += midTime - startTime;

    if (bedList != NULL)
	{
	itemCount += slCount(bedList);
	regionSize += region->end - region->start;
	cov = calcSpanOverRegion(region, bedList);
	slAddHead(&itemCovList, cov);
	if (hasBlocks)
	    {
	    cov = calcBlocksOverRegion(region, bedList);
	    slAddHead(&blockCovList, cov);
	    }
	if (hti->scoreField[0] != 0)
	    {
	    struct bed *bed;
	    for (bed = bedList; bed != NULL; bed = bed->next)
		{
		int score = bed->score;
		if (score < minScore) minScore = score;
		if (score > maxScore) maxScore = score;
		sumScores += score;
		}
	    }
	}
    endTime = clock1000();
    calcTime += endTime - midTime;
    lmCleanup(&lm);
    bedList = NULL;
    freeTime  += clock1000() - endTime;
    }

regionSize = basesInRegion(regionList, 0);
gapTotal = gapsInRegion(conn, regionList, 0);
realSize = regionSize - gapTotal;


hTableStart();
startTime = clock1000();
numberStatRow("item count", itemCount);
if (itemCount > 0)
    {
    cov = covStatsSum(itemCovList);
    percentStatRow("item bases", cov->basesCovered, realSize);
    percentStatRow("item total", cov->sumBases, realSize);
    numberStatRow("smallest item", cov->minBases);
    numberStatRow("average item", round((double)cov->sumBases/cov->itemCount));
    numberStatRow("biggest item", cov->maxBases);
    }

if (hasBlocks && itemCount > 0)
    {
    cov = covStatsSum(blockCovList);
    hPrintf("<TR><TD>block count</TD><TD ALIGN=RIGHT>");
    printLongWithCommas(stdout, cov->itemCount);
    hPrintf("</TD></TR>\n");
    percentStatRow("block bases", cov->basesCovered, realSize);
    percentStatRow("block total", cov->sumBases, realSize);
    numberStatRow("smallest block", cov->minBases);
    numberStatRow("average block", round((double)cov->sumBases/cov->itemCount));
    numberStatRow("biggest block", cov->maxBases);
    }

if (hasScore != 0 && itemCount > 0 && sumScores != 0)
    {
    numberStatRow("smallest score", minScore);
    numberStatRow("average score", round((double)sumScores/itemCount));
    numberStatRow("biggest score", maxScore);
    }
hTableEnd();

/* Show region and time stats part of stats page. */
webNewSection("Region and Timing Statistics");
hTableStart();
stringStatRow("region", regionName);
numberStatRow("bases in region", regionSize);
numberStatRow("bases in gaps", gapTotal);
floatStatRow("load time", 0.001*loadTime);
floatStatRow("calculation time", 0.001*calcTime);
floatStatRow("free memory time", 0.001*freeTime);
stringStatRow("filter", (anyFilter() ? "on" : "off"));
stringStatRow("intersection", (anyIntersection() ? "on" : "off"));
hTableEnd();
covStatsFreeList(&itemCovList);
covStatsFreeList(&blockCovList);
htmlClose();
}

void doSummaryStats(struct sqlConnection *conn)
/* Put up page showing summary stats for track. */
{
hgBotDelay();
if (isWiggle(database, curTable))
    doSummaryStatsWiggle(conn);
else if (isChromGraph(curTrack))
    doSummaryStatsChromGraph(conn);
else if (sameWord(curTable,WIKI_TRACK_TABLE))
    doSummaryStatsWikiTrack(conn);
else
    doSummaryStatsBed(conn);
}
