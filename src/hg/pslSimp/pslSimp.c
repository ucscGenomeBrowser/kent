/* Simplify psl - print only select fields of psl alignment. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "psl.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
"pslSimp - create simplified version of psl file.\n"
"usage:\n"
"    pslSimp filter in.psl. out.ps\n"
"where filter is either:\n"
"    some number - filters by score\n"
"    self - filters out self and bad scores\n"
"    none - no filtering\n");
}


void pslSimp(char *filter, char *inName, char *outName)
/* Simplify psl - print only select non-tab fields. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
FILE *f = mustOpen(outName, "w");
struct psl *psl;
char *line;
int lineSize;
int totMatch = 0, totMis = 0, totIns = 0, totRepMatch = 0;
int minScore = 0;
int minSize = 0;
boolean noSelf = FALSE;
boolean anyFilter = TRUE;
boolean passFilter;


if (sameWord(filter, "none"))
    anyFilter = FALSE;
else if (sameWord(filter, "self"))
    {
    minScore = 960;
    minSize = 80;
    noSelf = TRUE;
    }
else if (isdigit(filter[0]))
    {
    minScore = atoi(filter);
    }
else
    errAbort("Unknown filter %s", filter);
    
if (!lineFileNext(lf, &line, &lineSize))
    errAbort("%s is empty\n", inName);
if (startsWith("psLayout version", line))
   {
   int i;
   uglyf("Skipping header\n");
   for (i=0; i<4; ++i)
       lineFileNext(lf, &line, &lineSize);
   }
else
    lineFileReuse(lf);
while ((psl = pslNext(lf)) != NULL)
    {
    passFilter = TRUE;
    if (anyFilter)
        {
	if (noSelf && sameString(psl->qName, psl->tName))
	    passFilter = FALSE;
	if (psl->match + psl->repMatch < minSize)
	    passFilter = FALSE;
	if (1000 - pslCalcMilliBad(psl, FALSE) < minScore)
	    passFilter = FALSE;
	}
    if (passFilter)
	{
	fprintf(f, "%d\t%d\t%d\t%d\t%s\t%s\t%d\t%d\t%d\t%s\t%d\t%d\t%d\n",
	  psl->match, psl->misMatch, psl->repMatch, psl->blockCount-1, psl->strand,
	  psl->qName, psl->qSize, psl->qStart, psl->qEnd,
	  psl->tName, psl->tSize, psl->tStart, psl->tEnd);
	totMatch += psl->match;
	totMis += psl->misMatch;
	totIns += psl->blockCount-1;
	totRepMatch += psl->repMatch;
	}
    pslFree(&psl);
    }
fprintf(f, "Total match %d, mismatch %d, rep match %d, insert %d\n",
	totMatch, totMis, totRepMatch, totIns);
fclose(f);
lineFileClose(&lf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 4)
    usage();
pslSimp(argv[1], argv[2], argv[3]);
return 0;
}
