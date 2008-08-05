/* encode.c - hgTracks routines that are specific to the ENCODE project */

#include "common.h"
#include "hCommon.h"
#include "hdb.h"
#include "hgTracks.h"
#include "encode.h"
#include "encode/encodeRna.h"
#include "encode/encodePeak.h"

char *encodeErgeName(struct track *tg, void *item)
/* return the actual data name, in form xx/yyyy cut off xx/ return yyyy */
{
char *name;
struct linkedFeatures *lf = item;
name = strstr(lf->name, "/");
if (name != NULL)
    name ++;
if (name != NULL)
    return name;
return "unknown";
}
  
void encodeErgeMethods(struct track *tg)
/* setup special methods for ENCODE dbERGE II tracks */
{
tg->itemName = encodeErgeName;
}

Color encodeStanfordNRSFColor(struct track *tg, void *item, struct hvGfx *hvg)
/* color by strand */
{
struct bed *thisItem = item;
int r = tg->color.r;
int g = tg->color.g;
int b = tg->color.b;

if (thisItem->strand[0] == '-') 
    {
    r = g;
    g = b;
    b = tg->color.r;
    }
return hvGfxFindColorIx(hvg, r, g, b);
}

void encodeStanfordNRSFMethods(struct track *tg)
/* custom methods for ENCODE Stanford NRSF data */
{
tg->itemColor = encodeStanfordNRSFColor;
tg->itemNameColor = encodeStanfordNRSFColor;
}



void loadEncodeRna(struct track *tg)
/* Load up encodeRna from database table to track items. */
{
bedLoadItem(tg, "encodeRna", (ItemLoader)encodeRnaLoad);
}

void freeEncodeRna(struct track *tg)
/* Free up encodeRna items. */
{
encodeRnaFreeList((struct encodeRna**)&tg->items);
}

Color encodeRnaColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color of encodeRna track item. */
{
struct encodeRna *el = item;

if(el->isRmasked)     return MG_BLACK;
if(el->isTranscribed) return hvGfxFindColorIx(hvg, 0x79, 0xaa, 0x3d);
if(el->isPrediction)  return MG_RED;
return MG_BLUE;
}

char *encodeRnaName(struct track *tg, void *item)
/* Return RNA gene name. */
{
struct encodeRna *el = item;
char *full = el->name;
static char abbrev[64];
char *e;

strcpy(abbrev, skipChr(full));
subChar(abbrev, '_', ' ');
abbr(abbrev, " pseudogene");
if ((e = strstr(abbrev, "-related")) != NULL)
    strcpy(e, "-like");
return abbrev;
}

void encodeRnaMethods(struct track *tg)
/* Make track for rna genes . */
{
tg->loadItems = loadEncodeRna;
tg->freeItems = freeEncodeRna;
tg->itemName = encodeRnaName;
tg->itemColor = encodeRnaColor;
tg->itemNameColor = encodeRnaColor;
}

struct slList *encodePeakAlmostLoadItems(char **row)
/* Sort of an intermediary function to accommodate some */
/* general linkedFeatures loader. */
{
return (struct slList *)encodePeakLoad(row);
}

struct linkedFeatures *lfFromEncodePeak(struct slList *item)
/* converts encode peaksbed to linkedFeatures, and also puts */
/* that info in itemRgb in the extra field so we can use it later. */
/* (used by loadLinkedFeaturesWithLoaders). */
{
struct encodePeak *peak = (struct encodePeak *)item;
struct linkedFeatures *lf;
struct simpleFeature *sf;
AllocVar(lf);
lf->start = peak->chromStart;
lf->end = peak->chromEnd;
if (peak->peak >= 0)
    {
    lf->tallStart = peak->chromStart + peak->peak;
    lf->tallEnd = lf->tallStart + 1;
    AllocVar(sf);
    sf->start = lf->tallStart;
    sf->end = lf->tallEnd;
    lf->components = sf;
    lf->codons = CloneVar(sf);
    }
else 
    {
    lf->tallStart = lf->tallEnd = 0;
    lf->components = lf->codons = NULL;
    }
return lf;
}

void encodePeakLoadItems(struct track *tg)
/* Just call the flexible linkedFeatures loader with a few custom fns. */
{
loadLinkedFeaturesWithLoaders(tg, encodePeakAlmostLoadItems, lfFromEncodePeak,
			      NULL, NULL, NULL, NULL);
}

static void encodePeakDrawAt(struct track *tg, void *item,
	struct hvGfx *hvg, int xOff, int y, double scale, 
	MgFont *font, Color color, enum trackVisibility vis)
/* Draw the peak from the linkedFeature.  Currently this doesn't draw any */
/* sorta shading based on the signalValue/pValue. */
{
struct linkedFeatures *lf = item;
int heightPer = tg->heightPer;
int shortOff = heightPer/4;
int shortHeight = heightPer - 2*shortOff;
Color rangeColor = blackIndex();
Color peakColor = getOrangeColor();
drawScaledBox(hvg, lf->start, lf->end, scale, xOff, y+shortOff, 
	      shortHeight, rangeColor);
if ((lf->tallEnd > 0) && (lf->tallStart < lf->end))
    drawScaledBox(hvg, lf->tallStart, lf->tallEnd, scale, xOff, y, 
		  heightPer, peakColor);
}

void encodePeakMethods(struct track *tg)
/* Methods for ENCODE peak track uses mostly linkedFeatures. */
{
linkedFeaturesMethods(tg);
tg->loadItems = encodePeakLoadItems;
tg->drawItemAt = encodePeakDrawAt;
tg->labelNextPrevItem = linkedFeaturesLabelNextPrevItem;
}
