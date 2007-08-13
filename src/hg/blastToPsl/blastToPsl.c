/* blastToPsl - convert blast textual output to PSLs */
#include "common.h"
#include "linefile.h"
#include "options.h"
#include "psl.h"
#include "blastParse.h"
#include "dnautil.h"

static char const rcsid[] = "$Id: blastToPsl.c,v 1.21 2007/08/13 15:59:40 markd Exp $";

double eVal = -1; /* default Expect value signifying no filtering */
boolean pslxFmt = FALSE; /* output in pslx format */

struct block
/* coordinates of a block */
{
    int qStart;          /* Query start/end positions. */
    int qEnd;
    int tStart;          /* Target start/end position. */
    int tEnd;
    int qLetMult;        /* each letter counts as this many units */
    int tLetMult;
    int qSizeMult;       /* user to convert coordinates */
    int tSizeMult;
    int alnStart;        /* start/end in alignment */
    int alnEnd;
};

/* Internally used bit flags */
#define TBLASTN 0x01
#define TBLASTX 0x02

/* score file header */
static char *scoreHdr = "#strand\tqName\tqStart\tqEnd\ttName\ttStart\ttEnd\tbitScore\teVal\n";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"scores", OPTION_STRING},
    {"eVal", OPTION_DOUBLE},
    {"pslx", OPTION_BOOLEAN},
    {NULL, 0}
};

static void usage()
/* Explain usage and exit. */
{
errAbort(
  "blastToPsl - Convert blast alignments to PSLs.\n"
  "\n"
  "usage:\n"
  "   blastToPsl [options] blastOutput psl\n"
  "\n"
  "Options:\n"
  "  -scores=file - Write score information to this file.  Format is:\n"
  "       strands qName qStart qEnd tName tStart tEnd bitscore eVal\n"
  "  -verbose=n - n >= 3 prints each line of file after parsing.\n"
  "               n >= 4 dumps the result of each query\n"
  "  -eVal=n n is e-value threshold to filter results. Format can be either\n"
  "          an integer, double or 1e-10. Default is no filter.\n"
  "  -pslx - create PSLX output (includes sequences for blocks)\n" 
  );
}

static char *wackAfterWhite(char *s)
/* end string at first whitespare */
{
char *w = skipToSpaces(s);
if (w != NULL)
    *w = '\0';
return s;
}

static struct psl* createPsl(struct block *blk, struct blastBlock *bb, int pslSpace)
/* create PSL for a blast block */
{
struct blastGappedAli *ba = bb->gappedAli;
char strand[3];

strand[0] = (bb->qStrand > 0) ? '+' : '-';
strand[1] = (bb->tStrand > 0) ? '+' : '-';
strand[2] = '\0';

/* white space in query or target name breaks some psl software (like pslOpen()),
 * so only keep up to first whitespace.
 */
int qSize = blk->qSizeMult*ba->query->queryBaseCount;
return pslNew(wackAfterWhite(ba->query->query), qSize, 0, 0,
              wackAfterWhite(ba->targetName), ba->targetSize, 0, 0,
              strand, pslSpace, (pslxFmt ? PSL_XA_FORMAT : 0));
}

static void makeUntranslated(struct psl* psl)
/* convert a PSL so it is in the untranslated form produced by blat */
{
if (psl->strand[1] == '-')
    {
    /* swap around blocks so it's query that is reversed */
    int i;
    for (i = 0; i < psl->blockCount; i++)
        {
        psl->tStarts[i] = psl->tSize - (psl->tStarts[i] + psl->blockSizes[i]);
        psl->qStarts[i] = psl->qSize - (psl->qStarts[i] + psl->blockSizes[i]);
        }
    reverseUnsigned(psl->tStarts, psl->blockCount);
    reverseUnsigned(psl->qStarts, psl->blockCount);
    reverseUnsigned(psl->blockSizes, psl->blockCount);

    /* fix strand, +- now -, -- now + */
    psl->strand[0] = (psl->strand[0] == '+') ? '-' : '+';
    }
psl->strand[1] = '\0';
}

static void finishPsl(struct psl* psl, unsigned flags)
/* put finishing touches on a psl */
{
if ((flags & TBLASTN) == 0)
    makeUntranslated(psl);
assert(((flags & TBLASTN) != 0) == pslIsProtein(psl));
}

static void addPslBlock(struct psl* psl, struct blastBlock *bb, struct block* blk,
                        int* pslSpace)
/* add a block to a psl */
{
unsigned newIBlk = psl->blockCount;
unsigned blkSize = blk->qSizeMult * (blk->qEnd - blk->qStart);
if (newIBlk >= *pslSpace)
    pslGrow(psl, pslSpace);
psl->qStarts[newIBlk] = blk->qStart;
psl->tStarts[newIBlk] = blk->tStart;
/* uses query size so protein psl is right */
psl->blockSizes[newIBlk] = blkSize;

/* keep bounds current */
psl->qStart = psl->qStarts[0];
psl->qEnd = psl->qStarts[newIBlk]
    + (blk->qSizeMult * psl->blockSizes[newIBlk]);
if (psl->strand[0] == '-')
    reverseIntRange(&psl->qStart, &psl->qEnd, psl->qSize);
psl->tStart = psl->tStarts[0];
psl->tEnd = psl->tStarts[newIBlk]
    + (blk->tSizeMult * psl->blockSizes[newIBlk]);
if (psl->strand[1] == '-')
    reverseIntRange(&psl->tStart, &psl->tEnd, psl->tSize);

if (pslxFmt)
    {
    psl->qSequence[newIBlk] = cloneStringZ(bb->qSym + blk->alnStart, blkSize);
    psl->tSequence[newIBlk] = cloneStringZ(bb->tSym + blk->alnStart, blkSize);
    }
psl->blockCount++;
}

static boolean nextUngappedBlk(struct blastBlock* bb, struct block* blk, unsigned flags)
/* Find the next ungapped block in a blast alignment, in [0..n) coords in mrna
 * space.  On first call, block should be zero, subsequence calls should be
 * parsed the result of the previous call.
 */
{
char *qPtr, *tPtr;

if (blk->tEnd == 0)
    {
    /* first call */
    blk->qStart = bb->qStart;
    blk->qEnd = blk->qStart+1;
    blk->tStart = bb->tStart;
    blk->tEnd = blk->tStart+1;
    }
else
    {
    /* subsequence calls */
    blk->qStart = blk->qEnd;
    blk->tStart = blk->tEnd;
    blk->alnStart = blk->alnEnd;
    }

/* find start of next aligned block */
qPtr = bb->qSym + blk->alnStart;
tPtr = bb->tSym + blk->alnStart;

while ((*qPtr != '\0') && (*tPtr != '\0')
       && ((*qPtr == '-') || (*tPtr == '-')))
    {
    if (*qPtr != '-')
        blk->qStart += blk->qLetMult;
    if (*tPtr != '-')
        blk->tStart += blk->tLetMult;
    qPtr++;
    tPtr++;
    blk->alnStart++;
    }
blk->qEnd = blk->qStart;
blk->tEnd = blk->tStart;
blk->alnEnd = blk->alnStart;

if ((*qPtr == '\0') || (*tPtr == '\0'))
    {
    assert((*qPtr == '\0') && (*tPtr == '\0'));
    assert(blk->qStart == bb->qEnd);
    assert(blk->tStart == bb->tEnd);
    return FALSE;  /* no more */
    }


/* find end of aligned block */
while ((*qPtr != '\0') && (*tPtr != '\0')
       && (*qPtr != '-') && (*tPtr != '-'))
    {
    blk->qEnd += blk->qLetMult;
    blk->tEnd += blk->tLetMult;
    qPtr++;
    tPtr++;
    blk->alnEnd++;
    }

assert((blk->tSizeMult * (blk->qEnd - blk->qStart))
       == (blk->qSizeMult * (blk->tEnd - blk->tStart)));
return TRUE;
}

static void countBlock(struct blastBlock* bb, struct block* blk, struct block* prevBlk, struct psl* psl)
/* update the PSL counts between for a block and previous insert. */
{
int i;
char *qPtr, *tPtr;

if (prevBlk->tEnd != 0)
    {
    /* count insert */
    if (prevBlk->qEnd != blk->qStart)
        {
        /* insert in query */
        psl->qNumInsert++;
        psl->qBaseInsert += (blk->qStart - prevBlk->qEnd);
    }
    if (prevBlk->tEnd != blk->tStart)
        {
        /* insert in target */
        psl->tNumInsert++;
        psl->tBaseInsert += (blk->tStart - prevBlk->tEnd);
        }
    }

qPtr = bb->qSym + blk->alnStart;
tPtr = bb->tSym + blk->alnStart;
for (i = 0; (*qPtr != '\0') && (*tPtr != '\0'); i++, qPtr++, tPtr++)
    {
    if ((*qPtr == 'N') || (*qPtr == 'X') || (*tPtr == 'N') || (*tPtr == 'X'))
        psl->repMatch++;
    else if (*qPtr == *tPtr)
        psl->match++;
    else
        psl->misMatch++;
    }
assert((*qPtr == '\0') && (*tPtr == '\0'));
}

static void outputPsl(struct blastBlock *bb, unsigned flags, struct psl *psl,
                      FILE* pslFh, FILE* scoreFh)
/* output a psl and optional score */
{
pslTabOut(psl, pslFh);
if (scoreFh != NULL)
    fprintf(scoreFh, "%s\t%s\t%d\t%d\t%s\t%d\t%d\t%g\t%g\n", psl->strand,
            psl->qName, psl->qStart, psl->qEnd,
            psl->tName, psl->tStart, psl->tEnd, bb->bitScore, bb->eVal);
}

static void initBlk(unsigned flags, struct block *blk)
/* initialize a block object */
{
ZeroVar(blk);
if (flags & TBLASTN)
    {
    blk->qLetMult = 1;
    blk->qSizeMult = 1;
    blk->tLetMult = 3;
    blk->tSizeMult = 3;
    }
else if (flags & TBLASTX)
    {
    blk->qLetMult = 3;
    blk->qSizeMult = 1;
    blk->tLetMult = 3;
    blk->tSizeMult = 1;
    }
else
    {
    blk->qLetMult = 1;
    blk->qSizeMult = 1;
    blk->tLetMult = 1;
    blk->tSizeMult = 1;
    }
}

static void processBlock(struct blastBlock *bb, unsigned flags,
                         FILE* pslFh, FILE* scoreFh)
/* process one alignment block  */
{
int pslSpace = 8;
struct block blk, prevBlk;
initBlk(flags, &blk);
initBlk(flags, &prevBlk);

struct psl *psl = createPsl(&blk, bb, pslSpace);

/* fill in ungapped blocks */
while (nextUngappedBlk(bb, &blk, flags))
    {
    countBlock(bb, &blk, &prevBlk, psl);
    addPslBlock(psl, bb, &blk, &pslSpace);
    prevBlk = blk;
    }
if (psl->blockCount > 0 && (bb->eVal <= eVal || eVal == -1))
    {
    finishPsl(psl, flags);
    outputPsl(bb, flags, psl, pslFh, scoreFh);
    }
pslFree(&psl);
}

void processQuery(struct blastQuery *bq, unsigned flags, FILE* pslFh, FILE* scoreFh)
/* process one query  */
{
struct blastGappedAli* ba;
struct blastBlock *bb;
for (ba = bq->gapped; ba != NULL; ba = ba->next)
    {
    for (bb = ba->blocks; bb != NULL; bb = bb->next)
        processBlock(bb, flags, pslFh, scoreFh);
    }
}

static void blastToPsl(char *blastFile, char *pslFile, char* scoreFile)
/* process one query in */
{
struct blastFile *bf = blastFileOpenVerify(blastFile);
struct blastQuery *bq;
unsigned flags = 0;
FILE *pslFh = mustOpen(pslFile, "w");
FILE *scoreFh = NULL;
if (scoreFile != NULL)
    {
    scoreFh = mustOpen(scoreFile, "w");
    fputs(scoreHdr, scoreFh);
    }

if (strstr(bf->program, "TBLASTN") != NULL)
    flags |= TBLASTN;
else if (strstr(bf->program, "TBLASTX") != NULL)
    {
    flags |= TBLASTX;
    if (pslxFmt)
        errAbort("-pslx not supported for TBLASTX alignments");
    }

while ((bq = blastFileNextQuery(bf)) != NULL)
    {
    processQuery(bq, flags, pslFh, scoreFh);
    blastQueryFree(&bq);
    }

blastFileFree(&bf);
carefulClose(&scoreFh);
carefulClose(&pslFh);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
eVal = optionDouble("eVal", eVal);
pslxFmt = optionExists("pslx");
if (argc != 3)
    usage();
blastToPsl(argv[1], argv[2], optionVal("scores", NULL));

return 0;
}
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

