/* wigTestMaker - Create test wig files.. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "wigTestMaker - Create test wig files.\n"
  "usage:\n"
  "   wigTestMaker outDir\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void makeEmpty(char *fileName)
/* Make an empty file. */
{
FILE *f = mustOpen(fileName, "w");
carefulClose(&f);
}

void makeEmptyFixed(char *fileName)
/* Make short fixed step test. */
{
FILE *f = mustOpen(fileName, "w");
fprintf(f, "fixedStep chrom=chr1 span=10 start=100 step=10\n");
carefulClose(&f);
}

void makeShortFixed(char *fileName)
/* Make short fixed step test. */
{
FILE *f = mustOpen(fileName, "w");
fprintf(f, "fixedStep chrom=chr1 span=10 start=100 step=10\n");
fprintf(f, "1.0\n");
carefulClose(&f);
}

void makeEmptyVar(char *fileName)
/* Make short fixed step test. */
{
FILE *f = mustOpen(fileName, "w");
fprintf(f, "variableStep chrom=chr1 span=10\n");
carefulClose(&f);
}

void makeShortVar(char *fileName)
/* Make short fixed step test. */
{
FILE *f = mustOpen(fileName, "w");
fprintf(f, "variableStep chrom=chr1 span=10\n");
fprintf(f, "100 1.0\n");
carefulClose(&f);
}

void makeShortBed(char *fileName)
/* Make short fixed step test. */
{
FILE *f = mustOpen(fileName, "w");
fprintf(f, "chr1\t100\t110\t1.0\n");
carefulClose(&f);
}

void makeManyContigs(char *fileName, int contigCount)
/* Make a test bedGraph with many contigs. */
{
int i;
FILE *f = mustOpen(fileName, "w");
for (i=1; i<=contigCount; ++i)
    {
    fprintf(f, "contig%d\t%d\t%d\t%g\n", i, 1000000+i, 1000100+i, 1.0/i);
    fprintf(f, "contig%d\t%d\t%d\t%g\n", i, 1001000+i, 1001100+i, i*1.000001);
    }
carefulClose(&f);
}

#define TWOPI (3.1415 * 2.0)

void makeSineSineFixed(char *fileName, int innerRes, int outerRes, int outerCount, int chromCount)
/* Make a test set involving sine modulated sine waves in fixedStep format. */
{
FILE *f = mustOpen(fileName, "w");
int totalSteps = innerRes * outerRes * outerCount;
double innerStep = TWOPI/innerRes;
double outerStep = TWOPI/(innerRes*outerRes);
int chromIx;
for (chromIx=1; chromIx<=chromCount; ++chromIx)
    {
    fprintf(f, "fixedStep chrom=chr%d start=1 step=1 span=1\n", chromIx);
    double outerAngle = 0, innerAngle = 0;
    int i;
    for (i=0; i<totalSteps; ++i)
        {
	fprintf(f, "%f\n", 100.0*sin(innerAngle)*sin(outerAngle));
	innerAngle += innerStep;
	outerAngle += outerStep;
	}
    }
carefulClose(&f);
}

void makeSineSineVar(char *fileName, int innerRes, int outerRes, int outerCount, int chromCount)
/* Make a test set involving sine modulated sine waves in variableStep format. */
{
FILE *f = mustOpen(fileName, "w");
int totalSteps = innerRes * outerRes * outerCount;
double innerStep = TWOPI/innerRes;
double outerStep = TWOPI/(innerRes*outerRes);
int chromIx;
for (chromIx=1; chromIx<=chromCount; ++chromIx)
    {
    fprintf(f, "variableStep chrom=chr%d span=1\n", chromIx);
    double outerAngle = 0, innerAngle = 0;
    int i;
    for (i=0; i<totalSteps; ++i)
        {
	fprintf(f, "%d\t%f\n", i+1, 100.0*sin(innerAngle)*sin(outerAngle));
	innerAngle += innerStep;
	outerAngle += outerStep;
	}
    }
carefulClose(&f);
}

void makeSineSineBed(char *fileName, int innerRes, int outerRes, int outerCount, int chromCount)
/* Make a test set involving sine modulated sine waves in bedGraph format. */
{
FILE *f = mustOpen(fileName, "w");
int totalSteps = innerRes * outerRes * outerCount;
double innerStep = TWOPI/innerRes;
double outerStep = TWOPI/(innerRes*outerRes);
int chromIx;
for (chromIx=1; chromIx<=chromCount; ++chromIx)
    {
    double outerAngle = 0, innerAngle = 0;
    int i;
    for (i=0; i<totalSteps; ++i)
        {
	fprintf(f, "chr%d\t%d\t%d\t%f\n", chromIx, i, i+1, 100.0*sin(innerAngle)*sin(outerAngle));
	innerAngle += innerStep;
	outerAngle += outerStep;
	}
    }
carefulClose(&f);
}

void makeMixem(char *fileName)
/* Make a file that mixes up various types */
{
FILE *f = mustOpen(fileName, "w");
fprintf(f, "variableStep chrom=chr1\n");
fprintf(f, "100\t1.0\n");
fprintf(f, "200\t2.0\n");
fprintf(f, "fixedStep chrom=chr1 start=1000 step=2\n");
fprintf(f, "1.0\n");
fprintf(f, "2.0\n");
fprintf(f, "3.0\n");
fprintf(f, "4.0\n");
fprintf(f, "chr1\t10000\t10100\t100\n");
fprintf(f, "chr1\t20000\t20100\t200\n");
fprintf(f, "chr2\t10000\t10100\t100\n");
fprintf(f, "chr3\t10000\t10100\t100\n");
fprintf(f, "fixedStep chrom=chr11 start=1000 step=2\n");
fprintf(f, "11.0\n");
fprintf(f, "12.0\n");
fprintf(f, "13.0\n");
fprintf(f, "14.0\n");
carefulClose(&f);
}

void makeIncreasing(char *fileName)
/* Make simple fixed step wiggle with increasing sequence. */
{
FILE *f = mustOpen(fileName, "w");
int i;
double inc = 0.001;
double x = 0;
fprintf(f, "fixedStep chrom=chr1 start=1 step=10 span=5\n");
for (i=0; i<1000000; ++i)
    {
    fprintf(f, "%f\n", x);
    x += inc;
    }
carefulClose(&f);
}

void makeChromSizes(char *fileName, int count)
/* Make a chrom size file */
{
FILE *f = mustOpen(fileName, "w");
int i;
for (i=1; i<=count; ++i)
    fprintf(f, "chr%d\t%d\n", i, 100000000+i);
carefulClose(&f);
}

void makeContigSizes(char *fileName, int count)
/* Make scaffold size file. */
{
FILE *f = mustOpen(fileName, "w");
int i;
for (i=1; i<=count; ++i)
    fprintf(f, "contig%d\t%d\n", i, 2000000+i);
carefulClose(&f);
}
    
void wigTestMaker(char *outDir)
/* wigTestMaker - Create test wig files.. */
{
makeDir(outDir);
setCurrentDir(outDir);
makeChromSizes("chrom.sizes", 100);
makeEmpty("empty.wig");
makeEmptyFixed("emptyFixed.wig");
makeEmptyVar("emptyVar.wig");
makeShortFixed("shortFixed.wig");
makeShortVar("shortVar.wig");
makeShortBed("shortBed.wig");
makeSineSineFixed("sineSineFixed.wig", 1000, 500, 10, 8);
makeSineSineVar("sineSineVar.wig", 1000, 500, 10, 4);
makeSineSineBed("sineSineBed.wig", 1000, 500, 10, 2);
makeIncreasing("increasing.wig");
makeMixem("mixem.wig");
makeContigSizes("contig.sizes", 666666);
makeManyContigs("contigs.wig", 666666);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
wigTestMaker(argv[1]);
return 0;
}
