/* lavToAxt - Convert blastz lav file to an axt file (which includes sequence). */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "lav.h"
#include "hash.h"
#include "options.h"
#include "dlist.h"
#include "dystring.h"
#include "dnaseq.h"
#include "nib.h"
#include "twoBit.h"
#include "fa.h"

struct dnaSeq *qFaList;
struct dnaSeq *tFaList;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "lavToAxt - Convert blastz lav file to an axt file (which includes sequence)\n"
  "usage:\n"
  "   lavToAxt in.lav tNibDir qNibDir out.axt\n"
  "Where tNibDir/qNibDir are either directories full of nib files, or a single\n"
  "twoBit file\n"
  "options:\n"
  "   -fa  qNibDir is interpreted as a fasta file of multiple dna seq instead of directory of nibs\n"
  "   -tfa tNibDir is interpreted as a fasta file of multiple dna seq instead of directory of nibs\n"
  "   -dropSelf  drops alignment blocks on the diagonal for self alignments\n"
  "   -scoreScheme=fileName Read the scoring matrix from a blastz-format file.\n"
  "                (only used in conjunction with -dropSelf, to rescore \n"
  "                alignments when blocks are dropped)\n"
  );
}

boolean qIsFa = FALSE;	/* Corresponds to -fa flag. */
boolean tIsFa = FALSE;	/* Corresponds to -tfa flag. */
boolean dropSelf = FALSE;	/* Corresponds to -dropSelf flag. */
struct axtScoreScheme *scoreScheme = NULL;  /* -scoreScheme flag. */

struct cachedSeqFile
/* File in cache. */
    {
    struct cachedSeqFile *next;	/* next in list. */
    char *fileName;     /* File name (allocated here) */
    struct twoBitFile *tbf;	/* Two bit file if any. */
    char *name;		/* Sequence name (allocated here) for nib file */
    FILE *f;		/* Open nib file. */
    int size;		/* Size of sequence. */
    };

void cachedSeqFileFree(struct cachedSeqFile **pCn)
/* Free up resources associated with cached sequence file. */
{
struct cachedSeqFile *cn = *pCn;
if (cn != NULL)
    {
    carefulClose(&cn->f);
    twoBitClose(&cn->tbf);
    freeMem(cn->fileName);
    freeMem(cn->name);
    freez(pCn);
    }
}

struct cachedSeqFile *openNibFromCache(struct dlList *cache, char *dirName, char *seqName)
/* Return open file handle via cache.  */
{
static int maxCacheSize=32;
int cacheSize = 0;
struct dlNode *node;
struct cachedSeqFile *cn;
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
    cachedSeqFileFree(&cn);
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

struct cachedSeqFile *openTwoBitFromCache(struct dlList *cache, char *fileName)
/* Return open file handle via cache.  In this case it's just a cache of one.  */
{
struct cachedSeqFile *cn;
if (dlEmpty(cache))
    {
    AllocVar(cn);
    cn->fileName = cloneString(fileName);
    cn->tbf = twoBitOpen(fileName);
    dlAddValHead(cache, cn);
    }
else
    cn = cache->head->val;
return cn;
}

struct cachedSeqFile *openFromCache(struct dlList *cache, char *dirName, char *seqName, 
	boolean isTwoBit)
/* Return open file handle via cache.  */
{
if (isTwoBit)
    return openTwoBitFromCache(cache, dirName);
else
    return openNibFromCache(cache, dirName, seqName);
}

struct dnaSeq *readFromCache(struct dlList *cache, char *dirName, char *seqName,
	int start, int size, int seqSize, boolean isTwoBit)
/* Return dnaSeq read from the appropriate nib file.  
 * You need to dnaSeqFree this when done (it is the nib
 * file that is cached, not the sequence). */
{
struct cachedSeqFile *cn = openFromCache(cache, dirName, seqName, isTwoBit);
if (isTwoBit)
    {
    return twoBitReadSeqFrag(cn->tbf, seqName, start, start+size);
    }
else
    {
    if (seqSize != cn->size)
	errAbort("%s/%s is %d bases in .lav file and %d in .nib file\n",
	   dirName, seqName, seqSize, cn->size); 
    if ((start+size) > cn->size )
	printf("%s/%s is %d bases in .lav file and %d in .nib file start %d size %d end %d\n",
	   dirName, seqName, seqSize, cn->size, start, size, start+size); 
    return nibLdPartMasked(NIB_MASK_MIXED, cn->fileName, cn->f, cn->size, start, size);
    }
}

void outputBlocks(struct lineFile *lf,
	struct block *blockList, int score, FILE *f, boolean isRc, 
	char *qName, int qSize, char *qNibDir, struct dlList *qCache,
	char *tName, int tSize, char *tNibDir, struct dlList *tCache,
	boolean rescore)
/* Output block list as an axt to file f. */
{
int qStart = BIGNUM, qEnd = 0, tStart = BIGNUM, tEnd = 0;
struct block *lastBlock = NULL;
struct block *block;
struct dyString *qSym = newDyString(16*1024);
struct dyString *tSym = newDyString(16*1024);
struct dnaSeq *qSeq = NULL, *tSeq = NULL, *seq = NULL;
struct axt axt;
boolean qIsTwoBit = twoBitIsFile(qNibDir);
boolean tIsTwoBit = twoBitIsFile(tNibDir);

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
    if (qIsFa)
        {
        for (seq = qFaList ; seq != NULL ; seq = seq->next)
            if (sameString(qName, seq->name))
                break;
        if (seq != NULL)
            {
            AllocVar(qSeq);
            qSeq->size = qEnd - qStart;
            qSeq->name = cloneString(qName);
            qSeq->dna = cloneMem((seq->dna)+qStart, qSeq->size);
            }
        else
            errAbort("sequence not found %s\n",qName);
        }
    else
        qSeq = readFromCache(qCache, qNibDir, qName, qStart, qEnd - qStart, qSize, qIsTwoBit);
    reverseIntRange(&qStart, &qEnd, qSize);
    reverseComplement(qSeq->dna, qSeq->size);
    }
else
    {    
    if (qIsFa)
        {
        for (seq = qFaList ; seq != NULL ; seq = seq->next)
	    {
            if (sameString(qName, seq->name))
                break;
	    }
	if (seq != NULL)
	    {
	    AllocVar(qSeq);
	    qSeq->size = qEnd - qStart;
	    qSeq->name = cloneString(qName);
	    qSeq->dna = (seq->dna)+qStart;
	    }
	else
	    errAbort("sequence not found %s\n",qName);
        }
    else
        qSeq = readFromCache(qCache, qNibDir, qName, qStart, qEnd - qStart, qSize, qIsTwoBit);
    }
    if (tIsFa)
        {
        for (seq = tFaList ; seq != NULL ; seq = seq->next)
            if (sameString(tName, seq->name))
                break;
        if (seq != NULL)
            {
            AllocVar(tSeq);
            tSeq->size = tEnd - tStart;
            tSeq->name = cloneString(tName);
            tSeq->dna = cloneMem((seq->dna)+tStart, tSeq->size);
            }
        else
            errAbort("sequence not found %s\n",tName);
        }
    else
        tSeq = readFromCache(tCache, tNibDir, tName, tStart, tEnd - tStart, tSize, tIsTwoBit);

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
    if (qSeq->size < block->qStart - qStart)
        {
        errAbort("read past end of sequence %s size =%d block->qStart-qstart=%d block->qStart=%d qEnd=%d \n", qName, qSeq->size, block->qStart-qStart,block->qStart, block->qEnd );
        }
    dyStringAppendN(qSym, qSeq->dna + block->qStart - qStart,
    	block->qEnd - block->qStart);
    if (tSeq->size < block->tStart - tStart)
        {
        errAbort("read past end of sequence %s size =%d block->tStart-tstart=%d\n", tName, tSeq->size, block->tStart-tStart);
        }
    dyStringAppendN(tSym, tSeq->dna + block->tStart - tStart,
    	block->tEnd - block->tStart);
    lastBlock = block;
    }
if (qSym->stringSize != tSym->stringSize)
    errAbort("qSize and tSize don't agree in alignment ending line %d of %s",
	    lf->lineIx, lf->fileName);

if (rescore)
    score = axtScoreSym(scoreScheme, qSym->stringSize,
			qSym->string, tSym->string);

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
if (!qIsFa)
    freeDnaSeq(&qSeq);
freeDnaSeq(&tSeq);
dyStringFree(&qSym);
dyStringFree(&tSym);
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

static boolean breakUpIfOnDiagonal(struct block *blockList, boolean isRc,
	char *qName, char *tName, int qSize, int tSize,
	struct block *retBlockLists[], int maxBlockLists, int *retCount) 
/* If any blocks are on diagonal, remove the blocks and separate the lists 
 * of blocks before and after the diagonal. Store block list pointers in 
 * retBlockLists, the number of lists in retCount, and return TRUE if 
 * we found any blocks on diagonal so we know to rescore afterwards. */
{
int blockListIndex = 0;
boolean brokenUp = FALSE;

retBlockLists[blockListIndex] = blockList;
if (sameString(qName, tName))
    {
    struct block *block = NULL, *lastBlock = NULL;
    int i = 0;
    for (block = blockList;  block != NULL;  block = block->next)
	{
	int qStart = block->qStart;
	int qEnd   = block->qEnd;
	if (lastBlock != NULL && block == retBlockLists[blockListIndex])
	    freez(&lastBlock);
	if (isRc)
	    reverseIntRange(&qStart, &qEnd, qSize);
	if (rangeIntersection(block->tStart, block->tEnd, qStart, qEnd) > 0)
	    {
	    brokenUp = TRUE;
	    if (block != retBlockLists[blockListIndex])
		{
		assert(lastBlock != NULL);
		lastBlock->next = NULL;
		blockListIndex++;
		if (blockListIndex >= maxBlockLists)
		    errAbort("breakUpIfOnDiagonal: Too many fragmented block lists!");
		}
	    retBlockLists[blockListIndex] = block->next;
	    }
	lastBlock = block;
	}
    if (retBlockLists[blockListIndex] == NULL)
	{
	blockListIndex--;
	if (lastBlock != NULL)
	    freez(&lastBlock);
	}
    for (i=0;  i <= blockListIndex; i++)
	{
	retBlockLists[i] = removeFrayedEnds(retBlockLists[i]);
	}
    }
*retCount = blockListIndex + 1;
return brokenUp;
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
char *matrix = NULL, *command = NULL;
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
    else if (startsWith("d {", line))
        {
	parseD(lf, &matrix, &command, f);
	}
    else if (startsWith("a {", line))
        {
	parseA(lf, &blockList, &score);
        if (optionExists("dropSelf"))
	    {
	    struct block *bArr[256];
	    int numBLs = 0, i = 0;
	    boolean rescore = FALSE;
	    rescore = breakUpIfOnDiagonal(blockList, isRc, qName, tName,
					  qSize, tSize, bArr, ArraySize(bArr),
					  &numBLs);
	    for (i=0;  i < numBLs;  i++)
		{
		outputBlocks(lf, bArr[i], score, f, isRc, 
			     qName, qSize, qNibDir, qCache,
			     tName, tSize, tNibDir, tCache, rescore);
		slFreeList(&bArr[i]);
		}
	    }
	else
	    {
	    outputBlocks(lf, blockList, score, f, isRc, 
			 qName, qSize, qNibDir, qCache,
			 tName, tSize, tNibDir, tCache, FALSE);
	    slFreeList(&blockList);
	    }
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
char *scoreSchemeFile = NULL;
optionHash(&argc, argv);
if (argc != 5)
    usage();
if (optionExists("fa"))
    {
    qIsFa = TRUE;
    qFaList = faReadAllMixed(argv[3]);
    }
if (optionExists("tfa"))
    {
    tIsFa = TRUE;
    tFaList = faReadAllMixed(argv[2]);
    }
scoreSchemeFile = optionVal("scoreScheme", scoreSchemeFile);
if (scoreSchemeFile != NULL)
    scoreScheme = axtScoreSchemeRead(scoreSchemeFile);
else
    scoreScheme = axtScoreSchemeDefault();
lavToAxt(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
