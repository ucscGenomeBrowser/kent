/* pslSwap - Swap query and target in  PSL  */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "dlist.h"
#include "fa.h"
#include "nib.h"
#include "psl.h"

static char const rcsid[] = "$Id: pslSwap.c,v 1.6 2004/12/30 20:01:21 braney Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslSwap - Swap query and target in  PSL  \n"
  "usage:\n"
  "   pslSwap in.psl out.psl\n"
  "options:\n"
  "   -keepStrand     don't force target strand to '+'\n"
  );
}

boolean keepStrand = FALSE;
int dot = 0;
boolean doShort = FALSE;

void pslWrite(struct psl *el, FILE *f, char* q, char *t, char *blk, char sep, char lastSep) 
/* Print out psl.  Separate fields with sep. Follow last field with lastSep. */
{
int i;
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
fprintf(f, "%s", blk);
if (sep == ',') fputc('}',f);
fputc(sep,f);
if (sep == ',') fputc('{',f);
fprintf(f, "%s", q);
if (sep == ',') fputc('}',f);
fputc(sep,f);
if (sep == ',') fputc('{',f);
fprintf(f, "%s", t);
if (sep == ',') fputc('}',f);
fputc(lastSep,f);
if (ferror(f))
    {
    perror("Error writing psl file\n");
    errAbort("\n");
    }
}

void swapOne(struct psl *psl, FILE *f)
    /* swap query and target , blockStarts = size - blockStarts  if strand direction changes */
{
static char *tName = NULL, *qName = NULL;
struct dyString *q = newDyString(16*1024);
struct dyString *t = newDyString(16*1024);
struct dyString *blockSize = newDyString(16*1024);
int blockIx, diff, x;
int qs, ts;
int lastQ = 0, lastT = 0, size;
int qOffset = 0;
int tOffset = 0;
char *s;
char qStr[512], tStr[512], bStr[512];

if (qName == NULL || !sameString(qName, psl->qName))
    {
    freez(&qName);
    qName = cloneString(psl->qName);
    if (psl->strand[0] == '-' && psl->strand[1] != '-')
	    qOffset = psl->qSize ;
    }
/* if (tName == NULL) || !sameString(tName, psl->tName))
    {*/
    freez(&tName);
    tName = cloneString(psl->tName);
    if (psl->strand[0] == '-' && psl->strand[1] != '-')
	    tOffset = psl->tSize ;
/*    }*/
    /*
if (psl->strand[0] == '-')
if (psl->strand[1] == '-')
*/
if (!keepStrand && (psl->strand[0] == '-' && psl->strand[1] != '-'))
    {
    for (blockIx=(psl->blockCount)-1; blockIx >= 0; --blockIx)
        {
        sprintf(qStr, "%d,", qOffset - psl->qStarts[blockIx] - psl->blockSizes[blockIx]);
        sprintf(tStr, "%d,", tOffset - psl->tStarts[blockIx] - psl->blockSizes[blockIx]);
        dyStringAppend(q, qStr );
        dyStringAppend(t, tStr );
        sprintf(bStr, "%d,",psl->blockSizes[blockIx] );
        dyStringAppend(blockSize, bStr);
        };
        if ((qOffset - psl->qStarts[blockIx]) < 0)
            printf("qOffset %d tStart %d qStart %d strand %s q %s t %s tStarts[0] %d %d q %d %d %s\n",qOffset, psl->tStart,psl->qStart,psl->strand,q->string, t->string, psl->tStarts[0], psl->tStarts[1], psl->qStarts[0], psl->qStarts[1],qStr );
        if ((tOffset - psl->tStarts[blockIx]) < 0)
            printf("tOffset %d tStart %d qStart %d strand %s q %s t %s tStarts[0] %d %d q %d %d %s\n",tOffset, psl->tStart,psl->qStart,psl->strand,q->string, t->string, psl->tStarts[0], psl->tStarts[1], psl->qStarts[0], psl->qStarts[1], tStr);
    }
else
    {
    for (blockIx=0; blockIx < psl->blockCount; ++blockIx)
        {
        sprintf(qStr, "%d,", psl->qStarts[blockIx] );
        sprintf(tStr, "%d,", psl->tStarts[blockIx] );
        dyStringAppend(q, qStr );
        dyStringAppend(t, tStr );
        sprintf(bStr, "%d,",psl->blockSizes[blockIx] );
        dyStringAppend(blockSize, bStr);
        }
    }

x = psl->tStart; psl->tStart = psl->qStart; psl->qStart = x;
x = psl->tEnd;   psl->tEnd = psl->qEnd;     psl->qEnd = x;
x = psl->tSize;  psl->tSize = psl->qSize;   psl->qSize = x;
x = psl->tNumInsert;   psl->tNumInsert = psl->qNumInsert; psl->qNumInsert = x;
x = psl->tBaseInsert;  psl->tBaseInsert = psl->qBaseInsert; psl->qBaseInsert = x;
s = psl->tName;  psl->tName = psl->qName;   psl->qName = s;
if (keepStrand)
    {
    x = psl->strand[0]; psl->strand[0] = psl->strand[1]; psl->strand[1] = x;
    }
pslWrite(psl,f,t->string,q->string, blockSize->string,'\t','\n');
dyStringFree(&blockSize);
dyStringFree(&q);
dyStringFree(&t);
    freez(&qName);
}

void pslSwap(char *pslName, char *outName)
/* pslSwap - Convert PSL to human readable output. */
{
struct hash *fileHash = newHash(0);  /* No value. */
struct hash *tHash = newHash(20);  /* seqFilePos value. */
struct hash *qHash = newHash(20);  /* seqFilePos value. */
struct dlList *fileCache = newDlList();
struct lineFile *lf = pslFileOpen(pslName);
FILE *f = mustOpen(outName, "w");
struct psl *psl;
int dotMod = dot;

verbose(1,"Converting %s\n", pslName);
while ((psl = pslNext(lf)) != NULL)
    {
    if (dot > 0)
        {
	if (--dotMod <= 0)
	   {
	   printf(".");
       fflush(stdout);
	   dotMod = dot;
	   }
	}
    swapOne(psl, f);
    pslFree(&psl);
    }
lineFileClose(&lf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
static boolean axt;
optionHash(&argc, argv);
dot = optionInt("dot", dot);
doShort = !optionExists("long");
keepStrand = optionExists("keepStrand");
if (argc != 3)
    usage();
pslSwap(argv[1], argv[2]);
return 0;
}
