/* Copyright (C) 2005 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hCommon.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
//#include "jksql.h"
#include "hdb.h"
#include "dnautil.h"
#include "chain.h"
#include "chainNet.h"
#include "axt.h"
#include "bed.h"
#include "axtInfo.h"
#include "rmskOut.h"
#include "binRange.h"
#include "obscure.h"
#include "genePred.h"

int minGap = 60; /* minimum intron size */
float minOverlap = 0.60;
int bigChain = 1000000; /* max chain size  to consider */
static struct hash *chainHash = NULL; /* hash of all chains indexed by chainId*/

struct intronChain {
    /* scoring of aligned introns in a chain */ 
    struct intronChain *next;	/* Next in list. */
    struct chain *chain;        /* scored chain */
    int qIntron ;               /* count of introns on query side */ 
    int tIntron ;               /* count of introns on target side */ 
    int tGap ;                  /* count of bases in gaps on target */
    int qGap ;                  /* count of bases in gaps on query */
    int tReps;              /* total number of repeats in target */
    int qReps;              /* total number of repeats in target */
    int total;                  /* total size of chain */
    float score1;               /* score2 * ratio of bases in target/query gaps */
    float score2;               /* ratio of introns target/query , negative if query has more */
    int overlap1;               /* bases that overlap with gene */
    int overlap2;
    char strand;                /* strand where putative pseudogene is */
};

void usage()
/* Explain usage and exit. */
{
errAbort(
  "chainInfo - parse chains to determine number of introns on query and target\n"
  "usage:\n"
  "   chainInfo query_db target_chrom chainFile syntenicNet.bed target.lst query.lst target_repeatMasker.out query_repeatDir output refGene.tab genePred2.tab genePred3.tab\n"
  "options:\n"
    "    -minGap=N minimum size to be considered a gap (intron).  Default is 60.\n"
    "    -minOverlap=0.N threshold for repeat overlap with gap to be considered a repeat instead of intron.  Default is 0.60.\n"
  "db is query database to match genes to pseudogene\n"
  "chrom is target chromosome to match genes to pseudogene\n"
  "syntenicNet.bed is a filter net file using netFilter -syn converted to bed format\n"
  "query_repeatDir is the dirctory containing repeatMasker out files chrXX.fa.out\n"
  "target.lst, query.lst is a list of chromosome names followed by size of each chromosome\n"
  "refGene.tab, genePred2,3 are bed12 files of genes in query database\n"
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
struct cBlock *b, *nextB;
int tGapStart = 0;
int qGapStart = 0;
int tGapEnd = 0;
int qGapEnd = 0, tg,qg;
int tReps, qReps;
float percentOverlap, percentQOverlap;
struct binKeeper *bkQ;


AllocVar(iChain);
iChain->chain = chain;
iChain->qIntron = 0;         iChain->tIntron = 0;          
iChain->tGap = 0;              iChain->qGap = 0;               
iChain->tReps = 0;             iChain->qReps = 0;             iChain->total = 0;                  
iChain->strand = ' ';

for (b = chain->blockList; b != NULL; b = nextB)
    {
    nextB = b->next;
    if (nextB == NULL)
        break;
    tGapStart = b->tEnd;
    tGapEnd = nextB->tStart;
    if (chain->qStrand == '+')
        { 
        qGapStart = b->qEnd;
        qGapEnd = nextB->qStart; 
        }
    else
        { 
        qGapStart = chain->qSize-nextB->qStart;
        qGapEnd = chain->qSize-b->qEnd; 
        }
    tg = tGapEnd - tGapStart;
    qg = qGapEnd - qGapStart;
    t += tg;
    q += qg;

    /* remove repeasts so we don't call them 'introns' on target*/
    tReps = 0;
    for (el = binKeeperFindSorted(bk, tGapStart, tGapEnd); el != NULL; el = el->next)
        {
        rmsk = el->val;
        tReps += positiveRangeIntersection(tGapStart, tGapEnd, rmsk->genoStart, rmsk->genoEnd);
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

    /* remove repeasts so we don't call them 'introns' on query */
    /* get bk for query */
    qReps = 0;
    if (bkHash != NULL)
        {
        bkQ = hashFindVal(bkHash, chain->qName);
        for (el = binKeeperFindSorted(bkQ, qGapStart, qGapEnd); el != NULL; el = el->next)
            {
            rmsk = el->val;
            qReps += positiveRangeIntersection(qGapStart, qGapEnd, rmsk->genoStart, rmsk->genoEnd);
            }
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

iChain->score2 = iChain->qIntron > iChain->tIntron ? (iChain->qIntron+1.0)/(iChain->tIntron+1.0) : -(iChain->tIntron+1.0)/(iChain->qIntron+1.0) ;
iChain->score1 = (iChain->qIntron > iChain->tIntron) ? ((iChain->qGap+1)/(iChain->tGap+1)) : -(iChain->tGap+1)/(iChain->qGap+1);
return iChain;
}
#ifdef DUPE

struct binKeeper *readRepeats(char *chrom, char *rmskFileName, struct hash *tSizeHash)
/* read all repeats for a chromosome of size size, returns results in binKeeper structure for fast query*/
{
    boolean rmskRet;
    struct lineFile *rmskF = NULL;
    struct rmskOut *rmsk;
    struct binKeeper *bk; 
    int size;

    size = hashIntVal(tSizeHash, chrom);
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

struct hash *oldreadRepeatsAll(char *chrom, char *sizeFileName, char *rmskDir)
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
#endif

struct chain *readSortChains(char *fileName)
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct chain *chainList = NULL, *chain;
char chainId[128];

chainHash = newHash(0);
while ((chain = chainRead(lf)) != NULL)
    {
    slAddHead(&chainList, chain);
    safef(chainId, sizeof(chainId), "%d", chain->id);
    hashAddUnique(chainHash, chainId, chain);
    }
lineFileClose(&lf);

/* Sort. */
printf("Sorting Chains\n");
slSort(&chainList, chainCmpQuery);
return chainList;
}

struct chain *chainFromId(int id)
/** Return a chain given the id. */
{
char key[128];
struct chain *chain = NULL;
if(chainHash == NULL)
    errAbort("chainFromId() - no chains found");
safef(key, sizeof(key), "%d", id);
chain =  hashFindVal(chainHash, key);
if(chain == NULL)
    warn("Chain not found for id: %d", id);
return chain;
}
boolean checkChain(struct chain *chain, int start, int end)
/* Return true if chain covers start, end, false otherwise. */
{
struct chain *subChain=NULL, *toFree=NULL;
boolean good = FALSE;
if (chain == NULL) return good;
chainSubsetOnT(chain, start, end, &subChain, &toFree);    
if(subChain != NULL)
    good = TRUE;
chainFree(&toFree);
return good;
}

struct chain *lookForChain(struct cnFill *list, int start, int end)
/* Recursively look for a chain in this list containing the coordinates 
   desired. */
{
struct cnFill *fill=NULL;
struct cnFill *gap=NULL;

struct chain *chain = NULL;
for(fill = list; fill != NULL; fill = fill->next)
    {
    if (positiveRangeIntersection(fill->tStart, fill->tStart+fill->tSize, start, end))
        {
        chain = chainFromId(fill->chainId);
        if(checkChain(chain, start,end))
            return chain;
        for(gap = fill->children; gap != NULL; gap = gap->next)
            {
            chain = chainFromId(fill->chainId);
            if(checkChain(chain, start,end))
                return chain;
            if(gap->children)
                {
                chain = lookForChain(gap->children, start, end);
                if(checkChain(chain, start,end))
                    return chain;
                }
            }
        }
    }
return chain;
}

struct cnFill *getNetList(struct chain *chain, struct chainNet *netList)
/* return all cnFills from a netList that overlap a particular chain  by at least 100 bases*/
{
struct cnFill *fillList = NULL, *fill, *fillClone;
struct chainNet *net;

for (net = netList; net != NULL; net = net->next)
    {
    for (fill = net->fillList; fill != NULL; fill = fill->next)
    {
    if (positiveRangeIntersection(fill->tStart,fill->tStart+fill->tSize,  chain->tStart, chain->tEnd) > 10)
        {
        fillClone = cnFillNew();
        fillClone->next = NULL ;
        fillClone->children = fill->children ;
        fillClone->tStart = fill->tStart ;
        fillClone->tSize = fill->tSize ;
        fillClone->qName = cloneString(fill->qName) ;
        fillClone->type = cloneString(fill->type) ;
        fillClone->qStrand = fill->qStrand ;
        fillClone->qStart = fill->qStart ;
        fillClone->qSize = fill->qSize ;
        fillClone->chainId = fill->chainId ;
        fillClone->ali = fill->ali = fill->ali ;
        fillClone->tN = fill->tN = fill->tN ;
        fillClone->qN = fill->qN = fill->qN ;
        fillClone->tR = fill->tR ;
        fillClone->qR = fill->qR ;
        fillClone->tNewR = fill->tNewR ;
        fillClone->qNewR = fill->qNewR ;
        fillClone->tOldR = fill->tOldR ;
        fillClone->qOldR = fill->qOldR ;
        fillClone->tTrf = fill->tTrf ;
        fillClone->qTrf = fill->qTrf ;
        fillClone->qOver = fill->qOver ;
        fillClone->qFar = fill->qFar ;
        fillClone->qDup = fill->qDup ;
	
        slAddHead(&fillList, fillClone);
        printf(" %s %d-%d ", fill->type, fill->tStart, fill->tStart+fill->tSize);
        }
    //else printf(" %d-%d ", fill->tStart, fill->tStart+fill->tSize);
    }
    }
//slReverse(&fillList);
if (fillList == NULL)
    printf("return null for %d %s %s:%d-%d\n",chain->id, chain->qName, chain->tName, chain->tStart, chain->tEnd);
return fillList;
}
void scoreChains(struct chain *chainList, struct bed *bedList, char *db, FILE *f, struct binKeeper *bk, struct hash *bkHash, struct genePred *gpList1, struct genePred *gpList2, struct genePred *gpList3, struct genePred *gpList4)
/* score all chains in a chain file based on ration of introns and gaps  , mask repeats first */
/* reduce score for chains that are on the main syntenic net */
/* return best overlapping gene in the target sequence */
{
struct chain *chain, *retSubChain,*retChainToFree, *netChain;
struct cnFill *fillList, *fill;
struct intronChain *iChain=NULL, *iChainList = NULL;
struct genePred *gp = NULL, *vg;
struct bed *bed;
int overlap1, overlapVega;
boolean onDiagonal;
int overlapDiagonal;

for (chain = chainList; chain != NULL; chain = chain->next)
    {
    /* skip long chains - they are not pseudogenes */
    if ((chain->tEnd-chain->tStart)> bigChain) 
        continue;
    overlapDiagonal = 0; overlap1 = 0; overlapVega = 0;
    for (bed = bedList ; bed != NULL ; bed = bed->next)
        {
        if( positiveRangeIntersection(bed->chromStart, bed->chromEnd, chain->tStart, chain->tEnd) > overlapDiagonal )
            overlapDiagonal = positiveRangeIntersection(bed->chromStart, bed->chromEnd, chain->tStart, chain->tEnd);
        }
    /* look for gene on query side */
    gp = getOverlappingGene(&gpList1, "refGene",chain->qName, qStart(chain), qEnd(chain) , &overlap1);
    if (gp == NULL)
        {
        gp = getOverlappingGene(&gpList2, "softberryGene",chain->qName, qStart(chain), qEnd(chain) , &overlap1);
        if (gp == NULL)
            {
            gp = getOverlappingGene(&gpList4, "all_mrna",chain->qName, qStart(chain), qEnd(chain) , &overlap1);
            }
        }
    /* check for annontated pseudogene */
    vg = getOverlappingGene(&gpList3, "vegaPseudoGene",chain->tName, chain->tStart, chain->tEnd, &overlapVega );

    /* if we found something , score it and output record */
//    if (gp != NULL || vg != NULL)
        {
        assert (chain->tEnd > 0);
        retSubChain = NULL;
        if (gp != NULL)
            if (chain->qStrand =='+')
                chainSubsetOnQ(chain, gp->txStart, gp->txEnd, &retSubChain,  &retChainToFree);
            else
                chainSubsetOnQ(chain, chain->qSize - gp->txEnd, chain->qSize - gp->txStart, &retSubChain,  &retChainToFree);

        if (retSubChain != NULL)
            {
            iChain = scoreNextAlignedIntron(retSubChain,f, bk, bkHash);
            iChain->overlap1 = overlap1;
            iChain->overlap1 = overlapVega;
            if (gp != NULL)
                iChain->strand = gp->strand[0] == retSubChain->qStrand ? '+' : '-';
            else 
                iChain->strand = '0';
            slAddHead(&iChainList, iChain);
            if (gp != NULL) 
                fprintf(f," %7.1f %7.1f %6d %2d %2d %5d %5d %4d %4d %s %d %d %c %s %d %d %c %d %d" ,
                        iChain->score1, iChain->score2, retSubChain->tEnd-retSubChain->tStart, iChain->tIntron, iChain->qIntron, 
                        iChain->tGap, iChain->qGap, iChain->tReps, iChain->qReps, chain->tName, 
                        retSubChain->tStart, retSubChain->tEnd, iChain->strand , 
                        retSubChain->qName, qStart(retSubChain), qEnd(retSubChain), retSubChain->qStrand, 
                        chain->id, overlapDiagonal);
            else
                fprintf(f," %7.1f %7.1f %6d %2d %2d %5d %5d %4d %4d %s %d %d %c %s %d %d %c %d %d",
                        iChain->score1, iChain->score2, retSubChain->tEnd-retSubChain->tStart, iChain->tIntron, iChain->qIntron, 
                        iChain->tGap, iChain->qGap, iChain->tReps, iChain->qReps, chain->tName, 
                        retSubChain->tStart, retSubChain->tEnd, iChain->strand , 
                        retSubChain->qName, qStart(retSubChain), qEnd(retSubChain), retSubChain->qStrand, 
                        chain->id, overlapDiagonal);
            if (gp != NULL && genePredBases(gp) != 0)
                fprintf(f," %s %s %d %d %d %2.2f %c",
                        gp->name, gp->chrom, gp->txStart, gp->txEnd, iChain->overlap1, 
                        (float)iChain->overlap1/(float)genePredBases(gp), gp->strand[0]);
            else
                fprintf(f," NO gene 0 0 0 0.0 0");

            if (vg != NULL && genePredBases(vg) != 0 && gp != NULL)
                fprintf(f," %s %s %d %d %d %2.2f %c",
                        vg->name, vg->chrom, vg->txStart, vg->txEnd, iChain->overlap2, 
                        (float)iChain->overlap2/(float)genePredBases(vg), vg->strand[0]);

            fprintf(f,"\n");
            chainFree(&retChainToFree);
            }
        else
            {
                fprintf(f," 0 0 0 0 0 0 0 0 0 %s %d %d + %s %d %d %c %d %d NO gene 0 0 0 0.0 0\n",
                    chain->tName, chain->tStart, chain->tEnd,  
                    chain->qName, qStart(chain), qEnd(chain), chain->qStrand, 
                    chain->id, overlapDiagonal);
            }
        }
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
struct hash *chainHash;
FILE *outFile, *f; 
struct binKeeper *tBk = NULL;
struct hash *bkHash = NULL;
struct hash *tSizeHash;
struct chain *chainList;
struct genePred *genePred1;
struct genePred *genePred2;
struct genePred *genePred3;
struct genePred *genePred4 = NULL, *gp;
struct lineFile *lf = NULL;
struct chainNet *netList = NULL, *net;
struct bed *bedList = NULL, *bed;
char  *row[3];
int overlap;

optionHash(&argc, argv);
if (argc < 13)
    usage();
outFile = mustOpen(argv[9], "w");
/*hSetDb(argv[1]);
hSetDb2(argv[2]);
db = hGetDb();
db2 = hGetDb2();*/
minGap = optionInt("minGap", minGap);
minOverlap = optionFloat("minOverlap", minOverlap);
tSizeHash = readSizes(argv[5]);

hSetDb(argv[1]);
printf("Loading Syntenic Bed\n");
lf = lineFileOpen(argv[4], TRUE);
while (lineFileRow(lf,row))
    {
    bed = bedLoad3(row);
    slAddHead(&bedList, bed);
    }
printf("Loading %s\n",argv[10]);
genePred1 = genePredLoadAll(argv[10]);
printf("Loading %s\n",argv[11]);
genePred2 = genePredLoadAll(argv[11]);
printf("Loading %s\n",argv[12]);
genePred3 = genePredLoadAll(argv[12]);
if (argc > 13)
    {
    printf("Loading %s\n",argv[13]);
    genePred4 = genePredLoadAll(argv[13]);
    }
printf("Loading Chains\n");
chainList = readSortChains(argv[3]);
printf("Loading Target Repeats\n");
tBk = readRepeats(argv[2], argv[7], tSizeHash);
printf("Loading Query Repeats\n");
bkHash = readRepeatsAll(argv[6], argv[8]);
printf("Scoring Chains\n");
if (genePred4 == NULL)
    {
    gp = getOverlappingGene(&genePred4, "all_mrna","chr1", 1, 100 , &overlap);
    f = mustOpen("mrna.tab", "w");
    for (gp = genePred4; gp != NULL; gp = gp->next)
        {
        genePredTabOut(gp, f);
        }
    fclose(f);
    }
scoreChains(chainList, bedList, argv[1], outFile, tBk, bkHash, genePred1, genePred2, genePred3, genePred4);
return 0;
}
