/* splat - Speedy Local Alignment Tool. */
/* Copyright 2008 Jim Kent all rights reserved. */

/* Currently the program is just partially implemented.  The indexing part seems to work.
 * Need still to extend alignments of index hits (where we expect only about 1 in 200
 * extensions to be exact if running on whole genome) and then to write out the alignments. 
 * UCSC reviewers - I'm working on this for Kent Informatics rather than UCSC, so no
 * need for you to review it.  */

/* The algorithm is designed to map 25-mers accommodating up to two base substitutions,
 * or one substitution and a single base insertion or deletion. The idea of the algorithm
 * is based on the idea that there will be a very similar 12-mer, and a very similar
 * 6-mer in the query sequence.  First a "splix" indexed which is combined 12-mer and 6-mers
 * is constructed, and then the index is searched allowing a single base mismatch and a
 * single base shift between the 12-mers and 6-mers. 
 *
 * This all probably sounds a little vague.  To make it more concrete let's introduce some
 * notation.  Let the letters A-Y represent locations 1-25 of the 25-mer.  The 25-mer as a
 * whole is represented as ABCDEFGHIJKLMNOPQRSTUVWXY.  In our search we'll consider two
 * major cases - those where there is a near-perfect match on the left side (ABCDEFGHIJKL)
 * and where there is a near perfect match on the right side (NOPQRSTUVWXYZ).  
 *
 * Since we are only allowing only one indel, it must be either on the right or the left 
 * side. Thus the other side must be free from indels.  Since we allow only a single mismatch
 * if there is an indel, this indel-free side must differ by at most one base between the 
 * query and the genome. Thus if we index all 12-mers in the genome, and look up the side and
 * all possible all possible single base differences from that side in the index, we will be
 * guaranteed to be able to find the true mapping location (if any) in the index.  Since there
 * are 2 alternative bases at each of 12 positions in the side, plus the side itself, there are
 * 37 lookups we need to do for each side,  and a total of 74 for both sides of the query.
 *
 * Unfortunately in a 4 gigabase genome, approximately the size of the human genome, 
 * one would expect each 12-mer to occur 256 times. Since we're searching 74 times, this
 * means we expect 74 * 256 = 18944 spurious matches. We'd like to be a little more specific
 * than this.  This is where the 6-mers come in.  For now let's just consider the case
 * where the left side is the indel free near-perfect match.  In the splix index
 * there is a list of all six-mers that come after the 12-mer, and also a list of
 * all six-mers that come immemediately after that.  Thus in the case where there
 * is a perfect match to the query in the genome, there will be an index entry
 * corresponding to 
 *        ABCDEFGHIJKL MNOPQR STUVWX
 * where we call MNOPQR the 'after1' and STUVWX the 'after2.'
 *
 * In the no-indel case, we have up to two mismatches. If one of them is in the 12-mer,
 * then either after1 or after2 will be a perfect match.  If the 12-mer is perfect,
 * then we have two mismatches to distribute between after1 and after2.  If both mismatches
 * are in one of the afters, then the other will be perfect,  otherwise both afters will have
 * exactly one mismatch.  So, in the worst case, so long as we look for near-perfect matches
 * to both after1 and after2, we will be guaranteed to find the true mapping. 
 *
 * This will end up narrowing down our search a lot.  Each 6-mer lookup happens by chance just
 * 1 in 4096 times.  We will need to do 2 * (3 * 6 + 1) = 38 searches though to handle the
 * off-by-one.  This gets us down to approximately 176 spurious matches for every true match.
 *
 * The segmented nature of the index, and the fact that our query has 25 rather than 24
 * bases together make it possible to accommodate single base insertions in this scheme
 * relatively inexpensively. Considering just to consider the case where the indel is on
 * the right side,  it can either occur in the MNOPQR section of the STUVWX section.  If
 * it occurs in the latter section, there is no problem, because MNOPQR will match.  We only
 * need to consider then the case where the indel is in the 'third quadrant' that is in
 * MNOPQR.
 *
 * Let's represent the matching part of the genome with the same letters.  An insertion in 
 * the genome in this quadrant would thus yeild one of the following things in the splix index:
 *      ABCDEFGHIJKL +MNOPQ RSTUVW
 *      ABCDEFGHIJKL M+NOPQ RSTUVW
 *      ABCDEFGHIJKL MN+OPQ RSTUVW
 *      ABCDEFGHIJKL MNO+PQ RSTUVW
 *      ABCDEFGHIJKL MNOP+Q RSTUVW
 *      ABCDEFGHIJKL MNOPQ+ RSTUVW
 * Note in all cases the after2 is RSTUVW.  Fortunately this is a sequence that is found in
 * the query, just using bases 17-23  instead of the usual 18-24 for the after2.
 *
 * A deletion in the genome would be one of the following cases:
 *      ABCDEFGHIJKL NOPQRS TUVWXY
 *      ABCDEFGHIJKL MOPQRS TUVWXY
 *      ABCDEFGHIJKL MNPQRS TUVWXY
 *      ABCDEFGHIJKL MNOQRS TUVWXY
 *      ABCDEFGHIJKL MNOPRS TUVWXY
 *      ABCDEFGHIJKL MNOPQS TUVWXY
 * Note in all cases the after2 is TUVWXY.  Fortunately this is also a sequence that is found
 * in the query, just usig bases 19-25 instead of the usual 18-24 for the after2.
 *
 * Thus by doing a search on the after2 shifted one base left and right we pick up all single
 * base deletions. This involves 38 searches, and has the effect of bringing us up to an
 * expectation of 352 spurious matches.
 *
 * Doing 352 extensions per read is not that much, but still we want to do 100's of millions
 * of reads in a timely manner, so the strategy here is to do as little work per extension
 * as possible.  From the index search we already know the mismatches
 * in 18 of the query bases, know if there is an indel, and if there is an indel have
 * localized it to within about 6 bases.  So we only need to do the extension on the parts
 * that haven't been searched in the index.  The code to do this is a little long and
 * contains quite a few cases, but it contains no nested loops, and the single loops just run
 * about 6 times so it is quite fast.
 */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "localmem.h"
#include "dnaseq.h"
#include "dnaLoad.h"
#include "splix.h"
#include "intValTree.h"
#include "fa.h"
#include "fuzzyFind.h"
#include "psl.h"
#include "axt.h"
#include "maf.h"
#include "splat.h"

static char const rcsid[] = "$Id: splat.c,v 1.29 2008/10/31 05:51:59 kent Exp $";

char *version = "31";	/* Program version number. */

/* Command line driven variables. */
static char *over = NULL;
static char *out = "splat";
static int maxDivergence = 5;
static boolean worseToo = FALSE;
static int maxRepeat = 10;
static char *repeatOutput = NULL;
static int maxGap = 1;
static int maxMismatch = 2;
static boolean memoryMap = FALSE;
static int tagSize;

static boolean exactOnly;	/* Set to true if maxGap and maxMismatch are both 0. */

static void usage()
/* Explain usage and exit. */
{
errAbort(
"splat - SPeedy Local Alignment Tool. v%s. Designed to align small reads to large\n"
"DNA databases such as genomes quickly.  Reads need to be at least 25 bases.\n"
"usage:\n"
"   splat target query output\n"
"where:\n"
"   target is a large indexed dna database in splix format.  Use splixMake to create this\n"
"          file from fasta, 2bit or nib format\n"
"   query is a fasta or fastq file full of sequences of at least 25 bases each\n"
"   output is the output alignment file, by default in .splat format\n"
"note: can use 'stdin' or 'stdout' as a file name for better piping\n"
"overall options:\n"
"   -out=format - Output format.  Options include splat (default), psl, maf, [soap, eland soon....]\n"
"   -worseToo - if set return alignments other than the best alignments\n"
"   -maxRepeat=N  - maximum number of alignments to output on one query sequence. Default %d\n"
"   -over=fileName - Name of file with list of 25-mers that map many times in genome.\n"
"       Helps speed in some cases.  Typically use file human10.over for human genome.\n"
"   -repeatOutput=fileName.fa - Put reads that map more than maxRepeat times in here\n"
"options effecting just first 25 bases\n"
"   -maxGap=N. Maximum gap size. Default is %d. Set to 0 for no gaps\n"
"   -maxMismatch=N. Maximum number of mismatches. Default %d. (A gap counts as a mismatch.)\n"
"   -maxDivergence=N Maximum divergence level between read and genome to map.  Default %d\n"
"                    Divergence combines gaps and mismatches.  Mismatch=2, gap=3\n"
"   -mmap - Use memory mapping. Faster just a few reads, but much slower for many reads\n"
, version, maxRepeat, maxGap, maxMismatch, maxDivergence
);
}

static struct optionSpec options[] = {
   {"over", OPTION_STRING},
   {"out", OPTION_STRING},
   {"maxDivergence", OPTION_FLOAT},
   {"worseToo", OPTION_BOOLEAN},
   {"maxRepeat", OPTION_INT},
   {"repeatOutput", OPTION_STRING},
   {"maxGap", OPTION_INT},
   {"maxMismatch", OPTION_INT},
   {"mmap", OPTION_BOOLEAN},
   {NULL, 0},
};

static FILE *repeatOutputFile = NULL;
int overArraySize;
bits64 *overArray;

long exactIndexQueries, hits12, hits18, hits25, hitsFull;

static int splatTagCmp(const void *va, const void *vb)
/* Sort tags based on position fields, then divergence. */
{
const struct splatTag *a = *((struct splatTag **)va);
const struct splatTag *b = *((struct splatTag **)vb);
int diff;

if ((diff = a->t1 - b->t1) != 0) return diff;
int aEnd = a->t2 + a->size2;
int bEnd = b->t2 + b->size2;
if ((diff = aEnd - bEnd) != 0) return diff;
if ((diff = a->strand - b->strand) != 0) return diff;
if ((diff = a->divergence - b->divergence) != 0) return diff;
if ((diff = a->q1 - b->q1) != 0) return diff;
aEnd = a->q2 + a->size2;
bEnd = b->q2 + b->size2;
return aEnd - bEnd;
}

void splatAlignFree(struct splatAlign **pAli)
/* Free up a splatAlign. */
{
struct splatAlign *ali = *pAli;
if (ali != NULL)
    {
    chainFree(&ali->chain);
    freez(pAli);
    }
}

void splatAlignFreeList(struct splatAlign **pList)
/* Free a list of dynamically allocated splatAlign's */
{
struct splatAlign *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    splatAlignFree(&el);
    }
*pList = NULL;
}


void splatAlignFreeList(struct splatAlign **pList);
/* Free up a list of splatAligns. */


static int splatAlignCmpScore(const void *va, const void *vb)
/* Sort alignments based on score. */
{
const struct splatAlign *a = *((struct splatAlign **)va);
const struct splatAlign *b = *((struct splatAlign **)vb);
double diff = a->chain->score - b->chain->score;
if (diff < 0)
    return -1;
else if (diff > 0)
    return 1;
else
    return 0;
}


#ifdef DEBUG
static void dumpSixmer(int hexamer)
/* Write out hexamer */
{
fputc(valToNt[(hexamer>>10)&3], stdout);
fputc(valToNt[(hexamer>>8)&3], stdout);
fputc(valToNt[(hexamer>>6)&3], stdout);
fputc(valToNt[(hexamer>>4)&3], stdout);
fputc(valToNt[(hexamer>>2)&3], stdout);
fputc(valToNt[hexamer&3], stdout);
}

static void dumpTwelvemer(int twelvemer)
{
fputc(valToNt[(twelvemer>>22)&3], stdout);
fputc(valToNt[(twelvemer>>20)&3], stdout);
fputc(valToNt[(twelvemer>>18)&3], stdout);
fputc(valToNt[(twelvemer>>16)&3], stdout);
fputc(valToNt[(twelvemer>>14)&3], stdout);
fputc(valToNt[(twelvemer>>12)&3], stdout);
dumpSixmer(twelvemer);
}
#endif /* DEBUG */


static void dnaToBinary(DNA *dna, int maxGap, int *pFirstHalf, int *pSecondHalf,
	bits16 *pAfter1, bits16 *pAfter2, bits16 *pBefore1, bits16 *pBefore2,
	bits16 *pFirstHex, bits16 *pLastHex,
	bits16 *pTwoAfterFirstHex, bits16 *pTwoBeforeLastHex)
/* Convert from DNA string to binary representation chopped up various ways. */
{
bits32 firstHalf
    = (ntValNoN[(int)dna[0]] << 22) + (ntValNoN[(int)dna[1]] << 20)
    + (ntValNoN[(int)dna[2]] << 18) + (ntValNoN[(int)dna[3]] << 16)
    + (ntValNoN[(int)dna[4]] << 14) + (ntValNoN[(int)dna[5]] << 12)
    + (ntValNoN[(int)dna[6]] << 10) + (ntValNoN[(int)dna[7]] << 8)
    + (ntValNoN[(int)dna[8]] << 6) + (ntValNoN[(int)dna[9]] << 4)
    + (ntValNoN[(int)dna[10]] << 2) + (ntValNoN[(int)dna[11]]);
bits16 after1 
    = (ntValNoN[(int)dna[12]] << 10) + (ntValNoN[(int)dna[13]] << 8)
    + (ntValNoN[(int)dna[14]] << 6) + (ntValNoN[(int)dna[15]] << 4)
    + (ntValNoN[(int)dna[16]] << 2) + (ntValNoN[(int)dna[17]]);
bits16 after2 
    = (ntValNoN[(int)dna[18]] << 10) + (ntValNoN[(int)dna[19]] << 8)
    + (ntValNoN[(int)dna[20]] << 6) + (ntValNoN[(int)dna[21]] << 4)
    + (ntValNoN[(int)dna[22]] << 2) + (ntValNoN[(int)dna[23]]);
bits32 secondHalf;
bits16 before1, before2;
bits16 firstHex = firstHalf >> 12;
bits16 lastHex;
if (maxGap)
    {
    int base24 = ntValNoN[(int)dna[24]];
    lastHex = ((after2 << 2) + base24) & 0xFFF;
    secondHalf = (((((bits32)after1)<<14) + (after2<<2) + base24) & 0xFFFFFF);
    before1 = ((firstHalf>>10)&0xFFF);
    before2 = (((firstHalf<<2) + (ntValNoN[(int)dna[12]])) & 0xFFF);
    }
else
    {
    lastHex = after2;
    secondHalf = (((bits32)after1)<<12) + after2;
    before1 = firstHex;
    before2 = firstHalf & 0xFFF;
    }

*pFirstHalf = firstHalf;
*pSecondHalf = secondHalf;
*pAfter1 = after1;
*pAfter2 = after2;
*pBefore1 = before1;
*pBefore2 = before2;
*pFirstHex = firstHex;
*pLastHex = lastHex;
*pTwoAfterFirstHex = (firstHalf >> 8) & 0xFFF;
*pTwoBeforeLastHex = (secondHalf >> 4) & 0xFFF;
}

#ifdef SOON
static boolean chopOutPieces(bits64 bases25, int maxGap, int *pFirstHalf, int *pSecondHalf,
	bits16 *pAfter1, bits16 *pAfter2, bits16 *pBefore1, bits16 *pBefore2,
	bits16 *pFirstHex, bits16 *pLastHex,
	bits16 *pTwoAfterFirstHex, bits16 *pTwoBeforeLastHex)
/* Convert from DNA string to binary representation chopped up various ways. */
{
}
#endif /* SOON */

int hexCmp(const void *va, const void *vb)
/* Compare two bit16. */
{
bits16 a = *((bits16*)va), b = *((bits16*)vb);
return a-b;
}

int binaryFindHex(bits16 val, bits16 *pos, 
	int posCount)
/* Find index of hex that matches val, or -1 if none such. */
{
int startIx=0, endIx=posCount-1, midIx;
bits16 posVal;

for (;;)
    {
    if (startIx == endIx)
        {
	posVal = pos[startIx];
	if (posVal == val)
	    return startIx;
	else
	    return -1;
	}
    midIx = ((startIx + endIx)>>1);
    posVal = pos[midIx];
    if (posVal < val)
        startIx = midIx+1;
    else
        endIx = midIx;
    }
}


int linearFindHex(bits16 query, bits16 *hexes, int hexCount)
/* See if query sequence is in hexes, which is sorted. */
{
int i;
for (i=0; i<hexCount; ++i)
    if (hexes[i] == query)
	return i;
return -1;
}

static void exactIndexHits(bits16 hex, bits16 *sortedHexes, int slotSize, 
	bits32 *offsets, int startOffset, int subCount, int gapSize, 
	int missingQuad, struct lm *lm, struct splatHit **pHitList)
/* Look for hits that involve no index mismatches.  Put resultint offsets (plus startOffset)
 * into hitTree. */
{
hits12 += slotSize;
int ix = binaryFindHex(hex, sortedHexes, slotSize);
if (ix >= 0)
    {
    for (;;)
	{
	int dnaOffset = offsets[ix] + startOffset;
	struct splatHit *hit;
	lmAllocVar(lm, hit);
	hit->tOffset = dnaOffset;
	hit->gapSize = gapSize;
	hit->missingQuad = missingQuad;
	hit->subCount = subCount;
	slAddHead(pHitList, hit);
	ix += 1;
	++hits18;
	if (sortedHexes[ix] != hex)
	    break;
	}
    }
}

static void searchExact(struct splix *splix, int twelvemer, bits16 sixmer, int whichSixmer,
	int dnaStartOffset, int subCount, int gapSize, int missingQuad,
	struct lm *lm, struct splatHit **pHitList)
/* If there's an exact match put DNA offset of match into hitTree. */
{
exactIndexQueries += 1;
int slotSize = splix->slotSizes[twelvemer];
if (slotSize != 0)
    {
    /* Compute actual positions of the subindexes for the 1/4 reads in the slot. */
    char *slot = splix->slots[twelvemer];
    bits16 *hexesSixmer = (bits16*)(slot + whichSixmer*sizeof(bits16)*slotSize);
    bits32 *offsetsSixmer = (bits32*)((8+sizeof(bits32)*whichSixmer)*slotSize + slot);
    exactIndexHits(sixmer, hexesSixmer, slotSize, offsetsSixmer, dnaStartOffset,
    	subCount, gapSize, missingQuad, lm, pHitList);
    }
}

static void searchExact12Vary6(struct splix *splix, int twelvemer, bits16 sixmer, int whichSixmer,
	int dnaStartOffset, int gapSize, int missingQuad, struct lm *lm,
	struct splatHit **pHitList)
/* Search for exact matches to twelvemer and off-by-ones to sixmer. */
{
bits16 oneOff = sixmer;
/* The toggles here are xor'd to quickly cycle us through all 4 binary values for a base. */
bits16 toggle1 = 1, toggle2 = 2;
int baseIx;
for (baseIx = 0; baseIx<6; ++baseIx)
    {
    oneOff ^= toggle1;
    searchExact(splix, twelvemer, oneOff, whichSixmer, dnaStartOffset,
    	1, gapSize, missingQuad, lm, pHitList);
    oneOff ^= toggle2;
    searchExact(splix, twelvemer, oneOff, whichSixmer, dnaStartOffset, 
    	1, gapSize, missingQuad, lm, pHitList);
    oneOff ^= toggle1;
    searchExact(splix, twelvemer, oneOff, whichSixmer, dnaStartOffset, 
    	1, gapSize, missingQuad, lm, pHitList);
    oneOff ^= toggle2;	/* Restore base to unmutated form. */
    toggle1 <<= 2;	/* Move on to next base. */
    toggle2 <<= 2;	/* Move on to next base. */
    }
}

static void searchVary12Exact6(struct splix *splix, int twelvemer, bits16 sixmer, int whichSixmer,
	int dnaStartOffset, int gapSize, int missingQuad, 
	struct lm *lm, struct splatHit **pHitList)
/* Search for exact matches to twelvemer and off-by-ones to sixmer. */
{
bits32 oneOff = twelvemer;
/* The toggles here are xor'd to quickly cycle us through all 4 binary values for a base. */
bits32 toggle1 = 1, toggle2 = 2;
int baseIx;
for (baseIx = 0; baseIx<12; ++baseIx)
    {
    oneOff ^= toggle1;
    searchExact(splix, oneOff, sixmer, whichSixmer, dnaStartOffset, 
    	1, gapSize, missingQuad, lm, pHitList);
    oneOff ^= toggle2;
    searchExact(splix, oneOff, sixmer, whichSixmer, dnaStartOffset, 
    	1, gapSize, missingQuad, lm, pHitList);
    oneOff ^= toggle1;
    searchExact(splix, oneOff, sixmer, whichSixmer, dnaStartOffset, 
    	1, gapSize, missingQuad, lm, pHitList);
    oneOff ^= toggle2;	/* Restore base to unmutated form. */
    toggle1 <<= 2;	/* Move on to next base. */
    toggle2 <<= 2;	/* Move on to next base. */
    }
}

struct alignContext
/* Context for an alignment. Necessary data for rbTree-traversing call-back function. 
 * Actually we no longer need this, but it does bundle together parameters shared
 * by a number of routines together. */
    {
    struct splix *splix;	/* Index. */
    struct splatTag **pTagList;	/* Pointer to tag list that gets filled in. */
    struct dnaSeq *qSeq;	/* DNA sequence. */
    char strand;		/* query strand. */
    int tagPosition;		/* Position of tag. */
    struct lm *lm;		/* Local memory pool where tags are allocated. */
    };

int countDnaDiffs(DNA *a, DNA *b, int size)
/* Count number of differing bases. */
{
int i;
int count = 0;
for (i=0; i<size; ++i)
   if (a[i] != b[i])
       ++count;
return count;
}

static void addMatch(int diffCount, struct alignContext *c, 
	int t1, int q1, int size1,
	int t2, int q2, int size2)
/* Output a match, which may be in two blocks. */
{
q1 += c->tagPosition;
q2 += c->tagPosition;
/* Normalize q2,t2 to make 'gap' zero size in single block case */
if (size2 <= 0)
    {
    size2 = 0;
    q2 = q1 + size1;
    t2 = t1 + size1;
    }

/* Calculate divergence - 2*bases_different + 2*(gap_count) + base_in_gaps. */
int qGap = q1 + size1 - q2;
int tGap = t1 + size1 - t2;
int gap = max(qGap, tGap);
int divergence = 2*diffCount + gap;
if (gap)
    divergence += 2;


/* Alloc and fill in tag. */
struct splatTag *tag;
lmAllocVar(c->lm, tag);
tag->divergence  = divergence;
tag->q1 = q1;
tag->t1 = t1;
tag->size1 = size1;
tag->q2 = q2;
tag->t2 = t2;
tag->size2 = size2;
tag->strand = c->strand;

/* Add to list. */
slAddHead(c->pTagList, tag);
}

static void slideInsert1(DNA *q, DNA *t, int gapSize, int solidSize, int mysterySize,
	int *retBestGapPos, int *retLeastDiffs)
/* Slide insert though all possible positions and return best position and number
 * of differences there. */
{
int gapPos = -1;
int diffs = countDnaDiffs(q, t, solidSize);
int bestGapPos = gapPos;
int leastDiffs = diffs;
for (gapPos = 0; gapPos < mysterySize; ++gapPos)
    {
    DNA qq = q[gapPos];
    DNA tt = t[gapPos];
    if (qq != tt)
       diffs -= 1;
    if (q[gapPos-gapSize] != tt)
       diffs += 1;
    if (diffs < leastDiffs)
	{
	leastDiffs = diffs;
	bestGapPos = gapPos;
	}
    }
*retLeastDiffs = leastDiffs;
*retBestGapPos = bestGapPos;
}

static void slideInsert2(DNA *q, DNA *t, int gapSize, int solidSize, int mysterySize,
	int *retBestGapPos, int *retLeastDiffs)
/* Slide insert though all possible positions and return best position and number
 * of differences there. Does this a _slightly_ different way than slideInsert1. 
 * Actually different in at least three ways, and to parameterize all three would
 * give it a lot of parameters.... */
{
int diffs = countDnaDiffs(q+gapSize, t, solidSize);
int bestGapPos = -1;
int leastDiffs = diffs;
int gapPos;
for (gapPos = 0; gapPos < mysterySize; ++gapPos)
    {
    DNA qq = q[gapPos];
    DNA tt = t[gapPos];
    if (qq != tt)
       diffs += 1;
    if (q[gapPos+gapSize] != tt)
       diffs -= 1;
    if (diffs < leastDiffs)
	{
	leastDiffs = diffs;
	bestGapPos = gapPos;
	}
    }
*retLeastDiffs = leastDiffs;
*retBestGapPos = bestGapPos;
}

static void slideDeletion2(DNA *q, DNA *t, int gapSize, int solidSize, int mysterySize,
	int *retBestGapPos, int *retLeastDiffs)
/* Slide deletion through all possible positions and return best position and number
 * of differences there. */
{
int diffs = countDnaDiffs(q, t+gapSize, solidSize);
int bestGapPos = -1;
int leastDiffs = diffs;
int gapPos;
for (gapPos = 0; gapPos < mysterySize; ++gapPos)
    {
    DNA qq = q[gapPos];
    DNA tt = t[gapPos];
    if (qq != tt)
       diffs += 1;
    if (qq != t[gapPos+gapSize])
       diffs -= 1;
    if (diffs < leastDiffs)
	{
	leastDiffs = diffs;
	bestGapPos = gapPos;
	}
    }
*retLeastDiffs = leastDiffs;
*retBestGapPos = bestGapPos;
}

static void extendMaybeOutputNoIndexIndels(struct splatHit *hit, struct alignContext *c)
/* Handle case where the 12-mer and the 6-mer in the index both hit without any
 * indication of an indel.  In the case where the missing quadrant is the first or
 * last though there could still be an indel in one of these quadrands. */
{
int tOffset = hit->tOffset;
int origDiffCount = hit->subCount;
int diffCount = origDiffCount;
int quad = hit->missingQuad;
struct dnaSeq *qSeq = c->qSeq;
DNA *qDna = qSeq->dna + c->tagPosition;
DNA *tDna = c->splix->allDna + tOffset;
switch (hit->missingQuad)
    {
    /* The ?'s in the comments below indicate areas that have not been explored in the index.
     * The _'s indicate areas that have been explored in the index. 
     * The singleton ? may actually represent 0 up to maxGap bases. */
    case 0: /* Explore ? ?????? ______ ______ ______ */
       diffCount += countDnaDiffs(qDna, tDna, 6+maxGap);
       break;
    case 1: /* Explore ? ______ ?????? ______ ______ */
       diffCount += countDnaDiffs(qDna, tDna, maxGap);
       diffCount += countDnaDiffs(qDna + 6 + maxGap, tDna + 6 + maxGap, 6);
       break;
    case 2: /* Explore ______ ______ ?????? ______ ? */
       diffCount += countDnaDiffs(qDna+12, tDna+12, 6);
       diffCount += countDnaDiffs(qDna+24, tDna+24, maxGap);
       break;
    case 3: /* Explore ______ ______ ______ ?????? ? */
       diffCount += countDnaDiffs(qDna+18, tDna+18, 6+maxGap);
       break;
    }
if (qSeq->size > 24 && maxGap == 0)	/* Make 25th base significant in all cases. */
    if (qDna[24] != tDna[24])
       ++diffCount;
if (diffCount <= maxMismatch)
    addMatch(diffCount, c, tOffset, 0, tagSize, 0, 0, 0);
else if (maxGap > 0 && origDiffCount < maxMismatch) 
    {
    /* Need to explore possibilities of indels if we are on the end quadrants */
    int gapSize = 1; /* May need more work here for indels bigger than a single base. */
    int bestGapPosInsert, leastDiffsInsert;
    int bestGapPosDelete, leastDiffsDelete;
    if (quad == 0)
        {
	/* Figure out insertion best score and position. */
	slideInsert1(qDna+gapSize, tDna+gapSize, gapSize, 6, 7, 
		&bestGapPosInsert, &leastDiffsInsert);

	/* Figure out deletion best score and position. */
	slideDeletion2(qDna, tDna-gapSize, gapSize, 7, 8, &bestGapPosDelete, &leastDiffsDelete);

	/* Output better scoring of insert or delete - delete on tie since it has one
	 * more matching base. */
	if (leastDiffsInsert < leastDiffsDelete)
	    {
	    if (leastDiffsInsert < maxMismatch)
		{
		int b1Size = bestGapPosInsert + 1;
		addMatch(leastDiffsInsert+origDiffCount, c, tOffset+gapSize, 0, b1Size, 
		    tOffset+gapSize+b1Size, b1Size+gapSize, tagSize - b1Size - gapSize);
		}
	    }
	else
	    {
	    if (leastDiffsDelete < maxMismatch)
		{
		int b1Size = bestGapPosDelete + 1;
		addMatch(leastDiffsDelete+origDiffCount, c, tOffset-gapSize, 0, b1Size, 
		    tOffset+b1Size, b1Size, tagSize - b1Size);
		}
	    }
	}
    else if (quad == 3)
        {
	/* Figure out insertion best score and position. */
	int startOffset = 18;
	slideInsert2(qDna+startOffset, tDna+startOffset, gapSize, 
		6, 7, &bestGapPosInsert, &leastDiffsInsert);


	/* Figure out deletion best score and position. */
	slideDeletion2(qDna+startOffset, tDna+startOffset, gapSize, 
		7, 8, &bestGapPosDelete, &leastDiffsDelete);

	/* Output better scoring of insert or delete - delete on tie since it has one
	 * more matching base. */
	if (leastDiffsInsert < leastDiffsDelete)
	    {
	    if (leastDiffsInsert < maxMismatch)
		{
		int b1Size = bestGapPosInsert + 1 + startOffset;
		addMatch(leastDiffsInsert+origDiffCount, c, tOffset, 0, b1Size, 
		    tOffset+b1Size, b1Size+gapSize, tagSize - b1Size - gapSize);
		}
	    }
	else
	    {
	    if (leastDiffsDelete < maxMismatch)
		{
		int b1Size = bestGapPosDelete + 1 + startOffset;
		addMatch(leastDiffsDelete+origDiffCount, c, tOffset, 0, b1Size, 
		    tOffset+b1Size+gapSize, b1Size, tagSize - b1Size);
		}
	    }
	}
    }
}

static void extendMaybeOutputInsert(struct splatHit *hit, struct alignContext *c)
/* Handle case where there is a base inserted in the query relative to the target. */
{
int tOffset = hit->tOffset;
int mysteryOffset = 0;    /* Offset of unexplored region in query. */
int mysterySize = 6;	/* May need to adjust when have >1 size gaps, not sure. */
int gapSize = hit->gapSize;
switch (hit->missingQuad)
    {
    case 1: /* Explore ______ ?????? ? ______ ______ */
        mysteryOffset = 6;
	break;
    case 2: /* Explore ______ ______ ? ?????? ------ */
	mysteryOffset = 12;
	break;
    default:
        internalErr();
        break;
    }
int bestGapPos, leastDiffs;
slideInsert2(c->qSeq->dna + c->tagPosition + mysteryOffset, 
	     c->splix->allDna + tOffset + mysteryOffset, 
	     gapSize, mysterySize, mysterySize, 
	     &bestGapPos, &leastDiffs);
leastDiffs += hit->subCount + 1;
if (leastDiffs <= maxMismatch)
    {
    int b1Size = bestGapPos + mysteryOffset + 1;
    addMatch(leastDiffs, c, tOffset, 0, b1Size, 
    	tOffset+b1Size, b1Size+gapSize, tagSize - b1Size - gapSize);
    }
}


static void extendMaybeOutputDeletion(struct splatHit *hit, struct alignContext *c)
/* Handle case where there is a base deleted in the query relative to the target. */
{
int tOffset = hit->tOffset;
int gapSize = -hit->gapSize;
int bigMysteryOffset = 0, qSmallMysteryOffset = 0, tSmallMysteryOffset = 0;
int bigMysterySize = 6, smallMysterySize = gapSize;
int diffCount = hit->subCount;

switch (hit->missingQuad)
    {
    case 1: 
    	/* Explore Q  ?? ______ ????? ______ ______ */
	/* Explore T  ?? ______ ?????? ______ ______ */
	bigMysteryOffset = 8;
	break;
    case 2: 
    	/* Explore Q ______ ______ ????? ------ ?? */
	/* Explore T ______ ______ ?????? ------ ?? */
	bigMysteryOffset = 12;
	qSmallMysteryOffset = 23;
	tSmallMysteryOffset = 24;
	break;
    default:
        internalErr();
        break;
    }
DNA *q = c->qSeq->dna + c->tagPosition;
DNA *t = c->splix->allDna + tOffset;
diffCount += countDnaDiffs(q + qSmallMysteryOffset, t + tSmallMysteryOffset, smallMysterySize);
if (diffCount < maxMismatch)
    {
    int bestGapPos, leastDiffs;
    slideDeletion2(q + bigMysteryOffset, t+bigMysteryOffset, 
		 gapSize, bigMysterySize-gapSize, bigMysterySize, 
		 &bestGapPos, &leastDiffs);
    leastDiffs += diffCount + 1;

    if (leastDiffs <= maxMismatch)
	{
	int b1Size = bestGapPos + bigMysteryOffset + 1;
	addMatch(leastDiffs, c, tOffset, 0, b1Size, 
	    tOffset+b1Size+gapSize, b1Size, tagSize - b1Size);
	}
    }
}

static void extendMaybeOutput(struct splatHit *hit, struct alignContext *c)
/* Callback function for tree traverser that extends alignments and if good, outputs them. */
{
verbose(3, "   tOffset=%d gapSize=%d subCount=%d missingQuad=%d\n", 
	hit->tOffset, hit->gapSize, hit->subCount, hit->missingQuad);
if (hit->gapSize == 0)
    {
    extendMaybeOutputNoIndexIndels(hit, c);
    }
else if (hit->gapSize > 0)
    {
    extendMaybeOutputInsert(hit, c);
    }
else
    {
    extendMaybeOutputDeletion(hit, c);
    }
}

void splatHitsOneStrand(struct dnaSeq *qSeq, bits64 bases25, 
	char strand, int tagPosition, struct splix *splix, int maxGap,
	struct lm *lm, struct splatHit **pHitList)
/* Look through index for hits to query tag on one strand. 
 * Input:
 *   qSeq - entire query sequence, reverse complimented if on - strand
 *   strand - either + or -
 *   tagPosition - start position within query to pass against index.
 *                 splixMinQuerySize + maxGap long.
 *   splix - the index to search
 *   maxGap - the maximum insertion or deletion size
 * A list of splat hits allocated on lm. */
{
/* Convert DNA to binary format */
int firstHalf, secondHalf;
bits16 after1, after2, before1, before2;
bits16 firstHex, lastHex, twoAfterFirstHex, twoBeforeLastHex;

dnaToBinary(qSeq->dna + tagPosition, maxGap,
	&firstHalf, &secondHalf, &after1, &after2, &before1, &before2,
	&firstHex, &lastHex, &twoAfterFirstHex, &twoBeforeLastHex);

int secondHalfPos = -12 - maxGap;
searchExact(splix, firstHalf, after1, 2, 0, 0, 0, 3, lm, pHitList);
if (!exactOnly)
    {
    searchExact(splix, firstHalf, after2, 3, 0, 0, 0, 2, lm, pHitList);
    searchExact(splix, secondHalf, before1, 0, secondHalfPos, 0, 0, 1, lm, pHitList);
    searchExact(splix, secondHalf, before2, 1, secondHalfPos, 0, 0, 0, lm, pHitList);
    }
if (maxMismatch > 1)
    {
    searchVary12Exact6(splix, firstHalf, after1, 2, 0, 0, 3, lm, pHitList);
    searchVary12Exact6(splix, firstHalf, after2, 3, 0, 0, 2, lm, pHitList);
    searchVary12Exact6(splix, secondHalf, before1, 0, secondHalfPos, 0, 1, lm, pHitList);
    searchVary12Exact6(splix, secondHalf, before2, 1, secondHalfPos, 0, 0, lm, pHitList);
    searchExact12Vary6(splix, firstHalf, after1, 2, 0, 0, 3, lm, pHitList);
    searchExact12Vary6(splix, firstHalf, after2, 3, 0, 0, 2, lm, pHitList);
    searchExact12Vary6(splix, secondHalf, before1, 0, secondHalfPos, 0, 1, lm, pHitList);
    searchExact12Vary6(splix, secondHalf, before2, 1, secondHalfPos, 0, 0, lm, pHitList);
    }

/* Look at single base indels with few mismatches */
if (maxGap > 0)
    {
    searchExact(splix, secondHalf, twoAfterFirstHex, 0, secondHalfPos-maxGap, 0, -1, 1, lm, pHitList);
    searchExact(splix, secondHalf, firstHex, 0, secondHalfPos+maxGap, 0, 1, 1, lm, pHitList);
    searchExact(splix, firstHalf, twoBeforeLastHex, 3, 0, 0, -1, 2, lm, pHitList);
    searchExact(splix, firstHalf, lastHex, 3, 0, 0, 1, 2, lm, pHitList);
    if (maxMismatch > 1)
	{
	searchExact12Vary6(splix, secondHalf, twoAfterFirstHex, 0, secondHalfPos-maxGap, -1, 1, lm, pHitList);
	searchExact12Vary6(splix, secondHalf, firstHex, 0, secondHalfPos+maxGap, 1, 1, lm, pHitList);
	searchExact12Vary6(splix, firstHalf, twoBeforeLastHex, 3, 0, -1, 2, lm, pHitList);
	searchExact12Vary6(splix, firstHalf, lastHex, 3, 0, 1, 2, lm, pHitList);
	searchVary12Exact6(splix, secondHalf, twoAfterFirstHex, 0, secondHalfPos-maxGap, -1, 1, lm, pHitList);
	searchVary12Exact6(splix, secondHalf, firstHex, 0, secondHalfPos+maxGap, 1, 1, lm, pHitList);
	searchVary12Exact6(splix, firstHalf, twoBeforeLastHex, 3, 0, -1, 2, lm, pHitList);
	searchVary12Exact6(splix, firstHalf, lastHex, 3, 0, 1, 2, lm, pHitList);
	}
    }
}

void extendHitsToTags(struct splatHit *hitList, struct dnaSeq *qSeq, char strand, 
	int tagPosition, struct splix *splix, struct lm *lm, struct splatTag **pTagList)
/* Examine each of the hits in the hitTree, and add ones that are high scoring enough
 * after extension to the tagList. If bestOnly is set it may free existing tags on list if
 * it finds a higher scoring tag. */
{
struct splatHit *hit;
struct alignContext ac;
ZeroVar(&ac);
ac.splix = splix;
ac.pTagList = pTagList;
ac.qSeq = qSeq;
ac.strand = strand;
ac.tagPosition = tagPosition;
ac.lm = lm;
for (hit = hitList; hit != NULL; hit = hit->next)
    extendMaybeOutput(hit, &ac);
}

static struct splatTag *removeDupeTags(struct splatTag *tagList)
/* Return list with duplicate tags removed.  As a side effect sort list. */
{
struct splatTag *tag, *next, *newList = NULL;

slSort(&tagList, splatTagCmp);
for (tag = tagList; tag != NULL; tag = next)
    {
    next = tag->next;
    if (next == NULL || splatTagCmp(&tag, &next) != 0)
	slAddHead(&newList, tag);
    }
slReverse(&newList);
return newList;
}

static int findLeastDivergence(struct splatTag *tagList)
{
struct splatTag *tag;
int leastDivergence = tagList->divergence;
for (tag = tagList->next; tag != NULL; tag = tag->next)
    {
    if (tag->divergence < leastDivergence)
	leastDivergence = tag->divergence;
    }
return leastDivergence;
}

static struct splatTag *splatTagFilterOnDivergence(struct splatTag *tagList, int maxDivergence)
/* Remove (and free) tags with more than maxDivergence from list. */
{
struct splatTag *tag, *next, *newList = NULL;
for (tag = tagList; tag != NULL; tag = next)
    {
    next = tag->next;
    if (tag->divergence <= maxDivergence)
        slAddHead(&newList, tag);
    }
slReverse(&newList);
return newList;
}

static void splatOneStrand(struct dnaSeq *qSeq, bits64 bases25, char strand,
	int tagPosition, struct splix *splix, int maxGap, 
	struct lm *lm, struct splatTag **pTagList)
/* Align one query strand against index, filter, and write out results */
{
struct splatHit *hitList = NULL;
splatHitsOneStrand(qSeq, bases25, strand, tagPosition, splix, maxGap, lm, &hitList);
struct splatTag *tagList = NULL;
int hitCount = slCount(hitList);
verbose(2, " %d hits on %c strand\n", hitCount, strand);
if (hitCount > 0)
    {
    extendHitsToTags(hitList, qSeq, strand, tagPosition, splix, lm, &tagList);

    /* Since this is faster than removing dupes, and will often trim list a lot, do
     * it now, even though we have to do it again when we have data on both strands. */
    if (tagList != NULL && !worseToo)
	tagList = splatTagFilterOnDivergence(tagList, findLeastDivergence(tagList) );

    /* Some duplicate tags may have come through.  This also will filter the tags 
     * by chromosome position. */
    tagList = removeDupeTags(tagList);
    *pTagList = slCat(tagList, *pTagList);
    }
}

static void splatOne(struct dnaSeq *qSeqF, struct splix *splix, int maxGap, 
	struct axtScoreScheme *scoreScheme, FILE *f, int *retMapCount, boolean *retIsRepeat)
/* Align one query sequence against index, filter, and write out results.
 * Returns the number of mappings. */
{
/* Local memory pool for hits and tags for this sequence. */
struct lm *lm = lmInit(1024*4);

/* Make reverse complement of query too. */
struct dnaSeq *qSeqR = cloneDnaSeq(qSeqF);
reverseComplement(qSeqR->dna, qSeqR->size);

struct splatTag *tagList = NULL;
int size = qSeqF->size;
int desiredSize = tagSize;
int outputCount = 0;
boolean isRepeat = FALSE;
if (size < desiredSize)
    {
    warn("%s is %d bases, minimum splat query size %d, skipping", 
    	qSeqF->name, tagSize, qSeqF->size);
    return;
    }
bits64 bases25f = basesToBits64(qSeqF->dna, 25);
boolean isRepeatingOver = overCheck(bases25f, overArraySize, overArray);
if (!isRepeatingOver)
    {
    splatOneStrand(qSeqF, bases25f, '+', 0, splix, maxGap, lm, &tagList);
    int tagPosition = qSeqR->size - desiredSize;
    bits64 bases25r = basesToBits64(qSeqR->dna + tagPosition, 25);
    isRepeatingOver = overCheck(bases25r, overArraySize, overArray);
    if (!isRepeatingOver)
	splatOneStrand(qSeqR, bases25r, '-', tagPosition, splix, maxGap, lm, &tagList);
    }
if (isRepeatingOver)
    {
    if (repeatOutputFile != NULL)
	faWriteNext(repeatOutputFile, qSeqF->name, qSeqF->dna, qSeqF->size);
    isRepeat = TRUE;
    }
else if (tagList != NULL)
    {
    /* Find least divergence. */

    /* Unless doing worseToo remove tags that are not best scoring */
    if (!worseToo)
	tagList = splatTagFilterOnDivergence(tagList, findLeastDivergence(tagList) );
    

    /* Count up mappings, and output either to repeat file or to mapping file. */
    outputCount = slCount(tagList);
    hits25 += outputCount;
    isRepeat = (outputCount > maxRepeat);
    if (isRepeat)
        {
	if (repeatOutputFile != NULL)
	    faWriteNext(repeatOutputFile, qSeqF->name, qSeqF->dna, qSeqF->size);
	outputCount = 0;
	}
    else
        {
	struct splatAlign *aliList = splatExtendTags(tagList, qSeqF, qSeqR, splix, scoreScheme);
	hitsFull += slCount(aliList);
	slSort(&aliList, splatAlignCmpScore);
	splatOutList(aliList, out, qSeqF, qSeqR, splix, f);
	splatAlignFreeList(&aliList);
	}
#ifdef SOON
#endif /* SOON */
    }
dnaSeqFree(&qSeqR);
lmCleanup(&lm);
*retMapCount = outputCount;
*retIsRepeat = isRepeat;
}


void splat(char *target, char *query, char *output)
/* splat - Speedy Local Alignment Tool. */
{
struct axtScoreScheme *scoreScheme = axtScoreSchemeSimpleDna(2, 2, 2, 2);
struct dnaLoad *qLoad = dnaLoadOpen(query);
if (over != NULL)
    {
    overRead(over, maxRepeat+1, &overArraySize, &overArray);
    }
struct splix *splix = splixRead(target, memoryMap);
uglyTime("Loaded %s", target);
FILE *f = mustOpen(output, "w");
if (repeatOutput != NULL)
    repeatOutputFile = mustOpen(repeatOutput, "w");
splatOutHeader(target, query, out, f);
struct dnaSeq *qSeq;
int uniqCount = 0, totalMap = 0, totalRepeat = 0, totalReads = 0;
while ((qSeq = dnaLoadNext(qLoad)) != NULL)
    {
    verbose(2, "Processing %s\n", qSeq->name);
    toUpperN(qSeq->dna, qSeq->size);
    int mapCount = 0;
    boolean isRepeat = FALSE;
    splatOne(qSeq, splix, maxGap, scoreScheme, f, &mapCount, &isRepeat);
    verbose(2, " %d mappings\n", mapCount);
    if (mapCount > 0)
        {
	++totalMap;
	if (mapCount == 1)
	    ++uniqCount;
	}
    if (isRepeat)
        ++totalRepeat;
    dnaSeqFree(&qSeq);
    ++totalReads;
    }
uglyTime("Alignment");

/* Report statistics (to stderr) */
verbose(1, "Overall results for mapping %d reads in %s to %s\n", 
	totalReads, query, target);
int missCount = totalReads - totalMap - totalRepeat;
verbose(1, "%d (%5.2f%%) did not map\n", missCount, 100.0 * missCount/totalReads);
verbose(1, "%d (%5.2f%%) mapped more than %d places\n", 
	totalRepeat, 100.0 * totalRepeat / totalReads, maxRepeat);
if (maxRepeat >= 2)
    {
    int multiCount = totalMap - uniqCount;
    verbose(1,  "%d (%5.2f%%) mapped between 2 and %d places\n",
	multiCount, 100.0 * multiCount / totalReads, maxRepeat);
    }
verbose(1, "%d (%5.2f%%) mapped uniquely\n",
    uniqCount, 100.0 * uniqCount / totalReads);

verbose(1, "%ld index queries, %ld hits12, %ld hits18, %ld hits25, %ld hitsFull\n", 
	exactIndexQueries, hits12, hits18, hits25, hitsFull);

/* Clean up. */
splixFree(&splix);
dnaLoadClose(&qLoad);
carefulClose(&f);
carefulClose(&repeatOutputFile);
}

int main(int argc, char *argv[])
/* Process command line. */
{
uglyTime(NULL);
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
over = optionVal("over", over);
out = optionVal("out", out);
maxDivergence = optionInt("maxDivergence", maxDivergence);
worseToo = optionExists("worseToo");
maxRepeat = optionInt("maxRepeat", maxRepeat);
repeatOutput = optionVal("repeatOutput", NULL);
maxGap = optionInt("maxGap", maxGap);
maxMismatch = optionInt("maxMismatch", maxMismatch);
exactOnly = (maxGap == 0 && maxMismatch == 0);
uglyf("Exact only = %d\n", exactOnly);
tagSize = maxGap + splixMinQuerySize;
memoryMap = optionExists("mmap");
if (maxGap > 1)
    errAbort("Sorry, for now maxGap must be just 0 or 1");
dnaUtilOpen();
splat(argv[1], argv[2], argv[3]);
return 0;
}
