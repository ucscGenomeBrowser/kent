/* wigMafTrack - display multiple alignment files with score wiggle
 * and base-level alignment, or else density plot of pairwise alignments
 * (zoomed out) */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"
#include "maf.h"
#include "scoredRef.h"
#include "wiggle.h"
#include "hgMaf.h"
#include "mafTrack.h"

static char const rcsid[] = "$Id: wigMafTrack.c,v 1.5 2004/03/04 00:31:11 kate Exp $";

struct wigMafItem
/* A maf track item -- 
 * a line of bases (base level) or pairwise density gradient (zoomed out). */
    {
    struct wigMafItem *next;
    char *name;		/* Common name */
    char *db;		/* Database */
    int ix;		/* Position in list. */
    int height;		/* Pixel height of item. */
    };

static void wigMafItemFree(struct wigMafItem **pEl)
/* Free up a wigMafItem. */
{
struct wigMafItem *el = *pEl;
if (el != NULL)
    {
    freeMem(el->name);
    freeMem(el->db);
    freez(pEl);
    }
}

void wigMafItemFreeList(struct wigMafItem **pList)
/* Free a list of dynamically allocated wigMafItem's */
{
struct wigMafItem *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    wigMafItemFree(&el);
    }
*pList = NULL;
}

struct mafAli *wigMafLoadInRegion(struct sqlConnection *conn, 
	char *table, char *chrom, int start, int end)
/* Load mafs from region */
{
    return mafLoadInRegion(conn, table, chrom, start, end);
}

static boolean dbPartOfName(char *name, char *retDb, int retDbSize)
/* Parse out just database part of name (up to but not including
 * first dot). If dot found, return entire name */
{
int len;
char *e = strchr(name, '.');
/* Put prefix up to dot into buf. */
len = (e == NULL ? strlen(name) : e - name);
if (len >= retDbSize)
     len = retDbSize-1;
memcpy(retDb, name, len);
retDb[len] = 0;
return TRUE;
}

static int wigHeight(struct track *wigTrack, enum trackVisibility vis)
/* Pad wiggle height by one (remove if Hiram adds buffer at end of wiggle) */
{
return wigTotalHeight(wigTrack, vis) + 1;
}

static struct wigMafItem *scoreItem(scoreHeight)
/* Make up (wiggle) item that will show the score */
{
struct wigMafItem *mi;

AllocVar(mi);
mi->name = cloneString("Conservation");
mi->height = scoreHeight;
return mi;
}

static struct wigMafItem *loadBaseByBaseItems(struct track *track)
/* Make up base-by-base track items. */
{
struct wigMafItem *miList = NULL, *mi;
struct mafAli *maf;
char buf[64];
char *otherOrganism;
char *myOrg = hOrganism(database);
tolowers(myOrg);

/* Make up item that will show inserts in this organism. */
AllocVar(mi);
mi->name = cloneString("Hidden Gaps");
mi->height = tl.fontHeight;
slAddHead(&miList, mi);

/* Make up item for this organism. */
AllocVar(mi);
mi->name = myOrg;
mi->height = tl.fontHeight;
mi->db = cloneString(database);
slAddHead(&miList, mi);


/* Make up items for other organisms by scanning through
 * all mafs and looking at database prefix to source. */
    {
    /* get species order from trackDb setting */
    char *speciesOrder = trackDbRequiredSetting(track->tdb, "speciesOrder");
    char *species[100];
    int speciesCt = chopLine(speciesOrder, species);
    struct hash *hash = newHash(8);	/* keyed by database. */
    struct hashEl *el;
    int i;

    hashAdd(hash, mi->name, mi);	/* Add in current organism. */
    for (maf = track->customPt; maf != NULL; maf = maf->next)
        {
	struct mafComp *mc;
	for (mc = maf->components; mc != NULL; mc = mc->next)
	    {
	    dbPartOfName(mc->src, buf, sizeof(buf));
	    if (hashLookup(hash, buf) == NULL)
	        {
		AllocVar(mi);
		mi->db = cloneString(buf);
                otherOrganism = hOrganism(mi->db);
                mi->name = 
                    (otherOrganism == NULL ? cloneString(buf) : otherOrganism);
                tolowers(mi->name);
		mi->height = tl.fontHeight;
		hashAdd(hash, mi->name, mi);
		}
	    }
	}
    /* build item list in species order */
    for (i = 0; i < speciesCt; i++)
        {
        if ((el = hashLookup(hash, species[i])) != NULL)
            slAddHead(&miList, (struct wigMafItem *)el->val);
        }
    hashFree(&hash);
    }
/* Add item for score wiggle after base alignment */
if (track->subtracks != NULL)
    {
    mi = scoreItem(wigHeight(track->subtracks, track->visibility));
    slAddHead(&miList, mi);
    }
slReverse(&miList);
return miList;
}

static int pairwiseWigHeight(struct track *wigTrack)
/* Return the height of a pairwise wiggle for this track */
{
    return max(wigHeight(wigTrack, tvFull)/2, tl.fontHeight);
}

static struct wigMafItem *loadPairwiseItems(struct track *track)
/* Make up items for modes where pairwise data are shown.
   First an item for the score wiggle, then a pairwise item
   for each "other species" in the multiple alignment.
   These may be density plots (pack) or wiggles (full). */
{
struct wigMafItem *miList = NULL, *mi;
struct mafAli *maf;
char buf[64];
char *otherOrganism;
struct track *wigTrack = track->subtracks;
int smallWigHeight = pairwiseWigHeight(wigTrack);

if (wigTrack != NULL)
    {
    mi = scoreItem(wigHeight(wigTrack, tvFull));
    slAddHead(&miList, mi);
    }
if (trackDbSetting(track->tdb, "pairwise") != NULL)

/* Make up items for other organisms by scanning through
 * all mafs and looking at database prefix to source. */
    {
    // TODO: share code with baseByBaseItems
    char *speciesOrder = trackDbRequiredSetting(track->tdb, "speciesOrder");
    char *species[100];
    int speciesCt = chopLine(speciesOrder, species);
    struct hash *hash = newHash(8);	/* keyed by database. */
    struct hashEl *el;
    int i;

    for (maf = track->customPt; maf != NULL; maf = maf->next)
        {
	struct mafComp *mc;
        boolean isMyOrg = TRUE;
	for (mc = maf->components; mc != NULL; mc = mc->next)
	    {
            if (isMyOrg) 
                {
                /* skip first maf component (this organism) */
                isMyOrg = FALSE;
                continue;
                }
	    dbPartOfName(mc->src, buf, sizeof(buf));
	    if (hashLookup(hash, buf) == NULL)
	        {
		AllocVar(mi);
		mi->db = cloneString(buf);
                otherOrganism = hOrganism(mi->db);
                mi->name = 
                    (otherOrganism == NULL ? cloneString(buf) : otherOrganism);
                tolowers(mi->name);
                if (track->visibility == tvFull)
                    mi->height = smallWigHeight;
                else
                    mi->height = tl.fontHeight;
		hashAdd(hash, mi->name, mi);
		}
	    }
	}
    /* build item list in species order */
    for (i = 0; i < speciesCt; i++)
        {
        if ((el = hashLookup(hash, species[i])) != NULL)
            slAddHead(&miList, (struct wigMafItem *)el->val);
        }
    hashFree(&hash);
    }
slReverse(&miList);
return miList;
}

static struct wigMafItem *loadWigMafItems(struct track *track,
                                                 boolean isBaseLevel)
/* Load up items */
{
struct mafAli *mafList = NULL; struct wigMafItem *miList = NULL;
struct sqlConnection *conn = hAllocConn();

/* Load up mafs and store in track so drawer doesn't have
 * to do it again. */
mafList = wigMafLoadInRegion(conn, track->mapName, chromName, winStart, winEnd);
track->customPt = mafList;

/* Make up tracks for display. */
if (isBaseLevel)
    {
    miList = loadBaseByBaseItems(track);
    }
else if (track->visibility == tvFull || track->visibility == tvPack)
    {
    miList = loadPairwiseItems(track);
    }
else if (track->visibility == tvSquish)
    {
    miList = scoreItem(wigHeight(track->subtracks, tvFull));
    }
else 
    {
    /* dense mode, zoomed out - show density plot, with track label */
    AllocVar(miList);
    miList->name = cloneString(track->shortLabel);
    miList->height = tl.fontHeight;
    }
hFreeConn(&conn);
return miList;
}

static void wigMafLoad(struct track *track)
/* Load up maf tracks.  What this will do depends on
 * the zoom level and the display density. */
{
struct wigMafItem *miList = NULL;
struct track *wigTrack = track->subtracks;

if (zoomedToBaseLevel)
    {
    miList = loadWigMafItems(track, zoomedToBaseLevel);
    }
else
    {
    /* zoomed out */
    miList = loadWigMafItems(track, FALSE);
    }
track->items = miList;

if (wigTrack != NULL) 
    // load wiggle subtrack items
    {
    /* update track visibility from parent track,
     * since hgTracks will update parent vis before loadItems */
    wigTrack->visibility = track->visibility;
    wigTrack->loadItems(wigTrack);
    }
}

static int wigMafTotalHeight(struct track *track, enum trackVisibility vis)
/* Return total height of maf track.  */
{
struct wigMafItem *mi;
int total = 0;
for (mi = track->items; mi != NULL; mi = mi->next)
    total += mi->height;
track->height =  total;
return track->height;
}

static int wigMafItemHeight(struct track *track, void *item)
/* Return total height of maf track.  */
{
struct wigMafItem *mi = item;
return mi->height;
}


static void wigMafFree(struct track *track)
/* Free up maf items. */
{
if (track->customPt != NULL)
    mafAliFreeList((struct mafAli **)&track->customPt);
if (track->items != NULL)
    wigMafItemFreeList((struct wigMafItem **)&track->items);
}

static char *wigMafItemName(struct track *track, void *item)
/* Return name of maf level track. */
{
struct wigMafItem *mi = item;
return mi->name;
}

static void processInserts(char *text, int textSize, 
	char *insertLine, int baseCount)
/* Make up insert line - it has a ' ' if there is no space
 * before char, and a '-' if there is.  */
{
int i, baseIx = 0;
char c;
for (i=0; i<textSize && baseIx < baseCount; ++i)
    {
    c = text[i];
    if (c == '-')
	{
	unsigned char b = insertLine[baseIx];
	if (b < 255)
	    insertLine[baseIx] = b+1;
	}
    else
        baseIx += 1;
    }
}

static void charifyInserts(char *insertLine, int size)
/* Convert insert line from counts to characters. */
{
int i;
char c;
for (i=0; i<size; ++i)
    {
    c = insertLine[i];
    if (c == 0)
       c = ' ';
    else if (c <= 9)
       c += '0';
    else
       c = '+';
    insertLine[i] = c;
    }
}

static void processOtherSeq(char *text, char *masterText, int textSize,
	char *outLine, int outSize)
/* Copy text to outLine, suppressing copy where there are dashes
 * in masterText.  This effectively projects the alignment onto
 * the master genome. */
{
int i, outIx = 0;
for (i=0; i<textSize && outIx < outSize;  ++i)
    {
    if (masterText[i] != '-')
        {
	outLine[outIx] = text[i];
	outIx += 1;
	}
    }
}

static void setIxMafAlign(int ix, int *ixMafAli, int count)
/* make an array of alignment indices, one per base */
{
    int i;
    for (i = 0; i < count; i++)
        {
        *ixMafAli++ = ix;
        }
}

static int getIxMafAli(int *ixMafAli, int position, int maxPos)
/* get alignment index for a base position */
{
    if (position > maxPos)
        {
        return 0;
        }
    return *(ixMafAli + position);
}

static void drawDenseMaf(struct mafAli *mafList,int height,
                            int seqStart, int seqEnd, 
                            struct vGfx *vg, int xOff, int yOff,
                            int width, MgFont *font, 
                            Color color, Color altColor)
/* display alignments in dense format (uses on-the-fly scoring) */
{
drawMafRegionDetails(mafList, height, seqStart, seqEnd, 
                             vg, xOff, yOff, width, font,
                             color, altColor, tvDense, FALSE);
}

static boolean wigMafDrawPairwise(struct track *track, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, MgFont *font,
        Color color, enum trackVisibility vis)
/* Draw pairwise tables for this multiple alignment, 
 *  if "pairwise" setting is on.  For full mode, display wiggles,
 *  for pack mode, display density plot of mafs.
 */
{
struct mafAli *mafList;
struct sqlConnection *conn;
boolean ret = FALSE;
char *suffix;
char table[64];
Color newColor;
struct track *wigTrack = track->subtracks;
int wigTrackHeight = pairwiseWigHeight(wigTrack);
struct wigMafItem *miList = track->items, *mi = miList;
int seqSize = seqEnd - seqStart;

if (miList == NULL)
    return FALSE;
conn = hAllocConn();

/* obtain suffix for pairwise wiggle tables */
suffix = trackDbSetting(track->tdb, "pairwise");
if (vis == tvFull)
    {
    if (suffix == NULL || wigTrack == NULL)
        return FALSE;
    /* swap colors for pairwise wiggles */
    newColor = wigTrack->ixColor;
    wigTrack->ixColor = wigTrack->ixAltColor;
    wigTrack->ixAltColor = newColor;
    }

/* display all loaded pairwise items */
for (mi = miList; mi != NULL; mi = mi->next)
    {
    if (vis == tvFull)
        {
        /* get wiggle table, containing percent identity of pairwise */
        safef(table, sizeof(table), "%s_%s", mi->name, suffix);
        if (!hTableExistsDb(database, table))
            continue;
        /* reuse the wigTrack for pairwise tables */
        wigTrack->mapName = table;
        wigTrack->loadItems(wigTrack);
        wigTrack->height = wigTrack->lineHeight = wigTrack->heightPer =
                                                            wigTrackHeight;
        wigTrack->drawItems(wigTrack, seqStart, seqEnd, vg, xOff, yOff,
                             width, font, color, tvFull);
        //mi->height = wigHeight(wigTrack, tvFull);
        mi->height = wigTrackHeight;
        ret = TRUE;
        }
    else 
        {
        /* pack */
        /* get maf table, containing pairwise alignments for this organism */
        /* NOTE: might want to use the "pairwise" trackDb setting for this */
        safef(table, sizeof(table), "%s_%s", mi->name, track->mapName);
        if (!hTableExistsDb(database, table))
            continue;
        mafList = wigMafLoadInRegion(conn, table, chromName, 
                                    seqStart, seqEnd);
        /* display pairwise alignments in this region in dense format */
        drawDenseMaf(mafList, mi->height, seqStart, seqEnd, 
                                 vg, xOff, yOff, width, font,
                                 color, track->ixAltColor);
        ret = TRUE;
        mafAliFreeList(&mafList);
        }
    yOff += mi->height + 1;
    }
hFreeConn(&conn);
return ret;
}

static int wigMafDrawBases(struct track *track, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw base-by-base view, return new Y offset. */
{
struct wigMafItem *miList = track->items, *mi;
struct mafAli *mafList, *maf, *sub;
int lineCount = slCount(miList);
char **lines = NULL, *selfLine, *insertLine;
int *ixMafAli;   /* per base alignment index */
int i, x = xOff, y = yOff;
struct dnaSeq *seq = NULL;
struct hash *miHash = newHash(9);
char dbChrom[64];

/* Allocate a line of characters for each item. */
AllocArray(lines, lineCount);
lines[0] = needMem(winBaseCount+1);
for (i=1; i<lineCount; ++i)
    {
    lines[i] = needMem(winBaseCount+1);
    memset(lines[i], ' ', winBaseCount);
    }
AllocArray(ixMafAli, winBaseCount);

/* Give nice names to first two. */
insertLine = lines[0];
selfLine = lines[1];

/* Load up self-line with DNA */
seq = hChromSeq(chromName, seqStart, seqEnd);
memcpy(selfLine, seq->dna, winBaseCount);
toUpperN(selfLine, winBaseCount);
freeDnaSeq(&seq);

/* Make hash of items keyed by database. */
i = 0;
for (mi = miList; mi != NULL; mi = mi->next)
    {
    mi->ix = i++;
    if (mi->db != NULL)
	hashAdd(miHash, mi->db, mi);
    }

/* Go through the mafs saving relevant info in lines. */
mafList = track->customPt;
safef(dbChrom, sizeof(dbChrom), "%s.%s", database, chromName);
i = 0;
for (maf = mafList; maf != NULL; maf = maf->next)
    {
    sub = mafSubset(maf, dbChrom, winStart, winEnd);
    if (sub != NULL)
        {
	struct mafComp *mc, *mcMaster;
	char db[64];
	int subStart,subEnd;
	int lineOffset, subSize;

        i++;
	mcMaster = mafFindComponent(sub, dbChrom);
	if (mcMaster->strand == '-')
	    mafFlipStrand(sub);
	subStart = mcMaster->start;
	subEnd = subStart + mcMaster->size;
	subSize = subEnd - subStart;
	lineOffset = subStart - seqStart;
	for (mc = sub->components; mc != NULL; mc = mc->next)
	    {
            dbPartOfName(mc->src, db, sizeof(db));
	    if (mc == mcMaster)
		{
		processInserts(mc->text, sub->textSize, 
			insertLine+lineOffset, subSize);
		}
	    else
	        {
		mi = hashMustFindVal(miHash, db);
		processOtherSeq(mc->text, mcMaster->text, sub->textSize, 
			lines[mi->ix] + lineOffset, subSize);
		}
	    }
        setIxMafAlign(i, ixMafAli + lineOffset, subSize);
	}
    mafAliFree(&sub);
    }
/* draw inserts line */
charifyInserts(insertLine, winBaseCount);
mi = miList;
x -= (width/winBaseCount)/2;
/* adjust x offset a bit */
x += 2;

spreadString(vg, x, y, width, mi->height-1, color,
                font, insertLine, winBaseCount);
y += mi->height;

/* draw base-level alignments */
for (mi = miList->next, i=1; mi != NULL, mi->db != NULL; mi = mi->next, ++i)
    {
    char *line = lines[i];
    /* TODO: leave lower case in to indicate masking ?
       * NOTE: want to make sure that all sequences are soft-masked
       * if we do this */
    toUpperN(line, winBaseCount);
    /* draw sequence letters for alignment */
    spreadAlignString(vg, x, y, width, mi->height-1, color,
                            font, line, selfLine, winBaseCount);
    y += mi->height;
    }

/* Clean up */
for (i=0; i<lineCount-1; ++i)
    freeMem(lines[i]);
freez(&lines);
hashFree(&miHash);
return y;
}

static int wigMafDrawScoreGraph(struct track *track, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
{
/* Draw routine for score graph, returns new Y offset */
struct track *wigTrack = track->subtracks;

/* wiggle is displayed full for all wigMaf visibilities, except dense */
enum trackVisibility wigVis = (vis == tvDense ? tvDense : tvFull);
int y = 0;
if (wigTrack != NULL)
    {
    wigTrack->ixColor = vgFindRgb(vg, &wigTrack->color);
    wigTrack->ixAltColor = vgFindRgb(vg, &wigTrack->altColor);
    wigTrack->drawItems(wigTrack, seqStart, seqEnd, vg, xOff, yOff,
                         width, font, color, wigVis);
    y = wigHeight(wigTrack, wigVis);
    }
return yOff + y;
}


static void wigMafDraw(struct track *track, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw routine for maf type tracks */
{
int y = yOff;
if (zoomedToBaseLevel)
    {
    y = wigMafDrawBases(track, seqStart, seqEnd, vg, xOff, y, width, font,
                                color, vis);
    wigMafDrawScoreGraph(track, seqStart, seqEnd, vg, xOff, y, width,
                                font, color, vis);
    }
else 
    {
    y = wigMafDrawScoreGraph(track, seqStart, seqEnd, vg, xOff, y, width,
                                font, color, vis);
    if (vis == tvFull || vis == tvPack)
        wigMafDrawPairwise(track, seqStart, seqEnd, vg, xOff, y, 
                                width, font, color, vis);
    }
mapBoxHc(seqStart, seqEnd, xOff, yOff, width, track->height, track->mapName, 
            track->mapName, NULL);
}

/*
TODO:  REMOVE OR USE
static void wigMafLeftLabels(struct track *track, int seqStart, int seqEnd,
                          struct vGfx *vg, int xOff, int yOff, 
                          int width, int height, boolean withCenterLabels, 
                          MgFont *font, Color color, enum trackVisibility vis)
/* Draw species labels for each line of alignment or pairwise display */
/*
{
int fontHeight = mgFontLineHeight(font);
int centerOffset = withCenterLabels ? fontHeight : 0;
struct wigMafItem *item;

switch (vis)
    {
    case tvHide:
    case tvPack:
    case tvSquish:
        break;
    case tvFull:
        yOff += centerOffset;
        for (item = track->items; item != NULL; item = item->next)
            {
            char *name = track->itemName(track, item);
            int itemHeight = track->itemHeight(track, item);
            vgTextRight(vg, xOff, yOff, width - 1,
                            track->itemHeight(track, item), color, font, name);
            yOff += itemHeight;
            }
        break;
    case tvDense:
        vgTextRight(vg, xOff, yOff, width - 1, height-centerOffset,
                            track->ixColor, font, track->shortLabel);
        break;
    }
    vgUnclip(vg);
}
*/

void wigMafMethods(struct track *track, struct trackDb *tdb,
                                        int wordCount, char *words[])
/* Make track for maf multiple alignment. */
{
char *wigType = needMem(64);
char *wigTable;
struct track *wigTrack;
int i;
char *savedType;

track->loadItems = wigMafLoad;
track->freeItems = wigMafFree;
track->drawItems = wigMafDraw;
track->itemName = wigMafItemName;
track->mapItemName = wigMafItemName;
track->totalHeight = wigMafTotalHeight;
track->itemHeight = wigMafItemHeight;
track->itemStart = tgItemNoStart;
track->itemEnd = tgItemNoEnd;
track->mapsSelf = TRUE;
track->canPack = TRUE;
//track->drawLeftLabels = wigMafLeftLabels;

if ((wigTable = trackDbSetting(tdb, "wiggle")) != NULL)
    if (hTableExists(wigTable))
        {
        //  manufacture and initialize wiggle subtrack
        /* CAUTION: this code is very interdependent with
           hgTracks.c:fillInFromType()
           Also, both the main track and subtrack share the same tdb */
        // restore "type" line, but change type to "wig"
        savedType = tdb->type;
        *wigType = 0;
        strcat(wigType, "type wig ");
        for (i = 1; i < wordCount; i++)
            {
            strcat(wigType, words[i]);
            strcat(wigType, " ");
            }
        strcat(wigType, "\n");
        tdb->type = wigType;
        wigTrack = trackFromTrackDb(tdb);
        tdb->type = savedType;

        // replace tablename with wiggle table from "wiggle" setting
        wigTrack->mapName = cloneString(wigTable);

        // setup wiggle methods in subtrack
        wigMethods(wigTrack, tdb, wordCount, words);

        wigTrack->drawLeftLabels = NULL;
        track->subtracks = wigTrack;
        track->subtracks->next = NULL;
        }
}

