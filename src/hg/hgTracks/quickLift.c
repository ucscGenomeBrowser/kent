
#include "common.h"
#include "hgTracks.h"
#include "chromAlias.h"
#include "hgConfig.h"
#include "bigChain.h"
#include "bigLink.h"
#include "trackHub.h"
#include "quickLift.h"

static Color *highlightColors;
//static unsigned lengthLimit;

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
    highlightColors[QUICKTYPE_NOTHING] = getColor("quickLift.nothingColor","1,1,1,1");
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
if (startsWith("quickLiftChain", trackHubSkipHubName(tg->track)))
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

//mapBoxHc(hvg, seqStart, seqEnd, xOff, yOff, width,  tg->height, tg->track,  "fart", "hallo");
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

    if (hr->type == QUICKTYPE_NOTHING)
        {
        safef(mouseOver, sizeof mouseOver, "identical");
        mapBoxHc(hvg, hr->chromStart, hr->chromEnd, x1, yOff, w, height, tg->track, hr->id, mouseOver);
        }
    else
        {
        if (drawTriangle)
            {
            startX = x1 + w/2 - fontHeight/2;
            endX = x1 + w/2 + fontHeight/2 ;
            drawTri(hvg, startX, endX , yOff, hexColor);
            }
        else
            {
            startX = x1;
            endX = x1 + w;
            hvGfxBox(hvg, startX, yOff, w, height, hexColor);
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
        }
    }
}
