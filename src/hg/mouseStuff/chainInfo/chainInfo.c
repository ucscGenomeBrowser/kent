#include "common.h"
#include "hCommon.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
//#include "jksql.h"
#include "hdb.h"
#include "dnautil.h"
#include "chain.h"
#include "axt.h"
#include "axtInfo.h"
#include "rmskOut.h"
#include "binRange.h"
#include "obscure.h"
#include "chainInfo.h"
#include "genePred.h"

int minGap = 40;
float minOverlap = 0.90;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "chainInfo - parse chains to determine number of introns on query and target\n"
  "usage:\n"
  "   chainInfo target_db target_chrom chainFile target.lst query.lst target_repeatMasker.out query_repeatDir output refGene.tab genePred2.tab genePred3.tab\n"
  "options:\n"
    "    -minGap=N minimum size to be considered a gap.  Default is 40.\n"
    "    -minOverlap=0.N threshold for repeat overlap with gap to be considered a repeat instead of intron.  Default is 0.90.\n"
  "db is target database to match genes to pseudogene\n"
  "chrom is target chromosome to match genes to pseudogene\n"
  "query_repeatDir is the dirctory containing repeatMasker out files chrXX.fa.out\n"
  "target.lst, query.lst is a list of chromosome names followed by size of each chromosome\n"
  );
}

int qStart(struct chain *chain)
    /* convert coordinates */
{
    return (chain->qStrand == '+') ? chain->qStart : chain->qSize-chain->qEnd;
}
int qEnd(struct chain *chain)
    /* convert coordinates */
{
    return (chain->qStrand == '+') ? chain->qEnd : chain->qSize-chain->qStart ;
}
int qStartBlk(struct chain *chain, int start, int end)
    /* convert coordinates */
{
    return (chain->qStrand == '+') ? start : chain->qSize-end;
}
int qEndBlk(struct chain *chain, int start, int end)
    /* convert coordinates */
{
    return (chain->qStrand == '+') ? end : (chain->qSize)-start ;
}

int findSize(struct hash *hash, char *name)
/* Find size of name in hash or die trying. */
{
void *val = hashMustFindVal(hash, name);
return ptToInt(val);
}

struct hash *readSizes(char *fileName)
/* Read tab-separated file into hash with
 * name key size value. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct hash *hash = newHash(0);
char *row[2];
while (lineFileRow(lf, row))
    {
    char *name = row[0];
    int size = lineFileNeedNum(lf, row, 1);
    if (hashLookup(hash, name) != NULL)
        warn("Duplicate %s, ignoring all but first\n", name);
    else
	hashAdd(hash, name, intToPt(size));
    }
lineFileClose(&lf);
return hash;
}

struct intronChain *scoreNextAlignedIntron(struct chain *chain, FILE *f, struct binKeeper *bk, struct hash *bkHash)
/* get next chain from sorted list.  Return NULL at end of list.  attempt to chain chains together if possible
 * count introns on query and target side , blocks with < minGap bases between will be combined. 
 * Note that chain block scores are not filled in by this . 
 * repeats are filtered out and are passed in the bk structure 
 * score 2 = ratio of intron target/query;  score1 = score2 * ratio bases in target/query gaps */
{
char *row[13];
int wordCount;
int q = qStart(chain),t = chain->tStart;
static int id = 0;
struct binElement *el;
struct rmskOut *rmsk ;
struct intronChain *iChain;
struct boxIn *b, *nextB;
int tGapStart = 0;
int qGapStart = 0;
int tGapEnd = 0;
int qGapEnd = 0, tg,qg;
int tReps, qReps, percentOverlap, percentQOverlap;
struct binKeeper *bkQ;


AllocVar(iChain);
iChain->chain = chain;
iChain->qIntron = 0;         iChain->tIntron = 0;          
iChain->tGap = 0;              iChain->qGap = 0;               
iChain->tReps = 0;             iChain->qReps = 0;             iChain->total = 0;                  

for (b = chain->blockList; b != NULL; b = nextB)
    {
    nextB = b->next;
    if (nextB == NULL)
        break;
    tGapStart = b->tEnd;
    tGapEnd = nextB->tStart;
    if (chain->qStrand == '+')
        { qGapStart = b->qEnd;
        qGapEnd = nextB->qStart; }
    else
        { qGapStart = chain->qSize-nextB->qStart;
        qGapEnd = chain->qSize-b->qEnd; }
    tg = tGapEnd - tGapStart;
    qg = qGapEnd - qGapStart;
    t += tg;
    q += qg;

    /* remove 'introns' that are actually repeats on target*/
    tReps = 0;
    for (el = binKeeperFindSorted(bk, tGapStart, tGapEnd); el != NULL; el = el->next)
        {
        rmsk = el->val;
        tReps = positiveRangeIntersection(tGapStart, tGapEnd, rmsk->genoStart, rmsk->genoEnd);
        }
    percentOverlap = (float)tReps / (float)(tGapEnd - tGapStart);
    if (((tGapEnd - tGapStart) > 0) && percentOverlap > minOverlap)
        {
        if (tReps > tg) tReps=tg;
        tg -= tReps;
        if (tg < 0)
            assert(tg >=0);
        iChain->tReps += tReps;
        }

    /* get bk for query */
    bkQ = hashFindVal(bkHash, chain->qName);
    qReps = 0;
    for (el = binKeeperFindSorted(bkQ, qGapStart, qGapEnd); el != NULL; el = el->next)
        {
        rmsk = el->val;
        qReps = positiveRangeIntersection(qGapStart, qGapEnd, rmsk->genoStart, rmsk->genoEnd);
        if ((qEnd(chain) == 116409111) && sameString(chain->qName, "chr11"))
            printf("\n %s %d %d %d %d %d %s %4.1f", chain->qName, qGapStart, qGapEnd, qReps , rmsk->genoStart, rmsk->genoEnd, rmsk->repClass, (float)(rmsk->milliDiv)/10);
        }
    percentQOverlap = (float)qReps / (float)(qGapEnd - qGapStart);
    if (((qGapEnd - qGapStart) > 0) && percentQOverlap > minOverlap)
        {
        if (qReps > qg) qReps=qg;
        qg -= qReps;
        if (qg < 0)
            assert(qg >= 0);
        iChain->qReps += qReps;
        }



/*    if (chain->qStrand == '+')
 *      printf("\n---%d %d %3.2f %d gap=%s:%d-%d q=%s:%d-%d %d",tg, tReps , percentOverlap, tGapEnd - tGapStart,  chain->tName, 
 *              tGapStart, tGapEnd, chain->qName, b->qStart, b->qEnd , chain->id);
 *  else
 *      printf("\n---%d %d %3.2f %d gap=%s:%d-%d q=%s:%d-%d %d",tg, tReps , percentOverlap, tGapEnd - tGapStart,  chain->tName, 
 *              tGapStart, tGapEnd, chain->qName, chain->qSize-b->qEnd, chain->qSize-b->qStart , chain->id);
 */

    iChain->tGap += tg;
    if(iChain->tGap <0)
        assert(iChain->tGap >= 0);
    iChain->qGap += qg;
    if(iChain->qGap <0)
        assert(iChain->qGap >= 0);
    if (tg >= minGap)
        iChain->tIntron++;
    if (qg >= minGap)
        iChain->qIntron++;
    }
/*
if (q != chain->qEnd)
    errAbort("q end mismatch %d vs %d in chain %d \n", 
    	q, chain->qEnd, chain->id);
if (t != chain->tEnd)
    errAbort("t end mismatch %d vs %d in chain %d \n", 
    	t, chain->tEnd,chain->id );
        */
//slReverse(&chain->blockList);

assert(iChain->tIntron>=0);
assert(iChain->qIntron>=0);

iChain->score2 = iChain->tIntron > iChain->qIntron ? (iChain->tIntron+1.0)/(iChain->qIntron+1.0) : -(iChain->qIntron+1.0)/(iChain->tIntron+1.0) ;
iChain->score1 = (iChain->tIntron > iChain->qIntron) ? ((iChain->tGap+1)/(iChain->qGap+1)) : -(iChain->qGap+1)/(iChain->tGap+1);
return iChain;
}
struct binKeeper *readRepeats(char *chrom, char *rmskFileName, struct hash *tSizeHash)
/* read all repeats for a chromosome of size size, returns results in binKeeper structure for fast query*/
{
    boolean rmskRet;
    struct lineFile *rmskF = NULL;
    struct rmskOut *rmsk;
    struct binKeeper *bk; 
    int size;

    size = findSize(tSizeHash, chrom);
    bk = binKeeperNew(0, size);
    assert(size > 1);
    rmskOutOpenVerify(rmskFileName ,&rmskF , &rmskRet);
    while ((rmsk = rmskOutReadNext(rmskF)) != NULL)
        {
        binKeeperAdd(bk, rmsk->genoStart, rmsk->genoEnd, rmsk);
        }
    lineFileClose(&rmskF);
    return bk;
}

struct hash *readRepeatsAll(char *chrom, char *sizeFileName, char *rmskDir)
/* read all repeats for a all chromosome of size size, returns results in hash of binKeeper structure for fast query*/
{
boolean rmskRet;
struct binKeeper *bk; 
int size;
struct lineFile *rmskF = NULL;
struct rmskOut *rmsk;
struct lineFile *lf = lineFileOpen(sizeFileName, TRUE);
struct hash *hash = newHash(0);
char *row[2];
char rmskFileName[256];

while (lineFileRow(lf, row))
    {
    char *name = row[0];
    int size = lineFileNeedNum(lf, row, 1);
//    char *repSubDir = cloneString(skipChr(name));
//    chopSuffixAt(repSubDir,'_');

    if (hashLookup(hash, name) != NULL)
        warn("Duplicate %s, ignoring all but first\n", name);
    else
        {
        bk = binKeeperNew(0, size);
        assert(size > 1);
        sprintf(rmskFileName, "%s/%s.fa.out",rmskDir,name);
        rmskOutOpenVerify(rmskFileName ,&rmskF , &rmskRet);
        while ((rmsk = rmskOutReadNext(rmskF)) != NULL)
            {
            binKeeperAdd(bk, rmsk->genoStart, rmsk->genoEnd, rmsk);
            }
        lineFileClose(&rmskF);
	hashAdd(hash, name, bk);
        }
    }
lineFileClose(&lf);
return hash;
}

struct chain *readSortChains(char *fileName)
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct chain *chainList = NULL, *chain;

while ((chain = chainRead(lf)) != NULL)
    {
    slAddHead(&chainList, chain);
    }
lineFileClose(&lf);

/* Sort. */
printf("Sorting Chains\n");
slSort(&chainList, chainCmpQuery);
return chainList;
}


void scoreChains(struct chain *chainList, char *db, FILE *f, struct binKeeper *bk, struct hash *bkHash, struct genePred *gpList1, struct genePred *gpList2, struct genePred *gpList3)
/* score all chains in a chain file based on ration of introns and gaps  , mask repeats first */
/* return best overlapping gene in the target sequence */
{
    struct chain *chain;
    struct intronChain *iChain, *iChainList = NULL;
    struct genePred *gp = NULL, *vg;

    hSetDb(db);
    for (chain = chainList; chain != NULL; chain = chain->next)
        {
        iChain = scoreNextAlignedIntron(chain,f, bk, bkHash);
        slAddHead(&iChainList, iChain);
        chain = iChain->chain;
        gp = getOverlappingGene(&gpList1, "refGene",chain->tName, chain->tStart, chain->tEnd);
        if (gp == NULL)
            {
            gp = getOverlappingGene(&gpList2, "softberryGene",chain->tName, chain->tStart, chain->tEnd);
            }
//        if (chain->qStrand == '-')
//            fprintf(f,"%7.1f %7.1f %6d %2d %2d %5d %5d %4d %4d %s %d %d %s %d %d %d",iChain->score1, iChain->score2, chain->tEnd-chain->tStart, iChain->tIntron, iChain->qIntron, iChain->tGap, iChain->qGap, iChain->tReps, iChain->qReps, chain->tName, chain->tStart, chain->tEnd, chain->qName, chain->qStart, chain->qEnd, chain->id);
//        else
            fprintf(f," %7.1f %7.1f %6d %2d %2d %5d %5d %4d %4d %s %d %d %s %d %d %d",iChain->score1, iChain->score2, chain->tEnd-chain->tStart, iChain->tIntron, iChain->qIntron, iChain->tGap, iChain->qGap, iChain->tReps, iChain->qReps, chain->tName, chain->tStart, chain->tEnd, chain->qName, qStart(chain), qEnd(chain), chain->id);
        if (gp != NULL)
            fprintf(f," %s %s %d %d",gp->name, gp->chrom, gp->cdsStart, gp->cdsEnd);
        else
            fprintf(f," NO gene 0 0");


        vg = getOverlappingGene(&gpList3, "vegaPseudoGene",chain->qName, qStart(chain), qEnd(chain));
        if (vg != NULL)
            fprintf(f," %s %s %d %d",vg->name, vg->chrom, vg->cdsStart, vg->cdsEnd);

        fprintf(f,"\n");
        /*
       AllocVar(chainId);
        sprintf(chainId, "%d",chain->id);
        if (hashLookup(hash, chainId) == NULL)
                {
                hashAddUnique(hash, chainId, chain);
                }
        */
        }
    //lineFileClose(&lf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
struct hash *chainHash;
FILE *outFile; 
struct binKeeper *tBk = NULL;
struct hash *bkHash = NULL;
struct hash *tSizeHash;
struct chain *chainList;
struct genePred *genePred1;
struct genePred *genePred2;
struct genePred *genePred3;

optionHash(&argc, argv);
if (argc != 12)
    usage();
outFile = mustOpen(argv[8], "w");
/*hSetDb(argv[1]);
hSetDb2(argv[2]);
db = hGetDb();
db2 = hGetDb2();*/
minGap = optionInt("minGap", minGap);
minOverlap = optionFloat("minOverlap", minOverlap);
tSizeHash = readSizes(argv[4]);
printf("Loading Target Repeats\n");
tBk = readRepeats(argv[2], argv[6], tSizeHash);
printf("Loading Query Repeats\n");
bkHash = readRepeatsAll(argv[2], argv[5], argv[7]);
printf("Loading Chains\n");
chainList = readSortChains(argv[3]);
printf("Loading %s\n",argv[9]);
genePred1 = genePredLoadAll(argv[9]);
printf("Loading %s\n",argv[10]);
genePred2 = genePredLoadAll(argv[10]);
printf("Loading %s\n",argv[11]);
genePred3 = genePredLoadAll(argv[11]);
printf("Scoring Chains\n");
scoreChains(chainList, argv[1], outFile, tBk, bkHash, genePred1, genePred2, genePred3);
return 0;
}
