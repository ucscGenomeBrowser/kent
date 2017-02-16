/* bigBed - stuff to handle loading and display of bigBed type tracks in browser. 
 * Mostly just links to bed code, but handles a few things itself, like the dense
 * drawing code. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

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
#include "net.h"
#include "bigPsl.h"

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
        if (fileName == NULL)
            fileName = cloneString(trackDbSetting(track->tdb, "bigGeneDataUrl"));
	}
    else 
	{
	struct sqlConnection *conn = NULL;
	if (!trackHubDatabase(database))
	    conn = hAllocConnTrack(database, track->tdb);
	fileName = bbiNameFromSettingOrTable(track->tdb, conn, track->table);
	hFreeConn(&conn);
	}
    
    #ifdef USE_GBIB_PWD
    #include "gbib.c"
    #endif

    bbi = track->bbiFile = bigBedFileOpen(fileName);
    }
return bbi;
}

struct bigBedInterval *bigBedSelectRangeExt(struct track *track,
	char *chrom, int start, int end, struct lm *lm, int maxItems)
/* Return list of intervals in range. */
{
struct bigBedInterval *result = NULL;
/* protect against temporary network error */
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    struct bbiFile *bbi = fetchBbiForTrack(track);
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


char* restField(struct bigBedInterval *bb, int fieldIdx) 
/* return a given field from the bb->rest field, NULL on error */
{
if (fieldIdx==0) // we don't return the first(=name) field of bigBed
    return NULL;
char *rest = cloneString(bb->rest);
char *restFields[1024];
int restCount = chopTabs(rest, restFields);
char *field = NULL;
if (fieldIdx < restCount)
    field = cloneString(restFields[fieldIdx]);
freeMem(rest);
return field;
}


char *makeLabel(struct track *track,  struct bigBedInterval *bb)
// Build a label for a bigBedTrack from the requested label fields.
{
char *labelSeparator = stripEnclosingDoubleQuotes(trackDbSettingClosestToHome(track->tdb, "labelSeparator"));
if (labelSeparator == NULL)
    labelSeparator = "/";
char *restFields[256];
chopTabs(cloneString(bb->rest), restFields);
struct dyString *dy = newDyString(128);
boolean firstTime = TRUE;
struct slInt *labelInt = track->labelColumns;
for(; labelInt; labelInt = labelInt->next)
    {
    if (!firstTime)
        dyStringAppend(dy, labelSeparator);

    dyStringPrintf(dy, "%s", restFields[labelInt->val - 3]);
    firstTime = FALSE;
    }
return dyStringCannibalize(&dy);
}

void bigBedAddLinkedFeaturesFromExt(struct track *track,
	char *chrom, int start, int end, int scoreMin, int scoreMax, boolean useItemRgb,
	int fieldCount, struct linkedFeatures **pLfList, int maxItems)
/* Read in items in chrom:start-end from bigBed file named in track->bbiFileName, convert
 * them to linkedFeatures, and add to head of list. */
{
struct lm *lm = lmInit(0);
struct trackDb *tdb = track->tdb;
struct bigBedInterval *bb, *bbList = bigBedSelectRangeExt(track, chrom, start, end, lm, maxItems);
char *scoreFilter = cartOrTdbString(cart, track->tdb, "scoreFilter", NULL);
char *mouseOverField = cartOrTdbString(cart, track->tdb, "mouseOverField", NULL);
int minScore = 0;
if (scoreFilter)
    minScore = atoi(scoreFilter);

struct bbiFile *bbi = fetchBbiForTrack(track);
int seqTypeField =  0;
if (sameString(track->tdb->type, "bigPsl"))
    {
    seqTypeField =  bbExtraFieldIndex(bbi, "seqType");
    }

int mouseOverIdx = bbExtraFieldIndex(bbi, mouseOverField);

bbiFileClose(&bbi);
track->bbiFile = NULL;

for (bb = bbList; bb != NULL; bb = bb->next)
    {
    struct linkedFeatures *lf;
    if (sameString(track->tdb->type, "bigPsl"))
	{
	char *seq, *cds;
	struct psl *psl = pslFromBigPsl(chromName, bb, seqTypeField,  &seq, &cds); 
	int sizeMul =  pslIsProtein(psl) ? 3 : 1;
	boolean isXeno = 0;  // just affects grayIx
	boolean nameGetsPos = FALSE; // we want the name to stay the name

	lf = lfFromPslx(psl, sizeMul, isXeno, nameGetsPos, track);
	lf->original = psl;
	if ((seq != NULL) && (lf->orientation == -1))
	    reverseComplement(seq, strlen(seq));
	lf->extra = seq;
	lf->cds = cds;
	}
    else
	{
        char startBuf[16], endBuf[16];
        char *bedRow[32];

        bigBedIntervalToRow(bb, chromName, startBuf, endBuf, bedRow, ArraySize(bedRow));
	struct bed *bed = bedLoadN(bedRow, fieldCount);
	lf = bedMungToLinkedFeatures(&bed, tdb, fieldCount,
	    scoreMin, scoreMax, useItemRgb);
	}

    lf->label = makeLabel(track,  bb);
    if (sameString(track->tdb->type, "bigGenePred") || startsWith("genePred", track->tdb->type))
        {
        struct genePred  *gp = lf->original = genePredFromBigGenePred(chromName, bb); 
        lf->extra = gp->name2;
        lf->isBigGenePred = TRUE;
        }

    char* mouseOver = restField(bb, mouseOverIdx);
    lf->mouseOver   = mouseOver; // leaks some memory, cloneString handles NULL ifself 

    if (scoreFilter == NULL || lf->score >= minScore)
	slAddHead(pLfList, lf);
    }
lmCleanup(&lm);

if (!trackDbSettingClosestToHomeOn(track->tdb, "linkIdInName"))
    track->itemName = bigLfItemName;
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

char *bigBedItemName(struct track *tg, void *item)
// return label for simple beds
{
struct bed *bed = (struct bed *)item;

return bed->label;
}

char *bigLfItemName(struct track *tg, void *item)
// return label for linked features
{
struct linkedFeatures *lf = (struct linkedFeatures *)item;

return lf->label;
}

void bigBedMethods(struct track *track, struct trackDb *tdb, 
                                int wordCount, char *words[])
/* Set up bigBed methods. */
{
complexBedMethods(track, tdb, TRUE, wordCount, words);
}
