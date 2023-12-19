/* dotplot routines */

/* Copyright (C) 2023 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hash.h"
#include "hdb.h"
#include "hgBam.h"
#include "hgc.h"
#include "knetUdc.h"
#include "udc.h"
#include "chromAlias.h"
#include "dotPlot.h"
#include "pipeline.h"

struct dotLine 
{
struct dotLine *next;
int tStart;
int qStart;
int len;
int id;
};

struct qRegion
{
struct qRegion *next;
char *qName;
int qStart;
int qEnd;
struct dotLine *posLines;
struct dotLine *negLines;
double scale;
};

struct dotData
{
char *tName;
int tStart;
int tEnd;
struct qRegion *qRegions;
};

static void getQRange(struct psl *pslList, unsigned *qStart, unsigned *qEnd, int start, int end)
{
struct psl *psl;
unsigned minQ = 0xffffffff, maxQ = 0;
for(psl = pslList; psl; psl = psl->next)
    {
    unsigned *tStart = psl->tStarts;
    unsigned *qStart = psl->qStarts;
    unsigned *len = psl->blockSizes;

    int ii = 0;
    for(; ii < psl->blockCount; ii++, len++, qStart++, tStart++)
        {
        if ((*tStart > end) || (*tStart + *len < start))
            continue;

        int val = *qStart;
        if (psl->strand[0] == '-')
            val = psl->qSize - val;
        if (minQ > val)
            minQ = val;
        if (maxQ < val + *len)
            maxQ = val + *len;
        }
    }
*qStart = minQ;
*qEnd = maxQ;
}

static struct dotData *getDotData(char *chrom,  int start, int end, struct psl *pslList, int w, int h)
{
struct dotData *dotData;
AllocVar(dotData);
dotData->tStart = start;
dotData->tEnd = end;
struct qRegion *qRegion;
AllocVar(qRegion);
dotData->qRegions = qRegion;

unsigned qLeft, qRight;
getQRange(pslList, &qLeft, &qRight, start, end);
qRegion->qStart = qLeft;
double scale;
int tRange = end - start;
int qRange = qRight - qLeft;
if (tRange > qRange)
    scale = (double)w / tRange;
else
    scale = (double)w / qRange;
qRegion->scale = scale;

struct psl *psl;
int pslNum = 0;
for(psl = pslList; psl; psl = psl->next)
    {
    unsigned *qStart = psl->qStarts;
    unsigned *tStart = psl->tStarts;
    unsigned *len = psl->blockSizes;
    int ii = 0;
    if (psl->strand[0] == '-')
        {
        for(; ii < psl->blockCount; ii++, len++, qStart++, tStart++)
            {
            if ((*tStart > end) || (*tStart + *len < start))
                continue;

            struct dotLine *dotLine;
            AllocVar(dotLine);
            slAddHead(&qRegion->negLines, dotLine);
            dotLine->tStart = scale * (*tStart - start);
            dotLine->qStart = scale * ((psl->qSize - (*qStart + *len)) - qLeft);
            dotLine->len = *len * scale;
            dotLine->id = pslNum;
            }
        }
    else
        {
        for(; ii < psl->blockCount; ii++, len++, qStart++, tStart++)
            {
            if ((*tStart > end) || (*tStart + *len < start))
                continue;

            struct dotLine *dotLine;
            AllocVar(dotLine);
            slAddHead(&qRegion->posLines, dotLine);
            dotLine->tStart = scale * (*tStart - start);
            dotLine->qStart = scale * (*qStart - qLeft);
            dotLine->len = *len * scale;
            dotLine->id = pslNum;
            }
        }
    pslNum++;
    }

return dotData;
}

static int snakePalette2[] =
{
0x1f77b4, 0xaec7e8, 0xff7f0e, 0xffbb78, 0x2ca02c, 0x98df8a, 0xd62728, 0xff9896, 0x9467bd, 0xc5b0d5, 0x8c564b, 0xc49c94, 0xe377c2, 0xf7b6d2, 0x7f7f7f, 0xc7c7c7, 0xbcbd22, 0xdbdb8d, 0x17becf, 0x9edae5
};


static void drawDotPs(char *fileName, int w, int h, char *chrom, int start, int end,  struct psl *pslList)
{
struct dotData *dotData = getDotData(chrom,   start, end, pslList, w, h);
FILE *f = mustOpen(fileName, "w");
fprintf(f, "%%!PS-Adobe-3.1 EPSF-3.0\n");
fprintf(f, "%%%%BoundingBox: 0 0 %d %d\n\n", w, h);

    fprintf(f, "newpath\n");
fprintf(f,"%g %g %g setrgbcolor\n", 0.5, .5, .5);

/*
fprintf(f, "%d %g moveto\n", 0,dotData->qRegions->scale * (8202 - dotData->qRegions->qStart));
fprintf(f, "%d %g lineto\n", w, dotData->qRegions->scale * (8202 - dotData->qRegions->qStart));
fprintf(f, "stroke\n");
fprintf(f, "%g %d moveto\n", dotData->qRegions->scale * (68814940 - dotData->tStart),0);
fprintf(f, "%g %d lineto\n",  dotData->qRegions->scale * (68814940 - dotData->tStart),h);
fprintf(f, "stroke\n");
fprintf(f, "%d %g moveto\n", 0,dotData->qRegions->scale * (8227 - dotData->qRegions->qStart));
fprintf(f, "%d %g lineto\n", w, dotData->qRegions->scale * (8227 - dotData->qRegions->qStart));
fprintf(f, "stroke\n");
fprintf(f, "%g %d moveto\n", dotData->qRegions->scale * (68810603 - dotData->tStart),0);
fprintf(f, "%g %d lineto\n",  dotData->qRegions->scale * (68810603 - dotData->tStart),h);
fprintf(f, "stroke\n");
*/

struct qRegion *qRegion = dotData->qRegions;
for(; qRegion; qRegion = qRegion->next)
    {
    fprintf(f, "/Times-Roman findfont\n");
    fprintf(f, "12 scalefont\n");
    fprintf(f, "setfont\n");
    fprintf(f, "newpath\n");
    struct dotLine *dotLine = qRegion->posLines;
    for(; dotLine; dotLine = dotLine->next)
        {
        unsigned color = snakePalette2[dotLine->id];
        double r,g,b;
        r =  (color & 0xff) / 255.0;
        g =  ((color & 0xff00) >> 8) / 255.0;
        b =  ((color & 0xff0000) >> 16) / 255.0;
        fprintf(f, "%g %g %g setrgbcolor\n", r,g,b);
        fprintf(f, "%d %d moveto\n", dotLine->tStart, dotLine->qStart);
        fprintf(f, "%d %d lineto\n", dotLine->tStart+dotLine->len, dotLine->qStart+dotLine->len);
        fprintf(f, "stroke\n");
        }
    dotLine = qRegion->negLines;
    for(; dotLine; dotLine = dotLine->next)
        {
        unsigned color = snakePalette2[dotLine->id];
        double r,g,b;
        r =  (color & 0xff) / 255.0;
        g =  ((color & 0xff00) >> 8) / 255.0;
        b =  ((color & 0xff0000) >> 16) / 255.0;
        fprintf(f, "%g %g %g setrgbcolor\n", r,g,b);
        fprintf(f, "%d %d moveto\n", dotLine->tStart, dotLine->qStart + dotLine->len);
        fprintf(f, "%d %d lineto\n", dotLine->tStart+dotLine->len, dotLine->qStart);
        fprintf(f, "stroke\n");

        }
    }
fprintf(f, "showpage\n");
carefulClose(&f);
}

static void drawDot( int w, int h, char *chrom, int start, int end,  struct psl *pslList)
{
struct tempName pngTn, psTn;

makeTempName(&pngTn, "dot", ".png");
makeTempName(&psTn, "dot", ".ps");
drawDotPs(psTn.forCgi, w, h, chrom, start, end, pslList);

char geoBuf[1024];
safef(geoBuf, sizeof geoBuf, "-g%dx%d", w, h);
char outputBuf[1024];
safef(outputBuf, sizeof outputBuf, "-sOutputFile=%s", pngTn.forCgi);
char *gsExe = "gs";
char *pipeCmd[] = {gsExe, "-sDEVICE=png16m", outputBuf, "-dBATCH","-dNOPAUSE","-q", geoBuf, psTn.forCgi, NULL};
struct pipeline *pl = pipelineOpen1(pipeCmd, pipelineWrite | pipelineNoAbort, "/dev/null", NULL, 0);
int sysRet = pipelineWait(pl);
if (sysRet != 0)
    errAbort("System call returned %d for:\n  %s", sysRet, pipelineDesc(pl));

remove(psTn.forCgi);

    printf("<IMG SRC=\"%s\" BORDER=1>", pngTn.forHtml);
}

void bigPslDotPlot(struct trackDb *tdb, struct bbiFile *bbi, char *chrom, int start, int end)
{
//unsigned seqTypeField =  bbExtraFieldIndex(bbi, "seqType");
unsigned seqTypeField =  0;
struct bigBedInterval *bb, *bbList = NULL;
struct lm *lm = lmInit(0);
bbList = bigBedIntervalQuery(bbi, seqName, start, end, 0, lm);
char *bedRow[32];
char startBuf[16], endBuf[16];
struct psl* pslList = NULL;
for (bb = bbList; bb != NULL; bb = bb->next)
    {
    bigBedIntervalToRow(bb, chrom, startBuf, endBuf, bedRow, 4);
    char *cdsStr, *seq;
    struct psl *psl= getPslAndSeq(tdb, chrom, bb, seqTypeField, &seq, &cdsStr);
    slAddHead(&pslList, psl);
    }
drawDot(1000,1000, chrom, start, end, pslList);
}
