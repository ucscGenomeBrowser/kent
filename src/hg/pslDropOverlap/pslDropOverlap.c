/* used for cleaning up self alignments - deletes all overlapping self alignments */
#include "common.h"
#include "linefile.h"
#include "psl.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
"pslDropOverlap - deletes all overlapping self alignments. \n"
"usage:\n"
"    pslDropOverlap in.psl out.psl\n");
}


void pslDropOverlap(char *inName, char *outName)
/* Simplify psl - print only select non-tab fields. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
FILE *f = mustOpen(outName, "w");
struct psl *psl;
char *line;
int lineSize;
int skipMatch = 0, skipMis = 0, skipIns = 0, skipRepMatch = 0;
int totMatch = 0, totMis = 0, totIns = 0, totRepMatch = 0;
int minScore = 0;
int minSize = 0;
int totSkip = 0;
int totLines = 0;
boolean noSelf = FALSE;
boolean anyFilter = TRUE;
boolean passFilter;


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
    totLines++;
    totMatch += psl->match;
    totMis += psl->misMatch;
    totIns += psl->blockCount-1;
    totRepMatch += psl->repMatch;
	if (sameString(psl->qName, psl->tName))
        {
        if (rangeIntersection(psl->qStart, psl->qEnd, psl->tStart, psl->tEnd) > 0)
            {
            /*printf( "skip %d\t%d\t%d\t%d\t%s\t%s\t%d\t%d\t%d\t%s\t%d\t%d\t%d\n",
              psl->match, psl->misMatch, psl->repMatch, psl->nCount, 
              psl->strand,
              psl->qName, psl->qSize, psl->qStart, psl->qEnd,
              psl->tName, psl->tSize, psl->tStart, psl->tEnd
              );*/
            totSkip++;
            skipMatch += psl->match;
            skipMis += psl->misMatch;
            skipIns += psl->blockCount-1;
            skipRepMatch += psl->repMatch;
            continue;
            }
        }
    /* fprintf(f, "%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%s\t%s\t%d\t%d\t%d\t%s\t%d\t%d\t%d\t%d\n",
	  psl->match, psl->misMatch, psl->repMatch, psl->nCount, 
      psl->qNumInsert, psl->qBaseInsert, psl->tNumInsert, psl->tBaseInsert,
      psl->strand,
	  psl->qName, psl->qSize, psl->qStart, psl->qEnd,
	  psl->tName, psl->tSize, psl->tStart, psl->tEnd,
      psl->blockCount-1 );
      */

    pslTabOut(psl, f);

    pslFree(&psl);
    }
printf( "Total skipped %d out of %d match %d out of %d, mismatch %d, rep match %d, insert %d in %s\n",
	totSkip, totLines, skipMatch, totMatch,  totMis, totRepMatch, totIns,outName );
fclose(f);
lineFileClose(&lf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
pslDropOverlap(argv[1], argv[2]);
}
