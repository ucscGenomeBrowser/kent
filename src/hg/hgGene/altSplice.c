/* altSplice - altSplicing diagram. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "hdb.h"
#include "vGfx.h"
#include "portable.h"
#include "altGraphX.h"
#include "hgGene.h"

static int gpBedBasesShared(struct genePred *gp, struct bed *bed)
/* Return number of bases genePred and bed share. */
{
int intersect = 0;
int i, blockCount = bed->blockCount;
int s,e;
for (i=0; i<blockCount; ++i)
    {
    s = bed->chromStart + bed->chromStarts[i];
    e = s + bed->blockSizes[i];
    intersect += gpRangeIntersection(gp, s, e);
    }
return intersect;
}

char *altGraphId(struct sqlConnection *conn, struct genePred *gp)
/* Return altGraphId that overlaps most with genePred. */
{
int rowOffset;
struct sqlResult *sr;
char **row;
struct bed *bestBed = NULL;
int intersect, bestIntersect = 0;
char extra[64];
char *ret = NULL;

safef(extra, sizeof(extra), "strand='%s'", gp->strand);
sr = hRangeQuery(conn, "agxBed", gp->chrom, gp->txStart, gp->txEnd,
	extra, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct bed *bed = bedLoad12(row+rowOffset);
    intersect = gpBedBasesShared(gp, bed);
    if (intersect > bestIntersect)
	{
	bestIntersect = intersect;
	bestBed = bed;
	}
    else
        bedFree(&bed);
    }
if (bestBed != NULL)
    {
    ret = cloneString(bestBed->name);
    bedFree(&bestBed);
    }
return ret;
}

void makeGrayShades(struct vGfx *vg, int maxShade, Color shadesOfGray[])
/* Make eight shades of gray in display. */
{
int i;
for (i=0; i<=maxShade; ++i)
    {
    int level = 255 - (255*i/maxShade);
    if (level < 0) level = 0;
    shadesOfGray[i] = vgFindColorIx(vg, level, level, level);
    }
shadesOfGray[maxShade+1] = MG_RED;
}

char *altGraphXMakeImage(struct altGraphX *ag)
/* create a drawing of splicing pattern */
{
MgFont *font = mgSmallFont();
int trackTabWidth = 11;
int fontHeight = mgFontLineHeight(font);
struct spaceSaver *ssList = NULL;
struct hash *heightHash = NULL;
int rowCount = 0;
struct tempName gifTn;
int pixWidth = atoi(cartUsualString(cart, "pix", "620" ));
int pixHeight = 0;
struct vGfx *vg;
int lineHeight = 0;
double scale = 0;
Color shadesOfGray[9];
int maxShade = ArraySize(shadesOfGray)-1;

scale = (double)pixWidth/(ag->tEnd - ag->tStart);
lineHeight = 2 * fontHeight +1;
altGraphXLayout(ag, ag->tStart, pixWidth, pixWidth, scale, 100, &ssList, &heightHash, &rowCount);
hashFree(&heightHash);
pixHeight = rowCount * lineHeight;
makeTempName(&gifTn, "hgc", ".gif");
vg = vgOpenGif(pixWidth, pixHeight, gifTn.forCgi);
makeGrayShades(vg, maxShade, shadesOfGray);
vgSetClip(vg, 0, 0, pixWidth, pixHeight);
altGraphXDrawPack(ag, ssList, 0, 0, pixWidth, lineHeight, lineHeight-1,
		  ag->tStart, ag->tEnd, scale, ag->tEnd-ag->tStart, 
		  vg, font, MG_BLACK, shadesOfGray, "Dummy", NULL);
vgUnclip(vg);
vgClose(&vg);
printf(
       "<IMG SRC = \"%s\" BORDER=1 WIDTH=%d HEIGHT=%d><BR>\n",
       gifTn.forHtml, pixWidth, pixHeight);
return cloneString(gifTn.forHtml);
}

static boolean altSpliceExists(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Return TRUE if altSplice table exists and has something
 * on this one. */
{
if (!sqlTableExists(conn, "altGraphX") || !sqlTableExists(conn, "agxBed"))
    return FALSE;
section->items = altGraphId(conn, curGenePred);
return section->items != NULL;
}


static void altSplicePrint(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Print out altSplicing info. */
{
char *altId = section->items;
char query[256];
struct sqlResult *sr;
char **row;
struct altGraphX *ag;
char table[64];
boolean hasBin;

hFindSplitTable(curGeneChrom, "altGraphX", table, &hasBin);
safef(query, sizeof(query), "select * from %s where name='%s'", table, altId);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    ag = altGraphXLoad(row+hasBin);
    hPrintf("<TABLE><TR><TD BGCOLOR=#888888>\n");
    altGraphXMakeImage(ag);
    hPrintf("</TD></TR></TABLE>");
    }
sqlFreeResult(&sr);
}

struct section *altSpliceSection(struct sqlConnection *conn,
	struct hash *sectionRa)
/* Create altSplice section. */
{
struct section *section = sectionNew(sectionRa, "altSplice");
section->exists = altSpliceExists;
section->print = altSplicePrint;
return section;
}

