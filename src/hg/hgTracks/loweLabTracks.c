#include "common.h"
#include "hash.h"
#include "localmem.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "bed.h"
#include "psl.h"
#include "hgTracks.h"
#include "loweLabTracks.h"


/* Declare our color gradients and the the number of colors in them */
#define LL_EXPR_DATA_SHADES 16
Color LLshadesOfGreen[LL_EXPR_DATA_SHADES];
Color LLshadesOfRed[LL_EXPR_DATA_SHADES];
boolean LLexprBedColorsMade = FALSE; /* Have the shades of Green, Red, and Blue been allocated? */
int LLmaxRGBShade = LL_EXPR_DATA_SHADES - 1;

/**** Lowe lab additions ***/

void loadBed6(struct track *tg)
/* Load the items in one custom track - just move beds in
 * window... */
{
struct bed *bed, *list = NULL;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int rowOffset;

sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    bed = bedLoadN(row+rowOffset, 6);
    slAddHead(&list, bed);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&list);
tg->items = list;
}

Color gbGeneColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color to draw gene in. */
{
struct bed *bed = item;
if (bed->strand[0] == '+')
    return tg->ixColor;
return tg->ixAltColor;
}

void gbGeneMethods(struct track *tg)
/* Track group for genbank gene tracks */
{
tg->loadItems = loadBed6;
tg->itemColor = gbGeneColor;
}

Color tigrGeneColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color to draw gene in. */
{
struct bed *bed = item;
if (bed->strand[0] == '+')
    return tg->ixColor;
return tg->ixAltColor;
}

void tigrGeneMethods(struct track *tg)
/* Track group for genbank gene tracks */
{
tg->loadItems = loadBed6;
tg->itemColor = tigrGeneColor;
}

Color llArrayColor(struct track *tg, void *item, struct vGfx *vg)
/* Does the score->color conversion for various microarray tracks */
{
return expressionColor(tg, item, vg, 1.0, 3.0);
}

void lfsFromllArrayBed(struct track *tg)
/* filters the bedList stored at tg->items
into a linkedFeaturesSeries as determined by
filter type */
{
struct linkedFeaturesSeries *lfsList = NULL, *lfs;
struct linkedFeatures *lf;
struct bed *bed = NULL, *bedList= NULL;
char *llArrayExp = NULL;
int i=0;
bedList = tg->items;
llArrayExp = cloneStringZ(tg->mapName, strlen(tg->mapName)+5);
llArrayExp[strlen(tg->mapName)] = 0;
llArrayExp = strcat(llArrayExp, "Exps");
if(tg->limitedVis == tvDense)
    {
    tg->items = lfsFromMsBedSimple(bedList, "llArray");
    }
else
    {
    tg->items = msBedGroupByIndex(bedList, database, llArrayExp, 0, NULL, -1);
    slSort(&tg->items,lfsSortByName);
    }
bedFreeList(&bedList);
freeMem(llArrayExp);
}

void llArrayMethods(struct track *tg)
/* This is kind of like Chuck's microarray ones. */
{
linkedFeaturesSeriesMethods(tg);
tg->itemColor = llArrayColor;
tg->loadItems = loadMaScoresBed;
tg->trackFilter = lfsFromllArrayBed;
tg->mapItem = lfsMapItemName;
tg->mapsSelf = TRUE;
}

void loadOperon(struct track *tg)
/* Load the items in one custom track - just move beds in
 * window... */
{
struct linkedFeatures *lfList = NULL, *lf;
struct bed *bed, *list = NULL;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int rowOffset;

sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    bed = bedLoadN(row+rowOffset, 15);
    slAddHead(&list, bed);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&list);

for (bed = list; bed != NULL; bed = bed->next)
    {
    struct simpleFeature *sf;
    int i;
    lf = lfFromBed(bed);
    for (sf = lf->components, i = 0; sf != NULL, i < bed->expCount; sf = sf->next, i++)
	sf->grayIx = grayInRange((int)(bed->expScores[i]),0,1000);
    slAddHead(&lfList,lf);
    }
tg->items = lfList;
}

void tigrOperonDrawAt(struct track *tg, void *item,
	struct vGfx *vg, int xOff, int y, double scale, 
	MgFont *font, Color color, enum trackVisibility vis)
/* Draw the operon at position. */
{
struct linkedFeatures *lf = item; 
struct simpleFeature *sf;
int heightPer = tg->heightPer;
int x1,x2;
int s, e, e2, s2;
Color *shades = tg->colorShades;
int midY = y + (heightPer>>1);
int midY1 = midY - (heightPer>>2);
int midY2 = midY + (heightPer>>2);
int w;

color = tg->ixColor;
x1 = round((double)((int)lf->start-winStart)*scale) + xOff;
x2 = round((double)((int)lf->end-winStart)*scale) + xOff;
w = x2-x1;
innerLine(vg, x1, midY, w, color);
if (vis == tvFull || vis == tvPack)
    {
    clippedBarbs(vg, x1, midY, w, 2, 5, 
		 lf->orientation, color, FALSE);
    }
for (sf = lf->components; sf != NULL; sf = sf->next)
    {
    s = sf->start; e = sf->end;
    /* shade ORF (exon) based on the grayIx value of the sf */
    color = shades[sf->grayIx];
    drawScaledBox(vg, s, e, scale, xOff, y, heightPer,
			color );
    }
}

void tigrOperonMethods(struct track *tg)
{
linkedFeaturesMethods(tg);
tg->loadItems = loadOperon;
tg->colorShades = shadesOfGray;
tg->drawItemAt = tigrOperonDrawAt;
}

/**** End of Lowe lab additions ****/
