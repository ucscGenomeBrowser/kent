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

static void repeatDrawOneFull(struct track *tg, struct hvGfx *hvg, struct rmskOut *ro,
        struct hash *hash, Color overrideColor, int xOff, int width, int baseWidth)
/* Draw one repeat in full mode, on the row of its class, in a gray for its divergence. */
{
struct repeatItem *ri;
int heightPer = tg->heightPer;
int x1,x2,w;
Color col;
int percId;
int grayLevel;
char statusLine[128];
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
        sprintf(statusLine, "Repeat %s, family %s, class %s",
            ro->repName, ro->repFamily, ro->repClass);
        }
    else
        {
        sprintf(statusLine, "Repeat %s, family %s",
            ro->repName, ro->repFamily);
        }
    mapBoxHc(hvg, ro->genoStart, ro->genoEnd, x1, ri->yOffset, w, heightPer, tg->track,
        ro->repName, statusLine);
    }
}

static boolean liftedRepeatInWindow(struct rmskOut *ro)
/* A lifted repeat is read over padded source ranges, so it may land outside the window. */
{
return sameString(ro->genoName, chromName) && (ro->genoStart < winEnd) &&
       (ro->genoEnd > winStart);
}

static void repeatDraw(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width,
        MgFont *font, Color color, enum trackVisibility vis)
{
Color overrideColor = colorFromCart(tg, 0);
int baseWidth = seqEnd - seqStart;
struct repeatItem *ri;
int y = yOff;
int heightPer = tg->heightPer;
int lineHeight = tg->lineHeight;
int x1,x2,w;
boolean isFull = (vis == tvFull);
boolean lifted = quickLiftIsLifted(tg->tdb);
struct rmskOut *ro, *liftedList = NULL;

// a lifted track reads its rows from the assembly it came from, mapped onto this one
if (lifted)
    {
    // An rmsk in a quickLift hub written while the gate was on, drawn with it off:  the code
    // below would read the table of the same name in this assembly, so draw nothing.
    if (!quickLiftRmskEnabled(cart))
        return;
    liftedList = quickLiftSqlLoadRmsk(tg->tdb, tg->table, chromName, winStart, winEnd);
    }

struct sqlConnection *conn = lifted ? NULL : hAllocConn(database);
struct sqlResult *sr = NULL;
char **row;
int rowOffset;

if (isFull)
    {
    /* Do gray scale representation spread out among tracks. */
    struct hash *hash = newHash(6);
    struct rmskOut rowRo;

    for (ri = tg->items; ri != NULL; ri = ri->next)
        {
	ri->yOffset = y;
	y += lineHeight;
	hashAdd(hash, ri->class, ri);
	}
    if (lifted)
        {
        for (ro = liftedList; ro != NULL; ro = ro->next)
            if (liftedRepeatInWindow(ro))
                repeatDrawOneFull(tg, hvg, ro, hash, overrideColor, xOff, width, baseWidth);
        }
    else
        {
        sr = hRangeQuery(conn, tg->table, chromName, winStart, winEnd, NULL,
                         &rowOffset);
        while ((row = sqlNextRow(sr)) != NULL)
            {
            rmskOutStaticLoad(row+rowOffset, &rowRo);
            repeatDrawOneFull(tg, hvg, &rowRo, hash, overrideColor, xOff, width, baseWidth);
            }
        }
    freeHash(&hash);
    }
else if (lifted)
    {
    /* Do black and white on single track. */
    for (ro = liftedList; ro != NULL; ro = ro->next)
        {
        if (!liftedRepeatInWindow(ro))
            continue;
        x1 = roundingScale(ro->genoStart-winStart, width, baseWidth)+xOff;
        x2 = roundingScale(ro->genoEnd-winStart, width, baseWidth)+xOff;
        w = x2-x1;
        if (w <= 0)
            w = 1;
        hvGfxBox(hvg, x1, yOff, w, heightPer, overrideColor ? overrideColor : MG_BLACK);
        }
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
rmskOutFreeList(&liftedList);
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

