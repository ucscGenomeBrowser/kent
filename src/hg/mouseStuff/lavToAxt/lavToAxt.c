/* lavToAxt - Convert blastz lav file to an axt file (which includes sequence). */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dlist.h"
#include "dystring.h"
#include "dnaseq.h"
#include "nib.h"
#include "axt.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "lavToAxt - Convert blastz lav file to an axt file (which includes sequence)\n"
  "usage:\n"
  "   lavToAxt in.lav tNibDir qNibDir out.axt\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

struct block
/* A block of an alignment. */
    {
    struct block *next;
    int qStart, qEnd;	/* Query position. */
    int tStart, tEnd;	/* Target position. */
    int percentId;	/* Percentage identity. */
    };

struct cachedNib
/* File in cache. */
    {
    struct cashedNib *next;	/* next in list. */
    char *name;		/* Sequence name (allocated here) */
    char *fileName;     /* File name (allocated here) */
    FILE *f;		/* Open file. */
    int size;		/* Size of sequence. */
    };

struct cachedNib *openFromCache(struct dlList *cache, char *dirName, char *seqName)
/* Return open file handle via cache.  */
{
static int maxCacheSize=32;
int cacheSize = 0;
struct dlNode *node;
struct cachedNib *cn;
char fileName[512];

/* First loop through trying to find it in cache, counting
 * cache size as we go. */
for (node = cache->head; !dlEnd(node); node = node->next)
    {
    ++cacheSize;
    cn = node->val;
    if (sameString(seqName, cn->name))
        {
	dlRemove(node);
	dlAddHead(cache, node);
	return cn;
	}
    }

/* If cache has reached max size free least recently used. */
if (cacheSize >= maxCacheSize)
    {
    node = dlPopTail(cache);
    cn = node->val;
    carefulClose(&cn->f);
    freeMem(cn->fileName);
    freeMem(cn->name);
    freeMem(cn);
    freeMem(node);
    }

/* Cache new file. */
AllocVar(cn);
cn->name = cloneString(seqName);
snprintf(fileName, sizeof(fileName), "%s/%s.nib", dirName, seqName);
cn->fileName = cloneString(fileName);
nibOpenVerify(fileName, &cn->f, &cn->size);
dlAddValHead(cache, cn);
return cn;
}

struct dnaSeq *readFromCache(struct dlList *cache, char *dirName, char *seqName,
	int start, int size, int seqSize)
/* Return dnaSeq read from the appropriate nib file.  
 * You need to dnaSeqFree this when done (it is the nib
 * file that is cached, not the sequence). */
{
struct cachedNib *cn = openFromCache(cache, dirName, seqName);
if (seqSize != cn->size)
    errAbort("%s/%s is %d bases in .lav file and %d in .nib file\n",
       dirName, seqName, seqSize, cn->size); 
return nibLdPartMasked(NIB_MASK_MIXED, cn->fileName, cn->f, cn->size, start, size);
}

void outputBlocks(struct lineFile *lf,
	struct block *blockList, int score, FILE *f, boolean isRc, 
	char *qName, int qSize, char *qNibDir, struct dlList *qCache,
	char *tName, int tSize, char *tNibDir, struct dlList *tCache)
/* Output block list as an axt to file f. */
{
int qStart = BIGNUM, qEnd = 0, tStart = BIGNUM, tEnd = 0;
struct block *lastBlock = NULL;
struct block *block;
struct dyString *qSym = newDyString(16*1024);
struct dyString *tSym = newDyString(16*1024);
struct dnaSeq *qSeq, *tSeq;
struct axt axt;

static int ix = 0;

if (blockList == NULL)
    return;

/* Figure overall dimensions. */
for (block = blockList; block != NULL; block = block->next)
    {
    if (qStart > block->qStart) qStart = block->qStart;
    if (qEnd < block->qEnd) qEnd = block->qEnd;
    if (tStart > block->tStart) tStart = block->tStart;
    if (tEnd < block->tEnd) tEnd = block->tEnd;
    }

/* Load sequence covering alignment from nib files. */
if (isRc)
    {
    reverseIntRange(&qStart, &qEnd, qSize);
    qSeq = readFromCache(qCache, qNibDir, qName, qStart, qEnd - qStart, qSize);
    reverseIntRange(&qStart, &qEnd, qSize);
    reverseComplement(qSeq->dna, qSeq->size);
    }
else
    qSeq = readFromCache(qCache, qNibDir, qName, qStart, qEnd - qStart, qSize);
tSeq = readFromCache(tCache, tNibDir, tName, tStart, tEnd - tStart, tSize);

/* Loop through blocks copying sequence into dynamic strings. */
for (block = blockList; block != NULL; block = block->next)
    {
    if (lastBlock != NULL)
        {
	int qGap = block->qStart - lastBlock->qEnd;
	int tGap = block->tStart - lastBlock->tEnd;
	if (qGap != 0 && tGap != 0)
	    {
	    errAbort("Gaps in both strand on alignment ending line %d of %s",
	    	lf->lineIx, lf->fileName);
	    }
	if (qGap > 0)
	    {
	    dyStringAppendMultiC(tSym, '-', qGap);
	    dyStringAppendN(qSym, qSeq->dna + lastBlock->qEnd - qStart, qGap);
	    }
	if (tGap > 0)
	    {
	    dyStringAppendMultiC(qSym, '-', tGap);
	    dyStringAppendN(tSym, tSeq->dna + lastBlock->tEnd - tStart, tGap);
	    }
	}
    dyStringAppendN(qSym, qSeq->dna + block->qStart - qStart,
    	block->qEnd - block->qStart);
    dyStringAppendN(tSym, tSeq->dna + block->tStart - tStart,
    	block->tEnd - block->tStart);
    lastBlock = block;
    }
if (qSym->stringSize != tSym->stringSize)
    errAbort("qSize and tSize don't agree in alignment ending line %d of %s",
	    lf->lineIx, lf->fileName);

/* Fill in an axt and write it to output. */
ZeroVar(&axt);
axt.qName = qName;
axt.qStart = qStart;
axt.qEnd = qEnd;
axt.qStrand = (isRc ? '-' : '+');
axt.tName = tName;
axt.tStart = tStart;
axt.tEnd = tEnd;
axt.tStrand = '+';
axt.score = score;
axt.symCount = qSym->stringSize;
axt.qSym = qSym->string;
axt.tSym = tSym->string;
axtWrite(&axt, f);

/* Clean up. */
freeDnaSeq(&qSeq);
freeDnaSeq(&tSeq);
dyStringFree(&qSym);
dyStringFree(&tSym);
}

void unexpectedEof(struct lineFile *lf)
/* Squawk about unexpected end of file. */
{
errAbort("Unexpected end of file in %s", lf->fileName);
}

void seekEndOfStanza(struct lineFile *lf)
/* find end of stanza */
{
char *line;
for (;;)
    {
    if (!lineFileNext(lf, &line, NULL))
        unexpectedEof(lf);
    if (line[0] == '}')
        break;
    }
}

void parseS(struct lineFile *lf, int *tSize, int *qSize)
/* Parse s stanza and return tSize and qSize */
{
char *line, *words[3];
if (!lineFileRow(lf, words))
    unexpectedEof(lf);
*tSize = lineFileNeedNum(lf, words, 2);
if (!lineFileRow(lf, words))
    unexpectedEof(lf);
*qSize = lineFileNeedNum(lf, words, 2);
seekEndOfStanza(lf);
}

char *needNextWord(struct lineFile *lf, char **pLine)
/* Get next word from line or die trying. */
{
char *word = nextWord(pLine);
if (word == NULL)
   errAbort("Short line %d of %s\n", lf->lineIx, lf->fileName);
return word;
}

char *justChrom(char *s)
/* Simplify mongo nib file thing in axt. */
{
char *e = stringIn(".nib:", s);
if (e == NULL)
    return s;
*e = 0;
e = strrchr(s, '/');
if (e == NULL)
    return s;
else
    return e+1;
}


void parseH(struct lineFile *lf,  char **tName, char **qName, boolean *isRc)
/* Parse out H stanza */
{
char *line, *word, *e, *dupe;
int i;


/* Set return variables to default values. */
freez(qName);
freez(tName);
*isRc = FALSE;

for (i=0; ; ++i)
    {
    if (!lineFileNext(lf, &line, NULL))
       unexpectedEof(lf);
    if (line[0] == '#')
       continue;
    if (line[0] == '}')
       {
       if (i < 2)
	   errAbort("Short H stanza line %d of %s", lf->lineIx, lf->fileName);
       break;
       }
    word = needNextWord(lf, &line);
    word += 2;  /* Skip over "> */
    e = strchr(word, '"');
    if (e != NULL) 
        {
	*e = 0;
	if (line != NULL)
	    ++line;
	}
    if (i == 0)
        *tName = cloneString(justChrom(word));
    else if (i == 1)
        *qName = cloneString(justChrom(word));
    if ((line != NULL) && (stringIn("(reverse", line) != NULL))
        *isRc = TRUE;
    }
}

struct block *removeFrayedEnds(struct block *blockList)
/* Remove zero length blocks at start and/or end to 
 * work around blastz bug. */
{
struct block *block = blockList;
struct block *lastBlock = NULL;
if (block != NULL && block->qStart == block->qEnd)
    {
    blockList = blockList->next;
    freeMem(block);
    }
for (block = blockList; block != NULL; block = block->next)
    {
    if (block->next == NULL)  /* Last block in list */
        {
	if (block->qStart == block->qEnd)
	    {
	    freeMem(block);
	    if (lastBlock == NULL)  /* Only block on list. */
		blockList = NULL;
	    else
	        lastBlock->next = NULL;
	    }
	}
    lastBlock = block->next;
    }
return blockList;
}

void parseA(struct lineFile *lf, struct block **retBlockList, 
	int *retScore)
/* Parse an alignment stanza into a block list. */
{
struct block *block, *blockList = NULL;
char *line, *words[6], typeChar;
int wordCount;
int score = -666;
boolean gotScore = FALSE;

while (lineFileNext(lf, &line, NULL))
    {
    if (line[0] == '#')
	continue;
    if (line[0] == '}')
	break;
    wordCount = chopLine(line, words);
    if (wordCount == 0)
	continue;
    typeChar = words[0][0];
    if (typeChar == 'l')
	{
	lineFileExpectWords(lf, 6, wordCount);
	AllocVar(block);
	block->tStart = lineFileNeedNum(lf, words, 1) - 1;
	block->tEnd = lineFileNeedNum(lf, words, 3);
	block->qStart = lineFileNeedNum(lf, words, 2) - 1;
	block->qEnd = lineFileNeedNum(lf, words, 4);
	if (block->qEnd - block->qStart != block->tEnd - block->tStart)
	    errAbort("Block size mismatch line %d of %s", lf->lineIx, lf->fileName);
	block->percentId = lineFileNeedNum(lf, words, 5);
	slAddHead(&blockList, block);
	}
    else if (typeChar == 's')
        {
	gotScore = TRUE;
	score = lineFileNeedNum(lf, words, 1);
	}
    }
if (!gotScore)
    {
    errAbort("'a' stanza missing score line %d of %s", 
    	lf->lineIx, lf->fileName);
    }
slReverse(&blockList);
blockList = removeFrayedEnds(blockList);
*retBlockList = blockList;
*retScore = score;
}

void parseIntoAxt(char *lavFile, FILE *f, 
	char *tNibDir, struct dlList *tCache, 
	char *qNibDir, struct dlList *qCache)
/* Parse a blastz lav file and put it an axt. */
{
struct lineFile *lf = lineFileOpen(lavFile, TRUE);
char *line;
struct block *blockList = NULL;
boolean isRc = FALSE;
char *tName = NULL, *qName = NULL;
int qSize = 0, tSize = 0;
int score = 0;

/* Check header. */
if (!lineFileNext(lf, &line, NULL))
   errAbort("%s is empty", lf->fileName);
if (!startsWith("#:lav", line))
   errAbort("%s is not a lav file\n", lf->fileName);

while (lineFileNext(lf, &line, NULL))
    {
    if (startsWith("s {", line))
        {
	parseS(lf, &tSize, &qSize);
	}
    else if (startsWith("h {", line))
        {
	parseH(lf, &tName, &qName, &isRc);
	}
    else if (startsWith("a {", line))
        {
	parseA(lf, &blockList, &score);
	outputBlocks(lf, blockList, score, f, isRc, 
		qName, qSize, qNibDir, qCache,
		tName, tSize, tNibDir, tCache);
	slFreeList(&blockList);
	}
    }
lineFileClose(&lf);
}


void lavToAxt(char *inLav, char *tNibDir, char *qNibDir, char *outAxt)
/* lavToAxt - Convert blastz lav file to an axt file (which includes sequence). */
{
FILE *f = mustOpen(outAxt, "w");
struct dlList *qCache = newDlList(), *tCache = newDlList();
parseIntoAxt(inLav, f, tNibDir, tCache, qNibDir, qCache);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 5)
    usage();
lavToAxt(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
