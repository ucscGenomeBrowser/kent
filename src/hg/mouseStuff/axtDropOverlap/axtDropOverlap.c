/* used for cleaning up self alignments - deletes all overlapping self alignments */
#include "common.h"
#include "linefile.h"
#include "axt.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
"axtDropOverlap - deletes all overlapping self alignments. \n"
"usage:\n"
"    axtDropOverlap in.axt out.axt\n");
}


void axtDropOverlap(char *inName, char *outName)
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
FILE *f = mustOpen(outName, "w");
struct axt *axt;
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


/*if (!lineFileNext(lf, &line, &lineSize))
    errAbort("%s is empty\n", inName);*/
while ((axt = axtRead(lf)) != NULL)
    {
    totLines++;
    totMatch += axt->score;
	if (sameString(axt->qName, axt->tName))
        {
        if (rangeIntersection(axt->qStart, axt->qEnd, axt->tStart, axt->tEnd) > 0)
            {
            /*
            printf( "skip %c\t%s\t%d\t%d\t%d\t%s\t%d\t%d\t%d\n",
              axt->qStrand,
              axt->qName, axt->symCount, axt->qStart, axt->qEnd,
              axt->tName, axt->symCount, axt->tStart, axt->tEnd
              );
              */
            totSkip++;
/*            skipMatch += axt->match;
            skipMis += axt->misMatch;
            skipIns += axt->blockCount-1;
            skipRepMatch += axt->repMatch;*/
            continue;
            }
        }
    /*
            printf( "read %c\t%s\t%d\t%d\t%d\t%s\t%d\t%d\t%d\n",
              axt->qStrand,
              axt->qName, axt->symCount, axt->qStart, axt->qEnd,
              axt->tName, axt->symCount, axt->tStart, axt->tEnd
              );
              */
    /* fprintf(f, "%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%s\t%s\t%d\t%d\t%d\t%s\t%d\t%d\t%d\t%d\n",
	  axt->match, axt->misMatch, axt->repMatch, axt->nCount, 
      axt->qNumInsert, axt->qBaseInsert, axt->tNumInsert, axt->tBaseInsert,
      axt->strand,
	  axt->qName, axt->qSize, axt->qStart, axt->qEnd,
	  axt->tName, axt->tSize, axt->tStart, axt->tEnd,
      axt->blockCount-1 );
      */

    axtWrite(axt, f);

    axtFree(&axt);
    }
/*
printf( "Total skipped %d out of %d match %d out of %d, mismatch %d, rep match %d, insert %d in %s\n",
	totSkip, totLines, skipMatch, totMatch,  totMis, totRepMatch, totIns,outName );
    */
fclose(f);
lineFileClose(&lf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
axtDropOverlap(argv[1], argv[2]);
}
