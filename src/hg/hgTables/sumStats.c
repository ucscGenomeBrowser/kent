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
#include "hgTables.h"

static char const rcsid[] = "$Id: sumStats.c,v 1.9 2004/07/23 08:18:12 kent Exp $";

long long basesInRegion(struct region *regionList)
/* Count up all bases in regions. */
{
long long total = 0;
struct region *region;
for (region = regionList; region != NULL; region = region->next)
    {
    if (region->end == 0)
        total += hChromSize(region->chrom);
    else
	total += region->end - region->start;
    }
return total;
}

long long gapsInRegion(struct sqlConnection *conn, struct region *regionList)
/* Return count of gaps in all regions. */
{
long long gapBases = 0;
char *splitTable = chromTable(conn, "gap");
if (sqlTableExists(conn, splitTable))
    {
    struct region *region;
    for (region = regionList; region != NULL; region = region->next)
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
    int end = region->end;
    if (end == 0) 
	end = hChromSize(region->chrom);
    cov->bits = bitAlloc(end - region->start);
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

void covStatsInitOnRegions(
	struct region *regionList, 
	struct covStats **retList,
	struct hash **retHash)	/* Hash is keyed by chrom */
/* Make up list/hash of covsStats, one for each region. */
{
struct hash *hash = newHash(16);
struct covStats *list = NULL, *cov;
struct region *region;

for (region = regionList; region != NULL; region = region->next)
    {
    cov = covStatsNew(region);
    hashAdd(hash, region->chrom, cov);
    slAddHead(&list, cov);
    }
slReverse(&list);
*retList = list;
*retHash = hash;
}

static void basesCoveredFromBits(struct covStats *list)
/* Calculate basesCovered from bits for each item on list. */
{
struct covStats *cov;
for (cov = list; cov != NULL; cov = cov->next)
    {
    int regionSize = cov->region->end - cov->region->start;
    cov->basesCovered = bitCountRange(cov->bits, 0, regionSize);
    }
}

static struct covStats *calcSpanOverRegions(struct region *regionList,
	struct bed *bedList)
/* Get stats calculated over regions. */
{
struct hash *chromHash;
struct covStats *covList, *cov;
struct bed *bed;
struct hashEl *hel;

covStatsInitOnRegions(regionList, &covList, &chromHash);
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    hel = hashLookup(chromHash, bed->chrom);
    while (hel != NULL)
        {
	cov = hel->val;
	covAddRange(cov, bed->chromStart, bed->chromEnd);
	hel = hashLookupNext(hel);
	}
    }
basesCoveredFromBits(covList);
freeHash(&chromHash);
return covList;
}

static struct covStats *calcBlocksOverRegions(struct region *regionList,
	struct bed *bedList)
/* Get stats calculated over blocks in regions. */
{
struct hash *chromHash;
struct covStats *covList, *cov;
struct bed *bed;
struct hashEl *hel;

covStatsInitOnRegions(regionList, &covList, &chromHash);
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    hel = hashLookup(chromHash, bed->chrom);
    while (hel != NULL)
        {
	int i;
	cov = hel->val;
	for (i=0; i<bed->blockCount; ++i)
	    {
	    int start = bed->chromStarts[i] + bed->chromStart;
	    covAddRange(cov, start, start + bed->blockSizes[i]);
	    }
	hel = hashLookupNext(hel);
	}
    }
basesCoveredFromBits(covList);
freeHash(&chromHash);
return covList;
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
hPrintf("</TD><TR>\n");
}

void floatStatRow(char *label, double x)
/* Print label, x in table. */
{
hPrintf("<TR><TD>%s</TD><TD ALIGN=RIGHT>", label);
hPrintf("%3.2f", x);
hPrintf("</TD><TR>\n");
}


void stringStatRow(char *label, char *val)
/* Print label, string value. */
{
hPrintf("<TR><TD>%s</TD><TD ALIGN=RIGHT>%s</TD></TR>\n", label, val);
}


static void doSummaryStatsBed(struct sqlConnection *conn)
/* Put up page showing summary stats for track that is in database
 * or that is bed-format custom. */
{
struct trackDb *track = curTrack;
struct bed *bedList = NULL;
struct region *regionList = getRegions();
#ifdef OLD
struct region *regionList = getRegionsWithChromEnds();
#endif
char *regionName = getRegionName();
long long regionSize, gapTotal, realSize;
long startTime, loadTime, calcTime;
struct covStats *covList, *cov;
int itemCount;
struct hTableInfo *hti = getHti(database, track->tableName);
struct lm *lm = lmInit(64*1024);


htmlOpen("%s (%s) Summary Statistics", track->shortLabel, track->tableName);
cartSaveSession(cart);
startTime = clock1000();
bedList = getAllIntersectedBeds(conn, track, lm);
regionFillInChromEnds(regionList);
regionSize = basesInRegion(regionList);
gapTotal = gapsInRegion(conn, regionList);
realSize = regionSize - gapTotal;
loadTime = clock1000() - startTime;


hTableStart();
startTime = clock1000();
itemCount = slCount(bedList);
numberStatRow("item count", itemCount);
if (itemCount > 0)
    {
    covList = calcSpanOverRegions(regionList, bedList);
    cov = covStatsSum(covList);
    percentStatRow("item bases", cov->basesCovered, realSize);
    percentStatRow("item total", cov->sumBases, realSize);
    numberStatRow("smallest item", cov->minBases);
    numberStatRow("average item", round((double)cov->sumBases/cov->itemCount));
    numberStatRow("biggest item", cov->maxBases);
    covStatsFreeList(&covList);
    }

if (hti->hasBlocks && itemCount > 0)
    {
    covList = calcBlocksOverRegions(regionList, bedList);
    cov = covStatsSum(covList);
    hPrintf("<TR><TD>block count</TD><TD ALIGN=RIGHT>");
    printLongWithCommas(stdout, cov->itemCount);
    hPrintf("</TD><TR>\n");
    percentStatRow("block bases", cov->basesCovered, realSize);
    percentStatRow("block total", cov->sumBases, realSize);
    numberStatRow("smallest block", cov->minBases);
    numberStatRow("average block", round((double)cov->sumBases/cov->itemCount));
    numberStatRow("biggest block", cov->maxBases);
    covStatsFreeList(&covList);
    }

if (hti->scoreField[0] != 0 && itemCount > 0)
    {
    struct bed *bed;
    int minScore = BIGNUM, maxScore = -BIGNUM;
    long long sumScores = 0;
    for (bed = bedList; bed != NULL; bed = bed->next)
        {
	int score = bed->score;
	if (score < minScore) minScore = score;
	if (score > maxScore) maxScore = score;
	sumScores += score;
	}
    if (sumScores != 0)
        {
	numberStatRow("smallest score", minScore);
	numberStatRow("average score", round((double)sumScores/itemCount));
	numberStatRow("biggest score", maxScore);
	}
    }
calcTime = clock1000() - startTime;
hTableEnd();

/* Show region and time stats part of stats page. */
webNewSection("Region and Timing Statistics");
hTableStart();
stringStatRow("region", regionName);
numberStatRow("bases in region", regionSize);
numberStatRow("bases in gaps", gapTotal);
floatStatRow("load time", 0.001*loadTime);
floatStatRow("calculation time", 0.001*calcTime);
stringStatRow("filter", (anyFilter() ? "on" : "off"));
stringStatRow("intersection", (anyIntersection() ? "on" : "off"));
hTableEnd();
htmlClose();
lmCleanup(&lm);
}

void doSummaryStats(struct sqlConnection *conn)
/* Put up page showing summary stats for track. */
{
if (isWiggle(database, curTrack->tableName))
    doSummaryStatsWiggle(conn);
else
    doSummaryStatsBed(conn);
}


