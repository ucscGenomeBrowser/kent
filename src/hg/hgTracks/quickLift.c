
#include "common.h"
#include "hgTracks.h"
#include "chromAlias.h"
#include "hgConfig.h"
#include "bigChain.h"
#include "bigLink.h"
#include "trackHub.h"
#include "quickLift.h"
#include "chainCart.h"
#include "bigChain.h"

static Color *highlightColors;
//static unsigned lengthLimit;

struct cartOptions
    {
    enum chainColorEnum chainColor; /*  ChromColors, ScoreColors, NoColors */
    int scoreFilter ; /* filter chains by score if > 0 */
    };

static Color getColor(char *confVariable, char *defaultRgba)
{
Color color = 0xff0000ff;

char *str = cloneString(cfgOptionDefault(confVariable, defaultRgba));
char *words[10];
if (chopCommas(str, words) != 4)
    errAbort("conf variable '%s' is expected to have a string like  r,g,b,a ", confVariable);

color = MAKECOLOR_32_A(atoi(words[0]),atoi(words[1]),atoi(words[2]),atoi(words[3]));
return color;
}

static void initializeHighlightColors()
{
if (highlightColors == NULL)
    {
    highlightColors = needMem(sizeof(Color) * 5);
 
    //lengthLimit = atoi(cfgOptionDefault("quickLift.lengthLimit", "10000"));

    highlightColors[QUICKTYPE_INSERT] = getColor("quickLift.insColor","255,0,0,255");
    highlightColors[QUICKTYPE_DEL] = getColor("quickLift.delColor","0,255,0,255");
    highlightColors[QUICKTYPE_DOUBLE] = getColor("quickLift.doubleColor","0,0,255,255");
    highlightColors[QUICKTYPE_MISMATCH] = getColor("quickLift.mismatchColor","255,0,0,255");
    }
}


static void drawTri(struct hvGfx *hvg, int x1, int x2, int y, Color color)
/* Draw traingle. */
{
struct gfxPoly *poly = gfxPolyNew();
int half = (x2 - x1) / 2;

gfxPolyAddPoint(poly, x1, y);
gfxPolyAddPoint(poly, x1+half, y+2*half);
    gfxPolyAddPoint(poly, x2, y);
    hvGfxDrawPoly(hvg, poly, color, TRUE);
    gfxPolyFree(&poly);
    }

static int snakePalette2[] =
{
0x1f77b4, 0xaec7e8, 0xff7f0e, 0xffbb78, 0x2ca02c, 0x98df8a, 0xd62728, 0xff9896, 0x9467bd, 0xc5b0d5, 0x8c564b, 0xc49c94, 0xe377c2, 0xf7b6d2, 0x7f7f7f, 0xc7c7c7, 0xbcbd22, 0xdbdb8d, 0x17becf, 0x9edae5
};

static void drawDenseChain(struct track *tg, struct hvGfx *hvg, char *fileName, int start, int end, int xOff, int yOff, int width, MgFont *font, Color color)
// draw a dense version of a chain that users can use to click through to the other species
{
struct lm *lm = lmInit(0);
struct bbiFile *bbi =  bigBedFileOpenAlias(fileName, chromAliasFindAliases);
struct bigBedInterval *bb, *bbList =  bigBedIntervalQuery(bbi, chromName, start, end, 0, lm);
char *bedRow[12];
char startBuf[16], endBuf[16];
double scale = scaleForWindow(width, start, end);

unsigned colorIndex = 0;
for(bb=bbList; bb; bb = bb->next, colorIndex++)
    {
    colorIndex %= sizeof(snakePalette2) / sizeof(int);

    bigBedIntervalToRow(bb, chromName, startBuf, endBuf, bedRow, ArraySize(bedRow));
    struct bigChain *chain = bigChainLoad(bedRow);
    if (chain->chromStart < start)
        chain->chromStart = start;
    if (chain->chromEnd > end)
        chain->chromEnd = end;
    int x1 = xOff + scale * (chain->chromStart - start);
    int w =  scale * (chain->chromEnd - chain->chromStart);
    unsigned colorHex = snakePalette2[colorIndex];
    unsigned colorVal = MAKECOLOR_32(((colorHex >> 16) & 0xff),((colorHex >> 8) & 0xff),((colorHex >> 0) & 0xff));
    hvGfxBox(hvg, x1, yOff, w, tg->height, colorVal);
    char otherChrom[1024];
    safef(otherChrom, sizeof otherChrom, "%s:%d-%d", chain->qName, chain->qStart, chain->qEnd);
    mapBoxHc(hvg, chain->chromStart, chain->chromEnd, x1, yOff, w,  tg->height, tg->track,  chain->name, otherChrom);
    }
}

void maybeDrawQuickLiftLines( struct track *tg, int seqStart, int seqEnd,
                      struct hvGfx *hvg, int xOff, int yOff, int width,
                      MgFont *font, Color color, enum trackVisibility vis)
/* Draw the indel regions in quickLifted tracks as a highlight. */
{
char *quickLiftFile = cloneString(trackDbSetting(tg->tdb, "quickLiftUrl"));
if (quickLiftFile == NULL)
    return;

initializeHighlightColors();
boolean drawTriangle = FALSE;
if (startsWith("bigQuickLiftChain", tg->tdb->type))
    drawTriangle = TRUE;
char *liftDb = trackDbSetting(tg->tdb, "quickLiftDb");
struct quickLiftRegions *regions = quickLiftGetRegions(database, liftDb, quickLiftFile, chromName, seqStart, seqEnd);
struct quickLiftRegions *hr = regions;

int fontHeight = mgFontLineHeight(tl.font);
int height = tg->height;
if (!drawTriangle && isCenterLabelIncluded(tg))
    {
    height += fontHeight;
    yOff -= fontHeight;
    }

if (drawTriangle && (hr == NULL))  
    {
    drawDenseChain(tg, hvg, quickLiftFile, seqStart, seqEnd, xOff, yOff, width, font, color);
    return;
    }

int pos = xOff;
char * lastId = 0;
for(; hr; hr = hr->next)
    {
    unsigned int hexColor = highlightColors[hr->type];
    double scale = scaleForWindow(width, seqStart, seqEnd);
    int x1 = xOff + scale * (hr->chromStart - seqStart);
    int w =  scale * (hr->chromEnd - hr->chromStart);
    if (w == 0) 
        w = 1;
    hvGfxSetClip(hvg, xOff, yOff, width, height);  // we're drawing in the center label at the moment
    int startX, endX;
    char mouseOver[4096];

    if (drawTriangle)
        {
        startX = x1 + w/2 - fontHeight/2;
        endX = x1 + w/2 + fontHeight/2 ;
        drawTri(hvg, startX, endX , yOff, hexColor);
        if (drawTriangle && (x1 > pos))
            mapBoxHc(hvg, seqStart, seqEnd, pos, yOff, startX - pos,  tg->height, tg->track,  hr->id, "identical");
        pos = endX;
        }
    else
        {
        startX = x1;
        endX = x1 + w;
        hvGfxBox(hvg, startX, yOff, w, height, hexColor);
        continue;
        }

    if (hr->type == QUICKTYPE_MISMATCH)
        safef(mouseOver, sizeof mouseOver, "mismatch %c->%c", *hr->otherBases, *hr->bases);
    else if (hr->chromStart == hr->chromEnd)
        safef(mouseOver, sizeof mouseOver, "deletion %ldbp (%.*s)", hr->oChromEnd - hr->oChromStart, hr->otherBaseCount, hr->otherBases);
    else if (hr->oChromStart == hr->oChromEnd)
        safef(mouseOver, sizeof mouseOver, "insertion %ldbp (%.*s)", hr->chromEnd - hr->chromStart, hr->baseCount, hr->bases);
    else
        safef(mouseOver, sizeof mouseOver, "double %ldbp", hr->oChromEnd - hr->oChromStart);

    mapBoxHc(hvg, seqStart, seqEnd, startX, yOff, endX - startX, height, tg->track, hr->id, mouseOver);
    lastId = hr->id;
    }
if (drawTriangle && (pos != xOff))
    mapBoxHc(hvg, seqStart, seqEnd, pos, yOff, width - pos,  tg->height, tg->track, lastId, "identical");

}

void quickLiftDraw(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width,
        MgFont *font, Color color, enum trackVisibility vis)
{
//mapBoxHc(hvg, seqStart, seqEnd, xOff, yOff, width,  tg->height, tg->track,  "fart", "hallo");
maybeDrawQuickLiftLines( tg, seqStart, seqEnd, hvg, xOff, yOff, width, font, color, vis);
}

static int quickLiftTotalHeight(struct track *tg, enum trackVisibility vis)
/* Return configured height of track. */
{
return tg->lineHeight;
}

static void quickLiftDoLeftLabels(struct track *tg, int seqStart, int seqEnd, struct hvGfx *hvg,
                              int xOff, int yOff, int width, int height, boolean withCenterLabels,
                              MgFont *font, Color color, enum trackVisibility vis)
/* For now, draw no left labels. */
{
return;
}

void quickLiftChainMethods(struct track *tg, struct trackDb *tdb,
	int wordCount, char *words[])
/* Fill in custom parts of quickLift chains */
{
struct cartOptions *chainCart;

AllocVar(chainCart);
tg->extraUiData = (void *) chainCart;

tg->isBigBed = TRUE;

void bigChainLoadItems(struct track *tg);
tg->loadItems = bigChainLoadItems;
tg->drawItems = quickLiftDraw;
tg->totalHeight = quickLiftTotalHeight;
tg->drawLeftLabels = quickLiftDoLeftLabels;
tg->mapsSelf = TRUE;
}
