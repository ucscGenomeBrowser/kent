/* wikiTrack - wikiTrack functions */
#include "common.h"
#include "jksql.h"
#include "hgTracks.h"
#include "hdb.h"
#include "ra.h"
#include "itemAttr.h"
#include "bedCart.h"
#include "wikiLink.h"
#include "wikiTrack.h"
#include "hgConfig.h"

static char const rcsid[] = "$Id: wikiTrack.c,v 1.15 2008/05/22 21:57:51 hiram Exp $";


static void wikiTrackMapItem(struct track *tg, struct hvGfx *hvg, void *item,
	char *itemName, char *mapItemName, int start, int end, int x, int y, int width, int height)
/* create a special map box item with different i=hgcClickName and
 * pop-up statusLine with the item name
 */
{
char *userName;

/* already been determined to be enabled by getting here, need to verify
 *	userName vs editors and vs owner
 */
(void) wikiTrackEnabled(database, &userName);
char *hgcClickName = tg->mapItemName(tg, item);
char *statusLine = tg->itemName(tg, item);
boolean editor = isWikiEditor(userName);
struct wikiTrack *wikiItem = NULL;
boolean enableHgcClick = FALSE;

/* allow hgc click (i.e. delete privs) if the following are true
    1. this is the item 0 "add new item"
    2. logged into the wiki
    3. user is an editor or user is the owner
 */

if (differentWord("0", hgcClickName))
    wikiItem = findWikiItemId(hgcClickName);
else
    enableHgcClick = TRUE;	/* item 0 "add new item" must go to hgc */

if (wikiItem)
    {
    if (isNotEmpty(userName) && sameWord(userName, wikiItem->owner))
	enableHgcClick = TRUE;	/* owner has delete privls */
    if (editor)
	enableHgcClick = TRUE;	/* editors have delete privls */
    }

if (enableHgcClick)
    {
    mapBoxHgcOrHgGene(hvg, start, end, x, y, width, height, tg->mapName, 
                  hgcClickName, statusLine, NULL, FALSE, NULL         );
    }
else
    {	/* go directly to the wiki description */
    char *directUrl = wikiUrl(wikiItem);
    mapBoxHgcOrHgGene(hvg, start, end, x, y, width, height, tg->mapName, 
                  hgcClickName, statusLine, directUrl, FALSE, NULL);
    freeMem(directUrl);
    }
}

static char *wikiTrackMapItemName(struct track *tg, void *item)
/* Return the unique id track item. */
{
struct itemAttr *ia;
char id[64];
int iid = 0;
struct linkedFeatures *lf = item;
if (lf->itemAttr != NULL)
    {
    ia = lf->itemAttr;
    iid = (int)(ia->chromStart);
    }
safef(id,ArraySize(id),"%d", iid);
return cloneString(id);
}

static int hexToDecimal(char *hexString)
/* return decimal value for hex string (from utils/subChar/subChar.c) */
{
int acc = 0;
char c;

tolowers(hexString);
stripChar(hexString, '#');	/* can start with # (as in html) */
while ((c = *hexString++) != 0)
    {
    acc <<= 4;
    if (c >= '0' && c <= '9')
       acc += c - '0';
    else if (c >= 'a' && c <= 'f')
       acc += c + 10 - 'a';
/* ignoring bad characters
    else
       errAbort("Expecting hexadecimal character got %s", hexString);
*/
    }
return acc;
}

static void wikiTrackLoadItems(struct track *tg)
/* Load the items from the wikiTrack table */
{
struct bed *bed;
struct sqlConnection *conn = hConnectCentral();
struct sqlResult *sr;
char **row;
int rowOffset;
char where[256];
struct linkedFeatures *lfList = NULL, *lf;
int scoreMin = 0;
int scoreMax = 99999;

safef(where, ArraySize(where), "db='%s'", database);

sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, where, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct wikiTrack *item = wikiTrackLoad(row);
    AllocVar(bed);
    bed->chrom = cloneString(item->chrom);
    bed->chromStart = item->chromStart;
    bed->chromEnd = item->chromEnd;
    bed->name = cloneString(item->name);
    bed->score = item->score;
    safecpy(bed->strand, sizeof(bed->strand), item->strand);
    bed->thickStart = item->chromStart;
    bed->thickEnd = item->chromEnd;
    bed->itemRgb = hexToDecimal(item->color);
    bed8To12(bed);
    lf = lfFromBedExtra(bed, scoreMin, scoreMax);
    lf->extra = (void *)USE_ITEM_RGB;	/* signal for coloring */
    lf->filterColor=bed->itemRgb;

    /* overload itemAttr fields to be able to pass id to hgc click box */
    struct itemAttr *id;
    AllocVar(id);
    id->chromStart = item->id;
    lf->itemAttr = id;
    slAddHead(&lfList, lf);
    wikiTrackFree(&item);
    }
sqlFreeResult(&sr);
hDisconnectCentral(&conn);

slSort(&lfList, linkedFeaturesCmp);

if (wikiTrackEnabled(database, NULL))
    {
    // add special item to allow creation of new entries
    AllocVar(bed);
    bed->chrom = chromName;
    bed->chromStart = winStart;
    bed->chromEnd = winEnd;
    bed->name = cloneString("Make new entry");
    bed->score = 100;
    bed->strand[0] = ' ';  /* no barbs when strand is unknown */
    bed->thickStart = winStart;
    bed->thickEnd = winEnd;
    bed->itemRgb = 0xcc0000;
    bed8To12(bed);
    lf = lfFromBedExtra(bed, scoreMin, scoreMax);
    lf->extra = (void *)USE_ITEM_RGB;	/* signal for coloring */
    lf->filterColor=bed->itemRgb;
    slAddHead(&lfList, lf);
    }

tg->items = lfList;
}	/*	static void wikiTrackLoadItems(struct track *tg)	*/

struct bed *wikiTrackGetBedRange(char *mapName, char *chromName,
	int start, int end)
/* fetch wiki track items as simple bed 3 list in given range */
{
struct bed *bed, *bedList = NULL;
struct sqlConnection *conn = hConnectCentral();
struct sqlResult *sr;
char **row;
char where[256];
int rowOffset;

safef(where, ArraySize(where), "db='%s'", database);

sr = hRangeQuery(conn, mapName, chromName, start, end, where, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct wikiTrack *item = wikiTrackLoad(row);
    AllocVar(bed);
    bed->chrom = cloneString(item->chrom);
    bed->chromStart = item->chromStart;
    bed->chromEnd = item->chromEnd;
    slAddHead(&bedList, bed);
    wikiTrackFree(&item);
    }
sqlFreeResult(&sr);
hDisconnectCentral(&conn);
return bedList;
}

void addWikiTrack(struct track **pGroupList)
/* Add wiki track and append to group list. */
{
if (wikiTrackEnabled(database, NULL))
    {
    struct track *tg = trackNew();
    static char longLabel[80];
    struct trackDb *tdb;
    struct sqlConnection *conn = hConnectCentral();
    if (! sqlTableExists(conn,WIKI_TRACK_TABLE))
	errAbort("loadWikiTrack configuration error, set wikiTrack.URL in hg.conf");

    linkedFeaturesMethods(tg);
    AllocVar(tdb);
    tg->mapName = WIKI_TRACK_TABLE;
    tg->canPack = TRUE;
    tg->visibility = tvHide;
    tg->hasUi = TRUE;
    tg->shortLabel = cloneString(WIKI_TRACK_LABEL);
    safef(longLabel, sizeof(longLabel), WIKI_TRACK_LONGLABEL);
    tg->longLabel = longLabel;
    tg->loadItems = wikiTrackLoadItems;
    tg->itemName = linkedFeaturesName;
    tg->mapItemName = wikiTrackMapItemName;
    tg->mapItem = wikiTrackMapItem;
    tg->priority = 99.99;
    tg->defaultPriority = 99.99;
    tg->groupName = cloneString("map");
    tg->defaultGroupName = cloneString("map");
    tg->exonArrows = TRUE;
    tg->labelNextItemButtonable = TRUE;
    tdb->tableName = cloneString(tg->mapName);
    tdb->shortLabel = cloneString(tg->shortLabel);
    tdb->longLabel = cloneString(tg->longLabel);
    tdb->useScore = 1;
    tdb->grp = cloneString(tg->groupName);
    tdb->priority = tg->priority;
    trackDbPolish(tdb);
    tg->tdb = tdb;

    slAddHead(pGroupList, tg);
    hDisconnectCentral(&conn);
    }
}

void wikiTrackMethods(struct track *tg)
/* establish loadItems function for wiki track */
{
tg->loadItems = wikiTrackLoadItems;
}
