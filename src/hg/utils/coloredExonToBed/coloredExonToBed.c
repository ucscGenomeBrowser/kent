/* coloredExonToBed - Convenience utility to make simple-formatted colored-exon beds into standard bed 14.. */

/* Copyright (C) 2007 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bed.h"
#include "coloredExon.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "coloredExonToBed - Convenience utility to make simple-formatted colored-exon beds into standard bed 14.\n"
  "usage:\n"
  "   coloredExonToBed input.coloredExonBed output.bed\n"
  "where the input bed file is has 5-13 columns and the last column is a list\n"
  "of strings formatted in HTML hex colors like #FFFFFF or #ea1.  The number\n"
  "of colors in the list must (a) correspond to the blockCount field (field\n"
  "10) or (b) be a single color if the number of fields < 11 overall.\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

char *checkColor(char *color, int lineIx)
/* check a color string so it's #xxyyzz or #xyz and that the numbers */
/* are in the correct range. Then return the #xxyyzz version */
{
int i;
int len = strlen(color);
char *ret;
touppers(color);
if ((len != 4) && (len != 7))
    errAbort("color %s badly formatted in file on line %d", color, lineIx);
for (i = 1; i < len; i++)
    {
    if (!isxdigit(color[i]))
	errAbort("color %s badly formatted in file on line %d", color, lineIx);
    }
if (len == 4)
    {
    ret = needMem(8 * sizeof(char));
    ret[0] = '#';
    ret[7] = '\0';
    for (i = 1; i < len; i++)
	{
	ret[i*2] = color[i];
	ret[i*2-1] = color[i];
	}
    }
else
    ret = cloneString(color);
return ret;
}

struct coloredExon *loadAll(char *fileName)
/* Determines how many fields are in a file first. */
{
struct coloredExon *list = NULL;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
int numFields = 0, nf = 0;
char *words[13];
boolean byTab = FALSE;
/* If there is something in the file then read it. If file
   is empty return NULL. */
while ((nf = (byTab) ? lineFileChopTab(lf, words) :
	lineFileChop(lf, words)) > 0)
    {
    struct coloredExon *ce;
    int numColors = 0, i;
    char *colors[2048];
    char *colorWord;
    if ((numFields > 0) && (nf != numFields))
	errAbort("file %s has inconsistent # of fields. Line %d has %d fields (expected %d)",
		 fileName, lf->lineIx, nf, numFields);
    else if (numFields == 0)
	numFields = nf;
    if ((numFields < 5) || (numFields > 13)) /* Minimum number of fields. */
	errAbort("file %s doesn't appear to be in bed format. 4-12 fields (plus colors) required, got %d",
		 fileName, numFields);
    /* Now load them up with that number of fields. */
    ce = (struct coloredExon *)bedLoadN(words, numFields-1);
    makeItBed12((struct bed *)ce, numFields-1);
    colorWord = words[numFields-1];
    if (lastChar(colorWord) == ',')
	colorWord[strlen(colorWord)-1] = '\0';
    numColors = chopCommas(colorWord, colors);
    if (numColors != ce->blockCount)
	errAbort("file %s doesn't have enough colors on line %d", fileName, lf->lineIx);
    AllocArray(ce->colors, numColors);
    for (i = 0; i < numColors; i++)
	{
	/* Check format of each color is #xxyyzz or #xyz */
	ce->colors[i] = checkColor(colors[i], lf->lineIx);
	}
    slAddHead(&list, ce);
    }
lineFileClose(&lf);
slReverse(&list);
return list;
}

void bed13FromCe(struct coloredExon *ce, struct bed *bed)
/* This is just a shallow-copy of the first 13 fields in the coloredExon. */
{
bed->chrom = ce->chrom;
bed->chromStart = ce->chromStart;
bed->chromEnd = ce->chromEnd;
bed->name = ce->name;
bed->score = ce->score;
bed->strand[0] = ce->strand[0];
bed->strand[1] = ce->strand[1];
bed->thickStart = ce->thickStart;
bed->thickEnd = ce->thickEnd;
bed->itemRgb = ce->reserved;
bed->blockCount = ce->blockCount;
bed->blockSizes = ce->blockSizes;
bed->chromStarts = ce->chromStarts;
bed->expCount = bed->blockCount;
}

void coloredExonToBed(char *input, char *output)
/* coloredExonToBed - Convenience utility to make simple-formatted colored-exon beds */
/* into standard bed 14.. */
{
struct coloredExon *ceList = loadAll(input);
struct coloredExon *cur;
FILE *f = mustOpen(output, "w");
for (cur = ceList; cur != NULL; cur = cur->next)
    {
    struct bed mybed;
    int numColors = cur->blockCount;
    int i;
    int *colors;
    bed13FromCe(cur, &mybed);
    AllocArray(colors, numColors);
    for (i = 0; i < numColors; i++)
	sscanf(cur->colors[i], "#%x", &colors[i]);
    mybed.expIds = colors;
    bedTabOutN(&mybed, 14, f);
    freeMem(colors);
    }
carefulClose(&f);
coloredExonFreeList(&ceList);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
coloredExonToBed(argv[1], argv[2]);
return 0;
}
