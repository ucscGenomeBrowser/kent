/* wigSort - Sort a wig file.. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "bigWig.h"
#include "bwgInternal.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "wigSort - Sort a wig file.\n"
  "usage:\n"
  "   wigSort in.wig out.wig\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct pos
/* A chromosome position and a file position rolled into one. */
    {
    struct pos *next;
    bits64 fileOffset;	/* Position relative to start of file. */
    bits64 fileSize;	/* Size in file. */
    char *chrom;	/* Chromosome. */
    bits32 start;	/* Chromosome start. */
    };

static int posCmp(const void *va, const void *vb)
/* Compare to sort based on query start. */
{
const struct pos *a = *((struct pos **)va);
const struct pos *b = *((struct pos **)vb);
int dif = strcmp(a->chrom, b->chrom);
if (dif == 0)
    dif = (int)a->start - (int)b->start;
return dif;
}

static boolean stepTypeLine(char *line)
/* Return TRUE if it's a variableStep or fixedStep line. */
{
return (startsWithWord("variableStep", line) || startsWithWord("fixedStep", line));
}

static boolean steppedSectionEnd(char *line, int maxWords)
/* Return TRUE if line indicates the start of another section. */
{
int wordCount = chopByWhite(line, NULL, 5);
if (wordCount > maxWords)
    return TRUE;
return stepTypeLine(line);
}

static unsigned parseUnsignedVal(struct lineFile *lf, char *var, char *val)
/* Return val as an integer, printing error message if it's not. */
{
char c = val[0];
if (!isdigit(c))
    errAbort("Expecting numerical value for %s, got %s, line %d of %s", 
    	var, val, lf->lineIx, lf->fileName);
return sqlUnsigned(val);
}

static void parseSteppedSection(struct lineFile *lf, char *initialLine, struct pos *pos)
/* Parse out stepped section, adding info about it to pos. */
{
/* Parse out first word of initial line and make sure it is something we recognize. */
char *typeWord = nextWord(&initialLine);
enum bwgSectionType type = bwgTypeFixedStep;
if (sameString(typeWord, "variableStep"))
    type = bwgTypeVariableStep;
else if (sameString(typeWord, "fixedStep"))
    type = bwgTypeFixedStep;
else
    errAbort("Unknown type %s\n", typeWord);

/* Set up defaults for values we hope to parse out of rest of line. */
bits32 start = 0;
char *chrom = NULL;

/* Parse out var=val pairs. */
char *varEqVal;
while ((varEqVal = nextWord(&initialLine)) != NULL)
    {
    char *wordPairs[2];
    int wc = chopByChar(varEqVal, '=', wordPairs, 2);
    if (wc != 2)
        errAbort("strange var=val pair line %d of %s", lf->lineIx, lf->fileName);
    char *var = wordPairs[0];
    char *val = wordPairs[1];
    if (sameString(var, "chrom"))
        chrom = cloneString(val);
    else if (sameString(var, "start"))
        start = parseUnsignedVal(lf, var, val);
    }

/* Check that we have all that are required and no more, and call type-specific routine to parse
 * rest of section. */
if (chrom == NULL)
    errAbort("Missing chrom= setting line %d of %s\n", lf->lineIx, lf->fileName);
pos->chrom = chrom;
if (type == bwgTypeFixedStep)
    {
    if (start == 0)
	errAbort("Missing start= setting line %d of %s\n", lf->lineIx, lf->fileName);
    for (;;)
        {
	char *line;
	if (!lineFileNextReal(lf, &line))
	    break;
	if (steppedSectionEnd(line, 1))
	    {
	    lineFileReuse(lf);
	    break;
	    }
	}
    }
else
    {
    for (;;)
        {
	char *line;
	if (!lineFileNextReal(lf, &line))
	    break;
	if (steppedSectionEnd(line, 2))
	    {
	    lineFileReuse(lf);
	    break;
	    }
	bits32 s = atoi(line);
	if (start == 0 || s < start)
	     start = s;
	}
    }
pos->start = start-1;
}

void copyFileBytes(FILE *in, FILE *out, bits64 size)
/* Copy bytes to a file. */
{
char buf[4*1024];
bits64 sizeLeft = size;
bits64 oneSize;
while (sizeLeft > 0)
    {
    oneSize = sizeLeft;
    if (oneSize > sizeof(buf))
        oneSize = sizeof(buf);
    mustRead(in, buf, oneSize);
    mustWrite(out, buf, oneSize);
    sizeLeft -= oneSize;
    }
}

void wigSort(char *input, char *output)
/* wigSort - Sort a wig file.. */
{
struct lineFile *lf = lineFileOpen(input, TRUE);
struct pos *pos, *posList = NULL;
char *line;
while (lineFileNextReal(lf, &line))
    {
    verbose(2, "processing %s\n", line);
    AllocVar(pos);
    pos->fileOffset = lineFileTell(lf);
    if (posList != NULL)
        posList->fileSize = pos->fileOffset - posList->fileOffset;
    slAddHead(&posList, pos);
    if (stringIn("chrom=", line))
	{
	parseSteppedSection(lf, line, pos);
	}
    else
        {
	/* Check for bed... */
	char *words[5];
	int wordCount = chopLine(line, words);
	if (wordCount != 4)
	    errAbort("Unrecognized format line %d of %s:\n", lf->lineIx, lf->fileName);
	pos->chrom = cloneString(words[0]);
	pos->start = lineFileNeedNum(lf, words, 1);
	}
    }
if (posList != NULL)
    {
    posList->fileSize = lineFileTell(lf) - posList->fileOffset;
    slReverse(&posList);
    slSort(&posList, posCmp);
    }
lineFileClose(&lf);

FILE *in = mustOpen(input, "r");
FILE *out = mustOpen(output, "w");
for (pos = posList; pos != NULL; pos = pos->next)
    {
    fseek(in, pos->fileOffset, SEEK_SET);
    copyFileBytes(in, out, pos->fileSize);
    }
carefulClose(&in);
carefulClose(&out);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
wigSort(argv[1], argv[2]);
return 0;
}
