/* pslPretty - Convert PSL to human readable output. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "dlist.h"
#include "fa.h"
#include "nib.h"
#include "psl.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslPretty - Convert PSL to human readable output\n"
  "usage:\n"
  "   pslPretty in.psl target.lst query.lst pretty.out\n"
  "options:\n"
  "   -axt - save in something like Scott Schwartz's axt format\n"
  "          Note gaps in both sequences are still allowed in the\n"
  "          output which not all axt readers will expect\n"
  "   -dot=N Put out a dot every N records\n"
  "   -long - Don't abbreviate long inserts\n"
  "It's a really good idea if the psl file is sorted by target\n"
  "if it contains multiple targets.  Otherwise this will be\n"
  "very very slow.   The target and query lists can either be\n"
  "fasta files, nib files, or a list of fasta and/or nib files\n"
  "one per line\n");
}

int dot = 0;
boolean doShort = FALSE;

struct seqFilePos
/* Where a sequence is in a file. */
    {
    struct filePos *next;	/* Next in list. */
    char *name;	/* Sequence name. Allocated in hash. */
    char *file;	/* Sequence file name, allocated in hash. */
    long pos; /* Position in fa file/size of nib. */
    bool isNib;	/* True if a nib file. */
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
if (!faReadNext(f, "", TRUE, NULL, &seq))
    errAbort("Couldn't faReadNext on %s in %s\n", sfp->name, sfp->file);
return seq;
}

struct dnaSeq *readCachedSeq(char *seqName, struct hash *hash, 
	struct dlList *fileCache)
/* Read sequence hopefully using file cashe. */
{
struct seqFilePos *sfp = hashMustFindVal(hash, seqName);
FILE *f = openFromCache(fileCache, sfp->file);
if (sfp->isNib)
    {
    return nibLdPart(sfp->file, f, sfp->pos, 0, sfp->pos);
    }
else
    {
    return readSeqFromFaPos(sfp, f);
    }
}

void readCachedSeqPart(char *seqName, int start, int size, 
     struct hash *hash, struct dlList *fileCache, 
     struct dnaSeq **retSeq, int *retOffset, boolean *retIsNib)
/* Read sequence hopefully using file cashe. If sequence is in a nib
 * file just read part of it. */
{
struct seqFilePos *sfp = hashMustFindVal(hash, seqName);
FILE *f = openFromCache(fileCache, sfp->file);
if (sfp->isNib)
    {
    *retSeq = nibLdPart(sfp->file, f, sfp->pos, start, size);
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

void axtOutString(char *q, char *t, int size, int lineSize, 
	struct psl *psl, FILE *f)
/* Output string side-by-side in Scott's axt format. */
{
static int ix = 0;
int qs = psl->qStart, qe = psl->qEnd;

if (psl->strand[0] == '-')
    reverseIntRange(&qs, &qe, psl->qSize);

if (psl->strand[1] == '-')
    errAbort("axt option can't handle two character strands in psl");
if (psl->strand[1] == 0)
fprintf(f, "%d %s %d %d %s %d %d %c%c 0\n", ++ix, psl->tName, psl->tStart+1, 
	psl->tEnd, psl->qName, qs+1, qe, psl->strand[1], psl->strand[0]);
fprintf(f, "%s\n%s\n\n", t, q);
}

void prettyOutString(char *q, char *t, int size, int lineSize, 
	struct psl *psl, FILE *f)
/* Output string side-by-side. */
{
int oneSize, sizeLeft = size;
int i;
char tStrand = (psl->strand[1] == '-' ? '-' : '+');

fprintf(f, ">%s:%d%c%d %s:%d%c%d\n", 
	psl->qName, psl->qStart, psl->strand[0], psl->qEnd,
	psl->tName, psl->tStart, tStrand, psl->tEnd);
while (sizeLeft > 0)
    {
    oneSize = sizeLeft;
    if (oneSize > lineSize)
        oneSize = lineSize;
    mustWrite(f, q, oneSize);
    fputc('\n', f);

    for (i=0; i<oneSize; ++i)
        {
	if (q[i] == t[i] && isalpha(q[i]))
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


void writeGap(struct dyString *aRes, int aGap, struct dyString *bRes, int bGap)
/* Write double - gap.  Something like:
 *     ....123....
 *     ...4123.... */
{
char abbrev[16];
fillShortGapString(abbrev, aGap, '.', 13);
dyStringAppend(aRes, abbrev);
fillShortGapString(abbrev, bGap, '.', 13);
dyStringAppend(bRes, abbrev);
}

void prettyOne(struct psl *psl, struct hash *qHash, struct hash *tHash,
	struct dlList *fileCache, FILE *f, boolean axt)
/* Make pretty output for one psl.  Find target and query
 * sequence in hash.  Load them.  Output bases. */
{
static char *tName = NULL, *qName = NULL;
static struct dnaSeq *tSeq = NULL, *qSeq = NULL;
struct dyString *q = newDyString(16*1024);
struct dyString *t = newDyString(16*1024);
int blockIx, diff;
int qs, ts;
int lastQ = 0, lastT = 0, size;
int qOffset = 0;
int tOffset = 0;
boolean qIsNib = FALSE;
boolean tIsNib = FALSE;

if (qName == NULL || !sameString(qName, psl->qName))
    {
    freeDnaSeq(&qSeq);
    freez(&qName);
    qName = cloneString(psl->qName);
    readCachedSeqPart(qName, psl->qStart, psl->qEnd-psl->qStart, 
    	qHash, fileCache, &qSeq, &qOffset, &qIsNib);
    if (qIsNib && psl->strand[0] == '-')
	    qOffset = psl->qSize - psl->qEnd;
    }
/* if (tName == NULL) || !sameString(tName, psl->tName))
    {*/
    freeDnaSeq(&tSeq);
    freez(&tName);
    tName = cloneString(psl->tName);
    //tSeq = readCachedSeq(tName, tHash, fileCache);
    readCachedSeqPart(tName, psl->tStart, psl->tEnd-psl->tStart, 
    	tHash, fileCache, &tSeq, &tOffset, &tIsNib);
    if (tIsNib && psl->strand[1] == '-')
	    tOffset = psl->tSize - psl->tEnd;
/*    }*/
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
	    writeGap(q, qGap, t, tGap);
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
/*    printf("BUild tStart %d qStart %d strand %s q size %d t size %d qs size %d ts size %d\n",psl->tStart,psl->qStart,psl->strand,q->stringSize, t->stringSize, qSeq->size, tSeq->size );*/
    /* Output sequence. */
    size = psl->blockSizes[blockIx];
    dyStringAppendN(q, qSeq->dna + qs, size);
    dyStringAppendN(t, tSeq->dna + ts, size);
    lastQ = qs + size;
    lastT = ts + size;
    }

/*    printf("BF q size %d t size %d qs size %d ts size %d\n",q->stringSize, t->stringSize, qSeq->size, tSeq->size );*/
if (psl->strand[0] == '-' && !qIsNib)
    reverseComplement(qSeq->dna, qSeq->size);
if (psl->strand[1] == '-')
    reverseComplement(tSeq->dna, tSeq->size);

if(q->stringSize != t->stringSize)
    {
    printf("AF q size %d t size %d qs size %d ts size %d\n",q->stringSize, t->stringSize, qSeq->size, tSeq->size );
    }
assert(q->stringSize == t->stringSize);
if (axt)
    axtOutString(q->string, t->string, q->stringSize, 60, psl, f);
else
    prettyOutString(q->string, t->string, q->stringSize, 60, psl, f);
dyStringFree(&q);
dyStringFree(&t);
if (qIsNib)
    freez(&qName);
}

void pslPretty(char *pslName, char *targetList, char *queryList, 
	char *prettyName, boolean axt)
/* pslPretty - Convert PSL to human readable output. */
{
struct hash *fileHash = newHash(0);  /* No value. */
struct hash *tHash = newHash(20);  /* seqFilePos value. */
struct hash *qHash = newHash(20);  /* seqFilePos value. */
struct dlList *fileCache = newDlList();
struct lineFile *lf = pslFileOpen(pslName);
FILE *f = mustOpen(prettyName, "w");
struct psl *psl;
int dotMod = dot;

printf("Scanning %s\n", targetList);
hashFileList(targetList, fileHash, tHash);
printf("Scanning %s\n", queryList);
hashFileList(queryList, fileHash, qHash);
printf("Converting %s\n", pslName);
while ((psl = pslNext(lf)) != NULL)
    {
    if (dot > 0)
        {
	if (--dotMod <= 0)
	   {
	   printf(".");
       fflush(stdout);
	   dotMod = dot;
	   }
	}
    prettyOne(psl, qHash, tHash, fileCache, f, axt);
    pslFree(&psl);
    }
lineFileClose(&lf);
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
pslPretty(argv[1], argv[2], argv[3], argv[4], axt);
return 0;
}
