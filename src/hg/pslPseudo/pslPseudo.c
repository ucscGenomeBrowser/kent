/* pslPseudo - analyse repeats and generate list of processed pseudogenes
 * from a genome wide, sorted by mRNA .psl alignment file.
 */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "memalloc.h"
//#include "jksql.h"
//#include "hdb.h"
#include "psl.h"
#include "bed.h"
#include "dnaseq.h"
#include "nib.h"
#include "rmskOut.h"
#include "binRange.h"
#include "cheapcgi.h"
#include "genePred.h"
#include "genePredReader.h"

#define WORDCOUNT 3
#define POLYASLIDINGWINDOW 10
#define POLYAREGION 160

/* label for classification stored in pseudoGeneLink table */
#define PSEUDO 1
#define NOTPSEUDO -1

static char const rcsid[] = "$Id: pslPseudo.c,v 1.10 2004/03/09 01:58:28 baertsch Exp $";

char *db;
double minAli = 0.98;
double maxRep = 0.35;
double minAliPseudo = 0.60;
double nearTop = 0.01;
double minCover = 0.90;
double minCoverPseudo = 0.01;
int maxBlockGap = 60;
boolean ignoreSize = FALSE;
boolean noIntrons = FALSE;
boolean singleHit = FALSE;
boolean noHead = FALSE;
boolean quiet = FALSE;
int minNearTopSize = 10;
struct genePred *gpList1 = NULL, *gpList2 = NULL, *kgList = NULL;
FILE *bestFile, *pseudoFile, *linkFile;
void usage()
/* Print usage instructions and exit. */
{
errAbort(
    "pslPseudo - analyse repeats and generate genome wide best\n"
    "alignments from a sorted set of local alignments\n"
    "usage:\n"
    "    pslPseudo db in.psl sizes.lst rmskDir trf.bed syntenic.bed mrna.psl out.psl pseudo.psl pseudoLink.txt nibDir refGene.tab gene.tab kglist.tab \n\n"
    "where in.psl is an blat alignment of mrnas sorted by pslSort\n"
    "blastz.psl is an blastz alignment of mrnas sorted by pslSort\n"
    "sizes.lst is a list of chrromosome followed by size\n"
    "rmskDir is the directory containing repeat masker output file\n"
    "trf.bed is the simple repeat (trf) bed file\n"
    "mrna.psl is the blat best mrna alignments\n"
    "out.psl is the best mrna alignment for the gene \n"
    "and pseudo.psl contains pseudogenes\n"
    "pseudoLink.txt will have the link between gene and pseudogene\n"
    "options:\n"
    "    -nohead don't add PSL header\n"
    "    -ignoreSize Will not weigh in favor of larger alignments so much\n"
    "    -noIntrons Will not penalize for not having introns when calculating\n"
    "              size factor\n"
    "    -minCover=0.N minimum coverage for mrna to output.  Default is 0.90\n"
    "    -minCoverPseudo=0.N minimum coverage of pseudogene to output.  Default is 0.01\n"
    "    -minAli=0.N minimum alignment ratio for mrna\n"
    "               default is 0.98\n"
    "    -minAliPseudo=0.N minimum alignment ratio for pseudogenes\n"
    "               default is 0.60\n"
    "    -nearTop=0.N how much can deviate from top and be taken\n"
    "               default is 0.01\n"
    "    -minNearTopSize=N  Minimum size of alignment that is near top\n"
    "               for aligmnent to be kept.  Default 10.\n"
    "    -maxBlockGap=N  Max gap size between adjacent blocks that are combined. \n"
    "               Default 60.\n"
    "    -maxRep=N  max ratio of overlap with repeat masker track \n"
    "               for aligmnent to be kept.  Default .35\n");
}

int calcMilliScore(struct psl *psl)
/* Figure out percentage score. */
{
return 1000-pslCalcMilliBad(psl, TRUE);
}

void outputNoLink(struct psl *psl, char *type, char *bestqName, char *besttName, 
                int besttStart, int besttEnd, int maxExons, int geneOverlap, 
                char *bestStrand, int polyA, int polyAstart, int label, 
                int exonCover, int intronCount, int bestAliCount, 
                int rep, int qReps, int overlapDiagonal) 
/* output bed record with pseudogene details */
{
struct bed *bed = bedFromPsl(psl);
int milliBad = calcMilliScore(psl);
int coverage = ((psl->match+psl->repMatch)*100)/psl->qSize;

fprintf(linkFile,"\n");
bed->score = milliBad - (100-log(polyA)*20) - (overlapDiagonal*2) - (100-coverage) + log(psl->match+psl->repMatch)*100 ;
bedOutputN( bed , 6, linkFile, ' ', ' ');
fprintf(linkFile,"%s %s %s %s %d %d %s %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d ",
        /* 8     9     10      11       12          13 */
        db, type, bestqName, besttName, besttStart, besttEnd, 
        (bestStrand[0] == '+' || bestStrand[0] == '-') ? bestStrand : "0", 
        /* 14           15      16 */
        maxExons, geneOverlap, polyA, 
        /* polyAstart 17*/
        (psl->strand[0] == '+') ? polyAstart - psl->tEnd : psl->tStart - (polyAstart + polyA) , 
        /* 18           19              20      21 */
        exonCover, intronCount, bestAliCount, psl->match+psl->repMatch, 
        /* 22           23                24    25      26 */
        psl->qSize, psl->qSize-psl->qEnd, rep, qReps, overlapDiagonal, 
        /* 27      28      29     30*/
        coverage, label, milliBad,0);
bedFree(&bed);
}

void outputLink(struct psl *psl, char *type, char *bestqName, char *besttName, 
                int besttStart, int besttEnd, int maxExons, int geneOverlap, 
                char *bestStrand, int polyA, int polyAstart, int label, 
                int exonCover, int intronCount, int bestAliCount, 
                int tReps, int qReps, int overlapDiagonal, 
                struct genePred *kg, struct genePred *mgc, struct genePred *gp) 
/* output bed record with pseudogene details and link to gene*/
{
outputNoLink(psl, type, bestqName, besttName, besttStart, besttEnd, 
            maxExons, geneOverlap, bestStrand, polyA, polyAstart, label,
            exonCover, intronCount, bestAliCount, tReps, qReps, overlapDiagonal); 
if (gp != NULL)
    fprintf(linkFile,"%s %d %d ", gp->name, gp->txStart, gp->txEnd);
else
    fprintf(linkFile,"noRefSeq -1 -1 ");
if (mgc != NULL)
    fprintf(linkFile,"%s %d %d ", mgc->name, mgc->txStart, mgc->txEnd);
else
    fprintf(linkFile,"noMgc -1 -1 ");
if (kg != NULL)
    {
    if (kg->name2 != NULL)
        fprintf(linkFile,"%s %d %d %d ", kg->name2, kg->txStart, kg->txEnd, 0);
    else
        fprintf(linkFile,"%s %d %d %d ", kg->name, kg->txStart, kg->txEnd, 0);
    }
else
    fprintf(linkFile,"noKg -1 -1 -1 ");
}

int intronFactor(struct psl *psl, struct hash *bkHash, struct hash *trfHash)
/* Figure approx number of introns.  
 * An intron in this case is just a gap of 0 bases in query and
 * maxBlockGap or more in target iwth repeats masked. */
{
int i, blockCount = psl->blockCount;
int ts, qs, te, qe, sz, tsRegion, teRegion;
struct binKeeper *bk;
int intronCount = 0, tGap;
int maxQGap = 2; /* max size of gap on q size to be considered not an intron */

assert (psl != NULL);
if (blockCount <= 1)
    return 0;
sz = psl->blockSizes[0];
qe = psl->qStarts[0] + sz;
te = psl->tStarts[0] + sz;
tsRegion = psl->tStarts[0];
for (i=1; i<blockCount; ++i)
    {
    int trf = 0;
    int reps = 0, regionReps = 0;
    struct binElement *el, *elist;
    struct rmskOut *rmsk;
    struct bed *bed;
    qs = psl->qStarts[i];
    ts = psl->tStarts[i];
    teRegion = psl->tStarts[i] + psl->blockSizes[i];

    if (bkHash != NULL)
        {
        if (trfHash != NULL)
            {
            bk = hashFindVal(trfHash, psl->tName);
            elist = binKeeperFindSorted(bk, psl->tStart , psl->tEnd ) ;
            trf = 0;
            for (el = elist; el != NULL ; el = el->next)
                {
                bed = el->val;
                trf += positiveRangeIntersection(te, ts, bed->chromStart, bed->chromEnd);
                }
            slFreeList(&elist);
            }
        bk = hashFindVal(bkHash, psl->tName);
        elist = binKeeperFindSorted(bk, tsRegion , teRegion ) ;
        for (el = elist; el != NULL ; el = el->next)
            {
            assert (el->val != NULL);
            rmsk = el->val;
            assert(rmsk != NULL);
            /* count repeats in the gap */
            reps += positiveRangeIntersection(te, ts, rmsk->genoStart, rmsk->genoEnd);
            /* count repeats in the gap  and surrounding exons  */
            regionReps += positiveRangeIntersection(tsRegion, teRegion, rmsk->genoStart, rmsk->genoEnd);
            }
        if (elist != NULL)
            slFreeList(&elist);
        }
    /* don't subtract repeats if the entire region is masked */
    if ( regionReps + 10 >= teRegion - tsRegion )
        reps = 0;

    tGap = ts - te - reps*1.5 - trf;
    if (2*abs(qs - qe) <= tGap && tGap >= maxBlockGap ) 
        /* don't count q gaps in mrna as introns , if they are similar in size to tGap*/
        {
        intronCount++;
        }
    assert(psl != NULL);
    sz = psl->blockSizes[i];
    qe = qs + sz;
    te = ts + sz;
    tsRegion = psl->tStarts[i];
    }
return intronCount;
}

int sizeFactor(struct psl *psl, struct hash *bkHash, struct hash *trfHash)
/* Return a factor that will favor longer alignments. An intron is worth 3 bases...  */
{
int score;
if (ignoreSize) return 0;
assert(psl != NULL);
score = 4*round(sqrt(psl->match + psl->repMatch/4));
if (!noIntrons)
    {
    int bonus = intronFactor(psl, bkHash, trfHash) * 3;
    if (bonus > 10) bonus = 10;
    score += bonus;
    }
return score;
}

int calcSizedScore(struct psl *psl, struct hash *bkHash, struct hash *trfHash)
/* Return score that includes base matches and size. */
{
int score = calcMilliScore(psl) + sizeFactor(psl, bkHash, trfHash);
return score;
}

boolean uglyTarget(struct psl *psl)
/* Return TRUE if it's the one we're snooping on. */
{
return TRUE; //sameString(psl->qName, "NM_006905") || sameString(psl->qName, "AK");
}


boolean closeToTop(struct psl *psl, int *scoreTrack , struct hash *bkHash, struct hash *trfHash)
/* Returns TRUE if psl is near the top scorer for at least 20 bases. */
{
int milliScore = calcSizedScore(psl, bkHash, trfHash);
int threshold = round(milliScore * (1.0+nearTop));
int i, blockIx;
int start, size, end;
int topCount = 0;
char strand = psl->strand[0];

if (uglyTarget(psl)) uglyf("%s:%d milliScore %d, threshold %d\n", psl->tName, psl->tStart, milliScore, threshold);
for (blockIx = 0; blockIx < psl->blockCount; ++blockIx)
    {
    start = psl->qStarts[blockIx];
    size = psl->blockSizes[blockIx];
    end = start+size;
    if (strand == '-')
	reverseIntRange(&start, &end, psl->qSize);
    for (i=start; i<end; ++i)
	{
	if (scoreTrack[i] <= threshold)
	    {
	    if (++topCount >= minNearTopSize)
		return TRUE;
	    }
	}
    }
return FALSE;
}

int scoreWindow(char c, char *s, int size, int *score, int *start, int *end)
/* calculate score array with score at each position in s, match to char c adds 1 to score, mismatch adds -1 */
/* index of max score is returned , size is size of s */
{
int i=0, j=0, max=0, count = 0; 

*end = 0;

for (i=0 ; i<size ; i++)
    {
    int prevScore = (i > 0) ? score[i-1] : 0;

    if (toupper(s[i]) == toupper(c) )
        score[i] = prevScore+1;
    else
        score[i] = prevScore-1;
    if (score[i] >= max)
        {
        max = score[i];
        *end = i;
        for (j=i ; j>=0 ; j--)
            if (score[j] == 0)
                {
                *start = j+1;
                break;
                }
//        printf("max is %d %d - %d ",max, *start, *end);
        }
    if (score[i] < 0) 
        score[i] = 0;
    }
assert (*end < size);
/* traceback to find start */
for (i=*end ; i>=0 ; i--)
    if (score[i] == 0)
        {
        *start = i+1;
        break;
        }

for (i=*start ; i<=*end ; i++)
    {
    assert (i < size);
    if (toupper(s[i]) == toupper(c) )
        count++;
    }
return count;
}
   

int countCharsInWindow(char c, char *s, int size, int winSize, int *start)
/* Count number of characters matching c in s in a sliding window of size winSize. return start of polyA tail */
{
int i=0, count=0, maxCount=0, j=0;
for (j=0 ; j<=size-winSize ; j++)
    {
    count = 0;
    for (i=j; i<winSize+j ; i++)
        {
        assert (i < size);
        if (toupper(s[i]) == toupper(c) )
            count++;
        }
    maxCount = max(maxCount, count);
    *start = j;
    }
return maxCount;
}

int polyACalc(int start, int end, char *strand, int tSize, char *nibDir, char *chrom, int winSize, int region, int *polyAstart, int *polyAend)
/* get size of polyA tail in genomic dna , count bases in a 
 * sliding winSize window in region of the end of the sequence*/
{
char nibFile[256];
FILE *f;
int seqSize;
struct dnaSeq *seq = NULL;
int count = 0;
int seqStart = strand[0] == '+' ? end-(region/2) : start-(region/2);
int score[POLYAREGION+1], pStart = 0, pEnd = 0; 

assert(region > 0);
assert(end != 0);
*polyAstart = 0 , *polyAend = 0;
safef(nibFile, sizeof(nibFile), "%s/%s.nib", nibDir, chrom);
nibOpenVerify(nibFile, &f, &seqSize);
assert (seqSize == tSize);
if (seqStart < 0) seqStart = 0;
if (seqStart + region > seqSize) region = seqSize - seqStart;
assert(region > 0);
seq = nibLdPartMasked(NIB_MASK_MIXED, nibFile, f, seqSize, seqStart, region);
if (strand[0] == '+')
    {
    assert (seq->size <= POLYAREGION);
//printf("\n + range=%d %d %s \n",seqStart, seqStart+region, seq->dna );
    count = scoreWindow('A',seq->dna,seq->size, score, polyAstart, polyAend);
    }
else
    {
    assert (seq->size <= POLYAREGION);
//printf("\n - range=%d %d %s \n",seqStart, seqStart+region, seq->dna );
    count = scoreWindow('T',seq->dna,seq->size, score, polyAend, polyAstart);
    }
pStart += seqStart;
*polyAstart += seqStart;
*polyAend += seqStart;
//printf("\nst=%d %s range %d %d cnt %d\n",seqStart, seq->dna, *polyAstart, *polyAend, count);
freeDnaSeq(&seq);
fclose(f);
return count;
}

void pslMergeBlocks(struct psl *psl, struct psl *outPsl,
                       int insertMergeSize)
/* merge together blocks separated by small inserts. */
{
int iBlk, iExon = -1;
int startIdx, stopIdx, idxIncr;

assert(outPsl!=NULL);
outPsl->qStarts = needMem(psl->blockCount*sizeof(unsigned));
outPsl->tStarts = needMem(psl->blockCount*sizeof(unsigned));
outPsl->blockSizes = needMem(psl->blockCount*sizeof(unsigned));

//if (psl->strand[1] == '-')
//    {
//    startIdx = psl->blockCount-1;
//    stopIdx = -1;
//    idxIncr = -1;
//    }
//else
//    {
    startIdx = 0;
    stopIdx = psl->blockCount;
    idxIncr = 1;
//    }

for (iBlk = startIdx; iBlk != stopIdx; iBlk += idxIncr)
    {
    unsigned tStart = psl->tStarts[iBlk];
    unsigned tEnd = tStart+psl->blockSizes[iBlk];
    unsigned qStart = psl->qStarts[iBlk];
    unsigned size = psl->blockSizes[iBlk];
//    if (psl->strand[1] == '-')
//        reverseIntRange(&tStart, &tEnd, psl->tSize);
    if ((iExon < 0) || ((tStart - (outPsl->tStarts[iExon]+outPsl->blockSizes[iExon])) > insertMergeSize))
        {
        iExon++;
        outPsl->tStarts[iExon] = tStart;
        outPsl->qStarts[iExon] = qStart;
        outPsl->blockSizes[iExon] = size;
	}
    else
        outPsl->blockSizes[iExon] += size;
    }
outPsl->blockCount = iExon+1;
outPsl->match = psl->match;
outPsl->misMatch = psl->misMatch;
outPsl->repMatch = psl->repMatch;
outPsl->nCount = psl->nCount;
outPsl->qNumInsert = psl->qNumInsert;
outPsl->qBaseInsert = psl->qBaseInsert;
outPsl->tNumInsert = psl->tNumInsert;
outPsl->tBaseInsert = psl->tBaseInsert;
strcpy(outPsl->strand, psl->strand);
outPsl->qName = cloneString(psl->qName);
outPsl->qSize = psl->qSize;
outPsl->qStart = psl->qStart;
outPsl->qEnd = psl->qEnd;
outPsl->tName = cloneString(psl->tName);
outPsl->tSize = psl->tSize;
outPsl->tStart = psl->tStart;
outPsl->tEnd = psl->tEnd;
}

int pslCountExonSpan(struct psl *target, struct psl *query, int maxBlockGap, struct hash *bkHash , int *tReps, int *qReps)
/* count the number of blocks in the query that overlap the target */
/* merge blocks that are closer than maxBlockGap */
{
int i, j, start = 0, qs, qqstart = 0 , qqs;
int count = 0;
struct binKeeper *bk = NULL;
struct binElement *elist = NULL, *el = NULL;
struct rmskOut *rmsk = NULL;
struct psl *targetM , *queryM;

*tReps = 0; *qReps = 0;
if (target == NULL || query == NULL)
    return 0;

AllocVar(targetM);
AllocVar(queryM);
pslMergeBlocks(target, targetM, maxBlockGap);
pslMergeBlocks(query, queryM, maxBlockGap);

for (i = 0 ; i < targetM->blockCount ; i++)
  {
    int qe = targetM->qStarts[i] + targetM->blockSizes[i];
    int ts = targetM->tStarts[i] ;
    int te = targetM->tStarts[i] + targetM->blockSizes[i];
    int teReps = 0;
    qs = targetM->qStarts[i];
    /* combine blocks that are close together */
    if (i < (targetM->blockCount) -1)
        {
        /* skip exons that overlap repeats */
        if (bkHash != NULL)
            {
            bk = hashFindVal(bkHash, targetM->tName);
            elist = binKeeperFindSorted(bk, ts, te) ;
            for (el = elist; el != NULL ; el = el->next)
                {
                assert (el->val != NULL);
                rmsk = el->val;
                assert(rmsk != NULL);
                teReps += positiveRangeIntersection(ts, te, rmsk->genoStart, rmsk->genoEnd);
                }
            slFreeList(&elist);
            *tReps += teReps;
            if (2 * teReps > (te-ts))
                continue;
            }
        }
    if (targetM->strand[0] == '-')
        reverseIntRange(&qs, &qe, targetM->qSize);
    if (qs < 0 || qe > targetM->qSize)
        {
        warn("Odd: qName %s tName %s qSize %d psl->qSize %d start %d end %d",
            targetM->qName, targetM->tName, targetM->qSize, targetM->qSize, qs, qe);
        if (qs < 0)
            qs = 0;
        if (qe > targetM->qSize)
            qe = targetM->qSize;
        }
    if (positiveRangeIntersection(queryM->qStart, queryM->qEnd, qs, qe) > min(20,qe-qs))
        {
        int qqe = 0;
        for (j = 0 ; j < queryM->blockCount ; j++)
            {
            int localReps = 0;
            qqs = queryM->qStarts[0];
            qqe = queryM->qStarts[j] + queryM->blockSizes[j];
            if (queryM->strand[0] == '-')
                reverseIntRange(&qqs, &qqe, queryM->qSize);
            assert(j < queryM->blockCount);
            /* mask repeats */
            bk = hashFindVal(bkHash, queryM->tName);
            if (j < (queryM->blockCount) -1)
                {
                elist = binKeeperFindSorted(bk, queryM->tStarts[j], queryM->tStarts[j+1]) ;
                for (el = elist; el != NULL ; el = el->next)
                    {
                    assert (el->val != NULL);
                    rmsk = el->val;
                    assert(rmsk != NULL);
                    localReps += positiveRangeIntersection(queryM->tStarts[j], queryM->tStarts[j]+queryM->blockSizes[j], rmsk->genoStart, rmsk->genoEnd);
                    }
                *qReps += localReps;
                slFreeList(&elist);
                }
            }
        if (positiveRangeIntersection(qqs, qqe, qs, qe) > 10)
            count++;
        }
    start = i+1;
    }

if(count > targetM->blockCount )
    {
    printf("error in pslCountExonSpan: %s %s %s:%d-%d %d > targetBlk %d or query Blk %d \n",
            targetM->qName, queryM->qName, queryM->tName,queryM->tStart, queryM->tEnd, 
            count, targetM->blockCount, queryM->blockCount);
    assert(count > targetM->blockCount);
    }
pslFree(&targetM);
pslFree(&queryM);
return count;
}
void processBestMulti(char *acc, struct psl *pslList, struct hash *trfHash, struct hash *synHash, struct hash *mrnaHash, struct hash *bkHash, char *nibDir)
/* Find psl's that are best anywhere along their length. */

{
struct psl *bestPsl = NULL, *psl, *bestSEPsl = NULL;
struct genePred *gp = NULL, *kg = NULL, *mgc = NULL;
char bestOverlap[255];
int geneOverlap = -1;
int qSize = 0;
int *scoreTrack = NULL;
int milliScore;
int exonCount = -1;
int pslIx, blockIx;
int goodAliCount = 0;
int bestAliCount = 0;
int milliMin = 1000*minAli;
int milliMinPseudo = 1000*minAliPseudo;
int maxExons = 0;
int bestStart = 0, bestEnd = 0;
int bestSEStart = 0, bestSEEnd = 0;
int polyAstart = 0;
int polyAend = 0;
struct binElement *el, *elist;
struct binKeeper *bk;
int trf = 0, rep = 0;
char *bestChrom = NULL, *bestSEChrom = NULL;
struct bed *bed;
struct rmskOut *rmsk = NULL;
int tReps , qReps;
int i ;
int bestScore = 0, bestSEScore = 0;

if (pslList == NULL)
    return;

/* get biggest mrna - some have polyA tail stripped off */
for (psl = pslList; psl != NULL; psl = psl->next)
    if (psl->qSize > qSize)
        qSize = psl->qSize;

AllocArray(scoreTrack, qSize+1);

for (psl = pslList; psl != NULL; psl = psl->next)
    {
    int blockIx;
    char strand = psl->strand[0];

    //printf("checking %s %s:%d-%d\n",psl->qName, psl->tName, psl->tStart, psl->tEnd);
    assert (psl!= NULL);
    milliScore = calcMilliScore(psl);
    if (milliScore >= milliMin)
	{
	++goodAliCount;
	milliScore += sizeFactor(psl, bkHash, trfHash);
//if (uglyTarget(psl)) uglyf("@ %s %s:%d milliScore %d\n", psl->qName, psl->tName, psl->tStart, milliScore);
	for (blockIx = 0; blockIx < psl->blockCount; ++blockIx)
	    {
	    int start = psl->qStarts[blockIx];
	    int size = psl->blockSizes[blockIx];
	    int end = start+size;
	    int i;
            if (strand == '-')
	        reverseIntRange(&start, &end, psl->qSize);
	    if (start < 0 || end > psl->qSize || psl->qSize > qSize)
		{
		warn("Error: qName %s tName %s qSize %d psl->qSize %d start %d end %d",
		    psl->qName, psl->tName, qSize, psl->qSize, start, end);
		}
	    for (i=start; i<end; ++i)
		{
                assert(i<=qSize);
		if (milliScore > scoreTrack[i])
		    scoreTrack[i] = milliScore;
		}
	    }
	}
    }
if (uglyTarget(pslList)) uglyf("---finding best---\n");
/* Print out any alignments that are within 2% of top score. */
bestScore = 0;
bestSEScore = 0;
for (psl = pslList; psl != NULL; psl = psl->next)
    {
    int intronCount = 0;
    struct psl *pslMerge;
    if (
        calcMilliScore(psl) >= milliMin && closeToTop(psl, scoreTrack, bkHash, trfHash)
        && psl->match + psl->repMatch >= minCover * psl->qSize)
	{
        ++bestAliCount;
        AllocVar(pslMerge);
        pslMergeBlocks(psl, pslMerge, 30);
        if (calcMilliScore(psl) > bestScore && pslMerge->blockCount > 1)
            {
            bestPsl = psl;
            bestStart = psl->tStart;
            bestEnd = psl->tEnd;
            bestChrom = cloneString(psl->tName);
            bestScore = calcMilliScore(psl);
            }
        if (calcMilliScore(psl) > bestSEScore )
            {
            bestSEPsl = psl;
            bestSEStart = psl->tStart;
            bestSEEnd = psl->tEnd;
            bestSEChrom = cloneString(psl->tName);
            bestSEScore = calcMilliScore(psl);
            }
        //intronCount = intronFactor(psl, bkHash, trfHash);
        if (pslMerge->blockCount > maxExons )
            maxExons = pslMerge->blockCount;
        pslFree(&pslMerge);
	}
//if (uglyTarget(psl)) uglyf("accepted %s %s:%d maxIn = %d\n",psl->qName, psl->tName, psl->tStart , maxExons);
    }
/* output pseudogenes, if alignments have no introns and mrna alignmed with introns */
if (pslList != NULL)
    for (psl = pslList; psl != NULL; psl = psl->next)
    {
    if (
        calcMilliScore(psl) >= milliMin && closeToTop(psl, scoreTrack, bkHash, trfHash)
        && psl->match + psl->repMatch >= minCover * psl->qSize)
            {
            pslTabOut(psl, bestFile);
            }
    else 
        {
        /* calculate various features of pseudogene */
        int intronCount = intronFactor(psl, bkHash, trfHash);
        int overlapDiagonal = -1;
        int polyA = polyACalc(psl->tStart, psl->tEnd, psl->strand, psl->tSize, nibDir, psl->tName, 
                        POLYASLIDINGWINDOW, POLYAREGION, &polyAstart, &polyAend);
        /* count # of alignments that span introns */
        int exonCover = pslCountExonSpan(bestPsl, psl, maxBlockGap, bkHash, &tReps, &qReps) ;

        geneOverlap = 0;
        kg = NULL;
        gp = NULL;
        mgc = NULL;
        if (bestPsl != NULL)
            {
            kg = getOverlappingGene(&kgList, "knownGene", bestPsl->tName, bestPsl->tStart, 
                                bestPsl->tEnd , bestPsl->qName, &geneOverlap);
            gp = getOverlappingGene(&gpList1, "refGene", bestPsl->tName, bestPsl->tStart, 
                                bestPsl->tEnd , bestPsl->qName, &geneOverlap);
            mgc = getOverlappingGene(&gpList2, "mgcGenes", bestPsl->tName, bestPsl->tStart, 
                                bestPsl->tEnd , bestPsl->qName, &geneOverlap);
            }
        /* calculate if pseudogene overlaps the syntenic diagonal with another species */
        if (synHash != NULL)
            {
            bk = hashFindVal(synHash, psl->tName);
            elist = binKeeperFindSorted(bk, psl->tStart , psl->tEnd ) ;
            overlapDiagonal = 0;
            for (el = elist; el != NULL ; el = el->next)
                {
                bed = el->val;
                overlapDiagonal += positiveRangeIntersection(psl->tStart, psl->tEnd, 
                        bed->chromStart, bed->chromEnd);
                }
            overlapDiagonal = (overlapDiagonal*100)/(psl->tEnd-psl->tStart);
            slFreeList(&elist);
            }

        if (intronCount == 0 && 
            maxExons > 1 && bestAliCount > 0 && bestChrom != NULL &&
            (calcMilliScore(psl) >= milliMinPseudo && 
            psl->match + psl->repMatch >= minCoverPseudo * (float)psl->qSize))
            {

            if (exonCover > 1 && intronCount == 0 && 
                maxExons > 1 && bestAliCount > 0 && bestChrom != NULL &&
                (calcMilliScore(psl) >= milliMinPseudo && 
                psl->match + psl->repMatch >= minCoverPseudo * (float)psl->qSize))
                {
                if (trfHash != NULL)
                    {
                    bk = hashFindVal(trfHash, psl->tName);
                    elist = binKeeperFindSorted(bk, psl->tStart , psl->tEnd ) ;
                    trf = 0;
                    for (el = elist; el != NULL ; el = el->next)
                        {
                        bed = el->val;
                        trf += positiveRangeIntersection(psl->tStart, psl->tEnd, bed->chromStart, bed->chromEnd);
                        }
                    slFreeList(&elist);
                    }
                if ((float)trf/(float)(psl->match+psl->misMatch) > .5)
                    {
                    if (!quiet)
                        printf("NO. %s trf overlap %f > .5 %s %d \n",psl->qName, (float)trf/(float)(psl->match+psl->misMatch) , psl->tName, psl->tStart);
                    continue;
                    }



                /* count repeat overlap with pseudogenes and skip ones with more than maxRep% overlap*/
                if (bkHash != NULL)
                    {
                    rep = 0;
                    for (i = 0 ; i < psl->blockCount ; i++)
                        {
                        int ts = psl->tStarts[i] ;
                        int te = psl->tStarts[i] + psl->blockSizes[i];
                        bk = hashFindVal(bkHash, psl->tName);
                        elist = binKeeperFindSorted(bk, ts, te) ;
                        for (el = elist; el != NULL ; el = el->next)
                            {
                            rmsk = el->val;
                            if (rmsk != NULL)
                                {
                                assert (psl != NULL);
                                assert (rmsk != NULL);
                                assert (rmsk->genoName != NULL);
                                assert (psl->tName != NULL);
                                rep += positiveRangeIntersection(ts, te, rmsk->genoStart, rmsk->genoEnd);
                                //printf("%s %d %d rmsk ",psl->tName, ts,te );
                                //printf("%s %d %d %d\n",rmsk->genoName, rmsk->genoStart, rmsk->genoEnd, rep);
                                }
                            }
                        slFreeList(&elist);
                        }
                    }
                if ((float)rep/(float)(psl->match+(psl->misMatch)) > maxRep )
                    {
                    if (!quiet)
                        printf("NO %s reps %.3f %.3f\n",psl->tName,(float)rep/(float)(psl->match+(psl->misMatch)) , maxRep);
                    if (bestPsl == NULL)
                        outputNoLink( psl, "maxRep", "?", "?", -1, -1, 
                                maxExons, geneOverlap, "?", polyA, polyAstart, NOTPSEUDO ,
                                exonCover, intronCount, bestAliCount, rep, qReps, overlapDiagonal); 
                    else
                        outputNoLink( psl, "maxRep", bestPsl->qName, bestPsl->tName, bestPsl->tStart, bestPsl->tEnd, 
                                maxExons, geneOverlap, bestPsl->strand, polyA, polyAstart, NOTPSEUDO ,
                                exonCover, intronCount, bestAliCount, rep, qReps, overlapDiagonal); 
                    continue;
                    }


                /* count good mrna overlap with pseudogenes. If name matches then don't filter it.*/
                if (mrnaHash != NULL)
                    {
                    int mrnaBases = 0;
                    struct psl *mPsl = NULL, *mPslMerge = NULL;
                    bk = hashFindVal(mrnaHash, psl->tName);
                    elist = binKeeperFindSorted(bk, psl->tStart , psl->tEnd ) ;
                    safef(bestOverlap,255,"NONE");
                    for (el = elist; el != NULL ; el = el->next)
                        {
                        mPsl = el->val;
                        if (mPsl != NULL)
                            {
                            assert (psl != NULL);
                            assert (mPsl != NULL);
                            assert (psl->tName != NULL);
                            if (sameString(psl->qName, mPsl->qName))
                                continue;
                            //if (intronFactor(mPsl, bkHash, trfHash) > 0 )
                            AllocVar(mPslMerge);
                            pslMergeBlocks(mPsl, mPslMerge, 30);
                            if (mPsl->blockCount > 0)
                                for (blockIx = 0; blockIx < mPsl->blockCount; ++blockIx)
                                    {
                                    mrnaBases += positiveRangeIntersection(psl->tStart, psl->tEnd, 
                                        mPsl->tStarts[blockIx], mPsl->tStarts[blockIx]+mPsl->blockSizes[blockIx]);
                                    }
                            else
                                mrnaBases += positiveRangeIntersection(psl->tStart, psl->tEnd, mPsl->tStart, mPsl->tEnd);
                            if (mrnaBases > 50 && mPslMerge != NULL)
                                if ((int)mPslMerge->blockCount > exonCount)
                                    {
                                    exonCount = (int)mPslMerge->blockCount;
                                    safef(bestOverlap,255,"%s",mPsl->qName);
                                    }
                            pslFree(&mPslMerge);
                            }
                        }
                    slFreeList(&elist);
                    printf("mrnaBases %d exons %d name %s pos %s:%d-%d\n",
                            mrnaBases,exonCount,mPsl == NULL ? "NULL" : bestOverlap, psl->tName,psl->tStart,psl->tEnd);
                    if (mrnaBases > 50  && exonCount > 1) 
                        /* if overlap > 50 bases and not single exon mrna , then skip*/
                        {
                        if (!quiet)
                            printf("NO %s better blat mrna %s \n",psl->qName,bestOverlap);
                        outputNoLink(psl, "better", bestOverlap, mPsl->tName, mPsl->tStart, mPsl->tEnd, 
                                maxExons, geneOverlap, mPsl->strand, polyA, polyAstart, 
                                NOTPSEUDO, exonCover, intronCount, bestAliCount, 
                                tReps, qReps, overlapDiagonal); 
                        continue;
                        }
                    }


                    /* blat sometimes overlaps parts of the same mrna , filter these */
                    if ( positiveRangeIntersection(bestStart, bestEnd, psl->tStart, psl->tEnd) && 
                                sameString(psl->tName, bestChrom))
                        {
                        if (bestPsl == NULL)
                            outputNoLink( psl, "self", "?", "?", -1, -1, 
                                    maxExons, geneOverlap, "?", polyA, polyAstart, NOTPSEUDO ,
                                    exonCover, intronCount, bestAliCount, tReps, qReps, overlapDiagonal); 
                        else
                            outputNoLink(psl, "self", bestPsl->qName, bestPsl->tName, bestPsl->tStart, bestPsl->tEnd, 
                                    maxExons, geneOverlap, bestPsl->strand, polyA, polyAstart, NOTPSEUDO,
                                    exonCover, intronCount, bestAliCount, tReps, qReps, overlapDiagonal); 
                        }
                    else if (overlapDiagonal >= 40 && bestPsl == NULL)
                       {
                        if (!quiet)
                            printf("NO. %s %d diag %s %d  bestChrom %s\n",psl->qName, 
                                    overlapDiagonal, psl->tName, psl->tStart, bestChrom);
                        if (bestPsl == NULL)
                            outputNoLink( psl, "diagonal", "?", "?", -1, -1, 
                                    maxExons, geneOverlap, "?", polyA, polyAstart, NOTPSEUDO ,
                                    exonCover, intronCount, bestAliCount, tReps, qReps, overlapDiagonal); 
                        /* keep ancient pseudogenes 
                        else
                            outputNoLink(psl, "diagonal", bestPsl->qName, bestPsl->tName, bestPsl->tStart, bestPsl->tEnd, 
                                maxExons, geneOverlap, bestPsl->strand, polyA, polyAstart, NOTPSEUDO,
                                exonCover, intronCount, bestAliCount, tReps, qReps, overlapDiagonal); 
                                */
                        }
                    else
                        {
                        if (!quiet)
                            {
                            printf("YES %s %d rr %3.1f rl %d ln %d %s iF %d maxE %d bestAli %d isp %d score %d match %d cover %3.1f rp %d polyA %d syn %d",
                                psl->qName,psl->tStart,((float)rep/(float)(psl->tEnd-psl->tStart) ),rep, 
                                psl->tEnd-psl->tStart,psl->tName, intronCount, 
                                maxExons , bestAliCount, exonCover,
                                calcMilliScore(psl),  psl->match + psl->repMatch , 
                                minCoverPseudo * (float)psl->qSize, tReps + qReps, polyA, overlapDiagonal );
                            if (rmsk != NULL)
                                printf(" rmsk %s",rmsk->genoName);
                            printf("\n");
                            }
                        pslTabOut(psl, pseudoFile);
                        if (bestPsl == NULL)
                            outputNoLink(psl, "mrna", "?", "?", -1, -1, 
                                    maxExons, geneOverlap, "?", polyA, polyAstart, NOTPSEUDO,
                                    exonCover, intronCount, bestAliCount, tReps, qReps, overlapDiagonal); 
                        else if (kg == NULL && mgc == NULL && gp == NULL)
                            outputNoLink(psl, "mrna", bestPsl->qName, bestPsl->tName, bestPsl->tStart, bestPsl->tEnd, 
                                        maxExons, geneOverlap, bestPsl->strand, polyA, polyAstart, PSEUDO,
                                        exonCover, intronCount, bestAliCount, tReps, qReps, overlapDiagonal); 
                        else
                            outputLink( psl, "knownGene", bestPsl->qName, bestPsl->tName, bestPsl->tStart, bestPsl->tEnd, 
                                    maxExons, geneOverlap, bestPsl->strand, polyA, polyAstart, PSEUDO,
                                    exonCover, intronCount, bestAliCount, tReps, qReps, overlapDiagonal, kg, mgc, gp ); 
                        }
                }
                else
                {
                    if (!quiet)
                        printf("NO. %s %d rr %3.1f rl %d ln %d %s iF %d maxE %d bestAli %d isp %d score %d match %d cover %3.1f rp %d\n",
                            psl->qName,psl->tStart,((float)rep/(float)(psl->tEnd-psl->tStart) ),rep, 
                            psl->tEnd-psl->tStart,psl->tName, intronCount, maxExons , 
                            bestAliCount, exonCover,
                            calcMilliScore(psl),  psl->match + psl->repMatch , 
                            minCoverPseudo * (float)psl->qSize, tReps + qReps);
                    if (bestPsl == NULL)
                        outputNoLink( psl, "span", "?", "?", -1, -1, 
                                maxExons, geneOverlap, "?", polyA, polyAstart, NOTPSEUDO ,
                                exonCover, intronCount, bestAliCount, tReps, qReps, overlapDiagonal); 
                    else
                        outputNoLink( psl, "span", bestPsl->qName, bestPsl->tName, bestPsl->tStart, bestPsl->tEnd, 
                                maxExons, geneOverlap, bestPsl->strand, polyA, polyAstart, NOTPSEUDO,
                                exonCover, intronCount, bestAliCount, tReps, qReps, overlapDiagonal); 
                }
            }
            else 
            {
                if (!quiet)
                    printf("NO. %s %d rr %3.1f rl %d ln %d %s iF %d maxE %d bestAli %d isp %d score %d match %d cover %3.1f rp %d\n",
                        psl->qName,psl->tStart,((float)rep/(float)(psl->tEnd-psl->tStart) ),rep, 
                        psl->tEnd-psl->tStart,psl->tName, intronCount, maxExons , bestAliCount, exonCover,
                        calcMilliScore(psl),  psl->match + psl->repMatch , minCoverPseudo * (float)psl->qSize, tReps + qReps);
                if (bestPsl != NULL)
                    outputNoLink( psl, "introns", bestPsl->qName, bestPsl->tName, bestPsl->tStart, bestPsl->tEnd, 
                            maxExons, geneOverlap, bestPsl->strand, polyA, polyAstart, NOTPSEUDO,
                            exonCover, intronCount, bestAliCount, tReps, qReps, overlapDiagonal); 
                else
                    outputNoLink( psl, "noBest", "?", "?", -1, -1, 
                            maxExons, geneOverlap, "?", polyA, polyAstart, NOTPSEUDO,
                            exonCover, intronCount, bestAliCount, tReps, qReps, overlapDiagonal); 
            }
        }
    }
freeMem(scoreTrack);
}

void processBestSingle(char *acc, struct psl *pslList, struct hash *trfHash, struct hash *synHash, struct hash *mrnaHash, struct hash *bkHash, char *nibDir)
/* Find single best psl in list. */
{
struct psl *bestPsl = NULL, *psl;
int bestScore = 0, score, threshold;

assert(0==1); /* not all checks implemented */
for (psl = pslList; psl != NULL; psl = psl->next)
    {
    score = pslScore(psl);
    if (score > bestScore)
        {
	bestScore = score;
	bestPsl = psl;
	}
    }
threshold = round((1.0 - nearTop)*bestScore);
for (psl = pslList; psl != NULL; psl = psl->next)
    {
    if (pslScore(psl) >= threshold)
        pslTabOut(psl, bestFile);
    }
}

void doOneAcc(char *acc, struct psl *pslList, struct hash *trfHash, struct hash *synHash, struct hash *mrnaHash, 
        struct hash *bkHash, char *nibDir)
/* Process alignments of one piece of mRNA. */
{
if (singleHit)
    processBestSingle(acc, pslList, trfHash, synHash, mrnaHash, bkHash, nibDir);
else
    processBestMulti(acc, pslList, trfHash, synHash, mrnaHash, bkHash, nibDir);
}

void pslPseudo(char *inName, struct hash *bkHash, struct hash *trfHash, struct hash *synHash, struct hash *mrnaHash, 
        char *bestAliName, char *psuedoFileName, char *linkFileName, char *nibDir)
/* find best alignments with and without introns.
 * Put pseudogenes  in pseudoFileName. 
 * store link between pseudogene and gene in LinkFileName */
{
struct lineFile *in = pslFileOpen(inName);
int lineSize;
char *line;
char *words[32];
int wordCount;
struct psl *pslList = NULL, *psl;
char lastName[256] = "nofile";
int aliCount = 0;
quiet = sameString(bestAliName, "stdout") || sameString(psuedoFileName, "stdout");
bestFile = mustOpen(bestAliName, "w");
pseudoFile = mustOpen(psuedoFileName, "w");
linkFile = mustOpen(linkFileName, "w");

if (!quiet)
    printf("Processing %s to %s and %s\n", inName, bestAliName, psuedoFileName);
 if (!noHead)
     pslWriteHead(bestFile);
safef(lastName, sizeof(lastName),"x");
while (lineFileNext(in, &line, &lineSize))
    {
    if (((++aliCount & 0x1ffff) == 0) && !quiet)
        {
	printf(".");
	fflush(stdout);
	}
    wordCount = chopTabs(line, words);
    if (wordCount != 21)
	errAbort("Bad line %d of %s\n", in->lineIx, in->fileName);
    psl = pslLoad(words);
    if (!sameString(lastName, psl->qName))
	{
	doOneAcc(lastName, pslList, trfHash, synHash, mrnaHash, bkHash, nibDir);
	pslFreeList(&pslList);
	safef(lastName, sizeof(lastName), psl->qName);
	}
    slAddHead(&pslList, psl);
    }
doOneAcc(lastName, pslList, trfHash, synHash, mrnaHash, bkHash, nibDir);
pslFreeList(&pslList);
lineFileClose(&in);
fclose(bestFile);
fclose(pseudoFile);
if (!quiet)
    printf("Processed %d alignments\n", aliCount);
}

int main(int argc, char *argv[])
/* Process command line. */
{
struct hash *bkHash = NULL, *trfHash = NULL, *synHash = NULL, *mrnaHash = NULL;
//struct lineFile *lf = NULL;
//struct bed *syntenicList = NULL, *bed;
//char  *row[3];
struct genePredReader *gprKg;

cgiSpoof(&argc, argv);
if (argc != 15)
    usage();
db = cloneString(argv[1]);
minAli = cgiOptionalDouble("minAli", minAli);
maxRep = cgiOptionalDouble("maxRep", maxRep);
minAliPseudo = cgiOptionalDouble("minAliPseudo", minAliPseudo);
nearTop = cgiOptionalDouble("nearTop", nearTop);
minCover = cgiOptionalDouble("minCover", minCover);
minCoverPseudo = cgiOptionalDouble("minCoverPseudo", minCoverPseudo);
minNearTopSize = cgiOptionalInt("minNearTopSize", minNearTopSize);
maxBlockGap = cgiOptionalInt("maxBlockGap" , maxBlockGap) ;
ignoreSize = cgiBoolean("ignoreSize");
noIntrons = cgiBoolean("noIntrons");
//singleHit = cgiBoolean("singleHit");
noHead = cgiBoolean("nohead");

if (!quiet)
    printf("Scoring alignments from %s.\n",argv[2]);
gpList1 = genePredLoadAll(argv[12]);
gpList2 = genePredLoadAll(argv[13]);
//gprKg = genePredReaderFile(argv[14], NULL);
//kgLis = genePredReaderAll(gprKg);
kgList = genePredLoadAll(argv[14]);

if (!quiet)
    printf("Loading Syntenic Bed %s\n",argv[5]);
synHash = readBedToBinKeeper(argv[3], argv[5], WORDCOUNT);
if (!quiet)
    printf("Loading Trf Bed %s\n",argv[6]);
trfHash = readBedToBinKeeper(argv[3], argv[6], WORDCOUNT);
if (!quiet)
    printf("Reading Repeats from %s\n",argv[4]);
bkHash = readRepeatsAll(argv[3], argv[4]);
if (!quiet)
    printf("Reading mrnas from %s\n",argv[7]);
mrnaHash = readPslToBinKeeper(argv[3], argv[7]);
pslPseudo(argv[2], bkHash, trfHash, synHash, mrnaHash, argv[8], argv[9], argv[10], argv[11]);
//genePredReaderFree(&gprKg);
return 0;
}
