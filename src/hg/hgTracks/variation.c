/* variation.c - hgTracks routines that are specific to the SNP and
 * haplotype tracks */

#include "variation.h"

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
/* return the actual perlegen name, in form xx/yyyy cut off xx/ return yyyy */
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
Color   ldShadesOfGreen[LD_DATA_SHADES+1];
Color   ldShadesOfRed[LD_DATA_SHADES+1];
Color   ldShadesOfBlue[LD_DATA_SHADES+1];
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
ldColorsMade = TRUE;
}

void ldLoadItems(struct track *tg)
/* loadItems loads up items for the chromosome range indicated.   */
{
bedLoadItem(tg, "ld", (ItemLoader)ldLoad);
tg->canPack=FALSE;
tg->limitedVis=TRUE;
tg->visibility=tvFull;
}

static void mapDiamondUi(int xl, int yl, int xr, int yr, 
			 int xt, int yt, int xb, int yb, 
			 char *name, char *shortLabel)
/* Print out image map rectangle that invokes hgTrackUi. */
{
hPrintf("<AREA SHAPE=POLY COORDS=\"%d,%d,%d,%d,%d,%d,%d,%d\" ", 
	xl, yl, xt, yt, xr, yr, xb, yb);
hPrintf("HREF=\"%s?%s=%u&c=%s&g=ld&i=%s\"", hgTrackUiName(), cartSessionVarName(),
                         cartSessionId(cart), chromName, name);
mapStatusMessage("%s controls", shortLabel);
hPrintf(">\n");
}

int ldTotalHeight(struct track *tg, enum trackVisibility vis)
/* Return total height. Called before and after drawItems. 
 * Must set height, lineHeight, heightPer */ 
{
tg->lineHeight          = 0;
tg->heightPer           = 0;
tg->height = min(cartUsualInt(cart, "ldHeight", 200), 400);
return tg->height;
}

int ldCoverage()
{
return min(cartUsualInt(cart, "ldCov", 100000), 500000);
}

double ldSlope(int height, int width)
{
return height*(winEnd-winStart)/(1.0*width*insideWidth);
}

Color ldDiamondColor(struct track *tg, double score)
{
Color *posShades = tg->colorShades;
Color *negShades = tg->altColorShades;
if (abs(score)>1)
    errAbort("Score must be between -1 and 1, inclusive. (score=%.3f)",score);
if (score>=0)
    return posShades[(int)(score * (LD_DATA_SHADES+1))];
return negShades[(int)(-score * LD_DATA_SHADES)];
}

void drawDiamond(struct vGfx *vg, int xl, int yl, int xr, int yr,
        int xt, int yt, int xb, int yb, Color fillColor, Color outlineColor)
/* Draw diamond shape. */
{
struct gfxPoly *poly = gfxPolyNew();
gfxPolyAddPoint(poly, xl, yl);
gfxPolyAddPoint(poly, xt, yt);
gfxPolyAddPoint(poly, xr, yr);
gfxPolyAddPoint(poly, xb, yb);
vgDrawPoly(vg, poly, fillColor, TRUE);
vgDrawPoly(vg, poly, outlineColor, FALSE);
gfxPolyFree(&poly);
}

void ldDrawDiamond(struct track *tg, struct vGfx *vg, int width, int xOff, int yOff, 
		 int i, int j, int k, int l, double score, char *name, 
		 char *shortLabel, boolean drawOutline, Color outlineColor)
/* Draw a single pairwise LD box */
{
double scale  = scaleForPixels(insideWidth);
int    height = ldTotalHeight(tg, tvFull);
double m      = ldSlope(height, ldCoverage());
Color  color  = ldDiamondColor(tg, score);
int    xl,xr,xt,xb,yl,yr,yt,yb;

/* only calculate what is necesary by ignoring items to be 
 * drawn entirely outside the box.  Calculated in order of 
 * 'most likely' violations - left, right, top are first. */
xl=round((double)(scale*((k+i)/2-winStart)))+xOff;
if (xl > xOff+width)
    return; /*  left-most point is to the right of the window */
xr=round((double)(scale*((l+j)/2-winStart)))+xOff;
if (xr < xOff)
    return; /* right-most point is to the left  of the window */
yb=round((double)(scale*m*(k-j)))+yOff;
if (yb > height+yOff)
    return; /*     bottom point is to the top  of the window */

xt = round((double)(scale*((l+i)/2-winStart))) + xOff;
xb = round((double)(scale*((k+j)/2-winStart))) + xOff;
yl = round((double)(scale*m*(k-i))) + yOff;
yr = round((double)(scale*m*(l-j))) + yOff;
yt = round((double)(scale*m*(l-i))) + yOff;

if (xr>=xOff && xl<width+xOff)
    {
    drawDiamond(vg, xl, yl, xr, yr, xt, yt, xb, yb, 
	      color, outlineColor);
    /* this section should be corrected by the slope for the 
     * non-zeroed coordinate,  which would make a polygon 
     * instead of a diamond.  It's a non-trivial problem, 
     * as it could be a 5, 6, 7, or 8 sided polygon */
/*
    if (xl<xOff) xl=xOff; if (xl>width)  xl=width;
    if (xr<xOff) xr=xOff; if (xr>width)  xr=width;
    if (xt<xOff) xt=xOff; if (xt>width)  xt=width;
    if (xb<xOff) xb=xOff; if (xb>width)  xb=width;
    if (yl<yOff) yl=yOff; if (yl>height) yl=height;
    if (yr<yOff) yr=yOff; if (yr>height) yr=height;
    if (yt<yOff) yt=yOff; if (yt>height) yt=height;
    if (yb<yOff) yb=yOff; if (yb>height) yb=height; 
*/
    mapDiamondUi(xl, yl, xt, yt, xr, yr, xb, yb, name, shortLabel);
    }
}

void drawNecklace(struct track *tg, int width, int xOff, int yOff, 
		  void *item, struct vGfx *vg, Color outlineColor,
		  int *chromStarts, double *values, int arraySize,
		  boolean drawOutline)
/* Draw a string of diamonds that represent the pairwise LD
 * values for the current marker */
{
struct ld *ld       = item;
int        n        = 0;
int        coverage = ldCoverage();

for (n=0; n < ld->ldCount-1; n++)
    {
    if (n>0&&chromStarts[n-1]>winEnd) /* clip to triangle */
	return;
    if ((chromStarts[n]-ld->chromStart)/2 > winEnd) /* left is outside window */
	return;
    if ((chromStarts[n]-chromStarts[0]) > coverage) /* bottom is outside window */
	return;
//    if ((chromStarts[n+1]-chromStarts[0])/2 >= winStart)
    ldDrawDiamond(tg, vg, width, xOff, yOff, ld->chromStart, 
		  chromStarts[0], chromStarts[n], chromStarts[n+1], 
		  values[n], ld->name, tg->shortLabel, drawOutline, outlineColor);
    }
}

void ldTransformLods(int arraySize, double *values)
/* take an array of non-negative numbers and transform them to [0,1] */
{
int i;
for (i=0; i<arraySize; i++)
    values[i]=1-exp(-values[i]);
}

void ldDrawItems(struct track *tg, int seqStart, int seqEnd,
		  struct vGfx *vg, int xOff, int yOff, int width, 
		  MgFont *font, Color color, enum trackVisibility vis)
/* Draw item list, one per track. */
{
int        arraySize;
struct ld *el           = NULL;
int       *chromStarts  = NULL;
double    *values       = NULL;
char      *posColor     = cartUsualString(cart, "ldPos",    "red");
char      *negColor     = cartUsualString(cart, "ldNeg",    "blue");
char      *outColor     = cartUsualString(cart, "ldOut",    "white");
char      *valArray     = cartUsualString(cart, "ldValues", "dprime");
Color      outlineColor = MG_BLUE;
boolean    drawOutline  = differentString(outColor,"none");

makeLdShades(vg);

/* get positive color from cart */
if (sameString(posColor,"red"))
    tg->colorShades = ldShadesOfRed;
else if (sameString(posColor,"blue"))
    tg->colorShades = ldShadesOfBlue;
else
    tg->colorShades = ldShadesOfGreen;

/* get negative color from cart */
if (sameString(negColor,"blue"))
    tg->altColorShades = ldShadesOfBlue;
else if (sameString(negColor,"green"))
    tg->altColorShades = ldShadesOfGreen;
else
    tg->altColorShades = ldShadesOfRed;

/* get outline color from cart */
if (drawOutline)
    {
    if (sameString(outColor,"yellow"))
	outlineColor = MG_YELLOW;
    else if (sameString(outColor,"red"))
	outlineColor = MG_RED;
    else if (sameString(outColor,"black"))
	outlineColor = MG_BLACK;
    else if (sameString(outColor,"green"))
	outlineColor = MG_GREEN;
    else if (sameString(outColor,"white"))
	outlineColor = MG_WHITE;
    else
	outlineColor = MG_BLUE;
    }

/* choose values from different arrays based on cart settings */
if (sameString(valArray,"rsquared"))
    for (el=tg->items; el!=NULL; el=el->next)
	{
	sqlSignedDynamicArray(el->ldStarts, &chromStarts, &arraySize);
	if(arraySize != el->ldCount) 
	    errAbort ("%s: arraySize error in ldDrawItems (arraySize=%d, "
		      "el->ldCount=%d<BR>el->ldStarts: %s",
		      el->name,arraySize,el->ldCount,el->ldStarts);
	if (chromStarts[0] < winStart) /* clip to triangle */
	    continue;
	sqlDoubleDynamicArray(el->rsquared, &values, &arraySize);
	if(arraySize != el->ldCount) 
	    errAbort ("%s: arraySize error in ldDrawItems (arraySize=%d, "
		      "el->ldCount=%d<BR>el->rsquared: %s",
		      el->name,arraySize,el->ldCount,el->rsquared);
	drawNecklace(tg, width, xOff, yOff, el, vg, outlineColor, 
		     chromStarts, values, arraySize, drawOutline);
	}
else if (sameString(valArray, "dprime"))
    for (el=tg->items; el!=NULL; el=el->next)
	{
	sqlSignedDynamicArray(el->ldStarts, &chromStarts, &arraySize);
	if(arraySize != el->ldCount) 
	    errAbort ("%s: arraySize error in ldDrawItems (arraySize=%d, "
		      "el->ldCount=%d<BR>el->ldStarts: %s",
		      el->name,arraySize,el->ldCount,el->ldStarts);
	if (chromStarts[0] < winStart) /* clip to triangle */
	    continue;
	sqlDoubleDynamicArray(el->dprime, &values, &arraySize);
	if(arraySize != el->ldCount) 
	    errAbort ("%s: arraySize error in ldDrawItems (arraySize=%d, "
		      "el->ldCount=%d<BR>el->dprime: %s",
		      el->name,arraySize,el->ldCount,el->dprime);
	drawNecklace(tg, width, xOff, yOff, el, vg, outlineColor, 
		     chromStarts, values, arraySize, drawOutline);
	}
else if (sameString(valArray, "lod"))
    for (el=tg->items; el!=NULL; el=el->next)
	{
	sqlSignedDynamicArray(el->ldStarts, &chromStarts, &arraySize);
	if(arraySize != el->ldCount) 
	    errAbort ("%s: arraySize error in ldDrawItems (arraySize=%d, "
		      "el->ldCount=%d<BR>el->ldStarts: %s",
		      el->name,arraySize,el->ldCount,el->ldStarts);
	if (chromStarts[0] < winStart) /* clip to triangle */
	    continue;
	sqlDoubleDynamicArray(el->lod, &values, &arraySize);
	if(arraySize != el->ldCount) 
	    errAbort ("%s: arraySize error in ldDrawItems (arraySize=%d, "
		      "el->ldCount=%d<BR>el->lod: %s",
		      el->name,arraySize,el->ldCount,el->lod);
	/* transform lod values to [0,1] */
	ldTransformLods(arraySize, values);
	drawNecklace(tg, width, xOff, yOff, el, vg, outlineColor, 
		     chromStarts, values, arraySize, drawOutline);
	}
else
    errAbort ("LD score value must be 'rsquared', 'dprime', or 'lod'.  "
	      "'%s' is not known", valArray);
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
/* Draw Left Labels - don't do anything. */
{
}

void ldMethods(struct track *tg)
/* setup special methods for Linkage Disequilibrium track */
{
tg->loadItems      = ldLoadItems;
tg->totalHeight    = ldTotalHeight;
tg->drawItems      = ldDrawItems;
tg->freeItems      = ldFreeItems;
tg->drawLeftLabels = ldDrawLeftLabels;
}
