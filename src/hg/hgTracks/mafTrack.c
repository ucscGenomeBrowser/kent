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

static char const rcsid[] = "$Id: mafTrack.c,v 1.7 2003/05/09 16:20:25 kent Exp $";

struct mafItem
/* A maf track item. */
    {
    struct mafItem *next;
    char *name;		/* Common name */
    char *db;		/* Database */
    int ix;		/* Position in list. */
    int height;		/* Pixel height of item. */
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

static struct mafItem *mafBaseByBaseItems(struct track *tg, int scoreHeight)
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

/* Make up item that will show inserts in this organism. */
AllocVar(mi);
snprintf(buf, sizeof(buf), "%s Inserts", myOrg);
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
		mi->height = tl.fontHeight;
		slAddHead(&miList, mi);
		hashAdd(hash, mi->db, mi);
		}
	    }
	}
    hashFree(&hash);
    }

/* Make up item that will show the score as grayscale. */
AllocVar(mi);
mi->name = cloneString("Score");
mi->height = scoreHeight;
slAddHead(&miList, mi);

hFreeConn(&conn);
slReverse(&miList);
return miList;
}

static void mafLoad(struct track *tg)
/* Load up maf tracks.  What this will do depends on
 * the zoom level and the display density. */
{
struct mafItem *miList = NULL;
int scoreHeight = tl.fontHeight;

/* Create item list and set height depending
 * on display type. */
if (tg->visibility == tvFull)
    scoreHeight *= 4;
if (zoomedToBaseLevel)
    {
    miList = mafBaseByBaseItems(tg, scoreHeight);
    }
else
    {
    AllocVar(miList);
    miList->name = cloneString(tg->shortLabel);
    miList->height = scoreHeight;
    }
tg->items = miList;
}

static int mafTotalHeight(struct track *tg, 
	enum trackVisibility vis)
/* Return total height of maf track.  */
{
struct mafItem *mi;
int total = 0;
for (mi = tg->items; mi != NULL; mi = mi->next)
    total += mi->height;
tg->height =  total;
return tg->height;
}

static int mafItemHeight(struct track *tg, void *item)
/* Return total height of maf track.  */
{
struct mafItem *mi = item;
return mi->height;
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

static void getNormalizedScores(struct mafAli *maf, char *masterText,
	double *scores, int scoreCount)
/* Make an array of normalized scores, one per base. */
{
int textIx, outIx = 0;
double maxScore, minScore;
double scoreScale;
double score;
int shade;

mafColMinMaxScore(maf, &minScore, &maxScore);
scoreScale = 1.0/(maxScore - minScore);
for (textIx = 0; textIx < maf->textSize; ++textIx)
    {
    if (masterText[textIx] != '-')
        {
	score = (mafScoreRangeMultiz(maf, textIx, 1) - minScore)*scoreScale;
	if (score < 0.0) score = 0.0;
	if (score > 1.0) score = 1.0;
	scores[outIx] = score;
	++outIx;
	}
    }
}


void mafFillInPixelScores(struct mafAli *maf, struct mafComp *mcMaster,
	double *scores, int numScores)
/* Calculate one score per pixel normalized to be between 0.0 and 1.0. */
{
int i,j;
double score, minScore, maxScore, scoreScale;
int textSize = maf->textSize;
int masterSize = mcMaster->size;
char *masterText = mcMaster->text;

mafColMinMaxScore(maf, &minScore, &maxScore);
scoreScale = 1.0/(maxScore - minScore);
if (numScores >= masterSize)	 /* More pixels than bases */
    {
    int x1,x2;
    int masterPos = 0;
    for (i=0; i<textSize; ++i)
        {
	if (masterText[i] != '-')
	    {
	    score = mafScoreRangeMultiz(maf, i, 1);
	    score = (score - minScore) * scoreScale;
	    if (score < 0.0) score = 0.0;
	    if (score > 1.0) score = 1.0;
	    x1 = masterPos*numScores/masterSize;
	    x2 = (masterPos+1)*numScores/masterSize;
	    for (j=x1; j<x2; ++j)
	        scores[j] = score;
	    ++masterPos;
	    }
	}
    }
else	/* More bases than pixels. */
    {
    int b1=0,b2, deltaB, delta;
    int t1=0,t2, deltaT;
    for (i=0; i<numScores; ++i)
        {
	b2 = (i+1)*masterSize/numScores;
	deltaB = b2 - b1;
	for (t2 = t1; t2 < textSize; ++t2)
	    {
	    if (deltaB <= 0)
		break;
	    if (masterText[t2] != '-')
		deltaB -= 1;
	    }
	deltaT = t2 - t1;
	score = mafScoreRangeMultiz(maf, t1, deltaT)/(b2-b1);
	score = (score - minScore) * scoreScale;
	if (score < 0.0) score = 0.0;
	if (score > 1.0) score = 1.0;
	scores[i] = score;
	b1 = b2;
	t1 = t2;
	}
    }
}

static void mafDrawOverview(struct track *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw wiggle-plot based on overall maf scores rather than
 * computing them for sections.  For this routine we don't
 * need to actually load the mafs, it's sufficient to load
 * the scoredRefs. */
{
char **row;
unsigned int extFileId = 0;
int rowOffset;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = hRangeQuery(conn, tg->mapName, chromName, 
    seqStart, seqEnd, NULL, &rowOffset);
double scale = scaleForPixels(width);
int x1,x2,y,w;
int height1 = tg->heightPer-1;

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
	int shade = ref.score * maxShade;
	Color c = shadesOfGray[shade];
	vgBox(vg, x1 + xOff, yOff, 1, tg->heightPer, c);
	}
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}

static void mafDrawDetails(struct track *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw wiggle/density plot based on scoring things on the fly. 
 * The */
{
int seqSize = seqEnd - seqStart;
struct mafAli *mafList, *full, *sub = NULL, *maf = NULL;
struct mafComp *mcMaster, mc;
char dbChrom[64];
struct sqlConnection *conn = hAllocConn();
struct mafItem *mi = tg->items;
int height1 = mi->height-2;

safef(dbChrom, sizeof(dbChrom), "%s.%s", database, chromName);
mafList = mafLoadInRegion(conn, tg->mapName, chromName, seqStart, seqEnd);
for (full = mafList; full != NULL; full = full->next)
    {
    double scoreScale;
    int mafStartOff;
    int mafPixelStart, mafPixelEnd, mafPixelWidth;
    int i;
    double *pixelScores = NULL;
    if (mafNeedSubset(full, dbChrom, seqStart, seqEnd))
        sub = maf = mafSubset(full, dbChrom, seqStart, seqEnd);
    else
        maf = full;
    if (maf != NULL)
	{
	mcMaster = mafFindComponent(maf, dbChrom);
	if (mcMaster->strand == '-')
	    mafFlipStrand(maf);
	mafPixelStart = (mcMaster->start - seqStart) * width/winBaseCount;
	mafPixelEnd = (mcMaster->start + mcMaster->size - seqStart) 
	    * width/winBaseCount;
	mafPixelWidth = mafPixelEnd-mafPixelStart;
	if (mafPixelWidth < 1) mafPixelWidth = 1;
	AllocArray(pixelScores, mafPixelWidth);
	mafFillInPixelScores(maf, mcMaster, pixelScores, mafPixelWidth);
	for (i=0; i<mafPixelWidth; ++i)
	    {
	    if (vis == tvFull)
		{
		int y = pixelScores[i] * height1;
		vgBox(vg, i+mafPixelStart+xOff, yOff + height1 - y, 
		    1, y+1, color);
		}
	    else
	        {
		int shade = pixelScores[i] * maxShade;
		Color c = shadesOfGray[shade];
		vgBox(vg, i+mafPixelStart+xOff, yOff, 
		    1, mi->height-1, c);
		}
	    }
	freez(&pixelScores);
	}
    mafAliFree(&sub);
    }
mafAliFreeList(&mafList);
hFreeConn(&conn);
}

static void mafDrawGraphic(struct track *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw wiggle or density plot, not base-by-base. */
{
int seqSize = seqEnd - seqStart;
if (seqSize >= 500000)
    {
    mafDrawOverview(tg, seqStart, seqEnd, vg, 
    	xOff, yOff, width, font, color, vis);
    }
else
    {
    mafDrawDetails(tg, seqStart, seqEnd, vg, 
	xOff, yOff, width, font, color, vis);
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
double *scores;
double scoreScale;
int i, y = yOff;
struct dnaSeq *seq = NULL;
struct hash *miHash = newHash(9);
char dbChrom[64];


/* Allocate a line of characters for each item. */
AllocArray(lines, lineCount-1);
for (i=0; i<lineCount-1; ++i)
    {
    lines[i] = needMem(winBaseCount+1);
    memset(lines[i], ' ', winBaseCount);
    }
AllocArray(scores, winBaseCount);

/* Give nice names to first three. */
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
safef(dbChrom, sizeof(dbChrom), "%s.%s", database, chromName);
for (maf = mafList; maf != NULL; maf = maf->next)
    {
    sub = mafSubset(maf, dbChrom, winStart, winEnd);
    if (sub != NULL)
        {
	struct mafComp *mc, *mcMaster;
	char db[64];
	int subStart,subEnd;
	int lineOffset, subSize;
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
	getNormalizedScores(sub, mcMaster->text, scores + lineOffset, subSize);
	}
    mafAliFree(&sub);
    }

/* Draw lines with letters . */
for (mi = miList, i=0; mi->next != NULL; mi = mi->next, ++i)
    {
    char *line = lines[i];
    int x = xOff;
    if (line == insertLine)
        {
	int x1, x2;
	x -= (width/winBaseCount)/2;
	}
    spreadString(vg, x, y, width, mi->height-1, color, font, 
    	line, winBaseCount);
    y += mi->height;
    }

/* Draw score line. */
if (vis == tvDense)
    scoreScale = (maxShade);
else
    scoreScale = (mi->height-2);
for (i=0; i<winBaseCount; ++i)
    {
    int x1 = i * width/winBaseCount;
    int x2 = (i+1) * width/winBaseCount;
    if (vis == tvDense)
	{
	int shade = scores[i] * scoreScale;
	int color = shadesOfGray[shade];
	vgBox(vg, x1+xOff, y, x2-x1, mi->height-1, color);
	}
    else
        {
	int wiggleH = scores[i] * scoreScale;
	vgBox(vg, x1+xOff, y + mi->height-2 - wiggleH, 
	    x2-x1, wiggleH+1, color);
	}
    }
y += mi->height;

/* Clean up */
for (i=0; i<lineCount-1; ++i)
    freeMem(lines[i]);
freez(&lines);
freez(&scores);
hashFree(&miHash);
}


static void mafDraw(struct track *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw routine for mafAlign type tracks.  This will load
 * the items as well as drawing them. */
{
if (zoomedToBaseLevel)
    mafDrawBases(tg, seqStart, seqEnd, vg, xOff, yOff, width, font, color, vis);
else 
    mafDrawGraphic(tg, seqStart, seqEnd, vg, xOff, yOff, width, font, color, vis);
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
tg->itemHeight = mafItemHeight;
tg->itemStart = tgItemNoStart;
tg->itemEnd = tgItemNoEnd;
tg->mapsSelf = TRUE;
}

