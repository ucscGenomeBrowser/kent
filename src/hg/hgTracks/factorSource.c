/* Deal with factorSource type tracks, which are used to display 
 * transcription factor binding sites as measured in a number of
 * tissue or cell type sources. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"
#include "expRecord.h"

static struct bed *loadOne(char **row)
/* Load up factorSource from array of strings. */
{
return bedLoadN(row, 15);
}

static void loadAll(struct track *track)
/* Go load all items in window. */
{
bedLoadItem(track, track->mapName, (ItemLoader)loadOne);
}

static int rightPixels(struct track *track, void *item)
/* Return number of pixels we need to the right. */
{
return track->sourceRightPixels;
}

static void factorSourceDrawItemAt(struct track *track, void *item, 
	struct hvGfx *hvg, int xOff, int y, 
	double scale, MgFont *font, Color color, enum trackVisibility vis)
/* Draw factorSource item at a particular position. */
{
/* Calculate position, and draw box around where we are. */
struct bed *bed = item;
int heightPer = track->heightPer;
int x1 = round((double)((int)bed->chromStart-winStart)*scale) + xOff;
int x2 = round((double)((int)bed->chromEnd-winStart)*scale) + xOff;
int w = x2-x1;
if (w < 1)
    w = 1;
hvGfxBox(hvg, x1, y, w, heightPer, color);

/* Draw text to the right */
if (vis == tvFull || vis == tvPack)
    {
    int x = x2 + tl.mWidth/2;
    int i;
    for (i=0; i<track->sourceCount; ++i)
	{
	char *label = track->sources[i]->name;
	int w = mgFontStringWidth(font, label);
	float score = bed->expScores[i];
	if (score > 0.0)
	    {
	    int grayIx = grayInRange(score*100, 0, 100);
	    int color = shadesOfGray[grayIx];
	    hvGfxTextCentered(hvg, x, y, w, heightPer, color, font, label);
	    x += mgFontStringWidth(font, label);
	    }
	}
    }
}

void factorSourceMethods(struct track *track)
/* Set up special methods for factorSource type tracks. */
{
/* Start out as a bed, and then specialize loader, mark self as packable. */
track->bedSize = 15;
bedMethods(track);
track->drawItemAt = factorSourceDrawItemAt;
track->loadItems = loadAll;
track->itemRightPixels = rightPixels;

/* Get the associated data describing the various sources. */
track->expTable = trackDbRequiredSetting(track->tdb, "sourceTable");
struct sqlConnection *conn = hAllocConn(database);
char query[256];
safef(query, sizeof(query), "select count(*) from %s", track->expTable);
track->sourceCount = sqlQuickNum(conn, query);
safef(query, sizeof(query), "select * from %s order by id", track->expTable);
struct expRecord *exp, *expList = expRecordLoadByQuery(conn, query);
int expIx;
AllocArray(track->sources, track->sourceCount);
for (exp=expList, expIx=0; exp != NULL; exp = exp->next, expIx += 1)
    track->sources[expIx] = exp;
hFreeConn(&conn);

/* Figure out how many pixels need to the right. */
int rightCount = tl.mWidth/2;
for (expIx=0; expIx < track->sourceCount; ++expIx)
    rightCount += mgFontStringWidth(tl.font, track->sources[expIx]->name);
track->sourceRightPixels = rightCount;
}

