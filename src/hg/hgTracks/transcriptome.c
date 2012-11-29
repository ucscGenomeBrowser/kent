/* transcriptome - stuff to handle loading and coloring of the
   affymetrix transcriptome tracks. These are sets of wiggle tracks
   representing probes tiled through non-repetitive portions of the
   human genome at 5 bp resolution. Also included are transfrags
   tracks which are bed tracks indicating what is thought to be
   expressed.
*/

#include "common.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"
#include "bed.h"
#include "wigCommon.h"




static Color affyTransfragColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Color for an affyTransfrag item. Different colors are returned depending
   on score. 
   0 - Passes all filters
   1 - Overlaps a pseudogene
   2 - Overlaps a blat hit
   3 - Overlaps both a pseudogene and a blat hit.
*/
{
static Color cleanCol = MG_WHITE;
static Color blatCol = MG_WHITE;
static Color pseudoCol = MG_WHITE;
struct bed *bed = item;

/* If first time through make the colors. */
if(cleanCol == MG_WHITE)
    {
    cleanCol = tg->ixColor;
    blatCol = hvGfxFindColorIx(hvg, 100, 100, 160);
    pseudoCol = hvGfxFindColorIx(hvg, 190, 190, 230);
    }

switch(bed->score) 
    {
    case 0 :
	return cleanCol;
    case 1 :
	return pseudoCol;
    case 2 :
	return blatCol;
    case 3 : 
	return pseudoCol;
    default:
	errAbort("Don't recognize score for transfrag %s", bed->name);
    };
return tg->ixColor;
}

static struct slList *affyTransfragLoader(char **row)
/* Wrapper around bedLoad5 to conform to ItemLoader type. */
{
return (struct slList *)bedLoad5(row);
}

static void affyTransfragsLoad(struct track *tg)
/* Load the items from the chromosome range indicated. Filter as
   necessary to remove things the user has requested not to see. Score
   indicates the status of a transfrag:
   0 - Passes all filters
   1 - Overlaps a pseudogene
   2 - Overlaps a blat hit
   3 - Overlaps both a pseudogene and a blat hit.
*/
{
struct bed *bed = NULL, *bedNext = NULL, *bedList = NULL;
boolean skipPseudos = cartUsualBoolean(cart, "affyTransfrags.skipPseudos", TRUE);
boolean skipDups = cartUsualBoolean(cart, "affyTransfrags.skipDups", FALSE);
/* Use simple bed loader to do database work. */
bedLoadItem(tg, tg->table, affyTransfragLoader);

/* Now filter based on options. */
for(bed = tg->items; bed != NULL; bed = bedNext) 
    {
    bedNext = bed->next;
    if((bed->score == 1 || bed->score == 3) && skipPseudos) 
	bedFree(&bed);
    else if((bed->score == 2 || bed->score == 3) && skipDups)
	bedFree(&bed);
    else 
	{
	slAddHead(&bedList, bed);
	}
    }
slReverse(&bedList);
tg->items = bedList;
}

    
void affyTransfragsMethods(struct track *tg)
/* Substitute a new load method that filters based on score. Also add
   a new itemColor() method that draws transfrags that overlap dups
   and pseudoGenes in a different color. */
{
tg->itemColor = affyTransfragColor;
tg->loadItems = affyTransfragsLoad;
}

static Color affyTxnPhase2ItemColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Color for an affyTransfrag item. Different colors are returned depending
   on name. 
   0 - Passes all filters
   1 - Overlaps a pseudogene
   2 - Overlaps a blat hit
   3 - Overlaps both a pseudogene and a blat hit.
*/
{
static Color cleanCol = MG_WHITE;
static Color blatCol = MG_WHITE;
static Color pseudoCol = MG_WHITE;
struct bed *bed = item;

/* If first time through make the colors. */
if(cleanCol == MG_WHITE)
    {
    cleanCol = tg->ixColor;
    blatCol = hvGfxFindColorIx(hvg, 100, 100, 160);
    pseudoCol = hvGfxFindColorIx(hvg, 190, 190, 230);
    }

switch(bed->name[0]) 
    {
    case '0' :
	return cleanCol;
    case '1' :
	return pseudoCol;
    case '2' :
	return blatCol;
    case '3' : 
	return pseudoCol;
    default:
	errAbort("Don't recognize score for transfrag %s", bed->name);
    };
return tg->ixColor;
}

static void affyTxnPhase2BedDrawLeftLabels(struct track *track, int seqStart, int seqEnd,
				    struct hvGfx *hvg, int xOff, int yOff, int width, int height,
				    boolean withCenterLabels, MgFont *font, Color color,
				    enum trackVisibility vis)
/* Draw the left label. Simple as we draw the name centered in the
   track height no matter what. */
{
int fontHeight = mgFontLineHeight(font);
int tHeight = track->height;
int centerLabel = (tHeight/2)-(fontHeight/2); /* vertical center of track. */
Color labelColor = (track->labelColor ? 
		    track->labelColor : track->ixColor);
/* If dense do nothing and return. */
if(track->limitedVis == tvHide)
    return;

/* Add the center label if necessary and draw track
   name on left side centered vertically. */
if (withCenterLabels)
    yOff += fontHeight;
hvGfxSetClip(hvg, leftLabelX, yOff, leftLabelWidth, tHeight);
hvGfxTextRight(hvg, leftLabelX, yOff+centerLabel, leftLabelWidth-1, 
	    track->lineHeight, labelColor, font, 
	    track->shortLabel);
hvGfxUnclip(hvg);    
}

static char *affyTxnP2ItemName(struct track *track, void *item)
/* Don't have names for tese items. */
{
return "";
}

static void affyTxnPhase2MapItem(struct track *tg, struct hvGfx *hvg, void *item, 
			  char *itemName, char *mapItemName, int start, int end, 
			  int x, int y, int width, int height)
/* Don't map anything. */
{
}

void affyTxnPhase2Methods(struct track *track)
/* Methods for dealing with a composite transcriptome tracks. */
{
if (track->subtracks == NULL && stringIn("bed", track->tdb->type))
    {
    track->itemName = affyTxnP2ItemName;
    track->drawLeftLabels = affyTxnPhase2BedDrawLeftLabels;
    track->itemColor = affyTxnPhase2ItemColor;
    track->mapsSelf = TRUE;
    track->mapItem = affyTxnPhase2MapItem;
    }
}
