/* patSpace - cDNA alignment algorithm that occurs mostly in 
 * pattern space (as opposed to offset space).  This particular
 * implementation tried to stitch together small contigs in
 * unfinished Bacterial Artificial Chromosomes (BACs) by looking
 * for spanning cDNAs. A BAC is a roughly 150,000 base sequence
 * of genomic DNA.  An unfinished BAC consists of multiple
 * unordered non-overlapping subsequences. */
#include "common.h"
#include "obscure.h"
#include "portable.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "nt4.h"
#include "fa.h"
#include "fuzzyFind.h"
#include "cda.h"
#include "sig.h"

#include "htmshell.h"
FILE *bigHtmlFile;
FILE *littleHtmlFile;
int htmlIx;

#define blockSize (1024)

#define maxBlockCount (8*1024)
#define maxGenomeSize (blockSize*maxBlockCount)

#define patSize 11
#define patBitSize  (2*(patSize))
#define patSpaceSize  (1<<(patBitSize))

#define maxIntron (32*1024)
#define minMatch 4

#define maxPatCount (1024*16)        /* Maximum allowed count for one pattern. */

FILE *hitOut;   /* Output file for cDNA/BAC hits. */
FILE *mergerOut; /* Output file for mergers. */
FILE *dumpOut;

boolean dumpMe = FALSE;     /* Set on if dumping extra info. */

int polyT;
int polyA;
int polyG;
int polyC;

void makePolys()
/* Create value for poly T, which will be ignored (since
 * N's get mapped to T's) */
{
int i;
for (i=0; i<patSize; ++i)
    {
    polyT <<= 2;
    polyT += T_BASE_VAL;
    polyA <<= 2;
    polyA += A_BASE_VAL;
    polyC <<= 2;
    polyC += C_BASE_VAL;
    polyG <<= 2;
    polyG += G_BASE_VAL;
    }
}

struct patSpace
/* A pattern space - something that holds an index of all 11-mers in
 * genome. */
    {
    bits16 *lists[patSpaceSize];         /* A list for each 11-mer */
    bits16 listSizes[patSpaceSize];      /* Size of list for each 11-mer */
    bits16 *allocated;                   /* Storage space for all lists. */
    int    blocksUsed;			 /* Number of blocks used, <= maxBlockCount */
    bits16 maxPat;                       /* Max # of times pattern can occur
                                          * before it is ignored. */
    };

struct blockPos
/* Structure that stores where in a BAC we are - 
 * essentially an unpacked block index. */
    {
    int bacIx;          /* Which bac. */
    int seqIx;          /* Which subsequence in bac. */
    struct dnaSeq *seq; /* Pointer to subsequence. */
    int offset;         /* Start position of block within subsequence. */
    int size;           /* Size of block withing subsequence. */
    };

static int hitsAt[maxBlockCount];					/* Count of hits on each block. */
static struct blockPos blockPos[maxBlockCount];     /* Relate block to sequence. */

void fixBlockSize(struct blockPos *bp, int blockIx)
/* Update size field from rest of blockPos. */
{
struct dnaSeq *seq = bp->seq;
int size = seq->size - bp->offset;
if (size > blockSize)
	size = blockSize;
bp->size = size;
if (dumpMe)
    fprintf(dumpOut, "%d (%d %d) %s %d %d\n", blockIx, bp->bacIx, bp->seqIx, seq->name, bp->offset, bp->size);
}



struct patSpace *newPatSpace()
/* Return an empty pattern space. */
{
struct patSpace *ps;
long startTime, endTime;

startTime = clock1000();
ps = needLargeMem(sizeof(*ps));
endTime = clock1000();
startTime = clock1000();
memset(ps, 0, sizeof(*ps));
endTime = clock1000();
return ps;
}

void countPatSpace(struct dnaSeq *seq, struct patSpace *ps)
/* Add up number of times each pattern occurs in sequence into ps->listSizes. */
{
int size = seq->size;
int mask = patSpaceSize-1;
DNA *dna = seq->dna;
int i;
int bits = 0;
int bVal;
int ls;

for (i=0; i<patSize-1; ++i)
    {
    bVal = ntValNoN[dna[i]];
    bits <<= 2;
    bits += bVal;
    }
for (i=patSize-1; i<size; ++i)
    {
    bVal = ntValNoN[dna[i]];
    bits <<= 2;
    bits += bVal;
    bits &= mask;
    ls = ps->listSizes[bits];
    if (ls < maxPatCount)
        ps->listSizes[bits] = ls+1;
    }
}

int allocPatSpaceLists(struct patSpace *ps)
/* Allocate pat space lists and set up list pointers. 
 * Returns size of all lists. */
{
int maxCount = 64*1024-1;
int oneCount;
int count = 0;
int i;
bits16 *listSizes = ps->listSizes;
bits16 **lists = ps->lists;
bits16 *allocated;
int ignoreCount = 0;
bits16 maxPat = ps->maxPat;
int size;
int usedCount = 0, overusedCount = 0;

for (i=0; i<patSpaceSize; ++i)
    {
    /* If pattern is too much used it's no good to us, ignore. */
    if ((oneCount = listSizes[i]) < maxPat)
        {
        count += oneCount;
        usedCount += 1;
        }
    else
        {
        overusedCount += 1;
        }
    }
printf("%d patterns used, %d overused\n", usedCount, overusedCount);
ps->allocated = allocated = needLargeMem(count*sizeof(allocated[0]));
for (i=0; i<patSpaceSize; ++i)
    {
    if ((size = listSizes[i]) < maxPat)
        {
        lists[i] = allocated;
        allocated += size;
        }
    }
return count;
}

int addToPatSpace(int bacIx, int seqIx, struct dnaSeq *seq,int startBlock, struct patSpace *ps)
/* Add contents of one sequence to pattern space. Returns end block. */
{
int size = seq->size;
int mask = patSpaceSize-1;
DNA *dna = seq->dna;
int i;
int bits = 0;
int bVal;
int blockMod = blockSize;
int curCount;
bits16 maxPat = ps->maxPat;
bits16 *listSizes = ps->listSizes;
bits16 **lists = ps->lists;
struct blockPos *bp = &blockPos[startBlock];

bp->bacIx = bacIx;
bp->seqIx = seqIx;
bp->seq = seq;
bp->offset = 0;
fixBlockSize(bp, startBlock);
++bp;
for (i=0; i<patSize-1; ++i)
    {
    bVal = ntValNoN[dna[i]];
    bits <<= 2;
    bits += bVal;
    }
for (i=patSize-1; i<size; ++i)
    {
    bVal = ntValNoN[dna[i]];
    bits <<= 2;
    bits += bVal;
    bits &= mask;
    if ((curCount = listSizes[bits]) < maxPat)
        {
        listSizes[bits] = curCount+1;
        lists[bits][curCount] = startBlock;
        }
    if (--blockMod == 0)
        {
        ++startBlock;
        blockMod = blockSize;
	bp->bacIx = bacIx;
	bp->seqIx = seqIx;
	bp->seq = seq;
	bp->offset = i - (patSize-1) + 1;
	fixBlockSize(bp, startBlock);
	++bp;
        }
    }
for (i=0; i<patSize-1; ++i)
    {
    if (--blockMod == 0)
        {
        ++startBlock;
        blockMod = blockSize;
        }
    }
if (blockMod != blockSize)
    ++startBlock;
return startBlock;
}


struct patSpace *makePatSpace(struct dnaSeq **seqArray, int arraySize, char *oocFileName)
/* Allocate a pattern space and fill from sequences.  (Each element of
   seqArray is a list of sequences. */
{
struct patSpace *ps = newPatSpace();
int i;
int startIx = 0;
int total = 0;
long startTime, endTime;
struct dnaSeq *seq;
int globalOver = 0, localOver = 0;
bits16 maxPat;
bits16 *listSizes;

startTime = clock1000();
maxPat = ps->maxPat = maxPatCount;
for (i=0; i<arraySize; ++i)
    {
    for (seq = seqArray[i]; seq != NULL; seq = seq->next)
        {
        total += seq->size;
        countPatSpace(seq, ps);
        }
    }

endTime = clock1000();
printf("%4.2f seconds to countPatSpace %d bases\n", 0.001*(endTime-startTime), total );

listSizes = ps->listSizes;

/* Scan through over-popular patterns and set their count to value 
 * where they won't be added to pat space. */
    {
    bits32 sig, psz;
    FILE *f = mustOpen(oocFileName, "rb");
    bits32 oli;

    mustReadOne(f, sig);
    mustReadOne(f, psz);
    if (sig != oocSig)
        errAbort("Bad signature on %s\n", oocFileName);
    if (psz != patSize)
        errAbort("Oligo size mismatch in %s. Expecting %d got %d\n", 
            oocFileName, patSize, psz);
    while (readOne(f, oli))
        listSizes[oli] = maxPat;
    fclose(f);
    }

startTime = clock1000();
allocPatSpaceLists(ps);
endTime = clock1000();
printf("%4.2f seconds to allocPatSpaceLists\n", 0.001*(endTime-startTime) );


startTime = clock1000();

/* Zero out pattern counts that aren't oversubscribed. */
for (i=0; i<patSpaceSize; ++i)
    {
    if (listSizes[i] < maxPat)
        listSizes[i] = 0;
    }
if (dumpMe)
    fprintf(dumpOut, "BlockIx vs. Seq position table:\n");

for (i=0; i<arraySize; ++i)
    {
	int j;
    for (seq = seqArray[i], j=0; seq != NULL; seq = seq->next, ++j)
        {
        startIx = addToPatSpace(i, j, seq, startIx, ps);
        if (startIx >= maxBlockCount)
            errAbort("Too many blocks, can only handle %d\n", maxBlockCount);
        }
    }
ps->blocksUsed = startIx;
if (dumpMe)
    fprintf(dumpOut, "\n");
printf("%d blocks of %d used\n", ps->blocksUsed, maxBlockCount);

/* Zero local over-popular patterns. */
for (i=0; i<patSpaceSize; ++i)
    {
    if (listSizes[i] >= maxPat)
        listSizes[i] = 0;
    }

endTime = clock1000();
printf("%4.2f seconds to addToPatSpace\n", 0.001*(endTime-startTime) );

return ps;
}


void bfFind(int block, struct blockPos *ret)
/* Find dna sequence and position within sequence that corresponds to block. */
{
assert(block >= 0 && block < maxBlockCount);
*ret = blockPos[block];
}


void ffFindEnds(struct ffAli *ff, struct ffAli **retLeft, struct ffAli **retRight)
/* Find left and right ends of ffAli. */
{
while (ff->left != NULL)
    ff = ff->left;
*retLeft = ff;
while (ff->right != NULL)
    ff = ff->right;
*retRight = ff;
}



int scoreCdna(struct ffAli *left, struct ffAli *right)
/* Score ali using cDNA scoring. */
{
int size, nGap, hGap;
struct ffAli *ff, *next;
int score = 0, oneScore;


for (ff=left; ;)
    {
    next = ff->right;

    size = ff->nEnd - ff->nStart;
    oneScore = ffScoreMatch(ff->nStart, ff->hStart, size);
    
    if (next != NULL && ff != right)
        {
        /* Penalize all gaps except for intron-looking ones. */
        nGap = next->nStart - ff->nEnd;
        hGap = next->hStart - ff->hEnd;

        /* Process what are stretches of mismatches rather than gaps. */
        if (nGap > 0 && hGap > 0)
            {
            if (nGap > hGap)
                {
                nGap -= hGap;
                oneScore -= hGap;
                hGap = 0;
                }
            else
                {
                hGap -= nGap;
                oneScore -= nGap;
                nGap = 0;
                }
            }
   
        if (nGap < 0)
            nGap = -nGap-nGap;
        if (hGap < 0)
            hGap = -hGap-hGap;
        if (nGap > 0)
            oneScore -= 8*nGap;
        if (hGap > 30)
            oneScore -= 1;
        else if (hGap > 0)
            oneScore -= 8*hGap;
        }
    score += oneScore;
    if (ff == right)
        break;
    ff = next;
    } 
return score;
}

boolean solidMatch(struct ffAli **pLeft, struct ffAli **pRight, DNA *needle, 
    int *retStartN, int *retEndN)
/* Return start and end (in needle coordinates) of solid parts of 
 * match if any. Necessary because fuzzyFinder algorithm will extend
 * ends a little bit beyond where they're really solid.  We want
 * to effectively save these bases for aligning somewhere else. */
{
int minSegSize = 11;
struct ffAli *next;
int segSize;
int runTotal = 0;
int gapSize;
struct ffAli *left = *pLeft, *right = *pRight;

/* Get rid of small segments on left end that are separated from main body. */
for (;;)
    {
    if (left == NULL)
        return FALSE;
    next = left->right;
    segSize = left->nEnd - left->nStart;
    runTotal += segSize;
    if (segSize > minSegSize || runTotal > minSegSize*2)
        break;
    if (next != NULL)
        {
        gapSize = next->nStart - left->nEnd;
        if (gapSize > 1)
            runTotal = 0;
        }
    left = next;
    }
*retStartN = left->nStart - needle;

/* Do same thing on right end... */
runTotal = 0;
for (;;)
    {
    if (right == NULL)
        return FALSE;
    next = right->left;
    segSize = right->nEnd - right->nStart;
    runTotal += segSize;
    if (segSize > minSegSize || runTotal > minSegSize*2)
        break;
    if (next != NULL)
        {
        gapSize = next->nStart - left->nEnd;
        if (gapSize > 1)
            runTotal = 0;
        }
    right = next;
    }
*retEndN = right->nEnd - needle;

*pLeft = left;
*pRight = right;
return *retEndN - *retStartN >= minSegSize;
}

struct cdnaAliList
/* This structure keeps a list of where a particular cDNA aligns.
 * The list isn't kept long, just while that cDNA is in memory. */
    {
    struct cdnaAliList *next;   /* Link to next element on list. */
    int bacIx;                  /* Which BAC. */
    int seqIx;                  /* Which sequence in BAC. */
    int start, end;             /* Position within cDNA sequence. */
    char strand;                /* Was cdna reverse complemented? + if no, - if yes. */
    double cookedScore;
    };

struct cdnaAliList *calFreeList = NULL; /* List of reusable cdnaAliList elements. */

struct cdnaAliList *newCal(int bacIx, int seqIx, int start, int end, int size, char strand,
    double cookedScore)
/* Get a new element for this cdnaList, off of free list if possible. */
{
struct cdnaAliList *cal;
if ((cal = calFreeList) == NULL)
    {
    AllocVar(cal);
    }
else
    {
    calFreeList = cal->next;
    }
cal->next = NULL;
cal->bacIx = bacIx;
cal->seqIx = seqIx;;
cal->strand = strand;
cal->cookedScore = cookedScore;
if (strand == '-')
    {
    cal->start = size - end;
    cal->end = size - start;
    }
else
    {
    cal->start = start;
    cal->end = end;
    }
return cal;
}

void freeCal(struct cdnaAliList *cal)
/* Free up one cal. */
{
slAddHead(&calFreeList, cal);
}

void freeCalList(struct cdnaAliList **pList)
/* Free up cal list for reuse. */
{
struct cdnaAliList *cal, *next;

for (cal = *pList; cal != NULL; cal = next)
    {
    next = cal->next;
    freeCal(cal);
    }
*pList = NULL;
}

int ffSubmitted = 0;
int ffAccepted = 0;
int ffOkScore = 0;
int ffSolidMatch = 0;

void writeClump(struct blockPos *first, struct blockPos *last,
    char *cdnaName, char strand, DNA *cdna, int cdnaSize, struct cdnaAliList **pList)
/* Write hitOut one clump. */
{
struct dnaSeq *seq = first->seq;
char *bacName = seq->name;
int seqIx = first->seqIx;
int start = first->offset;
int end = last->offset+last->size;
struct ffAli *ff, *left, *right;
int extraAtEnds = minMatch*patSize;
struct cdnaAliList *cal;

start -= extraAtEnds;
if (start < 0)
    start = 0;
end += extraAtEnds;
if (end >seq->size)
    end = seq->size;

++ffSubmitted;
if (dumpMe)
	fprintf(dumpOut, "%s %d %s %d-%d\n", cdnaName, cdnaSize, bacName, start, end);
ff = ffFind(cdna, cdna+cdnaSize, seq->dna+start, seq->dna+end, ffCdna);
if (dumpMe)
    {
    fprintf(dumpOut, "ffFind = %x\n", ff);
    }
if (ff != NULL)
    {
    int ffScore = ffScoreCdna(ff);
    ++ffAccepted;
    if (dumpMe) fprintf(dumpOut, "ffScore = %d\n", ffScore);
    if (ffScore >= 22)
        {
        int hiStart, hiEnd;
        int oldStart, oldEnd;

        ffFindEnds(ff, &left, &right);
        hiStart = oldStart = left->nStart - cdna;
        hiEnd = oldEnd = right->nEnd - cdna;
        ++ffOkScore;

        if (solidMatch(&left, &right, cdna, &hiStart, &hiEnd))
            {
            int solidSize = hiEnd - hiStart;
            int solidScore;
            int seqStart, seqEnd;
            double cookedScore;

            solidScore = scoreCdna(left, right);
            cookedScore = (double)solidScore/solidSize - 0.01;
            if (cookedScore > 0.01)
                {
                ++ffSolidMatch;

                seqStart = left->hStart - seq->dna;
                seqEnd = right->hEnd - seq->dna;
                fprintf(hitOut, "%3.1f%% %c %s:%d-%d (old %d-%d) of %d at %s.%d:%d-%d\n", 
                    100.0 * cookedScore, strand, cdnaName, 
                    hiStart, hiEnd, oldStart, oldEnd, cdnaSize,
                    bacName, seqIx, seqStart, seqEnd);

                if (dumpMe)
                    {
                    fprintf(bigHtmlFile, "<A NAME=i%d>", htmlIx);
                    fprintf(bigHtmlFile, "<H2>%4.1f%% %4d %4d %c %s:%d-%d of %d at %s.%d:%d-%d</H2><BR>", 
                        100.0 * cookedScore, solidScore, ffScore, strand, cdnaName, 
                        hiStart, hiEnd, cdnaSize,
                        bacName, seqIx, seqStart, seqEnd);
                    fprintf(bigHtmlFile, "</A>");
                    ffShAli(bigHtmlFile, ff, cdnaName, cdna, cdnaSize, 0,
                        bacName, seq->dna+start, end-start, start, FALSE);
                    fprintf(bigHtmlFile, "<BR><BR>\n");

                    fprintf(littleHtmlFile, "<A HREF=\"patAli.html#i%d\">", htmlIx);
                    fprintf(littleHtmlFile, "%4.1f%% %4d %4d %c %s:%d-%d of %d at %s.%d:%d-%d\n", 
                        100.0 * cookedScore, solidScore, ffScore, strand, cdnaName, 
                        hiStart, hiEnd, cdnaSize,
                        bacName, seqIx, seqStart, seqEnd);
                    fprintf(littleHtmlFile, "</A><BR>");
                    ++htmlIx;
                    }

                cal = newCal(first->bacIx, seqIx, hiStart, hiEnd, cdnaSize, strand, cookedScore);
                slAddHead(pList, cal);
                }
            }
        }
    ffFreeAli(&ff);
    }
}

boolean noMajorOverlap(struct cdnaAliList *cal, struct cdnaAliList *clump)
/* Return TRUE if cal doesn't overlap in a big way with what's already in clump. */
{
int calStart = cal->start;
int calEnd = cal->end;
int calSize = calEnd - calStart;
int maxOverlap = calSize/3;
int s, e, o;

for (; clump != NULL; clump = clump->next)
    {
    s = max(calStart, clump->start);
    e = min(calEnd, clump->end);
    o = e - s;
    if (o > maxOverlap)
        return FALSE;
    }
return TRUE;
}

void writeMergers(struct cdnaAliList *calList, char *cdnaName, int cdnaSize, char *bacNames[])
/* Write out any mergers indicated by this cdna. This destroys calList. */
{
struct cdnaAliList *startBac, *endBac, *cal, *prevCal, *nextCal;
int bacCount;
int bacIx;

for (startBac = calList; startBac != NULL; startBac = endBac)
    {
    /* Scan until find a cal that isn't pointing into the same BAC. */
    bacCount = 1;
    bacIx = startBac->bacIx;
    prevCal = startBac;
    for (cal =  startBac->next; cal != NULL; cal = cal->next)
        {
        if (cal->bacIx != bacIx)
            {
            prevCal->next = NULL;
            break;
            }
        ++bacCount;
        prevCal = cal;
        }
    endBac = cal;
    if (bacCount > 1)
        {
        while (startBac != NULL)
            {
            struct cdnaAliList *clumpList = NULL, *leftoverList = NULL;
            for (cal = startBac; cal != NULL; cal = nextCal)
                {
                nextCal = cal->next;
                if (noMajorOverlap(cal, clumpList))
                    {
                    slAddHead(&clumpList, cal);
                    }
                else
                    {
                    slAddHead(&leftoverList, cal);
                    }
                }
            slReverse(&clumpList);
            slReverse(&leftoverList);
            if (slCount(clumpList) > 1)
                {
                char lastStrand = 0;
                boolean switchedStrand = FALSE;
                fprintf(mergerOut, "%s glues %s contigs", cdnaName, bacNames[bacIx]);
                lastStrand = clumpList->strand;
                for (cal = clumpList; cal != NULL; cal = cal->next)
                    {
                    if (cal->strand != lastStrand)
                        switchedStrand = TRUE;
                    fprintf(mergerOut, " %d %c (%d-%d) %3.1f%%", cal->seqIx, cal->strand, 
                        cal->start, cal->end, 100.0*cal->cookedScore);
                    }
                fprintf(mergerOut, "\n");
                }
            freeCalList(&clumpList);
            startBac = leftoverList;
            }        
        }
    else
        {
        freeCalList(&startBac);
        }
    }
}

void clumpHits(int hitBlocks[], int hitCount, int posBuf[], 
    char *dnaName, DNA *dna, int dnaSize, char strand, struct cdnaAliList **pList)
/* Double-check blocks, do fuzzy find. */
{
/* Clump together hits. */
int clumpSize = 1;
int block;
int i;
struct blockPos first, cur, pre;

if (dumpMe)
    {
    fprintf(dumpOut, "hitBlocks[] = ");
    for (i=0; i<hitCount; ++i)
        {
        fprintf(dumpOut, "%d ", hitBlocks[i]);
        }
    fprintf(dumpOut, "\n");
    }

bfFind(hitBlocks[0], &first);
pre = first;
for (i=1; i<hitCount; ++i)
    {
    block = hitBlocks[i];
    bfFind(block, &cur);
    if (cur.seq != pre.seq || cur.offset - (pre.offset + pre.size) > maxIntron)
        {
        /* Write hitOut old clump and start new one. */
        writeClump(&first, &pre,
            dnaName, strand, dna, dnaSize, pList);
        first = cur;
        }
    else
        {
        /* Extend old clump. */
        }
    pre = cur;
    }
/* Write hitOut last clump. */
writeClump(&first, &pre,
    dnaName, strand, dna, dnaSize, pList);

}

int grandTotalHits = 0;

void patSpaceFindOne(struct patSpace *ps, DNA *dna, int dnaSize, 
    char strand, char *estName, int estIx, struct cdnaAliList **pList)
/* Find occurrences of DNA in patSpace and print to hitOut. */
{
int lastStart = dnaSize - patSize;
int i,j;
int pat;
static int posBuf[maxBlockCount];
static int hitBlocks[maxBlockCount];
int hitBlockCount = 0;
int totalSigHits = 0;
DNA *tile = dna;
int blocksUsed = ps->blocksUsed;

memset(posBuf, 0, sizeof(posBuf));
for (i=0; i<=lastStart; i += patSize)
    {
    pat = 0;
    for (j=0; j<patSize; ++j)
        {
        int bVal = ntValNoN[tile[j]];
        pat <<= 2;
        pat += bVal;
        }
    if (pat != polyT && pat != polyA && pat != polyC && pat != polyG)
        {
        bits16 *list = ps->lists[pat];
        bits16 count = ps->listSizes[pat];
        for (j=0; j<count; ++j)
            posBuf[list[j]] += 1;            
        }
    tile += patSize;
    }

if (dumpMe)
    {
    int modMax = 25;
    int mod = modMax;
    fprintf(dumpOut, "posBuf[%d] = \n", maxBlockCount);
    for (i=0; i<blocksUsed; ++i)
        {
        fprintf(dumpOut, "%2d ", posBuf[i]);
        if (--mod == 0)
            {
            fprintf(dumpOut, "\n");
            mod = modMax;
            }
        }
    fprintf(dumpOut, "\n\n");
    }

/* Scan through array that records counts of hits at positions. */
for (i=0; i<blocksUsed-1; ++i)
    {
    /* Save significant hits in a more compact array */
    int a = posBuf[i], b = posBuf[i+1];
    int sum = a + b;
    if (sum >= minMatch)
        {
        if (a > 0)
            {
            if (hitBlockCount == 0 || hitBlocks[hitBlockCount-1] != i)
                {
                hitBlocks[hitBlockCount++] = i;
                totalSigHits += a;
                hitsAt[i] += 1;
                }
            }
        if (b > 0)
            {
            hitBlocks[hitBlockCount++] = i+1;
            totalSigHits += b;
            hitsAt[i+1] += 1;
            }
        }
    }
grandTotalHits += totalSigHits;

/* Output data on cDNA with significant hits. */
if (hitBlockCount > 0 && totalSigHits*patSize*8 > dnaSize)
    {
    clumpHits(hitBlocks, hitBlockCount, posBuf, estName, dna, dnaSize, strand, pList);
    }        
}

void usage()
{
errAbort("patSpace - tell where a cDNA is located quickly.\n"
         "usage:\n"
         "    patSpace genomeListFile cdnaListFile ignore.ooc hitFile.out mergerFile.out");
}


boolean fastFaReadNext(FILE *f, DNA **retDna, int *retSize, char **retName)
/* Read in next FA entry as fast as we can. Return FALSE at EOF. */
{
static int bufSize = 0;
static DNA *buf;
int c;
int bufIx = 0;
static char name[256];
int nameIx = 0;
boolean gotSpace = FALSE;

/* Seek to next '\n' and save first word as name. */
name[0] = 0;
for (;;)
    {
    if ((c = fgetc(f)) == EOF)
        {
        *retDna = NULL;
        *retSize = 0;
        *retName = NULL;
        return FALSE;
        }
    if (!gotSpace && nameIx < ArraySize(name)-1)
        {
        if (isspace(c))
            gotSpace = TRUE;
        else
            {
            name[nameIx++] = c;
            }
        }
    if (c == '\n')
        break;
    }
name[nameIx] = 0;
/* Read until next '>' */
for (;;)
    {
    c = fgetc(f);
    if (isspace(c) || isdigit(c))
        continue;
    if (c == EOF || c == '>')
        c = 0;
    else
        c = tolower(c);
    if (bufIx >= bufSize)
        {
        if (bufSize == 0)
            {
            bufSize = 64 * 1024;
            buf = needMem(bufSize);
            }
        else
            {
            DNA *newBuf;
            int newBufSize = bufSize + bufSize;
            newBuf = needMem(newBufSize);
            memcpy(newBuf, buf, bufIx);
            freeMem(buf);
            buf = newBuf;
            bufSize = newBufSize;
            }
        }
    buf[bufIx++] = c;
    if (c == 0)
        {
        *retDna = buf;
        *retSize = bufIx-1;
        *retName = name;
        return TRUE;
        }
    }
}


int main(int argc, char *argv[])
{
char *genoListName;
char *cdnaListName;
char *oocFileName;
char *hitFileName;
char *mergerFileName;
struct patSpace *patSpace;
long startTime, endTime;
char **genoList;
int genoListSize;
char *genoListBuf;
char **cdnaList;
int cdnaListSize;
char *cdnaListBuf;
char *genoName;
int i;
int estIx = 0;
struct dnaSeq **seqListList = NULL, *seq;

if (dumpMe)
    {
    bigHtmlFile = mustOpen("C:\\inetpub\\wwwroot\\test\\patAli.html", "w");
    littleHtmlFile = mustOpen("C:\\inetpub\\wwwroot\\test\\patSpace.html", "w");
    htmStart(bigHtmlFile, "PatSpace Alignments");
    htmStart(littleHtmlFile, "PatSpace Index");
    }

if (argc != 6)
    usage();

startTime = clock1000();
dnaUtilOpen();
makePolys();
genoListName = argv[1];
cdnaListName = argv[2];
oocFileName = argv[3];
hitFileName = argv[4];
mergerFileName = argv[5];

readAllWords(genoListName, &genoList, &genoListSize, &genoListBuf);
readAllWords(cdnaListName, &cdnaList, &cdnaListSize, &cdnaListBuf);
hitOut = mustOpen(hitFileName, "w");
mergerOut = mustOpen(mergerFileName, "w");
dumpOut = mustOpen("dump.out", "w");
seqListList = needMem(genoListSize*sizeof(seqListList[0]) );
fprintf(hitOut, "Pattern space 0.2 cDNA matcher\n");
fprintf(hitOut, "cDNA files: ", cdnaListSize);
for (i=0; i<cdnaListSize; ++i)
    fprintf(hitOut, " %s", cdnaList[i]);
fprintf(hitOut, "\n");
fprintf(hitOut, "%d genomic files\n", genoListSize);
for (i=0; i<genoListSize; ++i)
    {
    genoName = genoList[i];
    if (!startsWith("//", genoName)  )
        {
        seqListList[i] = seq = faReadAllDna(genoName);
        fprintf(hitOut, "%d els in %s ", slCount(seq), genoList[i]);
        for (; seq != NULL; seq = seq->next)
            fprintf(hitOut, "%d ", seq->size);
        fprintf(hitOut, "\n");
        }
    }

patSpace = makePatSpace(seqListList, genoListSize, oocFileName);

for (i=0; i<cdnaListSize; ++i)
    {
    FILE *f;
	char *estFileName;
    DNA *dna;
    char *estName;
    int size;
    int c;
    int maxSizeForFuzzyFind = 20000;
    int dotCount = 0;

	estFileName = cdnaList[i];
    if (startsWith("//", estFileName)  )
		continue;

	f = mustOpen(estFileName, "rb");
	while ((c = fgetc(f)) != EOF)
        if (c == '>')
            break;
    printf("%s", cdnaList[i]);
    fflush(stdout);
    while (fastFaReadNext(f, &dna, &size, &estName))
        {
        if (size < maxSizeForFuzzyFind)  /* Some day need to fix this somehow... */
            {
            struct cdnaAliList *calList = NULL;
            patSpaceFindOne(patSpace, dna, size, '+', estName, estIx, &calList);
            reverseComplement(dna, size);
            patSpaceFindOne(patSpace, dna, size, '-', estName, estIx, &calList);
            slReverse(&calList);
            writeMergers(calList, estName, size, genoList);
            ++estIx;
            if ((estIx & 0xfff) == 0)
                {
                printf(".");
                ++dotCount;
                fflush(stdout);
                }
            }
        }
    printf("\n");
    }
printf("raw %4d ffSubmitted %3d ffAccepted %3d ffOkScore %3d ffSolidMatch %2d\n",
    grandTotalHits, ffSubmitted, ffAccepted, ffOkScore, ffSolidMatch);

endTime = clock1000();

printf("Total time is %4.2f\n", 0.001*(endTime-startTime));

if (dumpMe)
    {
    htmEnd(bigHtmlFile);
    htmEnd(littleHtmlFile);
    }
return 0;
}

