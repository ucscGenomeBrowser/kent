/* wigSetSpanOrStep - Set span or step variables in an ascii format wiggle file. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"


boolean gotStep = FALSE;
boolean gotSpan = FALSE;
int step = 1;
int span = 1;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "wigSetSpanOrStep - Set span or step variables in an ascii format wiggle file\n"
  "usage:\n"
  "   wigSetSpanOrStep input.wig output.wig\n"
  "options:\n"
  "   -span=N set span variable to given value\n"
  "   -step=N set step variable to given value\n"
  "Either step or span must be used with fixedStep format wigs,  only span with variableStep.\n"
  );
}

static struct optionSpec options[] = {
   {"step", OPTION_INT},
   {"span", OPTION_INT},
   {NULL, 0},
};

static void handleStepLine(struct lineFile *lf, char *line, int firstWordSize,
	boolean isVarStep, FILE *f)
/* Handle a line that starts with fixedStep or variableStep */
{
char *firstPair = skipLeadingSpaces(line + firstWordSize);
mustWrite(f, line, firstWordSize);
struct slPair *pair, *pairList = slPairFromString(firstPair);
boolean seenStep = FALSE, seenSpan = FALSE;
for (pair = pairList; pair != NULL; pair = pair->next)
    {
    if (gotStep && sameString(pair->name, "step"))
        {
	fprintf(f, " step=%d", step);
	seenStep = TRUE;
	}
    else if (gotSpan && sameString(pair->name, "span"))
        {
	fprintf(f, " span=%d", span);
	seenSpan = TRUE;
	}
    else
        {
	fprintf(f, " %s=%s", pair->name, (char *)pair->val);
	}
    }
if (gotStep && !seenStep)
    {
    if (isVarStep)
        errAbort("%s isn't fixed step line %d, can't set step", lf->fileName, lf->lineIx);
    fprintf(f, " step=%d", step);
    }
if (gotSpan && !seenSpan)
    fprintf(f, " span=%d", span);
fprintf(f, "\n");
}

void wigSetSpanOrStep(char *input, char *output)
/* wigSetSpanOrStep - Set span or step variables in an ascii format wiggle file. */
{
struct lineFile *lf = lineFileOpen(input, TRUE);
FILE *f = mustOpen(output, "w");
char *line;
int lineSize;
char *fixedPat = "fixedStep";
int fixedPatSize = strlen(fixedPat);
char *varPat = "variableStep";
int varPatSize = strlen(varPat);
while (lineFileNext(lf, &line, &lineSize))
    {
    if (startsWithWord(fixedPat, line))
	handleStepLine(lf, line, fixedPatSize, FALSE, f);
    else if (startsWithWord(varPat, line))
	handleStepLine(lf, line, varPatSize, TRUE, f);
    else
	{
        mustWrite(f, line, lineSize-1);
	fputc('\n', f);
	}
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
step = optionInt("step", 0);
span = optionInt("span", 0);
if (step != 0)
    gotStep = TRUE;
if (span != 0)
    gotSpan = TRUE;
if (!step && !span)
    usage();
wigSetSpanOrStep(argv[1], argv[2]);
return 0;
}
