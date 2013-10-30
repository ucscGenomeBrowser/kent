/* pslPretty - Convert PSL to human readable output. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "dlist.h"
#include "fa.h"
#include "nib.h"
#include "twoBit.h"
#include "psl.h"
#include "axt.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslPretty - Convert PSL to human-readable output\n"
  "usage:\n"
  "   pslPretty in.psl target.lst query.lst pretty.out\n"
  "options:\n"
  "   -axt             Save in format like Scott Schwartz's axt format.\n"
  "                    Note gaps in both sequences are still allowed in the\n"
  "                    output, which not all axt readers will expect.\n"
  "   -dot=N           Output a dot every N records.\n"
  "   -long            Don't abbreviate long inserts.\n"
  "   -check=fileName  Output alignment checks to filename.\n" 
  "It's recommended that the psl file be sorted by target if it contains\n"
  "multiple targets; otherwise, this will be extremely slow. The target and query\n"
  "lists can be fasta, 2bit or nib files, or a list of these files, one per line.\n");
}

int dot = 0;
boolean doShort = FALSE;
struct axtScoreScheme *ss = NULL;

struct seqFilePos
/* Where a sequence is in a file. */
    {
    struct filePos *next;	/* Next in list. */
    char *name;	/* Sequence name. Allocated in hash. */
    char *file;	/* Sequence file name, allocated in hash. */
    long pos; /* Position in fa file/size of nib. */
    bool isNib;	/* True if a nib file. */
    bool isTwoBit;  /* True if a two bit file. */
    struct twoBitFile *tbf;	/* Open two bit file. */
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

void addTwoBit(char *file, struct hash *fileHash, struct hash *seqHash)
/* Add a nib file to hashes. */
{
struct seqFilePos *sfp;
struct twoBitFile *tbf = twoBitOpen(file);
struct twoBitIndex *index;
for (index = tbf->indexList; index != NULL; index = index->next)
    {
    AllocVar(sfp);
    hashAddSaveName(seqHash, index->name, sfp, &sfp->name);
    sfp->file = hashStoreName(fileHash, file);
    sfp->isTwoBit = TRUE;
    sfp->tbf = tbf;
    }
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


void hashFileList(char *fileList, struct hash *fileHash, struct hash *seqHash)
/* Read file list into hash */
{
if (twoBitIsFile(fileList))
    addTwoBit(fileList, fileHash, seqHash);
else if (endsWith(fileList, ".nib"))
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
	if (twoBitIsFile(file))
	    addTwoBit(file, fileHash, seqHash);
	else if (endsWith(file, ".nib"))
	    addNib(file, fileHash, seqHash);
	else
	    addFa(file, fileHash, seqHash);
	}
    lineFileClose(&lf);
    }
}

struct cachedFile
/* File in cache. */
    {
    struct cachedFile *next;	/* next in list. */
    char *name;		/* File name (allocated here) */
    FILE *f;		/* Open file. */
    };

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

struct dnaSeq *readSeqFromFaPos(struct seqFilePos *sfp,  FILE *f)
/* Read part of FA file. */
{
struct dnaSeq *seq;
fseek(f, sfp->pos, SEEK_SET);
if (!faReadMixedNext(f, TRUE, "", TRUE, NULL, &seq))
    errAbort("Couldn't faReadNext on %s in %s\n", sfp->name, sfp->file);
return seq;
}


void readCachedSeqPart(char *seqName, int start, int size, 
     struct hash *hash, struct dlList *fileCache, 
     struct dnaSeq **retSeq, int *retOffset, boolean *retIsPartial)
/* Read sequence hopefully using file cashe. If sequence is in a nib
 * file just read part of it. */
{
struct seqFilePos *sfp = hashMustFindVal(hash, seqName);
FILE *f = openFromCache(fileCache, sfp->file);
if (sfp->isNib)
    {
    *retSeq = nibLdPartMasked(NIB_MASK_MIXED, sfp->file, f, sfp->pos, start, size);
    *retOffset = start;
    *retIsPartial = TRUE;
    }
else if (sfp->isTwoBit)
    {
    *retSeq = twoBitReadSeqFrag(sfp->tbf, seqName, start, start+size);
    *retOffset = start;
    *retIsPartial = TRUE;
    }
else
    {
    *retSeq = readSeqFromFaPos(sfp, f);
    *retOffset = 0;
    *retIsPartial = FALSE;
    }
}

void axtOutString(char *q, char *t, int size, int lineSize, 
	struct psl *psl, FILE *f)
/* Output string side-by-side in Scott's axt format. */
{
int i;
static int ix = 0;
int qs = psl->qStart, qe = psl->qEnd;
int ts = psl->tStart, te = psl->tEnd;
int score = axtScoreSym(ss, size, q, t);

if (psl->strand[0] == '-')
    reverseIntRange(&qs, &qe, psl->qSize);

if (psl->strand[1] == '-')
    reverseIntRange(&ts, &te, psl->tSize);

if (psl->strand[1] != 0)
    fprintf(f, "%d %s %d %d %s %d %d %c%c %d\n", ++ix, psl->tName, ts+1, 
            te, psl->qName, qs+1, qe, psl->strand[1], psl->strand[0], score);
else
    fprintf(f, "%d %s %d %d %s %d %d %c %d\n", ++ix, psl->tName, psl->tStart+1, 
            psl->tEnd, psl->qName, qs+1, qe, psl->strand[0], score);
if (strlen(t) != size)
    warn("size of T %ld and Q %d differ on line %d\n",(long)strlen(t), size, ix);
for (i=0; i<size ; i++) 
    fputc(t[i],f);
fputc('\n',f);
if (strlen(q) != size)
    warn("size of T %ld and Q %d differ on line %d\n",(long)strlen(q), size, ix);
for (i=0; i<size ; i++) 
    fputc(q[i],f);
fputc('\n',f);
fputc('\n',f);
}

void prettyOutString(char *q, char *t, int size, int lineSize, 
	struct psl *psl, FILE *f)
/* Output string side-by-side. */
{
int oneSize, sizeLeft = size;
int i;
char tStrand = (psl->strand[1] == '-' ? '-' : '+');

fprintf(f, ">%s:%d%c%d of %d %s:%d%c%d of %d\n", 
	psl->qName, psl->qStart, psl->strand[0], psl->qEnd, psl->qSize,
	psl->tName, psl->tStart, tStrand, psl->tEnd, psl->tSize);
while (sizeLeft > 0)
    {
    oneSize = sizeLeft;
    if (oneSize > lineSize)
        oneSize = lineSize;
    mustWrite(f, q, oneSize);
    fputc('\n', f);

    for (i=0; i<oneSize; ++i)
        {
	if (toupper(q[i]) == toupper(t[i]) && isalpha(q[i]))
	    fputc('|', f);
	else
	    fputc(' ', f);
	}
    fputc('\n', f);

    if (oneSize > lineSize)
        oneSize = lineSize;
    mustWrite(f, t, oneSize);
    fputc('\n', f);
    fputc('\n', f);
    sizeLeft -= oneSize;
    q += oneSize;
    t += oneSize;
    }
}

void fillShortGapString(char *buf, int gapSize, char gapChar, int shortSize)
/* Fill in buf with something like  ---100--- (for gapSize 100
 * and shortSize 9 */
{
char gapAsNum[16];
int gapNumLen;
int dashCount, dashBefore, dashAfter;
sprintf(gapAsNum, "%d", gapSize);
gapNumLen = strlen(gapAsNum);
dashCount = shortSize - gapNumLen;
dashBefore = dashCount/2;
dashAfter = dashCount - dashBefore;
memset(buf, gapChar, dashBefore);
memcpy(buf+dashBefore, gapAsNum, gapNumLen);
memset(buf+dashBefore+gapNumLen, gapChar, dashAfter);
buf[shortSize] = 0;
}

void fillSpliceSites(char *buf, int gapSize, char *gapSeq, int shortSize)
/* Fill in buf with something like  gtcag...cctag */
{
int minDotSize = 3;	  /* Minimum number of dots. */
int dotSize = minDotSize; /* Actual number of dots. */
int seqBefore, seqAfter, seqTotal;

if (shortSize - dotSize > gapSize)
    dotSize =  shortSize - gapSize;
seqTotal = shortSize - dotSize;
seqBefore = seqTotal/2;
seqAfter = seqTotal - seqBefore;
memcpy(buf, gapSeq, seqBefore);
memset(buf+seqBefore, '.', dotSize);
memcpy(buf+seqBefore+dotSize, gapSeq + gapSize - seqAfter, seqAfter);
buf[shortSize] = 0;
}


void writeInsert(struct dyString *aRes, struct dyString *bRes, char *aSeq, int gapSize)
/* Write out gap, possibly shortened, to aRes, bRes. */
{
int minToAbbreviate = 16;
if (doShort && gapSize >= minToAbbreviate)
    {
    char abbrevGap[16];
    char abbrevSeq[16];
    fillSpliceSites(abbrevSeq, gapSize, aSeq, 15);
    dyStringAppend(aRes, abbrevSeq);
    fillShortGapString(abbrevGap, gapSize, '-', 15);
    dyStringAppend(bRes, abbrevGap);
    }
else
    {
    dyStringAppendN(aRes, aSeq, gapSize);
    dyStringAppendMultiC(bRes, '-', gapSize);
    }
}


void writeGap(struct dyString *aRes, int aGap, char *aSeq, struct dyString *bRes, int bGap, char *bSeq)
/* Write double - gap.  Something like:
 *     ....123.... or --c
 *     ...4123....    ag-  */

{
char abbrev[16];
int minToAbbreviate = 16;
if (doShort && (aGap >= minToAbbreviate || bGap >= minToAbbreviate))
    {
    fillShortGapString(abbrev, aGap, '.', 13);
    dyStringAppend(aRes, abbrev);
    fillShortGapString(abbrev, bGap, '.', 13);
    dyStringAppend(bRes, abbrev);
    }
else
    {
#ifdef OLD
    dyStringAppendMultiC(aRes, '-', aGap);
    dyStringAppendN(bRes, bSeq, aGap);
    dyStringAppendN(aRes, aSeq, bGap);
    dyStringAppendMultiC(bRes, '-', bGap);
#endif /* OLD */
    dyStringAppendMultiC(aRes, '-', bGap);
    dyStringAppendN(bRes, bSeq, bGap);
    dyStringAppendN(aRes, aSeq, aGap);
    dyStringAppendMultiC(bRes, '-', aGap);
    }
}

int smallSize = 8;	/* What our definition of 'small' is */
int tinySize = 3;

int boolify(int i)
/* Convert 0 to 0 and nonzero to 1 */
{
return (i != 0 ? 1 : 0);
}

int total_rnaCount;
int total_rnaPerfect;
int total_missSmallStart = 0;
int total_missLargeStart = 0;
int total_missSmallEnd = 0;
int total_missLargeEnd = 0;
int total_missSmallMiddle = 0;
int total_missLargeMiddle = 0;
int total_weirdSplice = 0;
int total_doubleGap = 0;
int total_jumpBack = 0;

boolean isIntron(char strand, char *start, char *end)
/* See if it look like an intron. */
{
if (strand == '+')
    {
    if (start[0] != 'g' || end[-2] != 'a' || end[-1] != 'g')
	return FALSE;
    return (start[1] == 't' || start[1] == 'c');
    }
else
    {
    if (start[0] != 'c' || start[1] != 't' || end[-1] != 'c')
	return FALSE;
    return (end[-2] == 'a' || end[-2] == 'g');
    }
}

void outputCheck(struct psl *psl, struct dnaSeq *qSeq, int qOffset,
	struct dnaSeq *tSeq, int tOffset, FILE *f)
/* Output quality check info to file */
{
int sizePolyA = 0;
int qSize = psl->qSize;
int i;
int missSmallStart = 0;
int missLargeStart = 0;
int missSmallEnd = 0;
int missLargeEnd = 0;
int missSmallMiddle = 0;
int missLargeMiddle = 0;
int weirdSplice = 0;
int doubleGap = 0;
int jumpBack = 0;
int diff;
int totalProblems = 0;
char strand = psl->strand[0];

if (strand == '+')
    {
    for (i=1; i<=qSize; ++i)
	{
	if (qSeq->dna[qSize - i - qOffset] == 'a')
	    ++sizePolyA;
	else
	    break;
	}
    }
else
    {
    for (i=0; i<qSize; ++i)
	{
	if (qSeq->dna[i - qOffset] == 't')
	    ++sizePolyA;
	else
	    break;
	}
    }
if (psl->qStart > tinySize)
    {
    if (psl->qStart <= smallSize)
	{
	missSmallStart = psl->qStart;
	++totalProblems;
	}
    else
	{
	missLargeStart = psl->qStart;
	++totalProblems;
	}
    }
diff = psl->qSize - psl->qEnd - sizePolyA;
if (diff > tinySize)
    {
    if (diff <= smallSize)
	{
	missSmallEnd = diff;
	++totalProblems;
	}
    else
	{
	missLargeEnd = diff;
	++totalProblems;
	}
    }
for (i=0; i<psl->blockCount-1; ++i)
    {
    int nextT = psl->tStarts[i+1];
    int nextQ = psl->qStarts[i+1];
    int sz = psl->blockSizes[i];
    int t = psl->tStarts[i] + sz;
    int q = psl->qStarts[i] + sz;
    int dq = nextQ - q;
    int dt = nextT - t;
    if (dq < 0 || dt < 0)
	{
	++jumpBack;
	++totalProblems;
	}
    else 
	{
	if (dq > 0 && dt > 0)
	    {
	    ++doubleGap;
	    ++totalProblems;
	    }
	if (dq > tinySize)
	    {
	    if (dq > smallSize)
		{
		++missLargeMiddle;
		++totalProblems;
		}
	    else
		{
		++missSmallMiddle;
		++totalProblems;
		}
	    }
	if (dq == 0 && dt >=30)
	    {
	    char *dna = tSeq->dna - tOffset;
	    if (!isIntron(strand, dna + t, dna + nextT))
		{
		++weirdSplice;
		++totalProblems;
		}
	    }
	}
    }
fprintf(f, "%2d %9s %s ", totalProblems, psl->qName, psl->strand);
fprintf(f, "%4dS ", missLargeStart);
fprintf(f, "%2ds ", missSmallStart);
fprintf(f, "%4dE ", missLargeEnd);
fprintf(f, "%2de ", missSmallEnd);
fprintf(f, "%2dM ", missLargeMiddle);
fprintf(f, "%2dm ", missSmallMiddle);
fprintf(f, "%2dW ", weirdSplice);
fprintf(f, "%2dG ", doubleGap);
fprintf(f, "%2dJ ", jumpBack);
fprintf(f, "\n");

total_missSmallStart += boolify(missSmallStart);
total_missLargeStart += boolify(missLargeStart);
total_missSmallEnd += boolify(missSmallEnd);
total_missLargeEnd += boolify(missLargeEnd);
total_missSmallMiddle += boolify(missSmallMiddle);
total_missLargeMiddle += boolify(missLargeMiddle);
total_weirdSplice += boolify(weirdSplice);
total_doubleGap += boolify(doubleGap);
total_jumpBack += boolify(jumpBack);
++total_rnaCount;
if (totalProblems == 0)
    ++total_rnaPerfect;
}

void prettyOne(struct psl *psl, struct hash *qHash, struct hash *tHash,
	struct dlList *fileCache, FILE *f, boolean axt, FILE *checkFile)
/* Make pretty output for one psl.  Find target and query
 * sequence in hash.  Load them.  Output bases. */
{
static char *tName = NULL, *qName = NULL;
static struct dnaSeq *tSeq = NULL, *qSeq = NULL;
struct dyString *q = newDyString(16*1024);
struct dyString *t = newDyString(16*1024);
int blockIx;
int qs, ts;
int lastQ = 0, lastT = 0, size;
int qOffset = 0;
int tOffset = 0;
boolean qIsPartial = FALSE;
boolean tIsPartial = FALSE;

if (qName == NULL || !sameString(qName, psl->qName))
    {
    freeDnaSeq(&qSeq);
    freez(&qName);
    qName = cloneString(psl->qName);
    readCachedSeqPart(qName, psl->qStart, psl->qEnd-psl->qStart, 
    	qHash, fileCache, &qSeq, &qOffset, &qIsPartial);
    if (qIsPartial && psl->strand[0] == '-')
	    qOffset = psl->qSize - psl->qEnd;
    }
if (tName == NULL || !sameString(tName, psl->tName) || tIsPartial)
    {
    freeDnaSeq(&tSeq);
    freez(&tName);
    tName = cloneString(psl->tName);
    readCachedSeqPart(tName, psl->tStart, psl->tEnd-psl->tStart, 
	tHash, fileCache, &tSeq, &tOffset, &tIsPartial);
    }
if (tIsPartial && psl->strand[1] == '-')
    tOffset = psl->tSize - psl->tEnd;
if (psl->strand[0] == '-')
    reverseComplement(qSeq->dna, qSeq->size);
if (psl->strand[1] == '-')
    reverseComplement(tSeq->dna, tSeq->size);
for (blockIx=0; blockIx < psl->blockCount; ++blockIx)
    {
    qs = psl->qStarts[blockIx] - qOffset;
    ts = psl->tStarts[blockIx] - tOffset;

    /* Output gaps except in first case. */
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
    /* Output sequence. */
    size = psl->blockSizes[blockIx];
    dyStringAppendN(q, qSeq->dna + qs, size);
    lastQ = qs + size;
    dyStringAppendN(t, tSeq->dna + ts, size);
    lastT = ts + size;
    if(q->stringSize != t->stringSize)
        {
//        printf("%d BLK %s q size %d t size %d diff %d qs size %d ts size %d\n",blockIx, psl->qName, q->stringSize, t->stringSize, q->stringSize - t->stringSize, qSeq->size, tSeq->size );
        }
    }

if (checkFile != NULL)
    {
    outputCheck(psl, qSeq, qOffset, tSeq, tOffset, checkFile);
    }
if (psl->strand[0] == '-' && !qIsPartial)
    reverseComplement(qSeq->dna, qSeq->size);
if (psl->strand[1] == '-' && !tIsPartial)
    reverseComplement(tSeq->dna, tSeq->size);

if(q->stringSize != t->stringSize)
    {
 //   printf("AF %s q size %d t size %d qs size %d ts size %d\n",psl->qName, q->stringSize, t->stringSize, qSeq->size, tSeq->size );
    }
//assert(q->stringSize == t->stringSize);
if (axt)
    axtOutString(q->string, t->string, min(q->stringSize,t->stringSize), 60, psl, f);
else
    prettyOutString(q->string, t->string, min(q->stringSize,t->stringSize), 60, psl, f);
dyStringFree(&q);
dyStringFree(&t);
if (qIsPartial)
    freez(&qName);
if (tIsPartial)
    freez(&tName);
}

void pslPretty(char *pslName, char *targetList, char *queryList, 
	char *prettyName, boolean axt, char *checkFileName)
/* pslPretty - Convert PSL to human readable output. */
{
struct hash *fileHash = newHash(0);  /* No value. */
struct hash *tHash = newHash(20);  /* seqFilePos value. */
struct hash *qHash = newHash(20);  /* seqFilePos value. */
struct dlList *fileCache = newDlList();
struct lineFile *lf = pslFileOpen(pslName);
FILE *f = mustOpen(prettyName, "w");
FILE *checkFile = NULL;
struct psl *psl;
int dotMod = dot;

if (checkFileName != NULL)
    checkFile = mustOpen(checkFileName, "w");
/* fprintf(stderr,"Scanning %s\n", targetList); */
hashFileList(targetList, fileHash, tHash);
/* fprintf(stderr,"Scanning %s\n", queryList); */
hashFileList(queryList, fileHash, qHash);
/* fprintf(stderr,"Converting %s\n", pslName); */
while ((psl = pslNext(lf)) != NULL)
    {
    if (dot > 0)
        {
	if (--dotMod <= 0)
	   {
	   fprintf(stderr,"."); /* stderr flushes itself */
	   dotMod = dot;
	   }
	}
    prettyOne(psl, qHash, tHash, fileCache, f, axt, checkFile);
    pslFree(&psl);
    }
if (dot > 0)
    fprintf(stderr,"\n");
if (checkFile != NULL)
    {
    fprintf(checkFile,"missLargeStart: %d\n", total_missLargeStart);
    fprintf(checkFile,"missSmallStart: %d\n", total_missSmallStart);
    fprintf(checkFile,"missLargeEnd: %d\n", total_missLargeEnd);
    fprintf(checkFile,"missSmallEnd: %d\n", total_missSmallEnd);
    fprintf(checkFile,"missLargeMiddle: %d\n", total_missLargeMiddle);
    fprintf(checkFile,"missSmallMiddle: %d\n", total_missSmallMiddle);
    fprintf(checkFile,"weirdSplice: %d\n", total_weirdSplice);
    fprintf(checkFile,"doubleGap: %d\n", total_doubleGap);
    fprintf(checkFile,"jumpBack: %d\n", total_jumpBack);
    fprintf(checkFile,"perfect: %d\n", total_rnaPerfect);
    fprintf(checkFile,"total: %d\n", total_rnaCount);
    }
lineFileClose(&lf);
carefulClose(&f);
carefulClose(&checkFile);
}

int main(int argc, char *argv[])
/* Process command line. */
{
static boolean axt;
optionHash(&argc, argv);
axt = optionExists("axt");
dot = optionInt("dot", dot);
doShort = !optionExists("long");
if (argc != 5)
    usage();
ss = axtScoreSchemeDefault();
pslPretty(argv[1], argv[2], argv[3], argv[4], axt, optionVal("check", NULL));
return 0;
}
