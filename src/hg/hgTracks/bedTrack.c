/* bedTrack - handle most aspects of a bed-format track (or bigBed). */

#include "common.h"
#include "jksql.h"
#include "bed.h"
#include "hdb.h"
#include "bedCart.h"
#include "bbiFile.h"
#include "bigBed.h"
#include "hgTracks.h"
#include "cds.h"

static char *bbiNameFromTable(struct sqlConnection *conn, char *table)
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
char *fileName = track->bbiFileName;
if (fileName == NULL)
    track->bbiFileName = fileName = bbiNameFromTable(conn, track->mapName);
struct bbiFile *bbi = bigBedFileOpen(fileName);
struct bigBedInterval *result = bigBedIntervalQuery(bbi, chrom, start, end, 0, lm);
bbiFileClose(&bbi);
return result;
}

void loadSimpleBed(struct track *tg)
/* Load the items in one track - just move beds in
 * window... */
{
struct bed *(*loader)(char **row);
struct bed *bed, *list = NULL;
struct sqlConnection *conn = hAllocConnTrack(database, tg->tdb);
char **row;
int rowOffset;
char *words[3];
int wordCt;
char query[128];
char *setting = NULL;
bool doScoreCtFilter = FALSE;
int scoreFilterCt = 0;
char *topTable = NULL;

if (tg->bedSize <= 3)
    loader = bedLoad3;
else if (tg->bedSize == 4)
    loader = bedLoad;
else if (tg->bedSize == 5)
    loader = bedLoad5;
else
    loader = bedLoad6;

/* limit to a specified count of top scoring items.
 * If this is selected, it overrides selecting item by specified score */
if ((setting = trackDbSettingClosestToHome(tg->tdb, "filterTopScorers")) != NULL)
    {
    wordCt = chopLine(cloneString(setting), words);
    if (wordCt >= 3)
        {
        doScoreCtFilter = cartUsualBooleanClosestToHome(cart, tg->tdb, FALSE,
                             "filterTopScorersOn",sameString(words[0], "on"));
        scoreFilterCt = cartUsualIntClosestToHome(cart, tg->tdb, FALSE,
                             "filterTopScorersCt", atoi(words[1]));
        topTable = words[2];
        /* if there are not too many rows in the table then can define */
        /* top table as the track or subtrack table */
        if (sameWord(topTable, "self"))
            topTable = cloneString(tg->mapName);
        }
    }

/* Get list of items */
if (tg->isBigBed)
    {
    struct lm *lm = lmInit(0);
    struct bigBedInterval *bb, *bbList = bigBedSelectRange(conn, tg,
    							   chromName, winStart, winEnd, lm);
    char *bedRow[32];
    char startBuf[16], endBuf[16];

    for (bb = bbList; bb != NULL; bb = bb->next)
        {
	bigBedIntervalToRow(bb, chromName, startBuf, endBuf, bedRow, ArraySize(bedRow));
	bed = loader(bedRow);
	slAddHead(&list, bed);
	}
    lmCleanup(&lm);
    }
else
    {
    struct sqlResult *sr = NULL;
    /* limit to items above a specified score */
    char *scoreFilterClause = getScoreFilterClause(cart, tg->tdb,NULL);
    if (doScoreCtFilter && (topTable != NULL) && hTableExists(database, topTable))
	{
	safef(query, sizeof(query),"select * from %s order by score desc limit %d",
	      topTable, scoreFilterCt);
	sr = sqlGetResult(conn, query);
	rowOffset = hOffsetPastBin(database, hDefaultChrom(database), topTable);
	}
    else if(scoreFilterClause != NULL && tg->bedSize >= 5)
	{
	sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, scoreFilterClause, &rowOffset);
	}
    else
	{
	sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, NULL, &rowOffset);
	}
    freeMem(scoreFilterClause);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	bed = loader(row+rowOffset);
	slAddHead(&list, bed);
	}
    sqlFreeResult(&sr);
    }

if (doScoreCtFilter)
    {
    /* filter out items not in this window */
    struct bed *newList = bedFilterListInRange(list, NULL, chromName, winStart, winEnd);
    list = newList;
    }
slReverse(&list);
hFreeConn(&conn);
tg->items = list;
}

void bed8To12(struct bed *bed)
/* Turn a bed 8 into a bed 12 by defining one block. */
{
// Make up a block: the whole thing.
bed->blockCount  = 1;
bed->blockSizes  = needMem(bed->blockCount * sizeof(int));
bed->chromStarts = needMem(bed->blockCount * sizeof(int));
bed->blockSizes[0]  = bed->chromEnd - bed->chromStart;
bed->chromStarts[0] = 0;
// Some tracks overload thickStart and thickEnd -- catch garbage here.
if ((bed->thickStart != 0) &&
    ((bed->thickStart < bed->chromStart) ||
     (bed->thickStart > bed->chromEnd)))
    bed->thickStart = bed->chromStart;
if ((bed->thickEnd != 0) &&
    ((bed->thickEnd < bed->chromStart) ||
     (bed->thickEnd > bed->chromEnd)))
    bed->thickEnd = bed->chromEnd;
}

struct linkedFeatures *bedMungToLinkedFeatures(struct bed **pBed, struct trackDb *tdb,
	int fieldCount, int scoreMin, int scoreMax, boolean useItemRgb)
/* Convert bed to a linkedFeature, destroying bed in the process. */
{
struct bed *bed = *pBed;
if (fieldCount < 12)
    bed8To12(bed);
adjustBedScoreGrayLevel(tdb, bed, scoreMin, scoreMax);
struct linkedFeatures *lf = lfFromBedExtra(bed, scoreMin, scoreMax);
if (useItemRgb)
    {
    lf->extra = (void *)USE_ITEM_RGB;       /* signal for coloring */
    lf->filterColor=bed->itemRgb;
    }
bedFree(pBed);
return lf;
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

void loadBed9(struct track *tg)
/* Convert bed 9 info in window to linked feature.  (to handle itemRgb)*/
{
struct sqlConnection *conn = hAllocConn(database);
struct bed *bed;
struct linkedFeatures *lfList = NULL, *lf;
struct trackDb *tdb = tg->tdb;
int scoreMin = atoi(trackDbSettingClosestToHomeOrDefault(tdb, "scoreMin", "0"));
int scoreMax = atoi(trackDbSettingClosestToHomeOrDefault(tdb, "scoreMax", "1000"));
boolean useItemRgb = FALSE;

useItemRgb = bedItemRgb(tdb);

if (tg->isBigBed)
    {
    bigBedAddLinkedFeaturesFrom(conn, tg, chromName, winStart, winEnd,
          scoreMin, scoreMax, useItemRgb, 9, &lfList);
    }
else
    {
    struct sqlResult *sr;
    char **row;
    int rowOffset;
    char *scoreFilterClause = getScoreFilterClause(cart, tg->tdb,NULL);
    if (scoreFilterClause != NULL)
	{
	sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd,scoreFilterClause, &rowOffset);
	freeMem(scoreFilterClause);
	}
    else
	{
	sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd,
			 NULL, &rowOffset);
	}

    while ((row = sqlNextRow(sr)) != NULL)
	{
	bed = bedLoadN(row+rowOffset, 9);
	lf = bedMungToLinkedFeatures(&bed, tdb, 9, scoreMin, scoreMax, useItemRgb);
	slAddHead(&lfList, lf);
	}
    sqlFreeResult(&sr);
    }
hFreeConn(&conn);
slReverse(&lfList);
slSort(&lfList, linkedFeaturesCmp);
tg->items = lfList;
}


void loadBed8(struct track *tg)
/* Convert bed 8 info in window to linked feature. */
{
struct sqlConnection *conn = hAllocConn(database);
struct bed *bed;
struct linkedFeatures *lfList = NULL, *lf;
struct trackDb *tdb = tg->tdb;
int scoreMin = atoi(trackDbSettingClosestToHomeOrDefault(tdb, "scoreMin", "0"));
int scoreMax = atoi(trackDbSettingClosestToHomeOrDefault(tdb, "scoreMax", "1000"));
boolean useItemRgb = FALSE;

useItemRgb = bedItemRgb(tdb);

if (tg->isBigBed)
    {
    bigBedAddLinkedFeaturesFrom(conn, tg, chromName, winStart, winEnd,
          scoreMin, scoreMax, useItemRgb, 8, &lfList);
    }
else
    {
    struct sqlResult *sr;
    char **row;
    int rowOffset;
    char *scoreFilterClause = getScoreFilterClause(cart, tg->tdb,NULL);
    if (scoreFilterClause != NULL)
	{
	sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd,scoreFilterClause, &rowOffset);
	freeMem(scoreFilterClause);
	}
    else
	{
	sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd,
			 NULL, &rowOffset);
	}

    while ((row = sqlNextRow(sr)) != NULL)
	{
	bed = bedLoadN(row+rowOffset, 8);
	lf = bedMungToLinkedFeatures(&bed, tdb, 8, scoreMin, scoreMax, useItemRgb);
	slAddHead(&lfList, lf);
	}
    sqlFreeResult(&sr);
    }
hFreeConn(&conn);
slReverse(&lfList);
slSort(&lfList, linkedFeaturesCmp);
tg->items = lfList;
}

static void filterBed(struct track *tg, struct linkedFeatures **pLfList)
/* Apply filters if any to mRNA linked features. */
{
struct linkedFeatures *lf, *next, *newList = NULL, *oldList = NULL;
struct mrnaUiData *mud = tg->extraUiData;
struct mrnaFilter *fil;
char *type;
boolean anyFilter = FALSE;
boolean colorIx = 0;
boolean isExclude = FALSE;
boolean andLogic = TRUE;

if (*pLfList == NULL || mud == NULL)
    return;

/* First make a quick pass through to see if we actually have
 * to do the filter. */
for (fil = mud->filterList; fil != NULL; fil = fil->next)
    {
    fil->pattern = cartUsualString(cart, fil->key, "");
    if (fil->pattern[0] != 0)
        anyFilter = TRUE;
    }
if (!anyFilter)
    return;

type = cartUsualString(cart, mud->filterTypeVar, "red");
if (sameString(type, "exclude"))
    isExclude = TRUE;
else if (sameString(type, "include"))
    isExclude = FALSE;
else
    colorIx = getFilterColor(type, MG_BLACK);
type = cartUsualString(cart, mud->logicTypeVar, "and");
andLogic = sameString(type, "and");

/* Make a pass though each filter, and start setting up search for
 * those that have some text. */
for (fil = mud->filterList; fil != NULL; fil = fil->next)
    {
    fil->pattern = cartUsualString(cart, fil->key, "");
    if (fil->pattern[0] != 0)
	{
	fil->hash = newHash(10);
	}
    }

/* Scan tables id/name tables to build up hash of matching id's. */
for (fil = mud->filterList; fil != NULL; fil = fil->next)
    {
    struct hash *hash = fil->hash;
    int wordIx, wordCount;
    char *words[128];

    if (hash != NULL)
	{
	boolean anyWild;
	char *dupPat = cloneString(fil->pattern);
	wordCount = chopLine(dupPat, words);
	for (wordIx=0; wordIx <wordCount; ++wordIx)
	    {
	    char *pattern = cloneString(words[wordIx]);
	    if (lastChar(pattern) != '*')
		{
		int len = strlen(pattern)+1;
		pattern = needMoreMem(pattern, len, len+1);
		pattern[len-1] = '*';
		}
	    anyWild = (strchr(pattern, '*') != NULL || strchr(pattern, '?') != NULL);
	    touppers(pattern);
	    for(lf = *pLfList; lf != NULL; lf=lf->next)
		{
		char copy[SMALLBUF];
		boolean gotMatch;
		safef(copy, sizeof(copy), "%s", lf->name);
		touppers(copy);
		if (anyWild)
		    gotMatch = wildMatch(pattern, copy);
		else
		    gotMatch = sameString(pattern, copy);
		if (gotMatch)
		    {
		    hashAdd(hash, lf->name, NULL);
		    }
		}
	    freez(&pattern);
	    }
	freez(&dupPat);
	}
    }

/* Scan through linked features coloring and or including/excluding ones that
 * match filter. */
for (lf = *pLfList; lf != NULL; lf = next)
    {
    boolean passed = andLogic;
    next = lf->next;
    for (fil = mud->filterList; fil != NULL; fil = fil->next)
	{
	if (fil->hash != NULL)
	    {
	    if (hashLookup(fil->hash, lf->name) == NULL)
		{
		if (andLogic)
		    passed = FALSE;
		}
	    else
		{
		if (!andLogic)
		    passed = TRUE;
		}
	    }
	}
    if (passed ^ isExclude)
	{
	slAddHead(&newList, lf);
	if (colorIx > 0)
	    lf->filterColor = colorIx;
	}
    else
        {
	slAddHead(&oldList, lf);
	}
    }

slReverse(&newList);
slReverse(&oldList);
if (colorIx > 0)
   {
   /* Draw stuff that passes filter first in full mode, last in dense. */
   if (tg->visibility == tvDense)
       {
       newList = slCat(oldList, newList);
       }
   else
       {
       newList = slCat(newList, oldList);
       }
   }
*pLfList = newList;
tg->limitedVisSet = FALSE;	/* Need to recalculate this after filtering. */

/* Free up hashes, etc. */
for (fil = mud->filterList; fil != NULL; fil = fil->next)
    {
    hashFree(&fil->hash);
    }
}

void loadGappedBed(struct track *tg)
/* Convert bed info in window to linked feature. */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
int rowOffset;
struct bed *bed;
struct linkedFeatures *lfList = NULL, *lf;
struct trackDb *tdb = tg->tdb;
int scoreMin = atoi(trackDbSettingClosestToHomeOrDefault(tdb, "scoreMin", "0"));
int scoreMax = atoi(trackDbSettingClosestToHomeOrDefault(tdb, "scoreMax", "1000"));
boolean useItemRgb = FALSE;

useItemRgb = bedItemRgb(tdb);

if (tg->isBigBed)
    {
    bigBedAddLinkedFeaturesFrom(conn, tg, chromName, winStart, winEnd,
          scoreMin, scoreMax, useItemRgb, 12, &lfList);
    }
else
    {
    /* Use tg->tdb->tableName because subtracks inherit composite track's tdb
     * by default, and the variable is named after the composite track. */
    char *scoreFilterClause = getScoreFilterClause(cart, tg->tdb,NULL);
    if (scoreFilterClause != NULL)
	{
	sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd,scoreFilterClause, &rowOffset);
	freeMem(scoreFilterClause);
	}
    else
	{
	sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, NULL, &rowOffset);
	}
    while ((row = sqlNextRow(sr)) != NULL)
	{
	bed = bedLoad12(row+rowOffset);
	adjustBedScoreGrayLevel(tdb, bed, scoreMin, scoreMax);
	lf = lfFromBedExtra(bed, scoreMin, scoreMax);
	if (useItemRgb)
	    {
	    lf->extra = (void *)USE_ITEM_RGB;       /* signal for coloring */
	    lf->filterColor=bed->itemRgb;
	    }
	slAddHead(&lfList, lf);
	bedFree(&bed);
	}
    sqlFreeResult(&sr);
    }
hFreeConn(&conn);
slReverse(&lfList);
if(tg->extraUiData)
    filterBed(tg, &lfList);
slSort(&lfList, linkedFeaturesCmp);
tg->items = lfList;
}

void bedDrawSimpleAt(struct track *tg, void *item,
	struct hvGfx *hvg, int xOff, int y,
	double scale, MgFont *font, Color color, enum trackVisibility vis)
/* Draw a single simple bed item at position. */
{
struct bed *bed = item;
int heightPer = tg->heightPer;
int x1 = round((double)((int)bed->chromStart-winStart)*scale) + xOff;
int x2 = round((double)((int)bed->chromEnd-winStart)*scale) + xOff;
int w;
struct trackDb *tdb = tg->tdb;
int scoreMin = atoi(trackDbSettingClosestToHomeOrDefault(tdb, "scoreMin", "0"));
int scoreMax = atoi(trackDbSettingClosestToHomeOrDefault(tdb, "scoreMax", "1000"));
char *directUrl = trackDbSetting(tdb, "directUrl");
boolean withHgsid = (trackDbSetting(tdb, "hgsid") != NULL);
boolean thickDrawItem = (trackDbSetting(tdb, "thickDrawItem") != NULL);

if (tg->itemColor != NULL)
    color = tg->itemColor(tg, bed, hvg);
else if (tg->colorShades)
    {
    adjustBedScoreGrayLevel(tdb, bed, scoreMin, scoreMax);
    color = tg->colorShades[grayInRange(bed->score, scoreMin, scoreMax)];
    }
w = x2-x1;
if (w < 1)
    w = 1;
/*	Keep the item at least 4 pixels wide at all viewpoints */
if (thickDrawItem && (w < 4))
    {
    x1 -= ((5-w) >> 1);
    w = 4;
    x2 = x1 + w;
    }
if (color)
    {
    hvGfxBox(hvg, x1, y, w, heightPer, color);
    if (tg->drawName && vis != tvSquish)
	{
	/* Clip here so that text will tend to be more visible... */
	char *s = tg->itemName(tg, bed);
	w = x2-x1;
	if (w > mgFontStringWidth(font, s))
	    {
	    Color textColor = hvGfxContrastingColor(hvg, color);
	    hvGfxTextCentered(hvg, x1, y, w, heightPer, textColor, font, s);
	    }
	mapBoxHgcOrHgGene(hvg, bed->chromStart, bed->chromEnd, x1, y, x2 - x1, heightPer,
                          tg->mapName, tg->mapItemName(tg, bed), NULL, directUrl, withHgsid, NULL);
	}
    }
if (tg->subType == lfWithBarbs || tg->exonArrows)
    {
    int dir = 0;
    if (bed->strand[0] == '+')
	dir = 1;
    else if(bed->strand[0] == '-')
	dir = -1;
    if (dir != 0 && w > 2)
	{
	int midY = y + (heightPer>>1);
	Color textColor = hvGfxContrastingColor(hvg, color);
	clippedBarbs(hvg, x1, midY, w, tl.barbHeight, tl.barbSpacing,
		dir, textColor, TRUE);
	}
    }
}

void bedDrawSimple(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width,
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw simple Bed items. */
{
if (!tg->drawItemAt)
    errAbort("missing drawItemAt in track %s", tg->mapName);

genericDrawItems(tg, seqStart, seqEnd, hvg, xOff, yOff, width,
	font, color, vis);
}

char *bedName(struct track *tg, void *item)
/* Return name of bed track item. */
{
struct bed *bed = item;
if (bed->name == NULL)
    return "";
return bed->name;
}

int bedItemStart(struct track *tg, void *item)
/* Return start position of item. */
{
struct bed *bed = item;
return bed->chromStart;
}

int bedItemEnd(struct track *tg, void *item)
/* Return end position of item. */
{
struct bed *bed = item;
return bed->chromEnd;
}

void freeSimpleBed(struct track *tg)
/* Free the beds in a track group that has
   beds as its items. */
{
bedFreeList(((struct bed **)(&tg->items)));
}

void bedMethods(struct track *tg)
/* Fill in methods for (simple) bed tracks. */
{
tg->drawItems = bedDrawSimple;
tg->drawItemAt = bedDrawSimpleAt;
tg->itemName = bedName;
tg->mapItemName = bedName;
tg->totalHeight = tgFixedTotalHeightNoOverflow;
tg->itemHeight = tgFixedItemHeight;
tg->itemStart = bedItemStart;
tg->itemEnd = bedItemEnd;
tg->labelNextPrevItem = linkedFeaturesLabelNextPrevItem;
tg->freeItems = freeSimpleBed;
}

struct linkedFeatures *simpleBedToLinkedFeatures(struct bed *b, int bedFields, boolean everyBase)
/* Create a linked feature from a single bed item
 * Any bed fields past the 6th field (strand) will be ignored
 * Make one simpleFeature for every base of the bed if everyBase is TRUE,
 * otherwise it will contain a single 'exon' corresponding to the bed (start,end)
 * Dont free the bed as a pointer to each item is stored in lf->original 
 */
{
struct linkedFeatures *lf = NULL;
if (b)
    {
    AllocVar(lf);
    lf->start = lf->tallStart = b->chromStart;
    lf->end = lf->tallEnd = b->chromEnd;
    struct simpleFeature *sf;
    int s;
    int stepSize = everyBase ? 1 : lf->end - lf->start;
    lf->components = NULL;
    for (s = lf->start ; s < lf->end ; s += stepSize)
	{
	AllocVar(sf);
	sf->start = s;
	sf->end = sf->start + stepSize;
	sf->qStart = s - lf->start;
	sf->qEnd = sf->qStart + stepSize;
	slAddHead(&lf->components, sf);
	}
    slReverse(&lf->components);
    if (bedFields > 3)
	safecpy(lf->name, sizeof(lf->name), b->name);
    if (bedFields > 4)
	lf->score = b->score;
    if (bedFields > 5)
	lf->orientation = (b->strand[0] == '+' ? 1 : (b->strand[0] == '-' ? -1 : 0)); 
    lf->original = b;
    }
return lf;
}

struct linkedFeatures *simpleBedListToLinkedFeatures(struct bed *b, int bedFields, boolean everyBase)
/* Create a list of linked features from a list of beds */
{
struct linkedFeatures *lfList = NULL;
while (b)
    {
    slAddHead(&lfList, simpleBedToLinkedFeatures(b, bedFields, everyBase));
    b = b->next;
    }
slReverse(&lfList);
return lfList;
}

void loadSimpleBedAsLinkedFeaturesPerBase(struct track *tg)
/* bed list not freed as pointer to it is stored in 'original' field */
{
loadSimpleBed(tg);
tg->items = simpleBedListToLinkedFeatures(tg->items, tg->bedSize, TRUE);
}


Color lfItemColorByStrand(struct track *tg, void *item, struct hvGfx *hvg)
/* Look up the RGB color from the trackDb setting 'colorByStrand' based on
 * the linkedFeature item orientation, and convert this to a color index
 * using hvGfx */
{
struct linkedFeatures *lf = item;
char *words[3];
unsigned char r, g, b;

char *colors = cloneString(trackDbSetting(tg->tdb, "colorByStrand"));
if (!colors)
    errAbort("colorByStrand setting missing (in %s)", tg->mapName);
if (chopByWhite(colors, words, sizeof(words)) != 2)
    errAbort("invalid colorByStrand setting %s (expecting pair of RGB values r,g,b r,g,b)", colors);
if (lf->orientation == 1)
    parseColor(words[0], &r, &g, &b);
else if (lf->orientation == -1) 
    parseColor(words[1], &r, &g, &b);
else // return the main color
    {
    r = tg->color.r; g = tg->color.g; b = tg->color.b;
    }
freez(&colors);
return hvGfxFindColorIx(hvg, r, g, b);
}


void complexBedMethods(struct track *track, struct trackDb *tdb, boolean isBigBed,
                                int wordCount, char *words[])
/* Fill in methods for more complex bed tracks. */
{
int fieldCount = 3;
boolean useItemRgb = FALSE;

useItemRgb = bedItemRgb(tdb);

if (wordCount > 1)
    fieldCount = atoi(words[1]);

track->bedSize = fieldCount;
track->isBigBed = isBigBed;

if (fieldCount < 8)
    {
    if (baseColorGetDrawOpt(track) != baseColorDrawOff)
	{
	// data must be loaded as bed and converted to linkedFeatures 
	// to draw each base character must make one simpleFeature per base
	linkedFeaturesMethods(track);
	track->loadItems = loadSimpleBedAsLinkedFeaturesPerBase;
	if (trackDbSetting(tdb, "colorByStrand"))
	    track->itemColor = lfItemColorByStrand;
	}
    else
	{
	bedMethods(track);
	track->loadItems = loadSimpleBed;
	}
    }
else if (useItemRgb && fieldCount == 9)
    {
    linkedFeaturesMethods(track);
    track->loadItems = loadBed9;
    }
else if (fieldCount < 12)
    {
    linkedFeaturesMethods(track);
    track->loadItems = loadBed8;
    }
else
    {
    linkedFeaturesMethods(track);
    track->extraUiData = newBedUiData(track->mapName);
    track->loadItems = loadGappedBed;
    }
}
