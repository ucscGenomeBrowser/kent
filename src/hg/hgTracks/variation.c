/* variation.c - hgTracks routines that are specific to the SNP and
 * haplotype tracks */

#include "variation.h"

void filterSnpMapItems(struct track *tg, boolean (*filter)(struct track *tg, void *item))
/* Filter out items from track->itemList. */
{
filterSnpItems(tg, filter);
}

void filterSnpItems(struct track *tg, boolean (*filter)(struct track *tg, void *item))
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
    snpMapSourceCart[snpMapSource] = cartUsualString(cart, snpMapSourceStrings[snpMapSource], snpMapSourceDefault[snpMapSource]);
for (snpMapType=0; snpMapType<snpMapTypeCartSize; snpMapType++)
    snpMapTypeCart[snpMapType] = cartUsualString(cart, snpMapTypeStrings[snpMapType], snpMapTypeDefault[snpMapType]);
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
    snpSourceCart[snpSource] = cartUsualString(cart, snpSourceStrings[snpSource], snpSourceDefault[snpSource]);
for (snpMolType=0; snpMolType < snpMolTypeCartSize; snpMolType++)
    snpMolTypeCart[snpMolType] = cartUsualString(cart, snpMolTypeStrings[snpMolType], snpMolTypeDefault[snpMolType]);
for (snpClass=0;   snpClass   < snpClassCartSize; snpClass++)
    snpClassCart[snpClass] = cartUsualString(cart, snpClassStrings[snpClass], snpClassDefault[snpClass]);
for (snpValid=0;   snpValid   < snpValidCartSize; snpValid++)
    snpValidCart[snpValid] = cartUsualString(cart, snpValidStrings[snpValid], snpValidDefault[snpValid]);
for (snpFunc=0;    snpFunc    < snpFuncCartSize; snpFunc++)
    snpFuncCart[snpFunc] = cartUsualString(cart, snpFuncStrings[snpFunc], snpFuncDefault[snpFunc]);
for (snpLocType=0; snpLocType < snpLocTypeCartSize; snpLocType++)
    snpLocTypeCart[snpLocType] = cartUsualString(cart, snpLocTypeStrings[snpLocType], snpLocTypeDefault[snpLocType]);
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
enum   snpColorEnum thisSnpColor = stringArrayIx(snpMapSourceCart[stringArrayIx(el->source,snpMapSourceDataName,snpMapSourceDataNameSize)],snpColorLabel,snpColorLabelSize);

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
char  *snpColorSource = cartUsualString(cart, snpColorSourceDataName[0], snpColorSourceDefault[0]);
char  *validString = NULL;
char  *funcString = NULL;
int    snpValid = 0;
int    snpFunc = 0;

switch (stringArrayIx(snpColorSource, snpColorSourceStrings, snpColorSourceStringsSize))
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
		vgTextRight(vg, textX, y, nameWidth, heightPer, itemNameColor, font, name);
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
		vgTextRight(vg, textX, y, nameWidth, heightPer, itemNameColor, font, name);
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
    color =  shades[lf->grayIx+isXeno];
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
	drawScaledBox(vg, s, e, scale, xOff, y+((tg->heightPer-heightPer)/2), heightPer, blackIndex());
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
