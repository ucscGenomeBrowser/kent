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

/**** End of Lowe lab additions ****/
