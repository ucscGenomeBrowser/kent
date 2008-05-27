/* hgTables - Main and utility functions for table browser. */

#include "common.h"
#include "trackDb.h"
#include "hui.h"
#include "hdb.h"
#include "wikiTrack.h"
#include "hgTables.h"

static char const rcsid[] = "$Id: wikiTrack.c,v 1.1 2008/05/27 23:48:28 hiram Exp $";

void wikiTrackDb(struct trackDb **list)
/* create a trackDb entry for the wiki track */
{
struct trackDb *tdb;

AllocVar(tdb);
tdb->tableName = WIKI_TRACK_TABLE;
tdb->shortLabel = WIKI_TRACK_LABEL;
tdb->longLabel = WIKI_TRACK_LONGLABEL;
tdb->visibility = tvFull;
tdb->priority = WIKI_TRACK_PRIORITY;

tdb->html = hFileContentsOrWarning(hHelpFile(tdb->tableName));
tdb->type = "none";
tdb->grp = "map";
tdb->canPack = FALSE;

slAddHead(list, tdb);
slSort(list, trackDbCmp);
}

struct hTableInfo *wikiHti()
/* Create an hTableInfo for the wikiTrack. */
{
struct hTableInfo *hti;

AllocVar(hti);
hti->rootName = cloneString(WIKI_TRACK_TABLE);
hti->isPos = TRUE;
hti->isSplit = FALSE;
hti->hasBin = TRUE;
hti->type = cloneString("none");
strncpy(hti->chromField, "chrom", 32);
strncpy(hti->startField, "chromStart", 32);
strncpy(hti->endField, "chromEnd", 32);
strncpy(hti->nameField, "name", 32);
strncpy(hti->scoreField, "score", 32);
strncpy(hti->strandField, "strand", 32);

return(hti);
}

void doSummaryStatsWikiTrack(struct sqlConnection *conn)
/* Put up page showing summary stats for wikiTrack. */
{
struct sqlConnection *wikiConn = wikiConnect();
doSummaryStatsBed(wikiConn);
wikiDisconnect(&wikiConn);
}

static void wikiTrackFilteredBedOnRegion(
	struct region *region,	/* Region to get data from. */
	struct hash *idHash,	/* Hash of identifiers or NULL */
	struct bedFilter *bf,	/* Filter or NULL */
	struct lm *lm,		/* Local memory pool. */
	struct bed **pBedList  /* Output get's appended to this list */
	)
/* Get the wikiTrack items passing filter on a single region. */
{
struct bed *bed;

int fieldCount = 6;
char query[512];
int rowOffset;
char **row;
struct sqlConnection *wikiConn = wikiConnect();
struct sqlResult *sr = NULL;
char where[512];

char *filter = filterClause(wikiDbName(), WIKI_TRACK_TABLE, region->chrom);

if (filter)
    safef(where, sizeof(where), "db='%s' AND %s", database, filter);
else
    safef(where, sizeof(where), "db='%s'", database);

safef(query, sizeof(query), "select * from %s", WIKI_TRACK_TABLE);
sr = hRangeQuery(wikiConn, WIKI_TRACK_TABLE, region->chrom,
    region->start, region->end, where, &rowOffset);

while ((row = sqlNextRow(sr)) != NULL)
    {
    bed = bedLoadN(row+rowOffset, fieldCount);
    if ((idHash == NULL || hashLookup(idHash, bed->name)) &&
	(bf == NULL || bedFilterOne(bf, bed)))
	{
	struct bed *copy = lmCloneBed(bed, lm);
	slAddHead(pBedList, copy);
	}
    }
sqlFreeResult(&sr);
wikiDisconnect(&wikiConn);
}

struct bed *wikiTrackGetFilteredBeds(char *name, struct region *regionList,
	struct lm *lm, int *retFieldCount)
/* Get list of beds from the wikiTrack * in current regions and that pass
 *	filters.  You can bedFree this when done.  */
{
struct bed *bedList = NULL;
struct hash *idHash = NULL;
struct bedFilter *bf = NULL;
struct region *region = NULL;

/* Figure out how to filter things. */
bf = bedFilterForCustomTrack(name);
idHash = identifierHash(database, name);

/* Grab filtered beds for each region. */
for (region = regionList; region != NULL; region = region->next)
    wikiTrackFilteredBedOnRegion(region, idHash, bf, lm, &bedList);

/* clean up. */
hashFree(&idHash);
slReverse(&bedList);
return bedList;
}
