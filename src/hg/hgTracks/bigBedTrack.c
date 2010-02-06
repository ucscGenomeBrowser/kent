/* bigBed - stuff to handle loading and display of bigBed type tracks in browser. 
 * Mostly just links to bed code, but handles a few things itself, like the dense
 * drawing code. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"
#include "hmmstats.h"
#include "localmem.h"
#include "wigCommon.h"
#include "bbiFile.h"
#include "obscure.h"
#include "bigWig.h"
#include "bigBed.h"

char *bbiNameFromTable(struct sqlConnection *conn, char *table)
/* Return file name from little table. */
{
char query[256];
safef(query, sizeof(query), "select fileName from %s", table);
char *fileName = sqlQuickString(conn, query);
if (fileName == NULL)
    errAbort("Missing fileName in %s table", table);
return fileName;
}

struct bigBedInterval *bigBedSelectRange(struct sqlConnection *conn, struct track *track,
	char *chrom, int start, int end, struct lm *lm)
/* Return list of intervals in range. */
{
struct bbiFile *bbi = track->bbiFile;
int maxItems = maximumTrackItems(track) + 1;
struct bigBedInterval *result = bigBedIntervalQuery(bbi, chrom, start, end, maxItems, lm);
if (slCount(result) >= maxItems)
    {
    track->limitedVis = tvDense;
    track->limitedVisSet = TRUE;
    result = NULL;
    }
return result;
}

void bigBedAddLinkedFeaturesFrom(struct sqlConnection *conn, struct track *track,
	char *chrom, int start, int end, int scoreMin, int scoreMax, boolean useItemRgb,
	int fieldCount, struct linkedFeatures **pLfList)
/* Read in items in chrom:start-end from bigBed file named in track->bbiFileName, convert
 * them to linkedFeatures, and add to head of list. */
{
struct lm *lm = lmInit(0);
struct trackDb *tdb = track->tdb;
struct bigBedInterval *bb, *bbList = bigBedSelectRange(conn, track, chrom, start, end, lm);
char *bedRow[32];
char startBuf[16], endBuf[16];

for (bb = bbList; bb != NULL; bb = bb->next)
    {
    bigBedIntervalToRow(bb, chromName, startBuf, endBuf, bedRow, ArraySize(bedRow));
    struct bed *bed = bedLoadN(bedRow, fieldCount);
    struct linkedFeatures *lf = bedMungToLinkedFeatures(&bed, tdb, fieldCount,
    	scoreMin, scoreMax, useItemRgb);
    slAddHead(pLfList, lf);
    }
lmCleanup(&lm);
}


boolean canDrawBigBedDense(struct track *tg)
/* Return TRUE if conditions are such that can do the fast bigBed dense data fetch and
 * draw. */
{
return tg->isBigBed;
#ifdef OLD
/* Unfortunately bigBed datasets are so big, that running filters on them is not so practical
 * in the dense mode. */
if (!tg->isBigBed)
    return FALSE;
int scoreFilter = cartOrTdbInt(cart, tg->tdb, "scoreFilter", 0);
return scoreFilter == 0 && trackDbSetting(tg->tdb, "colorByStrand") == NULL;
#endif /* OLD */
}


void bigBedDrawDense(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width,
        MgFont *font, Color color)
/* Use big-bed summary data to quickly draw bigBed. */
{
struct bbiSummaryElement summary[width];
if (bigBedSummaryArrayExtended(tg->bbiFile, chromName, seqStart, seqEnd, width, summary))
    {
    char *denseCoverage = trackDbSettingClosestToHome(tg->tdb, "denseCoverage");
    if (denseCoverage != NULL)
        {
	double startVal = 0, endVal = atof(denseCoverage);
	if (endVal <= 0)
	    {
	    struct bbiSummaryElement sumAll = bbiTotalSummary(tg->bbiFile);
	    double mean = sumAll.sumData/sumAll.validCount;
	    double std = calcStdFromSums(sumAll.sumData, sumAll.sumSquares, sumAll.validCount);
	    rangeFromMinMaxMeanStd(0, sumAll.maxVal, mean, std, &startVal, &endVal);
	    }
	int x;
	for (x=0; x<width; ++x)
	    {
	    if (summary[x].validCount > 0)
		{
		Color color = shadesOfGray[grayInRange(summary[x].maxVal, startVal, endVal)];
		hvGfxBox(hvg, x+xOff, yOff, 1, tg->heightPer, color);
		}
	    }
	}
    else
	{
	int x;
	for (x=0; x<width; ++x)
	    {
	    if (summary[x].validCount > 0)
		{
		hvGfxBox(hvg, x+xOff, yOff, 1, tg->heightPer, color);
		}
	    }
	}
    }
}

void bigBedMethods(struct track *track, struct trackDb *tdb, 
                                int wordCount, char *words[])
/* Set up bigBed methods. */
{
complexBedMethods(track, tdb, TRUE, wordCount, words);
if (track->bbiFile == NULL)
    {
    struct sqlConnection *conn = hAllocConn(database);
    char *fileName = bbiNameFromTable(conn, track->mapName);
    hFreeConn(&conn);
    track->bbiFile = bigBedFileOpen(fileName);
    }
}
