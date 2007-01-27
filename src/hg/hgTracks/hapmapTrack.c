/* hapmapTrack - Handle HapMap track. */
/* Modeled after rmskTrack.c */
/* Query database during drawing rather than loading */

#include "common.h"
#include "hash.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"

#include "hapmapAlleles.h"


struct popInfo
/* Simple content for items. */
    {
    struct popInfo *next;
    char *populationName;
    };

static char *populationNames[] =  {
    "CEPH", "Chinese", "Japanese", "Yoruban", 
};

static struct popInfo *makePopInfo()
{
struct popInfo *pi, *piList = NULL;
int i;
int numPops = ArraySize(populationNames);
for (i=0; i<numPops; ++i)
    {
    AllocVar(pi);
    pi->populationName = populationNames[i];
    slAddHead(&piList, pi);
    }
slReverse(&piList);
return piList;
}

static void hapmapLoad(struct track *tg)
{
tg->items = makePopInfo();
}

static void hapmapFree(struct track *tg)
/* Free up hapmap items. */
{
slFreeList(&tg->items);
}

static char *populationName(struct track *tg, void *item)
/* Return name of population. */
{
struct popInfo *pi = item;
return pi->populationName;
}

static void hapmapDraw(struct track *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
{
int baseWidth = seqEnd - seqStart;
int yCEU = yOff;
int yCHB = yCEU + tg->lineHeight;
int yJPT = yCHB + tg->lineHeight;
int yYRI = yJPT + tg->lineHeight;
int heightPer = tg->heightPer;
int x1,x2,w;
boolean isFull = (vis == tvFull);
Color col;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;
int rowOffset;

if (isFull)
    {
    /* Use gray scale for allele frequency. */
    struct hash *hash = newHash(6);
    struct hapmapAlleles hapmapSnp;
    int grayLevelCEU;
    int grayLevelCHB;
    int grayLevelJPT;
    int grayLevelYRI;
    char statusLine[128];

    sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, NULL,
		     &rowOffset);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	hapmapAllelesStaticLoad(row+rowOffset, &hapmapSnp);
	// CEU
	if (hapmapSnp.allele1CountCEU > 0 || hapmapSnp.allele2CountCEU > 0) 
	    {
	    grayLevelCEU = grayInRange(hapmapSnp.allele1CountCEU, 0, 120);
	    col = shadesOfGray[grayLevelCEU];
	    x1 = roundingScale(hapmapSnp.chromStart-winStart, width, baseWidth)+xOff;
	    x2 = roundingScale(hapmapSnp.chromEnd-winStart, width, baseWidth)+xOff;
	    w = x2-x1;
	    if (w <= 0)
	        w = 1;
	    vgBox(vg, x1, yCEU, w, heightPer, col);
	    if (baseWidth <= 100000)
	        {
		if (hapmapSnp.allele1CountCEU >= hapmapSnp.allele2CountCEU)
	            sprintf(statusLine, "%s %s", hapmapSnp.name, hapmapSnp.allele1);
		else
	            sprintf(statusLine, "%s %s", hapmapSnp.name, hapmapSnp.allele2);
	        mapBoxHc(hapmapSnp.chromStart, hapmapSnp.chromEnd, x1, yCEU, w, heightPer, tg->mapName,
	    	    hapmapSnp.name, statusLine);
	        }
	    }
	// CHB
	if (hapmapSnp.allele1CountCHB > 0 || hapmapSnp.allele2CountCHB > 0) 
	    {
	    grayLevelCHB = grayInRange(hapmapSnp.allele1CountCHB, 0, 90);
	    col = shadesOfGray[grayLevelCHB];
	    x1 = roundingScale(hapmapSnp.chromStart-winStart, width, baseWidth)+xOff;
	    x2 = roundingScale(hapmapSnp.chromEnd-winStart, width, baseWidth)+xOff;
	    w = x2-x1;
	    if (w <= 0)
	        w = 1;
	    vgBox(vg, x1, yCHB, w, heightPer, col);
	    if (baseWidth <= 100000)
	        {
		if (hapmapSnp.allele1CountCHB >= hapmapSnp.allele2CountCHB)
	            sprintf(statusLine, "%s %s", hapmapSnp.name, hapmapSnp.allele1);
		else
	            sprintf(statusLine, "%s %s", hapmapSnp.name, hapmapSnp.allele2);
	        mapBoxHc(hapmapSnp.chromStart, hapmapSnp.chromEnd, x1, yCHB, w, heightPer, tg->mapName,
	    	    hapmapSnp.name, statusLine);
	        }
	    }
	// JPT
	if (hapmapSnp.allele1CountJPT > 0 || hapmapSnp.allele2CountJPT > 0) 
	    {
	    grayLevelJPT = grayInRange(hapmapSnp.allele1CountJPT, 0, 90);
	    col = shadesOfGray[grayLevelJPT];
	    x1 = roundingScale(hapmapSnp.chromStart-winStart, width, baseWidth)+xOff;
	    x2 = roundingScale(hapmapSnp.chromEnd-winStart, width, baseWidth)+xOff;
	    w = x2-x1;
	    if (w <= 0)
	        w = 1;
	    vgBox(vg, x1, yJPT, w, heightPer, col);
	    if (baseWidth <= 100000)
	        {
		if (hapmapSnp.allele1CountJPT >= hapmapSnp.allele2CountJPT)
	            sprintf(statusLine, "%s %s", hapmapSnp.name, hapmapSnp.allele1);
		else
	            sprintf(statusLine, "%s %s", hapmapSnp.name, hapmapSnp.allele2);
	        mapBoxHc(hapmapSnp.chromStart, hapmapSnp.chromEnd, x1, yJPT, w, heightPer, tg->mapName,
	    	    hapmapSnp.name, statusLine);
	        }
	    }
	// YRI
	if (hapmapSnp.allele1CountYRI > 0 || hapmapSnp.allele2CountYRI > 0) 
	    {
	    grayLevelYRI = grayInRange(hapmapSnp.allele1CountYRI, 0, 120);
	    col = shadesOfGray[grayLevelYRI];
	    x1 = roundingScale(hapmapSnp.chromStart-winStart, width, baseWidth)+xOff;
	    x2 = roundingScale(hapmapSnp.chromEnd-winStart, width, baseWidth)+xOff;
	    w = x2-x1;
	    if (w <= 0)
	        w = 1;
	    vgBox(vg, x1, yYRI, w, heightPer, col);
	    if (baseWidth <= 100000)
	        {
		if (hapmapSnp.allele1CountYRI >= hapmapSnp.allele2CountYRI)
	            sprintf(statusLine, "%s %s", hapmapSnp.name, hapmapSnp.allele1);
		else
	            sprintf(statusLine, "%s %s", hapmapSnp.name, hapmapSnp.allele2);
	        mapBoxHc(hapmapSnp.chromStart, hapmapSnp.chromEnd, x1, yYRI, w, heightPer, tg->mapName,
	    	    hapmapSnp.name, statusLine);
	        }
	    }
	}
    freeHash(&hash);
    }
else
    {
    char table[64];
    boolean hasBin;
    struct dyString *query = newDyString(1024);
    if (hFindSplitTable(chromName, tg->mapName, table, &hasBin))
        {
	dyStringPrintf(query, "select chromStart,chromEnd from %s where ", table);
	if (hasBin)
	    hAddBinToQuery(winStart, winEnd, query);
	dyStringPrintf(query, "chromStart<%u and chromEnd>%u and chrom = '%s' ", winEnd, winStart, chromName);
	sr = sqlGetResult(conn, query->string);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    int start = sqlUnsigned(row[0]);
	    int end = sqlUnsigned(row[1]);
	    x1 = roundingScale(start-winStart, width, baseWidth)+xOff;
	    x2 = roundingScale(end-winStart, width, baseWidth)+xOff;
	    w = x2-x1;
	    if (w <= 0)
		w = 1;
	    vgBox(vg, x1, yOff, w, heightPer, MG_BLACK);
	    }
	}
    dyStringFree(&query);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void hapmapMethods(struct track *tg)
/* Make hapmap track.  */
{
tg->loadItems = hapmapLoad;
tg->freeItems = hapmapFree;
tg->drawItems = hapmapDraw;
tg->colorShades = shadesOfGray;
tg->itemName = populationName;
tg->mapItemName = populationName;
tg->totalHeight = tgFixedTotalHeightNoOverflow;
tg->itemHeight = tgFixedItemHeight;
tg->itemStart = tgItemNoStart;
tg->itemEnd = tgItemNoEnd;
}

