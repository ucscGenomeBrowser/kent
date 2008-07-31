/* wabaTrack - track for WABA alignments.  WABA is a pairwise 
 * HMM that does pairwise alignments and classifies bases. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"

struct wabaChromHit
/* Records where waba alignment hits chromosome. */
    {
    struct wabaChromHit *next;	/* Next in list. */
    char *query;	  	/* Query name. */
    int chromStart, chromEnd;   /* Chromosome position. */
    char strand;                /* + or - for strand. */
    int milliScore;             /* Parts per thousand */
    char *squeezedSym;          /* HMM Symbols */
    };

static struct wabaChromHit *wchLoad(char *row[])
/* Create a wabaChromHit from database row. 
 * Since squeezedSym autoSql can't generate this,
 * alas. */
{
int size;
char *sym;
struct wabaChromHit *wch;

AllocVar(wch);
wch->query = cloneString(row[0]);

wch->chromStart = sqlUnsigned(row[1]);
wch->chromEnd = sqlUnsigned(row[2]);
wch->strand = row[3][0];
wch->milliScore = sqlUnsigned(row[4]);
size = wch->chromEnd - wch->chromStart;
wch->squeezedSym = sym = needLargeMem(size+1);
memcpy(sym, row[5], size);
sym[size] = 0;
return wch;
}

static void wchFree(struct wabaChromHit **pWch)
/* Free a singlc wabaChromHit. */
{
struct wabaChromHit *wch = *pWch;
if (wch != NULL)
    {
    freeMem(wch->squeezedSym);
    freeMem(wch->query);
    freez(pWch);
    }
}

static void wchFreeList(struct wabaChromHit **pList)
/* Free list of wabaChromHits. */
{
struct wabaChromHit *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    wchFree(&el);
    }
*pList = NULL;
}

static void wabaLoad(struct track *tg)
/* Load up waba items intersecting window. */
{
char table[64];
char query[256];
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
char **row;
struct wabaChromHit *wch, *wchList = NULL;

/* Get the frags and load into tg->items. */
sprintf(table, "%s%s", chromName, (char *)tg->customPt);
sprintf(query, "select * from %s where chromStart<%u and chromEnd>%u",
    table, winEnd, winStart);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    wch = wchLoad(row);
    slAddHead(&wchList, wch);
    }
slReverse(&wchList);
tg->items = wchList;
sqlFreeResult(&sr);
hFreeConn(&conn);
}

static void wabaFree(struct track *tg)
/* Free up wabaTrackGroup items. */
{
wchFreeList((struct wabaChromHit**)&tg->items);
}


static void makeSymColors(struct track *tg, enum trackVisibility vis,
	Color symColor[128])
/* Fill in array with color for each symbol value. */
{
memset(symColor, MG_WHITE, 128);
symColor['1'] = symColor['2'] = symColor['3'] = symColor['H'] 
   = tg->ixColor;
symColor['L'] = tg->ixAltColor;
}

static int countSameColor(char *sym, int symCount, Color symColor[])
/* Count how many symbols are the same color as the current one. */
{
Color color = symColor[(int)sym[0]];
int ix;
for (ix = 1; ix < symCount; ++ix)
    {
    if (symColor[(int)sym[ix]] != color)
        break;
    }
return ix;
}


static void wabaDraw(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw waba alignment items. */
{
Color symColor[128];
int baseWidth = seqEnd - seqStart;
struct wabaChromHit *wch;
int y = yOff;
int heightPer = tg->heightPer;
int lineHeight = tg->lineHeight;
int x1,x2,w;
boolean isFull = (vis == tvFull);

makeSymColors(tg, vis, symColor);
for (wch = tg->items; wch != NULL; wch = wch->next)
    {
    int chromStart = wch->chromStart;
    int symCount = wch->chromEnd - chromStart;
    int typeStart, typeWidth, typeEnd;
    char *sym = wch->squeezedSym;
    for (typeStart = 0; typeStart < symCount; typeStart = typeEnd)
	{
	typeWidth = countSameColor(sym+typeStart, symCount-typeStart, symColor);
	typeEnd = typeStart + typeWidth;
	color = symColor[(int)sym[typeStart]];
	if (color != MG_WHITE)
	    {
	    x1 = roundingScale(typeStart+chromStart-winStart, width, baseWidth)+xOff;
	    x2 = roundingScale(typeEnd+chromStart-winStart, width, baseWidth)+xOff;
	    w = x2-x1;
	    if (w < 1)
		w = 1;
	    hvGfxBox(hvg, x1, y, w, heightPer, color);
	    }
	}
    if (isFull)
	y += lineHeight;
    }
}

static char *wabaName(struct track *tg, void *item)
/* Return name of waba track item. */
{
struct wabaChromHit *wch = item;
return wch->query;
}

static int wabaItemStart(struct track *tg, void *item)
/* Return starting position of waba item. */
{
struct wabaChromHit *wch = item;
return wch->chromStart;
}

static int wabaItemEnd(struct track *tg, void *item)
/* Return ending position of waba item. */
{
struct wabaChromHit *wch = item;
return wch->chromEnd;
}

void wabaMethods(struct track *tg)
/* Return track with fields shared by waba-based 
 * alignment tracks filled in. */
{
tg->loadItems = wabaLoad;
tg->freeItems = wabaFree;
tg->drawItems = wabaDraw;
tg->itemName = wabaName;
tg->mapItemName = wabaName;
tg->totalHeight = tgFixedTotalHeightNoOverflow;
tg->itemHeight = tgFixedItemHeight;
tg->itemStart = wabaItemStart;
tg->itemEnd = wabaItemEnd;
}

