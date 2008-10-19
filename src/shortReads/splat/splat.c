/* splat - Speedy Local Alignment Tool. */
/* This file is copyright 2008 Jim Kent.  All rights reserved. */

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

static char const rcsid[] = "$Id: splat.c,v 1.3 2008/10/19 02:53:18 kent Exp $";

char *out = "psl";
int minScore = 22;
boolean worseToo = FALSE;
int maxGap = 1;
int maxMismatch = 2;
boolean memoryMap = FALSE;
int tagSize;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "NOTE: splat is in early stages of development. No real output yet, just some diagnostic\n"
  "messages to stdout.\n"
  "splat - SPeedy Local Alignment Tool. Designed to align small reads to large\n"
  "DNA databases such as genomes quickly.  Reads need to be at least 25 bases.\n"
  "usage:\n"
  "   splat target query output\n"
  "where:\n"
  "   target is a large indexed dna database in splix format.  Use splixMake to create this\n"
  "          file from fasta, 2bit or nib format\n"
  "   query is a fasta or fastq file full of sequences of at least 25 bases each\n"
  "   output is the output alignment file, by default in .psl format\n"
  "note: can use 'stdin' or 'stdout' as a file name for better piping\n"
  "overall options:\n"
  "   -out=format. Output format.  Options include psl, pslx, axt, maf, bed4, bedMap\n"
  "   -minScore=N. Minimum alignment score for output. Default 22. Score is by default\n"
  "                match - mismatch - 2*gapOpen - gapExtend\n"
  "   -worseToo - if set return alignments other than the best alignments\n"
  "options effecting just first 25 bases\n"
  "   -maxGap=N. Maximum gap size. Default is %d. Set to 0 for no gaps\n"
  "   -maxMismatch=N. Maximum number of mismatches. Default %d. (A gap counts as a mismatch.)\n"
  "   -mmap - Use memory mapping. Faster just a few reads, but much slower for many reads\n"
  , maxGap, maxMismatch
  );
}

static struct optionSpec options[] = {
   {"out", OPTION_STRING},
   {"minScore", OPTION_FLOAT},
   {"worseToo", OPTION_BOOLEAN},
   {"maxGap", OPTION_INT},
   {"maxMismatch", OPTION_INT},
   {"mmap", OPTION_BOOLEAN},
   {NULL, 0},
};

struct splatHit
/* Information on an index hit.  This only has about a 1% chance of being real after
 * extension, but hey, it's a start. */
    {
    int tOffset;	/* Offset to DNA in target */
    int gapSize;	/* >0 for insert in query/del in target, <0 for reverse. 0 for no gap. */
    int subCount;	/* Count of substitutions we know about already. */
    int missingQuad;	/* Which data quadrant is lacking (not found in index). */
    };

int splatHitCmp(void *va, void *vb)
/* Return -1 if a before b,  0 if a and b overlap,
 * and 1 if a after b. */
{
struct splatHit *a = va;
struct splatHit *b = vb;
int diff = a->tOffset - b->tOffset;
if (diff == 0)
    diff = a->gapSize - b->gapSize;
return diff;
}

struct splatTag
/* Roughtly 25-base match that passes maxGap and maxMismatch criteria. */
    {
    struct splatTag *next;	 /* Next in list. */
    int score;  /* Match - mismatch - 2*inserts for now. */
    int q1,t1;  /* Position of first block in query and target. */
    int size1;	/* Size of first block. */
    int q2,t2;  /* Position of second block if any. */
    int size2;  /* Size of second block.  May be zero. */
    char strand; /* Query strand. */
    };

void dnaToBinary(DNA *dna, int maxGap, int *pFirstHalf, int *pSecondHalf,
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

int searchHexes(bits16 query, bits16 *hexes, int hexCount)
/* See if query sequence is in hexes, which is sorted. */
{
/* TODO: replace this with a binary search. */
int i;
for (i=0; i<hexCount; ++i)
    if (hexes[i] == query)
        return i;
return -1;
}

void exactIndexHits(bits16 hex, bits16 *sortedHexes, int slotSize, 
	bits32 *offsets, struct rbTree *hitTree, int startOffset,
	int subCount, int gapSize, int missingQuad)
/* Look for hits that involve no index mismatches.  Put resultint offsets (plus startOffset)
 * into hitTree. */
{
int ix = searchHexes(hex, sortedHexes, slotSize);
if (ix >= 0)
    {
    int dnaOffset = offsets[ix] + startOffset;
    struct splatHit h;
    h.tOffset = dnaOffset;
    h.gapSize = gapSize;
    struct splatHit *hit = rbTreeFind(hitTree, &h);
    if (hit == NULL)
	{
	lmAllocVar(hitTree->lm, hit);
	hit->tOffset = dnaOffset;
	hit->gapSize = gapSize;
	hit->missingQuad = missingQuad;
	hit->subCount = subCount;
        rbTreeAdd(hitTree, hit);
	}
    else if (subCount < hit->subCount)	/* Got a better hit, update tree leaf! */
        {
	hit->missingQuad = missingQuad;
	hit->subCount = subCount;
	}
    }
}

void searchExact(struct splix *splix, int twelvemer, bits16 sixmer, int whichSixmer,
	int dnaStartOffset, struct rbTree *hitTree,
	int subCount, int gapSize, int missingQuad)
/* If there's an exact match put DNA offset of match into hitTree. */
{
int slotSize = splix->slotSizes[twelvemer];
if (slotSize != 0)
    {
    /* Compute actual positions of the subindexes for the 1/4 reads in the slot. */
    char *slot = splix->slots[twelvemer];
    bits16 *hexesSixmer = (bits16*)(slot + whichSixmer*sizeof(bits16)*slotSize);
    bits32 *offsetsSixmer = (bits32*)((8+sizeof(bits32)*whichSixmer)*slotSize + slot);
    exactIndexHits(sixmer, hexesSixmer, slotSize, offsetsSixmer, hitTree, dnaStartOffset,
    	subCount, gapSize, missingQuad);
    }
}

void searchExact12Vary6(struct splix *splix, int twelvemer, bits16 sixmer, int whichSixmer,
	int dnaStartOffset, struct rbTree *hitTree, int gapSize, int missingQuad)
/* Search for exact matches to twelvemer and off-by-ones to sixmer. */
{
bits16 oneOff = sixmer;
/* The toggles here are xor'd to quickly cycle us through all 4 binary values for a base. */
bits16 toggle1 = 1, toggle2 = 2;
int baseIx;
for (baseIx = 0; baseIx<6; ++baseIx)
    {
    oneOff ^= toggle1;
    searchExact(splix, twelvemer, oneOff, whichSixmer, dnaStartOffset, hitTree,
    	1, gapSize, missingQuad);
    oneOff ^= toggle2;
    searchExact(splix, twelvemer, oneOff, whichSixmer, dnaStartOffset, hitTree,
    	1, gapSize, missingQuad);
    oneOff ^= toggle1;
    searchExact(splix, twelvemer, oneOff, whichSixmer, dnaStartOffset, hitTree,
    	1, gapSize, missingQuad);
    oneOff ^= toggle2;	/* Restore base to unmutated form. */
    toggle1 <<= 2;	/* Move on to next base. */
    toggle2 <<= 2;	/* Move on to next base. */
    }
}

void searchVary12Exact6(struct splix *splix, int twelvemer, bits16 sixmer, int whichSixmer,
	int dnaStartOffset, struct rbTree *hitTree, int gapSize, int missingQuad)
/* Search for exact matches to twelvemer and off-by-ones to sixmer. */
{
bits32 oneOff = twelvemer;
/* The toggles here are xor'd to quickly cycle us through all 4 binary values for a base. */
bits32 toggle1 = 1, toggle2 = 2;
int baseIx;
for (baseIx = 0; baseIx<12; ++baseIx)
    {
    oneOff ^= toggle1;
    searchExact(splix, oneOff, sixmer, whichSixmer, dnaStartOffset, hitTree,
    	1, gapSize, missingQuad);
    oneOff ^= toggle2;
    searchExact(splix, oneOff, sixmer, whichSixmer, dnaStartOffset, hitTree,
    	1, gapSize, missingQuad);
    oneOff ^= toggle1;
    searchExact(splix, oneOff, sixmer, whichSixmer, dnaStartOffset, hitTree,
    	1, gapSize, missingQuad);
    oneOff ^= toggle2;	/* Restore base to unmutated form. */
    toggle1 <<= 2;	/* Move on to next base. */
    toggle2 <<= 2;	/* Move on to next base. */
    }
}

struct alignContext
/* Context for an alignment. Necessary data for rbTree-traversing call-back function. */
    {
    struct splix *splix;	/* Index. */
    struct splatTag **pTagList;	/* Pointer to tag list that gets filled in. */
    struct dnaSeq *qSeq;	/* DNA sequence. */
    char strand;		/* query strand. */
    int tagPosition;		/* Position of tag. */
    };

int countDiff(DNA *a, DNA *b, int size)
/* Count number of differing bases. */
{
int i;
int count = 0;
for (i=0; i<size; ++i)
   if (a[i] != b[i])
       ++count;
return count;
}

void addMatch(int subCount, struct alignContext *c, 
	int t1, int q1, int size1,
	int t2, int q2, int size2)
/* Output a match, which may be in two blocks. */
{
int score = size1 + size2 - subCount;	// TODO: insert penalty???
					// TODO: check against high score on list
struct splatTag *tag;
AllocVar(tag);
tag->score  = score;
tag->q1 = q1 + c->tagPosition;
tag->t1 = t1;
tag->size1 = size1;
tag->q2 = q2 + c->tagPosition;
tag->t2 = t2;
tag->size2 = size2;
tag->strand = c->strand;
slAddHead(c->pTagList, tag);
}

void slideInsert1(DNA *q, DNA *t, int gapSize, int solidSize, int mysterySize,
	int *retBestGapPos, int *retLeastDiffs)
/* Slide insert though all possible positions and return best position and number
 * of differences there. */
{
int gapPos = -1;
int diffs = countDiff(q, t, solidSize);
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

void slideInsert2(DNA *q, DNA *t, int gapSize, int solidSize, int mysterySize,
	int *retBestGapPos, int *retLeastDiffs)
/* Slide insert though all possible positions and return best position and number
 * of differences there. Does this a _slightly_ different way than slideInsert1. 
 * Actually different in at least three ways, and to parameterize all three would
 * give it a lot of parameters.... */
{
int diffs = countDiff(q+gapSize, t, solidSize);
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

void slideDeletion2(DNA *q, DNA *t, int gapSize, int solidSize, int mysterySize,
	int *retBestGapPos, int *retLeastDiffs)
/* Slide deletion through all possible positions and return best position and number
 * of differences there. */
{
int diffs = countDiff(q, t+gapSize, solidSize);
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

void extendMaybeOutputNoIndexIndels(struct splatHit *hit, struct alignContext *c)
/* Handle case where the 12-mer and the 6-mer in the index both hit without any
 * indication of an indel.  In the case where the missing quadrant is the first or
 * last though there could still be an indel in one of these quadrands. */
{
int tOffset = hit->tOffset;
int diffCount = hit->subCount;
int quad = hit->missingQuad;
DNA *qDna = c->qSeq->dna + c->tagPosition;
DNA *tDna = c->splix->allDna + tOffset;
switch (hit->missingQuad)
    {
    /* The ?'s in the comments below indicate areas that have not been explored in the index.
     * The _'s indicate areas that have been explored in the index. 
     * The singleton ? may actually represent 0 up to maxGap bases. */
    case 0: /* Explore ? ?????? ______ ______ ______ */
       diffCount += countDiff(qDna, tDna, 6+maxGap);
       break;
    case 1: /* Explore ? ______ ?????? ______ ______ */
       diffCount += countDiff(qDna, tDna, maxGap);
       diffCount += countDiff(qDna + 6 + maxGap, tDna + 6 + maxGap, 6);
       break;
    case 2: /* Explore ______ ______ ?????? ______ ? */
       diffCount += countDiff(qDna+12, tDna+12, 6);
       diffCount += countDiff(qDna+24, tDna+24, maxGap);
       break;
    case 3: /* Explore ______ ______ ______ ?????? ? */
       diffCount += countDiff(qDna+18, tDna+18, 6+maxGap);
       break;
    }
if (diffCount <= maxMismatch)
    addMatch(diffCount, c, tOffset, 0, tagSize, 0, 0, 0);
else if (maxGap > 0 && hit->subCount < maxMismatch) 
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
	leastDiffsInsert += hit->subCount+1;

	/* Figure out deletion best score and position. */
	slideDeletion2(qDna, tDna-gapSize, gapSize, 7, 8, &bestGapPosDelete, &leastDiffsDelete);
	leastDiffsDelete += hit->subCount+1;

	/* Output better scoring of insert or delete - delete on tie since it has one
	 * more matching base. */
	if (leastDiffsInsert < leastDiffsDelete)
	    {
	    if (leastDiffsInsert <= maxMismatch)
		{
		int b1Size = bestGapPosInsert + 1;
		addMatch(leastDiffsInsert, c, tOffset+gapSize, 0, b1Size, 
		    tOffset+gapSize+b1Size, b1Size+gapSize, tagSize - b1Size - gapSize);
		}
	    }
	else
	    {
	    if (leastDiffsDelete <= maxMismatch)
		{
		int b1Size = bestGapPosDelete + 1;
		addMatch(leastDiffsDelete, c, tOffset-gapSize, 0, b1Size, 
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
	leastDiffsInsert += hit->subCount+1;


	/* Figure out deletion best score and position. */
	slideDeletion2(qDna+startOffset, tDna+startOffset, gapSize, 
		7, 8, &bestGapPosDelete, &leastDiffsDelete);
	leastDiffsDelete += hit->subCount+1;

	/* Output better scoring of insert or delete - delete on tie since it has one
	 * more matching base. */
	if (leastDiffsInsert < leastDiffsDelete)
	    {
	    if (leastDiffsInsert <= maxMismatch)
		{
		int b1Size = bestGapPosInsert + 1 + startOffset;
		addMatch(leastDiffsInsert, c, tOffset, 0, b1Size, 
		    tOffset+b1Size, b1Size+gapSize, tagSize - b1Size - gapSize);
		}
	    }
	else
	    {
	    if (leastDiffsDelete <= maxMismatch)
		{
		int b1Size = bestGapPosDelete + 1 + startOffset;
		addMatch(leastDiffsDelete, c, tOffset, 0, b1Size, 
		    tOffset+b1Size+gapSize, b1Size, tagSize - b1Size);
		}
	    }
	}
    }
}

void extendMaybeOutputInsert(struct splatHit *hit, struct alignContext *c)
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


void extendMaybeOutputDeletion(struct splatHit *hit, struct alignContext *c)
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
diffCount += countDiff(q + qSmallMysteryOffset, t + tSmallMysteryOffset, smallMysterySize);
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

void extendMaybeOutput(void *item, void *context)
/* Callback function for tree traverser that extends alignments and if good, outputs them. */
{
/* Rescue some typed variables out of container-driven voidness. */
struct splatHit *hit = item;
struct alignContext *c = context;

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

struct rbTree *splatHitsOneStrand(struct dnaSeq *qSeq, char strand,
	int tagPosition, struct splix *splix, int maxGap)
/* Look through index for hits to query tag on one strand. 
 * Input:
 *   qSeq - entire query sequence, reverse complimented if on - strand
 *   strand - either + or -
 *   tagPosition - start position within query to pass against index.
 *                 splixMinQuerySize + maxGap long.
 *   splix - the index to search
 *   maxGap - the maximum insertion or deletion size
 * Output: a binary tree filled with splatHits.  The tree owns the hits themselves,
 *   so you can dispose of it simply with rbTreeFree(). */
{
struct rbTree *hitTree = rbTreeNew(splatHitCmp);

/* Convert DNA to binary format */
int firstHalf, secondHalf;
bits16 after1, after2, before1, before2;
bits16 firstHex, lastHex, twoAfterFirstHex, twoBeforeLastHex;
dnaToBinary(qSeq->dna + tagPosition, maxGap,
	&firstHalf, &secondHalf, &after1, &after2, &before1, &before2,
	&firstHex, &lastHex, &twoAfterFirstHex, &twoBeforeLastHex);

int secondHalfPos = -12 - maxGap;
searchExact(splix, firstHalf, after1, 2, 0, hitTree, 0, 0, 3);
searchExact(splix, firstHalf, after2, 3, 0, hitTree, 0, 0, 2);
searchExact(splix, secondHalf, before1, 0, secondHalfPos, hitTree, 0, 0, 1);
searchExact(splix, secondHalf, before2, 1, secondHalfPos, hitTree, 0, 0, 0);
if (maxMismatch > 1)
    {
    searchVary12Exact6(splix, firstHalf, after1, 2, 0, hitTree, 0, 3);
    searchVary12Exact6(splix, firstHalf, after2, 3, 0, hitTree, 0, 2);
    searchVary12Exact6(splix, secondHalf, before1, 0, secondHalfPos, hitTree, 0, 1);
    searchVary12Exact6(splix, secondHalf, before2, 1, secondHalfPos, hitTree, 0, 0);
    searchExact12Vary6(splix, firstHalf, after1, 2, 0, hitTree, 0, 3);
    searchExact12Vary6(splix, firstHalf, after2, 3, 0, hitTree, 0, 2);
    searchExact12Vary6(splix, secondHalf, before1, 0, secondHalfPos, hitTree, 0, 1);
    searchExact12Vary6(splix, secondHalf, before2, 1, secondHalfPos, hitTree, 0, 0);
    }

/* Look at single base indels with few mismatches */
if (maxGap > 0)
    {
    searchExact(splix, secondHalf, twoAfterFirstHex, 0, secondHalfPos-maxGap, hitTree, 0, -1, 1);
    searchExact(splix, secondHalf, firstHex, 0, secondHalfPos+maxGap, hitTree, 0, 1, 1);
    searchExact(splix, firstHalf, twoBeforeLastHex, 3, 0, hitTree, 0, -1, 2);
    searchExact(splix, firstHalf, lastHex, 3, 0, hitTree, 0, 1, 2);
    if (maxMismatch > 1)
	{
	searchExact12Vary6(splix, secondHalf, twoAfterFirstHex, 0, secondHalfPos, hitTree, -1, 1);
	searchExact12Vary6(splix, secondHalf, firstHex, 0, secondHalfPos+maxGap, hitTree, 1, 1);
	searchExact12Vary6(splix, firstHalf, twoBeforeLastHex, 3, 0, hitTree, -1, 2);
	searchExact12Vary6(splix, firstHalf, lastHex, 3, 0, hitTree, 1, 2);
	searchVary12Exact6(splix, secondHalf, twoAfterFirstHex, 0, secondHalfPos, hitTree, -1, 1);
	searchVary12Exact6(splix, secondHalf, firstHex, 0, secondHalfPos+maxGap, hitTree, 1, 1);
	searchVary12Exact6(splix, firstHalf, twoBeforeLastHex, 3, 0, hitTree, -1, 2);
	searchVary12Exact6(splix, firstHalf, lastHex, 3, 0, hitTree, 1, 2);
	}
    }
return hitTree;
}

void extendHitsToTags(struct rbTree *hitTree, struct dnaSeq *qSeq, char strand, 
	int tagPosition, struct splix *splix, boolean bestOnly, struct splatTag **pTagList)
/* Examine each of the hits in the hitTree, and add ones that are high scoring enough
 * after extension to the tagList. If bestOnly is set it may free existing tags on list if
 * it finds a higher scoring tag. */
{
struct alignContext ac;
ZeroVar(&ac);
ac.splix = splix;
ac.pTagList = pTagList;
ac.qSeq = qSeq;
ac.strand = strand;
ac.tagPosition = tagPosition;
rbTreeTraverseWithContext(hitTree, extendMaybeOutput, &ac);
}

	

void splatOneStrand(struct dnaSeq *qSeq, char strand,
	int tagPosition, struct splix *splix, int maxGap, struct splatTag **pTagList)
/* Align one query strand against index, filter, and write out results */
{
struct rbTree *hitTree = splatHitsOneStrand(qSeq, strand, tagPosition, splix, maxGap);
if (hitTree->n > 0)
    {
    extendHitsToTags(hitTree, qSeq, strand, tagPosition, splix, !worseToo, pTagList);
    rbTreeFree(&hitTree);
    }
rbTreeFree(&hitTree);
}

void splatTagOutput(struct splatTag *tag, struct dnaSeq *qSeq, struct splix *splix, FILE *f)
/* Output tag match */
{
char strand = tag->strand;
if (strand == '-')
    reverseComplement(qSeq->dna, qSeq->size);
fprintf(f, "%s\t", qSeq->name);
int size1 = tag->size1;
int size2 = tag->size2;
int q1 = tag->q1, q2 = tag->q2;
int t1 = tag->t1, t2 = tag->t2;
DNA *qDna = qSeq->dna + q1;
DNA *tDna = splix->allDna + t1;
// uglyf("tag %s %c,  q1=%d, t1=%d, size1=%d, q2=%d, t2=%d, size2=%d\n", qSeq->name, strand, q1, t1, size1, q2, t2, size2);
int i;
for (i=0; i<size1; ++i)
    fputc(qDna[i], f);
int qGapSize = q2 - (q1+size1);
int tGapSize = t2 - (t1+size1);
if (size2)
    {
    for (i=0; i<qGapSize; ++i)
        fputc(qDna[size1+i], f);
    for (i=0; i<tGapSize; ++i)
        fputc('-', f);
    qDna = qSeq->dna + q2;
    for (i=0; i<size2; ++i)
	fputc(qDna[i], f);
    }
fprintf(f, "\t%d\t%c\n", q1, tag->strand);
fprintf(f, "%s\t", "splix");
for (i=0; i<size1; ++i)
    fputc(tDna[i], f);
if (size2)
    {
    for (i=0; i<qGapSize; ++i)
        fputc('-', f);
    for (i=0; i<tGapSize; ++i)
        fputc(tDna[size1+i], f);
    tDna = splix->allDna + t2;
    for (i=0; i<size2; ++i)
	fputc(tDna[i], f);
    }
fprintf(f, "\t%d\t%c\n", t1, '+');
fprintf(f, "\n");
if (strand == '-')
    reverseComplement(qSeq->dna, qSeq->size);
}

void splatOne(struct dnaSeq *qSeq, struct splix *splix, int maxGap, FILE *f)
/* Align one query sequence against index, filter, and write out results */
{
struct splatTag *tagList = NULL, *tag;
int size = qSeq->size;
int desiredSize = tagSize;
if (size < desiredSize)
    {
    warn("%s is %d bases, minimum splat query size %d, skipping", 
    	qSeq->name, tagSize, qSeq->size);
    return;
    }
verbose(2, "%s\n", qSeq->name);
splatOneStrand(qSeq, '+', 0, splix, maxGap, &tagList);
reverseComplement(qSeq->dna, qSeq->size);
int tagPosition = qSeq->size - desiredSize;
splatOneStrand(qSeq, '-', tagPosition, splix, maxGap, &tagList);
reverseComplement(qSeq->dna, qSeq->size);
slReverse(&tagList);
for (tag = tagList; tag != NULL; tag = tag->next)
    splatTagOutput(tag, qSeq, splix, f);
}

void splat(char *target, char *query, char *output)
/* splat - Speedy Local Alignment Tool. */
{
struct splix *splix = splixRead(target, memoryMap);
struct dnaLoad *qLoad = dnaLoadOpen(query);
FILE *f = mustOpen(output, "w");
struct dnaSeq *qSeq;
while ((qSeq = dnaLoadNext(qLoad)) != NULL)
    {
    verbose(2, "Processing %s\n", qSeq->name);
    toUpperN(qSeq->dna, qSeq->size);
    splatOne(qSeq, splix, maxGap, f);
    dnaSeqFree(&qSeq);
    }
splixFree(&splix);
dnaLoadClose(&qLoad);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
minScore = optionInt("minScore", minScore);
worseToo = optionExists("worseToo");
maxGap = optionInt("maxGap", maxGap);
maxMismatch = optionInt("maxMismatch", maxMismatch);
tagSize = maxGap + splixMinQuerySize;
memoryMap = optionExists("mmap");
if (maxGap > 1)
    errAbort("Sorry, for now maxGap must be just 0 or 1");
dnaUtilOpen();
splat(argv[1], argv[2], argv[3]);
return 0;
}
