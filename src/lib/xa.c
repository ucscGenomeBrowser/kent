/* xao.c - Manage cross-species alignments in Intronerator database. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */
#include "common.h"
#include "sig.h"
#include "xa.h"


void xaAliFree(struct xaAli *xa)
/* Free up a single xaAli. */
{
freeMem(xa->name);
freeMem(xa->query);
freeMem(xa->target);
freeMem(xa->qSym);
freeMem(xa->tSym);
freeMem(xa->hSym);
freeMem(xa);
}

void xaAliFreeList(struct xaAli **pXa)
/* Free up a list of xaAlis. */
{
struct xaAli *xa, *next;
for (xa = *pXa; xa != NULL; xa = next)
    {
    next = xa->next;
    xaAliFree(xa);
    }
*pXa = NULL;
}

int xaAliCmpTarget(const void *va, const void *vb)
/* Compare two xaAli's to sort by ascending target positions. */
{
const struct xaAli *a = *((struct xaAli **)va);
const struct xaAli *b = *((struct xaAli **)vb);
int diff;
if ((diff = strcmp(a->target, b->target)) == 0)
    diff = a->tStart - b->tStart;
return diff;
}


FILE *xaOpenVerify(char *fileName)
/* Open file, verify it's the right type, and
 * position file pointer for first xaReadNext(). */
{
FILE *f = mustOpen(fileName, "rb");
return f;
}

FILE *xaIxOpenVerify(char *fileName)
/* Open file, verify that it's a good xa index. */
{
FILE *f;
bits32 sig;
f = mustOpen(fileName, "rb");
mustReadOne(f, sig);
if (sig != xaoSig)
    errAbort("Bad signature on %s", fileName);
return f;
}

static void eatLf(FILE *f)
/* Read next char and make sure it's a lf. */
{
int c;
c = fgetc(f);
if (c == '\r')
    c = fgetc(f);
if (c != '\n')
    errAbort("Expecting new line in cross-species alignment file.");
}

static void eatThroughLf(FILE *f)
/* Read through next lf (discarding results). */
{
int c;
while ((c = fgetc(f)) != EOF)
    if (c == '\n')
        break;
}

/* An example line from .st file. 
   G11A11.SEQ.c1 align 53.9% of 6096 ACTIN2~1\G11A11.SEQ:0-4999 - v:9730780-9736763 +
         0         1     2    3   4             5               6        7          8
 */

struct xaAli *xaReadNext(FILE *f, boolean condensed)
/* Read next xaAli from file. If condensed
 * don't fill int query, target, qSym, tSym, or hSym. */
{
char line[512];
char *words[16];
int wordCount;
struct xaAli *xa;
char *parts[5];
int partCount;
double percentScore;
int symCount;
int newOffset = 0;
char *s, *e;

/* Get first line and parse out everything but the sym lines. */
if (fgets(line, sizeof(line), f) == NULL)
    return NULL;
wordCount = chopLine(line, words);
if (wordCount < 9)
    errAbort("Short line in cross-species alignment file");
if (wordCount == 10)
    newOffset = 1;
if (!sameString(words[1], "align"))
    errAbort("Bad line in cross-species alignment file");
AllocVar(xa);
xa->name = cloneString(words[0]);
s = words[5+newOffset];
e = strrchr(s, ':');
if (e == NULL)
    errAbort("Bad line (no colon) in cross-species alignment file");
*e++ = 0;
partCount = chopString(e, "-", parts, ArraySize(parts));
if (partCount != 2)
    errAbort("Bad range format in cross-species alignment file");
if (!condensed)
    xa->query = cloneString(s);
xa->qStart = atoi(parts[0]);
xa->qEnd = atoi(parts[1]);
xa->qStrand = words[6+newOffset][0];
partCount = chopString(words[7+newOffset], ":-", parts, ArraySize(parts));
if (!condensed)
    xa->target = cloneString(parts[0]);
xa->tStart = atoi(parts[1]);
xa->tEnd = atoi(parts[2]);
xa->tStrand = words[8+newOffset][0];
percentScore = atof(words[2]);
xa->milliScore = round(percentScore*10);    
xa->symCount = symCount = atoi(words[4]);

/* Get symbol lines. */
if (condensed)
    {
    eatThroughLf(f);
    eatThroughLf(f);
    eatThroughLf(f);
    }
else
    {
    xa->qSym = needMem(symCount+1);
    mustRead(f, xa->qSym, symCount);
    eatLf(f);

    xa->tSym = needMem(symCount+1);
    mustRead(f, xa->tSym, symCount);
    eatLf(f);

    xa->hSym = needMem(symCount+1);
    mustRead(f, xa->hSym, symCount);
    eatLf(f);
    }
return xa;
}

struct xaAli *xaRdRange(FILE *ix, FILE *data, 
    int start, int end, boolean condensed)
/* Return list of all xaAlis that range from start to end.  
 * Assumes that ix and data files are open. If condensed
 * don't fill int query, target, qSym, tSym, or hSym. */
{
int s, e;
int maxS, minE;
long offset;
struct xaAli *list = NULL, *xa;


/* Scan through index file looking for things in range.
 * When find one read it from data file and add it to list. */
fseek(ix, sizeof(bits32), SEEK_SET);
for (;;)
    {
    if (!readOne(ix, s))
        break;
    mustReadOne(ix, e);
    mustReadOne(ix, offset);
    if (s >= end)
        break;
    maxS = max(s, start);
    minE = min(e, end);
    if (minE - maxS > 0)
        {
        fseek(data, offset, SEEK_SET);
        xa = xaReadNext(data, condensed);
        slAddHead(&list, xa);
        }
    }

slReverse(&list);
return list;
}

struct xaAli *xaReadRange(char *rangeIndexFileName, char *dataFileName, 
    int start, int end, boolean condensed)
/* Return list of all xaAlis that range from start to end.  If condensed
 * don't fill int query, target, qSym, tSym, or hSym. */
{
FILE *ix = xaIxOpenVerify(rangeIndexFileName);
FILE *data = xaOpenVerify(dataFileName);
struct xaAli *xa = xaRdRange(ix, data, start, end, condensed);
fclose(data);
fclose(ix);
return xa;
}


char *xaAlignSuffix()
/* Return suffix of file with actual alignments. */
{
return ".st";
}

char *xaChromIxSuffix()
/* Return suffix of files that index xa's by chromosome position. */
{
return ".xao";
}



