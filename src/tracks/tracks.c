/* Tracks - this program displays multiple tracks of info associated with
 * a piece of DNA. */
#include "common.h"
#include "memalloc.h"
#include "portable.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "nt4.h"
#include "gdf.h"
#include "fuzzyFind.h"
#include "wormdna.h"
#include "snof.h"
#include "memgfx.h"
#include "cda.h"
#include "xa.h"
#include "cheapcgi.h"
#include "htmshell.h"

#define COLOR_TRACKS   /* undef this for black and white version. */

char **chromNames;
int chromCount;
char *organism = "celegans";

struct trackInput
/* Input that goes from one incarnation of program to another goes here. */
    {
    char *title;
    char *hilite;
    char *dynFa;
    char *dynCda;
    int mouseX, mouseY;
    char *locus;
    char *chrom;
    int chromStart, chromEnd;
    char strand;
    int relStart;
    boolean withCosmids, withGenes, withGenie, withCdna, withEmbryoCdna, 
        withLeftLabels, withCenterLabels, withBriggsae;
    };

static struct trackInput ti;

#define mapSig 0x9f7e3578
enum mapType
/* There's one of these for each type of area in the graphic display
 * that the user can click on. */
    {
    mtEnd,
    mtDoNothing,
    mtDynTitle,
    mtDynTrack,
    mtDynCondensed,
    mtDynLabel,
    mtCosmidTitle,
    mtCosmidTrack,
    mtCosmidCondensed,
    mtCosmidLabel,
    mtGeneTitle,
    mtGeneTrack,
    mtGeneOnBox,
    mtGeneCondensed,
    mtGeneLabel,
    mtCdnaTitle,
    mtCdnaTrack,
    mtCdnaCondensed,
    mtCdnaLabel,
    mtBriggsaeTitle,
    mtBriggsaeTrack,
    mtBriggsaeOnBox,
    mtBriggsaeCondensed,
    mtBriggsaeLabel,
    mtWiseTitle,
    mtWiseTrack,
    mtWiseCondensed,
    mtWiseLabel,
    mtRuler,
    mtOutside,     /* Outside any feature. */
    };

void mapReadString(FILE *f, char **ps)
/* Read string from map file. */
{
short len;
char *s;
mustReadOne(f, len);
if (len == 0)
    s = NULL;
else
    {
    s = needMem(len+1);
    mustRead(f, s, len);
    }
*ps = s;
}

void mapWriteString(FILE *f, char *s)
/* Write string to map file. */
{
short len;

if (s == NULL)
    {
    len = 0;
    writeOne(f, len);
    }
else
    {
    len = strlen(s);
    writeOne(f, len);
    mustWrite(f, s, len);
    }
}

void mapWriteBox(FILE *f, enum mapType mt, int x, int y, int width, int height, char *name, char strand)
/* Write out one hit box. */
{
writeOne(f, mt);
writeOne(f, x);
writeOne(f, y);
writeOne(f, width);
writeOne(f, height);
mapWriteString(f, name);
writeOne(f, strand);
}

void mapReadBox(FILE *f, enum mapType *mt, int *x, int *y, int *width, int *height, char **name, char *strand)
/* Read in one hit box. */
{
mustReadOne(f, *mt);
mustReadOne(f, *x);
mustReadOne(f, *y);
mustReadOne(f, *width);
mustReadOne(f, *height);
mapReadString(f, name);
mustReadOne(f, *strand);
}

boolean mapScanForHit(FILE *f, int mouseX, int mouseY, 
    enum mapType *mt, int *x, int *y, int *width, int *height, char **name, char *strand)
/* Scan file for one that hits mouse.  Return TRUE if get one. */
{
for (;;)
    {
    mapReadBox(f, mt, x, y, width, height, name, strand);
    if (*mt == mtEnd)
        return FALSE;
    if (mouseX >= *x && mouseX < *x + *width 
        && mouseY >= *y && mouseY < *y + *height)
        {
        return TRUE;
        }
    freez(name);
    }
}

void mapWriteHead(FILE *f, char *title, bits32 width, bits32 height,
    char *where, char *chrom, int chromStart, int chromEnd, char strand,
    char *hilitRange, int relStart)
{
bits32 sig = mapSig;
writeOne(f, sig);
mapWriteString(f, title);
writeOne(f,width);
writeOne(f,height);
mapWriteString(f, where);
mapWriteString(f, chrom);
writeOne(f, chromStart);
writeOne(f, chromEnd);
writeOne(f, strand);
mapWriteString(f, hilitRange);
writeOne(f, relStart);
}

void mapReadHead(FILE *f, char **title, bits32 *width, bits32 *height,
    char **where, char **chrom, int *chromStart, int *chromEnd, char *strand,
    char **hilitRange, int *relStart)
{
bits32 sig;
char *chromName;
mustReadOne(f, sig);
if (sig != mapSig)
    errAbort("Bad map file");
mapReadString(f, title);
mustReadOne(f,*width);
mustReadOne(f,*height);
mapReadString(f, where);

mapReadString(f, &chromName);
*chrom = wormOfficialChromName(chromName);
freeMem(chromName);

mustReadOne(f, *chromStart);
mustReadOne(f, *chromEnd);
mustReadOne(f, *strand);
mapReadString(f, hilitRange);
mustReadOne(f, *relStart);
}

char dominantStrand(struct wormFeature *geneList)
/* Figure out most commonly used strand in list. */
{
int plusCount = 0;
int minusCount = 0;
struct wormFeature *gene;

for (gene = geneList; gene != NULL; gene = gene->next)
    {
    if (gene->typeByte == '+')
        ++plusCount;
    else if (gene->typeByte == '-')
        ++minusCount;
    }
if (plusCount > minusCount)
    return '+';
else if (minusCount > plusCount)
    return '-';
else
    return '.';
}

void strandFixupPair(int start, int end, int *retStart, int *retEnd)
/* Fixup position of start and end if necessary if on - strand. */
{
if (ti.strand == '-')
    {
    int dnaSize = ti.chromEnd - ti.chromStart;
    *retStart = reverseOffset(end - ti.chromStart, dnaSize) + 1 + ti.chromStart;
    *retEnd = reverseOffset(start - ti.chromStart, dnaSize) + 1 + ti.chromStart;
    }
else
    {
    *retStart = start;
    *retEnd = end;
    }
}

void fixOffsets(struct gdfGene *gene, long dnaStart, long dnaEnd,
    char dnaStrand, long geneStart, long geneEnd)
/* Gdf offsets come in (aargh) offsets into the DNA in a bite sized
 * gff file. Here we first convert them to chromosome offsets, and
 * then into our local DNA offsets. */
{
long gffStart, gffEnd;

gdfGeneExtents(gene, &gffStart, &gffEnd);
gdfOffsetGene(gene, -gffStart + geneStart - dnaStart);
if (dnaStrand == '-')
    gdfRcGene(gene, dnaEnd - dnaStart);
}


void drawHilite(struct memGfx *mg, int xOff, int yOff, int width, int height, 
    char *hiString)
/* Display hilite areas. */
{
int x1, x2, w;
int start, end;
char *s = cloneString(hiString);
char *words[256];
int wordCount;
char *parts[3];
int partCount;
int hiStart, hiEnd;
int baseWidth = ti.chromEnd - ti.chromStart;
int i;

mgSetClip(mg, xOff, yOff, width, height);
wordCount = chopString(s, ",", words, ArraySize(words));
for (i=0; i<wordCount; ++i)
    {
    partCount = chopString(words[i], " \t-", parts, ArraySize(parts));
    if (partCount != 2 || !isdigit(parts[0][0]) || !isdigit(parts[1][0]))
        errAbort("Badly formatted hilite string %s", hiString);
    hiStart = atoi(parts[0]);
    hiEnd = atoi(parts[1]);
    strandFixupPair(hiStart, hiEnd, &start, &end);
    x1 = roundingScale(start-ti.chromStart, width, baseWidth) + xOff;
    x2 = roundingScale(end-ti.chromStart, width, baseWidth) + xOff;
    w = x2-x1;
    if (w < 1) w = 1;
    mgDrawBox(mg, x1, yOff, w, height, MG_CYAN);
    }
mgUnclip(mg);
freeMem(s);
}

int writeCenterLabel(boolean compactDisplay, FILE *mapFile, struct memGfx *mg, int x, int y, int width, int height,
    MgFont *font, Color color, char *label, enum mapType mt)
/* Write a centered label. */
{
char text[80];
if (ti.withCenterLabels)
    {
    sprintf(text, "%s%s",
        (compactDisplay ? "Condensed " : ""), label);
    mgTextCentered(mg, x, y+1, width, height-1, color, font, text);
    mapWriteBox(mapFile, mt, x, y, width, height, "", '.');
    y += height;
    }
return y;
}

struct wormFeature *stripNamelessClusters(struct wormFeature *oldList)
/* Remove nameless clusters from features list. */
{
struct wormFeature *newList = NULL, *el, *next;

el = oldList;
for (;;)
    {
    if (el == NULL)
        break;
    next = el->next;
    if (wormIsNamelessCluster(el->name))
        freeMem(el);
    else
        slAddHead(&newList, el);
    el = next;
    }
slReverse(&newList);
return newList;
}

char *abbreviateToFit(char *longString, MgFont *font, int width)
/* Return a string abbreviated enough to fit into space. */
{
int textWidth;
int charWidth;
static char abrv[33];
int charCount;
static int chopSize[] = {32, 16, 8, 6, 5, 4, 3, 2, 1};
int cs;
int i;

/* If string is empty or width is too small for one character return null. */
if (longString == NULL || longString[0] == 0)
    return NULL;
charWidth = mgFontWidth(font, longString, 1);
if (charWidth > width)
    return NULL;

/* If have enough space, return original unabbreviated string. */
textWidth = mgFontStringWidth(font, longString);
if (textWidth <= width)
    return longString;

/* Ok, chop it down into abrv */
charCount = strlen(longString);
for (i=0; i<ArraySize(chopSize); ++i)
    {
    cs = chopSize[i];
    if (cs > charCount)
        continue;
    textWidth = mgFontWidth(font, longString, cs);
    if (textWidth <= width)
        {
        strncpy(abrv, longString, cs);
        return abrv;
        }
    }
assert(FALSE);
return NULL;
}


struct trackGroup
/* Structure that displays a group of tracks. */
    {
    struct trackGroup *next;   /* Next on list. */
    char *checkBoxName;        /* Name of cgiBoolean that turns us on. */
    char *longLabel;           /* Long label to put in center. */
    char *shortLabel;          /* Short label to put on side. */
    enum mapType labelType;    /* What to do when they click on left label. */
    enum mapType trackType;    /* What to do when they click on expanded track. */
    enum mapType condensedType;/* What to do when they click on condensed track. */ 
    enum mapType titleType;    /* What to do when they click on center label. */
    struct rgbColor color;     /* Main color. */
    struct rgbColor grayColor; /* Dimmer color. */

    Color ixColor;              /* Indexed color (calculated by drawTracks) */
    Color ixGrayColor;          /* Indexed dim color (calculated by drawTracks) */

    void (*loadItems)(struct trackGroup *tg, boolean isCondensed, char *chrom, int start, int end);
        /* loadItems loads up items for the chromosome range indicated.  It also usually sets the
         * following variables.  */ 
    void *items;               /* Some type of slList of items. */

    char *(*itemName)(struct trackGroup *tg, void *item);
        /* Return name of one of a member of items above. */

    int (*totalHeight)(struct trackGroup *tg, MgFont *font);
        /* Return total height. Called before and afer drawItems. 
         * Must set the following variables. */
    int height;                /* Total height - must be set by above call. */
    int lineHeight;            /* Height per track including border. */
    int heightPer;             /* Height per track minus border. */

    void (*drawItems)(struct trackGroup *tg, FILE *mapFile, int chromStart, int chromEnd,
        struct memGfx *mg, int xOff, int yOff, int width, 
        MgFont *font, Color color, Color grayColor);
        /* Draw item list, one per track. */
    void (*drawCondensedItems)(struct trackGroup *tg, FILE *mapFile, int chromStart, int chromEnd,
        struct memGfx *mg, int xOff, int yOff, int width, int height, 
        MgFont *font, Color color, Color grayColor);
        /* Draw item list all condensed onto one track. */
    
    void (*freeItems)(struct trackGroup *tg, boolean isCondensed);
        /* Free item list. */

    void *customPt;            /* Misc pointer variable unique to group. */
    int customInt;             /* Misc int variable unique to group. */
    int subType;               /* Variable to say what subtype this is for similar groups
                                * to share code. */
    };

static boolean tgLoadNothing(){return TRUE;}
static void tgDrawNothing(){}
static void tgFreeNothing(){}
static char *tgNoName(){return"";}

static int tgUsualHeight(struct trackGroup *tg, MgFont *font)
/* Most track groups will use this to figure out the height they use. */
{
tg->lineHeight = mgFontLineHeight(font);
tg->heightPer = tg->lineHeight - 1;
tg->height = slCount(tg->items) * tg->lineHeight;
return tg->height;
}

static void tgDrawFeatures(struct trackGroup *tg, FILE *mapFile, int chromStart, int chromEnd,
        struct memGfx *mg, int xOff, int yOff, int width, 
        MgFont *font, Color color, Color grayColor)
/* Draw a simple features list. */
{
int x1,x2,w;
int baseWidth = chromEnd - chromStart;
double scaleFactor = (double)width/baseWidth;
struct wormFeature *feat;
char *name;

int y = yOff;
for (feat = tg->items; feat != NULL; feat = feat->next)
    {
    int start, end;
    strandFixupPair(feat->start, feat->end, &start, &end);
    x1 = (int)(scaleFactor * (start-chromStart)) + xOff;
    x2 = (int)(scaleFactor * (end-chromStart)) + xOff;

    /* Clip so that centered text tends to be in display. */
    if (x1 < xOff)
        x1 = xOff;
    if (x2 > xOff + width)
        x2 = xOff + width;
    w = x2-x1;

    mgDrawBox(mg, x1, y, w, tg->heightPer, color);
    if ((name = abbreviateToFit(feat->name, font, w)) != NULL)
        {
        mgTextCentered(mg, x1, y+1, w, tg->heightPer, MG_WHITE, font, feat->name); 
        }
    mapWriteBox(mapFile, tg->trackType, xOff, y, width, tg->lineHeight, feat->name, '+');
    y += tg->lineHeight;
    }
}

char *tgFeatureName(struct trackGroup *tg, void *item)
/* Return name of feature. */
{
struct wormFeature *feat = item;
return feat->name;
}

static void tgDrawCondensedFeatures(struct trackGroup *tg, FILE *mapFile, int chromStart, int chromEnd,
        struct memGfx *mg, int xOff, int yOff, int width, int height,
        MgFont *font, Color color, Color grayColor)
/* Draw a simple features list in condensed format. */
{
int x1,x2,w;
struct wormFeature *feat;
int baseWidth = chromEnd - chromStart;
double scaleFactor = (double)width/baseWidth;

for (feat = tg->items; feat != NULL; feat = feat->next)
    {
    int start, end;
    strandFixupPair(feat->start, feat->end, &start, &end);
    x1 = (int)(scaleFactor * (start-chromStart));
    x2 = (int)(scaleFactor * (end-chromStart));
    w = x2-x1;
    if (w < 1) w = 1;
    mgDrawBox(mg, x1+xOff, yOff, w, height, color);
    }
mapWriteBox(mapFile, tg->condensedType, xOff, yOff, width, height, "", '+');
}

static void tgFreeFeatures(struct trackGroup *tg, boolean isCondensed)
/* Free simple fireatures list. */
{
slFreeList(&tg->items);
}

void tgSetDefaults(struct trackGroup *tg)
/* Set up reasonable values for trackGroup object. */
{
zeroBytes(tg, sizeof(*tg));
tg->checkBoxName = "";
tg->longLabel = "";
tg->shortLabel = "";
tg->labelType = mtDoNothing;
tg->trackType = mtDoNothing;
tg->condensedType = mtDoNothing;
tg->titleType = mtDoNothing;
tg->color.r = tg->color.g = tg->color.b = 0;
tg->grayColor.r = tg->grayColor.g = tg->grayColor.b = 140;
tg->loadItems = NULL;
tg->itemName = tgFeatureName;
tg->totalHeight = tgUsualHeight;
tg->drawItems = tgDrawFeatures;
tg->drawCondensedItems = tgDrawCondensedFeatures;
tg->freeItems = tgFreeFeatures;
};

void cosmidLoadItems(struct trackGroup *tg, boolean isCondensed, char *chrom, int start, int end)
{
tg->items = wormCosmidsInRange(chrom, start, end);
}

struct trackGroup *getCosmidTg()
/* Get trackGroup for cosmids. */
{
static struct trackGroup tg;
tgSetDefaults(&tg);
tg.checkBoxName = "cosmids";
tg.longLabel = "Cosmids";
tg.shortLabel = "Cosmids";
tg.labelType = mtCosmidLabel;
tg.trackType = mtCosmidTrack;
tg.condensedType = mtCosmidCondensed;
tg.titleType = mtCosmidTitle;
#ifdef COLOR_TRACKS
tg.color.r = 180;
tg.color.g = 0;
tg.color.b = 0;
#else
tg.grayColor.r = 140;
tg.grayColor.g = 140;
tg.grayColor.b = 140;
#endif /* COLOR_TRACKS */
tg.loadItems = cosmidLoadItems;
return &tg;
}

char *tgCdnaName(struct trackGroup *tg, void *item)
/* Return name of feature. */
{
struct cdaAli *cda = item;
return cda->name;
}

void cdnaLoadItems(struct trackGroup *tg, boolean isCondensed, char *chrom, int start, int end)
/* Get all cdna items - both mixed and embryo. */
{
if (isCondensed)
    tg->items = wormCdnasInRange(ti.chrom, ti.chromStart, ti.chromEnd);
else
    {
    struct cdaAli *aliList;
    aliList = wormCdaAlisInRange(ti.chrom, ti.chromStart, ti.chromEnd);
    cdaCoalesceBlocks(aliList);
    tg->items = aliList;
    }
}

static void cdnaDrawItems(struct trackGroup *tg, FILE *mapFile, int chromStart, int chromEnd,
        struct memGfx *mg, int xOff, int yOff, int width, 
        MgFont *font, Color color, Color grayColor)
/* Draw cDNA aligment items. */
{
struct cdaAli *cda;
char dirChar;
if (ti.strand == '-')
    cdaRcList(tg->items, chromStart, chromEnd-chromStart);
for (cda = tg->items; cda != NULL; cda = cda->next)
    {
    dirChar =  (tg->subType ? 0 : cdaDirChar(cda, ti.strand));
    
    /* Show aligments. */
    cdaShowAlignmentTrack(mg, xOff, yOff, width, tg->heightPer, color, grayColor,
        chromEnd-chromStart, chromStart, cda, dirChar);

    /* Define area for image map. */
    mapWriteBox(mapFile, tg->trackType, xOff, yOff, width, tg->lineHeight, cda->name, '.');
    yOff += tg->lineHeight;
    }
}

static void cdnaFreeItems(struct trackGroup *tg, boolean isCondensed)
/* Free up items of cdna. */
{
if (isCondensed)
    slFreeList(&tg->items);
else
    cdaFreeAliList((struct cdaAli**)(&tg->items));
}

struct trackGroup *getCdnaTg()
/* Get trackGroup for cDNA (mixed). */
{
static struct trackGroup tg;
static boolean initted = FALSE;
if (!initted)
    {
    initted = TRUE;
    tgSetDefaults(&tg);
    tg.checkBoxName = "cDNA";
    tg.longLabel = "cDNA Alignments";
    tg.shortLabel = "cDNA/ESTs";
    tg.labelType = mtCdnaLabel;
    tg.trackType = mtCdnaTrack;
    tg.condensedType = mtCdnaCondensed;
    tg.titleType = mtCdnaTitle;
    tg.loadItems = cdnaLoadItems;
    tg.itemName = tgCdnaName;
    tg.drawItems = cdnaDrawItems;
    tg.freeItems = cdnaFreeItems;
    }
return &tg;
}

void separateEmbryonicAli(struct cdaAli *inCda, struct cdaAli **retMixedCda, struct cdaAli **retEmbCda)
/* Separate embryonic and non-embryonic alignments */
{
struct cdaAli *mixedCda = NULL, *embCda = NULL, *cda, *next;

for (cda = inCda; cda != NULL; )
    {
    next = cda->next;
    if (cda->isEmbryonic)
        {
        slAddHead(&embCda, cda);
        }
    else
        {
        slAddHead(&mixedCda, cda);
        }
    cda = next;
    }
slReverse(&embCda);
slReverse(&mixedCda);
*retEmbCda = embCda;
*retMixedCda = mixedCda;
}

void separateEmbryonicFeat(struct wormFeature *inFeat, struct wormFeature **retMixedFeat, struct wormFeature **retEmbFeat)
/* Separate embryonic and non-embryonic features */
{
struct wormFeature *mixedFeat = NULL, *embFeat = NULL, *feat, *next;

for (feat = inFeat; feat != NULL; )
    {
    next = feat->next;
    if (feat->typeByte)
        {
        slAddHead(&embFeat, feat);
        }
    else
        {
        slAddHead(&mixedFeat, feat);
        }
    feat = next;
    }
slReverse(&embFeat);
slReverse(&mixedFeat);
*retEmbFeat = embFeat;
*retMixedFeat = mixedFeat;
}




void embryonicCdnaLoadItems(struct trackGroup *tg, boolean isCondensed, char *chrom, int start, int end)
/* Get all cdna items - both mixed and embryo. */
/* This actually steals them from cdnaTrackGroup. */
{
struct trackGroup *cdnaTg = getCdnaTg();
if (cdnaTg->items == NULL)
    cdnaTg->loadItems(cdnaTg, isCondensed, chrom, start, end);

if (isCondensed)
    {
    separateEmbryonicFeat(cdnaTg->items, (struct wormFeature**)(&cdnaTg->items),
        (struct wormFeature**)(&tg->items) );
    }
else
    {
    separateEmbryonicAli(cdnaTg->items, (struct cdaAli **)(&cdnaTg->items),
        (struct cdaAli **)(&tg->items) );
    }
}

struct trackGroup *getEmbryonicCdnaTg()
/* Get trackGroup for embryonic cDNA.  For this to work, must be on after 
 * regular cDNA */
{
static struct trackGroup tg;
tgSetDefaults(&tg);
tg.checkBoxName = "embryoCdna";

tg.longLabel = "Embryonic cDNA Alignments";
tg.shortLabel = "Embryo cDNA";
tg.labelType = mtCdnaLabel;
tg.trackType = mtCdnaTrack;
tg.condensedType = mtCdnaCondensed;
tg.titleType = mtCdnaTitle;
tg.loadItems = embryonicCdnaLoadItems;
tg.itemName = tgCdnaName;
tg.drawItems = cdnaDrawItems;
tg.freeItems = cdnaFreeItems;

#ifdef COLOR_TRACKS
tg.color.r = 20;
tg.color.g = 110;
tg.color.b = 20;
tg.grayColor.r = 128;
tg.grayColor.g = 170;
tg.grayColor.b = 128;
#else
tg.grayColor.r = 140;
tg.grayColor.g = 140;
tg.grayColor.b = 140;
#endif /* COLOR_TRACKS */
return &tg;
}

void dynCdnaLoadItems(struct trackGroup *tg, boolean isCondensed, char *chrom, int start, int end)
/* Get all cdna item that was pasted in. */
{
struct cdaAli *dynList = NULL, *cda;
FILE *cdaFile = cdaOpenVerify(ti.dynCda);

/* Load alignments from pasted file. */
while ((cda = cdaLoadOne(cdaFile)) != NULL)
    {
    if (sameString(chromNames[cda->chromIx], ti.chrom) && 
        !(ti.chromStart >= cda->chromEnd || 
        ti.chromEnd <= cda->chromStart ))
        {
        slAddHead(&dynList, cda);
        }
    else
        {
        cdaFreeAli(cda);
        }
    }
slReverse(&dynList);
fclose(cdaFile);

if (isCondensed)
    {
    /* If condensed convert alignments to features. */
    struct wormFeature *list = NULL, *el;
    for (cda = dynList; cda != NULL; cda = cda->next)
        {
        el = newWormFeature("", chromNames[cda->chromIx], cda->chromStart, cda->chromEnd, 0);
        slAddHead(&list, el);
        }
    tg->items = list;
    cdaFreeAliList(&dynList);
    }
else
    {
    /* Else just store alignment list. */
    tg->items = dynList;
    }
}

struct trackGroup *getDynCdnaTg()
/* Get trackGroup for WormAlign cDNA.*/
{
static struct trackGroup tg;
tgSetDefaults(&tg);
tg.checkBoxName = "dynCda";

tg.longLabel = "WormAlign";
tg.shortLabel = "WormAlign";
tg.labelType = mtDynLabel;
tg.trackType = mtDynTrack;
tg.condensedType = mtDynCondensed;
tg.titleType = mtDynTitle;
tg.loadItems = dynCdnaLoadItems;
tg.itemName = tgCdnaName;
tg.drawItems = cdnaDrawItems;
tg.freeItems = cdnaFreeItems;

#ifdef COLOR_TRACKS
tg.color.r = 128;
tg.color.g = 0;
tg.color.b = 128;
tg.grayColor.r = 160;
tg.grayColor.g = 128;
tg.grayColor.b = 160;
#else
tg.grayColor.r = 140;
tg.grayColor.g = 140;
tg.grayColor.b = 140;
#endif /* COLOR_TRACKS */

tg.subType = TRUE;
return &tg;
}

struct geneGroupCustom
/* This helps manage special needs of groups which display
 * splicing diagrams. */
    {
    char *gdfDir;
    struct wormGdfCache *gdfCache;
    };

static int geneTotalHeight(struct trackGroup *tg, MgFont *font)
/* Most track groups will use this to figure out the height they use. */
{
tg->lineHeight = mgFontLineHeight(font)*2;
tg->heightPer = tg->lineHeight - 1;
tg->height = slCount(tg->items) * tg->lineHeight;
return tg->height;
}

struct gdfGene *loadGdfs(struct wormFeature *featList, struct geneGroupCustom *custom)
/* Make a gdfGene list corresponding to featList. */
{
struct gdfGene *list = NULL, *el;
struct wormFeature *name;
for (name = featList; name != NULL; name = name->next)
    {
    el = wormGetSomeGdfGene(name->name, custom->gdfCache);
    slAddHead(&list, el);
    }
slReverse(&list);
return list;
}


static void geneDrawItems(struct trackGroup *tg, FILE *mapFile, int chromStart, int chromEnd,
        struct memGfx *mg, int xOff, int yOff, int width, 
        MgFont *font, Color color, Color grayColor)
/* Draw gene splicing diagram items. */
{
struct geneGroupCustom *custom = tg->customPt;
int baseWidth = chromEnd - chromStart;
struct wormFeature *feat;
struct wormFeature *featList = tg->items;
struct gdfGene *geneList = loadGdfs(featList, custom);
struct gdfGene *gene;
int y = yOff;
int heightPer = tg->heightPer;
int lineHeight = tg->lineHeight;

for (gene = geneList, feat = featList; gene != NULL; gene = gene->next, feat = feat->next)
    {
    boolean relRc = (ti.strand != gene->strand);
    int exonInc = (relRc ? -1 : 1);
    int exonIx = (relRc ? gene->dataCount/2 : 1);
    struct gdfDataPoint *s;
    int i;
    int midY = y + heightPer/2;
    int startX = 0x7fffffff;
    int endX = -startX;

    fixOffsets(gene, chromStart, chromEnd, ti.strand, feat->start, feat->end);
    for (i=1; i<gene->dataCount-1; i += 2)
        {
        int x1, x2;
        int midX;
        s = &gene->dataPoints[i];
        x1 = roundingScale(s[0].start, width, baseWidth) + xOff;
        x2 = roundingScale(s[1].start, width, baseWidth) + xOff;
        midX = (x1+x2)/2;
        mgDrawLine(mg, x1, midY, midX, y, color);
        mgDrawLine(mg, midX, y, x2, midY, color);
        }
    for (i=0; i<gene->dataCount; i+=2)
        {
        int x1, x2, w, h;
	char tbuf[5];
	int twid;

        /* Draw exon box. */
        s = &gene->dataPoints[i];
        x1 = roundingScale(s[0].start, width, baseWidth) + xOff;
        x2 = roundingScale(s[1].start, width, baseWidth) + xOff;
        w = x2-x1;
        if (w < 1) w = 1;
        h = y+heightPer-midY;
        mgDrawBox(mg, x1, midY, w, h, color);

      	/* Draw exon number in middle if exon is big enough. */
	sprintf(tbuf, "%d", exonIx);
	twid = mgFontStringWidth(font, tbuf);
	if (w >= twid)
	    {
	    mgTextCentered(mg, x1, midY, w, h,
	       MG_WHITE, font, tbuf); 
	    }
        exonIx += exonInc;
        
        /* Keep track of actual gene boundarys in pixel coordinates. */
        if (startX > x1)
            startX = x1;
        if (endX < x2)
            endX = x2;
        }
    mapWriteBox(mapFile, tg->subType, startX, y, endX-startX, lineHeight, gene->name, gene->strand);
    mapWriteBox(mapFile, tg->trackType, xOff, y, width, lineHeight, gene->name, gene->strand);

    y += lineHeight;
    }
gdfFreeGeneList(&geneList);
}

void geneLoadItems(struct trackGroup *tg, boolean isCondensed, char *chrom, int start, int end)
/* Get gene names that intersect segment. */
{
struct geneGroupCustom *custom = tg->customPt;
tg->items = stripNamelessClusters(
    wormSomeGenesInRange(ti.chrom, ti.chromStart, ti.chromEnd,custom->gdfDir));
}


struct trackGroup *getAceGenesTg()
/* Get track group for ACE genes. */
{
static struct trackGroup tg;
static struct geneGroupCustom custom;

static boolean initted = FALSE;
if (!initted)
    {
    initted = TRUE;
    tgSetDefaults(&tg);
    tg.checkBoxName = "spliDi";

    tg.longLabel = "AceDB Gene Predictions";
    tg.shortLabel = "AceDB Genes";
    tg.labelType = mtGeneLabel;
    tg.trackType = mtGeneTrack;
    tg.subType = mtGeneOnBox;
    tg.condensedType = mtGeneCondensed;
    tg.titleType = mtGeneTitle;
    tg.loadItems = geneLoadItems;
    tg.itemName = tgFeatureName;
    tg.totalHeight = geneTotalHeight;
    tg.drawItems = geneDrawItems;
    tg.drawCondensedItems = tgDrawCondensedFeatures;
    tg.freeItems = tgFreeFeatures;

#ifdef COLOR_TRACKS
    tg.color.r = 0;
    tg.color.g = 0;
    tg.color.b = 200;
#endif
    
    custom.gdfDir = wormSangerDir();
    custom.gdfCache = &wormSangerGdfCache;
    tg.customPt = &custom;
    }
return &tg;
}

boolean genieExists()
/* Return TRUE if genie predictions are in database. */
{
char testFile[512];
sprintf(testFile, "%sgenes.gdf", wormGenieDir());
return fileExists(testFile);
}

struct trackGroup *getGenieGenesTg()
/* Get track group for ACE genes. */
{
static struct trackGroup tg;
static struct geneGroupCustom custom;

static boolean initted = FALSE;
if (!initted)
    {
    initted = TRUE;
    tgSetDefaults(&tg);
    tg.checkBoxName = "genie";

    tg.longLabel = "Genie Gene Predictions";
    tg.shortLabel = "Genie Genes";
    tg.labelType = mtGeneLabel;
    tg.trackType = mtGeneTrack;
    tg.subType = mtGeneOnBox;
    tg.condensedType = mtGeneCondensed;
    tg.titleType = mtGeneTitle;
    tg.loadItems = geneLoadItems;
    tg.itemName = tgFeatureName;
    tg.totalHeight = geneTotalHeight;
    tg.drawItems = geneDrawItems;
    tg.drawCondensedItems = tgDrawCondensedFeatures;
    tg.freeItems = tgFreeFeatures;

#ifdef COLOR_TRACKS
    tg.color.r = 160;
    tg.color.g = 110;
    tg.color.b = 0;
#endif
    
    custom.gdfDir = wormGenieDir();
    custom.gdfCache = &wormGenieGdfCache;
    tg.customPt = &custom;
    }
return &tg;
}


static void getHidAlignedToTarget(char *hSym, char *tSym, int symCount,
    char **retHid, int *retCount)
/* Make a copy of hidden symbols omitting those that correspond to
 * inserts in target. */
{
char *hid = needMem(symCount);
int hidCount = 0;
int i;

for (i=0; i<symCount; ++i)
    {
    if (tSym[i] != '-')
        hid[hidCount++] = hSym[i];
    }
*retHid = hid;
*retCount = hidCount;
}

static void xenoDraw(struct memGfx *mg, int x, int w, int y, int h, 
    Color strongColor, Color weakColor, struct xaAli *xa)
/* Draw xeno-alignment in the indicated box */
{
int startIx = -1;
enum state {skip, weak, strong} state = skip, lastState = skip;
int i;
char *hSym = xa->hSym;
int symCount = xa->symCount;
int hidCount;
char *hid;
char c;
Color col;
int x1, x2;

getHidAlignedToTarget(hSym, xa->tSym, symCount, &hid, &hidCount);
if (ti.strand == '-')
    reverseBytes(hid, hidCount);
for (i=0; i<=hidCount; ++i)
    {
    c = hid[i];
    if (i == hidCount)
        state = skip;
    else
        {
        switch (c)
            {
            case 'Q':
            case 'T':
                state = skip;
                break;
            case '1':
            case '2':
            case '3':
            case 'H':
                state = strong;
                break;
            case 'L':
                state = weak;
                break;
            }
        }    
    if (state != lastState)
        {
        if (lastState != skip)
            {
            x1 = roundingScale(startIx, w, hidCount);
            x2 = roundingScale(i, w, hidCount);
            col = (lastState == strong ? strongColor : weakColor);
            mgDrawBox(mg, x + x1, y, x2-x1, h, col);
            }
        lastState = state;
        startIx = i;
        }
    }
freeMem(hid);
}

static void briggsaeDrawItems(struct trackGroup *tg, FILE *mapFile, int chromStart, int chromEnd,
        struct memGfx *mg, int xOff, int yOff, int width, 
        MgFont *font, Color color, Color grayColor)
/* Draw a xeno alignment list. */
{
int x1,x2,w;
int baseWidth = chromEnd - chromStart;
double scaleFactor = (double)width/baseWidth;
struct xaAli *xa;

int y = yOff;
for (xa = tg->items; xa != NULL; xa = xa->next)
    {
    int start, end;
    strandFixupPair(xa->tStart, xa->tEnd, &start, &end);
    x1 = (int)(scaleFactor * (start-chromStart)) + xOff;
    x2 = (int)(scaleFactor * (end-chromStart)) + xOff;
    w = x2-x1;
    xenoDraw(mg, x1, w, y, tg->heightPer, color, grayColor, xa);
    mapWriteBox(mapFile, tg->subType, x1, y, w, tg->lineHeight, xa->name, ti.strand);
    mapWriteBox(mapFile, tg->trackType, xOff, y, width, tg->lineHeight, xa->name, ti.strand);

    y += tg->lineHeight;
    }
}

static void briggsaeDrawCondensedItems(struct trackGroup *tg, FILE *mapFile, int chromStart, int chromEnd,
        struct memGfx *mg, int xOff, int yOff, int width, int height,
        MgFont *font, Color color, Color grayColor)
/* Draw a xeno alignment list in condensed format. */
{
int x1,x2,w;
struct xaAli *xa;
int baseWidth = chromEnd - chromStart;
double scaleFactor = (double)width/baseWidth;
Color c;

for (xa = tg->items; xa != NULL; xa = xa->next)
    {
    int start, end;
    strandFixupPair(xa->tStart, xa->tEnd, &start, &end);
    x1 = (int)(scaleFactor * (start-chromStart));
    x2 = (int)(scaleFactor * (end-chromStart));
    w = x2-x1;
    if (w < 1) w = 1;
    c = (xa->milliScore >= 500 ? color : grayColor);
    mgDrawBox(mg, x1+xOff, yOff, w, height, c);
    }
mapWriteBox(mapFile, tg->condensedType, xOff, yOff, width, height, "", '+');
}

void briggsaeLoadItems(struct trackGroup *tg, boolean isCondensed, char *chrom, int start, int end)
/* Load in C. briggsae alignment data that's in range. */
{
char indexFileName[512];
char dataFileName[512];
char *xDir;
char *cb = "cbriggsae";
struct xaAli *xaList;

xDir = wormXenoDir();
sprintf(indexFileName, "%s%s/%s%s", xDir, cb, chrom, xaChromIxSuffix());
sprintf(dataFileName, "%s%s/all%s", xDir, cb, xaAlignSuffix());
tg->items = xaList = xaReadRange(indexFileName, dataFileName, start, end, isCondensed);
}

static void briggsaeFreeItems(struct trackGroup *tg, boolean isCondensed)
/* Free simple fireatures list. */
{
xaAliFreeList((struct xaAli**)(&tg->items));
}


char *briggsaeItemName(struct trackGroup *tg, void *item)
/* Return name of a briggsae item. */
{
struct xaAli *xa = item;
return xa->name;
}

struct trackGroup *getBriggsaeTg()
/* Get track group for C. briggsae homologies. */
{
static struct trackGroup tg;
static boolean initted = FALSE;
if (!initted)
    {
    tg.checkBoxName = "briggsae";
    tg.longLabel = "C. briggsae homologies";
    tg.shortLabel = "C. briggsae";
    tg.labelType = mtBriggsaeLabel;
    tg.trackType = mtBriggsaeTrack;
    tg.subType = mtBriggsaeOnBox;
    tg.condensedType = mtBriggsaeCondensed;
    tg.titleType = mtBriggsaeTitle;
#ifdef COLOR_TRACKS
    tg.color.r = 140;
    tg.color.g = 0;
    tg.color.b = 200;
    tg.grayColor.r = 210;
    tg.grayColor.g = 140;
    tg.grayColor.b = 250;
#else
    tg.grayColor.r = 140;
    tg.grayColor.g = 140;
    tg.grayColor.b = 140;
#endif /* COLOR_TRACKS */
    tg.loadItems = briggsaeLoadItems;
    tg.itemName = briggsaeItemName;
    tg.totalHeight = tgUsualHeight;
    tg.drawItems = briggsaeDrawItems;
    tg.drawCondensedItems = briggsaeDrawCondensedItems;
    tg.freeItems = briggsaeFreeItems;
    }
return &tg;
}

void drawTracks(char *where, struct trackGroup *groupList, boolean compactDisplay)
/* Draw tracks onto a gif in memory and save it. */
{
struct trackGroup *group;
int baseWidth = ti.chromEnd - ti.chromStart;
MgFont *font = mgSmallFont();
struct memGfx *mg;
struct tempName gifTn, mapTn;
FILE *mapFile;

int fontHeight = mgFontLineHeight(font);
int insideHeight = fontHeight-1;
int border = 1;
int xOff = border;
int pixWidth, pixHeight;
int insideWidth;
int y;

int typeCount = slCount(groupList);
int labelCount = typeCount;
int leftLabelWidth = 0;

int rulerHeight = fontHeight;
int relNumOff;

/* Figure out dimensions and allocate drawing space. */
if (!ti.withCenterLabels)
    labelCount = 0;
if (ti.withLeftLabels)
    {
    leftLabelWidth = 98;
    xOff += leftLabelWidth + border;
    }    
pixWidth = 616;
insideWidth = pixWidth-border-xOff;
if (compactDisplay)
    {
    pixHeight = rulerHeight + (typeCount+labelCount)*fontHeight + border;
    for (group = groupList; group != NULL; group = group->next)
        group->height = fontHeight;
    }
else
    {
    pixHeight = border + rulerHeight + fontHeight*labelCount;
    for (group = groupList; group != NULL; group = group->next)
        {
        pixHeight += group->totalHeight(group, font);
        }
    }
mg = mgNew(pixWidth, pixHeight);
mgClearPixels(mg);

/* Start writing an image map */
makeTempName(&mapTn, "trk", ".map");
mapFile = mustOpen(mapTn.forCgi, "wb");
mapWriteHead(mapFile, ti.title, pixWidth, pixHeight, 
    where, ti.chrom, ti.chromStart, ti.chromEnd, ti.strand,
    ti.hilite, ti.relStart);

/* Find colors to draw in. */
for (group = groupList; group != NULL; group = group->next)
    {
    group->ixColor = mgFindColor(mg, group->color.r, group->color.g, group->color.b);
    group->ixGrayColor = mgFindColor(mg, group->grayColor.r, group->grayColor.g, group->grayColor.b);
    }

/* Draw cyan hilite background. */
if (ti.hilite)
    {
    drawHilite(mg, xOff, border, insideWidth, pixHeight-2*border, ti.hilite);
    }

/* Draw left labels. */
if (ti.withLeftLabels)
    {
    int inWid = xOff-border*3;
    mgDrawBox(mg, xOff-border*2, 0, border, pixHeight, mgFindColor(mg, 0, 0, 200));
    mgSetClip(mg, border, border, inWid, pixHeight-2*border);
    y = border;
    for (group = groupList; group != NULL; group = group->next)
        {
        struct slList *item;
        if (ti.withCenterLabels)
            y += fontHeight;
        if (compactDisplay)
            {
            mgTextCentered(mg, border, y, inWid, fontHeight, group->ixColor, font, group->shortLabel);
            y += fontHeight;
            }
        else
            {
            for (item = group->items; item != NULL; item = item->next)
                {
                char *name = group->itemName(group, item);
                mgTextCentered(mg, border, y, inWid, group->lineHeight, group->ixColor, font, name);
                mapWriteBox(mapFile, group->labelType, border, y, inWid, group->lineHeight, name, '.');
                y += group->lineHeight;
                }
            }
        }
    }

/* Draw center labels. */
if (ti.withCenterLabels)
    {
    mgSetClip(mg, xOff, border, insideWidth, pixHeight - 2*border);
    y = border;
    for (group = groupList; group != NULL; group = group->next)
        {
        mgTextCentered(mg, xOff, y+1, insideWidth, insideHeight, group->ixColor, font, group->longLabel);
        mapWriteBox(mapFile, group->titleType, xOff, y, insideWidth, fontHeight, "", '.');
        y += fontHeight;
        y += group->height;
        }
    }

/* Draw tracks. */
y = 1;
for (group = groupList; group != NULL; group = group->next)
    {
    if (ti.withCenterLabels)
        y += fontHeight;
    mgSetClip(mg, xOff, y, insideWidth, group->height);
    if (compactDisplay)
        {
        mgSetClip(mg, xOff, y, insideWidth, fontHeight);
        group->drawCondensedItems(group, mapFile, ti.chromStart, ti.chromEnd,
            mg, xOff, y, insideWidth, insideHeight,
            font, group->ixColor, group->ixGrayColor);
        }
    else
        {
        group->drawItems(group, mapFile, ti.chromStart, ti.chromEnd,
            mg, xOff, y, insideWidth, 
            font, group->ixColor, group->ixGrayColor);
        }
    y += group->height;
    }

/* Show ruler at bottom. */
y = pixHeight - rulerHeight;
mgSetClip(mg, xOff, y, insideWidth, rulerHeight);
relNumOff = ti.chromStart - ti.relStart;
if (ti.strand == '-')
    relNumOff = -relNumOff;
mgDrawRuler(mg, xOff, y, rulerHeight, insideWidth, MG_BLACK, font,
    relNumOff, baseWidth);
mapWriteBox(mapFile, mtRuler, xOff, y, insideWidth, rulerHeight, "", '.');


/* Finish up image map. */
mapWriteBox(mapFile, mtEnd, 0,0,0,0,"",'.');
fclose(mapFile);
chmod(mapTn.forCgi, 0666);

/* Save out picture and tell html file about it. */
makeTempName(&gifTn, "trk", ".gif");
mgSaveGif(mg, gifTn.forCgi);
printf("<INPUT TYPE=HIDDEN NAME=map VALUE=\"%s\">\n", mapTn.forCgi);
printf(
    "<P><INPUT TYPE=IMAGE SRC = \"%s\" BORDER=1 WIDTH=%d HEIGHT=%d NAME = \"mouse\" ALIGN=BOTTOM><BR>\n",
    gifTn.forHtml, pixWidth, pixHeight);

mgFree(&mg);
chmod(gifTn.forCgi, 0666);
}

void makeNumText(char *name, int num, int digits)
/* Make a text control filled with a number. */
{
printf("<INPUT TYPE=TEXT NAME=\"%s\" SIZE=%d VALUE=%d>", name, digits, num);
}

void makeText(char *name, char *initialVal, int chars)
/* Make a text control filled with initial value. */
{
printf("<INPUT TYPE=TEXT NAME=\"%s\" SIZE=%d VALUE=%s>", name, chars, initialVal);
}

void makeSubmitButton(char *name, char *value)
/* Make a submit button. */
{
printf("<INPUT TYPE=SUBMIT NAME=\"%s\" VALUE=\"%s\">", name, value);
}

void makeDropList(char *name, char *menu[], int menuSize, char *checked)
/* Make a drop-down list. */
{
int i;
char *selString;
if (checked == NULL) checked = menu[0];
printf("<SELECT ALIGN=CENTER NAME=\"%s\">", name);
for (i=0; i<menuSize; ++i)
    {
    if (!differentWord(menu[i], checked))
        selString = " SELECTED";
    else
        selString = "";
    printf("<OPTION%s>%s</OPTION>", selString, menu[i]);
    }
printf("</SELECT>");
}

void makeCheckBox(char *name, boolean isChecked)
/* Create a checkbox with the given name in the given state. */
{
printf("<INPUT TYPE=CHECKBOX NAME=\"%s\" VALUE=on%s>", name,
    (isChecked ? " CHECKED" : "") );
}

void makeHiddenString(char *varName, char *string)
/* Store string in hidden input for next time around. */
{
printf("<INPUT TYPE=HIDDEN NAME=\"%s\" VALUE=\"%s\">", varName, string);
}

void continueHiddenVar(char *varName)
/* Write CGI var back to hidden input for next time around. */
{
if (cgiVarExists(varName))
    makeHiddenString(varName, cgiString(varName));
}

char *strandMenu[] = {"+", "-"};

void checkTitle()
/* If title doesn't exist, make it up from locus. */
{
static char titleBuf[80];
if (ti.title == NULL)
    {
    if (ti.locus)
        sprintf(titleBuf, "Region Near %s", ti.locus);
    else
        sprintf(titleBuf, "Tracks Display");
    ti.title = titleBuf;
    }
}

void doTracks(boolean gotMap)
/* Lookup chromosome position of ti.locus and do tracks around it. */
{
struct trackGroup *groupList = NULL, *group;
int baseWidth;
boolean doCompact;
char strandString[2];
boolean gotGenie = genieExists();

wormChromNames(&chromNames, &chromCount);

/* Get input vars from menu items. */
ti.withCosmids = cgiBoolean("cosmids");
ti.withCdna = cgiBoolean("cDNA");
ti.withEmbryoCdna = cgiBoolean("embryoCdna");
ti.withGenes = cgiBoolean("spliDi");
ti.withGenie = cgiBoolean("genie");
ti.withBriggsae = cgiBoolean("briggsae");
ti.withLeftLabels = cgiBoolean("leftLabels");
ti.withCenterLabels = cgiBoolean("centerLabels");
ti.dynFa = cgiOptionalString("dynFa");
ti.dynCda = cgiOptionalString("dynCda");

/* Other input vars may be set from map file. Otherwise get it from cgi. */
if (!gotMap)
    {
    if (ti.locus == NULL)
        ti.locus = trimSpaces(cgiString("where"));
    ti.title = cgiOptionalString("title");
    checkTitle();

    /* If they didn't ask for either cDNA or splicing diagram - hah!
     * give them both the first time through. Why? Saves space in
     * computer generated files calling tracker mostly. */
     if (!ti.withCdna && !ti.withEmbryoCdna && !ti.withGenes && !ti.withCosmids)
        {
        ti.withCdna = TRUE;
        ti.withEmbryoCdna = TRUE;
        ti.withGenes = TRUE;
        ti.withGenie = TRUE;
        ti.withBriggsae = TRUE;
        ti.withCenterLabels = TRUE;
        ti.withLeftLabels = TRUE;
        }

    /* Figure out bases covered by track. */
    if (!wormGeneRange(ti.locus, &ti.chrom, &ti.strand, &ti.chromStart, &ti.chromEnd))
        errAbort("Couldn't find locus %s", ti.locus);

    if (!wormIsChromRange(ti.locus))
        {
        ti.chromStart -= 500;
        ti.chromEnd += 500;
        }
    if (ti.chromStart == ti.chromEnd)
        {
        ti.chromStart -= 10;
        ti.chromEnd += 10;
        }

    /* Set up hiliting range. */
    if (cgiVarExists("hilite"))
        {
        ti.hilite = cgiString("hilite");
        }

    /* Set up numbering to initially start at zero. */
    ti.relStart = ti.chromStart;
    }

wormClipRangeToChrom(ti.chrom, &ti.chromStart, &ti.chromEnd);
baseWidth = ti.chromEnd-ti.chromStart;
doCompact = (baseWidth >= 50000);

/* Figure out which track groups to use. */
if (ti.dynCda)
    slAddTail(&groupList, getDynCdnaTg());
if (ti.withCosmids)
    slAddTail(&groupList, getCosmidTg());
if (ti.withGenes)
    slAddTail(&groupList, getAceGenesTg());
if (gotGenie && ti.withGenie)
    slAddTail(&groupList, getGenieGenesTg());
if (ti.withBriggsae)
    slAddTail(&groupList, getBriggsaeTg());
if (ti.withCdna)
    slAddTail(&groupList, getCdnaTg());
if (ti.withEmbryoCdna)
    slAddTail(&groupList, getEmbryonicCdnaTg());


/* Tell groups to load their items. */
for (group = groupList; group != NULL; group = group->next)
    group->loadItems(group, doCompact, ti.chrom, ti.chromStart, ti.chromEnd); 

/* Deal with ti.strand issues. Explicit parameter will over-ride
 * what we got with the DNA.  But if the DNA is ambiguous try
 * and find a dominant ti.strand in the gene list.  If all else
 * fails set it to '+' */
if (!gotMap)
    {
    ti.strand = dominantStrand(getAceGenesTg()->items);
    if (ti.strand == '.' && cgiVarExists("strand"))
        {
        ti.strand = cgiString("strand")[0];
        }
    if (ti.strand == '.')
        {
        ti.strand = '+';
        }
    }

/* Tell browser where to go when they click on image. */
printf("<FORM ACTION=\"%stracks.exe\">\n\n", cgiDir());

/* Perpetuate some hidden variables. */
continueHiddenVar("dynFa");
continueHiddenVar("dynCda");

/* Center everything from now on. */
printf("<CENTER>\n");


/* Show title . */
printf("<H2>%s</H2>", ti.title);

/* Put up scroll and zoom controls. */
fputs("move ", stdout);
makeSubmitButton("left3", "<<<");
makeSubmitButton("left2", " <<");
makeSubmitButton("left1", " < ");
makeSubmitButton("right1", " > ");
makeSubmitButton("right2", ">> ");
makeSubmitButton("right3", ">>>");
fputs(" zoom in ", stdout);
makeSubmitButton("in1", "1.5x");
makeSubmitButton("in2", " 3x ");
makeSubmitButton("in3", "10x");
fputs(" zoom out ", stdout);
makeSubmitButton("out1", "1.5x");
makeSubmitButton("out2", " 3x ");
makeSubmitButton("out3", "10x");
fputs("<BR>\n", stdout);

drawTracks(ti.locus, groupList, doCompact);

fputs("Click on image to view more information on a gene or alignment. Click on ruler for DNA<BR>", stdout);
if (ti.hilite)
    fputs("Unusual region is hilighted light blue.<BR>\n", stdout);

/* Put up control panel */
fputs("<HR ALIGN=CENTER>",stdout);

fputs("Chromosome ", stdout);
makeDropList("chrom", chromNames, chromCount, ti.chrom);
fputs(" bases ",stdout);
makeNumText("start", ti.chromStart, 9);
fputs(" - ", stdout);
makeNumText("end", ti.chromEnd, 9);
fputs(" strand ", stdout);
strandString[0] = ti.strand;
strandString[1] = 0;
makeDropList("strand", strandMenu, 2, strandString);
fputs(" ", stdout);
makeSubmitButton("explicit", "jump");
fputs("<BR>\n", stdout);

fputs("<B>View:</B>", stdout);
fputs(" Cosmids ", stdout);
makeCheckBox("cosmids", ti.withCosmids);
fputs(" AceDb Gene Predictions ", stdout);
makeCheckBox("spliDi", ti.withGenes);
if (gotGenie)
    {
    fputs("Genie Gene Predictons ", stdout);
    makeCheckBox("genie", ti.withGenie);
    }
fputs(" cDNA ", stdout);
makeCheckBox("cDNA", ti.withCdna);
fputs(" separate embryonic cDNA ", stdout);
makeCheckBox("embryoCdna", ti.withEmbryoCdna);
fputs(" <I>C. briggsae</I> homologies ", stdout);
makeCheckBox("briggsae", ti.withBriggsae);
fputs(" Left Labels ", stdout);
makeCheckBox("leftLabels", ti.withLeftLabels);
fputs(" Center Labels", stdout);
makeCheckBox("centerLabels", ti.withCenterLabels);
makeSubmitButton("explicit", "refresh");
fputs("<BR>\n", stdout);

fputs("Jump to named ORF or gene of known sequence: ",stdout);
makeText("where", "", 16);
fputs(" ", stdout);
makeSubmitButton("explicit", "jump");
fputs("</P>\n", stdout);

fputs("<P>Return to <A HREF=\"../intronerator/index.html\">Intronerator</A></P>\n", stdout);

for (group = groupList; group != NULL; group = group->next)
    group->freeItems(group, doCompact);
}

void zoomAroundCenter(double amount)
/* Set ends so as to zoom around center by scaling amount. */
{
int baseCount = ti.chromEnd - ti.chromStart;
int center = (ti.chromStart + ti.chromEnd)/2;
int newCount = (int)(baseCount*amount + 0.5);
if (newCount < 30) newCount = 30;
ti.chromStart = center - newCount/2;
ti.chromEnd = ti.chromStart + newCount;
}

void zoomInLots(double relCenter, int zoomFactor)
/* Zoom in around relative center position by zoomFactor. */
{
int oldWinWidth = ti.chromEnd - ti.chromStart;
int center = ti.chromStart + round(relCenter*oldWinWidth);
int newWinWidth = oldWinWidth/zoomFactor;
ti.chromStart = center - newWinWidth/2;
ti.chromEnd = ti.chromStart + newWinWidth;
}

void relativeScroll(double amount)
/* Scroll percentage of visible window. */
{
int baseCount = ti.chromEnd - ti.chromStart;
int offset;

if (ti.strand == '-')
    amount = -amount;
offset = (int)(amount * baseCount + 0.5);
ti.chromEnd += offset;
ti.chromStart += offset;
}

void dispatcher()
/* Peep at cgi string to see if we should just quickly pass the buck to 
 * fuzzyFind.exe or getGene.exe, or handle it ourselves. */
{
enum mapType mt;
int boxX, boxY, boxW, boxH;
char *boxName;
char boxStrand;
bits32 pixWidth, pixHeight;
boolean gotMap = FALSE;

if (cgiVarExists("map"))
    {
    char *mapName = cgiString("map");
    FILE *f = mustOpen(mapName, "rb");
    gotMap = TRUE;
    mapReadHead(f, &ti.title, &pixWidth, &pixHeight, 
        &ti.locus, &ti.chrom, &ti.chromStart, &ti.chromEnd, &ti.strand,
        &ti.hilite, &ti.relStart);
    if (cgiVarExists("explicit"))
        {
        char *newLocus = trimSpaces(cgiString("where"));
        if (newLocus == NULL)
            {
            ti.chrom = cgiString("chrom");
            ti.chromStart = atoi(cgiString("start"));
            ti.chromEnd = atoi(cgiString("end"));
            ti.strand = cgiString("strand")[0];
            ti.relStart = ti.chromStart;
            }
        else
            {
            gotMap = FALSE;     /* Disregard map, go for gene. */
            ti.locus = newLocus;
            ti.title = NULL;
            }
        }

    else if (cgiVarExists("left3"))
        relativeScroll(-0.95);
    else if (cgiVarExists("left2"))
        relativeScroll(-0.475);
    else if (cgiVarExists("left1"))
        relativeScroll(-0.1);
    else if (cgiVarExists("right1"))
        relativeScroll(0.1);
    else if (cgiVarExists("right2"))
        relativeScroll(0.475);
    else if (cgiVarExists("right3"))
        relativeScroll(0.95);

    else if (cgiVarExists("in3"))
        zoomAroundCenter(1.0/10.0);
    else if (cgiVarExists("in2"))
        zoomAroundCenter(1.0/3.0);
    else if (cgiVarExists("in1"))
        zoomAroundCenter(1.0/1.5);
    else if (cgiVarExists("out1"))
        zoomAroundCenter(1.5);
    else if (cgiVarExists("out2"))
        zoomAroundCenter(3.0);
    else if (cgiVarExists("out3"))
        zoomAroundCenter(10.0);
    else
        {
        ti.mouseX = cgiInt("mouse.x");
        ti.mouseY = cgiInt("mouse.y");
        if (!mapScanForHit(f, ti.mouseX, ti.mouseY, 
            &mt, &boxX, &boxY, &boxW, &boxH, &boxName, &boxStrand))
            {
            mt = mtOutside;
            }
        switch (mt)
            {
            case mtCdnaTrack:
            case mtCdnaLabel:
                {
                printf("Location: %sfuzzyFind.exe?cDNA=%s&gene=%s:%d-%d&hayStrand=%c\n\n",
                    cgiDir(), boxName, ti.chrom, ti.chromStart, ti.chromEnd, ti.strand);
                return;
                }
            case mtDynTrack:
            case mtDynLabel:
                {
                printf("Location: %sfuzzyFind.exe?needleFile=%s&stringency=cDNA&gene=%s:%d-%d&hayStrand=%c\n\n",
                    cgiDir(), cgiString("dynFa"), ti.chrom, ti.chromStart, ti.chromEnd, ti.strand);
                return;
                }
            case mtGeneTrack:
            case mtGeneLabel:
            case mtCosmidTrack:
            case mtCosmidLabel:
                {
                printf("Location: %sgetgene.exe?geneName=%s&litLink=on&intronsLowerCase=On\n\n", 
                    cgiDir(), boxName);
                return;
                }
            case mtGeneOnBox:
                {
                double relPos = (double)(ti.mouseX-boxX)/(double)(boxW);
                if (ti.strand != boxStrand) relPos = 1.0 - relPos;
                printf("Location: %sgetgene.exe?geneName=%s&hiliteNear=%f&litLink=on&intronsLowerCase=On\n\n", 
                    cgiDir(), boxName, relPos);
                return;
                }
            case mtBriggsaeTrack:
            case mtBriggsaeLabel:
                {
                printf("Location: %sxaShow.exe?qOrganism=%s&tOrganism=%s&query=%s&target=%s:%d-%d&strand=%c\n\n", 
                    cgiDir(), "cbriggsae", organism, boxName, 
                    ti.chrom, ti.chromStart, ti.chromEnd, ti.strand);
                }
                return;
            case mtBriggsaeOnBox:
                {
                double relPos = (double)(ti.mouseX-boxX)/(double)(boxW);
                printf("Location: %sxaShow.exe?clickPos=%f&qOrganism=%s&tOrganism=%s&query=%s&target=%s:%d-%d&strand=%c\n\n", 
                    cgiDir(), relPos, "cbriggsae", organism, boxName, 
                    ti.chrom, ti.chromStart, ti.chromEnd, ti.strand);
                }
                return;
            case mtRuler:
                {
                printf("Location: %sgetgene.exe?geneName=%s:%d-%d&litLink=on&intronsLowerCase=On&strand=%c\n\n", 
                    cgiDir(), ti.chrom, ti.chromStart, ti.chromEnd, ti.strand);
                return;
                }
            case mtDynCondensed:
            case mtCosmidCondensed:
            case mtGeneCondensed:
            case mtCdnaCondensed:
            case mtBriggsaeCondensed:
                {
                double relPos = (double)(ti.mouseX-boxX)/(double)(boxW);
                if (ti.strand == '-')
                    relPos = 1.0-relPos;
                zoomInLots(relPos, 10);
                break;
                }
            default:
                break;
            }
        }
    fclose(f);
    }
checkTitle();

htmlStart(ti.title);              
doTracks(gotMap);
htmlEnd();
}

int main(int argc, char *argv[])
{
if (argc == 2 && strcmp(argv[1], "test") == 0)
    {
    putenv("QUERY_STRING=where=unc-52");
    }
dnaUtilOpen();
htmEmptyShell(dispatcher, "QUERY");
return 0;
}
