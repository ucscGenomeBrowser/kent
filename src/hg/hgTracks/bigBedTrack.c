/* bigBed - stuff to handle loading and display of bigBed type tracks in browser. 
 * Mostly just links to bed code, but handles a few things itself, like the dense
 * drawing code. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "bedCart.h"
#include "hgTracks.h"
#include "hmmstats.h"
#include "localmem.h"
#include "wigCommon.h"
#include "bbiFile.h"
#include "obscure.h"
#include "bigWig.h"
#include "bigBed.h"
#include "bigWarn.h"
#include "errCatch.h"
#include "trackHub.h"




struct bbiFile *fetchBbiForTrack(struct track *track)
/* Fetch bbiFile from track, opening it if it is not already open. */
{
struct bbiFile *bbi = track->bbiFile;
if (bbi == NULL)
    {
    char *fileName = NULL;
    if (track->parallelLoading) // do not use mysql during parallel fetch
	{
	fileName = cloneString(trackDbSetting(track->tdb, "bigDataUrl"));
	}
    else
	{
	struct sqlConnection *conn = NULL;
	if (!trackHubDatabase(database))
	    conn = hAllocConnTrack(database, track->tdb);
	fileName = bbiNameFromSettingOrTable(track->tdb, conn, track->table);
	hFreeConn(&conn);
	}
    bbi = track->bbiFile = bigBedFileOpen(fileName);
    }
return bbi;
}

struct bigBedInterval *bigBedSelectRange(struct track *track,
	char *chrom, int start, int end, struct lm *lm)
/* Return list of intervals in range. */
{
struct bigBedInterval *result = NULL;
/* protect against temporary network error */
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    struct bbiFile *bbi = fetchBbiForTrack(track);
    int maxItems = min(BIGBEDMAXIMUMITEMS, maximumTrackItems(track)); // do not allow it to exceed BIGBEDMAXIMUMITEMS for bigBed
    result = bigBedIntervalQuery(bbi, chrom, start, end, maxItems + 1, lm);
    if (slCount(result) > maxItems)
	{
	track->limitedVis = tvDense;
	track->limitedVisSet = TRUE;
	result = NULL;
	AllocArray(track->summary, insideWidth);
	if (bigBedSummaryArrayExtended(bbi, chrom, start, end, insideWidth, track->summary))
	    {
	    char *denseCoverage = trackDbSettingClosestToHome(track->tdb, "denseCoverage");
	    if (denseCoverage != NULL)
		{
		double endVal = atof(denseCoverage);
		if (endVal <= 0)
		    {
		    AllocVar(track->sumAll);
		    *track->sumAll = bbiTotalSummary(bbi);
		    }
		}
	    }
	else
	    freez(&track->summary);
	}
    bbiFileClose(&bbi);
    track->bbiFile = NULL;
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    track->networkErrMsg = cloneString(errCatch->message->string);
    track->drawItems = bigDrawWarning;
    track->totalHeight = bigWarnTotalHeight;
    result = NULL;
    }
errCatchFree(&errCatch);

return result;
}

int bbExtraFieldIndex(struct trackDb *tdb, char* fieldName)
/* return the index of a given extra field from the bbInterval
 * 0 is the name-field of bigBed and is used as an error code 
 * as this is the default anyways */
{
if (fieldName==NULL)
    return 0;
// copied from hgc.c
// get .as file for track
struct sqlConnection *conn = NULL ;
if (!trackHubDatabase(database))
    conn = hAllocConnTrack(database, tdb);
struct asObject *as = asForTdb(conn, tdb);
hFreeConn(&conn);
if (as == NULL)
    return 0;

// search for field name, return index if found
struct asColumn *col = as->columnList;
int ix = 0;
for (;col != NULL;col=col->next, ix+=1)
    if (sameString(col->name, fieldName))
        return max(ix-3, 0); // never return a negative value
return 0;
}

char* restField(struct bigBedInterval *bb, int fieldIdx) 
/* return a given field from the bb->rest field, NULL on error */
{
if (fieldIdx==0) // we don't return the first(=name) field of bigBed
    return NULL;
char *rest = cloneString(bb->rest);
char *restFields[256];
int restCount = chopTabs(rest, restFields);
char *field = NULL;
if (fieldIdx < restCount)
    field = cloneString(restFields[fieldIdx]);
freeMem(rest);
return field;
}

void bigBedAddLinkedFeaturesFrom(struct track *track,
	char *chrom, int start, int end, int scoreMin, int scoreMax, boolean useItemRgb,
	int fieldCount, struct linkedFeatures **pLfList)
/* Read in items in chrom:start-end from bigBed file named in track->bbiFileName, convert
 * them to linkedFeatures, and add to head of list. */
{
struct lm *lm = lmInit(0);
struct trackDb *tdb = track->tdb;
struct bigBedInterval *bb, *bbList = bigBedSelectRange(track, chrom, start, end, lm);
char *bedRow[32];
char startBuf[16], endBuf[16];
char *scoreFilter = cartOrTdbString(cart, track->tdb, "scoreFilter", NULL);
char *mouseOverField = cartOrTdbString(cart, track->tdb, "mouseOverField", NULL);
int minScore = 0;
if (scoreFilter)
    minScore = atoi(scoreFilter);

int mouseOverIdx = bbExtraFieldIndex(tdb, mouseOverField);

for (bb = bbList; bb != NULL; bb = bb->next)
    {
    char* mouseOver = restField(bb, mouseOverIdx);
    bigBedIntervalToRow(bb, chromName, startBuf, endBuf, bedRow, ArraySize(bedRow));
    struct bed *bed = bedLoadN(bedRow, fieldCount);
    struct linkedFeatures *lf = bedMungToLinkedFeatures(&bed, tdb, fieldCount,
    	scoreMin, scoreMax, useItemRgb);
    if (scoreFilter == NULL || lf->score >= minScore)
	slAddHead(pLfList, lf);
    lf->mouseOver   = mouseOver; // leaks some memory, cloneString handles NULL ifself 
    }
lmCleanup(&lm);
}


boolean canDrawBigBedDense(struct track *tg)
/* Return TRUE if conditions are such that can do the fast bigBed dense data fetch and
 * draw. */
{
return tg->isBigBed;
}


void bigBedDrawDense(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width,
        MgFont *font, Color color)
/* Use big-bed summary data to quickly draw bigBed. */
{
struct bbiSummaryElement *summary = tg->summary;
if (summary)
    {
    char *denseCoverage = trackDbSettingClosestToHome(tg->tdb, "denseCoverage");
    if (denseCoverage != NULL)
	{
	double startVal = 0, endVal = atof(denseCoverage);
	if (endVal <= 0)
	    {
	    struct bbiSummaryElement sumAll = *tg->sumAll;
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
freez(&tg->summary);
}

static void bigBedMapItem(struct track *tg, struct hvGfx *hvg, void *item,
				char *itemName, char *mapItemName, int start, int end,
				int x, int y, int width, int height)
/* Pull out special mouseover text from item->mouseOver. 
 * Fallback to item->name if it's not set.
 * (derived from bedPlusLabelMapItem) */
{
// Don't bother if we are imageV2 and a dense child.
if (theImgBox && tg->limitedVis == tvDense && tdbIsCompositeChild(tg->tdb))
    return;

struct linkedFeatures *lf = item;
char *mouseOverText   = isEmpty(lf->mouseOver) ? lf->name : lf->mouseOver;
mapBoxHc(hvg, start, end, x, y, width, height, tg->track, mapItemName, mouseOverText);
}

void bigBedMethods(struct track *track, struct trackDb *tdb, 
                                int wordCount, char *words[])
/* Set up bigBed methods. */
{
complexBedMethods(track, tdb, TRUE, wordCount, words);
track->mapItem = bigBedMapItem;
}
