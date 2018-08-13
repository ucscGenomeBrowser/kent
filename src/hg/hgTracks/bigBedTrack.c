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
#include "bigBedFilter.h"

struct bigBedFilter *bigBedMakeNumberFilter(struct cart *cart, struct bbiFile *bbi, struct trackDb *tdb, char *filter, char *defaultLimits,  char *field)
/* Make a filter on this column if the trackDb or cart wants us to. */
{
struct bigBedFilter *ret = NULL;
char *setting = trackDbSettingClosestToHome(tdb, filter);
int fieldNum =  bbExtraFieldIndex(bbi, field) + 3;
if (setting)
    {
    struct asObject *as = bigBedAsOrDefault(bbi);
    // All this isFloat conditional code is here because the cart
    // variables used for floats are different than those used for ints
    // in ../lib/hui.c so we have to use the correct getScore*() routine
    // to access them..    We're doomed.
    boolean isFloat = FALSE;
    struct asColumn *asCol = asColumnFind(as, field);
    if (asCol != NULL)
        isFloat = asTypesIsFloating(asCol->lowType->type);
    boolean invalid = FALSE;
    double minValueTdb = 0,maxValueTdb = NO_VALUE;
    double minLimit=NO_VALUE,maxLimit=NO_VALUE,min = minValueTdb,max = maxValueTdb;
    if (!isFloat)
        {
        int minValueTdbInt = 0,maxValueTdbInt = NO_VALUE;
        colonPairToInts(setting,&minValueTdbInt,&maxValueTdbInt);
        int minLimitInt=NO_VALUE,maxLimitInt=NO_VALUE,minInt=minValueTdbInt,maxInt=maxValueTdbInt;
        colonPairToInts(defaultLimits,&minLimitInt,&maxLimitInt);
        getScoreIntRangeFromCart(cart,tdb,FALSE,filter,&minLimitInt,&maxLimitInt,&minInt,&maxInt);

        // copy all the ints over to the doubles (sigh)
        min = minInt;
        max = maxInt;
        minLimit = minLimitInt;
        maxLimit = maxLimitInt;
        minValueTdb = minValueTdbInt;
        maxValueTdb = maxValueTdbInt;
        }
    else
        {
        colonPairToDoubles(setting,&minValueTdb,&maxValueTdb);
        colonPairToDoubles(defaultLimits,&minLimit,&maxLimit);
        getScoreFloatRangeFromCart(cart,tdb,FALSE,filter,&minLimit,&maxLimit,&min,&max);
        }
    if ((int)minLimit != NO_VALUE || (int)maxLimit != NO_VALUE)
        {
        // assume tdb default values within range!
        // (don't give user errors that have no consequence)
        if ((min != minValueTdb && (((int)minLimit != NO_VALUE && min < minLimit)
                                || ((int)maxLimit != NO_VALUE && min > maxLimit)))
        ||  (max != maxValueTdb && (((int)minLimit != NO_VALUE && max < minLimit)
                                || ((int)maxLimit != NO_VALUE && max > maxLimit))))
            {
            invalid = TRUE;
            char value[64];
            if ((int)max == NO_VALUE) // min only is allowed, but max only is not
                safef(value, sizeof(value), "entered minimum (%g)", min);
            else
                safef(value, sizeof(value), "entered range (min:%g and max:%g)", min, max);
            char limits[64];
            if ((int)minLimit != NO_VALUE && (int)maxLimit != NO_VALUE)
                safef(limits, sizeof(limits), "violates limits (%g to %g)", minLimit, maxLimit);
            else if ((int)minLimit != NO_VALUE)
                safef(limits, sizeof(limits), "violates lower limit (%g)", minLimit);
            else //if ((int)maxLimit != NO_VALUE)
                safef(limits, sizeof(limits), "violates uppper limit (%g)", maxLimit);
            warn("invalid filter by %s: %s %s for track %s", field, value, limits, tdb->track);
            }
        }
    if (invalid)
        {
        char filterLimitName[64];
        safef(filterLimitName, sizeof(filterLimitName), "%s%s", filter, _MIN);
        cartRemoveVariableClosestToHome(cart,tdb,FALSE,filterLimitName);
        safef(filterLimitName, sizeof(filterLimitName), "%s%s", filter, _MAX);
        cartRemoveVariableClosestToHome(cart,tdb,FALSE,filterLimitName);
        }
    else if (((int)min != NO_VALUE && ((int)minLimit == NO_VALUE || minLimit != min))
         ||  ((int)max != NO_VALUE && ((int)maxLimit == NO_VALUE || maxLimit != max)))
         // Assumes min==NO_VALUE or min==minLimit is no filter
         // Assumes max==NO_VALUE or max==maxLimit is no filter!
        {
        AllocVar(ret);
        ret->fieldNum = fieldNum;
        if ((int)max == NO_VALUE || ((int)maxLimit != NO_VALUE && maxLimit == max))
            {
            ret->comparisonType = COMPARE_MORE;
            ret->value1 = min;
            }
        else if ((int)min == NO_VALUE || ((int)minLimit != NO_VALUE && minLimit == min))
            {
            ret->comparisonType = COMPARE_LESS;
            ret->value1 = max;
            }
        else
            {
            ret->comparisonType = COMPARE_BETWEEN;
            ret->value1 = min;
            ret->value2 = max;
            }
        }
    }
return ret;
}

struct bigBedFilter *bigBedMakeFilterText(struct cart *cart, struct bbiFile *bbi, struct trackDb *tdb, char *filterName, char *field)
/* Add a bigBed filter using a trackDb filterText statement. */
{
struct bigBedFilter *filter;
char *setting = trackDbSettingClosestToHome(tdb, filterName);
char *value = cartUsualStringClosestToHome(cart, tdb, FALSE, filterName, setting);

AllocVar(filter);
filter->fieldNum =  bbExtraFieldIndex(bbi, field) + 3;
filter->comparisonType = COMPARE_REGEXP;
regcomp(&filter->regEx, value, REG_NOSUB);

return filter;
}

struct bigBedFilter *bigBedMakeFilterBy(struct cart *cart, struct bbiFile *bbi, struct trackDb *tdb, char *field, struct slName *choices)
/* Add a bigBed filter using a trackDb filterBy statement. */
{
struct bigBedFilter *filter;

AllocVar(filter);
filter->fieldNum =  bbExtraFieldIndex(bbi, field) + 3;
filter->comparisonType = COMPARE_HASH;
filter->valueHash = newHash(5);

for(; choices; choices = choices->next)
    hashStore(filter->valueHash, choices->name);

return filter;
}

struct bigBedFilter *bigBedBuildFilters(struct cart *cart, struct bbiFile *bbi, struct trackDb *tdb)
/* Build all the numeric and filterBy filters for a bigBed */
{
struct bigBedFilter *filters = NULL, *filter;
struct slName *filterSettings = trackDbSettingsWildMatch(tdb, "*Filter");

for(; filterSettings; filterSettings = filterSettings->next)
    {
    char *fieldName = cloneString(filterSettings->name);
    fieldName[strlen(fieldName) - sizeof "Filter" + 1] = 0;
    if ((filter = bigBedMakeNumberFilter(cart, bbi, tdb, filterSettings->name, NULL, fieldName)) != NULL)
        slAddHead(&filters, filter);
    }

filterSettings = trackDbSettingsWildMatch(tdb, "*FilterText");

for(; filterSettings; filterSettings = filterSettings->next)
    {
    char *fieldName = cloneString(filterSettings->name);
    fieldName[strlen(fieldName) - sizeof "FilterText" + 1] = 0;
    if ((filter = bigBedMakeFilterText(cart, bbi, tdb, filterSettings->name,  fieldName)) != NULL)
        slAddHead(&filters, filter);
    }

filterBy_t *filterBySet = filterBySetGet(tdb, cart,NULL);
filterBy_t *filterBy = filterBySet;
for (;filterBy != NULL; filterBy = filterBy->next)
    {
    if (filterBy->slChoices && differentString(filterBy->slChoices->name, "All")) 
        {
        if ((filter = bigBedMakeFilterBy(cart, bbi, tdb, filterBy->column, filterBy->slChoices)) != NULL)
            slAddHead(&filters, filter);
        }
    }

return filters;
}


boolean bigBedFilterInterval(char **bedRow, struct bigBedFilter *filters)
/* Go through a row and filter based on filters.  Return TRUE if all filters are passed. */
{
struct bigBedFilter *filter;
for(filter = filters; filter; filter = filter->next)
    {
    double val = atof(bedRow[filter->fieldNum]);

    switch(filter->comparisonType)
        {
        case COMPARE_REGEXP:
            if (regexec(&filter->regEx,bedRow[filter->fieldNum], 0, NULL,0 ) != 0)
                return FALSE;
            break;
        case COMPARE_HASH:
            if (!hashLookup(filter->valueHash, bedRow[filter->fieldNum]))
                return FALSE;
            break;
        case COMPARE_LESS:
            if (!(val <= filter->value1))
                return FALSE;
            break;
        case COMPARE_MORE:
            if (!(val >= filter->value1))
                return FALSE;
            break;
        case COMPARE_BETWEEN:
            if (!((val >= filter->value1) && (val <= filter->value2)))
                return FALSE;
            break;
        }
    }
return TRUE;
}


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
if (bb->rest != NULL)
    chopTabs(cloneString(bb->rest), restFields);
struct dyString *dy = newDyString(128);
boolean firstTime = TRUE;
struct slInt *labelInt = track->labelColumns;
for(; labelInt; labelInt = labelInt->next)
    {
    if (!firstTime)
        dyStringAppend(dy, labelSeparator);

    switch(labelInt->val)
        {
        case 0:
            dyStringAppend(dy, chromName);
            break;
        case 1:
            dyStringPrintf(dy, "%d", bb->start);
            break;
        case 2:
            dyStringPrintf(dy, "%d", bb->end);
            break;
        default:
            assert(bb->rest != NULL);
            dyStringPrintf(dy, "%s", restFields[labelInt->val - 3]);
            break;
        }
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

track->bbiFile = NULL;

struct bigBedFilter *filters = bigBedBuildFilters(cart, bbi, track->tdb) ;
for (bb = bbList; bb != NULL; bb = bb->next)
    {
    struct linkedFeatures *lf = NULL;
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
        if (bigBedFilterInterval(bedRow, filters))
            {
            struct bed *bed = bedLoadN(bedRow, fieldCount);
            lf = bedMungToLinkedFeatures(&bed, tdb, fieldCount,
                scoreMin, scoreMax, useItemRgb);
            }
	}

    if (lf == NULL)
        continue;

    lf->label = makeLabel(track,  bb);
    if (sameString(track->tdb->type, "bigGenePred") || startsWith("genePred", track->tdb->type))
        {
        lf->original = genePredFromBigGenePred(chromName, bb); 
        }

    char* mouseOver = restField(bb, mouseOverIdx);
    lf->mouseOver   = mouseOver; // leaks some memory, cloneString handles NULL ifself 

    if (scoreFilter == NULL || lf->score >= minScore)
	slAddHead(pLfList, lf);
    }
lmCleanup(&lm);

if (!trackDbSettingClosestToHomeOn(track->tdb, "linkIdInName"))
    track->itemName = bigLfItemName;
bbiFileClose(&bbi);
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
