/* rmskTrack - Handle RepeatMasker track. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"
#include "rmskOut.h"
#include "quickLift.h"


/* Repeat items.  Since there are so many of these, to avoid 
 * memory problems we don't query the database and store the results
 * during repeatLoad, but rather query the database during the
 * actual drawing. */

static struct repeatItem *otherRepeatItem = NULL;
static char *repeatClassNames[] =  {
    "SINE", "LINE", "LTR", "DNA", "Simple", "Low Complexity", "Satellite", "RNA", "Other", "Unknown", 
};
static char *repeatClasses[] = {
    "SINE", "LINE", "LTR", "DNA", "Simple_repeat", "Low_complexity", "Satellite", "RNA", "Other", "Unknown", 
};

static struct repeatItem *riList = NULL;  // can be re-used by all windows

static struct repeatItem *makeRepeatItems()
/* Make the stereotypical repeat masker tracks. */
{
if (!riList)
    {
    struct repeatItem *ri;
    int i;
    int numClasses = ArraySize(repeatClasses);
    for (i=0; i<numClasses; ++i)
	{
	AllocVar(ri);
	ri->class = repeatClasses[i];
	ri->className = repeatClassNames[i];
	slAddHead(&riList, ri);
	if (sameString(repeatClassNames[i], "Other"))
	    otherRepeatItem = ri;               
	}
    slReverse(&riList);
    }
return riList;
}

static void repeatLoad(struct track *tg)
/* Load up repeat tracks.  (Will query database during drawing for a change.) */
{
tg->items = makeRepeatItems();
}

static void repeatFree(struct track *tg)
/* Free up repeatMasker items. */
{
slFreeList(&tg->items);
}

static char *repeatName(struct track *tg, void *item)
/* Return name of repeat item track. */
{
struct repeatItem *ri = item;
return ri->className;
}

static struct rmskOut *liftedRepeatList(struct track *tg)
/* The rows of a quickLifted rmsk track, read from the assembly they came from and lifted
 * onto the reference, that land in the window.  They are read over padded source ranges, so
 * some land outside it.  refs #37437 */
{
struct hash *chainHash = newHash(8);
struct rmskOut *ro, *next, *liftedList = NULL;
for (ro = quickLiftSqlLoadRmsk(tg->tdb, tg->table, chromName, winStart, winEnd, chainHash);
     ro != NULL; ro = next)
    {
    next = ro->next;
    ro->next = NULL;
    if (quickLiftRmskOut(quickLiftChainHashForItem(chainHash, ro), ro) &&
        sameString(ro->genoName, chromName) && (ro->genoStart < winEnd) &&
        (ro->genoEnd > winStart))
        slAddHead(&liftedList, ro);
    else
        rmskOutFree(&ro);
    }
return liftedList;
}

static void liftedRepeatDraw(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width,
        MgFont *font, Color color, enum trackVisibility vis)
/* The drawing of a quickLifted rmsk track, behind browser.quickLiftRmsk:  repeatDraw() over
 * rows read from the assembly they came from and lifted onto this one, kept in a list rather
 * than streamed from the table.  The two drawing loops are copies of the ones in
 * repeatDraw();  a fix to one belongs in the other.  refs #37437 */
{
Color overrideColor = colorFromCart(tg, 0);
int baseWidth = seqEnd - seqStart;
struct repeatItem *ri;
int y = yOff;
int heightPer = tg->heightPer;
int lineHeight = tg->lineHeight;
int x1,x2,w;
boolean isFull = (vis == tvFull);
Color col;
struct rmskOut *ro, *liftedList = liftedRepeatList(tg);

if (isFull)
    {
    /* Do gray scale representation spread out among tracks. */
    struct hash *hash = newHash(6);
    int percId;
    int grayLevel;
    char statusLine[128];

    for (ri = tg->items; ri != NULL; ri = ri->next)
        {
	ri->yOffset = y;
	y += lineHeight;
	hashAdd(hash, ri->class, ri);
	}
    for (ro = liftedList; ro != NULL; ro = ro->next)
        {
	char class[256];
	// Simplify repClass for lookup: strip trailing '?', simplify *RNA to RNA:
	safecpy(class, sizeof(class), ro->repClass);
	char *p = &(class[strlen(class)-1]);
	if (*p == '?')
	    *p = '\0';
	if (endsWith(class, "RNA"))
	    safecpy(class, sizeof(class), "RNA");
	ri = hashFindVal(hash, class);
	if (ri == NULL)
	   ri = otherRepeatItem;
	percId = 1000 - ro->milliDiv - ro->milliDel - ro->milliIns;
	grayLevel = grayInRange(percId, 500, 1000);
	col = overrideColor ? overrideColor : shadesOfGray[grayLevel];
	x1 = roundingScale(ro->genoStart-winStart, width, baseWidth)+xOff;
	x1 = max(x1, 0);
	x2 = roundingScale(ro->genoEnd-winStart, width, baseWidth)+xOff;
	if (x1 < insideX)
	    x1 = insideX;
	if (x2 > insideX+width)
	    x2 = insideX+width;
	w = x2-x1;
	if (w <= 0)
	    w = 1;
	hvGfxBox(hvg, x1, ri->yOffset, w, heightPer, col);
	if (baseWidth <= 100000)
	    {
	    if (ri == otherRepeatItem)
		{
		safef(statusLine, sizeof statusLine, "Repeat %s, family %s, class %s",
		    ro->repName, ro->repFamily, ro->repClass);
		}
	    else
		{
		safef(statusLine, sizeof statusLine, "Repeat %s, family %s",
		    ro->repName, ro->repFamily);
		}
	    mapBoxHc(hvg, ro->genoStart, ro->genoEnd, x1, ri->yOffset, w, heightPer, tg->track,
	    	ro->repName, statusLine);
	    }
	}
    freeHash(&hash);
    }
else
    {
    /* Do black and white on single track. */
    for (ro = liftedList; ro != NULL; ro = ro->next)
	{
	x1 = roundingScale(ro->genoStart-winStart, width, baseWidth)+xOff;
	x2 = roundingScale(ro->genoEnd-winStart, width, baseWidth)+xOff;
	w = x2-x1;
	if (w <= 0)
	    w = 1;
	hvGfxBox(hvg, x1, yOff, w, heightPer, overrideColor ? overrideColor : MG_BLACK);
	}
    }
rmskOutFreeList(&liftedList);
}

static void repeatDraw(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width,
        MgFont *font, Color color, enum trackVisibility vis)
{
if (quickLiftIsLifted(tg->tdb) && quickLiftRmskEnabled(cart))
    {
    // a quickLifted rmsk reads its rows from the assembly it came from, refs #37437
    liftedRepeatDraw(tg, seqStart, seqEnd, hvg, xOff, yOff, width, font, color, vis);
    return;
    }
Color overrideColor = colorFromCart(tg, 0);
int baseWidth = seqEnd - seqStart;
struct repeatItem *ri;
int y = yOff;
int heightPer = tg->heightPer;
int lineHeight = tg->lineHeight;
int x1,x2,w;
boolean isFull = (vis == tvFull);
Color col;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
char **row;
int rowOffset;

if (isFull)
    {
    /* Do gray scale representation spread out among tracks. */
    struct hash *hash = newHash(6);
    struct rmskOut ro;
    int percId;
    int grayLevel;
    char statusLine[128];

    for (ri = tg->items; ri != NULL; ri = ri->next)
        {
	ri->yOffset = y;
	y += lineHeight;
	hashAdd(hash, ri->class, ri);
	}
    // liftedRepeatDraw() has a copy of this loop;  a fix to one belongs in the other
    sr = hRangeQuery(conn, tg->table, chromName, winStart, winEnd, NULL,
		     &rowOffset);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	rmskOutStaticLoad(row+rowOffset, &ro);
	char class[256];
	// Simplify repClass for lookup: strip trailing '?', simplify *RNA to RNA:
	safecpy(class, sizeof(class), ro.repClass);
	char *p = &(class[strlen(class)-1]);
	if (*p == '?')
	    *p = '\0';
	if (endsWith(class, "RNA"))
	    safecpy(class, sizeof(class), "RNA");
	ri = hashFindVal(hash, class);
	if (ri == NULL)
	   ri = otherRepeatItem;
	percId = 1000 - ro.milliDiv - ro.milliDel - ro.milliIns;
	grayLevel = grayInRange(percId, 500, 1000);
	col = overrideColor ? overrideColor : shadesOfGray[grayLevel];
	x1 = roundingScale(ro.genoStart-winStart, width, baseWidth)+xOff;
	x1 = max(x1, 0);
	x2 = roundingScale(ro.genoEnd-winStart, width, baseWidth)+xOff;
	if (x1 < insideX)
	    x1 = insideX;
	if (x2 > insideX+width)
	    x2 = insideX+width;
	w = x2-x1;
	if (w <= 0)
	    w = 1;
	hvGfxBox(hvg, x1, ri->yOffset, w, heightPer, col);
	if (baseWidth <= 100000)
	    {
	    if (ri == otherRepeatItem)
		{
		sprintf(statusLine, "Repeat %s, family %s, class %s",
		    ro.repName, ro.repFamily, ro.repClass);
		}
	    else
		{
		sprintf(statusLine, "Repeat %s, family %s",
		    ro.repName, ro.repFamily);
		}
	    mapBoxHc(hvg, ro.genoStart, ro.genoEnd, x1, ri->yOffset, w, heightPer, tg->track,
	    	ro.repName, statusLine);
	    }
	}
    freeHash(&hash);
    }
else
    {
    char table[HDB_MAX_TABLE_STRING];
    boolean hasBin;
    struct dyString *query = dyStringNew(1024);
    /* Do black and white on single track.  Fetch less than we need from database. */
    if (hFindSplitTable(database, chromName, tg->table, table, sizeof table, &hasBin))
        {
	sqlDyStringPrintf(query, "select genoStart,genoEnd from %s where ", table);
	if (hasBin)
	    hAddBinToQuery(winStart, winEnd, query);
	sqlDyStringPrintf(query, "genoStart<%u and genoEnd>%u ", winEnd, winStart);
	/* if we're using a single rmsk table, add genoName to the where clause */
	if (startsWith("rmsk", table))
	    sqlDyStringPrintf(query, " and genoName = '%s' ", chromName);
	sr = sqlGetResult(conn, query->string);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    int start = sqlUnsigned(row[0]);
	    int end = sqlUnsigned(row[1]);
	    x1 = roundingScale(start-winStart, width, baseWidth)+xOff;
	    x2 = roundingScale(end-winStart, width, baseWidth)+xOff;
	    w = x2-x1;
	    if (w <= 0)
		w = 1;
	    hvGfxBox(hvg, x1, yOff, w, heightPer, overrideColor ? overrideColor : MG_BLACK);
	    }
	}
    dyStringFree(&query);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void repeatMethods(struct track *tg)
/* Make track for repeats. */
{
tg->loadItems = repeatLoad;
tg->freeItems = repeatFree;
tg->drawItems = repeatDraw;
tg->colorShades = shadesOfGray;
tg->itemName = repeatName;
tg->mapItemName = repeatName;
tg->totalHeight = tgFixedTotalHeightNoOverflow;
tg->itemHeight = tgFixedItemHeight;
tg->itemStart = tgItemNoStart;
tg->itemEnd = tgItemNoEnd;
tg->mapsSelf = TRUE;
}

