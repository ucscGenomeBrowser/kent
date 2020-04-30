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
#include "bigBedLabel.h"
#include "variation.h"

static unsigned getFieldNum(struct bbiFile *bbi, char *field)
// get field number for field name in bigBed.  errAbort if field not found.
{
int fieldNum =  bbFieldIndex(bbi, field);
if (fieldNum < 0)
    fieldNum = defaultFieldLocation(field);
if (fieldNum < 0)
    errAbort("error building filter with field %s.  Field not found.", field);

return fieldNum;
}

struct bigBedFilter *bigBedMakeNumberFilter(struct cart *cart, struct bbiFile *bbi, struct trackDb *tdb, char *filter, char *defaultLimits,  char *field)
/* Make a filter on this column if the trackDb or cart wants us to. */
{
struct bigBedFilter *ret = NULL;
char *setting = trackDbSettingClosestToHome(tdb, filter);
int fieldNum =  getFieldNum(bbi, field);
if (setting)
    {
    boolean invalid = FALSE;
    double minValueTdb = 0,maxValueTdb = NO_VALUE;
    double minLimit=NO_VALUE,maxLimit=NO_VALUE,min = minValueTdb,max = maxValueTdb;
    colonPairToDoubles(setting,&minValueTdb,&maxValueTdb);
    colonPairToDoubles(defaultLimits,&minLimit,&maxLimit);
    getScoreFloatRangeFromCart(cart,tdb,FALSE,filter,&minLimit,&maxLimit,&min,&max);
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

if (isEmpty(value)) 
    return NULL;

char *typeValue = getFilterType(cart, tdb, field, FILTERTEXT_WILDCARD);

AllocVar(filter);
filter->fieldNum =  getFieldNum(bbi, field);

if (sameString(typeValue, FILTERTEXT_REGEXP) )
    {
    filter->comparisonType = COMPARE_REGEXP;
    regcomp(&filter->regEx, value, REG_NOSUB);
    }
else
    {
    filter->comparisonType = COMPARE_WILDCARD;
    filter->wildCardString = cloneString(value);
    }

return filter;
}

struct bigBedFilter *bigBedMakeFilterBy(struct cart *cart, struct bbiFile *bbi, struct trackDb *tdb, char *field, struct slName *choices)
/* Add a bigBed filter using a trackDb filterBy statement. */
{
struct bigBedFilter *filter;
char *setting = getFilterType(cart, tdb, field,  FILTERBY_SINGLE);

AllocVar(filter);
filter->fieldNum =  getFieldNum(bbi, field);
filter->comparisonType = COMPARE_HASH;
if (setting) 
    {
    if (sameString(setting, FILTERBY_SINGLE_LIST) 
            || sameString(setting, FILTERBY_MULTIPLE_LIST_OR)
            || sameString(setting, FILTERBY_MULTIPLE_LIST_ONLY_OR))
                filter->comparisonType = COMPARE_HASH_LIST_OR;
    else if (sameString(setting, FILTERBY_MULTIPLE_LIST_AND) 
            || sameString(setting, FILTERBY_MULTIPLE_LIST_ONLY_AND))
                filter->comparisonType = COMPARE_HASH_LIST_AND;
    }
filter->valueHash = newHash(5);
filter->numValuesInHash = slCount(choices);

for(; choices; choices = choices->next)
    hashStore(filter->valueHash, choices->name);

return filter;
}

struct bigBedFilter *bigBedBuildFilters(struct cart *cart, struct bbiFile *bbi, struct trackDb *tdb)
/* Build all the numeric and filterBy filters for a bigBed */
{
struct bigBedFilter *filters = NULL, *filter;
struct trackDbFilter *tdbFilters = tdbGetTrackNumFilters(tdb);

if ((tdbFilters == NULL) && !trackDbSettingOn(tdb, "noScoreFilter"))
    {
    AllocVar(filter);
    slAddHead(&filters, filter);
    filter->fieldNum = 4;
    filter->comparisonType = COMPARE_MORE;
    char buffer[2048];
    safef(buffer, sizeof buffer, "%s.scoreFilter", tdb->track);
    filter->value1 = cartUsualDouble(cart, buffer, 0.0);
    }

for(; tdbFilters; tdbFilters = tdbFilters->next)
    {
    if ((filter = bigBedMakeNumberFilter(cart, bbi, tdb, tdbFilters->name, NULL, tdbFilters->fieldName)) != NULL)
        slAddHead(&filters, filter);
    }

tdbFilters = tdbGetTrackTextFilters(tdb);

for(; tdbFilters; tdbFilters = tdbFilters->next)
    {
    if ((filter = bigBedMakeFilterText(cart, bbi, tdb, tdbFilters->name,  tdbFilters->fieldName)) != NULL)
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
        case COMPARE_WILDCARD:
            if ( !wildMatch(filter->wildCardString, bedRow[filter->fieldNum]))
                return FALSE;
            break;
        case COMPARE_REGEXP:
            if (regexec(&filter->regEx,bedRow[filter->fieldNum], 0, NULL,0 ) != 0)
                return FALSE;
            break;
        case COMPARE_HASH_LIST_AND:
        case COMPARE_HASH_LIST_OR:
            {
            struct slName *values = commaSepToSlNames(bedRow[filter->fieldNum]);
            unsigned found = 0;
            struct hash *seenHash = newHash(3);
            for(; values; values = values->next)
                {
                if (hashLookup(seenHash, values->name))
                    continue;
                hashStore(seenHash, values->name);
                if (hashLookup(filter->valueHash, values->name))
                    {
                    found++;
                    if (filter->comparisonType == COMPARE_HASH_LIST_OR) 
                        break;
                    }
                }
            if (filter->comparisonType == COMPARE_HASH_LIST_AND) 
                {
                if (found < filter->numValuesInHash)
                    return FALSE;
                }
            else if (!found)
                return FALSE;
            }
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


void bigBedAddLinkedFeaturesFromExt(struct track *track,
	char *chrom, int start, int end, int scoreMin, int scoreMax, boolean useItemRgb,
	int fieldCount, struct linkedFeatures **pLfList, int maxItems)
/* Read in items in chrom:start-end from bigBed file named in track->bbiFileName, convert
 * them to linkedFeatures, and add to head of list. */
{
struct lm *lm = lmInit(0);
struct trackDb *tdb = track->tdb;
struct bigBedInterval *bb, *bbList = bigBedSelectRangeExt(track, chrom, start, end, lm, maxItems);
char *mouseOverField = cartOrTdbString(cart, track->tdb, "mouseOverField", NULL);

// check if this track can merge large items, this setting must be allowed in the trackDb
// stanza for the track, but can be enabled/disabled via trackUi/right click menu so
// we also need to check the cart for the current status
int mergeCount = 0;
boolean doWindowSizeFilter = trackDbSettingOn(track->tdb, MERGESPAN_TDB_SETTING);
if (doWindowSizeFilter)
    {
    char hasMergedItemsSetting[256];
    safef(hasMergedItemsSetting, sizeof(hasMergedItemsSetting), "%s.%s", track->track, MERGESPAN_CART_SETTING);
    if (cartVarExists(cart, hasMergedItemsSetting))
        doWindowSizeFilter = cartInt(cart, hasMergedItemsSetting) == 1;
    else // save the cart var so javascript can offer the right toggle
        cartSetInt(cart, hasMergedItemsSetting, 1);
    }
/* protect against temporary network error */
struct bbiFile *bbi = NULL;
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    bbi = fetchBbiForTrack(track);
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    track->networkErrMsg = cloneString(errCatch->message->string);
    track->drawItems = bigDrawWarning;
    track->totalHeight = bigWarnTotalHeight;
    return;
    }
errCatchFree(&errCatch);

int seqTypeField =  0;
if (sameString(track->tdb->type, "bigPsl"))
    {
    seqTypeField =  bbExtraFieldIndex(bbi, "seqType");
    }

int mouseOverIdx = bbExtraFieldIndex(bbi, mouseOverField);

track->bbiFile = NULL;

struct bigBedFilter *filters = bigBedBuildFilters(cart, bbi, track->tdb) ;
if (compositeChildHideEmptySubtracks(cart, track->tdb, NULL, NULL))
   labelTrackAsHideEmpty(track);

// a fake item that is the union of the items that span the current  window
struct linkedFeatures *spannedLf = NULL;
unsigned filtered = 0;
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
    else if (sameString(tdb->type, "bigDbSnp"))
        {
        // bigDbSnp does not have a score field, but I want to compute the freqSourceIx from
        // trackDb and settings one time instead of for each item, so I'm overloading scoreMin.
        int freqSourceIx = scoreMin;
        lf = lfFromBigDbSnp(tdb, bb, filters, freqSourceIx);
        }
    else
	{
        char startBuf[16], endBuf[16];
        char *bedRow[bbi->fieldCount];
        bigBedIntervalToRow(bb, chromName, startBuf, endBuf, bedRow, ArraySize(bedRow));
        if (bigBedFilterInterval(bedRow, filters))
            {
            struct bed *bed = bedLoadN(bedRow, fieldCount);
            lf = bedMungToLinkedFeatures(&bed, tdb, fieldCount,
                scoreMin, scoreMax, useItemRgb);
            }
        if (track->visibility != tvDense && lf && doWindowSizeFilter && bb->start < winStart && bb->end > winEnd)
            {
            mergeCount++;
            struct bed *bed = bedLoadN(bedRow, fieldCount);
            struct linkedFeatures *tmp = bedMungToLinkedFeatures(&bed, tdb, fieldCount,
                scoreMin, scoreMax, useItemRgb);
            if (spannedLf)
                {
                if (tmp->start < spannedLf->start)
                    spannedLf->start = tmp->start;
                if (tmp->end > spannedLf->end)
                    spannedLf->end = tmp->end;
                if (fieldCount > 9) // average the colors in the merged item
                    {
                    struct rgbColor itemColor = colorIxToRgb(lf->filterColor);
                    struct rgbColor currColor = colorIxToRgb(spannedLf->filterColor);
                    int r = currColor.r + round((itemColor.r - currColor.r) / mergeCount);
                    int g = currColor.g + round((itemColor.g - currColor.g) / mergeCount);
                    int b = currColor.b + round((itemColor.b - currColor.b) / mergeCount);
                    spannedLf->filterColor = MAKECOLOR_32(r,g,b);
                    }
                }
            else
                {
                // setting the label here protects against the case when only one item would
                // have been merged. When this happens we warn in the track longLabel that
                // nothing happened and essentially make the spanned item be what the actual
                // item would be. If multiple items are merged then the labels and mouseOvers
                // will get fixed up later
                tmp->label = bigBedMakeLabel(track->tdb, track->labelColumns,  bb, chromName);
                tmp->mouseOver   = restField(bb, mouseOverIdx);
                slAddHead(&spannedLf, tmp);
                }
            continue; // lf will be NULL, but these items aren't "filtered", they're merged
            }
	}

    if (lf == NULL)
        {
        filtered++;
        continue;
        }

    if (lf->label == NULL)
        lf->label = bigBedMakeLabel(track->tdb, track->labelColumns,  bb, chromName);
    if (sameString(track->tdb->type, "bigGenePred") || startsWith("genePred", track->tdb->type))
        {
        lf->original = genePredFromBigGenePred(chromName, bb); 
        }

    if (lf->mouseOver == NULL)
        {
        char* mouseOver = restField(bb, mouseOverIdx);
        lf->mouseOver   = mouseOver; // leaks some memory, cloneString handles NULL ifself 
        }
    slAddHead(pLfList, lf);
    }

if (filtered)
   labelTrackAsFilteredNumber(track, filtered);

if (doWindowSizeFilter)
    // add the number of merged items to the track longLabel
    {
    char labelBuf[256];
    if (mergeCount > 1)
        safef(labelBuf, sizeof(labelBuf), " (Merged %d items)", mergeCount);
    else
        safef(labelBuf, sizeof(labelBuf), " (No Items Merged in window)");
    track->longLabel = catTwoStrings(track->longLabel, labelBuf);
    }

if (spannedLf)
    {
    // if two or more items were merged together, fix up the label of the special merged item,
    // otherwise the label and mouseOver will be the normal bed one
    char itemLabelBuf[256], mouseOver[256];
    if (mergeCount > 1)
        {
        safef(itemLabelBuf, sizeof(itemLabelBuf), "Merged %d items", mergeCount);
        safef(mouseOver, sizeof(mouseOver), "Merged %d items. Right-click and select 'show merged items' to expand", mergeCount);
        spannedLf->label = cloneString(itemLabelBuf);
        spannedLf->name = "mergedItem"; // always, for correct hgc clicks
        spannedLf->mouseOver = cloneString(mouseOver);
        }
    slAddHead(pLfList, spannedLf);
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
