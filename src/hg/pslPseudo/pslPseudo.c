/* pslPseudo - analyse repeats and generate list of processed pseudogenes
 * from a genome wide, sorted by mRNA .psl alignment file.
 */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "memalloc.h"
#include "psl.h"
#include "bed.h"
#include "rmskOut.h"
#include "binRange.h"
#include "cheapcgi.h"

#define WORDCOUNT 3
static char const rcsid[] = "$Id: pslPseudo.c,v 1.1 2003/07/28 02:35:51 baertsch Exp $";

double minAli = 0.98;
double maxRep = 2;
double minAliPseudo = 0.85;
double nearTop = 0.01;
double minCover = 0.90;
double minCoverPseudo = 0.10;
int maxBlockGap = 60;
boolean ignoreSize = FALSE;
boolean noIntrons = FALSE;
boolean singleHit = FALSE;
boolean noHead = FALSE;
boolean quiet = FALSE;
int minNearTopSize = 10;

void usage()
/* Print usage instructions and exit. */
{
errAbort(
    "pslPseudo - analyse repeats and generate genome wide best\n"
    "alignments from a sorted set of local alignments\n"
    "usage:\n"
    "    pslPseudo in.psl sizes.lst rmskDir trf.bed syntenic.bed out.psl pseudo.psl\n\n"
    "where in.psl is an blat alignment of mrnas sorted by pslSort\n"
    "blastz.psl is an blastz alignment of mrnas sorted by pslSort\n"
    "sizes.lst is a list of chrromosome followed by size\n"
    "rmskDir is the directory containing repeat masker output file\n"
    "trf.bed is the simple repeat (trf) bed file\n"
    "out.psl is the best mrna alignment for the gene \n"
    "and pseudo.psl contains pseudogenes\n"
    "options:\n"
    "    -nohead don't add PSL header\n"
    "    -ignoreSize Will not weigh in favor of larger alignments so much\n"
    "    -noIntrons Will not penalize for not having introns when calculating\n"
    "              size factor\n"
    "    -singleHit  Takes single best hit, not splitting into parts\n"
    "    -minCover=0.N minimum coverage for mrna to output.  Default is 0.90\n"
    "    -minCoverPseudo=0.N minimum coverage of pseudogene to output.  Default is 0.10\n"
    "    -minAli=0.N minimum alignment ratio for mrna\n"
    "               default is 0.98\n"
    "    -minAliPseudo=0.N minimum alignment ratio for pseudogenes\n"
    "               default is 0.85\n"
    "    -nearTop=0.N how much can deviate from top and be taken\n"
    "               default is 0.01\n"
    "    -minNearTopSize=N  Minimum size of alignment that is near top\n"
    "               for aligmnent to be kept.  Default 10.\n"
    "    -maxBlockGap=N  Max gap size between adjacent blocks that are combined. \n"
    "               Default 60.\n"
    "    -maxRep=N  max ratio of overlap with repeat masker track \n"
    "               for aligmnent to be kept.  Default 2.00\n");
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
int intronCount = 0;
int maxQGap = 2; /* max size of gap on q size to be considered not an intron */

assert (psl != NULL);
/* debug statement */
if (sameString(psl->qName, "A14103") 
        //&& (sameString(psl->tName,"chr22") )
        )
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
        e/printf("elist count %d %s s %d e %d\n",slCount(&elist), psl->tName, tsRegion, teRegion);
        if (elist != NULL)
            slFreeList(&elist);
        }
    /* don't subtract repeats if the entire region is masked */
    if ( regionReps + 10 >= teRegion - tsRegion )
        reps = 0;

    if (abs(qs - qe) <= maxQGap && ts - te - reps >= maxBlockGap ) 
        /* don't count gaps in mrna as introns */
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

void processBestMulti(char *acc, struct psl *pslList, struct hash *trfHash, struct hash *synHash, FILE *bestFile, FILE *pseudoFile, struct hash *bkHash)
/* Find psl's that are best anywhere along their length. */
{
struct psl *bestPsl = NULL, *psl;
int qSize = 0;
int *scoreTrack = NULL;
int milliScore;
int pslIx;
int goodAliCount = 0;
int bestAliCount = 0;
int milliMin = 1000*minAli;
int milliMinPseudo = 1000*minAliPseudo;
int maxIntrons = 0;
int bestStart = 0, bestEnd = 0;
struct binElement *el, *elist;
struct binKeeper *bk;
int trf = 0, rep = 0;
char *bestChrom = NULL;
struct bed *bed;
struct rmskOut *rmsk = NULL;

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
if (uglyTarget(psl)) uglyf("@ %s %s:%d milliScore %d\n", psl->qName, psl->tName, psl->tStart, milliScore);
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
if (uglyTarget(psl)) uglyf("accepted %s %s:%d maxIn = %d\n",psl->qName, psl->tName, psl->tStart , maxIntrons);
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
    else if (pslCountBlocks(bestPsl, psl, maxBlockGap) > 1 && intronFactor(psl, bkHash) == 0 && 
        maxIntrons > 0 && bestAliCount > 0 && bestChrom != NULL &&
        (calcMilliScore(psl) >= milliMinPseudo && 
        psl->match + psl->repMatch >= minCoverPseudo * psl->qSize))
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
        if (trf > 20)
            continue;
        if (bkHash != NULL)
            {
            bk = hashFindVal(bkHash, psl->tName);
            elist = binKeeperFindSorted(bk, psl->tStart , psl->tEnd ) ;
            rep = 0;
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
                    rep += positiveRangeIntersection(psl->tStart, psl->tEnd, rmsk->genoStart, rmsk->genoEnd);
                    }
                }
            slFreeList(&elist);
            }
        /* skip pseudogenes that overlap repeasts more than maxRep% */
        if ((float)rep/(float)(psl->tEnd-psl->tStart) > maxRep )
            continue;
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
            /* blat sometimes overlaps parts of the same mrna , filter these */
            if ( positiveRangeIntersection(bestStart, bestEnd, psl->tStart, psl->tEnd) && 
                        sameString(psl->tName, bestChrom))
                overlapDiagonal = 999;
            if (overlapDiagonal == 0)
                {
                printf("YES %s %d rep ratio %f rep %d len %d %s intronFac %d maxI %d bestAli %d pslCountBlk %d\n",
                    psl->qName,psl->tStart,((float)rep/(float)(psl->tEnd-psl->tStart) ),rep, 
                    psl->tEnd-psl->tStart,psl->tName, intronFactor(psl, bkHash), 
                    maxIntrons , bestAliCount, pslCountBlocks(bestPsl, psl, maxBlockGap));
                if (rmsk != NULL)
                    printf("%s",rmsk->genoName);
                printf("\n");
                if (uglyTarget(psl)) uglyf("pseudo %s:%d maxIntron = %d introns = %d score = %d cover = %d\n",
                        psl->tName, psl->tStart, maxIntrons, intronFactor(psl, bkHash), calcMilliScore(psl), 
                        psl->match + psl->repMatch);
                pslTabOut(psl, pseudoFile);
                }
        }
        else 
        {
            printf("NO. %s %d rep ratio %f rep %d len %d %s intronFac %d maxI %d bestAli %d pslCountBlk %d\n",
                    psl->qName,psl->tStart,((float)rep/(float)(psl->tEnd-psl->tStart) ),rep, 
                    psl->tEnd-psl->tStart,psl->tName, intronFactor(psl, bkHash), 
                    maxIntrons , bestAliCount, pslCountBlocks(bestPsl, psl, maxBlockGap));
        }
    }
freeMem(scoreTrack);
}

void processBestSingle(char *acc, struct psl *pslList, struct hash *trfHash, struct hash *synHash, FILE *bestFile, FILE *repFile, struct hash *bkHash)
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

void doOneAcc(char *acc, struct psl *pslList, struct hash *trfHash, struct hash *synHash, FILE *bestFile, FILE *repFile, struct hash *bkHash)
/* Process alignments of one piece of mRNA. */
{
if (singleHit)
    processBestSingle(acc, pslList, trfHash, synHash, bestFile, repFile, bkHash);
else
    processBestMulti(acc, pslList, trfHash, synHash, bestFile, repFile, bkHash);
}

void pslPseudo(char *inName, struct hash *bkHash, struct hash *trfHash, struct hash *synHash, char *bestAliName, char *repName)
/* Analyse inName and put best alignments for eacmRNA in estAliName.
 * Put repeat info in repName. */
{
struct lineFile *in = pslFileOpen(inName);
FILE *bestFile = mustOpen(bestAliName, "w");
FILE *repFile = mustOpen(repName, "w");
int lineSize;
char *line;
char *words[32];
int wordCount;
struct psl *pslList = NULL, *psl;
char lastName[256];
int aliCount = 0;
quiet = sameString(bestAliName, "stdout") || sameString(repName, "stdout");

if (!quiet)
    printf("Processing %s to %s and %s\n", inName, bestAliName, repName);
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
	doOneAcc(lastName, pslList, trfHash, synHash, bestFile, repFile, bkHash);
	pslFreeList(&pslList);
	safef(lastName, sizeof(lastName), psl->qName);
	}
    slAddHead(&pslList, psl);
    }
doOneAcc(lastName, pslList, trfHash, synHash, bestFile, repFile, bkHash);
pslFreeList(&pslList);
lineFileClose(&in);
fclose(bestFile);
fclose(repFile);
if (!quiet)
    printf("Processed %d alignments\n", aliCount);
}

int main(int argc, char *argv[])
/* Process command line. */
{
struct hash *bkHash = NULL, *trfHash = NULL, *synHash = NULL;
//struct lineFile *lf = NULL;
//struct bed *syntenicList = NULL, *bed;
//char  *row[3];

cgiSpoof(&argc, argv);
if (argc != 8)
    usage();
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
singleHit = cgiBoolean("singleHit");
noHead = cgiBoolean("nohead");
printf("Loading Syntenic Bed %s\n",argv[4]);
synHash = readBedToBinKeeper(argv[2], argv[4], WORDCOUNT);
printf("Loading Trf Bed %s\n",argv[5]);
trfHash = readBedToBinKeeper(argv[2], argv[5], WORDCOUNT);
printf("Reading Repeats from %s\n",argv[3]);
bkHash = readRepeatsAll(argv[2], argv[3]);
printf("Scoring alignments from %s\n",argv[1]);
pslPseudo(argv[1], bkHash, trfHash, synHash, argv[6], argv[7]);
return 0;
}
