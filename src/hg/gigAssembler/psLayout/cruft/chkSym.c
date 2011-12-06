/* Check symmetry */
#include "common.h"
#include "linefile.h"
#include "psLayHit.h"



void usage()
/* Print usage and exit. */
{
errAbort("chkSym - checks alignment for symmetry\n"
       "usage:\n"
       "  chkSym file.psl");
}

int perfectCount = 0;
int okCount = 0;
int badCount = 0;

boolean areClose(int a, int b, int threshold)
/* Return TRUE if a and b are close. */
{
int dif = a-b;
if (dif < 0)
    dif = -dif;
return dif <= threshold;
}

void checkSymmetry(struct psLayHit *h1, struct psLayHit *hitList)
/* Check that there's a symmetrical hit on list. */
{
struct psLayHit *h2;
boolean okSym = FALSE;

for (h2 = hitList; h2 != NULL; h2 = h2->next)
    {
    if (sameString(h2->qName, h1->tName) && sameString(h2->tName, h1->qName))
	{
	if (h1->tStart == h2->qStart && h1->tEnd == h2->qEnd 
	 && h1->qStart == h2->tStart && h1->qEnd == h2->tEnd)
	    {
	    ++perfectCount;
	    return;
	    }
	else if (areClose(h1->tStart, h2->qStart, 10) && areClose(h1->tEnd, h2->qEnd, 10)
	      && areClose(h1->qStart, h2->tStart, 10) && areClose(h1->qEnd, h2->tEnd, 10))
	      {
	      okSym = TRUE;
	      }
	}
    }
if (okSym)
    ++okCount;
else
    {
    printf("No symmetry for %s %d-%d -> %s %d-%d\n",
	h1->qName, h1->qStart, h1->qEnd, h1->tName, h1->tStart, h1->tEnd);
    ++badCount;
    }
}


int main(int argc, char *argv[])
/* Check command line and go.  */
{
char *fileName;
struct lineFile *lf;
char *line;
int lineSize;
char *words[32];
int wordCount;
struct psLayHit *hitList = NULL, *hit;
int symCount = 0, asymCount = 0;

if (argc != 2)
    usage();
fileName = argv[1];
lf = lineFileOpen(fileName, TRUE);
lineFileNext(lf, &line, &lineSize);
if (startsWith("psLayout", line))
    {
    while (lineFileNext(lf, &line, &lineSize))
	{
	if (startsWith("--------", line))
	    break;
	}
    }
else
    {
    lineFileReuse(lf);
    }
while (lineFileNext(lf, &line, &lineSize))
    {
    wordCount = chopTabs(line, words);
//    wordCount = chopString(line, "\t", words, ArraySize(words));
    if (wordCount != 16)
	{
	errAbort("Bad line %d of %s:\n%s", lf->lineIx, lf->fileName, lineFileString(lf));
	}
    hit = psLayHitLoad(words);
    slAddHead(&hitList, hit);
    }
slReverse(&hitList);

for (hit = hitList; hit != NULL; hit = hit->next)
    {
    checkSymmetry(hit, hitList);
    }
printf("Got %d perfect symmetries %d ok symmetries %d assymetries\n", 
    perfectCount, okCount, badCount);
}

