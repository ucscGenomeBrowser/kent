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

static char const rcsid[] = "$Id: transcriptome.c,v 1.6 2006/04/07 04:13:56 kate Exp $";



static Color affyTransfragColor(struct track *tg, void *item, struct vGfx *vg)
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
    blatCol = vgFindColorIx(vg, 100, 100, 160);
    pseudoCol = vgFindColorIx(vg, 190, 190, 230);
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
bedLoadItem(tg, tg->mapName, affyTransfragLoader);

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

static Color affyTxnPhase2ItemColor(struct track *tg, void *item, struct vGfx *vg)
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
    //blatCol = vgFindColorIx(vg, 100, 100, 160);
    blatCol = vgFindColorIx(vg, 230, 190, 190);
    //pseudoCol = vgFindColorIx(vg, 190, 190, 230);
    pseudoCol = vgFindColorIx(vg, 230, 190, 190);
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

static void affyTxnPhase2DrawLeftLabels(struct track *tg, int seqStart, int seqEnd,
				 struct vGfx *vg, int xOff, int yOff, int width, int height, 
				 boolean withCenterLabels, MgFont *font,
				 Color color, enum trackVisibility vis)
{
/* Don't do anything here, let subtracks draw their own labels. */
}

static void affyTxnPhase2BedDrawLeftLabels(struct track *track, int seqStart, int seqEnd,
				    struct vGfx *vg, int xOff, int yOff, int width, int height,
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
vgSetClip(vg, leftLabelX, yOff, leftLabelWidth, tHeight);
vgTextRight(vg, leftLabelX, yOff+centerLabel, leftLabelWidth-1, 
	    track->lineHeight, labelColor, font, 
	    track->shortLabel);
vgUnclip(vg);    
}

static char *affyTxnP2ItemName(struct track *track, void *item)
/* Don't have names for tese items. */
{
return "";
}

static int affyTxnPhase2TotalHeight(struct track *track, enum trackVisibility vis)
/* Get the total height for affyTxnPhase 2 data. */
{
/* this has all been figured out in affyTxnPhase2Load() */
return track->height;
}

static void affyTxnPhase2MapItem(struct track *tg, void *item, 
			  char *itemName, int start, int end, 
			  int x, int y, int width, int height)
/* Don't map anything. */
{
}

static void affyTxnPhase2Load(struct track *track)
/* Load up the items for affyTxnPhase2 track and set the visibility
   for the track. */
{
struct track *sub = NULL;
int totalHeight=0;
boolean first = TRUE;
boolean tooBig = (winEnd - winStart) > 1000000;
enum trackVisibility tnfgVis = tvHide;
char *visString = cartUsualString(cart, "hgt.affyPhase2.tnfg", "hide");
tnfgVis = hTvFromString(visString);

slReverse(&track->subtracks);

/* After a megabase, just give the packed view. */
if(tooBig && track->visibility == tvFull)
    track->limitedVis = tvDense;
else
    track->limitedVis = track->visibility;

/* Hide the transfrags tracks when not in full. */
if(track->limitedVis != tvFull)
    tnfgVis = tvHide;

/* Override composite default settings. */
for(sub = track->subtracks; sub != NULL; sub = sub->next)
    {
    if(stringIn("bed", sub->tdb->type) && track->limitedVis == tvFull) 
	{
	sub->visibility = tnfgVis;
	sub->mapsSelf = TRUE;
	sub->mapItem = affyTxnPhase2MapItem;
	}
    if(stringIn("wig", sub->tdb->type))
	{
//	sub->mapItem = affyTxnPhase2MapItem;
	sub->mapsSelf = FALSE;
	sub->extraUiData = CloneVar((struct wigCartOptions *)track->extraUiData);
	if(trackDbSetting(sub->tdb, "wigColorBy") != NULL)
	    ((struct wigCartOptions *)sub->extraUiData)->colorTrack = trackDbSetting(sub->tdb, "wigColorBy");
	}
    }

/* For the composite track we are going to display the first
   track. */
if(track->limitedVis == tvDense)
    {
    for(sub = track->subtracks; sub != NULL; sub = sub->next)
	{
	if(first && stringIn("wig", sub->tdb->type))
	    {
	    struct dyString *s = newDyString(128);
	    /* Display first track as full when in dense mode. */
	    sub->visibility = tvFull;
	    /* Display parent track name when we are not in full mode. */
	    dyStringPrintf(s, "%s (%s)", track->longLabel, sub->shortLabel);
	    freez(&sub->shortLabel);
	    sub->shortLabel = cloneString(track->shortLabel);
	    freez(&track->longLabel);
	    track->longLabel = cloneString(s->string);
	    dyStringFree(&s);
	    assert(sub->tdb->settingsHash);
	    assert(track->tdb->settingsHash);
	    /* For legacy carts remove existing track settings. */
	    if(trackDbSetting(sub->tdb, "centerLabelsDense"))
		hashRemove(sub->tdb->settingsHash, "centerLabelsDense");
	    if(trackDbSetting(track->tdb, "centerLabelsDense"))
		hashRemove(track->tdb->settingsHash, "centerLabelsDense");
	    
	    /* When in dense mode a composite track is supposed to print its center
	       label and surpress the center lable of subtracks, otherwise the
	       click map gets out of sync. Since we are going to draw using the
	       first track, turn its labels off and the label for the main track on. */
	    hashAdd(sub->tdb->settingsHash, "centerLabelsDense", "off");
	    hashAdd(track->tdb->settingsHash, "centerLabelsDense", "on");
	    first = FALSE;
	    }
	else 
	    {
	    sub->visibility = tvHide;
	    sub->limitedVis = tvHide;
	    sub->limitedVisSet = TRUE;
	    }
	}
    }
else
    {
    assert(track->tdb->settingsHash);
    if(trackDbSetting(track->tdb, "centerLabelsDense"))
	hashRemove(track->tdb->settingsHash, "centerLabelsDense");
    hashAdd(track->tdb->settingsHash, "centerLabelsDense", "off");
    }

/* Load up everything appropriate. */
for(sub = track->subtracks; sub != NULL; sub = sub->next)
    {
    /* If sub is visibile load it and set the visibility. */
    if(sub->visibility != tvHide && 
       isSubtrackVisible(sub))
	{
	enum trackVisibility tmpVis = sub->visibility;
	sub->loadItems(sub);
	if(stringIn("bed", sub->tdb->type) && 
	   slCount(sub->items) > 1000)
	    sub->visibility = tvDense;
	limitVisibility(sub);
	sub->visibility = tmpVis;
	totalHeight += sub->height;
	}
    else 
	{
	sub->limitedVis = tvHide;
	sub->limitedVisSet = TRUE;
	}
    }
track->height = totalHeight;
track->limitedVisSet = TRUE;

}

void affyTxnPhase2Methods(struct track *track)
/* Methods for dealing with a composite transcriptome tracks. */
{
struct track *sub = NULL;
track->loadItems = affyTxnPhase2Load;
track->totalHeight = affyTxnPhase2TotalHeight;
track->drawLeftLabels = affyTxnPhase2DrawLeftLabels;

/* Initialize the bed version with some special handlers. */
for(sub = track->subtracks; sub != NULL; sub = sub->next)
    {
    if(stringIn("bed", sub->tdb->type)) 
	{
	sub->itemName = affyTxnP2ItemName;
	sub->drawLeftLabels = affyTxnPhase2BedDrawLeftLabels;
	sub->itemColor = affyTxnPhase2ItemColor;
	}
    }
}
