/* wigTrack - stuff to handle loading and display of
 * wig type tracks in browser. Wigs are arbitrary data graphs
 */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"
#include "wiggle.h"
#include "scoredRef.h"

static char const rcsid[] = "$Id: wigTrack.c,v 1.1 2003/09/22 18:03:24 hiram Exp $";

struct wigItem
/* A wig track item. */
    {
    struct wigItem *next;
    char *name;		/* Common name */
    char *db;		/* Database */
    int ix;		/* Position in list. */
    int height;		/* Pixel height of item. */
    };

static void wigItemFree(struct wigItem **pEl)
    /* Free up a wigItem. */
{
struct wigItem *el = *pEl;
if (el != NULL)
    {
    freeMem(el->name);
    freeMem(el->db);
    freez(pEl);
    }
}

void wigItemFreeList(struct wigItem **pList)
    /* Free a list of dynamically allocated wigItem's */
{
struct wigItem *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    wigItemFree(&el);
    }
*pList = NULL;
}


static char tmpName[] = "Wiggle Track";

static char dbgFile[] = "/tmp/wig.dbg";
static boolean debugOpened = FALSE;
static FILE * dF;

static void debugOpen(char * name) {
if( debugOpened ) return;
dF = fopen( dbgFile, "w" );
fprintf( dF, "opened by %s\n", name );
chmod(dbgFile, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP  | S_IROTH | S_IWOTH | S_IXOTH );
debugOpened = TRUE;
}

#define DBGMSGSZ	1023
char dbgMsg[DBGMSGSZ+1];
static void debugPrint(char * name) {
debugOpen(name);
if( debugOpened ) {
    if( dbgMsg[0] )
    fprintf( dF, "%s: %s\n", name, dbgMsg);
    else
    fprintf( dF, "%s:\n", name);
    }
    dbgMsg[0] = (char) NULL;
}

static char *wigName(struct track *tg, void *item)
/* Return name of wig level track. */
{
struct linkedFeatures *lf = item;
return lf->name;
/*
struct wigItem *mi = item;
return tmpName;
return mi->name;
*/
}


struct linkedFeatures *lfFromWiggle(struct wiggle *wiggle)
    /* Return a linked feature from a full wiggle track. */
{
struct linkedFeatures *lf;
struct simpleFeature *sf, *sfList = NULL;
int grayIx = grayInRange(wiggle->score, 0, 127);
unsigned Span = wiggle->Span;	/* bases spanned per data value */
unsigned Count = wiggle->Count;	/* number of values in this row */
unsigned Offset = wiggle->Offset;	/* start of data in the data file */
char *File = (char *) NULL;
int i;
size_t FileNameSize = 0;

assert(Span > 0 && Count > 0 && wiggle->File != NULL);

AllocVar(lf);
lf->grayIx = grayIx;
strncpy(lf->name, wiggle->name, sizeof(lf->name));
lf->orientation = orientFromChar(wiggle->strand[0]);

FileNameSize = strlen("/gbdb//") + strlen(database) + strlen(wiggle->File) + 1;
File = (char *) needMem((size_t)FileNameSize);
snprintf(File, FileNameSize, "/gbdb/%s/%s", database, wiggle->File);

if( ! fileExists(File) )
    errAbort("wiggle load: file '%s' missing", File );

AllocVar(sf);
sf->start = wiggle->chromStart;
sf->end = sf->start + Span * Count;
sf->grayIx = grayIx;
slAddHead(&sfList, sf);
/*slReverse(&sfList);*/
lf->components = sfList;
linkedFeaturesBoundsAndGrays(lf);
lf->start = wiggle->chromStart;
lf->end = wiggle->chromEnd;

snprintf(dbgMsg, DBGMSGSZ, "start: %u, end: %u", sf->start, sf->end );
debugPrint("lfFromWiggle");

freeMem(File);
return lf;
}

static int wigTotalHeight(struct track *tg, enum trackVisibility vis)
/* Wiggle track will use this to figure out the height they use
   as defined in the cart */
{
struct wigItem *mi;
int total = 0;
int itemCount = 0;

tg->heightPer = tl.fontHeight;
tg->lineHeight = tl.fontHeight + 1;
tg->height = tg->lineHeight;

if( tg->visibility == tvFull )
    {
    total = 0;
    for (mi = tg->items; mi != NULL; mi = mi->next)
	{
	++itemCount;
	total += tg->lineHeight;
	}
    tg->height = total;
    }
if( tg->visibility == tvDense )
    tg->height = tg->lineHeight;

snprintf(dbgMsg, DBGMSGSZ, "fontHeight: %d, vis: %d, returning: %d, itemCount: %d", tl.fontHeight, tg->visibility, tg->height, itemCount );
debugPrint("wigTotalHeight");

return tg->height;
}

static void wigLoadItems(struct track *tg) {
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int rowOffset;
struct wiggle *wiggle;
struct linkedFeatures *lfList = NULL;
struct linkedFeatures *lf;
char *where = NULL;
int rowsLoaded = 0;

sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, where, &rowOffset);

while ((row = sqlNextRow(sr)) != NULL)
    {
	++rowsLoaded;
	wiggle = wiggleLoad(row + rowOffset);
	lf = lfFromWiggle(wiggle);
	slAddHead(&lfList, lf);
	wiggleFree(&wiggle);
    }
if(where != NULL)
        freez(&where);
sqlFreeResult(&sr);
hFreeConn(&conn);

snprintf(dbgMsg, DBGMSGSZ, "viz: %d, mapName: %s, chromName: %s, db: %s", tg->visibility, tg->mapName, chromName, database );
debugPrint("wigLoadItems");
snprintf(dbgMsg, DBGMSGSZ, "winStart: %d, winEnd: %d", winStart, winEnd );
debugPrint("wigLoadItems");
snprintf(dbgMsg, DBGMSGSZ, "rowsLoaded: %d", rowsLoaded );
debugPrint("wigLoadItems");

slReverse(&lfList);
tg->items = lfList;

}

static void wigFreeItems(struct track *tg) {
debugPrint("wigFreeItems");
}

static void wigDrawItems(struct track *tg, int seqStart, int seqEnd,
	struct vGfx *vg, int xOff, int yOff, int width,
	MgFont *font, Color color, enum trackVisibility vis)
{
struct linkedFeatures *lf;
double scale = scaleForPixels(width);
int x1,y1,w,h,x2,y2;
struct rgbColor *normal = &(tg->color);
int black;
int itemCount = 0;


for (lf = tg->items; lf != NULL; lf = lf->next)
    ++itemCount;

black = vgFindColorIx(vg, 0, 0, 0);

/*	width - width of drawing window in pixels
 *	scale - pixels per base
 */
snprintf(dbgMsg, DBGMSGSZ, "width: %d, height: %d, heightPer: %d, scale: %.4f", width, tg->lineHeight, tg->heightPer, scale );
debugPrint("wigDrawItems");
snprintf(dbgMsg, DBGMSGSZ, "seqStart: %d, seqEnd: %d, xOff: %d, yOff: %d, black: %d", seqStart, seqEnd, xOff, yOff, black );
debugPrint("wigDrawItems");
snprintf(dbgMsg, DBGMSGSZ, "itemCount: %d", itemCount );
debugPrint("wigDrawItems");

y1 = yOff;
h = tg->lineHeight;
itemCount = 0;
for (lf = tg->items; lf != NULL; lf = lf->next)
    {
	struct simpleFeature *sf = lf->components;
	++itemCount;
	w = (sf->end - sf->start) * scale;
	x1 = xOff + (sf->start - seqStart)*scale;
        vgBox(vg, x1, y1, w, h, black);
	y1 += tg->lineHeight;

snprintf(dbgMsg, DBGMSGSZ, "itemCount: %d, start: %d, end: %d", itemCount, sf->start, sf->end );
debugPrint("wigDrawItems");
    }
}

/* Make track group for wig multiple alignment. */
void wigMethods(struct track *track, struct trackDb *tdb, 
	int wordCount, char *words[])
{
char o4[128];	/*	Option 4 - minimum Y axis value	*/
char o5[128];	/*	Option 5 - maximum Y axis value	*/
char *minY_str;	/*	string from cart	*/
char *maxY_str;	/*	string from cart	*/
double minYc;	/*	from cart */
double maxYc;	/*	from cart */
double minY;	/*	from trackDb.ra words, the absolute minimum */
double maxY;	/*	from trackDb.ra words, the absolute maximum */
char cartStr[64];	/*	to set cart strings	*/


#define DEFAULT_MIN_Yv	0.0
#define DEFAULT_MAX_Yv	127.0

minY = DEFAULT_MIN_Yv;
maxY = DEFAULT_MAX_Yv;

/*	Possibly fetch values from the trackDb.ra file	*/
if( wordCount > 1 )
	minY = atof(words[1]);
if( wordCount > 2 )
	maxY = atof(words[2]);

/*	Possibly fetch values from the cart	*/
snprintf( o4, sizeof(o4), "%s.minY", track->mapName);
snprintf( o5, sizeof(o4), "%s.maxY", track->mapName);
minY_str = cartOptionalString(cart, o4);
maxY_str = cartOptionalString(cart, o5);
if( minY_str )
    minYc = atof(minY_str);
else
    minYc = minY;
if( maxY_str )
    maxYc = atof(maxY_str);
else
    maxYc = maxY;

/*	The values from trackDb.ra are the clipping boundaries, do
 *	not let cart settings go outside that range
 */
track->minRange = max( minY, minYc );
track->maxRange = min( maxY, maxYc );
snprintf(dbgMsg, DBGMSGSZ, "Y range: %.1f - %.1f", track->minRange, track->maxRange);
debugPrint("wigMethods");

/*	And set those values into the cart	*/
snprintf( cartStr, sizeof(cartStr), "%g", track->minRange );
cartSetString( cart, o4, cartStr );
snprintf( cartStr, sizeof(cartStr), "%g", track->maxRange );
cartSetString( cart, o5, cartStr );

track->loadItems = wigLoadItems;
track->freeItems = wigFreeItems;
track->drawItems = wigDrawItems;
track->itemName = wigName;
track->mapItemName = wigName;
track->totalHeight = wigTotalHeight;
track->itemHeight = tgFixedItemHeight;
track->itemStart = tgItemNoStart;
track->itemEnd = tgItemNoEnd;
track->mapsSelf = TRUE;
}
