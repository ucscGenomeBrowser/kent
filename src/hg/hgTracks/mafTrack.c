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

struct mafAli *mafLoadInRegion(struct sqlConnection *conn, char *table,
	char *chrom, int start, int end)
/* Return list of alignments in region. */
{
char **row;
unsigned int extFileId = 0;
struct mafAli *maf, *mafList = NULL;
struct mafFile *mf = NULL;
int rowOffset;
struct sqlResult *sr = hRangeQuery(conn, table, chrom, 
    start, end, NULL, &rowOffset);

while ((row = sqlNextRow(sr)) != NULL)
    {
    struct scoredRef ref;
    scoredRefStaticLoad(row + rowOffset, &ref);
    if (ref.extFile != extFileId)
	{
	char *path = hExtFileName("extFile", ref.extFile);
	mafFileFree(&mf);
	mf = mafOpen(path);
	extFileId = ref.extFile;
	}
    lineFileSeek(mf->lf, ref.offset, SEEK_SET);
    maf = mafNext(mf);
    if (maf == NULL)
        internalErr();
    slAddHead(&mafList, maf);
    }
sqlFreeResult(&sr);
mafFileFree(&mf);
slReverse(&mafList);
return mafList;
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
	    char *e = strchr(mc->src, '.');
	    /* Put prefix up to dot into buf. */
	    if (e == NULL)
	    	strncpy(buf, mc->src, sizeof(buf));
	    else
	        {
		int len = e - mc->src;
		if (len >= sizeof(buf))
		     len = sizeof(buf)-1;
		memcpy(buf, mc->src, len);
		buf[len] = 0;
		}
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

static void mafDrawBases(struct track *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw base-by-base view. */
{
struct mafItem *miList = tg->items, *mi;
struct mafAli *mafList = tg->customPt, *maf;
int lineCount = slCount(miList);
char **lines = NULL, *scoreLine, *selfLine, *insertLine;
int i, y = yOff;
struct dnaSeq *seq = NULL;
extern int winBaseCount;  /* Number of bases in window. */

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
memset(insertLine, '^', winBaseCount);

/* Draw each line. */
for (i=0; i<lineCount; ++i)
    {
    spreadString(vg, xOff, y, width, tg->heightPer, color, font, 
    	lines[i], winBaseCount);
    y += tg->lineHeight;
    }
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

