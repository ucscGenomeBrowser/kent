/* wikiTrack - wikiTrack functions */
#include "common.h"
#include "jksql.h"
#include "hgTracks.h"
#include "hdb.h"
#include "wikiLink.h"
#include "wikiTrack.h"

static char const rcsid[] = "$Id: wikiTrack.c,v 1.1 2007/05/22 22:17:45 hiram Exp $";

static char *wikiTrackItemName(struct track *tg, void *item)
/* Return name of bed track item. */
{
struct bed *bed = item;
if (bed->name == NULL)
    return "";
return bed->name;
}

static void wikiTrackLoadItems(struct track *tg)
/* Load the items from the wikiTrack table */
{
struct bed *bed, *list = NULL;
struct sqlConnection *conn = hConnectCentral();
struct sqlResult *sr;
char **row;
int rowOffset;
char where[256];

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
    slAddHead(&list, bed);
    wikiTrackFree(&item);
    }
sqlFreeResult(&sr);
hDisconnectCentral(&conn);
slReverse(&list);

if (wikiTrackEnabled(NULL))
    {
// add dummy links
AllocVar(bed);
bed->chrom = chromName;
bed->chromStart = winStart;
bed->chromEnd = winEnd;
bed->name = "Make new entry";
bed->strand[0] = '+';
bed->score = 100;
slAddHead(&list, bed);
    }

tg->items = list;
}

void wikiTrackMethods(struct track *tg)
{
tg->loadItems = wikiTrackLoadItems;
}

void loadWikiTrack(struct track **pGroupList)
/* Load up wiki track and append to list. */
{
if (wikiTrackEnabled(NULL))
    {
    struct track *tg = trackNew();
    static char longLabel[80];
    struct trackDb *tdb;
    struct sqlConnection *conn = hConnectCentral();
    if (! sqlTableExists(conn,WIKI_TRACK_TABLE))
	errAbort("loadWikiTrack configuration error, set wikiTrack.URL in hg.conf");

    bedMethods(tg);
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
    trackDbPolish(tdb);
    tg->tdb = tdb;

    slAddHead(pGroupList, tg);
    hDisconnectCentral(&conn);
    }
}
