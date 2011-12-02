/* checkExp - check if retroGene aligns better to parent or retroGene */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "memalloc.h"
#include "psl.h"
#include "bed.h"
#include "hdb.h"
#include "axt.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "nib.h"
#include "fa.h"
#include "dlist.h"
#include "dystring.h"
#include "binRange.h"
#include "options.h"
#include "genePred.h"
#include "genePredReader.h"
#include "dystring.h"
#include "pseudoGeneLink.h"
#include "verbose.h"
#include "chain.h"
#include "twoBit.h"
#include "chainToAxt.h"

struct axtScoreScheme *ss = NULL; /* blastz scoring matrix */
struct dnaSeq *mrnaList = NULL; /* list of all input mrna sequences */
struct hash *pseudoHash = NULL, *mrnaHash = NULL, *chainHash = NULL, *faHash = NULL, *tHash = NULL;
int maxGap = 100;
struct dlList *fileCache = NULL;
struct hash *nibHash = NULL;
FILE *outFile = NULL; /* output tab sep file */
struct twoBitFile *twoBitFile = NULL;

struct misMatch
/* list of mismatches between gene and retrogene */
    {
    struct misMatch *next;      /* next in list. */
    int retroLoc;               /* genomic location of mismatch in retroGene */
    int parentLoc;              /* genomic location of mismatch in parent Gene */
    char retroBase;             /* contents of base in retroGene */
    char parentBase;            /* contents of base in parent gene */
    };

struct cachedFile
/* File in cache. */
    {
    struct cachedFile *next;	/* next in list. */
    char *name;		/* File name (allocated here) */
    FILE *f;		/* Open file. */
    };

struct seqFilePos
/* Where a sequence is in a file. */
    {
    struct filePos *next;	/* Next in list. */
    char *name;	/* Sequence name. Allocated in hash. */
    char *file;	/* Sequence file name, allocated in hash. */
    long pos; /* Position in fa file/size of nib. */
    bool isNib;	/* True if a nib file. */
    };
void usage()
/* Print usage instructions and exit. */
{
errAbort(
    "checkExp - check if retroGene aligns better to parent or retroGene \n"
    "usage:\n"
    "    checkExp pseudoGeneLink.bed chrom.sizes all_mrna.psl target.lst mrna.2bit chrX_self.chain nibDir score.tab\n\n"
    "where \n"
    "pseudoGeneLink.bed is a list of retroGenes records \n"
    "chrom.sizes is a list of chromosome followed by size\n"
    "all_mrna.psl contains blat mRNA alignment file\n"
    "target.lst contains list nib files\n"
    "mrna.2bit contains fasta sequences of all mrnas \n"
    "chrX_self.chain blastz Self chain\n"
    "directory containing nibs for self alignment\n"
    "score.tab is output\n" 
    );
}

void writeGap(struct dyString *aRes, int aGap, char *aSeq, struct dyString *bRes, int bGap, char *bSeq)
/* Write double - gap.  Something like:
 *         --c
 *         ag-  */

{
dyStringAppendMultiC(aRes, '-', bGap);
dyStringAppendN(bRes, bSeq, bGap);
dyStringAppendN(aRes, aSeq, aGap);
dyStringAppendMultiC(bRes, '-', aGap);
}

void writeInsert(struct dyString *aRes, struct dyString *bRes, char *aSeq, int gapSize)
/* Write out gap, possibly shortened, to aRes, bRes. */
{
dyStringAppendN(aRes, aSeq, gapSize);
dyStringAppendMultiC(bRes, '-', gapSize);
}

struct hash *readChainToBinKeeper(char *sizeFileName, char *fileName)
{
struct binKeeper *bk; 
struct chain *chain;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct lineFile *sf = lineFileOpen(sizeFileName, TRUE);
struct hash *hash = newHash(0);
char *chromRow[2];

while (lineFileRow(sf, chromRow))
    {
    char *name = chromRow[0];
    int size = lineFileNeedNum(sf, chromRow, 1);

    if (hashLookup(hash, name) != NULL)
        warn("Duplicate %s, ignoring all but first\n", name);
    else
        {
        bk = binKeeperNew(0, size);
        assert(size > 1);
	hashAdd(hash, name, bk);
        }
    }
while ((chain = chainRead(lf)) != NULL)
    {
    bk = hashMustFindVal(hash, chain->tName);
    binKeeperAdd(bk, chain->tStart, chain->tEnd, chain);
    }
lineFileClose(&lf);
return hash;
}

struct dnaSeq *nibInfoLoadSeq(struct nibInfo *nib, int start, int size)
/* Load in a sequence in mixed case from nib file. */
{
return nibLdPartMasked(NIB_MASK_MIXED, nib->fileName, nib->f, nib->size, 
	start, size);
}

struct dnaSeq *nibInfoLoadStrand(struct nibInfo *nib, int start, int end,
	char strand)
/* Load in a mixed case sequence from nib file, from reverse strand if
 * strand is '-'. */
{
struct dnaSeq *seq;
int size = end - start;
assert(size >= 0);
if (strand == '-')
    {
    reverseIntRange(&start, &end, nib->size);
    seq = nibInfoLoadSeq(nib, start, size);
    reverseComplement(seq->dna, seq->size);
    }
else
    {
    seq = nibInfoLoadSeq(nib, start, size);
    }
return seq;
}
/*
struct hash *readPslToHash(char *pslFileName)
{
int size;
struct psl *psl;
struct lineFile *pf = lineFileOpen(pslFileName , TRUE);
struct hash *hash = newHash(0);
char *chromRow[2];
char *row[21] ;

    }
while (lineFileNextRow(pf, row, ArraySize(row)))
    {
    psl = pslLoad(row);
    if (hashFindVal(hash, psl->qName) == NULL)
	hashAdd(hash, name, psl);
    }
lineFileClose(&pf);
return hash;
}
*/

struct dnaSeq *readSeqFromFaPos(struct seqFilePos *sfp,  FILE *f)
/* Read part of FA file. */
{
struct dnaSeq *seq;
fseek(f, sfp->pos, SEEK_SET);
if (!faReadNext(f, "", TRUE, NULL, &seq))
    errAbort("Couldn't faReadNext on %s in %s\n", sfp->name, sfp->file);
return seq;
}

FILE *openFromCache(struct dlList *cache, char *fileName)
/* Return open file handle via cache.  The simple logic here
 * depends on not more than N files being returned at once. */
{
static int maxCacheSize=32;
int cacheSize = 0;
struct dlNode *node;
struct cachedFile *cf;

/* First loop through trying to find it in cache, counting
 * cache size as we go. */
for (node = cache->head; !dlEnd(node); node = node->next)
    {
    ++cacheSize;
    cf = node->val;
    if (sameString(fileName, cf->name))
        {
	dlRemove(node);
	dlAddHead(cache, node);
	return cf->f;
	}
    }

/* If cache has reached max size free least recently used. */
if (cacheSize >= maxCacheSize)
    {
    node = dlPopTail(cache);
    cf = node->val;
    carefulClose(&cf->f);
    freeMem(cf->name);
    freeMem(cf);
    freeMem(node);
    }

/* Cache new file. */
AllocVar(cf);
cf->name = cloneString(fileName);
cf->f = mustOpen(fileName, "rb");
dlAddValHead(cache, cf);
return cf->f;
}

void readCachedSeqPart(char *seqName, int start, int size, 
     struct hash *hash, struct dlList *fileCache, 
     struct dnaSeq **retSeq, int *retOffset, boolean *retIsNib)
/* Read sequence hopefully using file cashe. If sequence is in a nib
 * file just read part of it. */
{
struct seqFilePos *sfp = hashMustFindVal(hash, seqName);
FILE *f = openFromCache(fileCache, sfp->file);
unsigned options = NIB_MASK_MIXED;
if (sfp->isNib)
    {
    *retSeq = nibLdPartMasked(options, sfp->file, f, sfp->pos, start, size);
    *retOffset = start;
    *retIsNib = TRUE;
    }
else
    {
    *retSeq = readSeqFromFaPos(sfp, f);
    *retOffset = 0;
    *retIsNib = FALSE;
    }
}
struct axt *axtCreate(char *q, char *t, int size, struct psl *psl)
/* create axt */
{
int qs = psl->qStart, qe = psl->qEnd;
int ts = psl->tStart, te = psl->tEnd;
int symCount = 0;
struct axt *axt = NULL;

AllocVar(axt);
if (psl->strand[0] == '-')
    reverseIntRange(&qs, &qe, psl->qSize);

if (psl->strand[1] == '-')
    reverseIntRange(&ts, &te, psl->tSize);

axt->qName = cloneString(psl->qName);
axt->tName = cloneString(psl->tName);
axt->qStart = qs+1;
axt->qEnd = qe;
axt->qStrand = psl->strand[0];
axt->tStrand = '+';
if (psl->strand[1] != 0)
    {
    axt->tStart = ts+1;
    axt->tEnd = te;
    }
else
    {
    axt->tStart = psl->tStart+1;
    axt->tEnd = psl->tEnd;
    }
axt->symCount = symCount = strlen(t);
axt->tSym = cloneString(t);
if (strlen(q) != symCount)
    warn("Symbol count %d != %d inconsistent at t %s:%d and qName %s\n%s\n%s\n",
    	symCount, (int)strlen(q), psl->tName, psl->tStart, psl->qName, t, q);
axt->qSym = cloneString(q);
axt->score = axtScoreFilterRepeats(axt, ss);
verbose(1,"axt score = %d\n",axt->score);
//for (i=0; i<size ; i++) 
//    fputc(t[i],f);
//for (i=0; i<size ; i++) 
//    fputc(q[i],f);
return axt;
}
struct axt *pslToAxt(struct psl *psl, struct hash *qHash, char *tNibDir, 
	struct dlList *fileCache)
{
static char *tName = NULL, *qName = NULL;
static struct dnaSeq *tSeq = NULL;
struct dyString *q = newDyString(16*1024);
struct dyString *t = newDyString(16*1024);
int blockIx;
int qs, ts ;
int lastQ = 0, lastT = 0, size;
int qOffset = 0;
int tOffset = 0;
struct axt *axt = NULL;
boolean qIsNib = FALSE;
boolean tIsNib = FALSE;
int cnt = 0;
//struct dnaSeq *tSeq = NULL;
struct nibInfo *tNib = NULL;

struct dnaSeq *qSeq = twoBitReadSeqFrag(twoBitFile, psl->qName, 0, 0);
   // hGenBankGetMrna(psl->qName, NULL);
/*
freeDnaSeq(&qSeq);
freez(&qName);
assert(mrnaList != NULL);
for (mrna = mrnaList; mrna != NULL ; mrna = mrna->next)
    {
    assert(mrna != NULL);
    cnt++;
    if (sameString(mrna->name, psl->qName))
        {
        qSeq = cloneDnaSeq(mrna);
        assert(qSeq != NULL);
        break;
        }
    }
    */
if (qSeq == NULL)
    {
    warn("mrna sequence data not found %s, searched %d sequences\n",psl->qName,cnt);
    dyStringFree(&q);
    dyStringFree(&t);
    dnaSeqFree(&tSeq);
    dnaSeqFree(&qSeq);
    return NULL;
    }
if (qSeq->size != psl->qSize)
    {
    warn("sequence %s aligned is different size %d from mrna.fa file %d \n",psl->qName,psl->qSize,qSeq->size);
    dyStringFree(&q);
    dyStringFree(&t);
    dnaSeqFree(&tSeq);
    dnaSeqFree(&qSeq);
    return NULL;
    }
qName = cloneString(psl->qName);
if (qIsNib && psl->strand[0] == '-')
    qOffset = psl->qSize - psl->qEnd;
else
    qOffset = 0;
verbose(5,"qString len = %d qOffset = %d\n",qSeq->size,qOffset);
if (tName == NULL || !sameString(tName, psl->tName) || tIsNib)
    {
    freeDnaSeq(&tSeq);
    freez(&tName);
    tName = cloneString(psl->tName);
    tNib = nibInfoFromCache(nibHash, tNibDir, tName);
    assert(tNib !=NULL);
    tSeq = nibInfoLoadStrand(tNib, psl->tStart, psl->tEnd, '+');
    assert(tSeq !=NULL);
    tOffset = psl->tStart;
    //readCachedSeqPart(tName, psl->tStart, psl->tEnd-psl->tStart, 
//	tHash, fileCache, &tSeq, &tOffset, &tIsNib);
    }
verbose(4,"strand t %s \n",psl->strand);
if (tSeq != NULL)
    verbose(5,"tString len = %d tOffset = %d\n",tSeq->size,tOffset);
else
    errAbort("tSeq is NULL\n");
if (psl->strand[0] == '-')
    reverseComplement(qSeq->dna, qSeq->size);
//if (strlen(psl->strand) > 1 )
//    if (psl->strand[1] == '-')
//        reverseComplement(tSeq->dna, tSeq->size);
for (blockIx=0; blockIx < psl->blockCount; ++blockIx)
    {
    qs = psl->qStarts[blockIx] - qOffset;
    ts = psl->tStarts[blockIx] - tOffset;

    if (blockIx != 0)
        {
	int qGap, tGap, minGap;
	qGap = qs - lastQ;
	tGap = ts - lastT;
	minGap = min(qGap, tGap);
	if (minGap > 0)
	    {
	    writeGap(q, qGap, qSeq->dna + lastQ, t, tGap, tSeq->dna + lastT);
	    }
	else if (qGap > 0)
	    {
	    writeInsert(q, t, qSeq->dna + lastQ, qGap);
	    }
	else if (tGap > 0)
	    {
	    writeInsert(t, q, tSeq->dna + lastT, tGap);
	    }
	}
    size = psl->blockSizes[blockIx];
    assert(qSeq != NULL);
    dyStringAppendN(q, qSeq->dna + qs, size);
    lastQ = qs + size;
    dyStringAppendN(t, tSeq->dna + ts, size);
    lastT = ts + size;
    }

if (strlen(q->string) != strlen(t->string))
    warn("Symbol count(t) %d != %d inconsistent at t %s:%d and qName %s\n%s\n%s\n",
    	(int)strlen(t->string), (int)strlen(q->string), psl->tName, psl->tStart, psl->qName, t->string, q->string);
if (psl->strand[0] == '-')
    {
    reverseComplement(q->string, q->stringSize);
    reverseComplement(t->string, t->stringSize);
    }
axt = axtCreate(q->string, t->string, min(q->stringSize,t->stringSize), psl);
dyStringFree(&q);
dyStringFree(&t);
//dnaSeqFree(&tSeq);
dnaSeqFree(&qSeq);
if (qIsNib)
    freez(&qName);
//if (tIsNib)
//    freez(&tName);
return axt;
}


struct misMatch *matchFound(struct misMatch **misMatchList, int retroLoc)
{
struct misMatch *el;

for (el = *misMatchList; el != NULL ; el = el->next)
    {
    verbose(5,"%d == %d %c %c\n",el->retroLoc, retroLoc, el->retroBase, el->parentBase);
    if (el->retroLoc == retroLoc && el->retroBase != '-' && el->parentBase != '-')
        return el;
    }
return NULL;
}

void addMisMatch(struct misMatch **misMatchList, struct axt *axt, int qSize)
{
int i;
struct misMatch *misMatch = NULL;

for (i = 0 ; i <= axt->symCount ; i++)
    {
    if (axt->tSym[i] != axt->qSym[i])
        {
        /* check to see if we already have this one
         * if not, add it */
        if (matchFound(misMatchList, axt->tStart+i) == NULL)
            {
            AllocVar(misMatch);
            misMatch->retroLoc = axt->tStart+i;
            if (axt->qStrand == '+')
                misMatch->parentLoc = axt->qStart+i;
            else
                misMatch->parentLoc = qSize-(axt->qStart+i);
            misMatch->retroBase = axt->tSym[i];
            misMatch->parentBase = axt->qSym[i];
            slAddHead(misMatchList, misMatch);

            verbose(5, "listLen = %d\n",slCount(misMatchList));
            verbose(4,"add [%d] %d %d R %c P %c\n",
                i, 
                misMatch->retroLoc ,
                misMatch->parentLoc,
                misMatch->retroBase,
                misMatch->parentBase);
            }
        }
    else
        {
        verbose(8,"don't add [%d] %d \n",i, axt->tStart+i);
        }
    }
}
void checkExp(char *bedFileName, char *tNibDir, char *nibList)
{
struct lineFile *bf = lineFileOpen(bedFileName , TRUE), *af = NULL;
char *row[PSEUDOGENELINK_NUM_COLS] ;
struct pseudoGeneLink *ps;
char *tmpName[512], cmd[512];
struct axt *axtList = NULL, *axt, *mAxt = NULL;
struct dnaSeq *qSeq = NULL, *tSeq = NULL, *seqList = NULL;
struct nibInfo *qNib = NULL, *tNib = NULL;
FILE *op;
int ret;

if (nibHash == NULL)
    nibHash = hashNew(0);
while (lineFileNextRow(bf, row, ArraySize(row)))
    {
    struct misMatch *misMatchList = NULL;
    struct binKeeper *bk = NULL;
    struct binElement *el, *elist = NULL;
    struct psl *mPsl = NULL, *rPsl = NULL, *pPsl = NULL, *psl ;
    struct misMatch *mf = NULL;
    ps = pseudoGeneLinkLoad(row);
    tmpName[0] = cloneString(ps->name);
    chopByChar(tmpName[0], '.', tmpName, sizeof(tmpName));
    verbose(2,"name %s %s:%d-%d\n",
            ps->name, ps->chrom, ps->chromStart,ps->chromEnd);
    /* get expressed retro from hash */
    bk = hashFindVal(mrnaHash, ps->chrom);
    elist = binKeeperFindSorted(bk, ps->chromStart, ps->chromEnd ) ;
    for (el = elist; el != NULL ; el = el->next)
        {
        rPsl = el->val;
        verbose(2,"retroGene %s %s:%d-%d\n",rPsl->qName, ps->chrom, ps->chromStart,ps->chromEnd);
        }
    /* find mrnas that overlap parent gene */
    bk = hashFindVal(mrnaHash, ps->gChrom);
    elist = binKeeperFindSorted(bk, ps->gStart , ps->gEnd ) ;
    for (el = elist; el != NULL ; el = el->next)
        {
        pPsl = el->val;
        verbose(2,"parent %s %s:%d %d,%d\n",
                pPsl->qName, pPsl->tName,pPsl->tStart,
                pPsl->match, pPsl->misMatch);
        }
    /* find self chain */
    bk = hashFindVal(chainHash, ps->chrom);
    elist = binKeeperFind(bk, ps->chromStart , ps->chromEnd ) ;
    slSort(&elist, chainCmpScoreDesc);
    for (el = elist; el != NULL ; el = el->next)
        {
        struct chain *chain = el->val, *subChain, *retChainToFree, *retChainToFree2;
        int qs = chain->qStart;
        int qe = chain->qEnd;
        int id = chain->id;
        if (chain->qStrand == '-')
            {
            qs = chain->qSize - chain->qEnd;
            qe = chain->qSize - chain->qStart;
            }
        if (!sameString(chain->qName , ps->gChrom) || 
                !positiveRangeIntersection(qs, qe, ps->gStart, ps->gEnd))
            {
            verbose(2," wrong chain %s:%d-%d %s:%d-%d parent %s:%d-%d\n", 
                chain->qName, qs, qe, 
                chain->tName,chain->tStart,chain->tEnd,
                ps->gChrom,ps->gStart,ps->gEnd);
            continue;
            }
        verbose(2,"chain id %d %4.0f",chain->id, chain->score);
        chainSubsetOnT(chain, ps->chromStart+7, ps->chromEnd-7, 
            &subChain,  &retChainToFree);
        if (subChain != NULL)
            chain = subChain;
        chainSubsetOnQ(chain, ps->gStart, ps->gEnd, 
            &subChain,  &retChainToFree2);
        if (subChain != NULL)
            chain = subChain;
        if (chain->qStrand == '-')
            {
            qs = chain->qSize - chain->qEnd;
            qe = chain->qSize - chain->qStart;
            }
        verbose(2," %s:%d-%d %s:%d-%d ", 
                chain->qName, qs, qe, 
                chain->tName,chain->tStart,chain->tEnd);
        if (subChain != NULL)
            verbose(2,"subChain %s:%d-%d %s:%d-%d\n",
                    subChain->qName, subChain->qStart, subChain->qEnd, 
                    subChain->tName,subChain->tStart,subChain->tEnd);

	qNib = nibInfoFromCache(nibHash, tNibDir, chain->qName);
	tNib = nibInfoFromCache(nibHash, tNibDir, chain->tName);
	tSeq = nibInfoLoadStrand(tNib, chain->tStart, chain->tEnd, '+');
	qSeq = nibInfoLoadStrand(qNib, chain->qStart, chain->qEnd, chain->qStrand);
	axtList = chainToAxt(chain, qSeq, chain->qStart, tSeq, chain->tStart,
	    maxGap, BIGNUM);
        verbose(2,"axt count %d misMatch cnt %d\n",slCount(axtList), slCount(misMatchList));
        for (axt = axtList; axt != NULL ; axt = axt->next)
            {
            addMisMatch(&misMatchList, axt, chain->qSize);
            }
        verbose(2,"%d in mismatch list %s id %d \n",slCount(misMatchList), chain->qName, id);
        chainFree(&retChainToFree);
        chainFree(&retChainToFree2);
        break;
        }
    /* create axt of each expressed retroGene to parent gene */
        /* get alignment for each mrna overlapping retroGene */
    bk = hashFindVal(mrnaHash, ps->chrom);
    elist = binKeeperFindSorted(bk, ps->chromStart , ps->chromEnd ) ;
    {
    char queryName[512];
    char axtName[512];
    char pslName[512];
    safef(queryName, sizeof(queryName), "/tmp/query.%s.fa", ps->chrom);
    safef(axtName, sizeof(axtName), "/tmp/tmp.%s.axt", ps->chrom);
    safef(pslName, sizeof(pslName), "/tmp/tmp.%s.psl", ps->chrom);
    op = fopen(pslName,"w");
    for (el = elist ; el != NULL ; el = el->next)
        {
        psl = el->val;
        pslOutput(psl, op, '\t','\n');
        qSeq = twoBitReadSeqFrag(twoBitFile, psl->qName, 0, 0);

        if (qSeq != NULL)
            slAddHead(&seqList, qSeq);
        else
            errAbort("seq %s not found \n", psl->qName);
        }
    fclose(op);
    faWriteAll(queryName, seqList);
    safef(cmd,sizeof(cmd),"pslPretty -long -axt %s %s %s %s",pslName , nibList, queryName, axtName);
    ret = system(cmd);
    if (ret != 0)
        errAbort("ret is %d %s\n",ret,cmd);
    verbose(2, "ret is %d %s\n",ret,cmd);
    af = lineFileOpen(axtName, TRUE);
    while ((axt = axtRead(af)) != NULL)
        slAddHead(&mAxt, axt);
    lineFileClose(&af);
    }
    slReverse(&mAxt);
    /* for each parent/retro pair, count bases matching retro and parent better */
    for (el = elist; el != NULL ; el = el->next)
        {
        int i, scoreRetro=0, scoreParent=0, scoreNeither=0;
        struct dyString *parentMatch = newDyString(16*1024);
        struct dyString *retroMatch = newDyString(16*1024);
        mPsl = el->val;

        if (mAxt != NULL)
            {
            verbose(2,"mrna %s %s:%d %d,%d axt %s\n",
                    mPsl->qName, mPsl->tName,mPsl->tStart,
                    mPsl->match, mPsl->misMatch, 
                    mAxt->qName);
            assert(sameString(mPsl->qName, mAxt->qName));
            for (i = 0 ; i< (mPsl->tEnd-mPsl->tStart) ; i++)
                {
                int j = mAxt->tStart - mPsl->tStart;
                verbose(5, "listLen = %d\n",slCount(&misMatchList));
                if ((mf = matchFound(&misMatchList, (mPsl->tStart)+i)) != NULL)
                    {
                    if (toupper(mf->retroBase) == toupper(mAxt->qSym[j+i]))
                        {
                        verbose (3,"match retro[%d] %d %c == %c parent %c %d\n",
                                i,mf->retroLoc, mf->retroBase, mAxt->qSym[j+i], 
                                mf->parentBase, mf->parentLoc);
                        dyStringPrintf(retroMatch, "%d,", mf->retroLoc);
                        scoreRetro++;
                        }
                    else if (toupper(mf->parentBase) == toupper(mAxt->qSym[j+i]))
                        {
                        verbose (3,"match parent[%d] %d %c == %c retro %c %d\n",
                                i,mf->parentLoc, mf->parentBase, mAxt->qSym[j+i], 
                                mf->retroBase, mf->retroLoc);
                        dyStringPrintf(parentMatch, "%d,", mf->parentLoc);
                        scoreParent++;
                        }
                    else
                        {
                        verbose (3,"match neither[%d] %d %c != %c retro %c %d\n",
                                i,mf->parentLoc, mf->parentBase, mAxt->tSym[j+i], 
                                mf->retroBase, mf->retroLoc);
                        scoreNeither++;
                        }
                    }
                }
            verbose(2,"final score %s parent %d retro %d  neither %d\n",
                    mPsl->qName, scoreParent, scoreRetro, scoreNeither);
            fprintf(outFile,"%s\t%d\t%d\t%s\t%d\t%s\t%d\t%d\t%s\t%d\t%d\t%d\t%s\t%s\n",
                    ps->chrom, ps->chromStart, ps->chromEnd, ps->name, ps->score, 
                    mPsl->tName, mPsl->tStart, mPsl->tEnd, mPsl->qName, 
                    scoreParent, scoreRetro, scoreNeither, parentMatch->string, retroMatch->string);
            mAxt = mAxt->next;
            }
        dyStringFree(&parentMatch);
        dyStringFree(&retroMatch);
        }
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 9)
    usage();
fileCache = newDlList();
maxGap = optionInt("maxGap", maxGap);
verboseSetLogFile("stdout");
ss = axtScoreSchemeDefault();
verbose(1,"Reading alignments from %s\n",argv[3]);
mrnaHash = readPslToBinKeeper(argv[2], argv[3]);
twoBitFile = twoBitOpen(argv[5]);
//verbose(1,"Reading alignments from %s\n",argv[]);
//pseudoHash = readPslToBinKeeper(argv[3], argv[]);
//verbose(1,"Reading mRNA sequences from %s\n",argv[5]);
//mrnaList = faReadAllMixed(argv[5]);
//if (mrnaList == NULL)
    //errAbort("could not open %s\n",argv[5]);
//faHash = newHash(0);
//for (el = mrnaList; el != NULL ; el = el->next)
    //hashAdd(faHash, el->name, el);

verbose(1,"Reading chains from %s\n",argv[6]);
chainHash = readChainToBinKeeper(argv[2], argv[6]);
outFile = fopen(argv[8],"w");
verbose(1,"Scoring %s\n",argv[1]);
checkExp(argv[1], argv[7], argv[4]);
fclose(outFile);
freeDnaSeqList(&mrnaList);
return(0);
}
