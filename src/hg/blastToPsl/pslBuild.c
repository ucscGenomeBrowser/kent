/* object using in building a PSL from a blast record */
#include "common.h"
#include "pslBuild.h"
#include "psl.h"

unsigned pslBuildGetBlastAlgo(char *program)
/* determine blast algorithm flags */
{
if (sameWord(program, "BLASTN"))
    return blastn;
else if (sameWord(program, "BLASTP"))
    return blastp;
else if (sameWord(program, "BLASTX"))
    return blastx;
else if (sameWord(program, "TBLASTN"))
    return tblastn;
else if (sameWord(program, "TBLASTX"))
    return tblastx;
else if (sameWord(program, "PSIBLAST"))
    return psiblast;
else
    errAbort("unknown BLAST program: \"%s\"", program);
return 0;
}

struct block
/* coordinates of a block use during build */
{
    char *qAln;          /* alignment strings (not owned) */
    char *tAln;
    int qStart;          /* Query start/end positions, in blast units */
    int qEnd;
    int tStart;          /* Target start/end position, in blast units */
    int tEnd;
    int qLetMult;        /* each letter counts as this many units */
    int tLetMult;
    int qCoordMult;      /* used to convert coordinates */
    int tCoordMult;
    int q2tBlkSizeMult;  /* convert query block size to target block size */
    int countMult;       /* column count for match/mismatch */
    int alnStart;        /* start/end in alignment */
    int alnEnd;
};

static void blkInit(struct block *blk, int qStart, char *qAln, int tStart, char *tAln, unsigned flags)
/* initialize a block object */
{
ZeroVar(blk);
blk->qAln = qAln;
blk->tAln = tAln;

// initialize ends to starts for first call to nextUngappedBlk
blk->qEnd = qStart;
blk->tEnd = tStart;

if (flags & cnvNucCoords)
    {
    // DNA/DNA PSL from protein/DNA align
    assert(flags & tblastn);
    blk->qLetMult = 1;
    blk->qCoordMult = 3;
    blk->tLetMult = 3;
    blk->tCoordMult = 1;
    blk->q2tBlkSizeMult = 3;
    blk->countMult = 3;
    }
else if (flags & tblastn)
    {
    // protein/DNA PSL
    blk->qLetMult = 1;
    blk->qCoordMult = 1;
    blk->tLetMult = 3;
    blk->tCoordMult = 1;
    blk->q2tBlkSizeMult = 3;
    blk->countMult = 1;
    }
else if (flags & tblastx)
    {
    blk->qLetMult = 3;
    blk->qCoordMult = 1;
    blk->tLetMult = 3;
    blk->tCoordMult = 1;
    blk->q2tBlkSizeMult = 1;
    blk->countMult = 3;
    }
else
    {
    blk->qLetMult = 1;
    blk->qCoordMult = 1;
    blk->tLetMult = 1;
    blk->tCoordMult = 1;
    blk->q2tBlkSizeMult = 1;
    blk->countMult = 1;
    }
}

static boolean isProteinSeqs(unsigned flags)
/* do the flags indicate a protein sequences in alignments */
{
return (flags & (blastp|blastx|tblastn|tblastx)) != 0;
}

static boolean nextUngappedBlk(struct block* blk)
/* Find the next ungapped block in a blast alignment, in [0..n) coords in mrna
 * space.  blk qStartNext and tStartNext should be initialize to 
 */
{
// initBlk set this up for first call, subsequent start where previous left off
blk->qStart = blk->qEnd;
blk->tStart = blk->tEnd;
blk->alnStart = blk->alnEnd;

/* find start of next aligned block */
char *qPtr = blk->qAln + blk->alnStart;
char *tPtr = blk->tAln + blk->alnStart;

while ((*qPtr != '\0') && (*tPtr != '\0') && ((*qPtr == '-') || (*tPtr == '-')))
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

// sanity test for blast blocks being the same length after conversion to the same units
assert((blk->tLetMult * (blk->qEnd - blk->qStart)) == (blk->qLetMult * (blk->tEnd - blk->tStart)));
return TRUE;
}

static void addUngappedBlock(struct psl* psl, int* pslSpace, struct block* blk, unsigned flags)
/* add the next  ungapped block to a psl */
{
unsigned newIBlk = psl->blockCount;
unsigned blkSize = blk->qEnd - blk->qStart;  // uses query size so protein psl is right
if (newIBlk >= *pslSpace)
    pslGrow(psl, pslSpace);
psl->qStarts[newIBlk] = blk->qCoordMult * blk->qStart;
psl->tStarts[newIBlk] = blk->tCoordMult * blk->tStart;
psl->blockSizes[newIBlk] = blk->qCoordMult * blkSize;

/* keep bounds current */
psl->qStart = psl->qStarts[0];
psl->qEnd = psl->qStarts[newIBlk] + (blk->qCoordMult * blkSize);
if (psl->strand[0] == '-')
    reverseIntRange(&psl->qStart, &psl->qEnd, psl->qSize);
psl->tStart = psl->tStarts[0];
psl->tEnd = psl->tStarts[newIBlk] + (blk->q2tBlkSizeMult * blkSize);
if (psl->strand[1] == '-')
    reverseIntRange(&psl->tStart, &psl->tEnd, psl->tSize);

if (flags & bldPslx)
    {
    psl->qSequence[newIBlk] = cloneStringZ(blk->qAln + blk->alnStart, blkSize);
    psl->tSequence[newIBlk] = cloneStringZ(blk->tAln + blk->alnStart, blkSize);
    }
psl->blockCount++;
}

static void countIndels(struct psl *psl)
/* update indel counts in psl after adding a block */
{
if (psl->blockCount > 1)
    {
    int iBlk = psl->blockCount - 1;
    if (pslQEnd(psl, iBlk-1) != psl->qStarts[iBlk])
        {
        /* insert in query */
        psl->qNumInsert++;
        psl->qBaseInsert += (psl->qStarts[iBlk] - pslQEnd(psl, iBlk-1));
    }
    if (pslTEnd(psl, iBlk-1) != psl->tStarts[iBlk])
        {
        /* insert in target */
        psl->tNumInsert++;
        psl->tBaseInsert += (psl->tStarts[iBlk] - pslTEnd(psl, iBlk-1));
        }
    }
}

static void countMatches(struct psl* psl, struct block* blk, unsigned flags)
/* update the PSL match/mismatch after adding a block. */
{
int i, alnLen = (blk->alnEnd - blk->alnStart);
char *qPtr = blk->qAln + blk->alnStart;
char *tPtr = blk->tAln + blk->alnStart;
boolean isProt = isProteinSeqs(flags);
for (i = 0; i < alnLen; i++, qPtr++, tPtr++)
    {
    if ((!isProt && ((*qPtr == 'N') || (*tPtr == 'N')))
        || (isProt && ((*qPtr == 'X') || (*tPtr == 'X'))))
        psl->repMatch += blk->countMult;
    else if (*qPtr == *tPtr)
        psl->match += blk->countMult;
    else
        psl->misMatch += blk->countMult;
    }
}

static void hspToBlocks(struct psl *psl, int *pslSpace, struct block *blk, unsigned flags)
/* build PSl blocks from an HSP */
{
/* fill in ungapped blocks */
while (nextUngappedBlk(blk))
    {
    addUngappedBlock(psl, pslSpace, blk, flags);
    countIndels(psl);
    countMatches(psl, blk, flags);
    }
assert(blk->qStart == blk->qEnd);
assert(blk->tStart == blk->tEnd);
// FIXME
//assert(blk->qStart == pslQEnd(psl, psl->blockCount-1));
//assert(blk->tStart == pslTEnd(psl, psl->blockCount-1));
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
if ((flags & tblastn) == 0)
    makeUntranslated(psl);
}

struct psl *pslBuildFromHsp(char *qName, int qSize, int qStart, int qEnd, char qStrand, char *qAln,
                            char *tName, int tSize, int tStart, int tEnd, char tStrand, char *tAln,
                            unsigned flags)
/* construct a new psl from an HSP.  Chaining is left to other programs. */
{
if ((flags & tblastx) && (flags & bldPslx))
    errAbort("can't convert TBLASTX to PSLX");

struct block blk;
blkInit(&blk, qStart, qAln, tStart, tAln, flags);

// construct psl
int pslSpace = 8;
char strand[3];
safef(strand, sizeof(strand), "%c%c", qStrand, tStrand);
struct psl *psl = pslNew(qName, blk.qCoordMult * qSize, blk.qCoordMult * qStart, blk.qCoordMult * qEnd,
                         tName, blk.tCoordMult * tSize, blk.tCoordMult * tStart, blk.tCoordMult * tEnd,
                         strand, pslSpace, ((flags & bldPslx) ? PSL_XA_FORMAT : 0));

// fill in psl
hspToBlocks(psl, &pslSpace, &blk, flags);
finishPsl(psl, flags);
return psl;
}

FILE *pslBuildScoresOpen(char *scoreFile, boolean inclDefs)
/* open score file and write headers */
{
FILE *fh = mustOpen(scoreFile, "w");
fputs("#strand\tqName\tqStart\tqEnd\ttName\ttStart\ttEnd\tbitScore\teVal", fh);
if (inclDefs)
    fputs("\tqDef\ttDef", fh);
fputc('\n', fh);
return fh;
}

static void writeBasicScores(FILE* scoreFh, struct psl *psl, double bitScore, double eValue)
/* write first part of row */
{
fprintf(scoreFh, "%s\t%s\t%d\t%d\t%s\t%d\t%d\t%g\t%g", psl->strand,
        psl->qName, psl->qStart, psl->qEnd, psl->tName, psl->tStart, psl->tEnd,
        bitScore, eValue);
}

void pslBuildScoresWrite(FILE* scoreFh, struct psl *psl, double bitScore, double eValue)
/* write scores for a PSL */
{
writeBasicScores(scoreFh, psl, bitScore, eValue);
fputc('\n', scoreFh);
}

void pslBuildScoresWriteWithDefs(FILE* scoreFh, struct psl *psl, double bitScore, double eValue, char *qDef, char *tDef)
/* write scores and definitions for a PSL */
{
writeBasicScores(scoreFh, psl, bitScore, eValue);
fprintf(scoreFh, "\t%s\t%s\n", qDef, tDef);
}
