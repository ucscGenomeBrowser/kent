/* wigAsciiAverageOverBed - Read wiggle file and bed file and emit file with average wig value over each bed record.. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "rbTree.h"
#include "obscure.h"
#include "sqlNum.h"
#include "../hg/inc/bed.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "wigAsciiAverageOverBed - Read wiggle file and bed file and emit file with average wig \n"
  "value over each bed record.  Currently the wiggle file must be fixedStep format, and the\n"
  "bed file bed4.  The output is bed4 plus one more column - the average wig value.\n"
  "usage:\n"
  "   wigAsciiAverageOverBed in.fixedWig in.bed out.tab\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct wigSection
/* A section of a wig file with values one per base. */
    {
    struct wigSection *next;	/* Next in list. */
    char *chrom;		/* Chromosome. */
    int chromStart, chromEnd;		/* Half open chromStart/end. */
    double *vals;		/* Values. */
    };

char *requiredVal(struct lineFile *lf, struct hash *hash, char *var)
/* Return value of given variable in hash.  Squawk and die if it's not there. */
{
char *val = hashFindVal(hash, var);
if (val == NULL)
   errAbort("Missing required %s field line %d of %s", var, lf->lineIx, lf->fileName);
return val;
}

struct wigSection *wigSectionRead(struct lineFile *lf)
/* Parse out next section of wig. */
{
static double *vals = NULL;
static int valAlloc = 0;

/* Get "fixedStep" line and parse it. */
char *line;
if (!lineFileNextReal(lf, &line))
    return NULL;
char *pattern = "fixedStep ";
int patSize = 10;
if (!startsWith(pattern, line))
    errAbort("Expecting fixedStep line %d of %s", lf->lineIx, lf->fileName);
line += patSize;
struct hash *varHash = hashVarLine(line, lf->lineIx);
int step = sqlUnsigned(requiredVal(lf, varHash, "step"));
int start = sqlUnsigned(requiredVal(lf, varHash, "start"));
char *chrom = cloneString(requiredVal(lf, varHash, "chrom"));
hashFree(&varHash);

/* Parse out numbers until next fixedStep. */
int valCount = 0;
int i;
for (;;)
    {
    if (!lineFileNextReal(lf, &line))
	break;
    if (startsWith(pattern, line))
	{
        lineFileReuse(lf);
	break;
	}
    for (i=0; i<step; ++i)
	{
	if (valCount >= valAlloc)
	    {
	    int newAlloc = valAlloc + 1024;
	    ExpandArray(vals, valAlloc, newAlloc);
	    valAlloc = newAlloc;
	    }
	vals[valCount] = lineFileNeedDouble(lf, &line, 0);
	++valCount;
	}
    }

/* Create wigSection. */
struct wigSection *section;
AllocVar(section);
section->chrom = chrom;
section->chromStart = start;
section->chromEnd = start + valCount;
section->vals = CloneArray(vals, valCount);
return section;
}

int bedRangeCmp(void *va, void *vb)
/* Return -1 if a before b,  0 if a and b overlap,
 * and 1 if a after b. */
{
struct bed *a = va;
struct bed *b = vb;
int diff = strcmp(a->chrom, b->chrom);
if (diff != 0)
    return diff;
if (a->chromEnd <= b->chromStart)
    return -1;
else if (b->chromEnd <= a->chromStart)
    return 1;
else
    return 0;
}



struct rbTree *wigIntoRangeTree(char *fileName)
/* Return a range tree full of wiggle records. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct rbTree *wigTree = rbTreeNew(bedRangeCmp);
struct wigSection *section;
while ((section = wigSectionRead(lf)) != NULL)
    rbTreeAdd(wigTree, section);
return wigTree;
}

int aveCount = 0;
double aveSum = 0;
struct bed *aveBed;

static void addToAve(void *v)
/* Callback to add item to average. */
{
struct wigSection *section = v;
int start = max(aveBed->chromStart, section->chromStart);
int end = min(aveBed->chromEnd, section->chromEnd);
int width = end - start;
if (width > 0)
    {
    aveCount += width;
    int endIx = end - section->chromStart;
    int ix;
    for (ix = start - section->chromStart; ix<endIx; ++ix)
        aveSum += section->vals[ix];
    }
}

double averageWigForBed(struct rbTree *wigTree, struct bed *bed)
/* Return average value for wig over bed.  Return 0 if no data. */
{
aveCount = 0;
aveSum = 0.0;
aveBed = bed;
rbTreeTraverseRange(wigTree, bed, bed, addToAve);
if (aveCount == 0)
    return 0;
else
    return aveSum/aveCount;
}

void wigAsciiAverageOverBed(char *inWig, char *inBed, char *out)
/* wigAsciiAverageOverBed - Read wiggle file and bed file and emit file with average wig value 
 * over each bed record.. */
{
struct rbTree *wigTree = wigIntoRangeTree(inWig);
verbose(1, "Read %d sections from %s\n", wigTree->n, inWig);
struct bed *bed, *bedList = bedLoadAll(inBed);
verbose(1, "Read %d items from %s\n", slCount(bedList), inBed);
FILE *f = mustOpen(out, "w");
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    fprintf(f, "%s\t%d\t%d\t%s\t%f\n", 
    	bed->chrom, bed->chromStart, bed->chromEnd, bed->name,
	averageWigForBed(wigTree, bed) );
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
wigAsciiAverageOverBed(argv[1], argv[2], argv[3]);
return 0;
}
