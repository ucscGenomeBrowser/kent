/* rmskTrack - Handle RepeatMasker track. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"
#include "rmskOut.h"


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

static struct repeatItem *makeRepeatItems()
/* Make the stereotypical repeat masker tracks. */
{
struct repeatItem *ri, *riList = NULL;
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

static void repeatDraw(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
{
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
    sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, NULL,
		     &rowOffset);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	rmskOutStaticLoad(row+rowOffset, &ro);
	if (endsWith(ro.repClass, "RNA"))
	    ri = hashFindVal(hash, "RNA");
	else
	    ri = hashFindVal(hash, ro.repClass);
	if (ri == NULL)
	   ri = otherRepeatItem;
	percId = 1000 - ro.milliDiv - ro.milliDel - ro.milliIns;
	grayLevel = grayInRange(percId, 500, 1000);
	col = shadesOfGray[grayLevel];
	x1 = roundingScale(ro.genoStart-winStart, width, baseWidth)+xOff;
	x2 = roundingScale(ro.genoEnd-winStart, width, baseWidth)+xOff;
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
	    mapBoxHc(hvg, ro.genoStart, ro.genoEnd, x1, ri->yOffset, w, heightPer, tg->mapName,
	    	ro.repName, statusLine);
	    }
	}
    freeHash(&hash);
    }
else
    {
    char table[64];
    boolean hasBin;
    struct dyString *query = newDyString(1024);
    /* Do black and white on single track.  Fetch less than we need from database. */
    if (hFindSplitTable(database, chromName, tg->mapName, table, &hasBin))
        {
	dyStringPrintf(query, "select genoStart,genoEnd from %s where ", table);
	if (hasBin)
	    hAddBinToQuery(winStart, winEnd, query);
	dyStringPrintf(query, "genoStart<%u and genoEnd>%u ", winEnd, winStart);
	/* if we're using a single rmsk table, add genoName to the where clause */
	if (startsWith("rmsk", table))
	    dyStringPrintf(query, " and genoName = '%s' ", chromName);
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
	    hvGfxBox(hvg, x1, yOff, w, heightPer, MG_BLACK);
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
}

