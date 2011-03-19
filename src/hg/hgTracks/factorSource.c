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
#include "dystring.h"
#include "txCluster.h"

static struct bed *loadOne(char **row)
/* Load up factorSource from array of strings. */
{
return bedLoadN(row, 15);
}

static void loadAll(struct track *track)
/* Go load all items in window. */
{
bedLoadItem(track, track->table, (ItemLoader)loadOne);
}

static int rightPixels(struct track *track, void *item)
/* Return number of pixels we need to the right. */
{
struct bed *bed = item;
struct dyString *dy = dyStringNew(0);
int i;
for (i=0; i<track->sourceCount; ++i)
    {
    char *label = track->sources[i]->name;
    float score = bed->expScores[i];
    if (score > 0.0)
	{
	dyStringAppend(dy, label);
	}
    }
int result = mgFontStringWidth(tl.font, dy->string);
dyStringFree(&dy);
return result;
}

static void factorSourceDrawItemAt(struct track *track, void *item, 
	struct hvGfx *hvg, int xOff, int y, 
	double scale, MgFont *font, Color color, enum trackVisibility vis)
/* Draw factorSource item at a particular position. */
{
/* Figure out maximum score and draw box based on it. */
int i, rowOffset;
struct bed *bed = item;
double maxScore = 0.0;
char *motifTable = NULL;
#ifdef TXCLUSTER_MOTIFS_TABLE
motifTable = TXCLUSTER_MOTIFS_TABLE;
#endif

for (i=0; i<track->sourceCount; ++i)
    {
    float score = bed->expScores[i];
    if (score > maxScore)
        maxScore = score;
    }
int grayIx = grayInRange(bed->score, 0, 1000);
color = shadesOfGray[grayIx];

/* Calculate position, and draw box around where we are. */
int heightPer = track->heightPer;
int x1 = round((double)((int)bed->chromStart-winStart)*scale) + xOff;
int x2 = round((double)((int)bed->chromEnd-winStart)*scale) + xOff;
int w = x2-x1;
if (w < 1)
    w = 1;
hvGfxBox(hvg, x1, y, w, heightPer, color);

if(motifTable != NULL)
    {
    // Draw region with highest motif score
    struct sqlConnection *conn = hAllocConn(database);
    if(sqlTableExists(conn, motifTable))
        {
        struct sqlResult *sr;
        char where[256];
        char **row;

        safef(where, sizeof(where), "name = '%s'", bed->name);
        sr = hRangeQuery(conn, "wgEncodeRegTfbsClusteredMotifs", bed->chrom, bed->chromStart,
                         bed->chromEnd, where, &rowOffset);
        while((row = sqlNextRow(sr)) != NULL)
            {
            Color color = hvGfxFindColorIx(hvg, 28, 226, 40);
            int start = sqlUnsigned(row[rowOffset+1]);
            int end = sqlUnsigned(row[rowOffset+2]);
            int x1 = round((double)((int)start-winStart)*scale) + xOff;
            int x2 = round((double)((int)end-winStart)*scale) + xOff;
            int w = x2-x1;
            if (w < 1)
                w = 1;
            hvGfxBox(hvg, x1, y, w, heightPer, color);
            }
        sqlFreeResult(&sr);
        }
    hFreeConn(&conn);
    }

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
	    int grayIx = grayInRange(score, 0, 1000);
	    int color = shadesOfGray[grayIx];
	    hvGfxTextCentered(hvg, x, y, w, heightPer, color, font, label);
	    x += w;
	    }
	}
    }
}

static void factorSourceDraw(struct track *track, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width,
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw factorSource items. */
{
if (vis == tvDense)
    slSort(&track->items, bedCmpScore);
genericDrawItems(track, seqStart, seqEnd, hvg, xOff, yOff, width,
	font, color, vis);
}

void factorSourceMethods(struct track *track)
/* Set up special methods for factorSource type tracks. */
{
/* Start out as a bed, and then specialize loader, mark self as packable. */
track->bedSize = 15;
bedMethods(track);
track->drawItemAt = factorSourceDrawItemAt;
track->drawItems = factorSourceDraw;
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

