/* pslPseudo - analyse repeats and generate list of processed pseudogenes
 * from a genome wide, sorted by mRNA .psl alignment file.
 */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "memalloc.h"
#include "jksql.h"
#include "hdb.h"
#include "psl.h"
#include "bed.h"
#include "dnaseq.h"
#include "nib.h"
#include "rmskOut.h"
#include "binRange.h"
#include "cheapcgi.h"
#include "genePred.h"

#define WORDCOUNT 3
#define POLYASLIDINGWINDOW 10
#define POLYAREGION 160

static char const rcsid[] = "$Id: pslPseudo.c,v 1.4 2003/11/06 00:20:44 baertsch Exp $";

double minAli = 0.98;
double maxRep = 0.35;
double minAliPseudo = 0.70;
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
    "    pslPseudo db in.psl sizes.lst rmskDir trf.bed syntenic.bed mrna.psl out.psl pseudo.psl pseudoLink.txt nibDir refGene.tab gene.tab\n\n"
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
    "               default is 0.70\n"
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

int intronFactor(struct psl *psl, struct hash *bkHash)
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
/* debug statement */
if (!quiet && (psl->tStart == 15781385 && (sameString(psl->tName,"chr22") )))
    printf(".") ;
if (blockCount <= 1)
    return 0;
sz = psl->blockSizes[0];
qe = psl->qStarts[0] + sz;
te = psl->tStarts[0] + sz;
tsRegion = psl->tStarts[0];
for (i=1; i<blockCount; ++i)
    {
    int reps = 0, regionReps = 0;
    struct binElement *el, *elist;
    struct rmskOut *rmsk;
    qs = psl->qStarts[i];
    ts = psl->tStarts[i];
    teRegion = psl->tStarts[i] + psl->blockSizes[i];

    if (bkHash != NULL)
        {
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
        //printf("elist count %d %s s %d e %d\n",slCount(&elist), psl->tName, tsRegion, teRegion);
        if (elist != NULL)
            slFreeList(&elist);
        }
    /* don't subtract repeats if the entire region is masked */
    if ( regionReps + 10 >= teRegion - tsRegion )
        reps = 0;

    tGap = ts - te - reps;
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

int sizeFactor(struct psl *psl, struct hash *bkHash)
/* Return a factor that will favor longer alignments. An intron is worth 3 bases...  */
{
int score;
if (ignoreSize) return 0;
assert(psl != NULL);
score = 4*round(sqrt(psl->match + psl->repMatch/4));
if (!noIntrons)
    {
    int bonus = intronFactor(psl, bkHash) * 3;
    if (bonus > 10) bonus = 10;
    score += bonus;
    }
return score;
}

int calcSizedScore(struct psl *psl, struct hash *bkHash)
/* Return score that includes base matches and size. */
{
int score = calcMilliScore(psl) + sizeFactor(psl, bkHash);
return score;
}

boolean uglyTarget(struct psl *psl)
/* Return TRUE if it's the one we're snooping on. */
{
return TRUE; //sameString(psl->qName, "NM_006905") || sameString(psl->qName, "AK");
}


boolean closeToTop(struct psl *psl, int *scoreTrack , struct hash *bkHash)
/* Returns TRUE if psl is near the top scorer for at least 20 bases. */
{
int milliScore = calcSizedScore(psl, bkHash);
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

int polyACalc(int start, int end, char *strand, char *nibDir, char *chrom, int winSize, int region, int *polyAstart, int *polyAend)
/* get size of polyA tail in genomic dna , count bases in a 
 * sliding winSize window in region of the end of the sequence*/
{
char nibFile[256];
FILE *f;
int seqSize;
struct dnaSeq *seq = NULL;
int count = 0, count_old = 0;
int seqStart = strand[0] == '+' ? end-(region/2) : start-(region/2);
int score[POLYAREGION+1], pStart = 0, pEnd = 0; 
int cSize = hChromSize(chrom);

*polyAstart = 0 , *polyAend = 0;
safef(nibFile, sizeof(nibFile), "%s/%s.nib", nibDir, chrom);
nibOpenVerify(nibFile, &f, &seqSize);
if (seqStart < 0) seqStart = 0;
if (seqStart + region > seqSize) region = seqSize - seqStart;
assert(seqSize <= cSize);
seq = nibLdPartMasked(NIB_MASK_MIXED, nibFile, f, seqSize, seqStart, region);
if (strand[0] == '+')
    {
//    count_old = countCharsInWindow('A',seq->dna,seq->size, winSize, &pStart);
    assert (seq->size <= POLYAREGION);
printf("\n + range=%d %d %s \n",seqStart, seqStart+region, seq->dna );
    count = scoreWindow('A',seq->dna,seq->size, score, polyAstart, polyAend);
    }
else
    {
//    count_old = countCharsInWindow('T',seq->dna,seq->size, winSize, &pStart);
    assert (seq->size <= POLYAREGION);
printf("\n - range=%d %d %s \n",seqStart, seqStart+region, seq->dna );
    count = scoreWindow('T',seq->dna,seq->size, score, polyAend, polyAstart);
    }
pStart += seqStart;
//if (count_old == winSize)
//    count_old = polyACalc(start, end, strand, nibDir, chrom, winSize*2, region*2, &pStart, &pEnd);
*polyAstart += seqStart;
*polyAend += seqStart;
printf("\nold cnt=%d st=%d %s range %d %d cnt %d\n",count_old, seqStart, seq->dna, *polyAstart, *polyAend, count);
fclose(f);
return count;
}

int pslCountIntronSpan(struct psl *target, struct psl *query, int maxBlockGap, struct hash *bkHash , int *tReps, int *qReps)
/* count the number of blocks in the query that overlap the target */
/* merge blocks that are closer than maxBlockGap */
{
int i, j, start = 0, qs, qqstart = 0 , qqs;
int count = 0;
struct binKeeper *bk = NULL;
struct binElement *elist = NULL, *el = NULL;
struct rmskOut *rmsk = NULL;

*tReps = 0; *qReps = 0;
/* debug statement */
if (target == NULL || query == NULL)
    return 0;

for (i = 0 ; i < target->blockCount ; i++)
  {
    int qe = target->qStarts[i] + target->blockSizes[i];
    int ts = target->tStarts[i] ;
    int te = target->tStarts[i] + target->blockSizes[i];
    qs = target->qStarts[start];
    /* combine blocks that are close together */
    if (i < (target->blockCount) -1)
        {
        /* skip exons that overlap repeats */
        if (bkHash != NULL)
            {
            bk = hashFindVal(bkHash, target->tName);
            elist = binKeeperFindSorted(bk, ts, te) ;
            for (el = elist; el != NULL ; el = el->next)
                {
                assert (el->val != NULL);
                rmsk = el->val;
                assert(rmsk != NULL);
                *tReps += positiveRangeIntersection(ts, te, rmsk->genoStart, rmsk->genoEnd);
                }
            slFreeList(&elist);
            if (2 * *tReps > (te-ts))
                continue;
            }

        //if (positiveRangeIntersection(ts, te, qs, qe) > min(20,qe-qs))
        if ((target->tStarts[i] + target->blockSizes[i] + maxBlockGap) > target->tStarts[i+1])   
            {
            continue;
            }
        }
    if (target->strand[0] == '-')
        {
        int temp = target->qSize - qe;
        qe = target->qSize - qs;
        qs = temp;
        }
    if (qs < 0 || qe > target->qSize)
        {
        warn("Odd: qName %s tName %s qSize %d psl->qSize %d start %d end %d",
            target->qName, target->tName, target->qSize, target->qSize, qs, qe);
        if (qs < 0)
            qs = 0;
        if (qe > target->qSize)
            qe = target->qSize;
        }
    if (positiveRangeIntersection(query->qStart, query->qEnd, qs, qe) > min(20,qe-qs))
        {
        for (j = 0 ; j < query->blockCount ; j++)
            {
            int qqe = query->qStarts[j] + query->blockSizes[j];
            int localReps = 0;
            qqs = query->qStarts[qqstart];
            /* mask repeats */
            bk = hashFindVal(bkHash, query->tName);
            if (j < (query->blockCount) -1)
                {
                elist = binKeeperFindSorted(bk, query->tStarts[j], query->tStarts[j+1]) ;
                for (el = elist; el != NULL ; el = el->next)
                    {
                    assert (el->val != NULL);
                    rmsk = el->val;
                    assert(rmsk != NULL);
                    localReps += positiveRangeIntersection(query->tStarts[j], query->tStarts[j+1], rmsk->genoStart, rmsk->genoEnd);
                    }
                *qReps += localReps;
                slFreeList(&elist);
                /* join together blocks that are close together */
                    if ((query->tStarts[j] + query->blockSizes[j] + maxBlockGap + localReps) > query->tStarts[j+1])   
                        {
                        continue;
                        }
      //          if (query->strand[0] == '-')
      //              {
      //              int temp = query->qSize - qqe;
      //              qqe = query->qSize - qqs;
      //              qqs = temp;
      //              }
                }
            if (query->blockSizes[j] > (2 * localReps) )
                {
                if (positiveRangeIntersection(qqs, qqe, qs, qe) > 10)
                    count++;
                }
            qqstart = j + 1;
            }
        }
    start = i+1;
    }
return count;
}
void processBestMulti(char *acc, struct psl *pslList, struct hash *trfHash, struct hash *synHash, struct hash *mrnaHash, struct hash *bkHash, char *nibDir)
/* Find psl's that are best anywhere along their length. */

{
struct psl *bestPsl = NULL, *psl;
struct genePred *gp = NULL, *kg = NULL;
int geneOverlap;
int qSize = 0;
int *scoreTrack = NULL;
int milliScore;
int pslIx, blockIx;
int goodAliCount = 0;
int bestAliCount = 0;
int milliMin = 1000*minAli;
int milliMinPseudo = 1000*minAliPseudo;
int maxIntrons = 0;
int bestStart = 0, bestEnd = 0;
int polyA = 0;
int polyAstart = 0;
int polyAend = 0;
struct binElement *el, *elist;
struct binKeeper *bk;
int trf = 0, rep = 0;
char *bestChrom = NULL;
struct bed *bed;
struct rmskOut *rmsk = NULL;
int tReps , qReps;
int i ;

if (pslList == NULL)
    return;

if (sameString(pslList->qName, "AK000045"))
    printf(".");

/* get biggest mrna - some have polyA tail stripped off */
for (psl = pslList; psl != NULL; psl = psl->next)
    if (psl->qSize > qSize)
        qSize = psl->qSize;

AllocArray(scoreTrack, qSize+1);

for (psl = pslList; psl != NULL; psl = psl->next)
    {
    int blockIx;
    char strand = psl->strand[0];

    assert (psl!= NULL);
    milliScore = calcMilliScore(psl);
    if (milliScore >= milliMin)
	{
	++goodAliCount;
	milliScore += sizeFactor(psl, bkHash);
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
for (psl = pslList; psl != NULL; psl = psl->next)
    {
    int intronCount = 0;
    if (
        calcMilliScore(psl) >= milliMin && closeToTop(psl, scoreTrack, bkHash)
        && psl->match + psl->repMatch >= minCover * psl->qSize)
	{
        ++bestAliCount;
        bestPsl = psl;
        bestStart = psl->tStart;
        bestEnd = psl->tEnd;
        bestChrom = cloneString(psl->tName);
        intronCount = intronFactor(psl, bkHash);
        if (intronCount > maxIntrons )
            maxIntrons = intronCount;
	}
//if (uglyTarget(psl)) uglyf("accepted %s %s:%d maxIn = %d\n",psl->qName, psl->tName, psl->tStart , maxIntrons);
    }
/* output pseudogenes, if alignments have no introns and mrna alignmed with introns */
if (pslList != NULL)
    for (psl = pslList; psl != NULL; psl = psl->next)
    {
    if (
        calcMilliScore(psl) >= milliMin && closeToTop(psl, scoreTrack, bkHash)
        && psl->match + psl->repMatch >= minCover * psl->qSize)
            {
            pslTabOut(psl, bestFile);
            }
    else 
        {
        if (intronFactor(psl, bkHash) == 0 && 
            maxIntrons > 0 && bestAliCount > 0 && bestChrom != NULL &&
            (calcMilliScore(psl) >= milliMinPseudo && 
            psl->match + psl->repMatch >= minCoverPseudo * (float)psl->qSize))
            {
            /* count # of alignments that span introns */
            int intronSpan = pslCountIntronSpan(bestPsl, psl, maxBlockGap, bkHash, &tReps, &qReps) ;

            if (intronSpan > 1 && intronFactor(psl, bkHash) == 0 && 
                maxIntrons > 0 && bestAliCount > 0 && bestChrom != NULL &&
                (calcMilliScore(psl) >= milliMinPseudo && 
                psl->match + psl->repMatch >= minCoverPseudo * (float)psl->qSize))
                {
                int overlapDiagonal = 0;
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
            //                    printf("%s %d %d ",psl->tName, psl->tStart,psl->tEnd );
            //                    printf("%s %d %d\n",rmsk->genoName, rmsk->genoStart, rmsk->genoEnd);
                                rep += positiveRangeIntersection(ts, te, rmsk->genoStart, rmsk->genoEnd);
                                }
                            }
                        slFreeList(&elist);
                        }
                    }
                if ((float)rep/(float)(psl->match+(psl->misMatch)) > maxRep )
                    continue;


                /* skip pseudogenes that overlap the syntenic diagonal with another species */
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
                    slFreeList(&elist);
                    }

                /* count good mrna overlap with pseudogenes. If name matches then don't filter it.*/
                if (mrnaHash != NULL)
                    {
                    int mrnaBases = 0;
                    struct psl *mPsl = NULL;
                    bk = hashFindVal(mrnaHash, psl->tName);
                    elist = binKeeperFindSorted(bk, psl->tStart , psl->tEnd ) ;
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
                            if (intronFactor(mPsl, bkHash) > 0 )
                                for (blockIx = 0; blockIx < mPsl->blockCount; ++blockIx)
                                    {
                                    mrnaBases += positiveRangeIntersection(psl->tStart, psl->tEnd, 
                                        mPsl->tStarts[blockIx], mPsl->tStarts[blockIx]+mPsl->blockSizes[blockIx]);
                                    }
                            }
                        }
                    slFreeList(&elist);
                    if (mrnaBases > 20 )
                        {
                        if (!quiet)
                            printf("NO %s better blat mrna %s \n",psl->qName,mPsl->qName);
                        fprintf(linkFile,"%s %d hg15 better %s %s %d %d %d %d 0 %s %d %d %s %d %d %s -1 %d %d %d %d %d %d %d %d\n",
                                psl->qName, calcMilliScore(psl), mPsl->qName, mPsl->tName, 
                                mPsl->tStart, mPsl->tEnd, 
                                maxIntrons, geneOverlap, bestPsl->strand, polyA, polyAstart, 
                                psl->tName, psl->tStart, psl->tEnd, psl->strand,
                                intronSpan, intronFactor(psl, bkHash), bestAliCount, psl->match+psl->repMatch, psl->qSize, tReps, qReps, overlapDiagonal); 
                        continue;
                        }
                    }


                    /* blat sometimes overlaps parts of the same mrna , filter these */
                    if ( positiveRangeIntersection(bestStart, bestEnd, psl->tStart, psl->tEnd) && 
                                sameString(psl->tName, bestChrom))
                        overlapDiagonal = 999;
                    if (overlapDiagonal != 0)
                        {
                        if (!quiet)
                            printf("NO. %s %d diag %s %d  bestChrom %s\n",psl->qName, overlapDiagonal, psl->tName, psl->tStart, bestChrom);
                        fprintf(linkFile,"%s %d hg15 diagonal %s %s %d %d %d %d 0 %s %d %d %s %d %d %s 1 %d %d %d %d %d %d %d %d\n",
                                psl->qName, calcMilliScore(psl), bestPsl->qName, bestPsl->tName, 
                                bestPsl->tStart, bestPsl->tEnd, 
                                maxIntrons, geneOverlap, bestPsl->strand, polyA, polyAstart, 
                                psl->tName, psl->tStart, psl->tEnd, psl->strand,
                                intronSpan, intronFactor(psl, bkHash), bestAliCount, psl->match+psl->repMatch, psl->qSize, tReps, qReps, overlapDiagonal); 
                        }
                    else
                        {
                        polyA = polyACalc(psl->tStart, psl->tEnd, psl->strand, nibDir, psl->tName, 
                                POLYASLIDINGWINDOW, POLYAREGION, &polyAstart, &polyAend);
                        if (!quiet)
                            {
                            printf("YES %s %d rr %3.1f rl %d ln %d %s iF %d maxI %d bestAli %d pCB %d score %d match %d cover %3.1f rp %d polyA %d syn %d",
                                psl->qName,psl->tStart,((float)rep/(float)(psl->tEnd-psl->tStart) ),rep, 
                                psl->tEnd-psl->tStart,psl->tName, intronFactor(psl, bkHash), 
                                maxIntrons , bestAliCount, intronSpan,
                                calcMilliScore(psl),  psl->match + psl->repMatch , 
                                minCoverPseudo * (float)psl->qSize, tReps + qReps, polyA, overlapDiagonal );
                            if (rmsk != NULL)
                                printf(" rmsk %s",rmsk->genoName);
                            printf("\n");
                            }
                        pslTabOut(psl, pseudoFile);
                        geneOverlap = 0;
                        kg = getOverlappingGene(&kgList, "knownGene", bestPsl->tName, bestPsl->tStart, 
                                bestPsl->tEnd , &geneOverlap);
                        if (kg != NULL)
                            {
                            fprintf(linkFile,"%s %d hg15 knownGene %s %s %d %d %d %d 0 %s %d %d %s %d %d %s 1 %d %d %d %d %d %d %d %d\n",
                                    psl->qName, calcMilliScore(psl), kg->name, bestPsl->tName, 
                                    bestPsl->tStart, bestPsl->tEnd, 
                                    maxIntrons, geneOverlap, kg->strand, polyA, polyAstart, 
                                    psl->tName, psl->tStart, psl->tEnd, psl->strand, 
                                    intronSpan, intronFactor(psl, bkHash), bestAliCount, psl->match+psl->repMatch, psl->qSize, tReps, qReps, overlapDiagonal); 
                            }
                        else 
                            {
                            gp = getOverlappingGene(&gpList1, "refGene", bestPsl->tName, bestPsl->tStart, 
                                    bestPsl->tEnd , &geneOverlap);
                            if (gp != NULL)
                                {
                                fprintf(linkFile,"%s %d hg15 refGene %s %s %d %d %d %d 0 %s %d %d %s %d %d %s 1 %d %d %d %d %d %d %d %d\n",
                                        psl->qName, calcMilliScore(psl), gp->name, bestPsl->tName, 
                                        bestPsl->tStart, bestPsl->tEnd, 
                                        maxIntrons, geneOverlap, gp->strand, polyA, polyAstart, 
                                        psl->tName, psl->tStart, psl->tEnd, psl->strand,
                                        intronSpan, intronFactor(psl, bkHash), bestAliCount, psl->match+psl->repMatch, psl->qSize, tReps, qReps, overlapDiagonal); 
                                }
                            else 
                                {
                                gp = getOverlappingGene(&gpList2, "mgcGenes", bestPsl->tName, bestPsl->tStart, 
                                        bestPsl->tEnd , &geneOverlap);
                                if (gp != NULL)
                                    {
                                    fprintf(linkFile,"%s %d hg15 mgcGenes %s %s %d %d %d %d 0 %s %d %d %s %d %d %s 1 %d %d %d %d %d %d %d %d\n",
                                            psl->qName, calcMilliScore(psl), gp->name, bestPsl->tName, 
                                            bestPsl->tStart, bestPsl->tEnd, 
                                            maxIntrons, geneOverlap, gp->strand, polyA, polyAstart, 
                                            psl->tName, psl->tStart, psl->tEnd, psl->strand,
                                            intronSpan, intronFactor(psl, bkHash), bestAliCount, psl->match+psl->repMatch, psl->qSize, tReps, qReps, overlapDiagonal); 
                                    }
                                }
                            if (gp == NULL)
                                {
                                fprintf(linkFile,"%s %d hg15 mrna %s %s %d %d %d %d 0 %s %d %d %s %d %d %s 1 %d %d %d %d %d %d %d %d\n",
                                        psl->qName, calcMilliScore(psl), bestPsl->qName, bestPsl->tName, 
                                        bestPsl->tStart, bestPsl->tEnd, 
                                        maxIntrons, geneOverlap, bestPsl->strand, polyA, polyAstart, 
                                        psl->tName, psl->tStart, psl->tEnd, psl->strand,
                                        intronSpan, intronFactor(psl, bkHash), bestAliCount, psl->match+psl->repMatch, psl->qSize, tReps, qReps, overlapDiagonal); 
                                }
                            }
                        }
                }
                else
                {
                    if (!quiet)
                        printf("NO. %s %d rr %3.1f rl %d ln %d %s iF %d maxI %d bestAli %d pCB %d score %d match %d cover %3.1f rp %d\n",
                            psl->qName,psl->tStart,((float)rep/(float)(psl->tEnd-psl->tStart) ),rep, 
                            psl->tEnd-psl->tStart,psl->tName, intronFactor(psl, bkHash), maxIntrons , 
                            bestAliCount, intronSpan,
                            calcMilliScore(psl),  psl->match + psl->repMatch , 
                            minCoverPseudo * (float)psl->qSize, tReps + qReps);
                    fprintf(linkFile,"%s %d hg15 span %s %s %d %d %d %d 0 %s %d %d %s %d %d %s -1 %d %d %d %d %d %d %d %d\n",
                            psl->qName, calcMilliScore(psl), bestPsl->qName, bestPsl->tName, 
                            bestPsl->tStart, bestPsl->tEnd, 
                            maxIntrons, geneOverlap, bestPsl->strand, polyA, polyAstart, 
                            psl->tName, psl->tStart, psl->tEnd, psl->strand,
                            intronSpan, intronFactor(psl, bkHash), bestAliCount, psl->match+psl->repMatch, psl->qSize, tReps, qReps, -1); 
                }
            }
            else 
            {
                int intronSpan = pslCountIntronSpan(bestPsl, psl, maxBlockGap, bkHash, &tReps, &qReps) ;
                if (!quiet)
                    printf("NO. %s %d rr %3.1f rl %d ln %d %s iF %d maxI %d bestAli %d pCB na score %d match %d cover %3.1f rp %d\n",
                        psl->qName,psl->tStart,((float)rep/(float)(psl->tEnd-psl->tStart) ),rep, 
                        psl->tEnd-psl->tStart,psl->tName, intronFactor(psl, bkHash), maxIntrons , bestAliCount,
                        calcMilliScore(psl),  psl->match + psl->repMatch , minCoverPseudo * (float)psl->qSize, tReps + qReps);
                if (bestPsl != NULL)
                    fprintf(linkFile,"%s %d hg15 introns %s %s %d %d %d %d 0 %s %d %d %s %d %d %s -1 %d %d %d %d %d %d %d %d\n",
                        psl->qName, calcMilliScore(psl), bestPsl->qName, bestPsl->tName, 
                        bestPsl->tStart, bestPsl->tEnd, 
                        maxIntrons, geneOverlap, bestPsl->strand, polyA, polyAstart, 
                        psl->tName, psl->tStart, psl->tEnd, psl->strand,
                        intronSpan, intronFactor(psl, bkHash), bestAliCount, psl->match+psl->repMatch, psl->qSize, tReps, qReps, -1); 
                else
                    fprintf(linkFile,"%s %d hg15 introns %s %s %d %d %d %d 0 %s %d %d %s %d %d %s -1 %d %d %d %d %d %d %d %d\n",
                        psl->qName, calcMilliScore(psl), "none", "none", 
                        -1, -1, 
                        maxIntrons, geneOverlap, " ", polyA, polyAstart, 
                        psl->tName, psl->tStart, psl->tEnd, psl->strand,
                        intronSpan, intronFactor(psl, bkHash), bestAliCount, psl->match+psl->repMatch, psl->qSize, tReps, qReps, -1); 
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

cgiSpoof(&argc, argv);
if (argc != 14)
    usage();
hSetDb(argv[1]);
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
if (!quiet)
    printf("Scoring alignments from %s.\n",argv[2]);
gpList1 = genePredLoadAll(argv[12]);
gpList2 = genePredLoadAll(argv[13]);
pslPseudo(argv[2], bkHash, trfHash, synHash, mrnaHash, argv[8], argv[9], argv[10], argv[11]);
return 0;
}
