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

static char const rcsid[] = "$Id: wigMafTrack.c,v 1.18 2004/04/02 20:21:38 hiram Exp $";

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
struct mafAli *mafList = NULL, *maf; 
char buf[64];
char *otherOrganism;
char *myOrg = hOrganism(database);
struct sqlConnection *conn = hAllocConn();
tolowers(myOrg);

/* load up mafs */
track->customPt = wigMafLoadInRegion(conn, track->mapName, 
                                        chromName, winStart, winEnd);
hFreeConn(&conn);

/* Make up item that will show inserts in this organism. */
AllocVar(mi);
safef(buf, sizeof(buf), "%s gaps", myOrg);
mi->name = cloneString(buf);
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
    enum trackVisibility wigVis = 
                (track->visibility == tvFull ? tvFull : tvDense);
    mi = scoreItem(wigTotalHeight(track->subtracks, wigVis));
    slAddHead(&miList, mi);
    }
slReverse(&miList);
return miList;
}

static int pairwiseWigHeight(struct track *wigTrack)
/* Return the height of a pairwise wiggle for this track
 * NOTE: set one pixel smaller than we actually want, to 
 * leave a border at the bottom of the wiggle.
 */
{
    return max(wigTotalHeight(wigTrack, tvFull)/3 - 1, tl.fontHeight);
}

static char *getWigTablename(char *species, char *suffix)
/* generate tablename for wiggle pairwise: "<species>_<table>_wig" */
{
char table[64];

safef(table, sizeof(table), "%s_%s_wig", species, suffix);
return cloneString(table);
}

static char *getMafTablename(char *species, char *suffix)
/* generate tablename for wiggle maf:  "<species>_<table>" */
{
char table[64];

safef(table, sizeof(table), "%s_%s", species, suffix);
return cloneString(table);
}

static struct wigMafItem *loadPairwiseItems(struct track *track)
/* Make up items for modes where pairwise data are shown.
   First an item for the score wiggle, then a pairwise item
   for each "other species" in the multiple alignment.
   These may be density plots (pack) or wiggles (full). */
{
struct wigMafItem *miList = NULL, *mi;
char *otherOrganism;
struct track *wigTrack = track->subtracks;
int smallWigHeight = pairwiseWigHeight(wigTrack);
char *suffix;

if (wigTrack != NULL)
    {
    mi = scoreItem(wigTotalHeight(wigTrack, tvFull));
    /* mark this as not a pairwise item */
    mi->ix = -1;
    slAddHead(&miList, mi);
    }
suffix = trackDbSetting(track->tdb, "pairwise");
if (suffix != NULL)
    /* make up items for other organisms by scanning through
     * all mafs and looking at database prefix to source. */
    {
    char *speciesOrder = trackDbRequiredSetting(track->tdb, "speciesOrder");
    char *species[200];
    int speciesCt = chopLine(speciesOrder, species);
    int i;

    /* build item list in species order */
    for (i = 0; i < speciesCt; i++)
        {
        char option[64];
        safef (option, sizeof(option), "%s.%s", track->mapName, species[i]);
        /* skip this species if UI checkbox was unchecked */
        if (!cartUsualBoolean(cart, option, TRUE))
            continue;
        AllocVar(mi);
        mi->name = species[i];
        if (track->visibility == tvFull)
            mi->height = smallWigHeight;
        else
            mi->height = tl.fontHeight;
        slAddHead(&miList, mi);
        }
    }
slReverse(&miList);
return miList;
}

static struct wigMafItem *loadWigMafItems(struct track *track,
                                                 boolean isBaseLevel)
/* Load up items */
{
struct wigMafItem *miList = NULL;

/* Load up mafs and store in track so drawer doesn't have
 * to do it again. */
/* Make up tracks for display. */
if (isBaseLevel)
    {
    miList = loadBaseByBaseItems(track);
    }
/* zoomed out */
else if (track->visibility == tvFull || track->visibility == tvPack)
    {
    miList = loadPairwiseItems(track);
    }
else if (track->visibility == tvSquish)
    {
    miList = scoreItem(wigTotalHeight(track->subtracks, tvFull));
    }
else 
    {
    /* dense mode, zoomed out - show density plot, with track label */
    AllocVar(miList);
    miList->name = cloneString(track->shortLabel);
    miList->height = tl.fontHeight;
    }
return miList;
}

static void wigMafLoad(struct track *track)
/* Load up maf tracks.  What this will do depends on
 * the zoom level and the display density. */
{
struct wigMafItem *miList = NULL;
struct track *wigTrack = track->subtracks;

miList = loadWigMafItems(track, zoomedToBaseLevel);
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

static void drawScoreOverview(char *tableName, int height,
                             int seqStart, int seqEnd, 
                            struct vGfx *vg, int xOff, int yOff,
                            int width, MgFont *font, 
                            Color color, Color altColor,
                            enum trackVisibility vis)
/* Draw density plot or graph for overall maf scores rather than computing
 * by sections, for speed.  Don't actually load the mafs -- just
 * the scored refs from the table.
 * TODO: reuse code in mafTrack.c 
 */
{
char **row;
int rowOffset;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = hRangeQuery(conn, tableName, chromName, 
                                        seqStart, seqEnd, NULL, &rowOffset);
double scale = scaleForPixels(width);
int x1,x2,y,w;
int height1 = height - 2;

while ((row = sqlNextRow(sr)) != NULL)
    {
    struct scoredRef ref;
    scoredRefStaticLoad(row + rowOffset, &ref);
    x1 = round((ref.chromStart - seqStart)*scale);
    x2 = round((ref.chromEnd - seqStart)*scale);
    w = x2-x1;
    if (w < 1) w = 1;
    if (vis == tvFull)
        {
        y = ref.score * height1;
        vgBox(vg, x1 + xOff, yOff + height1 - y, 1, y+1, color);
        }
    else
        {
        Color c;
        int shade = ref.score * maxShade;
        if ((shade < 0) || (shade >= maxShade))
            shade = 0;
        c = shadesOfGray[shade];
        vgBox(vg, x1 + xOff, yOff, 1, height-1, c);
        }
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}

static void drawMafScore(char *tableName, int height,
                            int seqStart, int seqEnd, 
                            struct vGfx *vg, int xOff, int yOff,
                            int width, MgFont *font, 
                            Color color, enum trackVisibility vis)
/* display alignments using on-the-fly scoring */
{
struct mafAli *mafList;
struct sqlConnection *conn;

if (seqEnd - seqStart > 1000000)
    drawScoreOverview(tableName, height, seqStart, seqEnd, vg, xOff, yOff,
                          width, font, color, color, vis);
else
    {
    /* load mafs */
    conn = hAllocConn();
    mafList = wigMafLoadInRegion(conn, tableName, chromName, 
                                    seqStart, seqEnd);
    drawMafRegionDetails(mafList, height, seqStart, seqEnd, 
                             vg, xOff, yOff, width, font,
                             color, color, vis, FALSE);
    hFreeConn(&conn);
    }
}

static boolean wigMafDrawPairwise(struct track *track, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, MgFont *font,
        Color color, enum trackVisibility vis)
/* Draw pairwise tables for this multiple alignment, 
 *  if "pairwise" setting is on.  For full mode, display wiggles,
 *  for pack mode, display density plot of mafs.
 */
{
boolean ret = FALSE;
char *suffix;
char *tableName;
Color newColor;
struct track *wigTrack = track->subtracks;
int pairwiseHeight = pairwiseWigHeight(wigTrack);
struct wigMafItem *miList = track->items, *mi = miList;
int seqSize = seqEnd - seqStart;

if (miList == NULL)
    return FALSE;

/* obtain suffix for pairwise wiggle tables */
suffix = trackDbSetting(track->tdb, "pairwise");
if (suffix == NULL)
    return FALSE;

/* we will display pairwise, either a graph (wiggle or on-the-fly),
   or density plot */
ret = TRUE;
if (vis == tvFull)
    {
    double minY = 50.0;
    double maxY = 100.0;

    if (wigTrack == NULL)
        return FALSE;
    /* swap colors for pairwise wiggles */
    newColor = wigTrack->ixColor;
    wigTrack->ixColor = wigTrack->ixAltColor;
    wigTrack->ixAltColor = newColor;
    wigSetCart(wigTrack, MIN_Y, (void *)&minY);
    wigSetCart(wigTrack, MAX_Y, (void *)&maxY);
    }

/* display pairwise items */
for (mi = miList; mi != NULL; mi = mi->next)
    {
    if (mi->ix < 0)
        /* ignore item for the score */
        continue;
    if (vis == tvFull)
        {
        /* get wiggle table, of pairwise 
           for example, percent identity */
        tableName = getWigTablename(mi->name, suffix);
        if (hTableExists(tableName))
            {
            /* reuse the wigTrack for pairwise tables */
            wigTrack->mapName = tableName;
            wigTrack->loadItems(wigTrack);
            wigTrack->height = wigTrack->lineHeight = wigTrack->heightPer =
                                                    pairwiseHeight - 1;
            /* clip, but leave 1 pixel border */
            vgSetClip(vg, xOff, yOff, width, wigTrack->height);
            wigTrack->drawItems(wigTrack, seqStart, seqEnd, vg, xOff, yOff,
                             width, font, color, tvFull);
            vgUnclip(vg);
            }
        else
            {
            /* no wiggle table for this -- compute a graph on-the-fly 
               from mafs */
            vgSetClip(vg, xOff, yOff, width, mi->height);
            tableName = getMafTablename(mi->name, suffix);
            if (hTableExists(tableName))
                drawMafScore(tableName, mi->height, seqStart, seqEnd, 
                                 vg, xOff, yOff, width, font,
                                 track->ixAltColor, tvFull);
            vgUnclip(vg);
            }
        /* need to add extra space between wiggles (for now) */
        //mi->height = wigTotalHeight(wigTrack, tvFull);
        mi->height = pairwiseHeight;
        }
    else 
        {
        /* pack */
        /* get maf table, containing pairwise alignments for this organism */
        /* display pairwise alignments in this region in dense format */
        vgSetClip(vg, xOff, yOff, width, mi->height);
        tableName = getMafTablename(mi->name, suffix);
        if (hTableExists(tableName))
            drawMafScore(tableName, mi->height, seqStart, seqEnd, 
                                 vg, xOff, yOff, width, font,
                                 color, tvDense);
        vgUnclip(vg);
        }
    yOff += mi->height;
    }
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
        MgFont *font, Color color, enum trackVisibility vis, boolean zoomedToBaseLevel)
{
/* Draw routine for score graph, returns new Y offset */
struct track *wigTrack = track->subtracks;
enum trackVisibility wigVis;

if (zoomedToBaseLevel)
    /* wiggle is displayed dense for all visibilities except full and  pack */
    wigVis = (vis == tvFull || vis == tvPack ? tvFull : tvDense);
else
    /* wiggle is displayed full for all visibilities except dense */
    wigVis = (vis == tvDense ? tvDense : tvFull);
if (wigTrack != NULL)
    {
    wigTrack->ixColor = vgFindRgb(vg, &wigTrack->color);
    wigTrack->ixAltColor = vgFindRgb(vg, &wigTrack->altColor);
    if (zoomedToBaseLevel && (vis == tvSquish || vis == tvPack))
        {
        /* display squished wiggle by reducing wigTrack height */
        wigTrack->height = wigTrack->lineHeight = wigTrack->heightPer =
                                                            tl.fontHeight - 1;
        vgSetClip(vg, xOff, yOff, width, wigTrack->height - 1);
        }
    else
    vgSetClip(vg, xOff, yOff, width, wigTotalHeight(wigTrack, wigVis) - 1);
    wigTrack->drawItems(wigTrack, seqStart, seqEnd, vg, xOff, yOff,
                         width, font, color, wigVis);
    vgUnclip(vg);
    yOff += wigTotalHeight(wigTrack, wigVis);
    }
return yOff;
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
                                font, color, vis, zoomedToBaseLevel);
    }
else 
    {
    y = wigMafDrawScoreGraph(track, seqStart, seqEnd, vg, xOff, y, width,
                                font, color, vis, zoomedToBaseLevel);
    if (vis == tvFull || vis == tvPack)
        wigMafDrawPairwise(track, seqStart, seqEnd, vg, xOff, y, 
                                width, font, color, vis);
    }
mapBoxHc(seqStart, seqEnd, xOff, yOff, width, track->height, track->mapName, 
            track->mapName, NULL);
}

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
//track->canPack = TRUE;
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

