/* bigWig - stuff to handle bigWig in the Table Browser. */

/* Copyright (C) 2012 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "localmem.h"
#include "jksql.h"
#include "cheapcgi.h"
#include "cart.h"
#include "web.h"
#include "bed.h"
#include "hdb.h"
#include "trackDb.h"
#include "customTrack.h"
#include "wiggle.h"
#include "hmmstats.h"
#include "correlate.h"
#include "bbiFile.h"
#include "bigWig.h"
#include "hubConnect.h"
#include "hgTables.h"
#include "mathWig.h"

boolean isBigWig(char *database, char *table, struct trackDb *parent,
	struct customTrack *(*ctLookupName)(char *table))
/* Local test to see if something is big wig.  Handles hub tracks unlike hIsBigWig. */
{
struct trackDb *tdb = hashFindVal(fullTableToTdbHash, table);
if (tdb)
    return tdbIsBigWig(tdb);
else
    return hIsBigWig(database, table, parent, ctLookupName);
}

boolean isBigWigTable(char *table)
/* Return TRUE if table corresponds to a bigWig file. */
{
struct trackDb *tdb = hashFindVal(fullTableToTdbHash, table);
if (tdb)
    return tdbIsBigWig(tdb);
else
    return trackIsType(database, table, curTrack, "bigWig", ctLookupName) || trackIsType(database, table, curTrack, "mathWig", ctLookupName);
}

static char *settingFromCtOrHub(char *table, struct sqlConnection *conn, char *settingName, boolean mustBeInHub)
/* If table is a custom track or hub track, return a setting;
 * otherwise return NULL.  Do a freeMem on returned string when done. */
{
char *settingVal = NULL;
if (isCustomTrack(table))
    {
    struct customTrack *ct = ctLookupName(table);
    if (ct != NULL)
        settingVal = cloneString(trackDbSetting(ct->tdb, settingName));
    }
else
    {
    struct trackDb *tdb = hashFindVal(fullTableToTdbHash, table);
    assert(tdb != NULL);
    settingVal = cloneString(trackDbSetting(tdb, settingName));
    if (mustBeInHub && isHubTrack(table))   // if it's a native track, we'll assume it has a table with the name
        assert(settingVal != NULL);
    }
return settingVal;
}

char *bigFileNameFromCtOrHub(char *table, struct sqlConnection *conn)
/* If table is a custom track or hub track, return the bigDataUrl setting;
 * otherwise return NULL.  Do a freeMem on returned string when done. */
{
return settingFromCtOrHub(table, conn, "bigDataUrl", TRUE);
}

char *bigDataIndexFromCtOrHub(char *table, struct sqlConnection *conn)
/* If table is a custom track or hub track, return the bigDataIndex setting;
 * otherwise return NULL.  Do a freeMem on returned string when done. */
{
return settingFromCtOrHub(table, conn, "bigDataIndex", FALSE);
}

char *bigWigFileName(char *table, struct sqlConnection *conn)
/* Return file name associated with bigWig.  This handles differences whether it's
 * a custom or built-in track.  Do a freeMem on returned string when done. */
{
if (isCustomTrack(table) || isHubTrack(table))
   return bigFileNameFromCtOrHub(table, conn);
struct trackDb *tdb = hashMustFindVal(fullTableToTdbHash, table);
return tdbBigFileName(conn, tdb);
}

struct hTableInfo *bigWigToHti(char *table)
/* Get fields of bigWig into hti structure. */
{
struct hTableInfo *hti;
AllocVar(hti);
hti->rootName = cloneString(table);
hti->isPos= TRUE;
safecpy(hti->chromField, sizeof(hti->chromField), "chrom");
safecpy(hti->startField, sizeof(hti->startField), "chromStart");
safecpy(hti->endField, sizeof(hti->endField), "chromEnd");
return hti;
}

struct bbiInterval *intersectedFilteredBbiIntervalsOnRegion(struct sqlConnection *conn,
	struct bbiFile *bwf, struct region *region, enum wigCompare filterCmp, double filterLl,
	double filterUl, struct lm *lm)
/* Get list of bbiIntervals (more-or-less bedGraph things from bigWig) out of bigWig file
 * and if necessary apply filter and intersection.  Return list which is allocated in lm. */
{
char *chrom = region->chrom;
int chromSize = hChromSize(database, chrom);
struct bbiInterval *iv, *ivList = bigWigIntervalQuery(bwf, chrom, region->start, region->end, lm);

/* Run filter if necessary */
if (filterCmp != wigNoOp_e)
    {
    struct bbiInterval *next, *newList = NULL;
    for (iv = ivList; iv != NULL; iv = next)
        {
	next = iv->next;
	if (wigCompareValFilter(iv->val, filterCmp, filterLl, filterUl))
	    {
	    slAddHead(&newList, iv);
	    }
	}
    slReverse(&newList);
    ivList = newList;
    }

/* Run intersection if necessary */
if (anyIntersection())
    {
    boolean isBpWise = intersectionIsBpWise();
    Bits *bits2 = bitsForIntersectingTable(conn, region, chromSize, isBpWise);
    struct bbiInterval *next, *newList = NULL;
    double moreThresh = cartCgiUsualDouble(cart, hgtaMoreThreshold, 0)*0.01;
    double lessThresh = cartCgiUsualDouble(cart, hgtaLessThreshold, 100)*0.01;
    char *op = cartString(cart, hgtaIntersectOp);
    for (iv = ivList; iv != NULL; iv = next)
	{
	next = iv->next;
	int start = iv->start;
	int size = iv->end - start;
	int overlap = bitCountRange(bits2, start, size);
	if (isBpWise)
	    {
	    if (overlap == size)
	        {
		slAddHead(&newList, iv);
		}
	    else if (overlap > 0)
	        {
		/* Here we have to break things up. */
		double val = iv->val;
		struct bbiInterval *partIv = iv;	// Reuse memory for first interval
		int s = iv->start, end = iv->end;
		for (;;)
		    {
		    s = bitFindSet(bits2, s, end);
		    if (s >= end)
		        break;
		    int bitsSet = bitFindClear(bits2, s, end) - s;
		    if (partIv == NULL)
			lmAllocVar(lm, partIv);
		    partIv->start = s;
		    partIv->end = s + bitsSet;
		    partIv->val = val;
		    slAddHead(&newList, partIv);
		    partIv = NULL;
		    s += bitsSet;
		    if (s >= end)
		        break;
		    }
		}
	    }
	else
	    {
	    double coverage = (double)overlap/size;
	    if (intersectOverlapFilter(op, moreThresh, lessThresh, coverage))
		{
		slAddHead(&newList, iv);
		}
	    }
	}
    slReverse(&newList);
    ivList = newList;
    bitFree(&bits2);
    }

return ivList;
}

void getWigFilter(char *database, char *table, enum wigCompare *retCmp, double *retLl, double *retUl)
/* Get wiggle filter variables from cart and convert them into numbers. */
{
char *dataConstraint;
enum wigCompare cmp = wigNoOp_e;
checkWigDataFilter(database, curTable, &dataConstraint, retLl, retUl);
if (dataConstraint != NULL)
    cmp = wigCompareFromString(dataConstraint);
*retCmp = cmp;
}

static void valuesToVector(double *values, struct dataVector *dv, int start)
/* Move an array of doubles into a dataVector. */
{
int ii;

for(ii=0; ii < dv->maxCount; ii++)
    {
    dv->value[ii] = values[ii];
    dv->position[ii] = start + ii;
    }
dv->count = dv->maxCount;
}

int mathWigOutRegion(struct trackDb *track, char *table, struct sqlConnection *conn,
			     struct region *region, int maxOut,
			     enum wigOutputType wigOutType)
/* Write out mathWig for region, doing intersecting and filtering as need be. */
{
int resultCount = 0;
struct dataVector *dv = dataVectorNew(region->chrom, region->end - region->start);
char *equation = cloneString(trackDbSetting(track, "mathDataUrl"));
double *values = mathWigGetValues(database, equation, region->chrom, region->start, region->end, TRUE);
valuesToVector(values, dv, region->start);
resultCount = wigPrintDataVectorOut(dv, wigOutType, maxOut, NULL);
dataVectorFree(&dv);
return resultCount;
}

int bigWigOutRegion(char *table, struct sqlConnection *conn,
			     struct region *region, int maxOut,
			     enum wigOutputType wigOutType)
/* Write out bigWig for region, doing intersecting and filtering as need be. */
{
boolean isMerged = anySubtrackMerge(table, database);
int resultCount = 0;
char *wigFileName = bigWigFileName(table, conn);
if (wigFileName)
    {
    struct bbiFile *bwf = bigWigFileOpen(wigFileName);
    if (bwf)
	{
	/* Easy case, just dump out data. */
	if (!anyFilter() && !anyIntersection() && !isMerged && wigOutType == wigOutData)
	    resultCount = bigWigIntervalDump(bwf, region->chrom, region->start, region->end,
		    maxOut, stdout);
	/* Pretty easy case, still do it ourselves. */
	else if (!isMerged && wigOutType == wigOutData)
	    {
	    double ll, ul;
	    enum wigCompare cmp;
	    getWigFilter(database, curTable, &cmp, &ll, &ul);
	    struct lm *lm = lmInit(0);
	    struct bbiInterval *ivList, *iv;
	    ivList = intersectedFilteredBbiIntervalsOnRegion(conn, bwf, region, cmp, ll, ul, lm);
	    for (iv=ivList; iv != NULL && resultCount < maxOut; iv = iv->next, ++resultCount)
	        {
		fprintf(stdout, "%s\t%d\t%d\t%g\n", region->chrom, iv->start, iv->end, iv->val);
		}
	    lmCleanup(&lm);
	    }
	/* Harder cases - resort to making a data vector and letting that machinery handle it. */
	else
	    {
	    struct dataVector *dv = bigWigDataVector(table, conn, region);
	    resultCount = wigPrintDataVectorOut(dv, wigOutType, maxOut, NULL);
	    dataVectorFree(&dv);
	    }
	}
    bbiFileClose(&bwf);
    }
freeMem(wigFileName);
return resultCount;
}

void doSummaryStatsBigWig(struct sqlConnection *conn)
/* Put up page showing summary stats for bigWig track. */
{
struct trackDb *track = curTrack;
char *table = curTable;
char *shortLabel = (track == NULL ? table : track->shortLabel);
char *fileName = bigWigFileName(table, conn);
long startTime = clock1000();

htmlOpen("%s (%s) Big Wig Summary Statistics", shortLabel, table);

if (anySubtrackMerge(database, curTable))
    hPrintf("<P><EM><B>Note:</B> subtrack merge is currently ignored on this "
	    "page (not implemented yet).  Statistics shown here are only for "
	    "the primary table %s (%s).</EM>", shortLabel, table);

struct bbiFile *bwf = bigWigFileOpen(fileName);
struct region *region, *regionList = getRegions();
double sumData = 0, sumSquares = 0, minVal = 0, maxVal = 0;
bits64 validCount = 0;

if (!anyFilter() && !anyIntersection())
    {
    for (region = regionList; region != NULL; region = region->next)
	{
	struct bbiSummaryElement sum;
	if (bbiSummaryArrayExtended(bwf, region->chrom, region->start, region->end,
		bigWigIntervalQuery, 1, &sum))
	    {
	    if (validCount == 0)
		{
		minVal = sum.minVal;
		maxVal = sum.maxVal;
		}
	    else
	        {
		if (sum.minVal < minVal)
		    minVal = sum.minVal;
		if (sum.maxVal > maxVal)
		    maxVal = sum.maxVal;
		}
	    sumData += sum.sumData;
	    sumSquares += sum.sumSquares;
	    validCount += sum.validCount;
	    }
	}
    }
else
    {
    double ll, ul;
    enum wigCompare cmp;
    getWigFilter(database, curTable, &cmp, &ll, &ul);
    for (region = regionList; region != NULL; region = region->next)
        {
	struct lm *lm = lmInit(0);
	struct bbiInterval *iv, *ivList;
	ivList = intersectedFilteredBbiIntervalsOnRegion(conn, bwf, region, cmp, ll, ul, lm);
	for (iv = ivList; iv != NULL; iv = iv->next)
	    {
	    double val = iv->val;
	    double size = iv->end - iv->start;
	    if (validCount == 0)
		minVal = maxVal = val;
	    else
	        {
		if (val < minVal)
		    minVal = val;
		if (val > maxVal)
		    maxVal = val;
		}
	    sumData += size*val;
	    sumSquares += size*val*val;
	    validCount += size;
	    }
	lmCleanup(&lm);
	}
    }

hTableStart();
floatStatRow("mean", sumData/validCount);
floatStatRow("min", minVal);
floatStatRow("max", maxVal);
floatStatRow("standard deviation", calcStdFromSums(sumData, sumSquares, validCount));
numberStatRow("bases with data", validCount);
long long regionSize = basesInRegion(regionList,0);
long long gapTotal = gapsInRegion(conn, regionList,0);
numberStatRow("bases with sequence", regionSize - gapTotal);
numberStatRow("bases in region", regionSize);
wigFilterStatRow(conn);
stringStatRow("intersection", cartUsualString(cart, hgtaIntersectTable, "off"));
long wigFetchTime = clock1000() - startTime;
floatStatRow("load and calc time", 0.001*wigFetchTime);
hTableEnd();

bbiFileClose(&bwf);
htmlClose();
}

struct bed *bigWigIntervalsToBed(struct sqlConnection *conn, char *table, struct region *region,
				 struct lm *lm)
/* Return a list of unfiltered, unintersected intervals in region as bed (for
 * secondary table in intersection). */
{
struct bed *bed, *bedList = NULL;
char *fileName = bigWigFileName(table, conn);
struct bbiFile *bwf = bigWigFileOpen(fileName);
struct bbiInterval *iv, *ivList = bigWigIntervalQuery(bwf, region->chrom, region->start,
						      region->end, lm);
for (iv = ivList;  iv != NULL;  iv = iv->next)
    {
    lmAllocVar(lm, bed);
    bed->chrom = region->chrom;
    bed->chromStart = iv->start;
    bed->chromEnd = iv->end;
    slAddHead(&bedList, bed);
    }
slReverse(&bedList);
return bedList;
}

void bigWigFillDataVector(char *table, struct region *region,
	struct sqlConnection *conn, struct dataVector *vector)
/* Fill in data vector with bigWig info on region.  Handles filters and intersections. */
{
/* Figure out filter values if any. */
double ll, ul;
enum wigCompare cmp;
getWigFilter(database, curTable, &cmp, &ll, &ul);

/* Get intervals that pass filter and intersection. */
struct lm *lm = lmInit(0);
char *fileName = bigWigFileName(table, conn);
struct bbiFile *bwf = bigWigFileOpen(fileName);
struct bbiInterval *iv, *ivList;
ivList = intersectedFilteredBbiIntervalsOnRegion(conn, bwf, region, cmp, ll, ul, lm);
int vIndex = 0;
for (iv = ivList; iv != NULL; iv = iv->next)
    {
    int start = max(iv->start, region->start);
    int end = min(iv->end, region->end);
    double val = iv->val;
    int i;
    for (i=start; i<end && vIndex < vector->maxCount; ++i)
        {
	vector->value[vIndex] = val;
	vector->position[vIndex] = i;
	++vIndex;
	}
    }
vector->count = vIndex;
bbiFileClose(&bwf);
freeMem(fileName);
lmCleanup(&lm);
}

struct dataVector *bigWigDataVector(char *table,
	struct sqlConnection *conn, struct region *region)
/* Read in bigWig as dataVector and return it.  Filtering, subtrack merge
 * and intersection are handled. */
{
if (anySubtrackMerge(database, table))
    return mergedWigDataVector(table, conn, region);
else
    {
    struct dataVector *dv = dataVectorNew(region->chrom, region->end - region->start);
    bigWigFillDataVector(table, region, conn, dv);
    return dv;
    }
}

void showSchemaBigWigNoTable(char *db, char *table, struct trackDb *tdb)
{
struct trackDb *tableTdb = hashFindVal(fullTableToTdbHash, table);
hPrintf("<B>Database:</B> %s", db);
printf("<BR>The data for this track is provided by a file in "
       "<A HREF=\"/goldenPath/help/bigWig.html\" TARGET=_BLANK>"
       "BigWig</A> format.");
hPrintf("<BR><B>Data URL:</B>  %s", trackDbSetting(tableTdb, "bigDataUrl"));
printTrackHtml(tableTdb);
}
