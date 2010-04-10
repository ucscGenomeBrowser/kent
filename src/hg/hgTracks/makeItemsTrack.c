/* makeItemsTrack.c - supports tracks of type makeItems.  Users can drag to create an item
 * and click to edit one. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"
#include "bed.h"
#include "makeItemsItem.h"

void makeItemsJsCommand(char *command, struct track *trackList, struct hash *trackHash)
/* Execute some command sent to us from the javaScript.  All we know for sure is that
 * the first word of the command is "makeItems."  We expect it to be of format:
 *    makeItems <trackName> <chrom> <chromStart> <chromEnd>
 * If it is indeed of this form then we'll create a new makeItemsItem that references this
 * location and stick it in the named track. */
{
/* Parse out command into local variables. */
char *words[6];
char *dupeCommand = cloneString(command);	/* For parsing. */
int wordCount = chopLine(dupeCommand, words);
if (wordCount != 5)
   errAbort("Expecting %d words in jsCommand '%s'", wordCount, command);
uglyf("makeItemsJsCommand %s|%s|%s|%s|%s<BR>\n", words[0], words[1], words[2], words[3], words[4]);
char *trackName = words[1];
char *chrom = words[2];
int chromStart = sqlUnsigned(words[3]);
int chromEnd = sqlUnsigned(words[4]);

/* Create a new item based on command. */
struct makeItemsItem *item;
AllocVar(item);
item->chrom = cloneString(chrom);
item->chromStart = item->thickStart = chromStart;
item->chromEnd = item->thickEnd = chromEnd;
item->name = cloneString("new");
item->score = 1000;
item->strand[0] = '.';
item->description = cloneString("n/a");

/* Add item to database. */
struct track *track = hashMustFindVal(trackHash, trackName);
struct customTrack *ct = track->customPt;
char *tableName = ct->dbTableName;
struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
makeItemsItemSaveToDbEscaped(conn, item, tableName, 0);
hFreeConn(&conn);

freez(&dupeCommand);
}

void makeItemsLoadItems(struct track *tg)
/* Load up items in track already.  Also make up a pseudo-item that is
 * where you drag to create an item. */
{
struct bed *bedList = NULL;
struct bed *firstItem;
AllocVar(firstItem);
firstItem->chrom = cloneString(chromName);
firstItem->chromStart = winStart;
firstItem->chromEnd = winEnd;
firstItem->name = cloneString("Drag here to create a new item");
struct customTrack *ct = tg->customPt;
char *tableName = ct->dbTableName;
firstItem->name = cloneString(tableName);
struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
int rowOffset;
struct sqlResult *sr = hRangeQuery(conn, tableName, chromName, winStart, winEnd, NULL, &rowOffset);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct bed *bed = bedLoad6(row+rowOffset);
    slAddHead(&bedList, bed);
    }
sqlFreeResult(&sr);
#ifdef SOON
#endif /* SOON */
hFreeConn(&conn);
slReverse(&bedList);
slAddHead(&bedList, firstItem);
tg->items = bedList;
}

void makeItemsMethods(struct track *track)
/* Set up special methods for makeItems type tracks. */
{
bedMethods(track);
track->loadItems = makeItemsLoadItems;
track->mapsSelf = TRUE;
track->canPack = TRUE;
}

