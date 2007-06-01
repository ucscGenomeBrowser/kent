/* wikiTrack - wikiTrack functions */
#include "common.h"
#include "jksql.h"
#include "hgTracks.h"
#include "hdb.h"
#include "ra.h"
#include "bedCart.h"
#include "wikiLink.h"
#include "wikiTrack.h"

static char const rcsid[] = "$Id: wikiTrack.c,v 1.3 2007/06/01 22:36:53 hiram Exp $";

static char *wikiTrackItemName(struct track *tg, void *item)
/* Return name of bed track item. */
{
struct linkedFeatures *lf = item;
if (lf->name == NULL)
    return "";
return lf->name;
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
    slAddHead(&lfList, lf);
    wikiTrackFree(&item);
    }
sqlFreeResult(&sr);
hDisconnectCentral(&conn);

slSort(&lfList, linkedFeaturesCmp);

if (wikiTrackEnabled(NULL))
    {
    // add special item to allow creation of new entries
    AllocVar(bed);
    bed->chrom = chromName;
    bed->chromStart = winStart;
    bed->chromEnd = winEnd;
    bed->name = cloneString("Make new entry");
    bed->score = 100;
    bed->strand[0] = '+';
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
}

void wikiTrackMethods(struct track *tg)
/* establish loadItems function for wiki track */
{
tg->loadItems = wikiTrackLoadItems;
}

void addWikiTrack(struct track **pGroupList)
/* Add wiki track and append to group list. */
{
if (wikiTrackEnabled(NULL))
    {
    struct track *tg = trackNew();
    static char longLabel[80];
    struct trackDb *tdb;
    struct sqlConnection *conn = hConnectCentral();
    if (! sqlTableExists(conn,WIKI_TRACK_TABLE))
	errAbort("loadWikiTrack configuration error, set wikiTrack.URL in hg.conf");

    linkedFeaturesMethods(tg);
    AllocVar(tdb);
    tg->mapName = "wikiTrack";
    tg->canPack = TRUE;
    tg->visibility = tvHide;
    tg->hasUi = FALSE;
    tg->shortLabel = cloneString("Wiki Track");
    safef(longLabel, sizeof(longLabel), "Wiki Track user annotations");
    tg->longLabel = longLabel;
    tg->loadItems = wikiTrackLoadItems;
    tg->itemName = wikiTrackItemName;
    tg->mapItemName = wikiTrackItemName;
    tg->priority = 99;
    tg->defaultPriority = 99;
    tg->groupName = "x";
    tg->defaultGroupName = "x";
    tdb->tableName = tg->mapName;
    tdb->shortLabel = tg->shortLabel;
    tdb->longLabel = tg->longLabel;
    tdb->useScore = 1;
#ifdef NOT
    tdb->settings = NULL;
    tdb->settingsHash = raFromString(tdb->settings);
    /* add to hash */
    hashReplace(tdb->settingsHash, "itemRgb", "on");
    /* regenerate settings string */
    tdb->settings = hashToRaString(tdb->settingsHash);
    tdb->type = cloneString("bed 9");
#endif
    trackDbPolish(tdb);
    tg->tdb = tdb;

    slAddHead(pGroupList, tg);
    hDisconnectCentral(&conn);
    }
}
