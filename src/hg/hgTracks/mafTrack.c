/* mafTrack - stuff to handle loading and display of
 * maf type tracks in browser. Mafs are multiple alignments. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"
#include "maf.h"
#include "scoredRef.h"

enum mafState {
/* Our three display modes */
    mafShort, 	/* Grayscale line. */
    mafTall, 	/* Wiggle plot. */
    mafBases,   /* Base-by-base view. */
    };
	
struct mafItem
/* A maf track item. */
    {
    struct mafItem *next;
    char *name;		/* Common name */
    char *db;		/* Database */
    int ix;		/* Position in list. */
    };

static void mafItemFree(struct mafItem **pEl)
/* Free up a mafItem. */
{
struct mafItem *el = *pEl;
if (el != NULL)
    {
    freeMem(el->name);
    freeMem(el->db);
    freez(pEl);
    }
}

void mafItemFreeList(struct mafItem **pList)
/* Free a list of dynamically allocated mafItem's */
{
struct mafItem *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    mafItemFree(&el);
    }
*pList = NULL;
}

static void dbPartOfName(char *name, char *retDb, int retDbSize)
/* Parse out just database part of name (up to but not including
 * first dot). */
{
char *e = strchr(name, '.');
/* Put prefix up to dot into buf. */
if (e == NULL)
    errAbort("Expecting db.chrom, got %s in maf", name);
else
    {
    int len = e - name;
    if (len >= retDbSize)
	 len = retDbSize-1;
    memcpy(retDb, name, len);
    retDb[len] = 0;
    }
}

static struct mafItem *mafBaseByBaseItems(struct track *tg)
/* Load up what we need for base-by-base view. */
{
struct mafItem *miList = NULL, *mi;
struct mafAli *mafList = NULL, *maf;
struct sqlConnection *conn = hAllocConn();
char *myOrg = hOrganism(database);
char buf[64];

/* Load up mafs and store in track so drawer doesn't have
 * to do it again. */
mafList = mafLoadInRegion(conn, tg->mapName, chromName, winStart, winEnd);
tg->customPt = mafList;

/* Make up item that will show the score as grayscale. */
AllocVar(mi);
mi->name = cloneString("Score");
slAddHead(&miList, mi);

/* Make up item that will show inserts in this organism. */
AllocVar(mi);
snprintf(buf, sizeof(buf), "%s Inserts", myOrg);
mi->name = cloneString(buf);
slAddHead(&miList, mi);

/* Make up item for this organism. */
AllocVar(mi);
mi->name = myOrg;
mi->db = cloneString(database);
slAddHead(&miList, mi);

/* Make up items for other organisms by scanning through
 * all mafs and looking at database prefix to source. */
    {
    struct hash *hash = newHash(8);	/* keyed by database. */
    hashAdd(hash, mi->db, mi);		/* Add in current organism. */
    for (maf = mafList; maf != NULL; maf = maf->next)
        {
	struct mafComp *mc;
	for (mc = maf->components; mc != NULL; mc = mc->next)
	    {
	    dbPartOfName(mc->src, buf, sizeof(buf));
	    if (hashLookup(hash, buf) == NULL)
	        {
		AllocVar(mi);
		mi->db = cloneString(buf);
		mi->name = hOrganism(mi->db);
		slAddHead(&miList, mi);
		hashAdd(hash, mi->db, mi);
		}
	    }
	}
    hashFree(&hash);
    }
hFreeConn(&conn);
slReverse(&miList);
return miList;
}

static void mafLoad(struct track *tg)
/* Load up maf tracks.  What this will do depends on
 * the zoom level and the display density. */
{
enum mafState display;
struct mafItem *miList = NULL;

/* First figure out which state we are going to draw in. 
 * Save this in customInt. */
if (tg->visibility == tvDense)
    display = mafShort;
else if (zoomedToBaseLevel)
    display = mafBases;
else
    display = mafTall;
tg->customInt = display;

/* Create item list and set height depending
 * on display type. */
tg->heightPer = tl.fontHeight;
if (display == mafBases)
    {
    miList = mafBaseByBaseItems(tg);
    }
else
    {
    AllocVar(miList);
    miList->name = tg->shortLabel;
    if (display == mafTall)
        tg->heightPer *= 4;
    }
tg->lineHeight = tg->heightPer+1;
tg->items = miList;
}

static int mafTotalHeight(struct track *tg, 
	enum trackVisibility vis)
/* Return total height of maf track.  */
{
tg->height =  slCount(tg->items) * tg->lineHeight;
return tg->height;
}

static void mafFree(struct track *tg)
/* Free up mafGroup items. */
{
mafAliFreeList((struct mafAli **)&tg->customPt);
mafItemFreeList((struct mafItem **)&tg->items);
}

static char *mafName(struct track *tg, void *item)
/* Return name of maf level track. */
{
struct mafItem *mi = item;
return mi->name;
}

static void processInserts(char *text, int textSize, 
	char *insertLine, int baseCount)
/* Make up insert line - it has a ' ' if there is no space
 * before char, and a '-' if there is.  */
{
int i, baseIx = 0;
char c;
memset(insertLine, ' ', baseCount);
for (i=0; i<textSize && baseIx < baseCount; ++i)
    {
    c = text[i];
    if (c == '-')
        insertLine[baseIx] = '-';
    else
        baseIx += 1;
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

static void processScore(struct mafAli *maf, char *masterText,
	char *outLine, int outSize)
/* Make an array of colors to output corresponding to score. */
{
int textIx, outIx = 0;
double maxScore = mafScoreMultizMaxCol(slCount(maf->components));
double minScore = -maxScore;  /* Just an approximation. */
double scoreScale = maxShade/(maxScore - minScore);
double score;
int shade;

uglyh("maxScore %f, scoreScale %f", maxScore, scoreScale);
memset(outLine, MG_WHITE, outSize);
for (textIx = 0; textIx < maf->textSize; ++textIx)
    {
    if (masterText[textIx] != '-')
        {
	score = mafScoreRangeMultiz(maf, textIx, 1);
	shade = (score - minScore) * scoreScale;
	if (shade < 0)
	    shade = 0;
	outLine[outIx] = shadesOfGray[shade];
	++outIx;
	uglyh("score %f, shade %d, colorIx %d", score, shade, shadesOfGray[shade]);
	}
    }
}

static void mafDrawBases(struct track *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw base-by-base view. */
{
struct mafItem *miList = tg->items, *mi;
struct mafAli *mafList = tg->customPt, *maf, *sub;
int lineCount = slCount(miList);
char **lines = NULL, *scoreLine, *selfLine, *insertLine;
int i, y = yOff;
struct dnaSeq *seq = NULL;
struct hash *miHash = newHash(9);
char dbChrom[64];

/* Allocate a line of characters for each item. */
AllocArray(lines, lineCount);
for (i=0; i<lineCount; ++i)
    {
    lines[i] = needMem(winBaseCount+1);
    memset(lines[i], '?', winBaseCount);
    }

/* Give nice names to first three. */
scoreLine = lines[0];
insertLine = lines[1];
selfLine = lines[2];

/* Load up self-line with DNA */
seq = hChromSeq(chromName, seqStart, seqEnd);
memcpy(selfLine, seq->dna, winBaseCount);
toUpperN(selfLine, winBaseCount);
freeDnaSeq(&seq);

/* Set insert line and score line to defaults. */
memset(scoreLine, '0', winBaseCount);

/* Make hash of items keyed by database. */
i = 0;
for (mi = miList; mi != NULL; mi = mi->next)
    {
    mi->ix = i++;
    if (mi->db != NULL)
	hashAdd(miHash, mi->db, mi);
    }

/* Go through the mafs saving relevant info in lines. */
safef(dbChrom, sizeof(dbChrom), "%s.%s", database, chromName);
for (maf = mafList; maf != NULL; maf = maf->next)
    {
    sub = mafSubset(maf, dbChrom, winStart, winEnd);
    if (sub != NULL)
        {
	struct mafComp *mc, *mcMaster;
	char db[64];
	mcMaster = mafFindComponent(sub, dbChrom);
	for (mc = sub->components; mc != NULL; mc = mc->next)
	    {
	    dbPartOfName(mc->src, db, sizeof(db));
	    if (mc == mcMaster)
		{
		processInserts(mc->text, sub->textSize, 
			insertLine, winBaseCount);
		}
	    else
	        {
		mi = hashMustFindVal(miHash, db);
		processOtherSeq(mc->text, mcMaster->text, sub->textSize, 
			lines[mi->ix], winBaseCount);
		}
	    }
	processScore(sub, mcMaster->text, scoreLine, winBaseCount);
	}
    mafAliFree(&sub);
    }

/* Draw score line. */
for (i=0; i<winBaseCount; ++i)
    {
    int x1 = i * width/winBaseCount;
    int x2 = (i+1) * width/winBaseCount;
    vgBox(vg, x1+xOff, y, x2-x1, tg->heightPer, scoreLine[i]);
    }
y += tg->lineHeight;

/* Draw other other lines. */
for (i=1; i<lineCount; ++i)
    {
    char *line = lines[i];
    int x = xOff;
    if (line == insertLine)
        {
	int x1, x2;
	x -= (width/winBaseCount)/2;
	}
    spreadString(vg, x, y, width, tg->heightPer, color, font, 
    	line, winBaseCount);
    y += tg->lineHeight;
    }
hashFree(&miHash);
}

static void mafDraw(struct track *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw routine for mafAlign type tracks.  This will load
 * the items as well as drawing them. */
{
enum mafState display = tg->customInt;
if (display == mafBases)
    mafDrawBases(tg, seqStart, seqEnd, vg, xOff, yOff, width, font, color, vis);
mapBoxHc(seqStart, seqEnd, xOff, yOff, width, tg->height, tg->mapName, 
    tg->mapName, NULL);
}

void mafMethods(struct track *tg)
/* Make track group for maf multiple alignment. */
{
tg->loadItems = mafLoad;
tg->freeItems = mafFree;
tg->drawItems = mafDraw;
tg->itemName = mafName;
tg->mapItemName = mafName;
tg->totalHeight = mafTotalHeight;
tg->itemHeight = tgFixedItemHeight;
tg->itemStart = tgItemNoStart;
tg->itemEnd = tgItemNoEnd;
tg->mapsSelf = TRUE;
}

