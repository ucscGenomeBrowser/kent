/* genePredHisto - get data for generating histograms from a genePred file. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "options.h"
#include "genePred.h"
#include "genePredReader.h"


/* Command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"ids", OPTION_BOOLEAN},
    {NULL, 0}
};
static boolean clIds = FALSE;  /* print ids */

/* type for function used to get histogram data */
typedef void (*histoFuncType)(struct genePred *gp, FILE *outFh);

void usage(char *msg)
/* Explain usage and exit. */
{
errAbort("%s\n\n"
         "genePredHisto - get data for generating histograms from a genePred file.\n"
         "usage:\n"
         "   genePredHisto [options] what genePredFile histoOut\n"
         "\n"
         "Options:\n"
         "  -ids - a second column with the gene name, useful for finding outliers.\n"
         "\n"
         "The what arguments indicates the type of output. The output file is\n"
         "a list of numbers suitable for input to textHistogram or similar\n"
         "The following values are current implemented\n"
         "   exonLen- length of exons\n"
         "   5utrExonLen- length of 5'UTR regions of exons\n"
         "   cdsExonLen- length of CDS regions of exons\n"
         "   3utrExonLen- length of 3'UTR regions of exons\n"
         "   exonCnt- count of exons\n"
         "   5utrExonCnt- count of exons containing 5'UTR\n"
         "   cdsExonCnt- count of exons count CDS\n"
         "   3utrExonCnt- count of exons containing 3'UTR\n",
         msg);
}

struct range
/* strand and end range */
{
    int start;
    int end;
};
struct featBounds
/* bounds of each feature within an exon */
{
    struct range utr5;
    struct range cds;
    struct range utr3;
};

static struct featBounds getFeatures(struct genePred *gp, int iExon)
/* get the bounds of the features within an exon */
{
int start = gp->exonStarts[iExon];
int end = gp->exonEnds[iExon];
struct featBounds fb;
ZeroVar(&fb);

if (start < gp->cdsStart)
    {
    /* has initial UTR */
    struct range *utr = (gp->strand[0] == '+') ? &fb.utr5 : &fb.utr3;
    utr->start = start;
    utr->end = (end < gp->cdsStart) ? end : gp->cdsStart;
    start = utr->end;
    }

if ((gp->cdsStart < end) && (gp->cdsEnd > start))
    {
    /* has CDS */
    fb.cds.start = start;
    fb.cds.end = (end < gp->cdsEnd) ? end : gp->cdsEnd;
    start = fb.cds.end;
    }

if (start >= gp->cdsEnd)
    {
    /* has terminal UTR */
    struct range *utr = (gp->strand[0] == '+') ? &fb.utr3 : &fb.utr5;
    utr->start = start;
    utr->end = end;
    }
return fb;
}

static void writeInt(FILE *outFh, struct genePred *gp, int val)
/* write a row with an integer value */
{
fprintf(outFh, "%d", val);
if (clIds)
    fprintf(outFh, "\t%s", gp->name);
fputc('\n', outFh);
}

static void exonLenHisto(struct genePred *gp, FILE *outFh)
/* get exon lengths */
{
int i;
for (i = 0; i < gp->exonCount; i++)
    writeInt(outFh, gp, (gp->exonEnds[i]-gp->exonStarts[i]));
}

static void utr5LenHisto(struct genePred *gp, FILE *outFh)
/* get 5'UTR exon lengths */
{
int i;
for (i = 0; i < gp->exonCount; i++)
    {
    struct featBounds fb = getFeatures(gp,  i);
    if (fb.utr5.start < fb.utr5.end)
        writeInt(outFh, gp, (fb.utr5.end - fb.utr5.start));
    }
}

static void cdsLenHisto(struct genePred *gp, FILE *outFh)
/* get CDS exon lengths */
{
int i;
for (i = 0; i < gp->exonCount; i++)
    {
    struct featBounds fb = getFeatures(gp,  i);
    if (fb.cds.start < fb.cds.end)
        writeInt(outFh, gp, (fb.cds.end - fb.cds.start));
    }
}

static void utr3LenHisto(struct genePred *gp, FILE *outFh)
/* get 3'UTR exon lengths */
{
int i;
for (i = 0; i < gp->exonCount; i++)
    {
    struct featBounds fb = getFeatures(gp,  i);
    if (fb.utr3.start < fb.utr3.end)
        writeInt(outFh, gp, (fb.utr3.end - fb.utr3.start));
    }
}

static void exonCntHisto(struct genePred *gp, FILE *outFh)
/* get exon counts */
{
writeInt(outFh, gp, gp->exonCount);
}

static void utr5CntHisto(struct genePred *gp, FILE *outFh)
/* get 5'UTR exon counts */
{
int i, c = 0;
for (i = 0; i < gp->exonCount; i++)
    {
    struct featBounds fb = getFeatures(gp,  i);
    if (fb.utr5.start < fb.utr5.end)
        c++;
    }
writeInt(outFh, gp, c);
}

static void cdsCntHisto(struct genePred *gp, FILE *outFh)
/* get CDS exon counts */
{
int i, c = 0;
for (i = 0; i < gp->exonCount; i++)
    {
    struct featBounds fb = getFeatures(gp,  i);
    if (fb.cds.start < fb.cds.end)
        c++;
    }
writeInt(outFh, gp, c);
}

static void utr3CntHisto(struct genePred *gp, FILE *outFh)
/* get 3'UTR exon counts */
{
int i, c = 0;
for (i = 0; i < gp->exonCount; i++)
    {
    struct featBounds fb = getFeatures(gp,  i);
    if (fb.utr3.start < fb.utr3.end)
        c++;
    }
writeInt(outFh, gp, c);
}

static histoFuncType getHistoFunc(char *what)
/* look up the stats function to use */
{
if (sameString(what, "exonLen"))
    return exonLenHisto;
else if (sameString(what, "5utrExonLen"))
    return utr5LenHisto;
else if (sameString(what, "cdsExonLen"))
    return cdsLenHisto;
else if (sameString(what, "3utrExonLen"))
    return utr3LenHisto;
else if (sameString(what, "exonCnt"))
    return exonCntHisto;
else if (sameString(what, "5utrExonCnt"))
    return utr5CntHisto;
else if (sameString(what, "cdsExonCnt"))
    return cdsCntHisto;
else if (sameString(what, "3utrExonCnt"))
    return utr3CntHisto;
else
    usage("invalid what argument");
return NULL;
}

static void genePredHisto(char *what, char *gpFile, char *outFile)
/* get data for generating histograms from a genePred file. */
{
struct genePredReader *gpr = genePredReaderFile(gpFile, NULL);
histoFuncType histoFunc = getHistoFunc(what);
struct genePred *gp;
FILE *outFh = mustOpen(outFile, "w");

while ((gp = genePredReaderNext(gpr)) != NULL)
    {
    histoFunc(gp, outFh);
    genePredFree(&gp);
    }
carefulClose(&outFh);
genePredReaderFree(&gpr);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 4)
    usage("wrong number of arguments");
clIds = optionExists("ids");
genePredHisto(argv[1], argv[2], argv[3]);
return 0;
}
