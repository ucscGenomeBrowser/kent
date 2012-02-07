/* subsetAxt - Rescore alignments and output those over threshold. 
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"
#include "axt.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "subsetAxt - Rescore alignments and output those over threshold\n"
  "usage:\n"
  "   subsetAxt in.axt out.axt matrix threshold\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

boolean findHsp(char *as, char *bs, int size, 
	struct axtScoreScheme *ss, int minScore,
	int *retStart, int *retEnd, int *retScore)
/* Find first HSP between a and b. */
{
int start = 0, best = 0, bestScore = 0, end;
int score = 0;
char a,b;
boolean lastGap = FALSE;
int gapStart = ss->gapOpen;
int gapExt = ss->gapExtend;

for (end = 0; end < size; ++end)
    {
    a = as[end];
    b = bs[end];
    if (a == '-' || b == '-')
        {
	if (lastGap)
	    score -= gapExt;
	else
	    {
	    score -= gapStart;
	    lastGap = TRUE;
	    }
	}
    else
        {
	score += ss->matrix[(int)a][(int)b];
	lastGap = FALSE;
	}
    if (score > bestScore)
        {
	bestScore = score;
	best = end;
	}
    if (score < 0 || end == size-1)
        {
	if (bestScore >= minScore)
	    {
	    *retStart = start;
	    *retEnd = best+1;
	    *retScore = bestScore;
	    return TRUE;
	    }
	start = end+1;
	score = 0;
	}
    }
return FALSE;
}

void outputSubAxt(struct axt *axt, int start, int size, int score, FILE *f)
/* Output subset of axt to axt file. */
{
struct axt a;
a = *axt;
a.symCount = size;
a.score = score;
a.qStart += countNonDash(a.qSym, start);
a.qEnd = a.qStart + countNonDash(a.qSym + start, size);
a.tStart += countNonDash(a.tSym, start);
a.tEnd = a.tStart + countNonDash(a.tSym + start, size);
a.qSym += start;
a.tSym += start;
axtWrite(&a, f);
}

void subsetOne(struct axt *axt, struct axtScoreScheme *ss, 
	int threshold, FILE *f)
/* Find subsets of axt that are over threshold and output. */
{
char *q = axt->qSym, *t = axt->tSym;
int size = axt->symCount, offset = 0;
int start, end, score;

while (findHsp(q + offset, t + offset, size - offset, ss, threshold,
	&start, &end, &score))
    {
    assert(start < end);
    assert(end <= size - offset);
    outputSubAxt(axt, start+offset, end-start, score, f);
    offset += end;
    if (offset >= size)
        break;
    }
}

void subsetAxt(char *inName, char *outName, char *scoreFile, int threshold)
/* subsetAxt - Rescore alignments and output those over threshold. */
{
struct axtScoreScheme *ss = axtScoreSchemeRead(scoreFile);
struct lineFile *lf = lineFileOpen(inName, TRUE);
FILE *f = mustOpen(outName, "w");
struct axt *axt;

if (threshold <= 0)
    errAbort("Threshold must be a positive number");
while ((axt = axtRead(lf)) != NULL)
    {
    subsetOne(axt, ss, threshold, f);
    axtFree(&axt);
    axt = NULL;
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 5)
    usage();
subsetAxt(argv[1], argv[2], argv[3], atoi(argv[4]));
return 0;
}
