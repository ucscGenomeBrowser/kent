/* used for cleaning up self alignments - deletes all overlapping self alignments */
#include "common.h"
#include "linefile.h"
#include "psl.h"
#include "dnautil.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
"pslDropOverlap - deletes all overlapping self alignments. \n"
"usage:\n"
"    pslDropOverlap in.psl out.psl\n");
}

void pslOutRev(struct psl *el, FILE *f)
/* print psl with qStarts in fwd coordinates */
{
int i;
char sep = ' ';
char lastSep = '\n';

fprintf(f, "%u", el->match);
fputc(sep,f);
fprintf(f, "%u", el->misMatch);
fputc(sep,f);
fprintf(f, "%u", el->repMatch);
fputc(sep,f);
fprintf(f, "%u", el->nCount);
fputc(sep,f);
fprintf(f, "%u", el->qNumInsert);
fputc(sep,f);
fprintf(f, "%d", el->qBaseInsert);
fputc(sep,f);
fprintf(f, "%u", el->tNumInsert);
fputc(sep,f);
fprintf(f, "%d", el->tBaseInsert);
fputc(sep,f);
if (sep == ',') fputc('"',f);
fprintf(f, "%s", el->strand);
if (sep == ',') fputc('"',f);
fputc(sep,f);
if (sep == ',') fputc('"',f);
fprintf(f, "%s", el->qName);
if (sep == ',') fputc('"',f);
fputc(sep,f);
fprintf(f, "%u", el->qSize);
fputc(sep,f);
fprintf(f, "%u", el->qStart);
fputc(sep,f);
fprintf(f, "%u", el->qEnd);
fputc(sep,f);
if (sep == ',') fputc('"',f);
fprintf(f, "%s", el->tName);
if (sep == ',') fputc('"',f);
fputc(sep,f);
fprintf(f, "%u", el->tSize);
fputc(sep,f);
fprintf(f, "%u", el->tStart);
fputc(sep,f);
fprintf(f, "%u", el->tEnd);
fputc(sep,f);
fprintf(f, "%u", el->blockCount);
fputc(sep,f);
if (sep == ',') fputc('{',f);
fputc(lastSep,f);
for (i=0; i<el->blockCount; ++i)
    {
    fprintf(f, "%u", el->blockSizes[i]);
    fputc(',', f);
    }
if (sep == ',') fputc('}',f);
fputc(sep,f);
fputc(lastSep,f);
if (sep == ',') fputc('{',f);
for (i=0; i<el->blockCount; ++i)
    {
    int qs = el->qStarts[i];
    int qe = el->qStarts[i]+el->blockSizes[i];
    if (el->strand[0] == '-')
        reverseIntRange(&qs, &qe, el->qSize);
    fprintf(f, "%u", qs);
    fputc(',', f);
    }
if (sep == ',') fputc('}',f);
fputc(sep,f);
fputc(lastSep,f);
if (sep == ',') fputc('{',f);
for (i=0; i<el->blockCount; ++i)
    {
    fprintf(f, "%u", el->tStarts[i]);
    fputc(',', f);
    }
if (sep == ',') fputc('}',f);
fputc(lastSep,f);
for (i=0; i<el->blockCount; ++i)
    {
    int qs = el->qStarts[i];
    int qe = el->qStarts[i]+el->blockSizes[i];
    if (el->strand[0] == '-')
        reverseIntRange(&qs, &qe, el->qSize);
    fprintf(f, "%u", qe);
    fputc(',', f);
    }
if (sep == ',') fputc('}',f);
fputc(sep,f);
fputc(lastSep,f);
for (i=0; i<el->blockCount; ++i)
    {
    fprintf(f, "%u", el->tStarts[i]+el->blockSizes[i]);
    fputc(',', f);
    }
if (sep == ',') fputc('}',f);
fputc(lastSep,f);
if (ferror(f))
    {
    perror("Error writing psl file\n");
    errAbort("\n");
    }
}

void deleteElement(unsigned *list, int i, int total)
/* delete element i from array of size total, move elements down by one to fill gap */
{
int j;

for (j = i ; j < total ; j++)
    list[j] = list[j+1];
list[total-1] = 0;
}

void pslDropOverlap(char *inName, char *outName)
/* Simplify psl - print only select non-tab fields. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
FILE *f = mustOpen(outName, "w");
struct psl *psl;
char *line;
int lineSize;
int skipMatch = 0;
int totMatch = 0, totMis = 0, totIns = 0, totRepMatch = 0;
int totSkip = 0;
int totLines = 0;

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
        int i;
        int newBlockCount = psl->blockCount;
        if (psl->tStart == psl->qStart)
            {
            pslFree(&psl);
            continue;
            }
        for (i = psl->blockCount-1 ; i >= 0 ; i--)
            {
            int ts = psl->tStarts[i];
            int te = psl->tStarts[i]+psl->blockSizes[i];
            int qs = psl->qStarts[i];
            int qe = psl->qStarts[i]+psl->blockSizes[i];
            if (psl->strand[0] == '-')
                reverseIntRange(&qs, &qe, psl->qSize);
            if (psl->strand[1] == '-')
                reverseIntRange(&ts, &te, psl->tSize);
            if (ts == qs)
                {
                newBlockCount--;
                psl->match -= psl->blockSizes[i];
                printf( "skip block size %d #%d blk %d %d\t%d\t%d\t%d\t%s\t%s\t%d\t%d\t%d\t%s\t%d\t%d\t%d\n",
                  psl->blockSizes[i],i, psl->blockCount, psl->match, psl->misMatch, psl->repMatch, psl->nCount, 
                  psl->strand,
                  psl->qName, psl->qSize, psl->qStart, psl->qEnd,
                  psl->tName, psl->tSize, psl->tStart, psl->tEnd
                  );
                /* debug */
                if (psl->match > 200000000)
                    {
                    printf("blk %d : ", psl->blockCount);
                    pslOutRev(psl, stdout);
                    }
                deleteElement(psl->tStarts, i , psl->blockCount);
                deleteElement(psl->qStarts, i , psl->blockCount);
                deleteElement(psl->blockSizes, i , psl->blockCount);
                totSkip++;
                skipMatch += psl->blockSizes[i];
                }
            }
        psl->blockCount = newBlockCount;
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
printf( "Total skipped %d blocks out of %d alignments, match %d out of %d, in %s\n",
	totSkip, totLines, skipMatch, totMatch,  outName );
fclose(f);
lineFileClose(&lf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
pslDropOverlap(argv[1], argv[2]);
return 0;
}
