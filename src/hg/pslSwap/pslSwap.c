/* pslSwap - Swap query and target in  PSL  */
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
  "pslSwap - Swap query and target in  PSL  \n"
  "usage:\n"
  "   pslPretty in.psl out.psl\n"
  );
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

void axtOutString(char *q, char *t, int size, int lineSize, 
	struct psl *psl, FILE *f)
/* Output string side-by-side in Scott's axt format. */
{
static int ix = 0;
int qs = psl->qStart, qe = psl->qEnd;

if (psl->strand[0] == '-')
    reverseIntRange(&qs, &qe, psl->qSize);

fprintf(f, "%d %s %d %d %s %d %d %s 0\n", ++ix, psl->tName, psl->tStart+1, 
	psl->tEnd, psl->qName, qs+1, qe, psl->strand);
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
void pslWrite(struct psl *el, FILE *f, char* q, char *t, char *blk, char sep, char lastSep) 
/* Print out psl.  Separate fields with sep. Follow last field with lastSep. */
{
int i;
fprintf(f, "%u", el->match);
fputc(sep,f);
fprintf(f, "%u", el->misMatch);
fputc(sep,f);
fprintf(f, "%u", el->repMatch);
fputc(sep,f);
fprintf(f, "%u", el->nCount);
fputc(sep,f);
fprintf(f, "%u", el->qNumInsert);
fputc(sep,f);
fprintf(f, "%d", el->qBaseInsert);
fputc(sep,f);
fprintf(f, "%u", el->tNumInsert);
fputc(sep,f);
fprintf(f, "%d", el->tBaseInsert);
fputc(sep,f);
if (sep == ',') fputc('"',f);
fprintf(f, "%s", el->strand);
if (sep == ',') fputc('"',f);
fputc(sep,f);
if (sep == ',') fputc('"',f);
fprintf(f, "%s", el->qName);
if (sep == ',') fputc('"',f);
fputc(sep,f);
fprintf(f, "%u", el->qSize);
fputc(sep,f);
fprintf(f, "%u", el->qStart);
fputc(sep,f);
fprintf(f, "%u", el->qEnd);
fputc(sep,f);
if (sep == ',') fputc('"',f);
fprintf(f, "%s", el->tName);
if (sep == ',') fputc('"',f);
fputc(sep,f);
fprintf(f, "%u", el->tSize);
fputc(sep,f);
fprintf(f, "%u", el->tStart);
fputc(sep,f);
fprintf(f, "%u", el->tEnd);
fputc(sep,f);
fprintf(f, "%u", el->blockCount);
fputc(sep,f);
if (sep == ',') fputc('{',f);
fprintf(f, "%s", blk);
if (sep == ',') fputc('}',f);
fputc(sep,f);
if (sep == ',') fputc('{',f);
fprintf(f, "%s", q);
if (sep == ',') fputc('}',f);
fputc(sep,f);
if (sep == ',') fputc('{',f);
fprintf(f, "%s", t);
if (sep == ',') fputc('}',f);
fputc(lastSep,f);
if (ferror(f))
    {
    perror("Error writing psl file\n");
    errAbort("\n");
    }
}

void swapOne(struct psl *psl, FILE *f)
    /* swap query and target , blockStarts = size - blockStarts  if strand direction changes */
{
static char *tName = NULL, *qName = NULL;
struct dyString *q = newDyString(16*1024);
struct dyString *t = newDyString(16*1024);
struct dyString *blockSize = newDyString(16*1024);
int blockIx, diff, x;
int qs, ts;
int lastQ = 0, lastT = 0, size;
int qOffset = 0;
int tOffset = 0;
char *s;
char qStr[512], tStr[512], bStr[512];

if (qName == NULL || !sameString(qName, psl->qName))
    {
    freez(&qName);
    qName = cloneString(psl->qName);
    if (psl->strand[0] == '-' && psl->strand[1] != '-')
	    qOffset = psl->qSize ;
    }
/* if (tName == NULL) || !sameString(tName, psl->tName))
    {*/
    freez(&tName);
    tName = cloneString(psl->tName);
    if (psl->strand[0] == '-' && psl->strand[1] != '-')
	    tOffset = psl->tSize ;
/*    }*/
    /*
if (psl->strand[0] == '-')
if (psl->strand[1] == '-')
*/
if (psl->strand[0] == '-' && psl->strand[1] != '-')
    {
    for (blockIx=(psl->blockCount)-1; blockIx >= 0; --blockIx)
        {
        sprintf(qStr, "%d,", qOffset - psl->qStarts[blockIx] - psl->blockSizes[blockIx]);
        sprintf(tStr, "%d,", tOffset - psl->tStarts[blockIx] - psl->blockSizes[blockIx]);
        dyStringAppend(q, qStr );
        dyStringAppend(t, tStr );
        sprintf(bStr, "%d,",psl->blockSizes[blockIx] );
        dyStringAppend(blockSize, bStr);
        };
        if ((qOffset - psl->qStarts[blockIx]) < 0)
            printf("qOffset %d tStart %d qStart %d strand %s q %s t %s tStarts[0] %d %d q %d %d %s\n",qOffset, psl->tStart,psl->qStart,psl->strand,q->string, t->string, psl->tStarts[0], psl->tStarts[1], psl->qStarts[0], psl->qStarts[1],qStr );
        if ((tOffset - psl->tStarts[blockIx]) < 0)
            printf("tOffset %d tStart %d qStart %d strand %s q %s t %s tStarts[0] %d %d q %d %d %s\n",tOffset, psl->tStart,psl->qStart,psl->strand,q->string, t->string, psl->tStarts[0], psl->tStarts[1], psl->qStarts[0], psl->qStarts[1], tStr);
    }
else
    {
    for (blockIx=0; blockIx < psl->blockCount; ++blockIx)
        {
        sprintf(qStr, "%d,", psl->qStarts[blockIx] );
        sprintf(tStr, "%d,", psl->tStarts[blockIx] );
        dyStringAppend(q, qStr );
        dyStringAppend(t, tStr );
        sprintf(bStr, "%d,",psl->blockSizes[blockIx] );
        dyStringAppend(blockSize, bStr);
        }
    }

x = psl->tStart; psl->tStart = psl->qStart; psl->qStart = x;
x = psl->tEnd;   psl->tEnd = psl->qEnd;     psl->qEnd = x;
x = psl->tSize;  psl->tSize = psl->qSize;   psl->qSize = x;
x = psl->tNumInsert;   psl->tNumInsert = psl->qNumInsert; psl->qNumInsert = x;
x = psl->tBaseInsert;  psl->tBaseInsert = psl->qBaseInsert; psl->qBaseInsert = x;
s = psl->tName;  psl->tName = psl->qName;   psl->qName = s;
pslWrite(psl,f,t->string,q->string, blockSize->string,'\t','\n');
dyStringFree(&blockSize);
dyStringFree(&q);
dyStringFree(&t);
    freez(&qName);
}

void pslSwap(char *pslName, char *outName)
/* pslSwap - Convert PSL to human readable output. */
{
struct hash *fileHash = newHash(0);  /* No value. */
struct hash *tHash = newHash(20);  /* seqFilePos value. */
struct hash *qHash = newHash(20);  /* seqFilePos value. */
struct dlList *fileCache = newDlList();
struct lineFile *lf = pslFileOpen(pslName);
FILE *f = mustOpen(outName, "w");
struct psl *psl;
int dotMod = dot;

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
    swapOne(psl, f);
    pslFree(&psl);
    }
lineFileClose(&lf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
static boolean axt;
optionHash(&argc, argv);
dot = optionInt("dot", dot);
doShort = !optionExists("long");
if (argc != 3)
    usage();
pslSwap(argv[1], argv[2]);
return 0;
}
