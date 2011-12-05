/* crudeali.c - Files for doing fast blast-style crude alignment. 
 * This has two modes - a 16-base at a time and a 6 base at a time
 * which basically is for cDNA alignments and genomic/genomic 
 * alignments. The six base at a time doesn't use six contigious
 * bases, but rather 8 bases in a row with the two in "wobble
 * positions" masked out.  It ends up making things more
 * sensitive, and, fortunately, the whole thing still fits in
 * a single 16 bit word. */
/* Copyright 2000-2003 Jim Kent.  All rights reserved. */

#include "common.h"
#include "dnautil.h"
#include "nt4.h"
#include "dnaseq.h"
#include "crudeali.h"


#define maxTileSize 16

#define wobbleMask 0xF3CF

static int caTileSize = 16;
static boolean caIsCdna = TRUE;

struct crudeHit
/* A tileSize exact match between probe and target. */
    {
    int probeOffset;
    int targetOffset;
    boolean isLumped;
    };


struct crudeExon
/* A bunch of hits that hang together on a diagonal. */
    {
    int probeStart;
    int probeEnd;
    int targetStart;
    int targetEnd;
    int hitCount;
    boolean isLumped;
    };

struct crudeGene
/* A collection of diagonals that seem to hang together
 * like exons. */
    {
    struct nt4Seq *target;
    boolean isRc;
    int probeStart;
    int probeEnd;
    int targetStart;
    int targetEnd;
    int score;
    };

static int lumpHitsIntoExons(struct crudeHit *hits, int hitCount, struct crudeExon *exons)
/* Lump together hits that are within 48 bp of each other and within 2 of
 * the same diagonal into "exons".  The exons array must be hitCount elements
 * in order to accomodate the worst case where no lumping occurs.  */
{
int i;
struct crudeExon *exon = exons;
int exonCount = 0;
int firstNonLumped = 0;

for (i=0; i<hitCount; ++i)
    hits[i].isLumped = FALSE;

for (;;)
    {
    boolean startedExon = FALSE;
    int diagDiff = 0;
    int tOff, pOff;
    int thisDiff;
    for (; firstNonLumped < hitCount; ++firstNonLumped)
        {
        if (!hits[firstNonLumped].isLumped)
            break;
        }
    for (i=firstNonLumped; i<hitCount; ++i)
        {
        if (hits[i].isLumped == FALSE)
            {
            pOff = hits[i].probeOffset;
            tOff = hits[i].targetOffset;
            thisDiff = pOff - tOff;
            if (!startedExon)   /* First hit in exon. */
                {
                startedExon = TRUE;
                hits[i].isLumped = TRUE;
                exon->probeStart = pOff;
                exon->probeEnd = pOff + caTileSize;
                exon->targetStart = tOff;
                exon->targetEnd = tOff + caTileSize;
                exon->hitCount = 1;
                diagDiff = thisDiff;
                }
            else if (diagDiff - 2 <= thisDiff && thisDiff <= diagDiff + 2
                    && exon->probeEnd - 2 <= pOff && pOff <= exon->probeEnd + 48
                    && exon->targetEnd - 2 <= tOff && tOff <= exon->targetEnd + 48)
                {
                hits[i].isLumped = TRUE;
                exon->probeEnd = pOff + caTileSize;
                exon->targetEnd = tOff + caTileSize;
                exon->hitCount += 1;
                }
            else if (tOff > exon->targetEnd + 48)
                break;
            }
        }
    if (!startedExon)   /* Must be all lumped together. */
        break;
    ++exonCount;
    ++exon;
    }

/* If small tile size then get rid of exons with only one hit. */
if (caTileSize == 8)
    {
    struct crudeExon *read = exons, *write = exons;
    int twoFerCount = 0;
    for (i=0; i<exonCount; ++i)
        {
        if (read->hitCount >= 2)
            {
            *write++ = *read++;
            ++twoFerCount;
            }
        else
            ++read;
        }
    exonCount = twoFerCount;
    }
return exonCount;
}

static int lumpExonsIntoGenes(struct crudeExon *exons, int exonCount, 
    struct crudeGene *genes, struct nt4Seq *target, boolean isRc)
/* Lump together crude exons into crude genes, and score them. */
{
int i;
struct crudeGene *gene = genes;
int geneCount = 0;
int firstNonLumped = 0;
int targetGapSpan = (caIsCdna ? 25000 : 200);
int probeGapSpan = (caIsCdna ? 64 : 200);

for (i=0; i<exonCount; ++i)
    exons[i].isLumped = FALSE;

for (;;)
    {
    boolean startedGene = FALSE;
    int lastDiff = 0;
    int tOff, pOff;
    int thisDiff;
    int exonsInThis = 0;

    for ( ; firstNonLumped < exonCount; ++firstNonLumped)
        {
        if (!exons[firstNonLumped].isLumped)
            break;
        }
    for (i=firstNonLumped; i<exonCount; ++i)
        {
        if (!exons[i].isLumped)
            {
            tOff = exons[i].targetStart;
            pOff = exons[i].probeStart;
            thisDiff = tOff - pOff;
            if (!startedGene)
                {
                startedGene = TRUE;
                exons[i].isLumped = TRUE;
                lastDiff = thisDiff;
                exonsInThis = 1;
                gene->target = target;
                gene->isRc = isRc;
                gene->probeStart = exons[i].probeStart;
                gene->probeEnd = exons[i].probeEnd;
                gene->targetStart = exons[i].targetStart;
                gene->targetEnd = exons[i].targetEnd;
                gene->score = exons[i].hitCount * exons[i].hitCount;
                }
            else if (gene->targetEnd - 10 <= tOff && tOff <= gene->targetEnd + targetGapSpan
                     && gene->probeEnd - 10 <= pOff && pOff <= gene->probeEnd + probeGapSpan
                     && lastDiff - 2 <= thisDiff)
                {
                exons[i].isLumped = TRUE;
                gene->targetEnd = exons[i].targetEnd;
                gene->probeEnd = exons[i].probeEnd;
                gene->score += exons[i].hitCount * exons[i].hitCount;
                exonsInThis += 1;
                lastDiff = thisDiff;
                }
            else if (tOff > gene->targetEnd + targetGapSpan)
                break;
            }
        }
    if (!startedGene)
        break;
    gene->score += exonsInThis;
    ++geneCount;
    ++gene;
    }
return geneCount;
}

static int copyExonsIntoGenes(struct crudeExon *exons, int exonCount, 
    struct crudeGene *genes, struct nt4Seq *target, boolean isRc)
/* Make crude genes that are just one per exon.  What a kludge! */
{
int i;
for (i=0; i<exonCount; ++i)
    {
    genes[i].target = target;
    genes[i].isRc = isRc;
    genes[i].probeStart = exons[i].probeStart;
    genes[i].probeEnd = exons[i].probeEnd;
    genes[i].targetStart = exons[i].targetStart;
    genes[i].targetEnd = exons[i].targetEnd;
    genes[i].score = exons[i].hitCount * exons[i].hitCount;
    }
return exonCount;
}

static boolean makeGoodTile(DNA *dna, bits32 *retTile16, bits16 *retTile8, int tileSize)
/* Turn next base pairs into a tile.  Return FALSE
 * if it wouldn't be a good tile. */
{
int i;
int repMod;
int endRep;
int maxRep = (tileSize / 2);

/* Make sure no N's in tile */
for (i=0; i<tileSize; ++i)
    {
    if (ntVal[(int)dna[i]] < 0)
        return FALSE;
    }

/* Make sure that tile isn't part of a pattern. */ 
for (repMod = 1; repMod <= maxRep; repMod += 1)
    {
    boolean allSame = TRUE;
    endRep = tileSize - repMod;
    for (i=0; i<endRep; ++i)
        {
        if (dna[i] != dna[i+repMod])
            {
            allSame = FALSE;
            break;
            }
        }
    if (allSame)
        return FALSE;
    }

if (tileSize > 8)
    *retTile16 = packDna16(dna);
else
    *retTile8 = packDna8(dna);

return TRUE;
}

struct probeTile
/* A tile from the probe and the offset where it came from. */
    {
    struct probeTile *next;
    int offset;
    bits32 tile16;
    bits16 tile8;
    };

#define tileHashBits 16
#define tileHashSize (1<<tileHashBits)
#define tileHashMask (tileHashSize-1)
#define tileHashFunc(tile) ((tile)&tileHashMask)

static struct probeTile **makeTileHash()
{
struct probeTile **table;
int tableSize = tileHashSize * sizeof(table[0]);

table = needLargeMem(tableSize);
memset(table, 0, tableSize);
return table;
}


struct fastProber
/* Structure that has hash list of probeSeq tiles for fast searching */
    {
    struct probeTile **hash;
    struct probeTile *probes;
    int probeSize;
    };

static struct fastProber *makeFastProber(DNA *probeDna, int probeSize)
{
int maxProbeCount = probeSize - caTileSize + 1;
int probeCount = 0;
struct probeTile *probes;
struct probeTile *probe;
struct probeTile **hash;
int i;
struct fastProber *fp;

if (maxProbeCount <= 0)
    return NULL;
AllocVar(fp);
fp->hash = hash = makeTileHash();
fp->probeSize = probeSize;
fp->probes = probes = needMem(maxProbeCount * sizeof(probes[0]) );
for (i=0; i<maxProbeCount; ++i)
    {
    probe = &probes[probeCount];
    if (makeGoodTile(probeDna+i, &probe->tile16, &probe->tile8, caTileSize))
        {
        int hashIx; 
        probe->tile8 &= wobbleMask;
        hashIx = (caTileSize > 8 ? tileHashFunc(probe->tile16) : tileHashFunc(probe->tile8));
        probe->offset = i;
        probe->next = hash[hashIx];
        hash[hashIx] = probe;
        ++probeCount;
        }
    }
return fp;
}

static void freeFastProber(struct fastProber **pFp)
{
struct fastProber *fp = *pFp;
if (fp != NULL)
    {
    freeMem(fp->probes);
    freeMem(fp->hash);
    freez(pFp);
    }
}

static int makeIndividualHits16(struct probeTile **hash, bits32 *bases, int baseOffset, int baseWordCount,
    struct crudeHit *hits, int maxHitCount, int hitCount)
/* Scan a short segment for individual hits. */
{
struct probeTile *probe;
int i;
for (i=0; i<baseWordCount; ++i)
    {
    if ((probe = hash[tileHashFunc(bases[i])]) != NULL)
        {
        do
            {
            if (bases[i] == probe->tile16)
                {
                if (hitCount >= maxHitCount)
                    {
                    warn("Too many hits, only taking first %d", maxHitCount);
                    goto END_HIT_LOOP;
                    }
                hits[hitCount].probeOffset = probe->offset;
                hits[hitCount].targetOffset = ((i+baseOffset) * caTileSize);
                ++hitCount;
                break;
                }
            probe = probe->next;
            }
        while (probe != NULL);
        }
    }
END_HIT_LOOP:
return hitCount;
}

static int makeHits16(struct fastProber *fp, struct nt4Seq *target, struct crudeHit *hits,
    int maxHitCount)
/* Scan entire target for hits to probe. */
{
bits32 *bases = target->bases;
struct probeTile **hash = fp->hash;
unsigned long acc;
int hitCount = 0;
int i;
int baseWordCount = (target->baseCount/caTileSize);
bits32 *endBases = bases + baseWordCount;
int chunkSize = 8;
int chunkCount = baseWordCount/chunkSize;
int baseOffset = 0;

for (i=0; i<chunkCount; ++i)
    {
    acc = ((unsigned long)(hash[tileHashFunc(bases[0])])
        |  (unsigned long)(hash[tileHashFunc(bases[1])])
        |  (unsigned long)(hash[tileHashFunc(bases[2])])
        |  (unsigned long)(hash[tileHashFunc(bases[3])])
        |  (unsigned long)(hash[tileHashFunc(bases[4])])
        |  (unsigned long)(hash[tileHashFunc(bases[5])])
        |  (unsigned long)(hash[tileHashFunc(bases[6])])
        |  (unsigned long)(hash[tileHashFunc(bases[7])]));
    if (acc)
        {
        hitCount = makeIndividualHits16(hash, bases, baseOffset, chunkSize, hits, maxHitCount, hitCount);
        if (hitCount >= maxHitCount)
            break;
        }
    bases += chunkSize;
    baseOffset += chunkSize;
    }
if (bases != endBases)
    hitCount = makeIndividualHits16(hash, bases, baseOffset, endBases-bases, hits, maxHitCount, hitCount);
return hitCount;
}

void squeezeHits8(struct fastProber *fp, 
    short *compactDiagBuf, int compactDiagBufByteSize, 
    int startToff, int lastCompactOff, int lastCompareOff,
    struct crudeHit *hits, int lastCompactedHit, int hitCount,
    int *retLastCompactedHit, int *retHitCount)
/* Get rid of isolated hits by dumping ones where there is only one
 * hit on their diagonal. */
{
int diagOffset = fp->probeSize+2;
int diag;
int i;
struct crudeHit *hit, *writeHit;
int compactDiagBufArraySize = compactDiagBufByteSize/sizeof(short);

memset(compactDiagBuf, 0, compactDiagBufByteSize);
for (i=lastCompactedHit; i<hitCount; ++i)
    {
    hit = hits+i;
    diag = (hit->targetOffset - startToff) - hit->probeOffset + diagOffset;
    assert(diag >= 0 && diag < compactDiagBufArraySize);
    compactDiagBuf[diag] += 1;
    }
writeHit = hits+lastCompactedHit;
for (i=lastCompactedHit; i<hitCount; ++i)
    {
    hit = hits+i;
    if (hit->targetOffset >= lastCompactOff)
        break;
    diag = (hit->targetOffset - startToff) - hit->probeOffset + diagOffset;
    if (compactDiagBuf[diag] > 1)
        *writeHit++ = *hit;
    }    
*retLastCompactedHit = (writeHit - hits);
for (;i<hitCount; ++i)
    {
    hit = hits+i;
    *writeHit++ = *hit;
    }
*retHitCount = (writeHit - hits);
}


static int makeHits8(struct fastProber *fp, struct nt4Seq *target, struct crudeHit *hits,
    int maxHitCount)
/* Scan entire target for hits to probe. */
{
bits16 *bases = (bits16*)target->bases;
struct probeTile **hash = fp->hash;
struct probeTile *pbt;
int hitCount = 0;
int i;
int baseWordCount = (target->baseCount/caTileSize);
int tOffset = 0;

/* Handle big/little endian problem. */
bits32 endianTest = 0x12345678;
bits16 *endianPt = (bits16*)(void*)(&endianTest);
boolean needSwap = (*endianPt == 0x5678);
int swapOffset[2];
int swapIx = 0;

/* Every so often we throw out singleton hits. The variables below
 * manage this. */
int compactWindowSizePower = 10;
int compactWindowSize = (1<<compactWindowSizePower);
int compactModMask = compactWindowSize-1;
int compactOverlap = 50;
int lastCompactedHit = 0;
int compactBufIntSize = (compactWindowSize+compactOverlap+fp->probeSize+4);
int compactBufSize = (compactBufIntSize*sizeof(short));
short *compactDiagBuf = needLargeMem(compactBufSize);

if (needSwap)
    {
    swapOffset[0] = 8;
    swapOffset[1] = -8;
    }
else
    {
    swapOffset[0] = swapOffset[1] = 0;
    }

assert(tileHashBits == (8*sizeof(bits16)));
for (i=0; i<baseWordCount; ++i)
    {
    if ((pbt = hash[*bases++ & wobbleMask]) != NULL)
        {
        if (hitCount < maxHitCount)
            {
            hits[hitCount].probeOffset = pbt->offset;
            hits[hitCount].targetOffset = tOffset + swapOffset[swapIx];
            ++hitCount;
            }
        else
            {
            warn("Got too many hits, only keeping first %d\n", hitCount);
            break;
            }
        }
    tOffset += caTileSize;
    if ((tOffset&compactModMask) == 0)
        {
        int startToff = tOffset - compactWindowSize - compactOverlap;
        int lastCompactOff = startToff + compactWindowSize;
        int lastCompareOff = tOffset;

        squeezeHits8(fp, compactDiagBuf, compactBufSize, 
            startToff,  lastCompactOff, lastCompareOff, 
            hits, lastCompactedHit, hitCount, &lastCompactedHit, &hitCount);
        }
    swapIx = 1 - swapIx;  /* Toggle between 0 and 1. */
    }
freeMem(compactDiagBuf);
return hitCount;
}

static int cmpHitsTargetFirst(const void *va, const void *vb)
/* Compare function to sort hit array by ascending
 * target offset followed by ascending probe offset. */
{
struct crudeHit *a = (struct crudeHit *)va;
struct crudeHit *b = (struct crudeHit *)vb;
long diff;
if ((diff = a->targetOffset - b->targetOffset) != 0)
    return diff;
return a->probeOffset - b->probeOffset;
}

static int cmpExonsTargetFirst(const void *va, const void *vb)
/* Compare function to sort exons by ascending target
 * offset followed by ascending probe offset. */
{
struct crudeExon *a = (struct crudeExon *)va;
struct crudeExon *b = (struct crudeExon *)vb;
long diff;
if ((diff = a->targetStart - b->targetStart) != 0)
    return diff;
return a->probeStart - b->probeStart;
}


#define maxHitsAtOnce 120000
/* Maximum number of hits to allow on a single scan.
 * If we get more than this need to toss out a bad tile
 * or something... */

static int makeHits(struct fastProber *fp, struct nt4Seq *target, struct crudeHit *hits,
    int maxHitCount)
/* Scan entire target for hits to probe. */
{
if (caTileSize == 8)
    return makeHits8(fp, target, hits, maxHitCount);
else if (caTileSize == 16)
    return makeHits16(fp, target, hits, maxHitCount);
else
    {
    errAbort("Can only do tile sizes of 8 or 16 in makeHits");
    return 0;
    }
}

static int findCrudeGenes(struct fastProber *fp, struct nt4Seq *target,
    struct crudeGene *genes, int maxGeneCount, boolean isRc)
/* Scan target with probe, and arrange hits into crude genes. */
{
struct crudeHit *hits;
struct crudeExon *exons;
int hitCount, exonCount, geneCount = 0;

if (fp == NULL)
    return 0;
assert(maxGeneCount >= maxHitsAtOnce);
hits = needMem(maxHitsAtOnce*sizeof(*hits));
exons = needMem(maxHitsAtOnce*sizeof(*exons));
hitCount = makeHits(fp, target, hits, maxHitsAtOnce);

if (hitCount > 0)
    {
    qsort(hits, hitCount, sizeof(hits[0]), cmpHitsTargetFirst);
    exonCount = lumpHitsIntoExons(hits, hitCount, exons);
    qsort(exons, exonCount, sizeof(exons[0]), cmpExonsTargetFirst);
    if (caIsCdna)
        geneCount = lumpExonsIntoGenes(exons, exonCount, genes, target, isRc);
    else
        geneCount = copyExonsIntoGenes(exons, exonCount, genes, target, isRc);
    }
freeMem(hits);
freeMem(exons);
return geneCount;
}

static int cmpGenesByScore(const void *va, const void *vb)
/* cmp function to sort leaving highest scores first. */
{
struct crudeGene *a = (struct crudeGene *)va;
struct crudeGene *b = (struct crudeGene *)vb;
return b->score - a->score;
}

static int countGenesBetter(struct crudeGene *genes, int geneCount, int cutoff)
/* Count number of genes (in sorted list) with scores better than cutoff */
{
int i;
for (i=0; i<geneCount; ++i)
    {
    if (genes[i].score <= cutoff)
        break;
    }
return i;
}

static int filterPoorGenes(struct crudeGene *genes, int geneCount)
/* Sort gene list by score and remove bottom scorers. 
 * This is gauranteed to get rid of half of genes on list or 
 * more. */
{
int bestScore, worstScore;
int newGeneCount;
int halfGeneCount = geneCount/2;

qsort(genes, geneCount, sizeof(genes[0]), cmpGenesByScore);
bestScore = genes[0].score;
worstScore = genes[geneCount-1].score;

/* If scores are clustered with half or more at the top,
 * we have to be crude and just lop off bottom half of scores. */
if (bestScore == genes[halfGeneCount].score)
    {
    warn("Bunches of genes, all scoring the same. Program is "
         "throwing out half out of necessity.\n");
    return geneCount/2;
    }

/* Drop bottom score until have gotten rid of at least half. */
newGeneCount = geneCount;
while (newGeneCount > halfGeneCount)
    {
    worstScore = genes[newGeneCount-1].score;
    newGeneCount = countGenesBetter(genes, newGeneCount, worstScore);
    }
return newGeneCount;
}

static int chromIx(struct nt4Seq *one, struct nt4Seq **chrome, int chromeCount)
{
int i;
for (i=0; i<chromeCount; ++i)
    {
    if (one == chrome[i])
        return i;
    }
assert(FALSE);
return -1;
}

static int countPrettyGood(struct crudeGene *crudeGenes, int geneCount, int fraction, int minScore)
/* Count up # of genes that are not in bottom fraction. (For fraction 4, count
 * all but bottom quarter. */
{
int count = 0;
int i;
int threshold = crudeGenes->score;  /* Top score */

threshold = (threshold + fraction/2)/fraction;
if (threshold < minScore)
    threshold = minScore;
for (i=0; i<geneCount; ++i)
    {
    if (crudeGenes[i].score >= threshold)
        ++count;
    else
        break;
    }
return count;
}

static int wormDnaMatchScores[256][256];

void initWormDnaMatchScores()
/* Initialize our big sloppy pairwise comparison table. */
{
int i,j;
static boolean initted = FALSE;

if (initted) return;
initted = TRUE;
for (i=0; i<256; ++i)
    for (j=0; j<256; ++j)
        wormDnaMatchScores[i][j] = -4;
wormDnaMatchScores['a']['a'] = 2;
wormDnaMatchScores['t']['t'] = 2;
wormDnaMatchScores['A']['A'] = 2;
wormDnaMatchScores['T']['T'] = 2;
wormDnaMatchScores['c']['c'] = 3;
wormDnaMatchScores['g']['g'] = 3;
wormDnaMatchScores['C']['C'] = 3;
wormDnaMatchScores['G']['G'] = 3;
}

#ifdef OLD
void scoreNoninsertingExtensions(struct crudeGene *crudeGene, DNA *probe, int probeSize)
/* Fetch target DNA the size of probe and see how good of a score we
 * can come up with. */
{
int i;
int pStart = crudeGene->probeStart;
int size = crudeGene->probeEnd - pStart;
int score = 0;
DNA *target = nt4Unpack(crudeGene->target, crudeGene->targetStart, size);
DNA p,t;
probe += pStart;
for (i=0; i < size; ++i)
    {
    p = probe[i];
    t = target[i];
    score += wormDnaMatchScores[p][t];
    }
freeMem(target);
}
#endif 

void scoreNoninsertingExtensions(struct crudeGene *crudeGene, DNA *probe, int probeSize)
/* Old fetch target DNA the size of probe and see how good of a score we
 * can come up with. */
{
int i, size;
int score = 0, maxScore = 0;
struct nt4Seq *targetChrom = crudeGene->target;
DNA *target;
DNA p,t;

/* Figure out what DNA to fetch - trying to get all of target that
 * corresponds to all of probe, but clipping if at ends of chromosome. */
int pTileStart = crudeGene->probeStart;
int tTileStart = crudeGene->targetStart;
int pStart = 0, pEnd = probeSize;
int tStart = tTileStart - pTileStart;
int tEnd = tStart + probeSize;
if (tStart < 0)
    {
    pStart -= tStart;
    tStart = 0;
    }
if (tEnd > targetChrom->baseCount)
    {
    int diff = tEnd - targetChrom->baseCount;
    tEnd -= diff;
    pEnd -= diff;
    }
size = tEnd - tStart;
if (crudeGene->isRc)
    reverseComplement(probe, probeSize);
if (size > 0)
    {
    target = nt4Unpack(targetChrom, tStart, size);
    for (i = 0; i<size; ++i)
        {
        assert(i + pStart < probeSize);
        p = probe[i + pStart];
        t = target[i];
        score += wormDnaMatchScores[(int)p][(int)t];
        if (score < 0)
            score = 0;
        if (score > maxScore)
            maxScore = score;
        }            
    freeMem(target);
    }
crudeGene->score = maxScore;
if (crudeGene->isRc)
    reverseComplement(probe, probeSize);
}


struct crudeAli *crudeAliFind(DNA *probe, int probeSize, 
    struct nt4Seq **chrome, int chromeCount, int tileSize, int minScore)
/* Returns a list of crude alignments.  (You can free this with slFreeList() */
{
int oneGeneCount;
int i;
int chromeIx;
int geneCount = 0;
int maxGeneCount = 2*maxHitsAtOnce;
int isRc;
struct nt4Seq *target;
struct crudeGene *crudeGenes = needMem(maxGeneCount*sizeof(*crudeGenes));
struct fastProber *fps[2];
struct crudeAli *aliList = NULL, *ali;
int goodGeneCount;

initWormDnaMatchScores();
caTileSize = tileSize;
caIsCdna = (tileSize >= 14);
fps[0] = makeFastProber(probe, probeSize);
reverseComplement(probe, probeSize);
fps[1] = makeFastProber(probe, probeSize);
reverseComplement(probe, probeSize);
for (chromeIx = 0; chromeIx < chromeCount; ++chromeIx)
    {
    target = chrome[chromeIx];
    for (isRc = 0; isRc <= 1; ++isRc)
        {
        /* Get crude idea of genes on one strand. */
        oneGeneCount = findCrudeGenes(fps[isRc], target, 
            crudeGenes+geneCount, maxGeneCount-geneCount, isRc);
            
        /* Increment gene count and if necessary compact list. */
        geneCount += oneGeneCount;
        if (maxGeneCount - geneCount < maxHitsAtOnce)
            geneCount = filterPoorGenes(crudeGenes, geneCount);
        }
    if (maxGeneCount - geneCount < maxHitsAtOnce)
        break;
    }

/* Sort genes by initial promise and get rid of some trash. */
    {
    int bestScore, worstScore;
    qsort(crudeGenes, geneCount, sizeof(crudeGenes[0]), cmpGenesByScore);
    bestScore = crudeGenes[0].score;
    worstScore = crudeGenes[geneCount-1].score;
    if (bestScore > worstScore)
        {
        int cutOff = bestScore/10;
        goodGeneCount = geneCount = countGenesBetter(crudeGenes, geneCount, cutOff);
        }
    else if (bestScore >= minScore)
        goodGeneCount = geneCount;
    else
        goodGeneCount = 0;
    }

if (tileSize == 8)
    {
    goodGeneCount = countPrettyGood(crudeGenes, geneCount, 4, 4);
    geneCount = goodGeneCount;
    for (i=0; i<geneCount; ++i)
        scoreNoninsertingExtensions(crudeGenes+i, probe, probeSize);
    qsort(crudeGenes, geneCount, sizeof(crudeGenes[0]), cmpGenesByScore);
    goodGeneCount = countPrettyGood(crudeGenes, geneCount, 4, minScore);
    }
for (i=0; i<goodGeneCount; ++i)
    {
    AllocVar(ali);
    ali->chromIx = chromIx(crudeGenes[i].target, chrome, chromeCount);
    ali->start = crudeGenes[i].targetStart;
    ali->end = crudeGenes[i].targetEnd;
    ali->score = crudeGenes[i].score;
    ali->strand = (crudeGenes[i].isRc ? '-' : '+');
    slAddHead(&aliList, ali);
    }
slReverse(&aliList);
freeFastProber(&fps[0]);
freeFastProber(&fps[1]);
freeMem(crudeGenes);

return aliList;
}

