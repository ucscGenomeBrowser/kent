/* chainToAxtRna - Convert chain to axt format. 
 * This file is copyright 2002 Robert Baertsch , but license is hereby
 * granted for all use - public, private or commercial. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "chainBlock.h"
#include "dnautil.h"
#include "nib.h"
#include "fa.h"
#include "axt.h"
#include "dystring.h"
#include "dlist.h"

static char const rcsid[] = "$Id: chainToAxtRna.c,v 1.2 2003/07/26 20:40:20 baertsch Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "chainToAxtRna - Convert chain file to axt format - allows double stranded gaps\n"
  "usage:\n"
  "   chainToAxtRna in.chain tSizes qSizes target.lst query.lst out.axt\n"
  "Where tSizes and qSizes are tab-delimited files with\n"
  "       <seqName><size>\n"
  "columns.\n"
  "The target and query lists can either be fasta files, nib files, \n"
  "or a list of fasta and/or nib files one per line\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

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
carefulClose(&f);
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
static int maxCacheSize=16;
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

int findSize(struct hash *hash, char *name)
/* Find size of name in hash or die trying. */
{
void *val = hashMustFindVal(hash, name);
return ptToInt(val);
}

void countInserts(char *s, int size, int *retNumInsert, int *retBaseInsert)
/* Count up number and total size of inserts in s. */
{
char c, lastC = s[0];
int i;
int baseInsert = 0, numInsert = 0;
if (lastC == '-')
    errAbort("%s starts with -", s);
for (i=0; i<size; ++i)
    {
    c = s[i];
    if (c == '-')
        {
	if (lastC != '-')
	     numInsert += 1;
	baseInsert += 1;
	}
    lastC = c;
    }
*retNumInsert = numInsert;
*retBaseInsert = baseInsert;
}

int countInitialChars(char *s, char c)
/* Count number of initial chars in s that match c. */
{
int count = 0;
char d;
while ((d = *s++) != 0)
    {
    if (c == d)
        ++count;
    else
        break;
    }
return count;
}

int countNonInsert(char *s, int size)
/* Count number of characters in initial part of s that
 * are not '-'. */
{
int count = 0;
int i;
for (i=0; i<size; ++i)
    if (*s++ != '-')
        ++count;
return count;
}

int countTerminalChars(char *s, char c)
/* Count number of initial chars in s that match c. */
{
int len = strlen(s), i;
int count = 0;
for (i=len-1; i>=0; --i)
    {
    if (c == s[i])
        ++count;
    else
        break;
    }
return count;
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
if (FALSE && gapSize >= minToAbbreviate)
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
int minToAbbreviate = 16;
if (FALSE && aGap >= minToAbbreviate && bGap >= minToAbbreviate)
    {
    fillShortGapString(abbrev, aGap, '.', 13);
    dyStringAppend(aRes, abbrev);
    fillShortGapString(abbrev, bGap, '.', 13);
    dyStringAppend(bRes, abbrev);
    }
else
    {
    dyStringAppendMultiC(aRes, '-', aGap);
    dyStringAppendMultiC(bRes, '-', bGap);
    }
}


void aliStringToAxt(struct lineFile *lf, char *qNameParm, char *tNameParm, 
	int qSize, int tSize, int aliSize, 
	int qStart, int qEnd, int tStart, int tEnd, char strand, FILE *f, struct chain *chain, struct hash *tHash, struct hash *qHash, struct dlList *fileCache )
/* Output alignment in a pair of strings with insert chars
 * to a axt to a file. */
{
struct axt *axt;
static char *tName = NULL, *qName = NULL;
static struct dnaSeq *tSeq = NULL, *qSeq = NULL;
struct dyString *q = newDyString(16*1024);
struct dyString *t = newDyString(16*1024);
unsigned match = 0;	/* Number of bases that match */
unsigned misMatch = 0;	/* Number of bases that don't match */
unsigned qNumInsert = 0;	/* Number of inserts in query */
int qBaseInsert = 0;	/* Number of bases inserted in query */
unsigned tNumInsert = 0;	/* Number of inserts in target */
int tBaseInsert = 0;	/* Number of bases inserted in target */
boolean qInInsert = FALSE; /* True if in insert state on query. */
boolean tInInsert = FALSE; /* True if in insert state on target. */
boolean eitherInsert = FALSE;	/* True if either in insert state. */
int lastQ = 0, lastT = 0, size;
int qOffset = 0;
int tOffset = 0;
int symCount;
boolean qIsNib = FALSE;
static boolean tIsNib;
int blockCount = 1, blockIx=0;
boolean qIsRc = FALSE;
int i;
int qs,qe,ts,te;
int *blocks = NULL, *qStarts = NULL, *tStarts = NULL;
struct boxIn *b, *nextB;
int qbSize = 0, tbSize = 0; /* sum of block sizes */
int qtSize = 0, ttSize = 0; /* sum of block + gap sizes */

/* Don't ouput if either query or target is zero length */
 if ((qStart == qEnd) || (tStart == tEnd))
     return;

if (qName == NULL || !sameString(qName, qNameParm))
    {
    freeDnaSeq(&qSeq);
    freez(&qName);
    qName = cloneString(qNameParm);
    readCachedSeqPart(qName, qStart, qEnd-qStart, 
    	qHash, fileCache, &qSeq, &qOffset, &qIsNib);
    if (qIsNib && strand == '-')
	    qOffset = qSize - qEnd;
    }
if (tName == NULL || !sameString(tName, tNameParm) || tIsNib)
    {
    freeDnaSeq(&tSeq);
    freez(&tName);
    tName = cloneString(tNameParm);
    readCachedSeqPart(tName, tStart, tEnd-tStart, 
	tHash, fileCache, &tSeq, &tOffset, &tIsNib);
    }
if (strand == '-')
    reverseComplement(qSeq->dna, qSeq->size);
for (b = chain->blockList; b != NULL; b = nextB)
    {
    blockCount++;
    qbSize += b->qEnd - b->qStart + 1;
    tbSize += b->tEnd - b->tStart + 1;
    nextB = b->next;
    qs = b->qStart - qOffset;
    ts = b->tStart - tOffset;

    /* Output gaps except in first case. */
    if (blockCount != 2)
        {
	int qGap = qs - lastQ;
	int tGap = ts - lastT;
	int minGap = min(qGap, tGap);
	int maxGap = max(qGap, tGap);

	if (minGap > 0)
	    {
	    writeGap(q, maxGap, t, maxGap);
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
    size = (b->tEnd)-(b->tStart);
    dyStringAppendN(q, qSeq->dna + qs, size);
    lastQ = qs + size;
    dyStringAppendN(t, tSeq->dna + ts, size);
    lastT = ts + size;
    if(q->stringSize != t->stringSize)
        {
        fprintf(stderr,"warning: unequal string size %d BLK %s q size %d t size %d diff %d qlen %d tlen %d tStart %d\n",blockIx, qName, q->stringSize, t->stringSize, q->stringSize - t->stringSize, strlen(q->string), strlen(t->string),tStart);
        }
    }

if (strand == '-')
    reverseComplement(qSeq->dna, qSeq->size);
/* Count up match/mismatch. */

/*
for (i=0; i<min(tSeq->size, qSeq->size); ++i)
    {
    char qq = qSeq->dna[i];
    char tt = tSeq->dna[i];
    if (qq != '-' && tt != '-')
	{
	if (qq == tt)
	    ++match;
	else
	    ++misMatch;
	}
    }
match=chain->score * tbSize / 70;
*/

AllocVar(axt);

axt->qName = cloneString(qName);
axt->qStart = qStart;
axt->qEnd = qEnd;
axt->qStrand = strand;
axt->tName = cloneString(tNameParm);
axt->tStart = tStart;
axt->tEnd = tEnd;
axt->tStrand = '+';

axt->symCount = symCount = q->stringSize;
axt->tSym = cloneString(t->string);//, t->stringSize);
if (strlen(t->string) != symCount)
    errAbort("Symbol count %d != %d inconsistent between sequences qName %s \n%s\n%s\n",
    	symCount, strlen(t->string), qName, q->string, t->string);
axt->qSym = cloneString(q->string);//, q->stringSize);

//printf("%d AF %s q size %d t size %d diff %d qlen %d tlen %d axtq %d axtt %d\n",blockIx, qName, q->stringSize, t->stringSize, q->stringSize - t->stringSize, strlen(q->string), strlen(t->string) , strlen(axt->qSym), strlen(axt->tSym));
axt->score = axtScoreDnaDefault(axt);

if (q->stringSize != t->stringSize)
    {
    fprintf(stderr,"warning: unequal qSize %d qsize %d\n",q->stringSize , t->stringSize);
    fprintf(stderr,"%s\n%s\n",q->string,t->string);
    }
axtWrite(axt, f);
/* Clean Up. */
freeDyString(&q);
freeDyString(&t);
axtFree(&axt);

}


void chainToAxtRna(char *inName, char *tSizeFile, char *qSizeFile,  char *targetList, char *queryList, char *outName)
/* chainToAxtRna - Convert chain file to axt format. */
{
struct hash *tSizeHash = readSizes(tSizeFile);
struct hash *qSizeHash = readSizes(qSizeFile);
struct lineFile *lf = lineFileOpen(inName, TRUE);
FILE *f = mustOpen(outName, "w");
struct hash *fileHash = newHash(0);  /* No value. */
struct hash *tHash = newHash(20);  /* seqFilePos value. */
struct hash *qHash = newHash(20);  /* seqFilePos value. */
struct dlList *fileCache = newDlList();
//FILE *f = mustOpen(prettyName, "w");
FILE *checkFile = NULL;
struct axt *axt;
struct chain *chain;
int q,t;

printf("Scanning %s\n", targetList);
hashFileList(targetList, fileHash, tHash);
printf("Scanning %s\n", queryList);
hashFileList(queryList, fileHash, qHash);
printf("Converting %s\n", inName);

while ((chain = chainRead(lf)) != NULL)
    {
    q = findSize(qSizeHash, chain->qName);
    t = findSize(tSizeHash, chain->tName);
    aliStringToAxt(lf, chain->qName, chain->tName, chain->qSize, chain->tSize,
	min(chain->tEnd-chain->tStart, chain->qEnd-chain->qStart), chain->qStart, chain->qEnd, chain->tStart, chain->tEnd,
        chain->qStrand, f, chain, tHash, qHash, fileCache);
    chainFree(&chain);
    }
lineFileClose(&lf);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 7)
    {
    fprintf(stderr,"argc %d\n",argc);
    usage();
    }
chainToAxtRna(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]);
return 0;
}
