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

#define SEQ_DELIM '~'

struct bed *bedLoadPairedTagAlign(char **row)
/* Load first six fields of bed. 
 * Add ~seq1~seq2 to end of name
 * Then remove the sequence to extra field when we convert to linkedFeature.
 * Assumes seq1 and seq2 are in row[6] and row[7], as they would be with a 
 * pairedTagAlign type (hg/lib/encode/pairedTagAlign.as). It would be good to be
 * able to check these columns exist but we dont have the sqlResult here. */
{
char buf[1024];
struct bed *ret = bedLoad6(row);
safef(buf, sizeof(buf), "%s%c%s%c%s", ret->name, SEQ_DELIM, row[6], SEQ_DELIM, row[7]);
freez(&(ret->name));
ret->name = cloneString(buf);
return ret;
}

void loadSimpleBed(struct track *tg)
/* Load the items in one track - just move beds in
 * window... */
{
struct bed *(*loader)(char **row);
struct bed *bed, *list = NULL;
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

// pairedTagAlign loader is required for base coloring using sequence from seq1 & seq2
// after removing optional bin column, this loader assumes seq1 and seq2 are in
// row[6] and row[7] respectively of the sql result.
if ((setting = trackDbSetting(tg->tdb, BASE_COLOR_USE_SEQUENCE)) 
	&& sameString(setting, "seq1Seq2"))
    loader = bedLoadPairedTagAlign;

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
            topTable = cloneString(tg->table);
        }
    }

/* Get list of items */
if (tg->isBigBed)
    { // avoid opening an unneeded db connection for bigBed; required not to use mysql for parallel fetch tracks
    char *scoreFilter = cartOrTdbString(cart, tg->tdb, "scoreFilter", NULL);
    struct lm *lm = lmInit(0);
    struct bigBedInterval *bb, *bbList = bigBedSelectRange(tg, chromName, winStart, winEnd, lm);
    char *bedRow[32];
    char startBuf[16], endBuf[16];
    int minScore = 0;
    if (scoreFilter)
	minScore = atoi(scoreFilter);

    for (bb = bbList; bb != NULL; bb = bb->next)
	{
	bigBedIntervalToRow(bb, chromName, startBuf, endBuf, bedRow, ArraySize(bedRow));
	bed = loader(bedRow);
	if (scoreFilter == NULL || bed->score >= minScore)
	    slAddHead(&list, bed);
	}
    lmCleanup(&lm);
    }
else
    {
    struct sqlConnection *conn = hAllocConnTrack(database, tg->tdb);
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
	sr = hRangeQuery(conn, tg->table, chromName, winStart, winEnd, scoreFilterClause, &rowOffset);
	}
    else
	{
	sr = hRangeQuery(conn, tg->table, chromName, winStart, winEnd, NULL, &rowOffset);
	}
    freeMem(scoreFilterClause);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	bed = loader(row+rowOffset);
	slAddHead(&list, bed);
	}
    sqlFreeResult(&sr);
    hFreeConn(&conn);
    }

if (doScoreCtFilter)
    {
    /* filter out items not in this window */
    struct bed *newList = bedFilterListInRange(list, NULL, chromName, winStart, winEnd);
    list = newList;
    }
slReverse(&list);
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

void loadBed9(struct track *tg)
/* Convert bed 9 info in window to linked feature.  (to handle itemRgb)*/
{
struct trackDb *tdb = tg->tdb;
struct bed *bed;
struct linkedFeatures *lfList = NULL, *lf;
int scoreMin = atoi(trackDbSettingClosestToHomeOrDefault(tdb, "scoreMin", "0"));
int scoreMax = atoi(trackDbSettingClosestToHomeOrDefault(tdb, "scoreMax", "1000"));
boolean useItemRgb = FALSE;

useItemRgb = bedItemRgb(tdb);

if (tg->isBigBed)
    { // avoid opening an unneeded db connection for bigBed; required not to use mysql for parallel fetch tracks
    bigBedAddLinkedFeaturesFrom(tg, chromName, winStart, winEnd,
          scoreMin, scoreMax, useItemRgb, 9, &lfList);
    }
else
    {
    struct sqlConnection *conn = hAllocConnTrack(database, tdb);
    struct sqlResult *sr;
    char **row;
    int rowOffset;
    char *scoreFilterClause = getScoreFilterClause(cart, tg->tdb,NULL);
    if (scoreFilterClause != NULL)
	{
	sr = hRangeQuery(conn, tg->table, chromName, winStart, winEnd,scoreFilterClause, &rowOffset);
	freeMem(scoreFilterClause);
	}
    else
	{
	sr = hRangeQuery(conn, tg->table, chromName, winStart, winEnd,
			 NULL, &rowOffset);
	}

    while ((row = sqlNextRow(sr)) != NULL)
	{
	bed = bedLoadN(row+rowOffset, 9);
	lf = bedMungToLinkedFeatures(&bed, tdb, 9, scoreMin, scoreMax, useItemRgb);
	slAddHead(&lfList, lf);
	}
    sqlFreeResult(&sr);
    hFreeConn(&conn);
    }
slReverse(&lfList);
slSort(&lfList, linkedFeaturesCmp);
tg->items = lfList;
}


void loadBed8(struct track *tg)
/* Convert bed 8 info in window to linked feature. */
{
struct bed *bed;
struct linkedFeatures *lfList = NULL, *lf;
struct trackDb *tdb = tg->tdb;
int scoreMin = atoi(trackDbSettingClosestToHomeOrDefault(tdb, "scoreMin", "0"));
int scoreMax = atoi(trackDbSettingClosestToHomeOrDefault(tdb, "scoreMax", "1000"));
boolean useItemRgb = FALSE;

useItemRgb = bedItemRgb(tdb);

if (tg->isBigBed)
    { // avoid opening an unneeded db connection for bigBed; required not to use mysql for parallel fetch tracks
    bigBedAddLinkedFeaturesFrom(tg, chromName, winStart, winEnd,
          scoreMin, scoreMax, useItemRgb, 8, &lfList);
    }
else
    {
    struct sqlConnection *conn = hAllocConn(database);
    struct sqlResult *sr;
    char **row;
    int rowOffset;
    char *scoreFilterClause = getScoreFilterClause(cart, tg->tdb,NULL);
    if (scoreFilterClause != NULL)
	{
	sr = hRangeQuery(conn, tg->table, chromName, winStart, winEnd,scoreFilterClause, &rowOffset);
	freeMem(scoreFilterClause);
	}
    else
	{
	sr = hRangeQuery(conn, tg->table, chromName, winStart, winEnd,
			 NULL, &rowOffset);
	}

    while ((row = sqlNextRow(sr)) != NULL)
	{
	bed = bedLoadN(row+rowOffset, 8);
	lf = bedMungToLinkedFeatures(&bed, tdb, 8, scoreMin, scoreMax, useItemRgb);
	slAddHead(&lfList, lf);
	}
    sqlFreeResult(&sr);
    hFreeConn(&conn);
    }
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
    { // avoid opening an unneeded db connection for bigBed; required not to use mysql for parallel fetch tracks
    bigBedAddLinkedFeaturesFrom(tg, chromName, winStart, winEnd,
          scoreMin, scoreMax, useItemRgb, 12, &lfList);
    }
else
    {
    /* Use tg->tdb->track because subtracks inherit composite track's tdb
     * by default, and the variable is named after the composite track. */
    struct sqlConnection *conn = hAllocConn(database);
    char *scoreFilterClause = getScoreFilterClause(cart, tg->tdb,NULL);
    if (scoreFilterClause != NULL)
	{
	sr = hRangeQuery(conn, tg->table, chromName, winStart, winEnd,scoreFilterClause, &rowOffset);
	freeMem(scoreFilterClause);
	}
    else
	{
	sr = hRangeQuery(conn, tg->table, chromName, winStart, winEnd, NULL, &rowOffset);
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
    hFreeConn(&conn);
    }
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
int s = max(bed->chromStart, winStart), e = min(bed->chromEnd, winEnd);
if (s > e)
    return;
int x1 = round((s-winStart)*scale) + xOff;
int x2 = round((e-winStart)*scale) + xOff;
int w = x2 - x1;
if (w < 1)
    w = 1;
struct trackDb *tdb = tg->tdb;
int scoreMin = atoi(trackDbSettingClosestToHomeOrDefault(tdb, "scoreMin", "0"));
int scoreMax = atoi(trackDbSettingClosestToHomeOrDefault(tdb, "scoreMax", "1000"));
char *directUrl = trackDbSetting(tdb, "directUrl");
boolean withHgsid = (trackDbSetting(tdb, "hgsid") != NULL);
boolean thickDrawItem = (trackDbSetting(tdb, "thickDrawItem") != NULL);

if (tg->itemColor != NULL)
    {
    color = tg->itemColor(tg, bed, hvg);
    }
else if (tg->colorShades)
    {
    adjustBedScoreGrayLevel(tdb, bed, scoreMin, scoreMax);
    color = tg->colorShades[grayInRange(bed->score, scoreMin, scoreMax)];
    }
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
                          tg->track, tg->mapItemName(tg, bed), NULL, directUrl, withHgsid, NULL);
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
    errAbort("missing drawItemAt in track %s", tg->track);

if (vis == tvDense && canDrawBigBedDense(tg))
    {
    bigBedDrawDense(tg, seqStart, seqEnd, hvg, xOff, yOff, width, font, color);
    }
else
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

void simpleBedNextPrevEdge(struct track *tg, struct hvGfx *hvg, void *item, int x, int y, int w,
			   int h, boolean next)
/* Like linkedFeaturesNextPrevItem, but for simple bed which has no block structure so
 * this simply zaps us to the right/left edge of the feature.  Arrows have already been
 * drawn; here we figure out coords and draw a mapBox. */
{
struct bed4 *bed = item;
char *mouseOverText = NULL;
if (next)
    mouseOverText = trackDbSettingClosestToHomeOrDefault(tg->tdb, "nextExonText",
							 revCmplDisp ? "Left Edge" : "Right Edge");
else
    mouseOverText = trackDbSettingClosestToHomeOrDefault(tg->tdb, "prevExonText",
							 revCmplDisp ? "Right Edge" : "Left Edge");
int winSize = winEnd - winStart;
int bufferToEdge = 0.05 * winSize;
int newWinStart, newWinEnd;
if (next)
    {
    newWinEnd = bed->chromEnd + bufferToEdge;
    newWinStart = newWinEnd - winSize;
    }
else
    {
    newWinStart = bed->chromStart - bufferToEdge;
    newWinEnd = newWinStart + winSize;
    }
mapBoxJumpTo(hvg, x, y, w, h, tg, chromName, newWinStart, newWinEnd, mouseOverText);
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
// Adding "tg->nextPrevExon = simpleBedNextPrevEdge;" opened a can of worms: too many
// bed-based tracks have their own drawItems methods that don't hook into nextItem stuff,
// or drawItemAt methods that draw mapboxes but tg->mapsSelf is not set...
// So, set tg->nextPrevExon = simpleBedNextPrevEdge case-by-case.
tg->nextPrevItem = linkedFeaturesLabelNextPrevItem;
tg->freeItems = freeSimpleBed;
}

void addSimpleFeatures(struct simpleFeature **pSfList, int start, int end, int qStart, boolean  everyBase)
/* Add simple features from start to end to the list. 
 * qStart is the offset in the query.
 * If everyBase is TRUE then add a simpleFeature for every base in the start,end region */
{
int s;
struct simpleFeature *sf;
int stepSize = everyBase ? 1 : end - start;
for (s = start ; s < end ; s += stepSize)
    {
    AllocVar(sf);
    sf->start = s;
    sf->end = sf->start + stepSize;
    sf->qStart = qStart + (s - start);
    sf->qEnd = sf->qStart + stepSize;
    slAddHead(pSfList, sf);
    }
}

struct linkedFeatures *simpleBedToLinkedFeatures(struct bed *b, int bedFields, 
    boolean everyBase, boolean paired)
/* Create a linked feature from a single bed item
 * Any bed fields past the 6th field (strand) will be ignored
 * Make one simpleFeature for every base of the bed if everyBase is TRUE,
 * otherwise it will contain a single 'exon' corresponding to the bed (start,end)
 * Dont free the bed as a pointer to each item is stored in lf->original 
 * If paired then need to strip ~seq1~seq2 from name and set it as DNA in lf->extra 
 *  and treat bed as 2-exon bed.
 */
{
struct linkedFeatures *lf = NULL;
if (b)
    {
    AllocVar(lf);
    lf->start = lf->tallStart = b->chromStart;
    lf->end = lf->tallEnd = b->chromEnd;
    lf->components = NULL;
    if (bedFields > 5) // need to know orientation before checking if paired
	lf->orientation = (b->strand[0] == '+' ? 1 : (b->strand[0] == '-' ? -1 : 0)); 
    if (paired)
	{
	// Find seq1 & seq2, strip them from the name,
	// add them as two blocks of simpleFeatures,
	// concatenate seq1 & seq2 into one dnaSeq,
	// and store in lf->extra field
	char *seq2 = strrchr(b->name, SEQ_DELIM);
	if (!seq2)
	    errAbort("Could not find seq2 in paired sequence");
	*(seq2++) = '\0';
	// Find seq1 and strip it from the name
	char *seq1 = strrchr(b->name, SEQ_DELIM);
	if (!seq1)
	    errAbort("Could not find seq1 in paired sequence");
	*(seq1++) = '\0';
	int l1 = strlen(seq1);
	int l2 = strlen(seq2);
	struct dyString *d = dyStringNew(l1+l2+1);
	dyStringAppend(d, seq1);
	dyStringAppend(d, seq2);
	lf->extra = newDnaSeq(dyStringCannibalize(&d), l1+l2, lf->name);
	// seq1 + seq2 are concatenated for drawing code
	// in - orientation they are also reverse complemented, 
	// and the query start is relative to the reverse complement, so:
	// in + orientation first feature (f1) is seq1, second (f2) is seq2
	// in - orientation first feature (f1) is seq2, second (f2) is seq1
	int f1 = lf->orientation == -1 ? l2 : l1;
	int f2 = lf->orientation == -1 ? l1 : l2;
	addSimpleFeatures(&lf->components, lf->start, lf->start + f1, 0, everyBase);
	addSimpleFeatures(&lf->components, lf->end - f2, lf->end, f1, everyBase);
	}
    else
	{
	addSimpleFeatures(&lf->components, lf->start, lf->end, 0, everyBase);
	}
    slReverse(&lf->components);
    if (bedFields > 3)
	lf->name = cloneString(b->name);
    if (bedFields > 4)
	lf->score = b->score;
    lf->original = b;
    }
return lf;
}

struct linkedFeatures *simpleBedListToLinkedFeatures(struct bed *b, int bedFields, 
    boolean everyBase, boolean paired)
/* Create a list of linked features from a list of beds */
{
struct linkedFeatures *lfList = NULL;
while (b)
    {
    slAddHead(&lfList, simpleBedToLinkedFeatures(b, bedFields, everyBase, paired));
    b = b->next;
    }
slReverse(&lfList);
return lfList;
}

void loadSimpleBedAsLinkedFeaturesPerBase(struct track *tg)
/* bed list not freed as pointer to it is stored in 'original' field */
{
loadSimpleBed(tg);
tg->items = simpleBedListToLinkedFeatures(tg->items, tg->bedSize, TRUE, FALSE);
}

void loadPairedTagAlignAsLinkedFeaturesPerBase(struct track *tg)
/* bed list not freed as pointer to it is stored in 'original' field */
{
loadSimpleBed(tg);
tg->items = simpleBedListToLinkedFeatures(tg->items, tg->bedSize, TRUE, TRUE);
}


static Color itemColorByStrand(struct track *tg, int orientation, struct hvGfx *hvg)
/* Look up the RGB color from the trackDb setting 'colorByStrand' based on
 * the orientation (1='+', 0=unknown, -1='-'), and convert this to a color index
 * using hvGfx */
{
char *words[3];
unsigned char r, g, b;

char *colors = cloneString(trackDbSetting(tg->tdb, "colorByStrand"));
if (!colors)
    errAbort("colorByStrand setting missing (in %s)", tg->track);
if (chopByWhite(colors, words, sizeof(words)) != 2)
    errAbort("invalid colorByStrand setting %s (expecting pair of RGB values r,g,b r,g,b)", colors);
if (orientation == 1)
    parseColor(words[0], &r, &g, &b);
else if (orientation == -1) 
    parseColor(words[1], &r, &g, &b);
else // return the main color
    {
    r = tg->color.r; g = tg->color.g; b = tg->color.b;
    }
freez(&colors);
return hvGfxFindColorIx(hvg, r, g, b);
}

Color lfItemColorByStrand(struct track *tg, void *item, struct hvGfx *hvg)
/* Look up the RGB color from the trackDb setting 'colorByStrand' based on
 * the linkedFeature item orientation, and convert this to a color index
 * using hvGfx */
{
struct linkedFeatures *lf = item;
return itemColorByStrand(tg, lf->orientation, hvg);
}

Color bedItemColorByStrand(struct track *tg, void *item, struct hvGfx *hvg)
/* Look up the RGB color from the trackDb setting 'colorByStrand' based on
 * the bed item strand, and convert this to a color index
 * using hvGfx */
{
struct bed *b = item;
return itemColorByStrand(tg, (b->strand[0] == '+' ? 1 : (b->strand[0] == '-' ? -1 : 0)), hvg);
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

//if (track->isBigBed)
    //track->nextItemButtonable = FALSE;

if (fieldCount < 8)
    {
    if (baseColorGetDrawOpt(track) != baseColorDrawOff)
	{
	// data must be loaded as bed and converted to linkedFeatures 
	// to draw each base character must make one simpleFeature per base
	linkedFeaturesMethods(track);
	char *setting = trackDbSetting(tdb, BASE_COLOR_USE_SEQUENCE);
	if (isNotEmpty(setting) && sameString(setting, "seq1Seq2"))
	    track->loadItems = loadPairedTagAlignAsLinkedFeaturesPerBase;
	else
	    track->loadItems = loadSimpleBedAsLinkedFeaturesPerBase;
	if (trackDbSetting(tdb, "colorByStrand"))
	    {
	    track->itemColor = lfItemColorByStrand;
	    }
	}
    else
	{
	bedMethods(track);
	track->loadItems = loadSimpleBed;
	if (trackDbSetting(tdb, "colorByStrand"))
	    {
	    track->itemColor = bedItemColorByStrand;
	    }
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
    if (trackDbSetting(tdb, "colorByStrand"))
	{
	track->itemColor = lfItemColorByStrand;
	}
    }
else
    {
    linkedFeaturesMethods(track);
    track->extraUiData = newBedUiData(track->track);
    track->loadItems = loadGappedBed;
    if (trackDbSetting(tdb, "colorByStrand"))
	{
	track->itemColor = lfItemColorByStrand;
	}
    }
}
