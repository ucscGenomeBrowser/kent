/* altSplice - altSplicing diagram. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "hdb.h"
#include "hvGfx.h"
#include "portable.h"
#include "altGraphX.h"
#include "hgGene.h"

static char const rcsid[] = "$Id: altSplice.c,v 1.10.24.1 2008/07/31 02:24:02 markd Exp $";

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

void makeGrayShades(struct hvGfx *hvg, int maxShade, Color shadesOfGray[])
/* Make eight shades of gray in display. */
{
int i;
for (i=0; i<=maxShade; ++i)
    {
    int level = 255 - (255*i/maxShade);
    if (level < 0) level = 0;
    shadesOfGray[i] = hvGfxFindColorIx(hvg, level, level, level);
    }
shadesOfGray[maxShade+1] = MG_RED;
}

char *altGraphXMakeImage(struct altGraphX *ag)
/* create a drawing of splicing pattern */
{
MgFont *font = mgSmallFont();
int fontHeight = mgFontLineHeight(font);
struct spaceSaver *ssList = NULL;
struct hash *heightHash = NULL;
int rowCount = 0;
struct tempName gifTn;
int pixWidth = atoi(cartUsualString(cart, "pix", "620" ));
int pixHeight = 0;
struct hvGfx *hvg;
int lineHeight = 0;
double scale = 0;
Color shadesOfGray[9];
int maxShade = ArraySize(shadesOfGray)-1;

scale = (double)pixWidth/(ag->tEnd - ag->tStart);
lineHeight = 2 * fontHeight +1;
altGraphXLayout(ag, ag->tStart, ag->tEnd, scale, 100, &ssList, &heightHash, &rowCount);
hashFree(&heightHash);
pixHeight = rowCount * lineHeight;
makeTempName(&gifTn, "hgc", ".gif");
hvg = hvGfxOpenGif(pixWidth, pixHeight, gifTn.forCgi);
makeGrayShades(hvg, maxShade, shadesOfGray);
hvGfxSetClip(hvg, 0, 0, pixWidth, pixHeight);
altGraphXDrawPack(ag, ssList, hvg, 0, 0, pixWidth, lineHeight, lineHeight-1,
		  ag->tStart, ag->tEnd, scale, 
		  font, MG_BLACK, shadesOfGray, "Dummy", NULL);
hvGfxUnclip(hvg);
hvGfxClose(&hvg);
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
char *mark = NULL;
if (!sqlTableExists(conn, "altGraphX") || !sqlTableExists(conn, "agxBed"))
    return FALSE;
section->items = altGraphId(conn, curGenePred);
/* each graph can have different connected components, if there
   is a component take the prefix for matching back to the graph.
   i.e. cut of an '-1' or '-2' */
if (section->items != NULL)
    if((mark = strrchr((char *)section->items, '-')) != NULL) 
        *mark = '\0';
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

hFindSplitTable(sqlGetDatabase(conn), curGeneChrom, "altGraphX", table, &hasBin);
safef(query, sizeof(query), "select * from %s where name='%s'", table, altId);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    ag = altGraphXLoad(row+hasBin);
    hPrintf("<TABLE><TR><TD BGCOLOR=#888888>\n");
    altGraphXMakeImage(ag);
    hPrintf("</TD></TR></TABLE><BR>");
    }
sqlFreeResult(&sr);
hPrintf("This graph shows alternative splicing observed in mRNAs and "
        "ESTs that is either conserved in mouse, present in full length "
	"mRNAs, or observed at least three times in ESTs.");
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

