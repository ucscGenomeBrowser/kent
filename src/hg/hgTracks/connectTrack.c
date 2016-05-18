
#include "common.h"
#include "hgTracks.h"

struct connect
{
struct connect *next;
unsigned s;
unsigned sw;
int sOrient;
char *sChrom;
unsigned e;
unsigned ew;
int eOrient;
char *eChrom;
unsigned height;
double score;
unsigned id;
};

int trackHeight = 200;

static int connectHeight(struct track *tg, enum trackVisibility vis)
/* calculate height of all the snakes being displayed */
{
return trackHeight;
}

static char *getOther(struct bed *bed, unsigned *s, unsigned *e, double *score)
{
char *otherChrom = cloneString(bed->name);
char *ptr = strchr(otherChrom, ':');
if (ptr == NULL)
    errAbort("bad longTabix bed name %s\n", bed->name);
*ptr++ = 0;
*s = atoi(ptr);
ptr = strchr(ptr, '-');
if (ptr == NULL)
    errAbort("bad longTabix bed name %s\n", bed->name);
ptr++;
*e = atoi(ptr);
ptr = strchr(ptr, ',');
if (ptr == NULL)
    errAbort("bad longTabix bed name %s\n", bed->name);
ptr++;
*score = sqlDouble(ptr);

return otherChrom;
}

static void connectDraw(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
{
double scale = scaleForWindow(width, seqStart, seqEnd);
struct bed *beds = tg->items;
struct connect *connect;
struct connect *connectList = NULL;

double maxWidth = 0;
for(; beds; beds=beds->next)
    {
    AllocVar(connect);
    slAddHead(&connectList, connect);

    unsigned otherS;
    unsigned otherE;
    double score;
    char *otherChrom = getOther(beds, &otherS, &otherE, &score);
    unsigned otherCenter = (otherS + otherE)/2;
    unsigned otherWidth = otherE - otherS;
    unsigned thisWidth = beds->chromEnd - beds->chromStart;
    unsigned center = (beds->chromEnd + beds->chromStart) / 2;


    // don't have oriented feet at the moment
    connect->sOrient = connect->eOrient = 0;
    connect->id = beds->score;
    connect->score = score;
    
    if (otherCenter < center)
        {
        connect->s = otherCenter;
        connect->sw = otherWidth;
        connect->sChrom = otherChrom;
        connect->e = center;
        connect->ew = thisWidth;
        connect->eChrom = beds->chrom;
        }
    else
        {
        connect->s = center;
        connect->sw = thisWidth;
        connect->sChrom = beds->chrom;
        connect->e = otherCenter;
        connect->ew = otherWidth;
        connect->eChrom = otherChrom;
        }
    double connectWidth = connect->e - connect->s;
    if (connectWidth > maxWidth)
        maxWidth = connectWidth;
    }

    for(connect=connectList; connect; connect=connect->next)
        {
        if (sameString(connect->sChrom, connect->eChrom))
            {
            int sx = (connect->s - seqStart) * scale + xOff;
            int ex = (connect->e - seqStart) * scale + xOff;
            double connectWidth = connect->e - connect->s;
            int height = (trackHeight - 15) * (connectWidth / maxWidth) + yOff + 10;
            int tsx = sx;
            int tex = ex;

            if (tsx > tex)
                {
                tsx = sx;
                tex = ex;
                }
            
            hvGfxLine(hvg, sx, yOff, tsx, height, color);
            hvGfxLine(hvg, tsx, height, tex, height, color);
            hvGfxLine(hvg, ex, yOff, tex, height, color);
            char itemBuf[2048];
            safef(itemBuf, sizeof itemBuf, "%d", connect->id);
            char statusBuf[2048];
            safef(statusBuf, sizeof statusBuf, "%g %s:%d", connect->score, connect->eChrom, connect->e);
            mapBoxHgcOrHgGene(hvg, connect->s, connect->e,tsx, height-2, tex-tsx, 4,
                                   tg->track, itemBuf, statusBuf, "directUrl", TRUE,
                                   "extra");
            }
        }
}

void connectMethods(struct track *tg, struct trackDb *tdb)
{
tg->drawItems = linkedFeaturesDraw;
tg->drawItemAt = linkedFeaturesDrawAt;
tg->canPack = tdb->canPack = TRUE;
tg->drawItems = connectDraw;
tg->totalHeight = connectHeight; 
tg->mapsSelf = TRUE;
}
