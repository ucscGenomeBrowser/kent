/* Deal with factorSource type tracks, which are used to display 
 * transcription factor binding sites as measured in a number of
 * tissue or cell type sources. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"
#include "expRecord.h"
#include "dystring.h"
#include "factorSource.h"
#include "bed6FloatScore.h"

static struct factorSource *loadOne(char **row)
/* Load up factorSource from array of strings. */
{
return factorSourceLoad(row);
}

/* Save info about factors and their motifs */
struct factorSourceInfo {
    boolean showCellAbbrevs;
    struct hash *factorChoices;
    struct hash *motifTargets;
    struct bed6FloatScore *motifs;
};

boolean factorFilter(struct track *track, void *item)
/* Returns true if an item should be passed by the filter. NOTE: single filter supported here*/
{
struct hash *factorHash = ((struct factorSourceInfo *)track->extraUiData)->factorChoices;
if (track->extraUiData != NULL && factorHash != NULL)
    if (hashLookup(factorHash, ((struct factorSource *)item)->name) != NULL)
        return TRUE;
return FALSE;
}

static void factorSourceLoadItems(struct track *track)
/* Load all items (and motifs if table is present) in window */
{
bedLoadItem(track, track->table, (ItemLoader)loadOne);
if (track->items == NULL)
    return;

struct factorSourceInfo *fsInfo = NULL;
AllocVar(fsInfo);
track->extraUiData = fsInfo;

// Check UI setting to show cell abbreviations
char varName[64];
safef(varName, sizeof(varName), "%s.showCellAbbrevs", track->track);
fsInfo->showCellAbbrevs = cartUsualBoolean(cart, varName, TRUE);

// Filter factors based on multi-select
filterBy_t *filter = filterBySetGet(track->tdb, cart, NULL);
if (filter != NULL && filter->slChoices != NULL && differentString(filter->slChoices->name, "All"))
    {
    struct slName *choice;
    struct hash *factorHash = newHash(0);
    for (choice = filter->slChoices; choice != NULL; choice = choice->next)
        {
        hashAdd(factorHash, choice->name, NULL);
        }
    fsInfo->factorChoices = factorHash;
    filterItems(track, factorFilter, "include");
}

// Motifs
safef(varName, sizeof(varName), "%s.highlightMotifs", track->track);
if (!cartUsualBoolean(cart, varName, trackDbSettingClosestToHomeOn(track->tdb, "motifDrawDefault")))
    return;

char *motifMaxWindow = trackDbSetting(track->tdb, "motifMaxWindow");
if (motifMaxWindow != NULL)
    if (winEnd - winStart > sqlUnsigned(motifMaxWindow))
        return;

char *motifTable = trackDbSetting(track->tdb, "motifTable");
if (motifTable == NULL)
    return;

struct sqlConnection *conn = hAllocConn(database);
if ((track->items != NULL) & sqlTableExists(conn, motifTable))
    {

    // Load all motifs for items in window (including motifs outside of window)
    struct slList *items = track->items;
    int winStartSave = winStart;
    int winEndSave = winEnd;

    winStart = min(winStart, ((struct factorSource *)items)->chromStart);
    winEnd = max(winEnd, ((struct factorSource *)items)->chromEnd);
    bedLoadItem(track, motifTable, (ItemLoader)bed6FloatScoreLoad);
    fsInfo->motifs = track->items;

    track->items = items;
    winStart = winStartSave;
    winEnd = winEndSave;

    char *motifMapTable = trackDbSetting(track->tdb, "motifMapTable");
    if (motifMapTable != NULL && sqlTableExists(conn, motifMapTable))
        {
        // Load map table 
        char query[256];
        struct sqlResult *sr = NULL;
        char **row = NULL;
        sqlSafef(query, sizeof(query), "select target, motif from %s", motifMapTable);
        sr = sqlGetResult(conn, query);
        struct hash *targetHash = newHash(0);
        while ((row = sqlNextRow(sr)) != NULL)
            {
            char *target = row[0];
            char *motifs = row[1];   // string, comma-sep list, or empty string
            if (motifs[0] != 0)
                hashAdd(targetHash, target, slNameListFromString(motifs, ','));
            }
        sqlFreeResult(&sr);
        fsInfo->motifTargets = targetHash;
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
struct factorSourceInfo *fsInfo = (struct factorSourceInfo *)track->extraUiData;
if ((vis == tvFull || vis == tvPack) && fsInfo->showCellAbbrevs)
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
struct factorSourceInfo *fsInfo = (struct factorSourceInfo *)track->extraUiData;
if (fsInfo == NULL)
    return;

// Draw region with highest motif score
// NOTE: shows only canonical motif for the factor, so hides co-binding potential
// NOTE: current table has single motif per factor
struct factorSource *fs = item;
int heightPer = track->heightPer;
struct hash *targetHash = fsInfo->motifTargets;
struct slName *motifNames = NULL;
if (targetHash != NULL)
    motifNames = hashFindVal(targetHash, fs->name);
if (motifNames == NULL)
    motifNames = slNameNew(fs->name);

// Find motifs that lie completely within peak (sensible for ChIP-seq data ?)
struct bed6FloatScore *m, *motif = NULL, *motifs = NULL;

for (m = fsInfo->motifs; m != NULL && m->chromEnd <= fs->chromEnd; m = m->next)
    {
    if (m->chromStart < fs->chromStart)
        continue;
    if (!slNameInList(motifNames, m->name))
        continue;
    AllocVar(motif);
    motif->chrom = cloneString(m->chrom);
    motif->chromStart = m->chromStart;
    motif->chromEnd = m->chromEnd;
    motif->name = m->name;
    motif->score = m->score;
    motif->strand[0] = m->strand[0];
    slAddHead(&motifs, motif);
    }
if (motifs == NULL)
    return;
slSort(&motifs, bedCmpScore);
slReverse(&motifs);

#define HIGHEST_SCORING
#ifdef HIGHEST_SCORING
if ((motif = motifs) != NULL)
    {
    if (motif->chromStart > winEnd || motif->chromEnd < winStart)
        return;
#else
for (motif = motifs; motifs != NULL; motif = motif->next)
    {
    // exclude motifs outside of window
    if (motif->chromStart > winEnd || motif->chromEnd < winStart)
        continue;
#endif
    int x1 = round((double)((int)motif->chromStart-winStart)*scale) + xOff;
    int x2 = round((double)((int)motif->chromEnd-winStart)*scale) + xOff;
    int w = x2-x1;
    if (w < 1)
        w = 1;
    hvGfxBox(hvg, x1, y, w, heightPer, color);
    if (w > 2)
        {
        // Draw chevrons for directionality
        Color textColor = hvGfxContrastingColor(hvg, color);
        int midY = y + (heightPer>>1);
        int dir = (*motif->strand == '+' ? 1 : -1);
        if (vis != tvDense)
            clippedBarbs(hvg, x1, midY, w, tl.barbHeight, tl.barbSpacing,
                       dir, textColor, TRUE);
        }
    }
//bed6FloatScoreFreeList(&motifs);  # not worth exec time (or debug time)
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
boolean saveWithLeftLabels = withLeftLabels;
withLeftLabels = FALSE;
genericDrawItems(track, seqStart, seqEnd, hvg, xOff, yOff, width,
	font, color, vis);
withLeftLabels = saveWithLeftLabels;
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

