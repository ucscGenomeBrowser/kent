/* variation.c - hgTracks routines that are specific to the SNP and
 * haplotype tracks */

#include "variation.h"

static char const rcsid[] = "$Id: variation.c,v 1.39 2005/10/23 07:51:49 daryl Exp $";

void filterSnpMapItems(struct track *tg, boolean (*filter)
		       (struct track *tg, void *item))
/* Filter out items from track->itemList. */
{
filterSnpItems(tg, filter);
}

void filterSnpItems(struct track *tg, boolean (*filter)
		    (struct track *tg, void *item))
/* Filter out items from track->itemList. */
{
struct slList *newList = NULL, *el, *next;

for (el = tg->items; el != NULL; el = next)
    {
    next = el->next;
    if (filter(tg, el))
 	slAddHead(&newList, el);
    }
slReverse(&newList);
tg->items = newList;
}

boolean snpMapSourceFilterItem(struct track *tg, void *item)
/* Return TRUE if item passes filter. */
{
struct snpMap *el = item;
int    snpMapSource = 0;

for (snpMapSource=0; snpMapSource<snpMapSourceCartSize; snpMapSource++)
    if (containsStringNoCase(el->source,snpMapSourceDataName[snpMapSource]))
 	if (sameString(snpMapSourceCart[snpMapSource], "exclude"))
 	    return FALSE;
return TRUE;
}

boolean snpMapTypeFilterItem(struct track *tg, void *item)
/* Return TRUE if item passes filter. */
{
struct snpMap *el = item;
int    snpMapType = 0;

for (snpMapType=0; snpMapType<snpMapTypeCartSize; snpMapType++)
    if (containsStringNoCase(el->type,snpMapTypeDataName[snpMapType]))
 	if (sameString(snpMapTypeCart[snpMapType], "exclude"))
 	    return FALSE;
return TRUE;
}

boolean snpAvHetFilterItem(struct track *tg, void *item)
/* Return TRUE if item passes filter. */
{
struct snp *el = item;

if (el->avHet < atof(cartUsualString(cart, "snpAvHetCutoff", "0.0")))
    return FALSE;
return TRUE;
}

boolean snpSourceFilterItem(struct track *tg, void *item)
/* Return TRUE if item passes filter. */
{
struct snp *el = item;
int    snpSource = 0;

for (snpSource=0; snpSource<snpSourceCartSize; snpSource++)
    if (containsStringNoCase(el->source,snpSourceDataName[snpSource]))
 	if (sameString(snpSourceCart[snpSource], "exclude") )
 	    return FALSE;
return TRUE;
}

boolean snpMolTypeFilterItem(struct track *tg, void *item)
/* Return TRUE if item passes filter. */
{
struct snp *el = item;
int    snpMolType = 0;

for (snpMolType=0; snpMolType<snpMolTypeCartSize; snpMolType++)
    if (containsStringNoCase(el->molType,snpMolTypeDataName[snpMolType]))
 	if ( sameString(snpMolTypeCart[snpMolType], "exclude") )
 	    return FALSE;
return TRUE;
}

boolean snpClassFilterItem(struct track *tg, void *item)
/* Return TRUE if item passes filter. */
{
struct snp *el = item;
int    snpClass = 0;

for (snpClass=0; snpClass<snpClassCartSize; snpClass++)
    if (containsStringNoCase(el->class,snpClassDataName[snpClass]))
 	if ( sameString(snpClassCart[snpClass], "exclude") )
 	    return FALSE;
return TRUE;
}

boolean snpValidFilterItem(struct track *tg, void *item)
/* Return TRUE if item passes filter. */
{
struct snp *el = item;
int    snpValid = 0;

for (snpValid=0; snpValid<snpValidCartSize; snpValid++)
    if (containsStringNoCase(el->valid,snpValidDataName[snpValid]))
 	if ( sameString(snpValidCart[snpValid], "exclude") )
 	    return FALSE;
return TRUE;
}

boolean snpFuncFilterItem(struct track *tg, void *item)
/* Return TRUE if item passes filter. */
{
struct snp *el = item;
int    snpFunc = 0;

for (snpFunc=0; snpFunc<snpFuncCartSize; snpFunc++)
    if (containsStringNoCase(el->func,snpFuncDataName[snpFunc]))
 	if ( sameString(snpFuncCart[snpFunc], "exclude") )
 	    return FALSE;
return TRUE;
}

boolean snpLocTypeFilterItem(struct track *tg, void *item)
/* Return TRUE if item passes filter. */
{
struct snp *el = item;
int    snpLocType = 0;

for (snpLocType=0; snpLocType<snpLocTypeCartSize; snpLocType++)
    if (containsStringNoCase(el->locType,snpLocTypeDataName[snpLocType]))
 	if ( sameString(snpLocTypeCart[snpLocType], "exclude") )
 	    return FALSE;
return TRUE;
}

void loadSnpMap(struct track *tg)
/* Load up snpMap from database table to track items. */
{
int  snpMapSource, snpMapType;

for (snpMapSource=0; snpMapSource<snpMapSourceCartSize; snpMapSource++)
    snpMapSourceCart[snpMapSource] = cartUsualString(cart, 
       snpMapSourceStrings[snpMapSource], snpMapSourceDefault[snpMapSource]);
for (snpMapType=0; snpMapType<snpMapTypeCartSize; snpMapType++)
    snpMapTypeCart[snpMapType] = cartUsualString(cart, 
       snpMapTypeStrings[snpMapType], snpMapTypeDefault[snpMapType]);
bedLoadItem(tg, "snpMap", (ItemLoader)snpMapLoad);
if (!startsWith("hg",database))
    return;
filterSnpMapItems(tg, snpMapSourceFilterItem);
filterSnpMapItems(tg, snpMapTypeFilterItem);
}

void loadSnp(struct track *tg)
/* Load up snp from database table to track items. */
{
int  snpSource  = 0;
int  snpMolType = 0;
int  snpClass   = 0;
int  snpValid   = 0;
int  snpFunc    = 0;
int  snpLocType = 0;

snpAvHetCutoff = atof(cartUsualString(cart, "snpAvHetCutoff", "0.0"));
for (snpSource=0;  snpSource  < snpSourceCartSize; snpSource++)
    snpSourceCart[snpSource] = cartUsualString(cart, 
       snpSourceStrings[snpSource], snpSourceDefault[snpSource]);
for (snpMolType=0; snpMolType < snpMolTypeCartSize; snpMolType++)
    snpMolTypeCart[snpMolType] = cartUsualString(cart, 
       snpMolTypeStrings[snpMolType], snpMolTypeDefault[snpMolType]);
for (snpClass=0;   snpClass   < snpClassCartSize; snpClass++)
    snpClassCart[snpClass] = cartUsualString(cart, 
       snpClassStrings[snpClass], snpClassDefault[snpClass]);
for (snpValid=0;   snpValid   < snpValidCartSize; snpValid++)
    snpValidCart[snpValid] = cartUsualString(cart, 
       snpValidStrings[snpValid], snpValidDefault[snpValid]);
for (snpFunc=0;    snpFunc    < snpFuncCartSize; snpFunc++)
    snpFuncCart[snpFunc] = cartUsualString(cart, 
       snpFuncStrings[snpFunc], snpFuncDefault[snpFunc]);
for (snpLocType=0; snpLocType < snpLocTypeCartSize; snpLocType++)
    snpLocTypeCart[snpLocType] = cartUsualString(cart, 
       snpLocTypeStrings[snpLocType], snpLocTypeDefault[snpLocType]);
bedLoadItem(tg, "snp", (ItemLoader)snpLoad);

filterSnpItems(tg, snpAvHetFilterItem);
filterSnpItems(tg, snpSourceFilterItem);
filterSnpItems(tg, snpMolTypeFilterItem);
filterSnpItems(tg, snpClassFilterItem);
filterSnpItems(tg, snpValidFilterItem);
filterSnpItems(tg, snpFuncFilterItem);
filterSnpItems(tg, snpLocTypeFilterItem);
}

void freeSnpMap(struct track *tg)
/* Free up snpMap items. */
{
snpMapFreeList((struct snpMap**)&tg->items);
}

void freeSnp(struct track *tg)
/* Free up snp items. */
{
snpFreeList((struct snp**)&tg->items);
}

Color snpMapColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color of snpMap track item. */
{
struct snpMap *el = item;
enum   snpColorEnum thisSnpColor = \
    stringArrayIx(snpMapSourceCart[
		     stringArrayIx(el->source,
				   snpMapSourceDataName,
				   snpMapSourceDataNameSize
				   )],
		  snpColorLabel,snpColorLabelSize);

switch (thisSnpColor)
    {
    case snpColorRed:
 	return MG_RED;
 	break;
    case snpColorGreen:
 	return vgFindColorIx(vg, 0x79, 0xaa, 0x3d);
 	break;
    case snpColorBlue:
 	return MG_BLUE;
 	break;
    default:
 	return MG_BLACK;
 	break;
    }
}

Color snpColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color of snp track item. */
{
struct snp *el = item;
enum   snpColorEnum thisSnpColor = snpColorBlack;
char  *snpColorSource = cartUsualString(cart, 
			snpColorSourceDataName[0], snpColorSourceDefault[0]);
char  *validString = NULL;
char  *funcString = NULL;
int    snpValid = 0;
int    snpFunc = 0;

switch (stringArrayIx(snpColorSource, 
		      snpColorSourceStrings, snpColorSourceStringsSize))
    {
    case snpColorSourceSource:
	thisSnpColor=(enum snpColorEnum)stringArrayIx(snpSourceCart[stringArrayIx(el->source,snpSourceDataName,snpSourceDataNameSize)],snpColorLabel,snpColorLabelSize);
	break;
    case snpColorSourceMolType:
	thisSnpColor=(enum snpColorEnum)stringArrayIx(snpMolTypeCart[stringArrayIx(el->molType,snpMolTypeDataName,snpMolTypeDataNameSize)],snpColorLabel,snpColorLabelSize);
	break;
    case snpColorSourceClass:
	thisSnpColor=(enum snpColorEnum)stringArrayIx(snpClassCart[stringArrayIx(el->class,snpClassDataName,snpClassDataNameSize)],snpColorLabel,snpColorLabelSize);
	break;
    case snpColorSourceValid:
	validString = cloneString(el->valid);
	for (snpValid=0; snpValid<snpValidCartSize; snpValid++)
	    if (containsStringNoCase(validString, snpValidDataName[snpValid]))
		thisSnpColor = (enum snpColorEnum) stringArrayIx(snpValidCart[snpValid],snpColorLabel,snpColorLabelSize);
	break;
    case snpColorSourceFunc:
	funcString = cloneString(el->func);
	for (snpFunc=0; snpFunc<snpFuncCartSize; snpFunc++)
	    if (containsStringNoCase(funcString, snpFuncDataName[snpFunc]))
		thisSnpColor = (enum snpColorEnum) stringArrayIx(snpFuncCart[snpFunc],snpColorLabel,snpColorLabelSize);
	break;
    case snpColorSourceLocType:
	thisSnpColor=(enum snpColorEnum)stringArrayIx(snpLocTypeCart[stringArrayIx(el->locType,snpLocTypeDataName,snpLocTypeDataNameSize)],snpColorLabel,snpColorLabelSize);
	break;
    default:
	thisSnpColor = snpColorBlack;
	break;
    }
switch (thisSnpColor)
    {
    case snpColorRed:
 	return MG_RED;
 	break;
    case snpColorGreen:
 	return vgFindColorIx(vg, 0x79, 0xaa, 0x3d);
 	break;
    case snpColorBlue:
 	return MG_BLUE;
 	break;
    case snpColorBlack:
    default:
 	return MG_BLACK;
 	break;
    }
}

void snpMapDrawItemAt(struct track *tg, void *item, 
	struct vGfx *vg, int xOff, int y, 
	double scale, MgFont *font, Color color, enum trackVisibility vis)
/* Draw a single snpMap item at position. */
{
struct snpMap *sm = item;
int heightPer = tg->heightPer;
int x1 = round((double)((int)sm->chromStart-winStart)*scale) + xOff;
int x2 = round((double)((int)sm->chromEnd-winStart)*scale) + xOff;
int w = x2-x1;
Color itemColor = tg->itemColor(tg, sm, vg);

if ( w<1 )
    w=1;
vgBox(vg, x1, y, w, heightPer, itemColor);
/* Clip here so that text will tend to be more visible... */
if (tg->drawName && vis != tvSquish)
    mapBoxHc(sm->chromStart, sm->chromEnd, x1, y, w, heightPer,
	     tg->mapName, tg->mapItemName(tg, sm), NULL);
}

void snpDrawItemAt(struct track *tg, void *item, 
	struct vGfx *vg, int xOff, int y, 
	double scale, MgFont *font, Color color, enum trackVisibility vis)
/* Draw a single snp item at position. */
{
struct snp *s = item;
int heightPer = tg->heightPer;
int x1 = round((double)((int)s->chromStart-winStart)*scale) + xOff;
int x2 = round((double)((int)s->chromEnd-winStart)*scale) + xOff;
int w = x2-x1;
Color itemColor = tg->itemColor(tg, s, vg);

if ( w<1 )
    w=1;
vgBox(vg, x1, y, w, heightPer, itemColor);
/* Clip here so that text will tend to be more visible... */
if (tg->drawName && vis != tvSquish)
    mapBoxHc(s->chromStart, s->chromEnd, x1, y, w, heightPer,
	     tg->mapName, tg->mapItemName(tg, s), NULL);
}

static void snpMapDrawItems(struct track *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw snpMap items. */
{
double scale = scaleForPixels(width);
int lineHeight = tg->lineHeight;
int heightPer = tg->heightPer;
int y;
boolean withLabels = (withLeftLabels && vis == tvPack && !tg->drawName);
boolean doHgGene = trackWantsHgGene(tg);

if (!tg->drawItemAt)
    errAbort("missing drawItemAt in track %s", tg->mapName);

if (vis == tvPack || vis == tvSquish)
    {
    struct spaceSaver *ss = tg->ss;
    struct spaceNode *sn = NULL;
    vgSetClip(vg, insideX, yOff, insideWidth, tg->height);
    for (sn = ss->nodeList; sn != NULL; sn = sn->next)
        {
        struct slList *item = sn->val;
        int s = tg->itemStart(tg, item);
        int e = tg->itemEnd(tg, item);
        int x1 = round((s - winStart)*scale) + xOff;
        int x2 = round((e - winStart)*scale) + xOff;
        int textX = x1;
        char *name = tg->itemName(tg, item);
	Color itemColor = tg->itemColor(tg, item, vg);
	Color itemNameColor = tg->itemNameColor(tg, item, vg);
	
        y = yOff + lineHeight * sn->row;
        tg->drawItemAt(tg, item, vg, xOff, y, scale, font, itemColor, vis);
        if (withLabels)
            {
            int nameWidth = mgFontStringWidth(font, name);
            int dotWidth = tl.nWidth/2;
            textX -= nameWidth + dotWidth;
            if (textX < insideX)        /* Snap label to the left. */
		{
		textX = leftLabelX;
		vgUnclip(vg);
		vgSetClip(vg, leftLabelX, yOff, insideWidth, tg->height);
		vgTextRight(vg, leftLabelX, y, leftLabelWidth-1, heightPer,
			    itemNameColor, font, name);
		vgUnclip(vg);
		vgSetClip(vg, insideX, yOff, insideWidth, tg->height);
		}
            else
		vgTextRight(vg, textX, y, nameWidth, heightPer, 
			    itemNameColor, font, name);
            }
        if (!tg->mapsSelf)
            {
            int w = x2-textX;
            if (w > 0)
                mapBoxHgcOrHgGene(s, e, textX, y, w, heightPer, tg->mapName, 
				  tg->mapItemName(tg, item), name, doHgGene);
            }
        }
    vgUnclip(vg);
    }
else
    {
    struct slList *item;
    y = yOff;
    for (item = tg->items; item != NULL; item = item->next)
        {
	Color itemColor = tg->itemColor(tg, item, vg);
        tg->drawItemAt(tg, item, vg, xOff, y, scale, font, itemColor, vis);
        if (vis == tvFull) 
	    y += lineHeight;
        } 
    }
}

static void snpDrawItems(struct track *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw snp items. */
{
double scale = scaleForPixels(width);
int lineHeight = tg->lineHeight;
int heightPer = tg->heightPer;
int y;
boolean withLabels = (withLeftLabels && vis == tvPack && !tg->drawName);
boolean doHgGene = trackWantsHgGene(tg);

if (!tg->drawItemAt)
    errAbort("missing drawItemAt in track %s", tg->mapName);

if (vis == tvPack || vis == tvSquish)
    {
    struct spaceSaver *ss = tg->ss;
    struct spaceNode *sn = NULL;
    vgSetClip(vg, insideX, yOff, insideWidth, tg->height);
    for (sn = ss->nodeList; sn != NULL; sn = sn->next)
        {
        struct slList *item = sn->val;
        int s = tg->itemStart(tg, item);
        int e = tg->itemEnd(tg, item);
        int x1 = round((s - winStart)*scale) + xOff;
        int x2 = round((e - winStart)*scale) + xOff;
        int textX = x1;
        char *name = tg->itemName(tg, item);
	Color itemColor = tg->itemColor(tg, item, vg);
	Color itemNameColor = tg->itemNameColor(tg, item, vg);
	
        y = yOff + lineHeight * sn->row;
        tg->drawItemAt(tg, item, vg, xOff, y, scale, font, itemColor, vis);
        if (withLabels)
            {
            int nameWidth = mgFontStringWidth(font, name);
            int dotWidth = tl.nWidth/2;
            textX -= nameWidth + dotWidth;
            if (textX < insideX)        /* Snap label to the left. */
		{
		textX = leftLabelX;
		vgUnclip(vg);
		vgSetClip(vg, leftLabelX, yOff, insideWidth, tg->height);
		vgTextRight(vg, leftLabelX, y, leftLabelWidth-1, heightPer,
			    itemNameColor, font, name);
		vgUnclip(vg);
		vgSetClip(vg, insideX, yOff, insideWidth, tg->height);
		}
            else
		vgTextRight(vg, textX, y, nameWidth, heightPer, 
			    itemNameColor, font, name);
            }
        if (!tg->mapsSelf)
            {
            int w = x2-textX;
            if (w > 0)
                mapBoxHgcOrHgGene(s, e, textX, y, w, heightPer, tg->mapName, 
				  tg->mapItemName(tg, item), name, doHgGene);
            }
        }
    vgUnclip(vg);
    }
else
    {
    struct slList *item;
    y = yOff;
    for (item = tg->items; item != NULL; item = item->next)
        {
	Color itemColor = tg->itemColor(tg, item, vg);
        tg->drawItemAt(tg, item, vg, xOff, y, scale, font, itemColor, vis);
        if (vis == tvFull) 
	    y += lineHeight;
        } 
    }
}

void snpMapMethods(struct track *tg)
/* Make track for snps. */
{
tg->drawItems = snpMapDrawItems;
tg->drawItemAt = snpMapDrawItemAt;
tg->loadItems = loadSnpMap;
tg->freeItems = freeSnpMap;
tg->itemColor = snpMapColor;
tg->itemNameColor = snpMapColor;
}

void snpMethods(struct track *tg)
/* Make track for snps. */
{
tg->drawItems     = snpDrawItems;
tg->drawItemAt    = snpDrawItemAt;
tg->loadItems     = loadSnp;
tg->freeItems     = freeSnp;
tg->itemColor     = snpColor;
tg->itemNameColor = snpColor;
}

char *perlegenName(struct track *tg, void *item)
/* return the actual perlegen name, in form xx/yyyy:
      cut off xx/ and return yyyy */
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

int haplotypeHeight(struct track *tg, struct linkedFeatures *lf,
                    struct simpleFeature *sf)
/* if the item isn't the first or the last make it smaller */
{
if(sf == lf->components || sf->next == NULL)
    return tg->heightPer;
return (tg->heightPer-4);
}

static void haplotypeLinkedFeaturesDrawAt(struct track *tg, void *item,
               struct vGfx *vg, int xOff, int y, double scale, 
	       MgFont *font, Color color, enum trackVisibility vis)
/* draws and individual haplotype and a given location */
{
struct linkedFeatures *lf = item;
struct simpleFeature *sf = NULL;
int x1 = 0;
int x2 = 0;
int w = 0;
int heightPer = tg->heightPer;
int shortHeight = heightPer / 2;
int s = 0;
int e = 0;
Color *shades = tg->colorShades;
boolean isXeno = (tg->subType == lfSubXeno) || (tg->subType == lfSubChain);
boolean hideLine = (vis == tvDense && isXeno);

/* draw haplotype thick line ... */
if (lf->components != NULL && !hideLine)
    {
    x1 = round((double)((int)lf->start-winStart)*scale) + xOff;
    x2 = round((double)((int)lf->end-winStart)*scale) + xOff;
    w = x2 - x1;
    color = shades[lf->grayIx+isXeno];

    vgBox(vg, x1, y+3, w, shortHeight-2, color);
    if (vis==tvSquish)
	vgBox(vg, x1, y+1, w, tg->heightPer/2, color);
    else
	vgBox(vg, x1, y+3, w, shortHeight-1, color);
    }
for (sf = lf->components; sf != NULL; sf = sf->next)
    {
    s = sf->start;
    e = sf->end;
    heightPer = haplotypeHeight(tg, lf, sf);
    if (vis==tvSquish)
	drawScaledBox(vg, s, e, scale, xOff, y, tg->heightPer, blackIndex());
    else
	drawScaledBox(vg, s, e, scale, xOff, y+((tg->heightPer-heightPer)/2),
		      heightPer, blackIndex());
    }
}

void haplotypeMethods(struct track *tg)
/* setup special methods for haplotype track */
{
tg->drawItemAt = haplotypeLinkedFeaturesDrawAt;
tg->colorShades = shadesOfSea;
}

void perlegenMethods(struct track *tg)
/* setup special methods for Perlegen haplotype track */
{
haplotypeMethods(tg);
tg->itemName = perlegenName;
}


/*******************************************************************/

/* Declare our color gradients and the the number of colors in them */
Color   ldShadesOfGreen[LD_DATA_SHADES];
Color   ldShadesOfRed[LD_DATA_SHADES];
Color   ldShadesOfBlue[LD_DATA_SHADES];
Color   ldHighLodLowDprime;
Color   ldHighDprimeLowLod;
boolean ldColorsMade  = FALSE; /* Have the shades been allocated? */
int     maxLdRgbShade = LD_DATA_SHADES - 1;

void makeLdShades(struct vGfx *vg) 
/* Allocate the LD shades of Red, Green and Blue */
{
static struct rgbColor white = {255, 255, 255};
static struct rgbColor red   = {255,   0,   0};
static struct rgbColor green = {  0, 255,   0};
static struct rgbColor blue  = {  0,   0, 255};
vgMakeColorGradient(vg, &white, &red,   LD_DATA_SHADES, ldShadesOfRed);
vgMakeColorGradient(vg, &white, &green, LD_DATA_SHADES, ldShadesOfGreen);
vgMakeColorGradient(vg, &white, &blue,  LD_DATA_SHADES, ldShadesOfBlue);
ldHighLodLowDprime = vgFindColorIx(vg, 255, 224, 224); /* pink */
ldHighDprimeLowLod = vgFindColorIx(vg, 192, 192, 240); /* blue */
ldColorsMade       = TRUE;
}

void ldLoadItems(struct track *tg)
/* loadItems loads up items for the chromosome range indicated.   */
{
int count=0;

bedLoadItem(tg, tg->mapName, (ItemLoader)ldLoad);
count = slCount((struct sList *)(tg->items));
tg->canPack = FALSE;
if (count>5000)
    {
    tg->limitedVis=tvDense;
    tg->limitedVisSet=TRUE;
    cartSetString(cart, "ldOut", "none"); /* overkill? */
    }
else 
    {
    tg->limitedVis=tvFull;
    tg->limitedVisSet=FALSE;
    }
}

static void mapDiamondUi(int xl, int yl, int xt, int yt, 
			 int xr, int yr, int xb, int yb, 
			 char *name, char *shortLabel)
/* Print out image map rectangle that invokes hgTrackUi. */
{
hPrintf("<AREA SHAPE=POLY COORDS=\"%d,%d,%d,%d,%d,%d,%d,%d\" ", 
	xl, yl, xt, yt, xr, yr, xb, yb);
// change 'hapmapLd' to use parent composite table name
hPrintf("HREF=\"%s?%s=%u&c=%s&g=hapmapLd&i=%s\"", hgTrackUiName(), 
	cartSessionVarName(), cartSessionId(cart), chromName, name);
mapStatusMessage("%s controls", shortLabel);
hPrintf(">\n");
}

int ldTotalHeight(struct track *tg, enum trackVisibility vis)
/* Return total height. Called before and after drawItems. 
 * Must set height, lineHeight, heightPer */ 
{
tg->lineHeight = tl.fontHeight+1;
tg->heightPer  = tg->lineHeight - 1;
if (vis==tvDense||(tg->limitedVisSet&&tg->limitedVis==tvDense))
    tg->height = tg->lineHeight;
else if (winEnd-winStart<500000)
    tg->height = insideWidth/2;
else
    tg->height = 2+(int)((insideWidth/2)*(500000.0/(winEnd-winStart)));
return tg->height;
}

Color ldDiamondColor(struct track *tg, struct vGfx *vg, double score, 
		     double lodScore, boolean isLod)
{
Color *posShades = tg->colorShades;
Color *negShades = tg->altColorShades;

if (abs(score)>1)
    errAbort("Score must be between -1 and 1, inclusive. (score=%.3f)",
	     score);
if (isLod)
    {
    if (lodScore>=2)             /* high LOD */
	{
	if (abs(score)<0.5)     /* high LOD, low D'  -> pink */
	    return ldHighLodLowDprime;
	else                    /* high LOD, high D' -> shades of red*/
	    {
	    /* lodScore has a minimum value of 2 and score has a maximum magnitude of 0.5,
	     * so [lodScore-abs(score)] will have a minimum value of 1.5
	     * subtract 1.5 to get the minimumm of the range.  
	     * Subtract 32 from 255 to get the starting intensity at ldHighLodLowDprime 
	     * Multiply by 2.0 to amplify the difference (reduces range also)
	     * Use min and max to stay within the range [0,255] */
	    int blgr = 255-max(0,min((int)((255-32)*(lodScore-abs(score)-1.5)),255));
	    /* This version is from Mark Daly's group, via Lalitha Krishnan at DCC
	     *    double blgr = (255-32)*2*(1-d);
	     * int blgr =  min(255, 446 * (int)(lodScore - abs(score))); */
	    return vgFindColorIx(vg, 255, blgr, blgr);
	    }
	}
    else if (abs(score)>0.99)   /* high D', low LOD  -> blue */
	return ldHighDprimeLowLod;
    else                        /* no LD */
	return MG_WHITE;
    }
if (score>=0)
    return posShades[(int)(score * (LD_DATA_SHADES-1))];
return negShades[(int)(-score * (LD_DATA_SHADES-1))];
}

void drawDiamond(struct vGfx *vg, 
	 int xl, int yl, int xt, int yt, int xr, int yr, int xb, int yb, 
	 Color fillColor, boolean drawOutline, Color outlineColor)
/* Draw diamond shape. */
{
struct gfxPoly *poly = gfxPolyNew();
gfxPolyAddPoint(poly, xl, yl);
gfxPolyAddPoint(poly, xt, yt);
gfxPolyAddPoint(poly, xr, yr);
gfxPolyAddPoint(poly, xb, yb);
vgDrawPoly(vg, poly, fillColor, TRUE);
if (drawOutline)
    vgDrawPoly(vg, poly, outlineColor, FALSE);
gfxPolyFree(&poly);
}

void ldDrawDiamond(struct track *tg, struct vGfx *vg, int width, 
		   int xOff, int yOff, int i, int j, int k, int l, 
		   double score, double lodScore, char *name, 
		   boolean drawOutline, Color outlineColor, 
		   double scale, boolean isLod, boolean drawMap)
/* Draw and map a single pairwise LD box */
{
Color  color = ldDiamondColor(tg, vg, score, lodScore, isLod);
int    xl    = round((double)(scale*((k+i)/2-winStart))) + xOff;
int    xt    = round((double)(scale*((l+i)/2-winStart))) + xOff;
int    xr    = round((double)(scale*((l+j)/2-winStart))) + xOff;
int    xb    = round((double)(scale*((k+j)/2-winStart))) + xOff;
int    yl    = round((double)(scale*(k-i)/2)) + yOff;
int    yt    = round((double)(scale*(l-i)/2)) + yOff;
int    yr    = round((double)(scale*(l-j)/2)) + yOff;
int    yb    = round((double)(scale*(k-j)/2)) + yOff;

if (yb<=0)
    yb=1;
drawDiamond(vg, xl, yl, xt, yt, xr, yr, xb, yb, color, drawOutline, 
	    outlineColor);
if (drawMap && xt-xl>5 && xb-xl>5)
    mapDiamondUi(xl, yl, xt, yt, xr, yr, xb, yb, name, tg->mapName);
}

void drawNecklace(struct track *tg, int width, int xOff, int yOff, 
		  void *item, struct vGfx *vg, Color outlineColor,
		  int *chromStarts, double *values, double *lodValues, 
		  int arraySize, boolean drawOutline, double scale, 
		  boolean trim, boolean isLod, boolean drawMap)
/* Draw a string of diamonds that represent the pairwise LD
 * values for the current marker */
{
struct ld *ld = item;
int        n  = 0;

if (!isLod)
    lodValues = values;
if (!trim || (chromStarts[0] <= winEnd        /* clip right to triangle */
	      && ld->chromStart >= winStart)) /* clip left to triangle */
    ldDrawDiamond(tg, vg, width, xOff, yOff, ld->chromStart, chromStarts[0], 
		  ld->chromStart, chromStarts[0], values[0], lodValues[0], 
		  ld->name, drawOutline, outlineColor, scale,
		  isLod, drawMap);
for (n=0; n < ld->ldCount-1; n++)
    {
    if ((chromStarts[n]+ld->chromStart)/2 > winEnd) /* left is outside window */
	return;
    if ((chromStarts[n]-chromStarts[0]) > winEnd-winStart) /* bottom is outside window */
	return;
    if (trim && chromStarts[n+1] > winEnd) /* trim right to triangle */
	return;
    if (trim && ld->chromStart < winStart) /* trim left to triangle */
	return;
    if ((chromStarts[0]+chromStarts[n+1])/2 < winStart) /* right is outside window */
	continue;
    ldDrawDiamond(tg, vg, width, xOff, yOff, ld->chromStart, chromStarts[0],
		  chromStarts[n], chromStarts[n+1], values[n], lodValues[n],
		  ld->name, drawOutline, outlineColor, scale,
		  isLod, drawMap);
    }
}

Color *ldFillColors(char *colorString)
/* reuturn the array of colors for the LD diamonds */
{
if (sameString(colorString,"red")) 
    return ldShadesOfRed;
else if (sameString(colorString,"blue"))
    return ldShadesOfBlue;
else if (sameString(colorString,"green"))
    return ldShadesOfGreen;
else
    errAbort("LD fill color must be 'red', 'blue', or 'green'; "
	     "'%s' is not recognized", colorString);
return 0;
}

Color ldOutlineColor(char *outColor)
/* get outline color from cart */
{
if (sameString(outColor,"yellow"))
    return MG_YELLOW;
else if (sameString(outColor,"red"))
    return MG_RED;
else if (sameString(outColor,"blue"))
    return MG_BLUE;
else if (sameString(outColor,"green"))
    return MG_GREEN;
else if (sameString(outColor,"white"))
    return MG_WHITE;
else
    return MG_BLACK;
}

void ldDrawItems(struct track *tg, int seqStart, int seqEnd,
		 struct vGfx *vg, int xOff, int yOff, int width,
		 MgFont *font, Color color, enum trackVisibility vis)
/* Draw item list, one per track. */
{
int        arraySize    = 0;
struct ld *el           = NULL;
int       *chromStarts  = NULL;
double    *values       = NULL;
double    *lodValues    = NULL;
char      *valArray     = cartUsualString(cart, "ldValues", ldValueDefault);
char      *outColor     = cartUsualString(cart, "ldOut",    ldOutDefault);
boolean    drawOutline  = differentString(outColor,"none");
Color      outlineColor = MG_BLACK;
boolean    trim         = cartUsualBoolean(cart, "ldTrim", ldTrimDefault);
boolean    isLod        = FALSE;
boolean    isRsquared   = FALSE;
boolean    isDprime     = FALSE;
double     scale        = scaleForPixels(insideWidth);
int        itemCount    = slCount((struct slList *)tg->items) + 1;
int        i            = 0;
Color      denseColor;
int        heightPer    = tg->heightPer;
int        x;
int        w            = 3;
boolean    drawMap      = ( itemCount<200 ? TRUE :FALSE );

makeLdShades(vg);
if (drawOutline) 
    outlineColor = ldOutlineColor(outColor);

/* choose LD values based on cart settings */
if (sameString(valArray, "lod"))
    isLod = TRUE;
else if (sameString(valArray, "dprime"))
    isDprime = TRUE;
else if (sameString(valArray,"rsquared"))
    isRsquared = TRUE;
else
    errAbort ("LD score value must be 'rsquared', 'dprime', or 'lod'; "
	      "'%s' is not known", valArray);

if ( vis==tvFull && tg->limitedVisSet && tg->limitedVis==tvFull )
    for (el=tg->items; el!=NULL; el=el->next)
	{
	sqlSignedDynamicArray(el->ldStarts, &chromStarts, &arraySize);
	if (isRsquared)
	    sqlDoubleDynamicArray(el->rsquared, &values, &arraySize);
	else if (isDprime)
	    sqlDoubleDynamicArray(el->dprime, &values, &arraySize);
	else if (isLod)
	    {
	    sqlDoubleDynamicArray(el->dprime, &values, &arraySize);
	    sqlDoubleDynamicArray(el->lod, &lodValues, &arraySize);
	    }
	drawNecklace(tg, width, xOff, yOff, el, vg, outlineColor, 
		     chromStarts, values, lodValues, arraySize, 
		     drawOutline, scale, trim, isLod, drawMap);
	}
else if ( vis==tvDense || (tg->limitedVisSet && tg->limitedVis==tvDense) )
    {
    struct ldStats lds[itemCount], *ldsStartPtr=NULL, *ldsEndPtr=NULL;

    /* initialize array to represent SNPs and hold counts */
    for (i=0, el=tg->items; i<itemCount-1 && el!=NULL; i++, el=el->next)
	{
	lds[i].chromStart           = el->chromStart;
	lds[i].n                    = 0;
	lds[i].sumValues            = 0;
	lds[i].sumLodValues         = 0;
	lds[itemCount-1].chromStart = el->chromEnd-1;
	}
    lds[itemCount-1].n              = 0;
    lds[itemCount-1].sumValues      = 0;
    lds[itemCount-1].sumLodValues   = 0;

    /* fill up the bins */
    for (el=tg->items; el!=NULL; el=el->next)
	{
	sqlSignedDynamicArray(el->ldStarts, &chromStarts, &arraySize);
	if (isRsquared)
	    {
	    sqlDoubleDynamicArray(el->rsquared, &values, &arraySize);
	    lodValues=values;
	    }
	else if (isDprime)
	    {
	    sqlDoubleDynamicArray(el->dprime, &values, &arraySize);
	    lodValues=values;
	    }
	else /* isLod */
	    {
	    sqlDoubleDynamicArray(el->dprime, &values, &arraySize);
	    sqlDoubleDynamicArray(el->lod, &lodValues, &arraySize);
	    }
	/* set start pointer for the first element */
	ldsStartPtr = lds;
	while (ldsStartPtr!=NULL && ldsStartPtr->chromStart!=el->chromStart)
	    ldsStartPtr++;
	/* note that the start pointer will remain constant for the  */
	/* remainder of this loop - the end pointer will increment   */	   
	/* end pointer must be greater than the start pointer        */
	if(ldsStartPtr==NULL)
	    errAbort("assert(ldsStartPtr!=NULL) failed.");
	else
	    ldsEndPtr = ldsStartPtr+1;
	if (ldsEndPtr->chromStart >= el->chromEnd)
	    errAbort("assert(ldsEndPtr->chromStart < el->chromEnd) failed.");

	/* Walk through lists, update pointers, and store data in bins */
	for (i=0; i<arraySize; i++)
	    {
	    /* move end pointer until it finds the correct bin. */
	    /* This loop is necessary as some lists have missing data */
	    while ( ldsEndPtr->chromStart != chromStarts[i] && ldsEndPtr != &(lds[itemCount-1]) )
		ldsEndPtr++;
	    ldsStartPtr->n++;
	    ldsStartPtr->sumValues     += values[i];
	    ldsStartPtr->sumLodValues  += lodValues[i];
	    ldsEndPtr->n++;
	    ldsEndPtr->sumValues       += values[i];
	    ldsEndPtr->sumLodValues    += lodValues[i];
	    }
	}

    /* write out the results */
    vgBox(vg, insideX, yOff, insideWidth, tg->height-1, shadesOfGray[2]);
    for (i=0; i<itemCount; i++)
	{
	if (lds[i].n==0)
	    {
	    lds[i].n=1;
	    lds[i].sumValues=0;
	    lds[i].sumLodValues=0;
	    /*  come back and check on this later
		printf("<BR>Empty bin: %d %d<BR>", (lds[i]).chromStart, (lds[i]).n);
	    */
	    }
	if (lds[i].chromStart<winStart || lds[i].chromStart>winEnd)
	    continue;
	denseColor = ldDiamondColor(tg, vg, lds[i].sumValues/lds[i].n, 
				    lds[i].sumLodValues/lds[i].n, isLod);
	x = round((lds[i].chromStart-winStart)*scale) + xOff - w/2;
	vgBox(vg, x, yOff, w, heightPer, denseColor);
	if (drawOutline) 
	    {
	    vgLine(vg, x-1, yOff,             x+w, yOff,             outlineColor);
	    vgLine(vg, x-1, yOff+heightPer-1, x+w, yOff+heightPer-1, outlineColor);
	    vgLine(vg, x-1, yOff,             x-1, yOff+heightPer-1, outlineColor);
	    vgLine(vg, x+w, yOff,             x+w, yOff+heightPer-1, outlineColor);
	    }
	}
    }
else
    errAbort("visibility '%s' not supported yet.", hStringFromTv(vis));
}

void ldFreeItems(struct track *tg)
/* Free item list. */
{
ldFreeList((struct ld**)&tg->items);
}

void ldDrawLeftLabels(struct track *tg, int seqStart, int seqEnd,
	struct vGfx *vg, int xOff, int yOff, int width, int height, 
	boolean withCenterLabels, MgFont *font,
	Color color, enum trackVisibility vis)
/* Draw left labels. */
{
char  label[16];
char *valueString = cartUsualString(cart, "ldValues", ldValueDefault);
char *pop         = tg->mapName;

if (strlen(tg->mapName)>3)
    pop += strlen(tg->mapName) - 3;

if (sameString(valueString,"lod"))
    valueString = cloneString("LOD");
else if (sameString(valueString,"rsquared"))
    valueString = cloneString("R^2");
else if (sameString(valueString,"dprime"))
    valueString = cloneString("D'");
else
    errAbort("%s values are not recognized", valueString);

safef(label, sizeof(label), "LD: %s; %s", pop, valueString);
toUpperN(label, sizeof(label));
vgTextRight(vg, leftLabelX, yOff+tl.fontHeight, leftLabelWidth-1, 
	    tl.fontHeight, color, font, label);
}

void ldMethods(struct track *tg)
/* setup special methods for Linkage Disequilibrium track */
{
if(tg->subtracks != 0) /* Only load subtracks, not top level track. */
    return;
tg->loadItems      = ldLoadItems;
tg->totalHeight    = ldTotalHeight;
tg->drawItems      = ldDrawItems;
tg->freeItems      = ldFreeItems;
tg->drawLeftLabels = ldDrawLeftLabels;
tg->canPack        = FALSE;
tg->colorShades    = ldFillColors(cartUsualString(cart, "ldPos", ldPosDefault));
tg->altColorShades = ldFillColors(cartUsualString(cart, "ldNeg", ldNegDefault));
}

void cnpLoadItems(struct track *tg)
{
struct track *subtrack = NULL;

for (subtrack=tg->subtracks;subtrack!=NULL;subtrack=subtrack->next)
    if(sameString(subtrack->mapName,"cnpIafrate"))
	bedLoadItem(subtrack, "cnpIafrate", (ItemLoader)cnpIafrateLoad);
    else if(sameString(subtrack->mapName,"cnpSebat"))
	bedLoadItem(subtrack, "cnpSebat", (ItemLoader)cnpSebatLoad);
    else if(sameString(subtrack->mapName,"cnpSharp"))
	bedLoadItem(subtrack, "cnpSharp", (ItemLoader)cnpSharpLoad);
    else if(sameString(subtrack->mapName,"cnpFosmid"))
	bedLoadItem(subtrack, "cnpFosmid", (ItemLoader)bedLoad);
}

void cnpFreeItems(struct track *tg)
{
struct track *subtrack = NULL;

for (subtrack=tg->subtracks;subtrack!=NULL;subtrack=subtrack->next)
    if(sameString(tg->mapName,"cnpIafrate"))
	cnpIafrateFreeList((struct cnpIafrate**)&tg->items);
    else if(sameString(tg->mapName,"cnpSebat"))
	cnpSebatFreeList((struct cnpSebat**)&tg->items);
    else if(sameString(tg->mapName,"cnpSharp"))
	cnpSharpFreeList((struct cnpSharp**)&tg->items);
    else if(sameString(subtrack->mapName,"cnpFosmid"))
	bedFreeList((struct bed**)&tg->items);
}

Color cnpItemColor(struct track *tg, void *item, struct vGfx *vg)
{
struct cnpIafrate *cnpIa = item;
struct cnpSebat   *cnpSe = item;
struct cnpSharp   *cnpSh = item;
struct bed        *cnpFo = item;
char *type = NULL;

if(sameString(tg->mapName,"cnpIafrate"))
    type = cnpIa->variationType;
else if(sameString(tg->mapName,"cnpSebat"))
    type = cnpSe->name;
else if(sameString(tg->mapName,"cnpSharp"))
    type = cnpSh->variationType;
else if(sameString(tg->mapName,"cnpFosmid"))
    type = cnpFo->name;

if (sameString(type, "Gain")||type[0]=='I')
    return MG_GREEN;
if (sameString(type, "Loss")||type[0]=='D')
    return MG_RED;
if (sameString(type, "Gain and Loss"))
    return MG_BLUE;
return MG_BLACK;
}

void cnpMethods(struct track *tg)
{
tg->loadItems = cnpLoadItems;
tg->freeItems = cnpFreeItems;
tg->itemColor = cnpItemColor;
tg->itemNameColor = cnpItemColor;
}

