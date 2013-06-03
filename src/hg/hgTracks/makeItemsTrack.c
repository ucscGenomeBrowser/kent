/* makeItemsTrack.c - supports tracks of type makeItems.  Users can drag to create an item
 * and click to edit one. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"
#include "bed.h"
#include "binRange.h"
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
char *trackName = words[1];
char *chrom = words[2];
int chromStart = sqlUnsigned(words[3]);
int chromEnd = sqlUnsigned(words[4]);

/* Create a new item based on command. */
struct makeItemsItem *item;
AllocVar(item);
item->bin = binFromRange(chromStart, chromEnd);
item->chrom = cloneString(chrom);
item->chromStart = item->thickStart = chromStart;
item->chromEnd = item->thickEnd = chromEnd;
item->name = cloneString("");
item->score = 1000;
item->strand[0] = '.';
item->description = cloneString("");

/* Add item to database. */
struct track *track = hashMustFindVal(trackHash, trackName);
struct customTrack *ct = track->customPt;
char *tableName = ct->dbTableName;
struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
makeItemsItemSaveToDb(conn, item, tableName, 0);
hFreeConn(&conn);

freez(&dupeCommand);
}

static int makeItemsExtraHeight(struct track *track)
/* Return extra height of track. */
{
return tl.fontHeight+2;
}

static void updateTextField(char *trackName, struct sqlConnection *conn,
	char *tableName, char *fieldName, int id)
/* Update text valued field with new val. */
{
char varName[128];
char sql[256];
safef(varName, sizeof(varName), "%s_%s", trackName, fieldName);
char *newVal = cartOptionalString(cart, varName);
if (newVal != NULL)
    {
    sqlSafef(sql, sizeof(sql), "update %s set %s='%s' where id=%d",
	    tableName, fieldName, newVal, id);
    sqlUpdate(conn, sql);
    cartRemove(cart, varName);	/* We don't need it any more. */
    }
}

static void makeItemsEditOrDelete(char *trackName, struct sqlConnection *conn, char *tableName)
/* Troll through cart variables looking for things that indicate user edited item
 * or deleted it in hgc,  and carry out edits. See hgc/makeItemsClick.c. */
{
char varName[128];
char sql[256];
safef(varName, sizeof(varName), "%s_%s", trackName, "id");
char *idString = cartOptionalString(cart, varName);
if (idString != NULL)
    {
    int id = sqlUnsigned(idString);
    idString = NULL;			// Will be no good after cartRemove
    cartRemove(cart, varName);		// Remove so only do edits once.

    /* Handle cancel. */
    safef(varName, sizeof(varName), "%s_%s", trackName, "cancel");
    if (cartVarExists(cart, varName))
        {
	cartRemove(cart, varName);	// Only want to do cancels once
	return;
	}

    /* Handle delete. */
    safef(varName, sizeof(varName), "%s_%s", trackName, "delete");
    if (cartVarExists(cart, varName))
        {
	cartRemove(cart, varName);	// Especially only want to do deletes once!
	sqlSafef(sql, sizeof(sql), "delete from %s where id=%d", tableName, id);
	sqlUpdate(conn, sql);
	return;
	}

    /* Handle edits. */
    updateTextField(trackName, conn, tableName, "name", id);
    updateTextField(trackName, conn, tableName, "description", id);
    }
}

void makeItemsLoadItems(struct track *track)
/* Load up items in track already.  Also make up a pseudo-item that is
 * where you drag to create an item. */
{
struct bed *bedList = NULL;
struct customTrack *ct = track->customPt;
char *tableName = ct->dbTableName;
struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
makeItemsEditOrDelete(track->track, conn, tableName);
int rowOffset;
struct sqlResult *sr = hRangeQuery(conn, tableName, chromName, winStart, winEnd, NULL, &rowOffset);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct bed *bed = bedLoadN(row+rowOffset, 9);
    /* Add id to front of name */
    char *id = row[rowOffset + 10];
    char buf[64];
    safef(buf, sizeof(buf), "%s %s", id, bed->name);
    freeMem(bed->name);
    bed->name = cloneString(buf);
    slAddHead(&bedList, bed);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&bedList);
track->items = bedList;
}

char *makeItemsItemName(struct track *track, void *item)
/* Return name of one of an item to display on left side. */
{
struct bed *bed = item;
char *name = bed->name;
name = skipToSpaces(name);
name = skipLeadingSpaces(name);
return name;
}

void makeItemsDrawItems(struct track *track, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width,
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw simple Bed items. */
{
int dragBarHeight = makeItemsExtraHeight(track);
hvGfxTextCentered(hvg, xOff, yOff, width, dragBarHeight, color, font, 
	"--- Drag here or in between items to create a new item. ---");
bedDrawSimple(track, seqStart, seqEnd, hvg, xOff, yOff + dragBarHeight, width,
	font, color, vis);
}


void makeItemsDrawLeftLabels(struct track *track, int seqStart, int seqEnd,
    struct hvGfx *hvg, int xOff, int yOff, int width, int height,
    boolean withCenterLabels, MgFont *font,
    Color color, enum trackVisibility vis)
/* Draw left label - just in dense or full mode. Needed to cope with empty space at top of track. */
{
int y = yOff + makeItemsExtraHeight(track);
int fontHeight = mgFontLineHeight(font);
if (withCenterLabels)
    y += mgFontLineHeight(font);
if (vis == tvDense)
    hvGfxTextRight(hvg, xOff, y, width-1, fontHeight, color, font, track->shortLabel);
else if (vis == tvFull)
    {
    struct bed *bed, *bedList = track->items;
    for (bed = bedList; bed != NULL; bed = bed->next)
	{
	if (track->itemLabelColor != NULL)
	    color = track->itemLabelColor(track, bed, hvg);
	int itemHeight = track->itemHeight(track, bed);
	hvGfxTextRight(hvg, xOff, y, width - 1,
	    itemHeight, color, font, track->itemName(track, bed));
	y += itemHeight;
	}
    }
/* In pack mode the draw routine takes care of the labels. */
}


int makeItemsTotalHeight(struct track *track, enum trackVisibility vis)
/* Most fixed height track groups will use this to figure out the height
 * they use. */
{
track->height = tgFixedTotalHeightOptionalOverflow(track, vis, tl.fontHeight+1, tl.fontHeight, FALSE) + 
	makeItemsExtraHeight(track);
return track->height;
}


void makeItemsMethods(struct track *track)
/* Set up special methods for makeItems type tracks. */
{
bedMethods(track);
track->totalHeight = makeItemsTotalHeight;
track->drawItems = makeItemsDrawItems;
track->drawLeftLabels = makeItemsDrawLeftLabels;
track->loadItems = makeItemsLoadItems;
track->itemName = makeItemsItemName;
}

