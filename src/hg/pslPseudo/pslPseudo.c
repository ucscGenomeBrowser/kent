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
#include "axt.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "nib.h"
#include "fa.h"
#include "dlist.h"
#include "rmskOut.h"
#include "binRange.h"
#include "cheapcgi.h"
#include "genePred.h"
#include "genePredReader.h"

#define WORDCOUNT 3
#define POLYASLIDINGWINDOW 10
#define POLYAREGION 160
#define INTRONMAGIC 10 /* allow more introns if lots of exons covered - if (exonCover - intronCount > INTRONMAGIC) */
/* label for classification stored in pseudoGeneLink table */
#define PSEUDO 1
#define NOTPSEUDO -1

static char const rcsid[] = "$Id: pslPseudo.c,v 1.11 2004/03/25 06:48:52 baertsch Exp $";

char *db;
char *nibDir;
char *mrnaSeq;
double minAli = 0.98;
double maxRep = 0.35;
double minAliPseudo = 0.60;
double nearTop = 0.01;
double minCover = 0.50;
double minCoverPseudo = 0.01;
int maxBlockGap = 60;
boolean ignoreSize = FALSE;
boolean noIntrons = FALSE;
boolean singleHit = FALSE;
boolean noHead = FALSE;
boolean quiet = FALSE;
int minNearTopSize = 10;
struct genePred *gpList1 = NULL, *gpList2 = NULL, *kgList = NULL;
FILE *bestFile, *pseudoFile, *linkFile, *axtFile;
struct axtScoreScheme *ss = NULL; /* blastz scoring matrix */
struct dnaSeq *mrnaList = NULL; /* list of all input mrna sequences */
struct hash *tHash = NULL;  /* seqFilePos value. */
struct hash *qHash = NULL;  /* seqFilePos value. */
struct dlList *fileCache = NULL;
struct hash *fileHash = NULL;  

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
    "pslPseudo - analyse repeats and generate genome wide best\n"
    "alignments from a sorted set of local alignments\n"
    "usage:\n"
    "    pslPseudo db in.psl sizes.lst rmskDir trf.bed syntenic.bed mrna.psl out.psl pseudo.psl pseudoLink.txt nib.lst mrna.fa refGene.tab mgcGene.tab kglist.tab \n\n"
    "where in.psl is an blat alignment of mrnas sorted by pslSort\n"
    "blastz.psl is an blastz alignment of mrnas sorted by pslSort\n"
    "sizes.lst is a list of chrromosome followed by size\n"
    "rmskDir is the directory containing repeat masker output file\n"
    "trf.bed is the simple repeat (trf) bed file\n"
    "mrna.psl is the blat best mrna alignments\n"
    "out.psl is the best mrna alignment for the gene \n"
    "and pseudo.psl contains pseudogenes\n"
    "pseudoLink.txt will have the link between gene and pseudogene\n"
    "nib.lst list of genome nib file\n"
    "mrna.fa sequence data for all aligned mrnas using blastz\n"
    "options:\n"
    "    -nohead don't add PSL header\n"
    "    -ignoreSize Will not weigh in favor of larger alignments so much\n"
    "    -noIntrons Will not penalize for not having introns when calculating\n"
    "              size factor\n"
    "    -minCover=0.N minimum coverage for mrna to output.  Default is 0.50\n"
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

struct cachedFile
/* File in cache. */
    {
    struct cachedFile *next;	/* next in list. */
    char *name;		/* File name (allocated here) */
    FILE *f;		/* Open file. */
    };

boolean isFa(char *file)
/* Return TRUE if looks like a .fa file. */
{
FILE *f = mustOpen(file, "r");
int c = fgetc(f);
fclose(f);
return c == '>';
}

void addNib(char *file, struct hash *fileHash, struct hash *seqHash)
/* Add a nib file to hashes. */
{
struct seqFilePos *sfp;
char root[128];
int size;
FILE *f = NULL;
splitPath(file, NULL, root, NULL);
AllocVar(sfp);
hashAddSaveName(seqHash, root, sfp, &sfp->name);
sfp->file = hashStoreName(fileHash, file);
sfp->isNib = TRUE;
nibOpenVerify(file, &f, &size);
sfp->pos = size;
}

void addFa(char *file, struct hash *fileHash, struct hash *seqHash)
/* Add a fa file to hashes. */
{
struct lineFile *lf = lineFileOpen(file, TRUE);
char *line, *name;
char root[128];
char *rFile = hashStoreName(fileHash, file);

while (lineFileNext(lf, &line, NULL))
    {
    if (line[0] == '>')
        {
	struct seqFilePos *sfp;
	line += 1;
	name = nextWord(&line);
	if (name == NULL)
	   errAbort("bad line %d of %s", lf->lineIx, lf->fileName);
	AllocVar(sfp);
	hashAddSaveName(seqHash, name, sfp, &sfp->name);
	sfp->file = rFile;
	sfp->pos = lineFileTell(lf);
	}
    }
lineFileClose(&lf);
}

void hashFileList(char *fileList, struct hash *fileHash, struct hash *seqHash)
/* Read file list into hash */
{
if (endsWith(fileList, ".nib"))
    addNib(fileList, fileHash, seqHash);
else if (isFa(fileList))
    addFa(fileList, fileHash, seqHash);
else
    {
    struct lineFile *lf = lineFileOpen(fileList, TRUE);
    char *row[1];
    while (lineFileRow(lf, row))
        {
	char *file = row[0];
	if (endsWith(file, ".nib"))
	    addNib(file, fileHash, seqHash);
	else
	    addFa(file, fileHash, seqHash);
	}
    lineFileClose(&lf);
    }
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

struct dnaSeq *readCachedSeq(char *seqName, struct hash *hash, 
	struct dlList *fileCache)
/* Read sequence hopefully using file cache. */
{
struct dnaSeq *mrna = NULL;
struct dnaSeq *qSeq = NULL;
struct seqFilePos *sfp = hashMustFindVal(hash, seqName);
FILE *f = openFromCache(fileCache, sfp->file);
unsigned options = NIB_MASK_MIXED;
if (sfp->isNib)
    {
    return nibLdPartMasked(options, sfp->file, f, sfp->pos, 0, sfp->pos);
    }
else
    {
    for (mrna = mrnaList; mrna != NULL ; mrna = mrna->next)
        if (sameString(mrna->name, seqName))
            {
            qSeq = mrna;
            break;
            }
    return qSeq;
    }
}

struct dnaSeq *readSeqFromFaPos(struct seqFilePos *sfp,  FILE *f)
/* Read part of FA file. */
{
struct dnaSeq *seq;
fseek(f, sfp->pos, SEEK_SET);
if (!faReadNext(f, "", TRUE, NULL, &seq))
    errAbort("Couldn't faReadNext on %s in %s\n", sfp->name, sfp->file);
return seq;
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
int i;
static int ix = 0;
int qs = psl->qStart, qe = psl->qEnd;
int ts = psl->tStart, te = psl->tEnd;
int symCount = 0;
struct axt *axt = NULL;

AllocVar(axt);
if (psl->strand[0] == '-')
    reverseIntRange(&qs, &qe, psl->qSize);

if (psl->strand[1] == '-')
    reverseIntRange(&ts, &te, psl->tSize);

//if (psl->strand[1] != 0)
//    fprintf(f, "%d %s %d %d %s %d %d %c%c 0\n", ++ix, psl->tName, ts+1, 
//            te, psl->qName, qs+1, qe, psl->strand[1], psl->strand[0]);
//else
//    fprintf(f, "%d %s %d %d %s %d %d %c 0\n", ++ix, psl->tName, psl->tStart+1, 
//            psl->tEnd, psl->qName, qs+1, qe, psl->strand[0]);

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
    	symCount, strlen(q), psl->tName, psl->tStart, psl->qName, t, q);
axt->qSym = cloneString(q);
axt->score = axtScore(axt, ss);
printf("axt score = %d\n",axt->score);
assert(axt->score<800000);
//for (i=0; i<size ; i++) 
//    fputc(t[i],f);
//for (i=0; i<size ; i++) 
//    fputc(q[i],f);
return axt;
}

void writeInsert(struct dyString *aRes, struct dyString *bRes, char *aSeq, int gapSize)
/* Write out gap, possibly shortened, to aRes, bRes. */
{
dyStringAppendN(aRes, aSeq, gapSize);
dyStringAppendMultiC(bRes, '-', gapSize);
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

struct axt *pslToAxt(struct psl *psl, struct hash *qHash, struct hash *tHash,
	struct dlList *fileCache)
{
static char *tName = NULL, *qName = NULL;
static struct dnaSeq *tSeq = NULL, *qSeq = NULL, *mrna;
struct dyString *q = newDyString(16*1024);
struct dyString *t = newDyString(16*1024);
int blockIx, diff;
int qs, ts , symCount;
int lastQ = 0, lastT = 0, size;
int qOffset = 0;
int tOffset = 0;
struct axt *axt = NULL;
boolean qIsNib = FALSE;
boolean tIsNib = FALSE;
int cnt = 0;

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
if (qSeq == NULL)
    errAbort("mrna sequence data not found %s, searched %d sequences\n",psl->qName,cnt);
qName = cloneString(psl->qName);
if (qIsNib && psl->strand[0] == '-')
    qOffset = psl->qSize - psl->qEnd;
else
    qOffset = 0;
printf("qString len = %d qOffset = %d\n",strlen(qSeq->dna),qOffset);
if (tName == NULL || !sameString(tName, psl->tName) || tIsNib)
    {
    freeDnaSeq(&tSeq);
    freez(&tName);
    tName = cloneString(psl->tName);
    readCachedSeqPart(tName, psl->tStart, psl->tEnd-psl->tStart, 
	tHash, fileCache, &tSeq, &tOffset, &tIsNib);
    }
if (tIsNib && psl->strand[1] == '-')
    tOffset = psl->tSize - psl->tEnd;
if (psl->strand[0] == '-')
    reverseComplement(qSeq->dna, qSeq->size);
if (psl->strand[1] == '-')
    reverseComplement(tSeq->dna, tSeq->size);
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
    	strlen(t->string), strlen(q->string), psl->tName, psl->tStart, psl->qName, t->string, q->string);
axt = axtCreate(q->string, t->string, min(q->stringSize,t->stringSize), psl);
dyStringFree(&q);
dyStringFree(&t);
if (qIsNib)
    freez(&qName);
if (tIsNib)
    freez(&tName);
return axt;
}
int calcScore(struct psl *psl)
{
char nibFile[256];
FILE *f;
int seqSize;
int size = (psl->tEnd)-(psl->tStart);
struct dnaSeq *tSeq = NULL;
struct dnaSeq *qSeq = NULL;
struct dnaSeq *mrna = NULL;
errAbort("calcScore");
//safef(nibFile, sizeof(nibFile), "%s/%s.nib", nibDir, psl->tName);
nibOpenVerify(nibFile, &f, &seqSize);
assert(seqSize == psl->tSize);
tSeq = nibLdPart(nibFile, f, seqSize, psl->tStart, size);
for (mrna = mrnaList; mrna != NULL ; mrna = mrna->next)
    if (sameString(mrna->name, psl->qName))
        {
        qSeq = mrna;
        break;
        }
return axtScoreSym(ss, size, qSeq->dna, tSeq->dna);
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
bed->score = (milliBad - (100-log(polyA)*20) - (overlapDiagonal*2) - (100-coverage) + log(psl->match+psl->repMatch)*100)/2 ;
bedOutputN( bed , 6, linkFile, ' ', ' ');
fprintf(linkFile,"%s %s %s %s %d %d %s %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d ",
        /* 8     9     10      11       12          13 */
        db, type, bestqName, besttName, besttStart, besttEnd, 
        (bestStrand[0] == '+' || bestStrand[0] == '-') ? bestStrand : "0", 
        /* 14           15      16 */
        maxExons, geneOverlap, polyA, 
        /* polyAstart 17*/
        (psl->strand[0] == '+') ? polyAstart - psl->tEnd : psl->tStart - polyAstart , 
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

struct axt *axt = NULL;
int score = -999999999;

if (label == 1)
    {
    axt = pslToAxt(psl, qHash, tHash, fileCache);
    score = axtScore(axt, ss);
    axtWrite(axt, axtFile);
    }

fprintf(linkFile,"%d ",score);
if (gp != NULL)
    {
    fprintf(linkFile,"%s %d %d ", gp->name, gp->txStart, gp->txEnd);
    }
else
    {
    fprintf(linkFile,"noRefSeq -1 -1 ");
    }

if (mgc != NULL)
    {
    fprintf(linkFile,"%s %d %d ", mgc->name, mgc->txStart, mgc->txEnd);
    }
else
    {
    fprintf(linkFile,"noMgc -1 -1 ");
    }

if (kg != NULL)
    {
    if (kg->name2 != NULL)
        {
        fprintf(linkFile,"%s %d %d %d ", kg->name2, kg->txStart, kg->txEnd, 0);
        }
    else
        {
        fprintf(linkFile,"%s %d %d %d ", kg->name, kg->txStart, kg->txEnd, 0);
        }
    }
else
    {
    fprintf(linkFile,"noKg -1 -1 -1 ");
    }
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


boolean closeToTop(struct psl *psl, int *scoreTrack , int milliScore) /*struct hash *bkHash, struct hash *trfHash)*/
/* Returns TRUE if psl is near the top scorer for at least 20 bases. */
{
//int milliScore = calcSizedScore(psl, bkHash, trfHash);
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
    //uglyf("block %d threshold=%d s-e:%d-%d. ",blockIx, threshold, start, end);
    for (i=start; i<end; ++i)
	{
        //uglyf("s=%d tc=%d ",scoreTrack[i],topCount);
	if (scoreTrack[i] <= threshold)
	    {
	    if (++topCount >= minNearTopSize)
                {
                //uglyf("\nYES\n");
		return TRUE;
                }
	    }
	}
        //uglyf("\nblock NO\n");
    }
    //uglyf("\nctt=false\n");
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

int polyACalc(int start, int end, char *strand, int tSize, char *chrom, int winSize, int region, int *polyAstart, int *polyAend)
/* get size of polyA tail in genomic dna , count bases in a 
 * sliding winSize window in region of the end of the sequence*/
{
char nibFile[256];
int seqSize;
struct dnaSeq *seq = NULL;
int count = 0;
int seqStart = strand[0] == '+' ? end-(region/2) : start-(region/2);
int score[POLYAREGION+1], pStart = 0, pEnd = 0; 
struct seqFilePos *sfp = hashMustFindVal(tHash, chrom);
FILE *f = openFromCache(fileCache, sfp->file);
seqSize = sfp->pos;

assert(region > 0);
assert(end != 0);
*polyAstart = 0 , *polyAend = 0;
//safef(nibFile, sizeof(nibFile), "%s/%s.nib", nibDir, chrom);
//nibOpenVerify(nibFile, &f, &seqSize);
printf("polyA file %s size %d\n",sfp->file, seqSize);
assert (seqSize == tSize);
if (seqStart < 0) seqStart = 0;
if (seqStart + region > seqSize) region = seqSize - seqStart;
assert(region > 0);
seq = nibLdPartMasked(NIB_MASK_MIXED, nibFile, f, seqSize, seqStart, region);
if (strand[0] == '+')
    {
    assert (seq->size <= POLYAREGION);
printf("\n + range=%d %d %s \n",seqStart, seqStart+region, seq->dna );
    count = scoreWindow('A',seq->dna,seq->size, score, polyAstart, polyAend);
    }
else
    {
    assert (seq->size <= POLYAREGION);
printf("\n - range=%d %d %s \n",seqStart, seqStart+region, seq->dna );
    count = scoreWindow('T',seq->dna,seq->size, score, polyAend, polyAstart);
    }
pStart += seqStart;
*polyAstart += seqStart;
*polyAend += seqStart;
printf("\nst=%d %s range %d %d cnt %d\n",seqStart, seq->dna, *polyAstart, *polyAend, count);
freeDnaSeq(&seq);
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
void processBestMulti(char *acc, struct psl *pslList, struct hash *trfHash, struct hash *synHash, struct hash *mrnaHash, struct hash *bkHash)
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

/* get biggest mrna to size Score array - some have polyA tail stripped off */
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
//            uglyf("milliScore: %d qName %s tName %s:%d-%d qSize %d psl->qSize %d start %d end %d \n",
 //                   milliScore, psl->qName, psl->tName, psl->tStart, psl->tEnd, qSize, psl->qSize, start, end);
	    for (i=start; i<end; ++i)
		{
                assert(i<=qSize);
		if (milliScore > scoreTrack[i])
                    {
                    //printf(" %d ",milliScore);
                    scoreTrack[i] = milliScore;
                    }
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
    int score = calcSizedScore(psl, bkHash, trfHash);
    
   /* uglyf("checking: %d qName %s tName %s:%d-%d psl->qSize %d start %d end %d match %d >= cover %d\n",
    *           calcSizedScore(psl, bkHash, trfHash), 
    *           psl->qName, psl->tName, psl->tStart, psl->tEnd, psl->qSize, psl->qStart, psl->qEnd
    *           ,(psl->match + psl->repMatch) , round(minCover * psl->qSize));
    */
    if (
        calcMilliScore(psl) >= milliMin && closeToTop(psl, scoreTrack, score) /*bkHash, trfHash)*/
        && (psl->match + psl->repMatch + psl->misMatch) >= round(minCover * psl->qSize))
	{
        ++bestAliCount;
        AllocVar(pslMerge);
        pslMergeBlocks(psl, pslMerge, 30);
        
//        uglyf("checking score %d against %d blockCount %d\n", score ,bestScore,pslMerge->blockCount);
        if (score  > bestScore && pslMerge->blockCount > 1)
            {
            bestPsl = psl;
            bestStart = psl->tStart;
            bestEnd = psl->tEnd;
            bestChrom = cloneString(psl->tName);
//            uglyf("new score %d beats %d\n", score ,bestScore);
            bestScore = score;
            }
        if (score  > bestSEScore )
            {
            bestSEPsl = psl;
            bestSEStart = psl->tStart;
            bestSEEnd = psl->tEnd;
            bestSEChrom = cloneString(psl->tName);
//            uglyf("new SE score %d beats %d\n", score ,bestSEScore);
            bestSEScore = score  ;
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
    int score = calcSizedScore(psl, bkHash, trfHash);
    if (
        calcMilliScore(psl) >= milliMin && closeToTop(psl, scoreTrack, score) /*bkHash, trfHash)*/
        && psl->match + psl->misMatch + psl->repMatch >= minCover * psl->qSize)
            {
            pslTabOut(psl, bestFile);
            }
    else 
        {
        /* calculate various features of pseudogene */
        int intronCount = intronFactor(psl, bkHash, trfHash);
        int overlapDiagonal = -1;
        int polyA = polyACalc(psl->tStart, psl->tEnd, psl->strand, psl->tSize, psl->tName, 
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

        if ((intronCount == 0 || (exonCover - intronCount > INTRONMAGIC)) && 
            maxExons > 1 && bestAliCount > 0 && bestChrom != NULL &&
            (calcMilliScore(psl) >= milliMinPseudo && 
            psl->match + psl->misMatch + psl->repMatch >= minCoverPseudo * (float)psl->qSize))
            {

            if (exonCover > 1 && (intronCount == 0 || (exonCover - intronCount > INTRONMAGIC))&& 
                maxExons > 1 && bestAliCount > 0 && bestChrom != NULL &&
                (calcMilliScore(psl) >= milliMinPseudo && 
                psl->match + psl->misMatch + psl->repMatch >= minCoverPseudo * (float)psl->qSize))
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
//                    printf("mrnaBases %d exons %d name %s pos %s:%d-%d\n",
//                            mrnaBases,exonCount,mPsl == NULL ? "NULL" : bestOverlap, psl->tName,psl->tStart,psl->tEnd);
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
                            printf("YES %s %d rr %3.1f rl %d ln %d %s iF %d maxE %d bestAli %d isp %d score %d match %d cover %3.1f rp %d polyA %d syn %d distA %d polyAstart %d",
                                psl->qName,psl->tStart,((float)rep/(float)(psl->tEnd-psl->tStart) ),rep, 
                                psl->tEnd-psl->tStart,psl->tName, intronCount, 
                                maxExons , bestAliCount, exonCover,
                                calcMilliScore(psl),  psl->match + psl->misMatch + psl->repMatch , 
                                minCoverPseudo * (float)psl->qSize, tReps + qReps, polyA, overlapDiagonal ,
                                (psl->strand[0] == '+') ? polyAstart - psl->tEnd : psl->tStart - (polyAstart + polyA), polyAstart );
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
                            calcMilliScore(psl),  psl->match + psl->misMatch + psl->repMatch , 
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
                        calcMilliScore(psl),  psl->match + psl->misMatch + psl->repMatch , minCoverPseudo * (float)psl->qSize, tReps + qReps);
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

void processBestSingle(char *acc, struct psl *pslList, struct hash *trfHash, struct hash *synHash, struct hash *mrnaHash, struct hash *bkHash)
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
        struct hash *bkHash)
/* Process alignments of one piece of mRNA. */
{
if (singleHit)
    processBestSingle(acc, pslList, trfHash, synHash, mrnaHash, bkHash);
else
    processBestMulti(acc, pslList, trfHash, synHash, mrnaHash, bkHash);
}

void pslPseudo(char *inName, struct hash *bkHash, struct hash *trfHash, struct hash *synHash, struct hash *mrnaHash, 
        char *bestAliName, char *psuedoFileName, char *linkFileName, char *axtFileName)
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
axtFile = mustOpen(axtFileName, "w");

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
//    struct axt *axt = pslToAxt(psl, qHash, tHash, fileCache);
//    axtWrite(axt, stdout);
    if (!sameString(lastName, psl->qName))
	{
        slReverse(&pslList);
	doOneAcc(lastName, pslList, trfHash, synHash, mrnaHash, bkHash);
	pslFreeList(&pslList);
	safef(lastName, sizeof(lastName), psl->qName);
	}
    slAddHead(&pslList, psl);
    }
slReverse(&pslList);
doOneAcc(lastName, pslList, trfHash, synHash, mrnaHash, bkHash);
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
ss = axtScoreSchemeDefault();
fileHash = newHash(0);  
tHash = newHash(20);  /* seqFilePos value. */
qHash = newHash(20);  /* seqFilePos value. */
fileCache = newDlList();
if (argc != 17)
    usage();
db = cloneString(argv[1]);
nibDir = cloneString(argv[11]);
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
    printf("Scanning %s\n", argv[12]);
hashFileList(argv[12], fileHash, tHash);
if (!quiet)
    printf("Loading mrna sequences from %s\n",argv[13]);
mrnaList = faReadAllMixed(argv[13]);
if (mrnaList == NULL)
    errAbort("could not open %s\n",argv[13]);
gpList1 = genePredLoadAll(argv[14]);
gpList2 = genePredLoadAll(argv[15]);
//gprKg = genePredReaderFile(argv[16], NULL);
//kgLis = genePredReaderAll(gprKg);
kgList = genePredLoadAll(argv[16]);

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
printf("start\n");
pslPseudo(argv[2], bkHash, trfHash, synHash, mrnaHash, argv[8], argv[9], argv[10], argv[11]);
return 0;
}
