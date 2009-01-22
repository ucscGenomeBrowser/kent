/* bwTest - Test out some big wig related things.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "localmem.h"
#include "sqlNum.h"

static char const rcsid[] = "$Id: bwTest.c,v 1.1 2009/01/22 21:15:01 kent Exp $";

int blockSize = 1024;
int itemsPerSlot = 512;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bwTest - Test out some big wig related things.\n"
  "usage:\n"
  "   bwTest in.wig out.bw\n"
  "options:\n"
  "   -blockSize=N - Number of items to bundle in r-tree.  Default %d\n"
  "   -itemsPerSlot=N - Number of data points bundled at lowest level. Default %d\n"
  , blockSize, itemsPerSlot
  );
}

static struct optionSpec options[] = {
   {"blockSize", OPTION_INT},
   {"itemsPerSlot", OPTION_INT},
   {NULL, 0},
};

struct bigWigBedGraphItem
/* An bedGraph-type item in a bigWigSection. */
    {
    struct bigWigBedGraphItem *next;	/* Next in list. */
    bits32 start,end;		/* Range of chromosome covered. */
    double val;			/* Value. */
    };

struct bigWigVariableStepItem
/* An variableStep type item in a bigWigSection. */
    {
    struct bigWigVariableStepItem *next;	/* Next in list. */
    bits32 start;		/* Start position in chromosome. */
    double val;			/* Value. */
    };

struct bigWigFixedStepItem
/* An fixedStep type item in a bigWigSection. */
    {
    struct bigWigFixedStepItem *next;	/* Next in list. */
    double val;			/* Value. */
    };

enum bigWigSectionType 
/* Code to indicate section type. */
    {
    bwstBedGraph=1,
    bwstVariableStep=2,
    bwstFixedStep=3,
    };

union bigWigItem
/* Union of item pointers for all possible section types. */
    {
    struct bigWigBedGraphItem *bedGraph;
    struct bigWigVariableStepItem *variableStep;
    struct bigWigFixedStepItem *fixedStep;
    };


struct bigWigSection
/* A section of a bigWig file - all on same chrom */
    {
    struct bigWigSection *next;		/* Next in list. */
    char *chrom;			/* Chromosome name. */
    bits32 start,end;			/* Range of chromosome covered. */
    enum bigWigSectionType type;
    union bigWigItem itemList;		/* List of items in this section. */
    int itemStep;			/* Step within item if applicable. */
    int itemSpan;			/* Item span if applicable. */
    int itemCount;			/* Number of items in section. */
    };

void sectionWriteBedGraphAsAscii(struct bigWigSection *section, FILE *f)
/* Write out a bedGraph section. */
{
struct bigWigBedGraphItem *item;
for (item = section->itemList.bedGraph; item != NULL; item = item->next)
    fprintf(f, "%s\t%u\t%u\t%g\n",  section->chrom, (unsigned)item->start, (unsigned)item->end,
    	item->val);

}

void sectionWriteFixedStepAsAscii(struct bigWigSection *section, FILE *f)
/* Write out a fixedStep section. */
{
struct bigWigFixedStepItem *item;
fprintf(f, "fixedStep chrom=%s start=%u step=%d span=%d\n",
	section->chrom, (unsigned)section->start+1, section->itemStep, section->itemSpan);
for (item = section->itemList.fixedStep; item != NULL; item = item->next)
    fprintf(f, "%g\n", item->val);
}

void sectionWriteVariableStepAsAscii(struct bigWigSection *section, FILE *f)
/* Write out a variableStep section. */
{
struct bigWigVariableStepItem *item;
fprintf(f, "variableStep chrom=%s span=%d\n", section->chrom, section->itemSpan);
for (item = section->itemList.variableStep; item != NULL; item = item->next)
    fprintf(f, "%u\t%g\n", (unsigned)item->start+1, item->val);
}

void sectionWriteAsAscii(struct bigWigSection *section, FILE *f)
/* Write out ascii representation of section (which will be wig-file readable). */
{
switch (section->type)
    {
    case bwstBedGraph:
	sectionWriteBedGraphAsAscii(section, f);
	break;
    case bwstVariableStep:
	sectionWriteVariableStepAsAscii(section, f);
	break;
    case bwstFixedStep:
	sectionWriteFixedStepAsAscii(section, f);
	break;
    default:
        internalErr();
	break;
    }
}

boolean steppedSectionEnd(char *line, int maxWords)
/* Return TRUE if line indicates the start of another section. */
{
int wordCount = chopByWhite(line, NULL, 5);
if (wordCount > maxWords)
    return TRUE;
if (stringIn("chrom=", line))
    return TRUE;
return FALSE;
}

void parseFixedStepSection(struct lineFile *lf, struct lm *lm,
	char *chrom, bits32 span, bits32 sectionStart, 
	bits32 step, struct bigWigSection **pSectionList)
/* Read the single column data in section until get to end. */
{
/* Stream through section until get to end of file or next section,
 * adding values from single column to list. */
char *words[1];
int itemCount=0;
char *line;
struct bigWigFixedStepItem *item, *itemList = NULL;
while (lineFileNextReal(lf, &line))
    {
    if (steppedSectionEnd(line, 1))
	{
        lineFileReuse(lf);
	break;
	}
    chopLine(line, words);
    lmAllocVar(lm, item);
    item->val = lineFileNeedDouble(lf, words, 0);
    slAddHead(&itemList, item);
    ++itemCount;
    }
slReverse(&itemList);

/* Break up into sections of no more than items-per-slot size. */
struct bigWigFixedStepItem *startItem, *endItem, *nextStartItem = itemList;
for (startItem = nextStartItem; startItem != NULL; startItem = nextStartItem)
    {
    /* Find end item of this section, and start item for next section.
     * Terminate list at end item. */
    int sectionSize = 0;
    int i;
    endItem = startItem;
    for (i=0; i<itemsPerSlot; ++i)
        {
	if (nextStartItem == NULL)
	    break;
	endItem = nextStartItem;
	nextStartItem = nextStartItem->next;
	++sectionSize;
	}
    endItem->next = NULL;

    /* Fill in section and add it to list. */
    struct bigWigSection *section;
    lmAllocVar(lm, section);
    section->chrom = chrom;
    section->start = sectionStart;
    sectionStart += sectionSize * step;
    section->end = sectionStart - step + span;
    section->type = bwstFixedStep;
    section->itemList.fixedStep = startItem;
    section->itemStep = step;
    section->itemSpan = span;
    section->itemCount = sectionSize;
    slAddHead(pSectionList, section);
    }
}

void parseVariableStepSection(struct lineFile *lf, struct lm *lm,
	char *chrom, bits32 span, struct bigWigSection **pSectionList)
/* Read the single column data in section until get to end. */
{
/* Stream through section until get to end of file or next section,
 * adding values from single column to list. */
char *words[2];
int itemCount=0;
char *line;
struct bigWigVariableStepItem *item, *itemList = NULL;
while (lineFileNextReal(lf, &line))
    {
    if (steppedSectionEnd(line, 2))
	{
        lineFileReuse(lf);
	break;
	}
    chopLine(line, words);
    lmAllocVar(lm, item);
    item->start = lineFileNeedNum(lf, words, 0) - 1;
    item->val = lineFileNeedDouble(lf, words, 1);
    slAddHead(&itemList, item);
    ++itemCount;
    }
slReverse(&itemList);

/* Break up into sections of no more than items-per-slot size. */
struct bigWigVariableStepItem *startItem, *endItem, *nextStartItem = itemList;
for (startItem = itemList; startItem != NULL; startItem = nextStartItem)
    {
    /* Find end item of this section, and start item for next section.
     * Terminate list at end item. */
    int sectionSize = 0;
    int i;
    endItem = startItem;
    for (i=0; i<itemsPerSlot; ++i)
        {
	if (nextStartItem == NULL)
	    break;
	endItem = nextStartItem;
	nextStartItem = nextStartItem->next;
	++sectionSize;
	}
    endItem->next = NULL;

    /* Fill in section and add it to list. */
    struct bigWigSection *section;
    lmAllocVar(lm, section);
    section->chrom = chrom;
    section->start = startItem->start;
    section->end = endItem->start + span;
    section->type = bwstVariableStep;
    section->itemList.variableStep = startItem;
    section->itemSpan = span;
    section->itemCount = sectionSize;
    slAddHead(pSectionList, section);
    }
}

unsigned parseUnsignedVal(struct lineFile *lf, char *var, char *val)
/* Return val as an integer, printing error message if it's not. */
{
char c = val[0];
if (!isdigit(c))
    errAbort("Expecting numerical value for %s, got %s, line %d of %s", 
    	var, val, lf->lineIx, lf->fileName);
return sqlUnsigned(val);
}

void parseSteppedSection(struct lineFile *lf, char *initialLine, 
	struct lm *lm, struct bigWigSection **pList)
/* Parse out a variableStep or fixedStep section and add it to list, breaking it up as need be. */
{
/* Parse out first word of initial line and make sure it is something we recognize. */
char *typeWord = nextWord(&initialLine);
enum bigWigSectionType type = bwstFixedStep;
if (sameString(typeWord, "variableStep"))
    type = bwstVariableStep;
else if (sameString(typeWord, "fixedStep"))
    type = bwstFixedStep;
else
    errAbort("Unknown type %s\n", typeWord);

/* Set up defaults for values we hope to parse out of rest of line. */
int span = 1;
bits32 step = 0;
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
    else if (sameString(var, "span"))
	span = parseUnsignedVal(lf, var, val);
    else if (sameString(var, "step"))
	step = parseUnsignedVal(lf, var, val);
    else if (sameString(var, "start"))
        start = parseUnsignedVal(lf, var, val);
    else
	errAbort("Unknown setting %s=%s line %d of %s", var, val, lf->lineIx, lf->fileName);
    }

/* Check that we have all that are required and no more, and call type-specific routine to parse
 * rest of section. */
if (chrom == NULL)
    errAbort("Missing chrom= setting line %d of %s\n", lf->lineIx, lf->fileName);
if (type == bwstFixedStep)
    {
    if (start == 0)
	errAbort("Missing start= setting line %d of %s\n", lf->lineIx, lf->fileName);
    if (step == 0)
	errAbort("Missing step= setting line %d of %s\n", lf->lineIx, lf->fileName);
    parseFixedStepSection(lf, lm, chrom, span, start-1, step, pList);
    }
else
    {
    if (start != 0)
	errAbort("Extra start= setting line %d of %s\n", lf->lineIx, lf->fileName);
    if (step != 0)
	errAbort("Extra step= setting line %d of %s\n", lf->lineIx, lf->fileName);
    parseVariableStepSection(lf, lm, chrom, span, pList);
    }
}

void bwTest(char *inName, char *outName)
/* bwTest - Test out some big wig related things. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
FILE *f = mustOpen(outName, "wb");
char *line;
struct bigWigSection *sectionList = NULL;
struct lm *lm = lmInit(0);
while (lineFileNextReal(lf, &line))
    {
    verbose(2, "processing %s\n", line);
    if (stringIn("chrom=", line))
	parseSteppedSection(lf, line, lm, &sectionList);
    else
        {
	/* Check for bed... */
	char *dupe = cloneString(line);
	char *words[5];
	int wordCount = chopLine(dupe, words);
	if (wordCount != 4)
	    errAbort("Unrecognized line %d of %s:\n%s\n", lf->lineIx, lf->fileName, line);

	/* Parse out a bed graph line just to check numerical format. */
	char *chrom = words[0];
	int start = lineFileNeedNum(lf, words, 1);
	int end = lineFileNeedNum(lf, words, 2);
	double val = lineFileNeedDouble(lf, words, 3);

#ifdef SOON
	/* Push back line and call bed parser. */
	lineFileReuse(lf);
	parseBedSection(lf, &sectionList);
#endif /* SOON */
	}
    }

slReverse(&sectionList);
struct bigWigSection *section;
for (section = sectionList; section != NULL; section = section->next)
    {
    sectionWriteAsAscii(section, f);
    }


carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
blockSize = optionInt("blockSize", blockSize);
itemsPerSlot = optionInt("itemsPerSlot", itemsPerSlot);
if (argc != 3)
    usage();
bwTest(argv[1], argv[2]);
return 0;
}
