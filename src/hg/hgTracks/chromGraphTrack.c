/* chromGraphTrack - displays track that is just a line
 * connecting certain data points.  This does not handle the
 * data volume that the wiggle track does.  It's meant for
 * things like graphs for gene association studies. */
#include "common.h"
#include "hash.h"
#include "jksql.h"
#include "trackDb.h"
#include "hdb.h"
#include "hgTracks.h"
#include "vGfx.h"
#include "chromGraph.h"

static void cgDrawItems(struct track *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw chromosome graph. */
{
struct chromGraphSettings *cgs = tg->customPt;
struct sqlConnection *conn = hAllocConn();
char query[512];
struct sqlResult *sr;
char **row;
int x,y,lastX=0,lastY=0;
int height = tg->height;
int maxGapToFill = cgs->maxGapToFill;
int lastPos = -maxGapToFill-1;
double minVal = cgs->minVal;
double yScale = height/(cgs->maxVal - minVal);
double xScale = scaleForPixels(width);

/* Construct query.  Set up a little more than window so that
 * we can draw connecting lines. */
safef(query, sizeof(query), 
    "select chromStart,val from %s "
    "where chrom='%s' and chromStart>=%d and chromStart<%d",
    tg->mapName, chromName, 
    seqStart - cgs->maxGapToFill, seqEnd + cgs->maxGapToFill);
sr = sqlGetResult(conn, query);

/* Loop through drawing lines from one point to another unless
 * the points are too far apart. */
while ((row = sqlNextRow(sr)) != NULL)
    {
    int pos = sqlUnsigned(row[0]);
    double val = atof(row[1]);
    x = (pos - seqStart)*xScale + xOff;
    y = (val - minVal)*yScale + yOff;
    if (x >= xOff)
        {
	if (pos - lastPos <= maxGapToFill)
	    vgLine(vg, lastX, lastY, x, y, color);
	else
	    vgDot(vg, x, y, color);
	}
    lastX = x;
    lastY = y;
    lastPos = pos;
    if (pos >= seqEnd)
        break;
    }

sqlFreeResult(&sr);
hFreeConn(&conn);
}

int cgTotalHeight(struct track *tg, enum trackVisibility vis)
/* Most fixed height track groups will use this to figure out the height 
 * they use. */
{
struct chromGraphSettings *cgs = tg->customPt;
switch (vis)
    {
    case tvFull:
    case tvPack:
        tg->height = cgs->pixels;
	break;
    case tvDense:
    default:
        tg->height = tl.fontHeight;
	break;
    }
return tg->height;
}

void chromGraphMethods(struct track *tg)
/* Return track with methods filled in */
{
struct sqlConnection *conn = hAllocConn();
tg->loadItems = tgLoadNothing;
tg->drawItems = cgDrawItems;
tg->totalHeight = cgTotalHeight;
tg->freeItems = tgFreeNothing;
tg->customPt = chromGraphSettingsGet(tg->mapName, conn,
	tg->tdb, cart);
tg->mapsSelf = TRUE;
hFreeConn(&conn);
}

