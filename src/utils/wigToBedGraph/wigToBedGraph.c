/* wigToBedGraph - Convert wig files to bedGraph, merging adjacent items with identical 
 * values when possible. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "sqlNum.h"


boolean clNoCollapse = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "wigToBedGraph - Convert wig files to bedGraph, merging adjacent items with identical values \n"
  "when possible.\n"
  "usage:\n"
  "   wigToBedGraph input.wig output.bedGraph\n"
  "options:\n"
  "   -noCollapse - do not collapse adjacent items of same value into single bedGraph record\n"
  );
}

static struct optionSpec options[] = {
   {"noCollapse", OPTION_BOOLEAN},
   {NULL, 0},
};


struct bgOut
/* Facilitate output of bedGraphs, with option to collapse where appropriate. */
    {
    boolean gotData;	/* TRUE if have a valid value. */
    FILE *f;		/* File to write to. */
    char *chrom;	/* Chromosome. */
    int chromAlloc;	/* Max memory size of chrom string. */
    int start, end;	/* Half open zero based range. */
    double val;		/* Value within range. */
    };

struct bgOut *bgOutNew(FILE *f)
/* Wrap an output buffer/collapser around file. */
{
struct bgOut *out;
AllocVar(out);
out->f = f;
out->chromAlloc=64;
out->chrom = needMem(out->chromAlloc);
return out;
}

void bgOutFlush(struct bgOut *out)
/* Write out any pending data to file. */
{
if (out->gotData)
    {
    fprintf(out->f, "%s\t%d\t%d\t%g\n", out->chrom, out->start, out->end, out->val);
    out->gotData = FALSE;
    }
}

void bgOutWrite(struct bgOut *out, char *chrom, int start, int end, double val)
/* Store data to output stream buffering and maybe collapsing a tiny bit. */
{
if (clNoCollapse || !out->gotData || start != out->end || val != out->val
	|| !sameString(chrom, out->chrom))
    {
    bgOutFlush(out);
    int len = strlen(chrom) + 1;
    if (len > out->chromAlloc)
        {
	out->chromAlloc = len;
	out->chrom = needMoreMem(out->chrom, 0, len);
	}
    strcpy(out->chrom, chrom);
    out->start = start;
    out->end = end;
    out->val = val;
    out->gotData = TRUE;
    }
else
    {
    out->end = end;
    }
}

void bgOutFree(struct bgOut **pOut)
/* Close and free bgOut. */
{
struct bgOut *out = *pOut;
if (out != NULL)
    {
    bgOutFlush(out);
    freeMem(out->chrom);
    freez(pOut);
    }
}

char *requiredVar(struct hash *hash, char *varName, struct lineFile *lf)
/* Look up varName in hash and return value.  Give error including line number if
 * can't find it. */
{
char *val = hashFindVal(hash, varName);
if (val == NULL)
    errAbort("Missing required %s= line %d of %s", varName, lf->lineIx, lf->fileName);
return val;
}

char *optionalVar(struct hash *hash, char *varName, char *defaultVal)
/* Look up varName in hash and return associated value.  Return defaultVal if can't find it. */
{
char *val = hashFindVal(hash, varName);
if (val == NULL)
    val = defaultVal;
return val;
}

void convertFixedStepSection(struct lineFile *lf, struct hash *vars, struct bgOut *out)
/* Read through section and output. */
{
char *chrom = requiredVar(vars, "chrom", lf);
int start = sqlUnsigned(requiredVar(vars, "start", lf)) - 1;
char *spanString = optionalVar(vars, "span", "1");
int span = sqlUnsigned(spanString);
int step = sqlUnsigned(optionalVar(vars, "step", spanString));
char *line;
while (lineFileNextReal(lf, &line))
    {
    line = skipLeadingSpaces(line);
    if (isalpha(line[0]))
	{
        lineFileReuse(lf);
	break;
	}
    eraseTrailingSpaces(line);
    double val = sqlDouble(line);
    bgOutWrite(out, chrom, start, start+span, val);
    start += step;
    }
}

void convertVariableStepSection(struct lineFile *lf, struct hash *vars, struct bgOut *out)
/* Read through section and output. */
{
char *chrom = requiredVar(vars, "chrom", lf);
int span = sqlUnsigned(optionalVar(vars, "span", "1"));
char *line;
while (lineFileNextReal(lf, &line))
    {
    line = skipLeadingSpaces(line);
    if (isalpha(line[0]))
	{
        lineFileReuse(lf);
	break;
	}
    char *words[3];
    int wordCount = chopLine(line, words);
    if (wordCount != 2)
        errAbort("Expecting exactly two numbers line %d of %s", lf->lineIx, lf->fileName);
    int start = lineFileNeedNum(lf, words, 0) - 1;
    double val = lineFileNeedDouble(lf, words, 1);
    bgOutWrite(out, chrom, start, start+span, val);
    }
}


void wigToBedGraph(char *wigIn, char *bedOut)
/* wigToBedGraph - Convert wig files to bedGraph, merging adjacent items with identical values 
 * when possible.. */
{
struct lineFile *lf = lineFileOpen(wigIn, TRUE);
FILE *f = mustOpen(bedOut, "w");
struct bgOut *out = bgOutNew(f);
char *line;
while (lineFileNextReal(lf, &line))
    {
    char *firstWord = nextWord(&line);
    struct hash *vars = hashVarLine(line, lf->lineIx);
    if (sameString("fixedStep", firstWord))
        convertFixedStepSection(lf, vars, out);
    else if (sameString("variableStep", firstWord))
        convertVariableStepSection(lf, vars, out);
    else
        errAbort("Expecting fixedStep or variableStep line %d of %s, got:\n\t%s", lf->lineIx,
		lf->fileName, line);
    freeHashAndVals(&vars);
    }
bgOutFree(&out);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
clNoCollapse = optionExists("noCollapse");
wigToBedGraph(argv[1], argv[2]);
return 0;
}
