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
#include "factorSource.h"


static struct factorSource *loadOne(char **row)
/* Load up factorSource from array of strings. */
{
return factorSourceLoad(row);
}

/* Save info about motifs */
struct motifInfo {
    char *motifTable;
    struct hash *motifTargets;
};

static void factorSourceLoadItems(struct track *track)
/* Load all items (and motifs if table is present) in window */
{
bedLoadItem(track, track->table, (ItemLoader)loadOne);

char *motifTable = trackDbSetting(track->tdb, "motifTable");
if (motifTable == NULL)
    return;

struct sqlConnection *conn = hAllocConn(database);
if (sqlTableExists(conn, motifTable))
    {
    struct motifInfo *motifInfo = NULL;
    AllocVar(motifInfo);
    track->extraUiData = motifInfo;
    motifInfo->motifTable = cloneString(motifTable);

    char *motifMapTable = trackDbSetting(track->tdb, "motifMapTable");
    if (sqlTableExists(conn, motifMapTable))
        {
        /* load (small) map table into hash */
        char query[256];
        struct sqlResult *sr = NULL;
        char **row = NULL;
        sqlSafef(query, sizeof(query), "select target, motif from %s", motifMapTable);
        sr = sqlGetResult(conn, query);
        struct hash *targetHash = newHash(0);
        while ((row = sqlNextRow(sr)) != NULL)
            {
            char *target = row[0];
            char *motif = row[1];
            if (motif[0] != 0)
                hashAdd(targetHash, cloneString(target), cloneString(motif));
            }
        sqlFreeResult(&sr);
        motifInfo->motifTargets = targetHash;
        }
    }
hFreeConn(&conn);
}

static int rightPixels(struct track *track, void *item)
/* Return number of pixels we need to the right. */
{
struct factorSource *fs = item;
struct dyString *dy = dyStringNew(0);
int i;
for (i=0; i<fs->expCount; ++i)
    {
    int expNum = fs->expNums[i];
    char *label = track->sources[expNum]->name;
    dyStringAppend(dy, label);
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
struct factorSource *fs = item;
int grayIx = grayInRange(fs->score, 0, 1000);
color = shadesOfGray[grayIx];

/* Calculate position, and draw box around where we are. */
int heightPer = track->heightPer;
int x1 = round((double)((int)fs->chromStart-winStart)*scale) + xOff;
int x2 = round((double)((int)fs->chromEnd-winStart)*scale) + xOff;
int w = x2-x1;
if (w < 1)
    w = 1;
hvGfxBox(hvg, x1, y, w, heightPer, color);

/* Draw text to the right */
if (vis == tvFull || vis == tvPack)
    {
    int x = x2 + tl.mWidth/2;
    int i;
    for (i=0; i<fs->expCount; ++i)
	{
        int id = fs->expNums[i];
	char *label = track->sources[id]->name;
	int w = mgFontStringWidth(font, label);
	float score = fs->expScores[i];
        int grayIx = grayInRange(score, 0, 1000);
        int color = shadesOfGray[grayIx];
        hvGfxTextCentered(hvg, x, y, w, heightPer, color, font, label);
        x += w;
	}
    }
}

static void factorSourceDrawMotifForItemAt(struct track *track, void *item, 
	struct hvGfx *hvg, int xOff, int y, 
	double scale, MgFont *font, Color color, enum trackVisibility vis)
/* Draw motif on factorSource item at a particular position. */
{
int rowOffset;
struct motifInfo *motifInfo = (struct motifInfo *)track->extraUiData;
if (motifInfo == NULL)
    return;

// Draw region with highest motif score
// NOTE: shows only canonical motif for the factor, so hides co-binding potential
// NOTE: current table has single motif per factor
struct sqlConnection *conn = hAllocConn(database);

struct sqlResult *sr;
char where[256];
char **row;

// QUESTION: Is performance adequate with this design ?  Could query table once and
// add motif ranges to items.

struct factorSource *fs = item;
char *target = fs->name;
int heightPer = track->heightPer;
struct hash *targetHash = (struct hash *)motifInfo->motifTargets;
if (targetHash != NULL)
    {
    target = hashFindVal(targetHash, fs->name);
    if (target == NULL)
        target = fs->name;
    }
sqlSafefFrag(where, sizeof(where), "name = '%s' order by score desc", target);
sr = hRangeQuery(conn, motifInfo->motifTable, fs->chrom, fs->chromStart,
                 fs->chromEnd, where, &rowOffset);
#define HIGHEST_SCORING
#ifdef HIGHEST_SCORING
if ((row = sqlNextRow(sr)) != NULL)
#else
     while ((row = sqlNextRow(sr)) != NULL)
#endif
    {
    int start = sqlUnsigned(row[rowOffset+1]);
    int end = sqlUnsigned(row[rowOffset+2]);
    int x1 = round((double)((int)start-winStart)*scale) + xOff;
    int x2 = round((double)((int)end-winStart)*scale) + xOff;
    int w = x2-x1;
    if (w < 1)
        w = 1;
    hvGfxBox(hvg, x1, y, w, heightPer, color);
    if (w > 2)
        {
        // Draw chevrons for directionality
        Color textColor = hvGfxContrastingColor(hvg, color);
        int midY = y + (heightPer>>1);
        int dir = (row[rowOffset+5][0] == '+' ? 1 : -1);
        clippedBarbs(hvg, x1, midY, w, tl.barbHeight, tl.barbSpacing,
                       dir, textColor, TRUE);
        }
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
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

// Highlight motif regions in green
color = hvGfxFindColorIx(hvg, 22, 182, 33);
//Color color = hvGfxFindColorIx(hvg, 25, 204, 37);
track->drawItemAt = factorSourceDrawMotifForItemAt;

// suppress re-draw of item labels in motif color
extern boolean withLeftLabels;
withLeftLabels = FALSE;
genericDrawItems(track, seqStart, seqEnd, hvg, xOff, yOff, width,
	font, color, vis);
withLeftLabels = TRUE;
}

void factorSourceMethods(struct track *track)
/* Set up special methods for factorSource type tracks. */
{
/* Start out as a bed, and then specialize loader, mark self as packable. */
track->bedSize = 5;
bedMethods(track);
track->drawItemAt = factorSourceDrawItemAt;
track->drawItems = factorSourceDraw;
track->loadItems = factorSourceLoadItems;
track->itemRightPixels = rightPixels;

/* Get the associated data describing the various sources. */
track->expTable = trackDbRequiredSetting(track->tdb, SOURCE_TABLE);
struct sqlConnection *conn = hAllocConn(database);
char query[256];
track->sourceCount = sqlTableSizeIfExists(conn, track->expTable);
sqlSafef(query, sizeof(query), "select * from %s order by id", track->expTable);
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

