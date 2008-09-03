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

void allEncodePeakLoadItems(struct track *tg)
/* Just call the flexible linkedFeatures loader with a few custom fns. */
{
enum encodePeakType peakType = tg->customInt;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
char **row;
int rowOffset;
struct linkedFeatures *lfList = NULL;
sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {  
    struct linkedFeatures *lf;
    int peak = -1;
    char **rowPastOffset = row + rowOffset;
    AllocVar(lf);
    /* Get the first three fields... that's essentially universal. */
    lf->start = sqlUnsigned(rowPastOffset[1]);
    lf->end = sqlUnsigned(rowPastOffset[2]);
    if ((peakType == narrowPeak) || (peakType == broadPeak) || (peakType == gappedPeak))
	{
	safecpy(lf->name, sizeof(lf->name), rowPastOffset[3]);
	lf->score = (float)sqlUnsigned(rowPastOffset[4]);
	lf->orientation = orientFromChar(rowPastOffset[5][0]);
	}
    else 
	lf->score = 1000;
    if (peakType == narrowPeak)
	peak = sqlSigned(rowPastOffset[8]);
    else if ((peakType == encodePeak6) || (peakType == encodePeak9))
	peak = sqlSigned(rowPastOffset[5]);
    /* zero is the first base in a peak. */
    if (peak > -1)
	{
	lf->tallStart = lf->start + peak;
	lf->tallEnd = lf->tallStart + 1;
	}
    /* Load blocks. */
    if ((peakType == gappedPeak) || (peakType == encodePeak9))
	{
	int blocksOffset = 6;
	int i;
	int numBlocks = 0;
	int *blockSizes, *chromStarts;
	int sizeOne;
	struct simpleFeature *sfList;
	if (peakType == gappedPeak)
	    blocksOffset = 9;
	numBlocks = sqlUnsigned(rowPastOffset[blocksOffset]);
	sqlSignedDynamicArray(rowPastOffset[blocksOffset+1], &blockSizes, &sizeOne);
	if (sizeOne != numBlocks)
	    errAbort("wrong number of blockSizes in track %s", tg->mapName);
	sqlSignedDynamicArray(rowPastOffset[blocksOffset+2], &chromStarts, &sizeOne);
	if (sizeOne != numBlocks)
	    errAbort("wrong number of chromStarts in track %s", tg->mapName);
	for (i = 0; i < numBlocks; i++)
	    {
	    struct simpleFeature *sf;
	    AllocVar(sf);
	    sf->start = chromStarts[i];
	    sf->end = chromStarts[i] + blockSizes[i];
	    slAddHead(&sfList, sf);
	    }
	slReverse(&sfList);
 	lf->components = sfList;
	}
    slAddHead(&lfList, lf);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&lfList);
slSort(&lfList, linkedFeaturesCmp);
tg->items = lfList;
}

void narrowPeakLoadItems(struct track *tg)
/* reverts to allEncodePeakLoadItems after checking the size. */
{
enum encodePeakType pt = narrowPeak;
int numFields = encodePeakNumFields(database, tg->mapName);
if (numFields == 9)
    pt = narrowPeak;
else
    errAbort("track %s has wrong number of fields for narrowPeak type\n", tg->mapName);
tg->customInt = pt;
allEncodePeakLoadItems(tg);
}

void broadPeakLoadItems(struct track *tg)
/* reverts to allEncodePeakLoadItems after checking the size. */
{
enum encodePeakType pt = broadPeak;
int numFields = encodePeakNumFields(database, tg->mapName);
if (numFields == 8)
    pt = broadPeak;
else
    errAbort("track %s has wrong number of fields for broadPeak type\n", tg->mapName);
tg->customInt = pt;
allEncodePeakLoadItems(tg);
}

void gappedPeakLoadItems(struct track *tg)
/* reverts to allEncodePeakLoadItems after checking the size. */
{
enum encodePeakType pt = gappedPeak;
int numFields = encodePeakNumFields(database, tg->mapName);
if (numFields == 14)
    pt = gappedPeak;
else
    errAbort("track %s has wrong number of fields for gappedPeak type\n", tg->mapName);
tg->customInt = pt;
allEncodePeakLoadItems(tg);
}

void encodePeakLoadItems(struct track *tg)
/* Determine the size of the table i.e. number of fields, then call the */
/* appropriate loader */
{
enum encodePeakType pt = 0;
int numFields = encodePeakNumFields(database, tg->mapName);
if (numFields == 5)
    pt = encodePeak5;
else if (numFields == 6)
    pt = encodePeak6;
else if (numFields == 9)
    pt = encodePeak9;
else 
    errAbort("track %s has wrong number of fields (%d) for the encodePeak type\n", tg->mapName, numFields);
tg->customInt = pt;
allEncodePeakLoadItems(tg);
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
Color rangeColor = shadesOfGray[grayInRange(lf->score, 1, 1000)];
Color peakColor = getOrangeColor();
if (lf->components)
    {
    struct simpleFeature *sf;
    drawScaledBox(hvg, lf->start, lf->end, scale, xOff, y+(heightPer/2), 1, rangeColor);
    for (sf = lf->components; sf != NULL; sf = sf->next)
	drawScaledBox(hvg, sf->start, sf->end, scale, xOff, y+shortOff, 
		      shortHeight, rangeColor);	
    }
else 
    drawScaledBox(hvg, lf->start, lf->end, scale, xOff, y+shortOff, 
		  shortHeight, rangeColor);
if ((lf->tallEnd > 0) && (lf->tallStart < lf->end))
    drawScaledBox(hvg, lf->tallStart, lf->tallEnd, scale, xOff, y, 
		  heightPer, peakColor);
}

char *encodePeakItemName(struct track *tg, void *item)
/* Get rid of the '.' names */
{
struct linkedFeatures *lf = item;
if (lf->name && sameString(lf->name, "."))
    return "";
else 
    return lf->name;
}

void encodePeakMethods(struct track *tg)
/* Methods for ENCODE peak track uses mostly linkedFeatures. */
{
linkedFeaturesMethods(tg);
tg->loadItems = encodePeakLoadItems;
tg->drawItemAt = encodePeakDrawAt;
tg->labelNextPrevItem = linkedFeaturesLabelNextPrevItem;
tg->itemName = encodePeakItemName;
tg->canPack = TRUE;
}

/* These next three are all straight forward.  Not much individuality in the family.  */

void narrowPeakMethods(struct track *tg)
{
encodePeakMethods(tg);
tg->loadItems = narrowPeakLoadItems;
}

void broadPeakMethods(struct track *tg)
{
encodePeakMethods(tg);
tg->loadItems = broadPeakLoadItems;
}

void gappedPeakMethods(struct track *tg)
{
encodePeakMethods(tg);
tg->loadItems = gappedPeakLoadItems;
}
