/* bigBed - stuff to handle loading and display of bigBed type tracks in browser. 
 * Mostly just links to bed code, but handles a few things itself, like the dense
 * drawing code. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

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
#include "chromAlias.h"
#include "quickLift.h"
#include "hgConfig.h"
#include "heatmap.h"

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

struct bigBedFilter *bigBedMakeNumberFilter(struct cart *cart, struct bbiFile *bbi, struct trackDb *tdb, char *filter, char *defaultLimits,  char *field, boolean isHighlight)
/* Make a filter/highlight on this column if the trackDb or cart wants us to. */
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
        if (isHighlight)
            ret->isHighlight = TRUE;
        }
    }
return ret;
}

struct bigBedFilter *bigBedMakeFilterText(struct cart *cart, struct bbiFile *bbi, struct trackDb *tdb, char *filterName, char *field, boolean isHighlight)
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

filter->isHighlight = isHighlight;

return filter;
}

char *getHighlightType(struct cart *cart, struct trackDb *tdb, char *field, char *def)
{
char settingString[4096];
safef(settingString, sizeof settingString, "%s.%s", HIGHLIGHT_TYPE_NAME_LOW, field);
char *setting = cartOrTdbString(cart, tdb, settingString, NULL);
if (setting == NULL)
    {
    safef(settingString, sizeof settingString, "%s.%s", field, HIGHLIGHT_TYPE_NAME_CAP);
    setting = cartOrTdbString(cart, tdb, settingString, NULL);
    }
if (setting == NULL)
    {
    safef(settingString, sizeof settingString, "%s%s", field, HIGHLIGHT_TYPE_NAME_CAP);
    setting = cartOrTdbString(cart, tdb, settingString, def);
    }
return setting;
}

struct bigBedFilter *bigBedMakeFilterBy(struct cart *cart, struct bbiFile *bbi, struct trackDb *tdb, char *field, struct slName *choices, boolean isHighlight)
/* Add a bigBed filter using a trackDb filterBy statement. */
{
struct bigBedFilter *filter;
char *setting = NULL;
if (isHighlight)
    setting = getHighlightType(cart, tdb, field, HIGHLIGHTBY_DEFAULT);
else
setting = getFilterType(cart, tdb, field,  FILTERBY_DEFAULT);

AllocVar(filter);
filter->fieldNum =  getFieldNum(bbi, field);
filter->comparisonType = COMPARE_HASH;
if (setting) 
    {
    if (sameString(setting, FILTERBY_SINGLE_LIST) 
            || sameString(setting, FILTERBY_MULTIPLE_LIST_OR)
            || sameString(setting, FILTERBY_MULTIPLE_LIST_ONLY_OR)
            || sameString(setting, HIGHLIGHTBY_SINGLE_LIST)
            || sameString(setting, HIGHLIGHTBY_MULTIPLE_LIST_OR)
            || sameString(setting, HIGHLIGHTBY_MULTIPLE_LIST_ONLY_OR))
                filter->comparisonType = COMPARE_HASH_LIST_OR;
    else if (sameString(setting, FILTERBY_MULTIPLE_LIST_AND) 
            || sameString(setting, FILTERBY_MULTIPLE_LIST_ONLY_AND)
            || sameString(setting, HIGHLIGHTBY_MULTIPLE_LIST_AND)
            || sameString(setting, HIGHLIGHTBY_MULTIPLE_LIST_ONLY_AND))
                filter->comparisonType = COMPARE_HASH_LIST_AND;
    }
filter->valueHash = newHash(5);
filter->numValuesInHash = slCount(choices);

for(; choices; choices = choices->next)
    hashStore(filter->valueHash, choices->name);

filter->isHighlight = isHighlight;
return filter;
}

static void addGencodeFilters(struct cart *cart, struct trackDb *tdb, struct bigBedFilter **pFilters)
/* Add GENCODE custom bigBed filters. */
{
struct bigBedFilter *filter;
char varName[64];
struct hash *hash;

/* canonical */
safef(varName, sizeof(varName), "%s.show.spliceVariants", tdb->track);
boolean option = cartUsualBoolean(cart, varName, TRUE);
if (!option)
    {
    AllocVar(filter);
    slAddHead(pFilters, filter);
    filter->fieldNum = 25;
    filter->comparisonType = COMPARE_HASH_LIST_OR;
    hash = newHash(5);
    filter->valueHash = hash;
    filter->numValuesInHash = 1;
    hashStore(hash, "canonical" );
    }

/* transcript class */
AllocVar(filter);
slAddHead(pFilters, filter);
filter->fieldNum = 20;
filter->comparisonType = COMPARE_HASH;
hash = newHash(5);
filter->valueHash = hash;
filter->numValuesInHash = 1;
hashStore(hash,"coding");  // coding is always included

safef(varName, sizeof(varName), "%s.show.noncoding", tdb->track);
if (cartUsualBoolean(cart, varName, TRUE))
    {
    filter->numValuesInHash++;
    hashStore(hash,"nonCoding");
    }

safef(varName, sizeof(varName), "%s.show.pseudo", tdb->track);
if (cartUsualBoolean(cart, varName, FALSE))
    {
    filter->numValuesInHash++;
    hashStore(hash,"pseudo");
    }

/* tagged sets */
safef(varName, sizeof(varName), "%s.show.set", tdb->track);
char *setString = cartUsualString(cart, varName, "basic");

if (differentString(setString, "all"))
    {
    AllocVar(filter);
    slAddHead(pFilters, filter);
    filter->fieldNum = 23;
    filter->comparisonType = COMPARE_HASH_LIST_OR;
    hash = newHash(5);
    filter->valueHash = hash;
    filter->numValuesInHash = 1;
    hashStore(hash, setString);
    }
}

struct bigBedFilter *bigBedBuildFilters(struct cart *cart, struct bbiFile *bbi, struct trackDb *tdb)
/* Build all the numeric and filterBy filters for a bigBed */
{
struct bigBedFilter *filters = NULL, *filter;
struct trackDbFilter *tdbFilters = tdbGetTrackNumFilters(tdb);

if ((tdbFilters == NULL) && !trackDbSettingOn(tdb, "noScoreFilter") && (bbi->definedFieldCount >= 5))
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
    if ((filter = bigBedMakeNumberFilter(cart, bbi, tdb, tdbFilters->name, NULL, tdbFilters->fieldName, FALSE)) != NULL)
        slAddHead(&filters, filter);
    }

tdbFilters = tdbGetTrackTextFilters(tdb);

for(; tdbFilters; tdbFilters = tdbFilters->next)
    {
    if ((filter = bigBedMakeFilterText(cart, bbi, tdb, tdbFilters->name,  tdbFilters->fieldName, FALSE)) != NULL)
        slAddHead(&filters, filter);
    }

filterBy_t *filterBySet = filterBySetGet(tdb, cart,NULL);
filterBy_t *filterBy = filterBySet;
for (;filterBy != NULL; filterBy = filterBy->next)
    {
    if (filterBy->slChoices && differentString(filterBy->slChoices->name, "All")) 
        {
        if ((filter = bigBedMakeFilterBy(cart, bbi, tdb, filterBy->column, filterBy->slChoices, FALSE)) != NULL)
            slAddHead(&filters, filter);
        }
    }

/* custom gencode filters */
boolean isGencode3 = trackDbSettingOn(tdb, "isGencode3");

if (isGencode3)
    addGencodeFilters(cart, tdb, &filters);

return filters;
}

struct bigBedFilter *bigBedBuildHighlights(struct cart *cart, struct bbiFile *bbi, struct trackDb *tdb)
/* Build all the numeric and highlights for a bigBed */
{
struct bigBedFilter *highlights = NULL, *highlight;
struct trackDbFilter *tdbHighlights = tdbGetTrackNumHighlights(tdb);

for(; tdbHighlights; tdbHighlights = tdbHighlights->next)
    {
    if ((highlight = bigBedMakeNumberFilter(cart, bbi, tdb, tdbHighlights->name, NULL, tdbHighlights->fieldName, TRUE)) != NULL)
        slAddHead(&highlights, highlight);
    }

tdbHighlights = tdbGetTrackTextHighlights(tdb);

for(; tdbHighlights; tdbHighlights = tdbHighlights->next)
    {
    if ((highlight = bigBedMakeFilterText(cart, bbi, tdb, tdbHighlights->name,  tdbHighlights->fieldName, TRUE)) != NULL)
        slAddHead(&highlights, highlight);
    }

filterBy_t *filterBySet = highlightBySetGet(tdb, cart,NULL);
filterBy_t *filterBy = filterBySet;
for (;filterBy != NULL; filterBy = filterBy->next)
    {
    if (filterBy->slChoices && differentString(filterBy->slChoices->name, "All"))
        {
        if ((highlight = bigBedMakeFilterBy(cart, bbi, tdb, filterBy->column, filterBy->slChoices, TRUE)) != NULL)
            slAddHead(&highlights, highlight);
        }
    }

return highlights;
}

boolean bigBedFilterOne(struct bigBedFilter *filter, char **bedRow, struct bbiFile *bbi)
/* Return TRUE if a bedRow passes one filter or is in hgFindMatches */
{
if ((bbi->definedFieldCount > 3) && (hgFindMatches != NULL) && 
    (bedRow[3] != NULL)  && hashLookup(hgFindMatches, bedRow[3]) != NULL)
    return TRUE;

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
return TRUE;
}


boolean bigBedFilterInterval(struct bbiFile *bbi, char **bedRow, struct bigBedFilter *filters)
/* Go through a row and filter based on filters.  Return TRUE if all filters are passed. */
{
if ((bbi->definedFieldCount > 3) && (hgFindMatches != NULL) && 
    (bedRow[3] != NULL)  && hashLookup(hgFindMatches, bedRow[3]) != NULL)
    return TRUE;

struct bigBedFilter *filter;
for(filter = filters; filter; filter = filter->next)
    if (!bigBedFilterOne(filter, bedRow, bbi))
        return FALSE;
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

    bbi = track->bbiFile = bigBedFileOpenAlias(fileName, chromAliasFindAliases);
    }
return bbi;
}

static unsigned bigBedMaxItems()
/* Get the maximum number of items to grab from a bigBed file.  Defaults to ten thousand . */
{
static boolean set = FALSE;
static unsigned maxItems = 0;

if (!set)
    {
    char *maxItemsStr = cfgOptionDefault("bigBedMaxItems", "10000");

    maxItems = sqlUnsigned(maxItemsStr);
    set = TRUE;
    }

return maxItems;
}

static boolean hasOverflowedInWindow(struct track *track)
/* has it overlowed in the given track? */
{
uint resultCount = 0;  // do not use -1, it messes up the compare of signed resultsCount with unsigned bigMaxItems.
char *resultCountString = trackDbSetting(track->tdb, "bigBedItemsCount");
if (resultCountString)
    resultCount = sqlUnsigned(resultCountString);
return (resultCount > bigBedMaxItems());
}


static void loadBigBedSummary(struct track *track)
/* Check if summary loading needed for bigBed */
{
if (track->subtracks)  // do tracks or subtracks but not parents.
    {
    return;
    }

struct lm *lm = lmInit(0);

char *chrom = chromName;
int start = winStart;
int end = winEnd;

/* protect against temporary network error */
struct errCatch *errCatch = errCatchNew();
boolean filtering = FALSE; // for the moment assume we're not filtering
if (errCatchStart(errCatch))
    {
    // scan all windows for errors and overflows
    boolean errorsInWindows = FALSE;
    boolean overFlowedInWindows = FALSE;

    struct track *thisTrack;
    for(thisTrack=track->prevWindow; thisTrack; thisTrack=thisTrack->prevWindow)
        {
        if (hasOverflowedInWindow(thisTrack))
	    overFlowedInWindows = TRUE;
        if (thisTrack->drawItems == bigDrawWarning)
	    errorsInWindows = TRUE;
        }
    for(thisTrack=track->nextWindow; thisTrack; thisTrack=thisTrack->nextWindow)
        {
        if (hasOverflowedInWindow(thisTrack))
	    overFlowedInWindows = TRUE;
        if (thisTrack->drawItems == bigDrawWarning)
	    errorsInWindows = TRUE;
        }
    
    if (hasOverflowedInWindow(track))
	overFlowedInWindows = TRUE;

    if (!errorsInWindows && overFlowedInWindows)
	{
        if (filtering)
            errAbort("Too many items in window to filter.Zoom in or remove filters to view track.");
        else
            {
	    struct bbiFile *bbi = fetchBbiForTrack(track);
            if (bbi)
		{
		// use summary levels
		if (track->visibility != tvDense)
		    {
		    track->limitedVis = tvFull;
		    track->limitWiggle = TRUE;
		    track->limitedVisSet = TRUE;
		    }
		else
		    {
		    track->limitedVis = tvDense;
		    track->limitedVisSet = TRUE;
		    }
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
	    }
        }
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    track->networkErrMsg = cloneString(errCatch->message->string);
    track->drawItems = bigDrawWarning;
    track->totalHeight = bigWarnTotalHeight;
    }
errCatchFree(&errCatch);
lmCleanup(&lm);
track->bbiFile = NULL;

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
    result = bigBedIntervalQuery(bbi, chrom, start, end, bigBedMaxItems() + 1, lm);  // pass in desired limit or 0 for all.

    char resultCount[32];
    safef(resultCount, sizeof resultCount, "%u", slCount(result));
    trackDbAddSetting(track->tdb, "bigBedItemsCount", resultCount);

    if (slCount(result) > bigBedMaxItems())
	    {
            result = NULL;  // IS Having it return NULL a critical part of this?
	    }

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

void addHighlightToLinkedFeature(struct linkedFeatures *lf, struct bigBedFilter *highlights, struct bbiFile *bbi, char **bedRow, struct trackDb *tdb)
/* Fill out the lf->highlightColor if the cart says to highlight this item. The color will
 * be the 'average' of all the highlight colors specified */
{
struct bigBedFilter *highlight;
char *cartHighlightColor = cartOrTdbString(cart, tdb, HIGHLIGHT_COLOR_CART_VAR, HIGHLIGHT_COLOR_DEFAULT);
for (highlight = highlights; highlight != NULL; highlight = highlight->next)
    {
    if (bigBedFilterOne(highlight, bedRow, bbi))
        {
        unsigned rgb = bedParseColor(cartHighlightColor);
        Color color = bedColorToGfxColor(rgb);
        lf->highlightColor = color;
        lf->highlightMode = highlightBackground;
        }
    }
}

void bigBedAddLinkedFeaturesFromExt(struct track *track,
	char *chrom, int start, int end, int scoreMin, int scoreMax, boolean useItemRgb,
	int fieldCount, struct linkedFeatures **pLfList, int maxItems)
/* Read in items in chrom:start-end from bigBed file named in track->bbiFileName, convert
 * them to linkedFeatures, and add to head of list. */
{
struct lm *lm = lmInit(0);
struct trackDb *tdb = track->tdb;
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

fieldCount = track->bedSize;
boolean bigBedOnePath = cfgOptionBooleanDefault("bigBedOnePath", TRUE);
if (bigBedOnePath  && (fieldCount == 0))
    track->bedSize = fieldCount = bbi->definedFieldCount;

struct bigBedInterval *bb, *bbList; 
char *quickLiftFile = cloneString(trackDbSetting(track->tdb, "quickLiftUrl"));
struct hash *chainHash = NULL;
if (quickLiftFile)
    bbList = quickLiftGetIntervals(quickLiftFile, bbi, chromName, winStart, winEnd, &chainHash);
else
    bbList = bigBedSelectRangeExt(track, chrom, start, end, lm, maxItems);

char *squishField = cartOrTdbString(cart, track->tdb, "squishyPackField", NULL);
int squishFieldIdx = bbExtraFieldIndex(bbi, squishField);

int seqTypeField =  0;
if (sameString(track->tdb->type, "bigPsl"))
    {
    seqTypeField =  bbExtraFieldIndex(bbi, "seqType");
    }

int mouseOverIdx = bbExtraFieldIndex(bbi, mouseOverField);

track->bbiFile = NULL;

struct bigBedFilter *filters = bigBedBuildFilters(cart, bbi, track->tdb) ;
struct bigBedFilter *highlights = bigBedBuildHighlights(cart, bbi, track->tdb) ;
if (compositeChildHideEmptySubtracks(cart, track->tdb, NULL, NULL))
   labelTrackAsHideEmpty(track);

// mouseOvers can be built constructed via trackDb settings instead
// of embedded directly in bigBed
char *mouseOverPattern = NULL;
char **fieldNames = NULL;
if (!mouseOverIdx)
    {
    mouseOverPattern = cartOrTdbString(cart, track->tdb, "mouseOver", NULL);

    if (mouseOverPattern)
        {
        AllocArray(fieldNames, bbi->fieldCount);
        struct slName *field = NULL, *fields = bbFieldNames(bbi);
        int i =  0;
        for (field = fields; field != NULL; field = field->next)
            fieldNames[i++] = field->name;
        }
    }

// a fake item that is the union of the items that span the current  window
struct linkedFeatures *spannedLf = NULL;
unsigned filtered = 0;
struct bed *bed = NULL, *bedCopy = NULL;
for (bb = bbList; bb != NULL; bb = bb->next)
    {
    struct linkedFeatures *lf = NULL;
    char *bedRow[bbi->fieldCount];
    if (sameString(track->tdb->type, "bigPsl"))
        {
        // fill out bedRow to support mouseOver pattern replacements
        char startBuf[16], endBuf[16];
        bigBedIntervalToRow(bb, chromName, startBuf, endBuf, bedRow, ArraySize(bedRow));
        char *seq, *cds;
        struct psl *psl = pslFromBigPsl(chromName, bb, seqTypeField,  &seq, &cds);
        int sizeMul =  pslIsProtein(psl) ? 3 : 1;
        boolean isXeno = 0;  // just affects grayIx
        boolean nameGetsPos = FALSE; // we want the name to stay the name

        if (sizeMul == 3)
            {
            // these tags are not currently supported by the drawing engine for protein psl
            hashRemove(track->tdb->settingsHash, "showDiffBasesAllScales");
            hashRemove(track->tdb->settingsHash, "baseColorUseSequence");
            hashRemove(track->tdb->settingsHash, "baseColorDefault");
            }

        lf = lfFromPslx(psl, sizeMul, isXeno, nameGetsPos, track);
        lf->original = psl;
        if ((seq != NULL) && (lf->orientation == -1))
            reverseComplement(seq, strlen(seq));
        lf->extra = seq;
        lf->cds = cds;
        lf->useItemRgb = useItemRgb;
        if ( lf->useItemRgb )
            lf->filterColor = itemRgbColumn(bedRow[8]);
        }
    else if (sameString(tdb->type, "bigDbSnp"))
        {
        // bigDbSnp does not have a score field, but I want to compute the freqSourceIx from
        // trackDb and settings one time instead of for each item, so I'm overloading scoreMin.
        int freqSourceIx = scoreMin;
        lf = lfFromBigDbSnp(tdb, bb, filters, freqSourceIx, bbi, chainHash);
        }
    else
	{
        char startBuf[16], endBuf[16];
        bigBedIntervalToRow(bb, chromName, startBuf, endBuf, bedRow, ArraySize(bedRow));
        if (bigBedFilterInterval(bbi, bedRow, filters))
            {
            if (quickLiftFile)
                {
                if ((bed = quickLiftIntervalsToBed(bbi, chainHash, bb)) != NULL)
                    {
                    bedCopy = cloneBed(bed);
                    lf = bedMungToLinkedFeatures(&bed, tdb, fieldCount,
                        scoreMin, scoreMax, useItemRgb);
                    }
                }
            else
                {
                bed = bedLoadN(bedRow, fieldCount);
                bedCopy = cloneBed(bed);
                lf = bedMungToLinkedFeatures(&bed, tdb, fieldCount,
                    scoreMin, scoreMax, useItemRgb);
                }
            }

        if (lf && highlights)
            addHighlightToLinkedFeature(lf, highlights, bbi, bedRow, track->tdb);

        if (lf && squishFieldIdx)
            lf->squishyPackVal = atof(restField(bb, squishFieldIdx));

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
                    // Not sure how averaging alphas in the merged item would work; probably better
                    // to ignore it.
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
                if (mouseOverIdx > 0)
                    tmp->mouseOver = restField(bb, mouseOverIdx);
                else if (mouseOverPattern)
                    tmp->mouseOver = replaceFieldInPattern(mouseOverPattern, bbi->fieldCount, fieldNames, bedRow);
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
    if (startsWith("bigGenePred", track->tdb->type) || startsWith("genePred", track->tdb->type))
        {
        // bedRow[5] has original strand in it, bedCopy has new strand.  If they're different we want to reverse exonFrames
        boolean changedStrand = FALSE;
        if (quickLiftFile)
            {
            if (*bedRow[5] != *bedCopy->strand)
                changedStrand = TRUE;
            }
        lf->original = genePredFromBedBigGenePred(chromName, bedCopy, bb, changedStrand); 
        }

    if (startsWith("bigBed", track->tdb->type))
        {
        // Clone bb so that we'll have access the to extra fields contents.  This is used in
        // alternate display modes for bigBeds (so far just "heatmap", but more are likely to come).
        struct bigBedInterval *bbCopy = CloneVar(bb);
        if (bbCopy->rest)
            bbCopy->rest = cloneMem(bbCopy->rest, strlen(bbCopy->rest)+1);
        bbCopy->next = NULL;
        lf->original = bbCopy;
        }

    if (lf->mouseOver == NULL)
        {
        if (mouseOverIdx > 0)
            lf->mouseOver = restField(bb, mouseOverIdx);
        else if (mouseOverPattern)
            lf->mouseOver = replaceFieldInPattern(mouseOverPattern, bbi->fieldCount, fieldNames, bedRow);
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
        safef(mouseOver, sizeof(mouseOver), "Merged %d items. Click to expand or right-click and select 'show merged items'", mergeCount);
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
maybeDrawQuickLiftLines(tg, seqStart, seqEnd, hvg, xOff, yOff, width, font, color, tvDense);
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

#ifdef NOTNOW
static int getFieldCount(struct track *track)
// return the definedFieldCount of the passed track with is assumed to be a bigBed
{
struct bbiFile *bbi = NULL;
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    bbi = fetchBbiForTrack(track);
    }
errCatchEnd(errCatch);

if (bbi)
    return bbi->definedFieldCount;

return 3; // if we can't get the bbi, use the minimum
}
#endif

void commonBigBedMethods(struct track *track, struct trackDb *tdb, 
                                int wordCount, char *words[])
/* Set up common bigBed methods used by several track types that depend on the bigBed format. */
{
boolean bigBedOnePath = cfgOptionBooleanDefault("bigBedOnePath", TRUE);

if (bigBedOnePath)
    {
    track->isBigBed = TRUE;
    linkedFeaturesMethods(track);
    track->extraUiData = newBedUiData(track->track);
    track->loadItems = loadGappedBed;

    if (wordCount > 1)
        track->bedSize = atoi(words[1]);

    if (trackDbSetting(tdb, "colorByStrand"))
        {
        Color lfItemColorByStrand(struct track *tg, void *item, struct hvGfx *hvg);
        track->itemColor = lfItemColorByStrand;
        }
    }
else 
    {
    char *newWords[wordCount];

    int ii;
    for(ii=0; ii < wordCount; ii++)
        newWords[ii] = words[ii];

    #ifdef NOTNOW
    // let's help the user out and get the definedFieldCount if they didn't specify it on the type line
    if (!tdbIsSuper(track->tdb) && (track->tdb->subtracks == NULL) && (wordCount == 1) && sameString(words[0], "bigBed"))
        {
        int fieldCount = getFieldCount(track);
        if (fieldCount > 3) 
            {
            char buffer[1024];
            safef(buffer, sizeof buffer, "%d", fieldCount);
            newWords[1] = cloneString(buffer);
            wordCount = 2;
            }
        }
    #endif
    complexBedMethods(track, tdb, TRUE, wordCount, newWords);
    }
track->loadSummary = loadBigBedSummary;
}

void bigBedMethods(struct track *track, struct trackDb *tdb, 
                                int wordCount, char *words[])
/* Set up bigBed methods for tracks that are type bigBed. */
{
commonBigBedMethods(track, tdb, wordCount, words);
if (sameWordOk(trackDbSetting(tdb, "style"), "heatmap"))
    {
    // Might want to check here if required heatmap settings/fields are in place,
    // maybe some combo of row count and labels.
    heatmapMethods(track);
    }
}

