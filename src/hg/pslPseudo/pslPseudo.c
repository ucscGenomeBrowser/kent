/* pslPseudo - analyse repeats and generate list of processed pseudogenes
 * from a genome wide, sorted by mRNA .psl alignment file.
 */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "memalloc.h"
#include "jksql.h"
//#include "hdb.h"
//#include "sqlnum.h"
#include "psl.h"
#include "obscure.h"
#include "bed.h"
#include "axt.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "nib.h"
#include "fa.h"
#include "dlist.h"
#include "binRange.h"
#include "options.h"
#include "genePred.h"
#include "genePredReader.h"
#include "dystring.h"
#include "pseudoGeneLink.h"
#include "twoBit.h"
#include "chainNet.h"
//#include "scoreWindow.h" TODO fix this
#include "verbose.h"

#define ScoreNorm 3
#define BEDCOUNT 3
#define POLYASLIDINGWINDOW 10
#define POLYAREGION 70
#define INTRONMAGIC 10 /* allow more introns if lots of exons covered - if (exonCover - intronCount > INTRONMAGIC) */

/* label for classification stored in pseudoGeneLink table */
#define PSEUDO 1
#define NOTPSEUDO -1
#define EXPRESSED -2

static char const rcsid[] = "$Id: pslPseudo.c,v 1.49 2006/05/28 23:32:26 baertsch Exp $";

char *db;
char *nibDir;
char *mrnaSeq;
int verbosity = 1;
float minAli = 0.98;
float maxRep = 0.60;
float minAliPseudo = 0.60;
float nearTop = 0.005;
float repsPerIntron = 0.7;
float splicedOverlapRatio = 0.1;
float minCover = 0.50;
float minCoverPseudo = 0.01;
int maxBlockGap = 50;
int intronSlop = 35;
int spliceDrift = 7;
int scoreThreshold = 425;
float intronRatio = 1.5;
bool ignoreSize = FALSE;
bool noIntrons = FALSE;
bool noHead = FALSE;
bool skipExciseRepeats = FALSE;
bool stripVersion = FALSE;
bool showall = FALSE;
double  wt[12];     /* weights on score function*/
int minNearTopSize = 10;
struct genePred *gpList1 = NULL, *gpList2 = NULL, *kgList = NULL, *mrnaGene = NULL;
FILE *bestFile, *pseudoFile, *linkFile, *axtFile;
struct twoBitFile *twoBitFile = NULL;
struct axtScoreScheme *ss = NULL; /* blastz scoring matrix */
//struct dnaSeq *mrnaList = NULL; 
struct slName *mrnaList = NULL;/* list of all input mrna sequence names  */
struct hash *tHash = NULL;  /* seqFilePos value. */
struct hash *qHash = NULL;  /* seqFilePos value. */
struct dlList *fileCache = NULL;
struct hash *fileHash = NULL;  
char mrnaOverlap[255];
struct hash *rmskHash = NULL, *trfHash = NULL, *synHash = NULL, *syn2Hash, *syn3Hash, *mrnaHash = NULL;

/* command line */
static struct optionSpec optionSpecs[] = {
    {"species1", OPTION_STRING},
    {"species2", OPTION_STRING},
    {"nibdir1", OPTION_STRING},
    {"nibdir2", OPTION_STRING},
    {"mrna1", OPTION_STRING},
    {"mrna2", OPTION_STRING},
    {"bedOut", OPTION_STRING},
    {"score", OPTION_STRING},
    {"snp", OPTION_STRING},
    {"minDiff", OPTION_INT},
    {"computeSS", OPTION_BOOLEAN},
    {"nohead", OPTION_BOOLEAN},
    {"ignoreSize", OPTION_BOOLEAN},
    {"noIntrons", OPTION_BOOLEAN},
    {"minCover", OPTION_FLOAT},
    {"minCoverPseudo", OPTION_FLOAT},
    {"minAli", OPTION_FLOAT},
    {"minAliPseudo", OPTION_FLOAT},
    {"splicedOverlapRatio", OPTION_FLOAT},
    {"intronSlop", OPTION_INT},
    {"nearTop", OPTION_FLOAT},
    {"minNearTopSize", OPTION_INT},
    {"maxBlockGap", OPTION_INT},
    {"maxRep", OPTION_INT},
    {"stripVersion", OPTION_BOOLEAN},
    {"showall", OPTION_BOOLEAN},
    {NULL, 0}
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
void usage(int count)
/* Print usage instructions and exit. */
{
errAbort(
    "pslPseudo - analyse repeats and generate genome wide best\n"
    "alignments from a sorted set of local alignments\n"
    "usage:\n"
    "    pslPseudo db in.psl sizes.lst rmsk.bed mouseNet.txt dogNet.txt trf.bed all_mrna.psl out.psl outPseudo.psl outPseudoLink.txt out.axt nib.lst mrna.2bit refGene.tab mgcGene.tab kglist.tab rheMac2Net.txt\n\n"
    "where in.psl is an blat alignment of mrnas sorted by pslSort\n"
    "blastz.psl is an blastz alignment of mrnas sorted by pslSort\n"
    "sizes.lst is a list of chromosome followed by size\n"
    "rmsk.bed is the repeat masker bed file\n"
    "mouseNet.txt = select tName, tStart, tEnd, level, qName, qStart, qEnd, type from netMm6\n"
    "dogNet.txt = select tName, tStart, tEnd, level, qName, qStart, qEnd, type from netCanFam2\n"
    "trf.bed is the simple repeat (trf) bed file\n"
    "all_mrna.psl is the blat best mrna alignments\n"
    "out.psl is the best mrna alignment for the gene \n"
    "outPseudo.psl contains pseudogenes\n"
    "outPseudoLink.txt will have the link between gene and pseudogene\n"
    "out.axt contains the pseudogene aligned to the gene (can be /dev/null)\n"
    "nib.lst list of genome nib file\n"
    "mrna.2bit  sequence data for all aligned mrnas using blastz\n"
    "refGene.tab refseq annotations for finding parent gene (genePred format)\n"
    "mgcGene.tab mgc annotations for finding parent gene (genePred format)\n"
    "kglist.tab knownGene annotations for finding parent gene (genePred format)\n"
    "mrnaGene.tab mrna cds  annotations (genePred format)\n"
    "rhesusNet.txt = select tName, tStart, tEnd, level, qName, qStart, qEnd, type from netRheMac2\n"
    "options:\n"
    "    -nohead don't add PSL header\n"
    "    -ignoreSize Will not weigh in favor of larger alignments so much\n"
    "    -noIntrons Will not penalize for not having introns when calculating size factor\n"
    "    -minCover=0.N minimum coverage for mrna to output, default %4.3f\n"
    "    -minCoverPseudo=0.N minimum coverage of pseudogene to output, default %4.3f\n"
    "    -minAli=0.N minimum alignment ratio for mrna, default %4.3f\n"
    "    -minAliPseudo=0.N minimum alignment ratio for pseudogenes, default %4.3f\n"
    "    -splicedOverlapRatio=0.N max overlap with spliced mrna,  default %4.3f\n"
    "    -intronSlop=N max delta of intron position on q side alignment, default %d bp\n"
    "    -nearTop=0.N how much can deviate from top and be taken, default %4.3f\n"
    "    -minNearTopSize=N  Minimum size of alignment that is near top for aligmnent to be kept,  default %d.\n"
    "    -maxBlockGap=N  Max gap size between adjacent blocks that are combined, default %d.\n"
    "    -maxRep=N  max ratio of overlap with repeat masker track \n"
    "               for aligmnent to be kept,  default %4.3f\n"
    "    -stripVersion  ignore version number of mRNA in input file \n"
    "    -showall  do not eliminate low scoring hits from output file , default=false\n\nexpecting argc=20 got %d"
       , minCover, minCoverPseudo, minAli, minAliPseudo, splicedOverlapRatio, 
       intronSlop, nearTop, minNearTopSize, maxBlockGap, maxRep, count );
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
fclose(f);
}

void addFa(char *file, struct hash *fileHash, struct hash *seqHash)
/* Add a fa file to hashes. */
{
struct lineFile *lf = lineFileOpen(file, TRUE);
char *line, *name;
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

bool samePrefix(char *string1, char *string2)
/* check if accessions match ignoring suffix and version */
{
char buf1[256], buf2[256];
char *name1[3];
char *name2[3];
safef(buf1, sizeof(buf1), "%s",string1);
safef(buf2, sizeof(buf2), "%s",string2);
chopString(buf1, ".", name1, ArraySize(name1));
chopString(buf2, ".", name2, ArraySize(name2));
if (name1[0] == NULL)
    name1[0] = string1;
if (name2[0] == NULL)
    name2[0] = string2;
if (sameString(name1[0], name2[0]))
    {
    return TRUE;
    }
return FALSE;
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

//struct dnaSeq *readCachedSeq(char *seqName, struct hash *hash, 
//	struct dlList *fileCache)
/* Read sequence hopefully using file cache. */
/*{
struct dnaSeq *mrna = NULL;
struct dnaSeq *qSeq = NULL;
assert(seqName != NULL);
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

struct hash *readBedCoordToBinKeeper(char *sizeFileName, char *bedFileName, int wordCount)
/* read a list of beds and return results in hash of binKeeper structure for fast query*/
/* free bed in binKeeper to save memory only start/end coord */
{
struct binKeeper *bk; 
struct bed *bed;
struct lineFile *lf = lineFileOpen(sizeFileName, TRUE);
struct lineFile *bf = lineFileOpen(bedFileName , TRUE);
struct hash *hash = newHash(0);
char *chromRow[2];
char *row[3] ;

assert (wordCount == 3);
while (lineFileRow(lf, chromRow))
    {
    char *name = chromRow[0];
    int size = lineFileNeedNum(lf, chromRow, 1);

    if (hashLookup(hash, name) != NULL)
        warn("Duplicate %s, ignoring all but first\n", name);
    else
        {
        bk = binKeeperNew(0, size);
        assert(size > 1);
	hashAdd(hash, name, bk);
        }
    }
while (lineFileNextRow(bf, row, ArraySize(row)))
    {
    bed = bedLoadN(row, wordCount);
    bk = hashMustFindVal(hash, bed->chrom);
    binKeeperAdd(bk, bed->chromStart, bed->chromEnd, bed);
    bedFree(&bed);
    }
lineFileClose(&bf);
lineFileClose(&lf);
return hash;
}

struct hash *readNetToBinKeeper(char *sizeFileName, char *netFileName)
/* read a truncated net table and return results in hash of binKeeper structure for fast query*/
/* free net in binKeeper to save memory only start/end coord */
/*  select tName, tStart, tEnd, level, qName, qStart, qEnd, type from netMm6 */
{
struct binKeeper *bk; 
struct lineFile *lf = lineFileOpen(sizeFileName, TRUE);
struct lineFile *nf = lineFileOpen(netFileName , TRUE);
struct hash *hash = newHash(0);
char *chromRow[2];
char *row[8] ;

while (lineFileRow(lf, chromRow))
    {
    char *name = chromRow[0];
    int size = lineFileNeedNum(lf, chromRow, 1);

    if (hashLookup(hash, name) != NULL)
        warn("Duplicate %s, ignoring all but first\n", name);
    else
        {
        bk = binKeeperNew(0, size);
        assert(size > 1);
	hashAdd(hash, name, bk);
        }
    }
while (lineFileNextRow(nf, row, ArraySize(row)))
    {
//    char *chrom = cloneString(row[0]);
    int chromStart = sqlUnsigned(row[1]);
    int chromEnd = sqlUnsigned(row[2]);
    int level = sqlUnsigned(row[3]);
    if (differentString(row[7],"gap"))
        {
        bk = hashMustFindVal(hash, row[0]);
        binKeeperAdd(bk, chromStart, chromEnd, intToPt(level));
        }
    }
lineFileClose(&nf);
lineFileClose(&lf);
return hash;
}

/* get accession before version and suffix */
char *getPrefix(char *acc)
{
char *name[3];
static char buf[255];
int splitCount = 0;
safef(buf, sizeof(buf), "%s",acc);
splitCount = chopString(buf, ".", name, ArraySize(name));
if (splitCount == 0)
    safef(name[0], sizeof(buf), "%s",acc);
return name[0];
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
axt->qStart = qs;
axt->qEnd = qe;
axt->qStrand = psl->strand[0];
axt->tStrand = '+';
if (psl->strand[1] != 0)
    {
    axt->tStart = ts;
    axt->tEnd = te;
    }
else
    {
    axt->tStart = psl->tStart;
    axt->tEnd = psl->tEnd;
    }
axt->symCount = symCount = strlen(t);
axt->tSym = cloneString(t);
if (strlen(q) != symCount)
    warn("Symbol count %d != %d inconsistent at t %s:%d and qName %s\n%s\n%s\n",
    	symCount, strlen(q), psl->tName, psl->tStart, psl->qName, t, q);
axt->qSym = cloneString(q);
axt->score = axtScoreFilterRepeats(axt, ss);
verbose(1,"axt score = %d\n",axt->score);
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
static struct dnaSeq *tSeq = NULL, *qSeq = NULL;
static struct slName *mrna;
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
char name[512];
//int cnt = 0;

freeDnaSeq(&qSeq);
freez(&qName);
assert(mrnaList != NULL);
for (mrna = mrnaList; mrna != NULL ; mrna = mrna->next)
    {
    safef(name, sizeof(name), "%s", psl->qName);
    chopSuffixAt(name, '-');
    assert(mrna != NULL);
    if (sameString(mrna->name, name))
        {
        qSeq = twoBitReadSeqFrag(twoBitFile, name, 0, 0);
        assert(qSeq != NULL);
        if(abs((qSeq->size)-psl->qSize) >= 3)
        {
            warn("Error: psl %s qSize = %d and sequence len is %d\n",
                    name, psl->qSize, qSeq->size);
            verbose(1,"Error: psl %s qSize = %d and sequence len is %d\n",
                    name, psl->qSize, qSeq->size);
        }
        assert(abs((qSeq->size)-psl->qSize) < 3);
        break;
        }
    }
if (qSeq == NULL)
    {
    warn("mrna sequence data not found %s\n",name);
    dyStringFree(&q);
    dyStringFree(&t);
    dnaSeqFree(&tSeq);
    dnaSeqFree(&qSeq);
    return NULL;
    }
if (qSeq->size != psl->qSize)
    {
    warn("sequence %s aligned is different size %d from mrna.fa file %d \n",name,psl->qSize,qSeq->size);
    verbose(2,"sequence %s aligned is different size %d from mrna.fa file %d \n",name,psl->qSize,qSeq->size);
    dyStringFree(&q);
    dyStringFree(&t);
    dnaSeqFree(&tSeq);
    dnaSeqFree(&qSeq);
    return NULL;
    }
qName = cloneString(name);
if (qIsNib && psl->strand[0] == '-')
    qOffset = psl->qSize - psl->qEnd;
else
    qOffset = 0;
verbose(6,"qString len = %d qOffset = %d\n",strlen(qSeq->dna),qOffset);
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
verbose(6,"tString len = %d tOffset = %d\n",strlen(tSeq->dna),tOffset);
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
    	strlen(t->string), strlen(q->string), psl->tName, psl->tStart, name[0], t->string, q->string);
//if (psl->strand[0] == '-')
//    {
//    reverseComplement(q->string, q->stringSize);
//    reverseComplement(t->string, t->stringSize);
//    }
axt = axtCreate(q->string, t->string, min(q->stringSize,t->stringSize), psl);
dyStringFree(&q);
dyStringFree(&t);
dnaSeqFree(&tSeq);
dnaSeqFree(&qSeq);
if (qIsNib)
    freez(&qName);
if (tIsNib)
    freez(&tName);
return axt;
}

void binKeeperPslHashFree(struct hash **hash)
{
if (*hash != NULL)
    {
    struct hashEl *hashEl = NULL;
    struct hashCookie cookie = hashFirst(*hash);
    while ((hashEl = hashNext(&cookie)) != NULL)
        {
        struct binKeeper *bk = hashEl->val;
        struct binElement *elist = NULL, *el = NULL;;
        elist = binKeeperFindAll(bk) ;
        for (el = elist; el != NULL ; el = el->next)
            {
            struct psl *psl = el->val;
            pslFreeList(&psl);
            }
        binKeeperFree(&bk);
        }
    hashFree(hash);
    }
}

void binKeeperHashFree(struct hash **hash)
{
if (*hash != NULL)
    {
    struct hashEl *hashEl = NULL;
    struct hashCookie cookie = hashFirst(*hash);
    while ((hashEl = hashNext(&cookie)) != NULL)
        {
        struct binKeeper *bk = hashEl->val;
        binKeeperFree(&bk);
        }
    hashFree(hash);
    }
}

bool checkForGene(struct genePred **list, char *table, char *chrom, int cStart, int cEnd, char *name, int *retOverlap)
{
/* check if this coor/name matches something in gene list
   Cache the list of genes to so we only read it once */

struct genePred *el = NULL, *bestMatch = NULL;
int overlap = 0 , bestOverlap = 0, i;


if (*list == NULL)
    return FALSE;
for (el = *list; el != NULL; el = el->next)
    {
    if (chrom != NULL && el->chrom != NULL)
        {
        overlap = 0;
        if ( sameString(chrom, el->chrom))
            {
            for (i = 0 ; i<(el->exonCount); i++)
                {
                overlap += positiveRangeIntersection(cStart,cEnd, el->exonStarts[i], el->exonEnds[i]) ;
                }
            if (overlap > 20 && sameString(name, el->name))
                {
                return TRUE;
                bestMatch = el;
                bestOverlap = overlap;
                *retOverlap = bestOverlap;
                }
            if (overlap > bestOverlap)
                {
                bestMatch = el;
                bestOverlap = overlap;
                *retOverlap = bestOverlap;
                }
            }
        }
    }

return FALSE;
}
int makeGenePred(struct pseudoGeneLink *pg, struct psl *psl)
/* return 0 if no gene pred or positive number to bump the score to favor parents with CDS annotation */
{
int geneOverlap = 0;
char name[512];
if (psl == NULL) 
    return 0;
safef(name, sizeof(name), "%s", psl->qName);
chopSuffix(name);
if (checkForGene(&gpList2, "mgcGene", psl->tName, psl->tStart, 
                        psl->tEnd , name, &geneOverlap))
    return 100;
if (checkForGene(&gpList1, "refGene", psl->tName, psl->tStart, 
                        psl->tEnd , name, &geneOverlap))
    return 80;
if (checkForGene(&kgList, "knownGene", psl->tName, psl->tStart, 
                        psl->tEnd , name, &geneOverlap))
    return 50;
return 0;
}

int calcMilliScore(struct psl *psl)
/* Figure out percentage score. */
{
return 1000-pslCalcMilliBad(psl, TRUE);
}

void outputNoLinkScore(struct psl *psl, struct pseudoGeneLink *pg)
    
/* output bed record with pseudogene details */
{
int blockCount;
int i, *chromStarts, chromStart;
pg->oldScore = (pg->milliBad - (100-log(pg->polyAlen)*20) - (pg->overlapMouse*2) - (100-pg->coverage) 
        + log(psl->match+psl->repMatch)*100)/2 ;

pg->chrom = cloneString(psl->tName);
pg->chromStart = pg->thickStart = chromStart = psl->tStart;
pg->chromEnd = pg->thickEnd = psl->tEnd;
strncpy(pg->strand,  psl->strand, sizeof(pg->strand));
pg->blockCount = blockCount = psl->blockCount;
pg->blockSizes = (int *)cloneMem(psl->blockSizes,(sizeof(int)*psl->blockCount));
pg->chromStarts = chromStarts = (int *)cloneMem(psl->tStarts, (sizeof(int)*psl->blockCount));
pg->name = cloneString(psl->qName);

/* Switch minus target strand to plus strand. */
if (psl->strand[1] == '-')
    {
    int chromSize = psl->tSize;
    reverseInts(pg->blockSizes, blockCount);
    reverseInts(chromStarts, blockCount);
    for (i=0; i<blockCount; ++i)
	chromStarts[i] = chromSize - chromStarts[i];
    }

/* Convert coordinates to relative. */
for (i=0; i<blockCount; ++i)
    chromStarts[i] -= chromStart;

//bedOutputN( bed , 12, linkFile, '\t', '\t');
if (pg->type == NULL)
    pg->type = cloneString("NONE");
if (strlen(pg->type)<=1)
    pg->type = cloneString("NONE");
if (pg->gChrom==NULL)
    pg->gChrom = cloneString("NONE");
pg->polyAstart = (psl->strand[0] == '+') ? pg->polyAstart - psl->tEnd : psl->tStart - pg->polyAstart ;
pg->matches = psl->match+psl->repMatch ;
pg->qSize = psl->qSize;
pg->qEnd = psl->qSize - psl->qEnd;
if (pg->overName == NULL)
    pg->overName = cloneString("none");
if (strlen(pg->overStrand) < 1)
    {
    pg->overStrand[0] = '0'; 
    pg->overStrand[1] = '\0'; 
    }
pseudoGeneLinkOutput(pg, linkFile, '\t','\n');
        /* bed 1-12 */
        /* 13        14   15        16         17           18  */
//        trfRatio, type, axtScore, bestgName, bestgStart, bestgEnd, 
        /*   19  */
//        (bestStrand[0] == '+' || bestStrand[0] == '-') ? bestStrand : "0", 
        /* 20           21      22 */
//        exonCount, geneOverlap, polyA, 
        /* polyAstart 23*/
//        (psl->strand[0] == '+') ? polyAstart - psl->tEnd : psl->tStart - polyAstart , 
        /* 24           25              26      27 */
//        exonCover, intronCount, bestAliCount, psl->match+psl->repMatch, 
        /* 28           29                30    31      32 */
//        psl->qSize, psl->qSize-psl->qEnd, rep, qReps, overlapMouse, 
        /* 33      34      35        36         37              38,                39        40          */
//        coverage, label, milliBad, oldScore, oldIntronCount, conservedIntrons, consSplice, maxOverlap, 
        /* 41      42      43     44      45     46,      47        48      49        50        51          52         53     */
//        *refSeq , rStart, rEnd, *mgc , mStart , mEnd , *kgName , kStart , kEnd , *overName , overStart , overEnd , overStrand[3]
        /* 54           55        56 */
//        overlapDog , posConf , polyAlen
}

void initWeights()
{
/*
    0 = + milliBad
  2 1 = + exon Coverage
  8 2 = + log axtScore
  1 3 = + log polyAlen
  5 4 = + max(overlapMouse overlapDog)
    5 = - conservedIntrons
  3 6 = + intronCount ^.5
  4 7 = - log maxOverlap
  7 8 = + coverage *((qSize-qEnd)/qSize)
  6 9 = - repeats
    10 = NOT USED log (qSize - qEnd)
 */
wt[0] = 0.2; wt[1] = 0.85; wt[2] = 0.3; wt[3] = 0.4; wt[4] = 1; 
wt[5] = 0.85; wt[6] = 1  ; wt[7] = 0.5; wt[8] = 1; wt[9] = 1; wt[10] = 1;

}
void outputLink(struct psl *psl, struct pseudoGeneLink *pg , struct dyString *reason, struct psl *bestPsl)
   /* char *type, char *bestqName, char *besttName, 
                int besttStart, int besttEnd, int maxExons, int geneOverlap, 
                char *bestStrand, int polyA, int polyAstart, int label, 
                int exonCover, int intronCount, int bestAliCount, 
                int tReps, int qReps, int overlapMouse, 
                struct genePred *kg, struct genePred *mgc, struct genePred *gp, 
                int oldIntronCount, struct dyString *iString, int conservedIntrons, int maxOverlap) */
/* output bed record with pseudogene details and link to gene*/
{
struct axt *axt = NULL;
int pseudoScore = 0;
int bump = 0;
float maxOverlap = (float)pg->maxOverlap/(float)(psl->match+psl->misMatch+psl->repMatch)  ;
int overlapOrtholog = max(pg->overlapMouse, pg->overlapDog);
int exonsSpliced = pg->exonCover-pg->conservedSpliceSites;
/* if retroGene has no overlap with orthologous DNA, assume it is a new insertion
   and give it a proxy orthlog score with 70% overlap with parent */
if (pg->overlapMouse == -10 && pg->overlapDog == -10)
    overlapOrtholog = 70;
verbose(3,"%s mouse %d dog %d composite %d\n",
        psl->qName, pg->overlapMouse, pg->overlapDog, overlapOrtholog);
pg->milliBad = calcMilliScore(psl);
pg->axtScore = -1;
pg->type = reason->string;


/* convert to axt and compute axt score */
if (pg->label == PSEUDO || pg->label == EXPRESSED) 
    {
    axt = pslToAxt(psl, qHash, tHash, fileCache);
    if (axt != NULL)
        {
        pg->axtScore = axtScoreFilterRepeats(axt, ss);
        verbose(4,"axt Score %d q len %d t len %d \n",pg->axtScore, strlen(axt->qSym), strlen(axt->tSym));
        }
    }

/* map orf from parent and write genePred */
/* bump score more for knownGene, mgc and refSeq parents or parents valid ORF */
bump = makeGenePred(pg, bestPsl);
/* all weighted features are scaled to range from 0 to 1000 */
assert(psl->qSize > 0);
pseudoScore = ( wt[0]*pg->milliBad 
                + wt[1]*(log(pg->exonCover+1)/log(2))*200 
                + wt[2]*log(pg->axtScore>0?pg->axtScore:1)*70 
                + wt[3]*(log(pg->polyAlen+2)*200) 
                + wt[4]*(overlapOrtholog*10)
                + wt[5]*((log(exonsSpliced > 0 ? exonsSpliced : 1))/log(2))*200 
                - wt[6]*pow(pg->intronCount,0.5)*2000 
                - wt[7]*(maxOverlap*300)
                + wt[8]*((pg->coverage/100.0)*(1.0-((float)(psl->qSize-psl->qEnd)/(float)psl->qSize))*300.0)
                - wt[9]*(pg->tReps*10)
                - wt[10]*(pg->oldIntronCount)
                ) / ScoreNorm + bump;
verbose(1,"##score %d %s %s:%d-%d scr %d milbad %d %4.1f xon %d %4.1f retainS %d ax %4.1f pA %4.1f net + %4.1f max (%d, %d) exonSplice %d -%4.1f in.c %d -%4.1f ov - %4.1f  cov %d*(qe %d- qsz %d)/%d=%4.1f tRep - %4.1f oldintron %d %4.1f norm %d %s bump %d\n", 
                pg->label, psl->qName, psl->tName, psl->tStart, psl->tEnd, pseudoScore, 
                pg->milliBad, wt[0]*pg->milliBad,
                pg->exonCover,
                wt[1]*(log(pg->exonCover+1)/log(2))*200 , 
                pg->conservedSpliceSites,
                wt[2]*log(pg->axtScore>0?pg->axtScore:1)*70 , 
                wt[3]*(log(pg->polyAlen+2)*200) ,
                wt[4]*overlapOrtholog*10 , pg->overlapMouse, pg->overlapDog,
                exonsSpliced,
                wt[5]*((log(exonsSpliced > 0 ? exonsSpliced : 1))/log(2))*200 ,
                pg->intronCount, 
                wt[6]*pow(pg->intronCount,0.5)*200 ,
                wt[7]*(maxOverlap*300),
                pg->coverage, psl->qEnd, psl->qSize , psl->qSize,
                wt[8]*((pg->coverage/100.0)*(1.0-((float)(psl->qSize-psl->qEnd)/(float)psl->qSize))*300.0),
                wt[9]*(pg->tReps*10), 
                pg->oldIntronCount,
                wt[10]*pg->oldIntronCount,
                ScoreNorm, pg->type, bump
                ) ;
if (pseudoScore > 0)
    pg->score = pseudoScore;
else 
    pg->score = 0;

/* write pseudogene to gene alignment in psl and axt format */
if ((pg->label == PSEUDO || pg->label == EXPRESSED) ) 
    {
    pslTabOut(psl, pseudoFile);
    if (axt != NULL && pg->score > scoreThreshold)
        {
        axtWrite(axt, axtFile);
        axtFree(&axt);
        }
    else
        verbose(3, "no axt, type = %d score = %d\n",pg->label, pg->score);
    }

//wt[0] = 0.3; wt[1] = 0.85; wt[2] = 0.7; wt[3] = 0.4; wt[4] = 0.3; 
//wt[5] = 0;   wt[6] = 1  ;  wt[7] = 0.5; wt[8] = 1;   wt[9] = 1;
outputNoLinkScore(psl, pg);
}

int intronFactor(struct psl *psl, struct hash *rmskHash, struct hash *trfHash, int *baseCount)
/* Figure approx number of introns.  
 * An intron in this case is just a gap of 0 bases in query and
 * maxBlockGap or more in target iwth repeats masked. */
{
int i, blockCount = psl->blockCount  ;
int ts, qs, te, qe, sz, tsRegion, teRegion;
struct binKeeper *bk;
int intronCount = 0, tGap;

*baseCount = 0;
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
    qs = psl->qStarts[i];
    ts = psl->tStarts[i];
    teRegion = psl->tStarts[i] + psl->blockSizes[i];

    if (rmskHash != NULL)
        {
        if (trfHash != NULL)
            {
            bk = hashFindVal(trfHash, psl->tName);
            elist = binKeeperFindSorted(bk, psl->tStart , psl->tEnd ) ;
            trf = 0;
            for (el = elist; el != NULL ; el = el->next)
                {
                //bed = el->val;
                trf += positiveRangeIntersection(te, ts, el->start, el->end);
                }
            slFreeList(&elist);
            }
        bk = hashFindVal(rmskHash, psl->tName);
        elist = binKeeperFindSorted(bk, tsRegion , teRegion ) ;
        for (el = elist; el != NULL ; el = el->next)
            {
            /* count repeats in the gap */
            reps += positiveRangeIntersection(te, ts, el->start, el->end);
            /* count repeats in the gap  and surrounding exons  */
            regionReps += positiveRangeIntersection(tsRegion, teRegion, el->start, el->end);
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
        *baseCount += tGap - (2*abs(qs-qe));
        verbose(5, "YES ");
        }
    verbose(5,"%s:%d (%d < tGap %d) and > %d  qs %d qe %d   ts %d te %d reps %d trf %d \n",
            psl->tName, psl->tStart,2*abs(qs-qe),tGap, maxBlockGap,  qs,qe, ts, te, reps, trf);
    assert(psl != NULL);
    sz = psl->blockSizes[i];
    qe = qs + sz;
    te = ts + sz;
    tsRegion = psl->tStarts[i];
    }
verbose(4, "old intronCount %d %s %s bases %d\n",intronCount, psl->qName, psl->tName, *baseCount);
return intronCount;
}

int sizeFactor(struct psl *psl, struct hash *rmskHash, struct hash *trfHash)
/* Return a factor that will favor longer alignments. An intron is worth 3 bases...  */
{
int score;
int bases;
if (ignoreSize) return 0;
assert(psl != NULL);
score = 4*round(sqrt(psl->match + psl->repMatch/4));
if (!noIntrons)
    {
    int bonus = intronFactor(psl, rmskHash, trfHash, &bases) * 3;
    if (bonus > 10) bonus = 10;
    score += bonus;
    }
return score;
}

int calcSizedScore(struct psl *psl, struct hash *rmskHash, struct hash *trfHash)
/* Return score that includes base matches and size. */
{
int score = calcMilliScore(psl) + sizeFactor(psl, rmskHash, trfHash);
return score;
}

boolean closeToTop(struct psl *psl, int *scoreTrack , int milliScore) /*struct hash *rmskHash, struct hash *trfHash)*/
/* Returns TRUE if psl is near the top scorer for at least minNearTopSize bases. */
{
int threshold = round(milliScore * (1.0+nearTop));
int i, blockIx;
int start, size, end;
int topCount = 0;
char strand = psl->strand[0];

verbose(5,"%s:%d milliScore %d, threshold %d\n", psl->tName, psl->tStart, milliScore, threshold);
for (blockIx = 0; blockIx < psl->blockCount; ++blockIx)
    {
    start = psl->qStarts[blockIx];
    size = psl->blockSizes[blockIx];
    end = start+size;
    if (strand == '-')
	reverseIntRange(&start, &end, psl->qSize);
    verbose(5,"block %d threshold=%d s-e:%d-%d. ",blockIx, threshold, start, end);
    for (i=start; i<end; ++i)
	{
        verbose(6,"s=%d tc=%d ",scoreTrack[i],topCount);
	if (scoreTrack[i] <= threshold)
	    {
	    if (++topCount >= minNearTopSize)
                {
		return TRUE;
                }
	    }
	}
    }
return FALSE;
}

int scoreWindow(char c, char *s, int size, int *score, int *start, int *end, int match, int misMatch, float threshold)
/* calculate score array with score at each position in s, match to char c adds 1 to score, mismatch adds -1 */
/* index of max score is returned , size is size of s */
/* number char c within window must have density greater than threshold */
{
int i=0, j=0, max=2, count = 0; 

*end = 0;

assert(match >= 0);
assert(misMatch <= 0);
for (i=0 ; i<size ; i++)
    {
    int prevScore = (i > 0) ? score[i-1] : 0;

    if (toupper(s[i]) == toupper(c) )
        score[i] = prevScore+match;
    else
        score[i] = prevScore+misMatch;
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
        }
    verbose(6,"max is %d %d - %d score=%d i=%d\n",max, *start, *end, score[i],i);
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
if (*end != 0 )
    {
    if (count/(*end-*start) < threshold)
        verbose(4,"below threshold count %d %d - %d %5.2f < %5.2f \n",count, *start, *end, (float)count/(float)(*end-*start), threshold);
    }
else
    count = 0;
return count;
}
   

#ifdef NEVER
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
#endif

int polyACalc(int start, int end, char *strand, int tSize, char *chrom, int region, 
        int *polyAstart, int *polyAend, float divergence)
/* get size of polyA tail in genomic dna , count bases in a 
 * sliding window in region of the end of the sequence*/
/* use the divergence from the gene as the scoring threshold */
{
char nibFile[256];
int seqSize;
struct dnaSeq *seq = NULL;
int count = 0;
int length = 0;
int seqStart = strand[0] == '+' ? end : start - region;
int score[POLYAREGION+1], pStart = 0; 
struct seqFilePos *sfp = hashMustFindVal(tHash, chrom);
FILE *f = openFromCache(fileCache, sfp->file);
int match = 1;
int misMatch = -1;
float threshold = divergence/100;
seqSize = sfp->pos;

assert(region > 0);
assert(end != 0);
*polyAstart = 0 , *polyAend = 0;
safef(nibFile, sizeof(nibFile), "%s/%s.nib", nibDir, chrom);
verbose(2,"polyA file %s size %d start %d tSize %d match %d misMatch %d %f\n",
        sfp->file, seqSize, seqStart, tSize,match,misMatch,divergence);
assert (seqSize == tSize);
if (seqStart < 0) seqStart = 0;
if (seqStart + region > seqSize) region = seqSize - seqStart;
if (region == 0)
    return 0;
assert(region > 0);
seq = nibLdPartMasked(NIB_MASK_MIXED, nibFile, f, seqSize, seqStart, region);
if (strand[0] == '+')
    {
    assert (seq->size <= POLYAREGION);
verbose(4,"\n + range=%d %d %s \n",seqStart, seqStart+region, seq->dna );
    count = scoreWindow('A',seq->dna,seq->size, score, polyAstart, polyAend, match, misMatch, threshold);
    }
else
    {
    assert (seq->size <= POLYAREGION);
verbose(4,"\n - range=%d %d %s \n",seqStart, seqStart+region, seq->dna );
    count = scoreWindow('T',seq->dna,seq->size, score, polyAend, polyAstart, match, misMatch, threshold);
    }
pStart += seqStart;
*polyAstart += seqStart;
*polyAend += seqStart;
length = (strand[0]=='+'?*polyAend-*polyAstart:(*polyAstart)-(*polyAend))+1;
verbose(4,"\npolyA is %d from end. seqStart=%d %s polyS/E %d-%d exact matches %d length %d \n",
        strand[0]=='+'?*polyAstart-seqStart:*polyAend-seqStart+seq->size, 
	seqStart, seq->dna+(*polyAstart)-seqStart, 
	*polyAstart, *polyAend, count, length);
freeDnaSeq(&seq);
return count;
}

void pslMergeBlocks(struct psl *psl, struct psl *outPsl,
                       int insertMergeSize)
/* merge together blocks separated by small inserts. */
{
int iBlk, iExon = -1, blockIx;
int startIdx, stopIdx, idxIncr;

assert(outPsl!=NULL);
outPsl->qStarts = needMem(psl->blockCount*sizeof(unsigned));
outPsl->tStarts = needMem(psl->blockCount*sizeof(unsigned));
outPsl->blockSizes = needMem(psl->blockCount*sizeof(unsigned));

verbose(4,"MERGE %d blocks on %c match %d misMatch %d %s %s \n",
        psl->blockCount, psl->strand[0], psl->match, psl->misMatch, psl->qName, psl->tName);
verbose(4, "before merge %s %c %s %d blocks  ",
        psl->qName, psl->strand[0], psl->tName, psl->blockCount);
for (blockIx = 0; blockIx < psl->blockCount; ++blockIx)
    verbose(4, "%d-%d, ",psl->qStarts[blockIx], 
            psl->qStarts[blockIx]+psl->blockSizes[blockIx]);
verbose(4, "\n");

    startIdx = 0;
    stopIdx = psl->blockCount;
    idxIncr = 1;

for (iBlk = startIdx; iBlk != stopIdx; iBlk += idxIncr)
    {
    unsigned tStart = psl->tStarts[iBlk];
    unsigned qStart = psl->qStarts[iBlk];
    unsigned size = psl->blockSizes[iBlk];
    unsigned qEnd = psl->qStarts[iBlk]+size;
    unsigned outQStart = outPsl->qStarts[iExon];
    unsigned outQEnd = outPsl->qStarts[iExon]+outPsl->blockSizes[iExon];
    int tdiff = 0;
    int qdiff = 0;
    if (psl->strand[0] == '-')
        {
        reverseIntRange(&qStart, &qEnd, psl->qSize);
        reverseIntRange(&outQStart, &outQEnd, psl->qSize);
        }
    tdiff = abs(tStart - (outPsl->tStarts[iExon]+outPsl->blockSizes[iExon]));
    qdiff = abs(qStart - (outQEnd));
    if (iExon < 0 || (tdiff > insertMergeSize))
        {
        iExon++;
        verbose(4, " tdiff %d qdiff %d=%d-%d tStart %d out tStarts[%d] %d outPsl->size %d\n",
                tdiff, qdiff, qStart, outQEnd, tStart, iExon, outPsl->tStarts[iExon], outPsl->blockSizes[iExon]);
        verbose(4,"  init or Not merge %s[%d] new q %d t %s %d %d size %d to %d \n", 
            psl->qName, iExon, qStart,
            psl->tName, psl->tStarts[iBlk], tStart, outPsl->blockSizes, size);
        outPsl->tStarts[iExon] = tStart;
        if (psl->strand[0] == '-')
            reverseIntRange(&qStart, &qEnd, psl->qSize);
        outPsl->qStarts[iExon] = qStart;
        if (size > psl->qSize)
            assert(size <= psl->qSize);
        outPsl->blockSizes[iExon] = size; 
	}
    else
        {
        outPsl->blockSizes[iExon] += size + qdiff ;
        verbose(4, " tdiff %d tStart %d out.tStarts[%d] %d size %d outPsl->blkSmize %d tdiff %d qdiff %d tStart %d\n",
                tdiff, tStart, iExon, outPsl->tStarts[iExon], size, outPsl->blockSizes[iExon], tdiff, qdiff, psl->tStart);
        }
    }

verbose(4, "before last step merge %s %d ",psl->qName, outPsl->blockCount);
for (blockIx = 0; blockIx < outPsl->blockCount; ++blockIx)
    verbose(4, "%d-%d, ",outPsl->qStarts[blockIx], 
            outPsl->qStarts[blockIx]+outPsl->blockSizes[blockIx]);
verbose(4, "\n");

if (psl->strand[0] == '-')
    for (iBlk = 0; iBlk != outPsl->blockCount; iBlk += 1)
        {
        int qStart = outPsl->qStarts[iBlk];
        int qEnd = outPsl->qStarts[iBlk]+outPsl->blockSizes[iBlk];
        reverseIntRange(&qStart, &qEnd, psl->qSize);
        outPsl->qStarts[iBlk] = qStart;
        if (qStart < 0)
            assert(qStart >= 0);
        if (qStart+outPsl->blockSizes[iBlk] > outPsl->qSize)
            assert(qStart+outPsl->blockSizes[iBlk] <= outPsl->qSize);
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
for (iBlk = 0; iBlk != psl->blockCount; iBlk += 1)
    verbose(4, "%d,",psl->qStarts[iBlk]);
verbose(4,"  ");
for (iBlk = 0; iBlk != psl->blockCount; iBlk += 1)
    verbose(4, "%d,",psl->blockSizes[iBlk]);
verbose(4," to ");
for (iBlk = 0; iBlk != outPsl->blockCount; iBlk += 1)
    verbose(4, "%d,",outPsl->qStarts[iBlk]);
verbose(4,"  ");
for (iBlk = 0; iBlk != outPsl->blockCount; iBlk += 1)
    verbose(4, "%d,",outPsl->blockSizes[iBlk]);
verbose(4,"\n");

}

int interpolateStart(struct psl *gene, struct psl *pseudo)
/* estimate how much of the 5' end of the pseudogene was truncated */
    /* not used */
{
int qs = pseudo->qStarts[0];
int qe = pseudo->qStarts[0]+pseudo->blockSizes[0];
int gs, ge, qExonStart = 0;
int geneBegin = gene->tStart;
int j, index = -1;
int offset = 0, bestOverlap = 0;
int exonStart = 0;
assert(pseudo!= NULL);
assert(gene!= NULL);
if (gene->strand[0] == '-')
    geneBegin = gene->tEnd;
for (j = 0 ; j < gene->blockCount ; j++)
    {
    int i = j;
    if (gene->strand[0] == '-')
        i = gene->blockCount - j -1;
    gs = gene->qStarts[i];
    ge = gene->qStarts[i]+gene->blockSizes[i];
    if (gene->strand[0] == '-')
        reverseIntRange(&gs, &ge, gene->qSize);
    int overlap = positiveRangeIntersection(qs, qe, gs, ge);
    if (overlap > bestOverlap)
        {
        bestOverlap = overlap;
        index = i;
        exonStart = gene->tStarts[i];
        qExonStart = gs;
        if (gene->strand[0] == '-')
            exonStart = exonStart + gene->blockSizes[i];
        }
    }
if (index >= 0)
    {
    if (gene->strand[0] == '+')
        offset = exonStart - geneBegin + qs - qExonStart;
    else
        offset = geneBegin - exonStart + qs - qExonStart;
    }
return offset;
}
bool isRepeat(char *chrom, int is, int ie, struct hash *rmskHash)
/* check for repeats from intron*/
{
struct binKeeper *bk = NULL;
struct binElement *elist = NULL, *el = NULL;

if (rmskHash != NULL)
    {
    int reps = 0;
    bk = hashFindVal(rmskHash, chrom);
    elist = binKeeperFindSorted(bk, is, ie) ;
    for (el = elist; el != NULL ; el = el->next)
        {
        reps += positiveRangeIntersection(is, ie, el->start, el->end);
        verbose(6,"    isRep? chkRep %s:%d-%d reps %d ratio %f > 0.7\n",chrom,is, ie, reps,(float)reps/(float)(ie-is) );
        }
    slFreeList(&elist);
    if (reps > ie-is)
        {
        reps = ie-is;
        warn("Warning: too many reps %d in %s:%d-%d size %d\n",
                        reps, chrom, is, ie, ie-is);
        }
    if ((float)reps/(float)(ie-is) > repsPerIntron)
        {
        verbose(3,"     Intron is repeat: ratio = %f \n",(float)reps/(float)(ie-is));
        return TRUE;
        }
    }
return FALSE;
}

bool getNextIntron(struct psl *psl, int i, int *tStart, int *tEnd, int *qStart, int *qEnd, struct hash *rmskHash)
/* get next boundaries indexed intron */
/* t coords are virtual 0 = start of psl*/
/* return false of no next intron */
{
verbose(3,"    getNextIntron %s %s: %c total blocks %d\n",
        psl->qName, psl->tName,  psl->strand[0], psl->blockCount);
if (i+1 >= psl->blockCount)
    {
    verbose(3,"   no next exon, cannot check for introns i=%d > blockcount=%d\n",i,psl->blockCount);
    return FALSE;
    }
if (psl->strand[1] == '-')
    {
    i = psl->blockCount - i -1;
    *qStart = psl->qStarts[i-1] + psl->blockSizes[i-1];
    *tEnd = psl->tStarts[i]-2;
    *tStart = psl->tStarts[i-1] + psl->blockSizes[i-1];
    *qEnd = psl->qStarts[i];
//    *tStart = psl->tEnd - *tStart;
//    *tEnd = psl->tEnd - *tEnd;
    }
else
    {
    *qEnd = psl->qStarts[i+1];
    *tEnd = psl->tStarts[i+1];
    assert(i < psl->blockCount);
    *qStart = psl->qStarts[i] + psl->blockSizes[i];
    *tStart = psl->tStarts[i] + psl->blockSizes[i];
    if(*tStart < psl->tStart)
        verbose(4,"    i %d blk %d *tStart %d psl->tStart %d qName %s %s\n",
                i, psl->blockCount, *tStart, psl->tStart, psl->qName, 
                psl->tName);
    assert(*tStart >= psl->tStart);
//    *tStart = *tStart - psl->tStart;
//    *tEnd = *tEnd - psl->tStart;
    }
if (psl->strand[1] == '-')
    {
    reverseIntRange(tStart, tEnd, psl->tSize);
    }
if(*tStart >= *tEnd)
    verbose(1,"start > end %s:%d-%d strand %s %s\n",
            psl->tName, *tStart, *tEnd, psl->strand, psl->qName);
assert(*tStart < *tEnd);
if (isRepeat(psl->tName, *tStart,*tEnd,rmskHash))
    {
    verbose(3,"    next intron ret isRpt=TRUE  %s:%d-%d %s %d-%d\n",psl->tName,*tStart, *tEnd, psl->qName, *qStart, *qEnd, psl->blockCount);
    return FALSE;
    }
verbose(3,"    next intron ret isRpt=FALSE %s:%d-%d %s %d-%d\n",psl->tName,*tStart, *tEnd, psl->qName, *qStart, *qEnd, psl->blockCount);
return TRUE;
}

void exciseRepeats(struct psl *psl, struct hash *rmskHash)
/* remove repeats from intron, readjust t coordinates */
{
struct binKeeper *bk = NULL;
struct binElement *elist = NULL, *el = NULL;
int i;
unsigned *outReps = needMem(psl->blockCount*sizeof(unsigned));

for (i = 0 ; i < psl->blockCount-1 ; i++)
    {
    int is = psl->tStarts[i] + psl->blockSizes[i];
    int ie = psl->tStarts[i+1] ;
    outReps[i] = 0;
    /* skip exons that overlap repeats */
    if (rmskHash != NULL)
        {
        int reps = 0;
        bk = hashFindVal(rmskHash, psl->tName);
        elist = binKeeperFindSorted(bk, is, ie) ;
        for (el = elist; el != NULL ; el = el->next)
            {
            reps += positiveRangeIntersection(is, ie, el->start, el->end);
            }
        slFreeList(&elist);
        if (reps > ie-is)
            {
            reps = ie-is;
            warn("Warning: too many reps %d in st %d in end %d\n",reps, is, ie);
            }
        outReps[i] = reps;
        }
    }
/* shrink the repeats from the gene */
for (i = 0 ; i < psl->blockCount-1 ; i++)
    {
    int j;
    for (j = i ; j < psl->blockCount-1 ; j++)
        psl->tStarts[j+1] -= outReps[i];
    psl->tEnd -= outReps[i];
    assert(psl->tEnd > psl->tStarts[i+1]);
    }
freeMem(outReps);
/*
outPsl->blockCount = psl->blockCount;
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
*/
}
char *getSpliceSite(struct psl *psl, int start)
/* get sequence for splice site */
{
char *tName ;
struct dnaSeq *tSeq;
int tOffset;
char *site = NULL;
boolean isNib = TRUE;
tName = cloneString(psl->tName);
readCachedSeqPart(tName, start, 2, tHash, fileCache, &tSeq, &tOffset, &isNib);
assert (tOffset == start);
site = cloneStringZ(tSeq->dna, 2);
if (psl->strand[0] == '-')
    {
    reverseComplement(site, 2);
    }
freeDnaSeq(&tSeq);
freez(&tName);
return site;
}
int pslCountIntrons(struct psl *genePsl, struct psl *pseudoPsl, int maxBlockGap, 
        struct hash *rmskHash , int slop, struct dyString *iString , int *conservedIntron, int *conservedSplice)
/* return number of introns aligned between parent and retro have a size ratio greater than intronRatio (1.5)
 * also count of conserved introns in retro that match parent gene introns based on q position allowing some slop
 * also check bases and see if consensus splice sites are conserved */
{
struct psl *gene , *pseudo;
int count = 0;
int i,j;
int gts = 0, gte = 0, gqs = 0, gqe = 0; /* gene intron boundaries */
int pts = 0, pte = 0, pqs = 0, pqe = 0; /* pseudoegene intron boundaries */
//int offset = 0; /* 5' truncation of pseudogene */

if (genePsl == NULL )
    {
    verbose(3," pslCountIntrons failed no parent %s \n", pseudoPsl->qName);
    return 0;
    }
if (pseudoPsl == NULL)
    {
    verbose(3," pslCountIntrons failed no pseudo \n");
    return 0;
    }

if (genePsl->blockCount == 1)
    {
    verbose(3," pslCountIntrons failed single exon parent %s \n", pseudoPsl->qName);
    return 0;
    }
AllocVar(gene);
AllocVar(pseudo);
pslMergeBlocks(genePsl, gene, maxBlockGap);
pslMergeBlocks(pseudoPsl, pseudo, maxBlockGap);
if (gene->strand[0] == '-')
    {
    pslRc(gene);
    }
if (pseudo->strand[0] == '-')
    {
    pslRc(pseudo);
    }
//if (!skipExciseRepeats)
//    {
//    exciseRepeats(gene, rmskHash );
//    exciseRepeats(pseudo, rmskHash );
//    }
//offset = interpolateStart(gene, pseudo);
verbose(3,">>>pslCountIntrons %s gene cnt %d pseudocnt %d\n",pseudo->qName, gene->blockCount, pseudo->blockCount);
for (i = 0 ; i < gene->blockCount ; i++)
    {
    float intronG = 0.0;
    float intronP = 0.0;
    verbose (3, " gene block %d ",i);
    if (!getNextIntron(gene, i, &gts, &gte, &gqs, &gqe, rmskHash))
        if (i+1 >= gene->blockCount)
            break;
    for (j = 0 ; j < pseudo->blockCount ; j++)
        {
        verbose (3, " gene %d pseudogene blk %d ",i, j);
        if (!getNextIntron(pseudo, j, &pts, &pte, &pqs, &pqe, rmskHash))
            {
            verbose(3," NO gt %d-%d %d pt (%c) %d-%d %d  >> gq %d-%d pq %d-%d << \n",
                    gts,gte,gte-gts,pseudo->strand[0],pts,pte,pte-pts,gqs,gqe,pqs,pqe);
            if (i+1 >= gene->blockCount)
                break;     /* done, get next exon on gene */
            else
                continue;  /* repeat, keep looking */
            }
        intronG = gte-gts;
        intronP = pte-pts;
        verbose(4, " g %d-%d p %d-%d | stSlp %d + endSlp %d < slop %d inG %d inP %d",
                gqs,gqe,pqs,pqe, abs(gqs-pqs), abs(gqe-pqe), slop, intronG, intronP) ;

        if (abs(gqs-pqs) + abs(gqe-pqe) < slop) 
//                || ((abs(gqs-pqs) < slop && intronG/intronP) < intronRatio)
//                || ((abs(gqe-pqe) < slop && intronG/intronP) < intronRatio))
            {
            char *gd = NULL, *ga = NULL, *pd = NULL, *pa = NULL;
            if (genePsl->strand[0] == '+')
                {
                gd = getSpliceSite(genePsl, gts); /* donor */
                ga = getSpliceSite(genePsl, gte); /* acceptor */
                }
            else
                {
                gd = getSpliceSite(genePsl, gte); /* donor */
                ga = getSpliceSite(genePsl, gts); /* acceptor */
                }
            if (pseudoPsl->strand[0] == '+')
                {
                pd = getSpliceSite(pseudoPsl, pts); /* donor */
                pa = getSpliceSite(pseudoPsl, pte); /* acceptor */
                }
            else
                {
                pd = getSpliceSite(pseudoPsl, pte); /* donor */
                pa = getSpliceSite(pseudoPsl, pts); /* acceptor */
                }
            if (sameString(gd,pd) && sameString(gd,"GT") )
                {
                *conservedSplice = (*conservedSplice) + 1;
                verbose(3," \nconserved splice site %s acc %s %d\n",gd,pd, *conservedIntron);
                }
            if (sameString(ga,pa) && sameString(ga,"AG"))
                {
                *conservedSplice = (*conservedSplice) + 1;
                verbose(3," \nconserved splice site %s acc %s %d\n",ga,pa, *conservedIntron);
                }
            verbose(3," \nconserved intron part1 gt %d-%d %3.0f pt %d-%d %3.0f  gq %d-%d pq %d-%d ratio %f gd %s ga %s pd %s pa %s diffS %d diffE %d slop %d\n"
                    ,gts,gte,intronG,pts,pte,intronP,gqs,gqe,pqs,pqe,
                    intronG/intronP, gd, ga, pd, pa,  
                    abs(gqs-pqs) , abs(gqe-pqe) , slop); 
            if (intronP > 500 || (intronG/intronP) < intronRatio)
                {
                count++;
                verbose(2," \n** Yes ** count = %d gt %d-%d %d pt %d-%d %d  gq %d-%d pq %d-%d \n",
                        count,gts,gte,gte-gts,pts,pte,pte-pts,gqs,gqe,pqs,pqe);
                }
            freez(&gd); freez(&ga); freez(&pd); freez(&pa);
            *conservedIntron = (*conservedIntron) + 1;
            }
        else
            {
            verbose(6," gt %d-%d %d pt %d-%d %d  >> gq %d-%d pq %d-%d <<\n",
                    gts,gte,gte-gts,pts,pte,pte-pts,gqs,gqe,pqs,pqe);
            verbose(3," NO \n");
            }
        }
        dyStringPrintf(iString, "%4.1f,%4.1f,",intronG,intronP);
    }
verbose(2,">>>pslIntronCount %s %s:%d returns %d >1.5 intron size change,   cons intron=%d cons splice=%d\n\n",
    pseudo->qName, pseudo->tName, pseudo->tStart, count, *conservedIntron, *conservedSplice);
pslFree(&pseudo);
pslFree(&gene);
return count;
}
bool pslInRange(struct psl *psl, int i)
/* check if block index is out of range of psl */
{
if (i < 0) 
    return FALSE;
if (i >= psl->blockCount)
    return FALSE;
return TRUE;
}

int countRetainedSpliceSites(struct psl *target, struct psl *query, int spliceDrift)
/* count number of splice sites from parent gene that are within spliceDrift bases of splice site in retro */
/* splice site is measure relative to q coordinates */
{
int count = 0, i;
bool swapT = FALSE, swapQ = FALSE;
//struct psl *targetM = NULL, *queryM = NULL;

if (target == NULL || query == NULL)
    return 0;

//AllocVar(targetM);
//AllocVar(queryM);
//pslMergeBlocks(target, targetM, maxBlockGap);
//pslMergeBlocks(query, queryM, maxBlockGap);
verbose(4, "q strand %s t strand %s ",query->strand, target->strand);
if (target->strand[0] == '-')
    {
    pslRc(target);
    swapT = TRUE;
    }
if (query->strand[0] == '-')
    {
    pslRc(query);
    swapQ = TRUE;
    }
verbose(4, "countRetainSS target blocks %d query blocks %d %s tstr %s\n",target->blockCount, query->blockCount,query->strand, target->strand);
for (i = 0 ; i < target->blockCount ; i++)
    {
    int qe = target->qStarts[i] + target->blockSizes[i];
    int qs = target->qStarts[i];
    int te = target->tStarts[i] + target->blockSizes[i];
    int ts = target->tStarts[i];
    int j;
    bool negStrand = query->strand[1] == '-';
    if (target->strand[1] == '-')
        reverseIntRange(&ts, &te, target->tSize);
    for (j = 0 ; j < query->blockCount ; j++)
        {
        int offset = (negStrand) ? -1 : 1;
        int qqe = query->qStarts[j] + query->blockSizes[j];
        int qqs = query->qStarts[j] ;
        int qte = query->tStarts[j] + query->blockSizes[j];
        int qts = query->tStarts[j] ;
        int qpts = pslInRange(query, j-offset) ? query->tStarts[j-offset] : 0;
        int qpte = pslInRange(query, j-offset) ? query->tStarts[j-offset]+query->blockSizes[j-offset] : 0;
        int qsts = pslInRange(query, j+offset) ? query->tStarts[j+offset] : query->tStarts[0];
        int qste = pslInRange(query, j+offset) ? query->tStarts[j+offset]+query->blockSizes[j+offset] : query->tStarts[0]+query->blockSizes[0];
        verbose(5, "ps j-offset %d before %d %d prev %d-%d succ %d-%d ",
                        j-offset, qts, qte, qpts, qpte, qsts, qste);
        if (negStrand)
            {
            reverseIntRange(&qts, &qte, query->tSize);
            reverseIntRange(&qpts, &qpte, query->tSize);
            reverseIntRange(&qsts, &qste, query->tSize);
            }
        verbose(5, "t after %d %d prev %d-%d succ %d-%d diff %d %d\n",qts, qte, qpts, qpte, qsts, qste,
                        abs(qpte-qts), abs(qte-qsts));
        if ((abs(qqs - qs) <= spliceDrift) && qqs != target->qStart && 
                ((negStrand) ? abs(qsts-qte) : abs(qpte-qts)) > spliceDrift &&
                i > 0 /* skip beginning of gene*/) 
            {
            count++;
            verbose(5, "   COUNT LEFT i=%d j=%d %c%c q %d-%d parent q %d-%d \
t %c %s:%d-%d q %s %s:%d-%d target start %d End %d left %d %d-%d right %d %d-%d\n",
                 i,j,query->strand[0],query->strand[1],qqs,qqe, qs,qe, 
                 target->strand[0],target->tName, ts, te, 
                 query->qName, query->tName, qts,qte, target->qStart, target->qEnd,
                 qpte-qts, qpte, qts, qte-qsts, qte, qsts
                 );
            }
        else
            verbose(4, "   NO i=%d j=%d %c qqs/e %d-%d parent qs/e %d-%d \
t %c %s:%d-%d q %s:%d-%d \
qqs-qe %d qqs %d qStart %d qpte-qts %d qpte %d qts %d \n" ,
                 i,j,query->strand[0], qqs, qqe, qs, qe,
                 target->strand[0], target->tName, ts, te, 
                 query->tName, qts,qte , 
                 abs(qqs - qe), qqs, target->qStart, abs(qpte-qts) , qpte, qts);
        if ((abs(qqe - qe) <= spliceDrift) && qqe != target->qEnd   && 
                ((negStrand) ? abs(qts-qpte) : abs(qte-qsts)) > spliceDrift  &&
                i < target->blockCount -1 /* skip end of gene */)
            {
            count++;
            verbose(4, "   COUNT RIGHT i=%d j=%d %c%c q %d-%d parent q %d-%d \
t %c %s:%d-%d q %s %s:%d-%d target start %d End %d left %d %d-%d right %d %d-%d\n",
                 i,j,query->strand[0],query->strand[1],qqs,qqe, qs,qe, 
                 target->strand[0],target->tName, ts, te, 
                 query->qName, query->tName, qts,qte, target->qStart, target->qEnd,
                 qpte-qts, qpte, qts, qte-qsts, qte, qsts
                 );
            }
        else
            verbose(4, "   NO i=%d j=%d %c qqs/e %d-%d parent qs/e %d-%d \
t %c %s:%d-%d q %s:%d-%d \
qqe-qe %d qqe %d qEnd %d qte-qsts %d qte %d qsts %d\n",
                 i,j,query->strand[0], qqs, qqe, qs, qe,
                 target->strand[0], target->tName, ts, te, 
                 query->tName, qts,qte , 
                 abs(qqe - qe), qqe, target->qEnd  , abs(qte-qsts), qte, qsts);
        }
    }
verbose(2, "FINAL COUNT %d %s:%d-%d q %s:%d-%d\n",
     count,
     target->tName,  target->tStart, target->tEnd,
     query->tName, query->tStart, query->tEnd);
//pslFree(&targetM);
//pslFree(&queryM);
if (swapT)
    pslRc(target);
if (swapQ)
    pslRc(query);
if (query->strand[1] == '+')
    query->strand[1] = '\0';
if (target->strand[1] == '+')
    target->strand[1] = '\0';
return count;
}
int pslCountExonSpan(struct psl *target, struct psl *query, int maxBlockGap, struct hash *rmskHash , int *tReps, int *qReps)
/* count the number of blocks in the query that overlap the target */
/* merge blocks that are closer than maxBlockGap */
{
int i, j, start = 0, qs, qqs;
int count = 0;
struct binKeeper *bk = NULL;
struct binElement *elist = NULL, *el = NULL;
struct psl *targetM ;

*tReps = 0; *qReps = 0;
if (target == NULL || query == NULL)
    return 0;

AllocVar(targetM);
//AllocVar(queryM);
assert(target!=NULL);
pslMergeBlocks(target, targetM, maxBlockGap);
//pslMergeBlocks(query, queryM, maxBlockGap);
verbose(4, "target blocks %d  merged %d, query blocks %d\n",
                target->blockCount,targetM->blockCount, query->blockCount);
assert(target!=NULL);
if (targetM->blockCount == 1)
    {
    for (j = 0 ; j < query->blockCount ; j++)
        {
        int qqs = query->qStarts[j];
        int qqe = query->qStarts[j] + query->blockSizes[j];
        if (query->strand[0] == '-')
            reverseIntRange(&qqs, &qqe, query->qSize);
        if (positiveRangeIntersection(qqs, qqe, target->qStart, target->qEnd) > 10)
            {
            count++;
            break;
            }
        }
    }
else
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
            if (rmskHash != NULL)
                {
                bk = hashFindVal(rmskHash, targetM->tName);
                elist = binKeeperFindSorted(bk, ts, te) ;
                for (el = elist; el != NULL ; el = el->next)
                    {
                    teReps += positiveRangeIntersection(ts, te, el->start, el->end);
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
        if (positiveRangeIntersection(query->qStart, query->qEnd, qs, qe) > min(20,qe-qs))
            {
            int qqe = 0;
            for (j = 0 ; j < query->blockCount ; j++)
                {
                int localReps = 0;
                qqs = query->qStarts[j];
                qqe = query->qStarts[j] + query->blockSizes[j];
                if (query->strand[0] == '-')
                    reverseIntRange(&qqs, &qqe, query->qSize);
                assert(j < query->blockCount);
                /* mask repeats */
                if (rmskHash != NULL)
                    {
                    bk = hashFindVal(rmskHash, query->tName);
                    if (j < (query->blockCount) -1)
                        {
                        elist = binKeeperFindSorted(bk, query->tStarts[j], query->tStarts[j+1]) ;
                        for (el = elist; el != NULL ; el = el->next)
                            {
                            localReps += positiveRangeIntersection(query->tStarts[j], query->tStarts[j]+query->blockSizes[j], el->start, el->end);
                            }
                        *qReps += localReps;
                        slFreeList(&elist);
                        }
                    }
                if (positiveRangeIntersection(qqs, qqe, qs, qe) > 10)
                    {
                    count++;
                    verbose(4, "count exon intersect %d >10  qqs %d qqe %d target: qs %d qe %d\n",positiveRangeIntersection(qqs, qqe, qs, qe), qqs,qqe,qs,qe);
                    break;
                    }
                else
                    verbose(4, "count exon not intersect %d <10  qqs %d qqe %d target: qs %d qe %d\n",positiveRangeIntersection(qqs, qqe, qs, qe), qqs,qqe,qs,qe);
                }
            }
        else
            {
            verbose(4, "count exon not intersect %d < min(20,qe-qs %d) query %d-%d target %d %d \n",
                  positiveRangeIntersection(query->qStart, query->qEnd, qs, qe) , qe-qs,query->qStart, query->qEnd, qs, qe);
            }
        start = i+1;
        }

if(count > targetM->blockCount )
    {
    verbose(1,"error in pslCountExonSpan: %s %s %s:%d-%d %d > targetBlk %d or query Blk %d \n",
            targetM->qName, query->qName, query->tName,query->tStart, query->tEnd, 
            count, targetM->blockCount, query->blockCount);
    assert(count > targetM->blockCount);
    }
pslFree(&targetM);
//pslFree(&queryM);
verbose(4, "exon cover = %d\n",count);
return count;
}

float calcTrf(struct psl *psl, struct hash *trfHash)
/* calc ratio of simple repeats overlap to aligning bases */
{
int trf = 0;
if (trfHash != NULL)
    {
    int i, trf = 0;
    for (i = 0 ; i < psl->blockCount ; i++)
        {
        int ts = psl->tStarts[i] ;
        int te = psl->tStarts[i] + psl->blockSizes[i];
        struct binKeeper *bk = hashFindVal(trfHash, psl->tName);
        struct binElement *el, *elist = binKeeperFindSorted(bk, ts, te ) ;
        for (el = elist; el != NULL ; el = el->next)
            {
            trf += positiveRangeIntersection(ts, te, el->start, el->end);
            }
        slFreeList(&elist);
        }
    }
return (float)trf/(float)(psl->match+psl->misMatch);
}

int overlapMrna(struct psl *psl, int *exonOverlapCount, struct psl **overlapPsl)
/* count bases that mrna overlaps with pseudogenes. If self match then don't filter it.*/
/* exonOverlapCount has number of exons in matched mRna */
{
int maxOverlap = 0;
safef(mrnaOverlap,255,"NONE");
if (mrnaHash != NULL)
    {
    int mrnaBases = 0;
    struct psl *mPsl = NULL , *mPslMerge = NULL;
    struct binKeeper *bk = hashFindVal(mrnaHash, psl->tName);
    struct binElement *el, *elist = binKeeperFindSorted(bk, psl->tStart , psl->tEnd ) ;
    int blockIx;
    for (el = elist; el != NULL ; el = el->next)
        {
        mrnaBases = 0;
        mPsl = el->val;
        if (mPsl != NULL)
            {
            assert (psl != NULL);
            assert (mPsl != NULL);
            assert (psl->tName != NULL);
            if (differentString(psl->qName, mPsl->qName))
                {
                AllocVar(mPslMerge);
                pslMergeBlocks(mPsl, mPslMerge, 30);
                assert(mPslMerge != NULL);
                verbose(4,"blk %d %s %d ec %d\n",mPslMerge->blockCount, mPsl->qName, mrnaBases, *exonOverlapCount);

                if (mPslMerge->blockCount > 0)
                    {
                    for (blockIx = 0; blockIx < mPslMerge->blockCount; ++blockIx)
                        {
                        mrnaBases += positiveRangeIntersection(psl->tStart, psl->tEnd, 
                            mPslMerge->tStarts[blockIx], mPslMerge->tStarts[blockIx]+mPslMerge->blockSizes[blockIx]);
                        }
                    }
                else
                    {
                    mrnaBases += positiveRangeIntersection(psl->tStart, psl->tEnd, mPsl->tStart, mPsl->tEnd);
//                    verbose(6,"blk merge %d %s %d ec %d\n",mPslMerge->blockCount, mPsl->qName, mrnaBases, *exonOverlapCount);
                    }
                verbose(4,"MRNABASES %d block cnt %d maxOverlap %d exonOverlapCount %d \
                        %s %s %d-%d best so far %s\n",
                        mrnaBases, mPslMerge->blockCount, maxOverlap, *exonOverlapCount, 
                        mPslMerge->qName, mPslMerge->qName, mPslMerge->tStart, mPslMerge->tEnd, mrnaOverlap);
                if (mrnaBases > 50 && (mPslMerge->blockCount > 0) && 
                        (((int)mPslMerge->blockCount > *exonOverlapCount) || 
                         (((int)mPslMerge->blockCount == *exonOverlapCount) && (mrnaBases > maxOverlap)))
                   )
                    {
                        *exonOverlapCount = (int)mPslMerge->blockCount;
                        safef(mrnaOverlap,255,"%s",mPslMerge->qName);
                        maxOverlap = mrnaBases;
                        *overlapPsl = mPslMerge;
                    }
                //pslFree(&mPslMerge);
                }
            }
        }
    slFreeList(&elist);
    }
return maxOverlap ;
}

int netOverlap(struct psl *psl, struct hash *nHash)
/* calculate if pseudogene overlaps the syntenic diagonal with another species */
{
int overlap = 0;
if (nHash != NULL)
    {
    int maxlevel = 0;
    int netSize = 0;
    float coverage = 0.0;
    struct binKeeper *bk = hashFindVal(nHash, psl->tName);
    struct binElement *el, *elist;
    elist = binKeeperFindSorted(bk, psl->tStart , psl->tEnd ) ;
    for (el = elist; el != NULL ; el = el->next)
        {
        float overlapThreshold = 0.01;
        int level = ptToInt(el->val);
        int size = psl->tEnd - psl->tStart;
        int overlap = positiveRangeIntersection(psl->tStart, psl->tEnd, 
                el->start, el->end);
        if (level > maxlevel && (float)overlap/(float)size > overlapThreshold && ((el->end)-(el->start) > (size * 0.40)))
            maxlevel = level;
        else
            verbose(4, "failed threshold %f <= %f or net %d < retro %d \n", (float)overlap/(float)size , overlapThreshold, (el->end)-(el->start) , size);
        verbose(4,"%s %s:%d level %d size %d net %d \n",psl->qName, psl->tName, psl->tStart, level, size, el->end-el->start);
        }
    verbose(3,"%s %s:%d max level %d \n",psl->qName, psl->tName, psl->tStart, maxlevel );
    for (el = elist; el != NULL ; el = el->next)
        {
        //bed = el->val;
        int level = ptToInt(el->val);
        if (maxlevel == level)
            {
            overlap += positiveRangeIntersection(psl->tStart, psl->tEnd, 
                el->start, el->end);
            netSize += (el->end)-(el->start);
            verbose(4,"net size %d after %d-%d overlap %d tSize %d\n",netSize, el->start, el->end, overlap, (psl->tEnd-psl->tStart));
            }
        }
    if (overlap > 0)
        coverage = overlap/(psl->tEnd-psl->tStart);
    if (netSize > 0)
        {
        verbose(2,"#score %s net %d/%d=%d level %d cover %4.1f\n",
                psl->qName, overlap, netSize,(overlap*100)/(netSize), maxlevel, coverage);
        overlap = (overlap*100)/(netSize);
        }
    else
        {
        verbose(2,"#score %s net %d/%d=%d level %d cover %4.1f\n",
                psl->qName, overlap, netSize,overlap, maxlevel, coverage);
        overlap = -10;
        }
    //overlap = (overlap*100)/(psl->tEnd-psl->tStart);
    slFreeList(&elist);
    }
return overlap;
}

void pseudoFeaturesCalc(struct psl *psl, struct psl *bestPsl, int maxExons, int bestAliCount, 
        char *bestChrom, int bestStart, int bestEnd, char *bestStrand) 
/* calculate features of retroGene */
{
struct pseudoGeneLink *pg = NULL;
struct dyString *iString = newDyString(16*1024);
struct dyString *reason = newDyString(255);
struct genePred *gp = NULL, *kg = NULL, *mgc = NULL;
int milliMinPseudo = 1000*minAliPseudo;
int conservedIntrons = 0;    
//int conservedSpliceSites = 0;    
int geneOverlap = -1;
int polyAstart = 0;
int polyAend = 0;
int tReps , qReps;
int trf = 0, rep = 0;
bool keepChecking = TRUE;
int intronBases;

AllocVar(pg);
pg->exonCount = (maxExons*2)-2; /* really splice sites */
pg->bestAliCount = bestAliCount;
pg->oldIntronCount = intronFactor(psl, rmskHash, trfHash, &intronBases);
pg->oldIntronCount *= (intronBases/10);
pg->gChrom = cloneString(bestChrom);
pg->gStart = bestStart;
pg->gEnd = bestEnd;
safef(pg->gStrand, sizeof(pg->gStrand), bestStrand);
pg->overlapMouse= -1;
pg->milliBad = calcMilliScore(psl);
pg->coverage = ((psl->match+psl->misMatch+psl->repMatch)*100)/psl->qSize;
verbose(1,"\nchecking new %s:%d-%d %s best %s:%d-%d milli %d cover %d parent exons %d\n", 
         psl->tName, psl->tStart, psl->tEnd, psl->qName,  bestChrom, bestStart, bestEnd, 
         pg->milliBad, pg->coverage, pg->exonCount);
pg->overStart = pg->overEnd = pg->kStart = pg->kEnd = pg->rStart = pg->rEnd = pg->mStart = pg->mEnd = -1;
pg->polyA = polyACalc(psl->tStart, psl->tEnd, psl->strand, psl->tSize, psl->tName, 
                POLYAREGION, &polyAstart, &polyAend, pg->milliBad/10);
pg->polyAlen = abs(polyAend-polyAstart)+1;
pg->polyAstart = polyAstart;
/* count # of alignments that span introns */
pg->exonCover = pslCountExonSpan(bestPsl, psl, maxBlockGap, rmskHash, &tReps, &qReps) ;
pg->intronCount = 0;//pslCountIntrons(bestPsl, psl, maxBlockGap, rmskHash, intronSlop, iString, &conservedIntrons, &conservedSpliceSites) ;
pg->conservedIntrons = conservedIntrons;
pg->conservedSpliceSites = countRetainedSpliceSites(bestPsl, psl , spliceDrift);
pg->trfRatio = calcTrf(psl, trfHash);
if (bestPsl == NULL)
    pg->intronCount = pg->oldIntronCount;

geneOverlap = 0;
genePredFree(&kg); 
genePredFree(&gp); 
genePredFree(&mgc); 
if (bestPsl != NULL)
    {
    kg = getOverlappingGene(&kgList, "knownGene", bestPsl->tName, bestPsl->tStart, 
                        bestPsl->tEnd , bestPsl->qName, &geneOverlap);
    if (kg != NULL)
        {
        pg->kStart = kg->txStart;
        pg->kEnd = kg->txEnd;
        if (kg->name2 != NULL)
            pg->kgName = cloneString(kg->name2);
        else
            pg->kgName = cloneString(kg->name);
        }
    else
        {
        pg->kgName = cloneString("noKg");
        }
    gp = getOverlappingGene(&gpList1, "refGene", bestPsl->tName, bestPsl->tStart, 
                        bestPsl->tEnd , bestPsl->qName, &geneOverlap);
    if (gp != NULL)
        {
        pg->refSeq = cloneString(gp->name);
        pg->rStart = gp->txStart;
        pg->rEnd = gp->txEnd;
        }
    else
        {
        pg->refSeq = cloneString("noRefSeq");
        }
    mgc = getOverlappingGene(&gpList2, "mgcGenes", bestPsl->tName, bestPsl->tStart, 
                        bestPsl->tEnd , bestPsl->qName, &geneOverlap);
    if (mgc != NULL)
        {
        pg->mgc = cloneString(mgc->name);
        pg->mStart = mgc->txStart;
        pg->mEnd = mgc->txEnd;
        }
    else
        {
        pg->mgc = cloneString("noMgc");
        }
    }
else
    {
    pg->refSeq = cloneString("noRefSeq");
    pg->kgName = cloneString("noKg");
    pg->mgc = cloneString("noMgc");
    }
/* calculate if pseudogene overlaps the syntenic diagonal with another species */
pg->overlapMouse = netOverlap(psl, synHash);
pg->overlapDog = netOverlap(psl, syn2Hash);
pg->overlapRhesus = netOverlap(psl, syn3Hash);

if (pg->trfRatio > .5)
    {
    verbose(1,"NO. %s trf overlap %f > .5 %s %d \n",
            psl->qName, (float)trf/(float)(psl->match+psl->misMatch) ,
            psl->tName, psl->tStart);
    keepChecking = FALSE;
    }
/* blat sometimes overlaps parts of the same mrna , filter these */

if ( keepChecking && positiveRangeIntersection(bestStart, bestEnd, psl->tStart, psl->tEnd) && 
            sameString(psl->tName, bestChrom))
   {
   dyStringAppend(reason,"self;");
   keepChecking = FALSE;
   }

/* count repeat overlap with pseudogenes and skip ones with more than maxRep% overlap*/
if (keepChecking && rmskHash != NULL)
    {
    int i;
    struct binElement *el, *elist;
    rep = 0;
    assert (psl != NULL);
    for (i = 0 ; i < psl->blockCount ; i++)
        {
        int ts = psl->tStarts[i] ;
        int te = psl->tStarts[i] + psl->blockSizes[i];
        struct binKeeper *bk;
        bk = hashFindVal(rmskHash, psl->tName);
        elist = binKeeperFindSorted(bk, ts, te) ;
        for (el = elist; el != NULL ; el = el->next)
            {
            rep += positiveRangeIntersection(ts, te, el->start, el->end);
            }
        slFreeList(&elist);
        }
    }
pg->tReps = round((float)(rep*100)/(float)(psl->match+(psl->misMatch)));
if ((float)rep/(float)(psl->match+(psl->misMatch)) > maxRep )
    {
    verbose(1,"NO %s reps %.3f %.3f\n",psl->tName,(float)rep/(float)(psl->match+(psl->misMatch)) , maxRep);
    dyStringAppend(reason,"maxRep;");
    pg->label = NOTPSEUDO;
    keepChecking = FALSE;
    }

if (keepChecking && (pg->intronCount == 0 /*|| (pg->exonCover - pg->intronCount > INTRONMAGIC)*/) && 
    /*maxExons > 1 && */ pg->bestAliCount > 0 && bestChrom != NULL &&
    (calcMilliScore(psl) >= milliMinPseudo && pg->trfRatio < .5 && (pg->exonCover-pg->conservedSpliceSites) > 0 &&
    psl->match + psl->misMatch + psl->repMatch >= minCoverPseudo * (float)psl->qSize))
    {
        struct psl *mPsl;
        int exonOverlapCount = -1;
        //struct genePred *gene = getOverlappingGene(&gpList1, "refGene", psl->tName, psl->tStart, 
        //                    psl->tEnd , psl->qName, &geneOverlap);
        int maxOverlap = overlapMrna(psl, &exonOverlapCount, &mPsl);
        pg->maxOverlap = maxOverlap;
        if ((float)maxOverlap/(float)(psl->match+psl->misMatch+psl->repMatch) > splicedOverlapRatio 
                && maxOverlap > 10 ) 
            /* if overlap > 50 bases  and 10% overlap with pseudogene, then skip */
            {
            verbose(1,"NO %s:%d-%d %s expressed blat mrna %s %d bases overlap %f %%\n",
                    psl->tName, psl->tStart, psl->tEnd, psl->qName,mrnaOverlap, 
                    maxOverlap, (float)maxOverlap/(float)psl->qSize);
            dyStringAppend(reason,"expressed");
            pg->overName = cloneString(mPsl->qName); 
            pg->overStart = mPsl->tStart;
            pg->overEnd = exonOverlapCount;
            strncpy(pg->overStrand, mPsl->strand , sizeof(pg->overStrand));
            pslFree(&mPsl);
            pg->label = EXPRESSED;
            outputLink(psl, pg, reason, bestPsl);
            keepChecking = FALSE;
            }


/*        if (pg->overlapMouse>= 40 && bestPsl == NULL)
           {
            verbose(1,"NO. %s %d diag %s %d  bestChrom %s\n",psl->qName, 
                    pg->overlapMouse, psl->tName, psl->tStart, bestChrom);
           dyStringAppend(reason,"diagonal;");
           keepChecking = FALSE;
           }*/
        if (keepChecking)
           {
            verbose(2,"YES %s %d rr %3.1f rl %d ln %d %s iF %d maxE %d bestAli %d isp %d millibad %d match %d cover %3.1f rp %d polyA %d syn %d polyA %d start %d overlap ratio %d/%d %s\n",
                psl->qName,psl->tStart,((float)rep/(float)(psl->tEnd-psl->tStart) ),rep, 
                psl->tEnd-psl->tStart,psl->tName, pg->intronCount, 
                maxExons , pg->bestAliCount, pg->exonCover,
                calcMilliScore(psl),  psl->match + psl->misMatch + psl->repMatch , 
                minCoverPseudo * (float)psl->qSize, pg->tReps , pg->polyA, pg->overlapMouse,
                pg->polyAlen, pg->polyAstart ,
                maxOverlap,psl->match+psl->misMatch+psl->repMatch , pg->overName
                );
           if (pg->exonCover - pg->conservedSpliceSites < 2)
               {
               dyStringAppend(reason,"singleExon;");
               pg->label = PSEUDO;
               outputLink(psl, pg, reason, bestPsl);
               }
           else if (bestPsl == NULL)
               {
               dyStringAppend(reason,"noBest;");
               pg->label = NOTPSEUDO;
               outputLink(psl, pg, reason, bestPsl);
               }
           else if (kg == NULL && mgc == NULL && gp == NULL)
               {
               dyStringAppend(reason,"mrna");
               pg->label = PSEUDO;
               outputLink(psl, pg, reason, bestPsl);
               }
           else
               {
               dyStringAppend(reason,"good");
               pg->label = PSEUDO;
               outputLink(psl, pg, reason, bestPsl);
               }
           keepChecking = FALSE;
           }
        }
    else
        {
        if (bestPsl == NULL)
            dyStringAppend(reason,"noBest;");
        if (pg->exonCover < 1)
            dyStringAppend(reason,"exonCover;");
        if (pg->bestAliCount < 1)
            dyStringAppend(reason,"noAli;");
        if (pg->trfRatio > .5)
            dyStringAppend(reason,"trf;");
        if (pg->intronCount > 0)
            dyStringAppend(reason,"introns;");
        if (pg->exonCover == 1)
            dyStringAppend(reason,"singleExon;");
        if (maxExons <= 1)
            dyStringAppend(reason,"singleExonParent;");
        if (calcMilliScore(psl) < milliMinPseudo)
            dyStringAppend(reason,"milliBad;");
        if (psl->match + psl->misMatch + psl->repMatch < minCoverPseudo * (float)psl->qSize)
            dyStringAppend(reason,"coverage;");

       if (bestPsl == NULL)
           {
           pg->label = NOTPSEUDO;
           outputLink(psl, pg, reason, bestPsl);
           }
        else
           {
           pg->label = NOTPSEUDO;
           outputLink(psl, pg, reason, bestPsl);
           }
        verbose(2,"NO. %s %s %d rr %3.1f rl %d ln %d %s iF %d maxE %d bestAli %d isp %d score %d match %d cover %3.1f rp %d\n",
            reason->string, psl->qName,psl->tStart,((float)rep/(float)(psl->tEnd-psl->tStart) ),rep, 
            psl->tEnd-psl->tStart,psl->tName, pg->intronCount, maxExons , pg->bestAliCount, pg->exonCover,
            calcMilliScore(psl),  psl->match + psl->misMatch + psl->repMatch , 
            minCoverPseudo * (float)psl->qSize, pg->tReps );
        }
dyStringFree(&iString);
dyStringFree(&reason);
//pseudoGeneLinkFree(&pg);
}

void processBestMulti(char *acc, struct psl *pslList)
/* This function is passed a list of all hits of a single mrna */
/* Find psl's that are align best anywhere along their length. */

{
struct psl *bestPsl = NULL, *psl, *bestSEPsl = NULL;
int qSize = 0;
int *scoreTrack = NULL;
int maxExons = 0;
int milliScore;
int goodAliCount = 0;
int bestAliCount = 0;
int milliMin = 1000*minAli;
int bestStart = -1, bestEnd = -1;
static char bestStrand[3];
static char bestSEStrand[3];
int bestSEStart = 0, bestSEEnd = 0;
char *bestChrom = NULL, *bestSEChrom = NULL;
int bestScore = 0, bestSEScore = 0;

if (pslList == NULL)
    return;

/* calculate size of scoreArray by finding the longest aligment - some have polyA tail stripped off */
for (psl = pslList; psl != NULL; psl = psl->next)
    if (psl->qSize > qSize)
        qSize = psl->qSize;

AllocArray(scoreTrack, qSize+1);

/* search all alignments and store the best score for each base in scoreArray*/
for (psl = pslList; psl != NULL; psl = psl->next)
    {
    int blockIx;
    char strand = psl->strand[0];

    verbose(2,"checking %s %s:%d-%d\n",psl->qName, psl->tName, psl->tStart, psl->tEnd);
    assert (psl!= NULL);
    milliScore = calcMilliScore(psl);
    if (milliScore >= milliMin)
	{
	++goodAliCount;
	milliScore += sizeFactor(psl, rmskHash, trfHash);
        verbose(5,"@ %s %s:%d milliScore %d\n", psl->qName, psl->tName, psl->tStart, milliScore);
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
            verbose(5,"milliScore: %d qName %s tName %s:%d-%d qSize %d psl->qSize %d start %d end %d \n",
                    milliScore, psl->qName, psl->tName, psl->tStart, psl->tEnd, qSize, psl->qSize, start, end);
	    for (i=start; i<end; ++i)
		{
                assert(i<=qSize);
		if (milliScore > scoreTrack[i])
                    {
                    verbose(6," %d base %d",milliScore, i);
                    scoreTrack[i] = milliScore;
                    }
		}
	    }
	}
    }
verbose(2,"---finding best---\n");
/* Print out any alignments that are within minTop% of top score for at least . */
bestScore = 0;
bestSEScore = 0;
for (psl = pslList; psl != NULL; psl = psl->next)
    {
    struct psl *pslMerge;
    int score = calcSizedScore(psl, rmskHash, trfHash);
    
    verbose(3,"milli %d > %d match %d > %d score: %d best %d qName %s tName %s:%d-%d \n",
            calcMilliScore(psl), milliMin, 
            (psl->match + psl->repMatch + psl->misMatch) , round(minCover * psl->qSize),
            score, bestScore, psl->qName, psl->tName, psl->tStart, psl->tEnd );
    if (
        calcMilliScore(psl) >= milliMin && closeToTop(psl, scoreTrack, score) 
        && (psl->match + psl->repMatch + psl->misMatch) >= round(minCover * psl->qSize))
	{
        ++bestAliCount;
        AllocVar(pslMerge);
        pslMergeBlocks(psl, pslMerge, 30);
        verbose(4,"merge blockCount %d \n", pslMerge->blockCount);
        
        if (score  > bestScore && pslMerge->blockCount > 1)
            {
            bestPsl = psl;
            bestStart = psl->tStart;
            bestEnd = psl->tEnd;
            bestChrom = cloneString(psl->tName);
            bestScore = score;
            safef(bestStrand, sizeof(bestStrand), psl->strand );
            verbose(2,"BEST score: %d tName %s:%d \n",score,psl->tName,psl->tStart);
            }
        if (score  > bestSEScore )
            {
            bestSEPsl = psl;
            bestSEStart = psl->tStart;
            bestSEEnd = psl->tEnd;
            bestSEChrom = cloneString(psl->tName);
            bestSEScore = score  ;
            safef(bestSEStrand, sizeof(bestSEStrand), psl->strand );
            verbose(2,"BEST single Exon score: %d tName %s:%d \n",score,psl->tName,psl->tStart);
            }
        if (pslMerge->blockCount > maxExons )
            maxExons = pslMerge->blockCount;
        pslFree(&pslMerge);
	}
    }
if (bestScore== 0)
    { /* take best single exon hit, if no multi exon */
    bestPsl = bestSEPsl;
    bestStart = bestSEStart;
    bestEnd = bestSEEnd;
    bestChrom = bestSEChrom;
    bestScore = bestSEScore;
    safef(bestStrand , sizeof(bestStrand), bestSEStrand) ;
    }
if (bestChrom != NULL)
    verbose(2,"---DONE finding best--- %s:%d-%d\n",bestChrom, bestStart, bestEnd);
/* output parent genes, retrogenes, and calculate feature vector */
for (psl = pslList; psl != NULL; psl = psl->next)
{
int score = calcSizedScore(psl, rmskHash, trfHash);
if (
    calcMilliScore(psl) >= milliMin && closeToTop(psl, scoreTrack, score) 
    && psl->match + psl->misMatch + psl->repMatch >= minCover * psl->qSize)
        {
        pslTabOut(psl, bestFile);
        }
else 
    {
    /* calculate various features of pseudogene and output feature records */
    pseudoFeaturesCalc(psl, bestPsl, maxExons, bestAliCount, 
            bestChrom, bestStart, bestEnd, bestStrand );
    }
}
freeMem(scoreTrack);
}

void pslPseudo(char *inName, char *bestAliName, char *psuedoFileName, char *linkFileName, char *axtFileName)
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
bestFile = mustOpen(bestAliName, "w");
pseudoFile = mustOpen(psuedoFileName, "w");
linkFile = mustOpen(linkFileName, "w");
axtFile = mustOpen(axtFileName, "w");

verbose(1,"Processing %s to %s and %s\n", inName, bestAliName, psuedoFileName);
 if (!noHead)
     pslWriteHead(bestFile);
safef(lastName, sizeof(lastName),"x");
while (lineFileNext(in, &line, &lineSize))
    {
    if ((++aliCount & 0x1ffff) == 0)
        {
	verboseDot();
	}
    wordCount = chopTabs(line, words);
    if (wordCount != 21)
	errAbort("Bad line %d of %s\n", in->lineIx, in->fileName);
    psl = pslLoad(words);
    if (stripVersion)
        chopSuffix(psl->qName);
    verbose(2,"scoring %s version %d\n",psl->qName, stripVersion);
    if (!samePrefix(lastName, psl->qName))
	{
        slReverse(&pslList);
	processBestMulti(lastName, pslList);
	pslFreeList(&pslList);
	safef(lastName, sizeof(lastName), "%s", psl->qName);
	}
    slAddHead(&pslList, psl);
    }
slReverse(&pslList);
processBestMulti(lastName, pslList);
pslFreeList(&pslList);
lineFileClose(&in);
fclose(bestFile);
fclose(pseudoFile);
fclose(linkFile);
fclose(axtFile);
verbose(1,"Processed %d alignments\n", aliCount);
}

int main(int argc, char *argv[])
/* Process command line. */
{
//struct genePredReader *gprKg;

optionInit(&argc, argv, optionSpecs);
if (argc != 19)
    usage(argc);
verboseSetLogFile("stdout");
verbosity = optionInt("verbose", 1);
verboseSetLevel(verbosity);
verbose(1,"version is %s\n",rcsid);
ss = axtScoreSchemeDefault();
/* smaller gap to handle spliced introns */
ss->gapExtend = 5;
fileHash = newHash(0);  
tHash = newHash(20);  /* seqFilePos value. */
qHash = newHash(20);  /* seqFilePos value. */
fileCache = newDlList();
db = cloneString(argv[1]);
nibDir = cloneString(argv[11]);
minAli = optionFloat("minAli", minAli);
maxRep = optionFloat("maxRep", maxRep);
minAliPseudo = optionFloat("minAliPseudo", minAliPseudo);
nearTop = optionFloat("nearTop", nearTop);
splicedOverlapRatio = optionFloat("splicedOverlapRatio", splicedOverlapRatio);
minCover = optionFloat("minCover", minCover);
minCoverPseudo = optionFloat("minCoverPseudo", minCoverPseudo);
minNearTopSize = optionInt("minNearTopSize", minNearTopSize);
intronSlop = optionInt("intronSlop", intronSlop);
spliceDrift = optionInt("spliceDrift", spliceDrift);
maxBlockGap = optionInt("maxBlockGap" , maxBlockGap) ;
ignoreSize = optionExists("ignoreSize");
skipExciseRepeats = optionExists("skipExciseRepeats");
noIntrons = optionExists("noIntrons");
stripVersion = optionExists("stripVersion");
showall = optionExists("showall");
noHead = optionExists("nohead");
initWeights();

srand(time(NULL));
//sleep((float)rand()*10/RAND_MAX);
verbose(1,"Scanning %s\n", argv[13]);
hashFileList(argv[13], fileHash, tHash);
verbose(1,"Loading mrna sequences from %s\n",argv[14]);
//mrnaList = faReadAllMixed(argv[14]);
//if (mrnaList == NULL)
twoBitFile = twoBitOpen(argv[14]);
if (twoBitFile == NULL)
    errAbort("could not open %s\n",argv[14]);
mrnaList = twoBitSeqNames(argv[14]);
gpList1 = genePredLoadAll(argv[15]);
gpList2 = genePredLoadAll(argv[16]);
//gprKg = genePredReaderFile(argv[16], NULL);
//kgLis = genePredReaderAll(gprKg);
kgList = genePredLoadAll(argv[17]);
//mrnaGene = genePredLoadAll(argv[18]);

//verbose(1,"Loading Syntenic Bed %s\n",argv[5]);
//synHash = readBedCoordToBinKeeper(argv[3], argv[5], BEDCOUNT);
verbose(1,"Loading net %s\n",argv[5]);
synHash = readNetToBinKeeper(argv[3], argv[5]);
verbose(1,"Loading net %s\n",argv[6]);
syn2Hash = readNetToBinKeeper(argv[3], argv[6]);
verbose(1,"Loading net %s\n",argv[18]);
syn3Hash = readNetToBinKeeper(argv[3], argv[18]);

verbose(1,"Loading Trf Bed %s\n",argv[7]);
trfHash = readBedCoordToBinKeeper(argv[3], argv[7], BEDCOUNT);

verbose(1,"Reading Repeats from %s\n",argv[4]);
rmskHash = readBedCoordToBinKeeper(argv[3], argv[4], BEDCOUNT);

verbose(1,"Reading mrnas from %s\n",argv[8]);
mrnaHash = readPslToBinKeeper(argv[3], argv[8]);
verbose(1,"Scoring alignments from %s.\n",argv[2]);
verbose(1,"start\n");

pslPseudo(argv[2], argv[9], argv[10], argv[11], argv[12]);

verbose(1,"freeing everything\n");
binKeeperPslHashFree(&mrnaHash);
binKeeperHashFree(&synHash);
binKeeperHashFree(&syn2Hash);
binKeeperHashFree(&syn3Hash);
binKeeperHashFree(&trfHash);
genePredFreeList(&gpList1);
genePredFreeList(&gpList2);
genePredFreeList(&kgList);
//freeDnaSeqList(&mrnaList);
twoBitClose(&twoBitFile);
return 0;
}
