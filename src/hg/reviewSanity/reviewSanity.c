/* reviewSanity - Look through sanity files and make sure things are ok.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "portable.h"
#include "obscure.h"
#include "fa.h"
#include "psl.h"

static char const rcsid[] = "$Id: reviewSanity.c,v 1.3 2006/08/17 15:45:51 angie Exp $";

FILE *missLog;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "reviewSanity - Look through sanity files and make sure things are ok.\n"
  "usage:\n"
  "   reviewSanity contigDir(s)\n");
}

static int faFastBufSize = 0;
static DNA *faFastBuf;

static void expandFaFastBuf(int bufPos)
/* Make faFastBuf bigger. */
{
if (faFastBufSize == 0)
    {
    faFastBufSize = 64 * 1024;
    faFastBuf = needLargeMem(faFastBufSize);
    }
else
    {
    DNA *newBuf;
    int newBufSize = faFastBufSize + faFastBufSize;
    newBuf = needLargeMem(newBufSize);
    memcpy(newBuf, faFastBuf, bufPos);
    freeMem(faFastBuf);
    faFastBuf = newBuf;
    faFastBufSize = newBufSize;
    }
}


boolean faSpeedReadNextKeepCase(struct lineFile *lf, 
	DNA **retDna, int *retSize, char **retName)
/* Read in next FA entry as fast as we can. Faster than that old,
 * pokey faFastReadNext. Return FALSE at EOF. 
 * The returned DNA and name will be overwritten by the next call
 * to this function. */
{
int c;
int bufIx = 0;
static char name[256];
int lineSize, i;
char *line;

dnaUtilOpen();

/* Read first line, make sure it starts wiht '>', and read first word
 * as name of sequence. */
name[0] = 0;
if (!lineFileNext(lf, &line, &lineSize))
    {
    *retDna = NULL;
    *retSize = 0;
    return FALSE;
    }
if (line[0] == '>')
    {
    line = firstWordInLine(skipLeadingSpaces(line+1));
    if (line == NULL)
        errAbort("Expecting sequence name after '>' line %d of %s", lf->lineIx, lf->fileName);
    strncpy(name, line, sizeof(name));
    }
else
    {
    errAbort("Expecting '>' line %d of %s", lf->lineIx, lf->fileName);
    }
/* Read until next '>' */
for (;;)
    {
    if (!lineFileNext(lf, &line, &lineSize))
        break;
    if (line[0] == '>')
        {
	lineFileReuse(lf);
	break;
	}
    if (bufIx + lineSize >= faFastBufSize)
	expandFaFastBuf(bufIx);
    for (i=0; i<lineSize; ++i)
        {
	c = line[i];
	if (isalpha(c))
	    {
	    faFastBuf[bufIx++] = c;
	    }
	}
    }
if (bufIx >= faFastBufSize)
    expandFaFastBuf(bufIx);
faFastBuf[bufIx] = 0;
*retDna = faFastBuf;
*retSize = bufIx;
*retName = name;
return TRUE;
}


#define blockSize 1020
#define minUniq (blockSize/4)

int thresholds[10];     /* Thresholds for hit size for binning. */
int hitCount[10];	/* Hit counts in bins of 100. */
int perfectCount = 0;   /* Blocks that align perfectly. */
int missCount = 0;      /* Blocks that don't align. */
int dupeCount = 0;	/* Blocks that align well two or more places. */
int blockCount = 0;	/* Total number of blocks. */
int weakAliCount = 0;	/* Number of alignments that are too divergent. */
int repMaskedCount = 0; /* The number of blocks that are too repeat masked. */

struct block
/* Info on one block. */
    {
    struct block *next;
    char *name;		/* Allocated in hash. */
    int size;
    short hitCount[10];		/* Hit counts in bins of 100. */
    short perfectCount;		/* 100% hits. */
    boolean maskedOut;		/* True if masked out. */
    };

int ucCount(DNA *dna, int size)
/* Count upper case (non-repeat) DNA. */
{
int i, count = 0;
DNA b;

for (i=0; i<size; ++i)
    {
    b = dna[i];
    if (b == 'A' || b == 'C' || b == 'G' || b == 'T')
        ++count;
    }
return count;
}

void readBlockInfo(char *listFile, 
	struct hash *blockHash, struct block **pBlockList)
/* Read in blocks from all files listed in listFile. */
{
char *fileNamesBuf = NULL, **fileNames, *dnaName;
int fileCount, i, dnaSize;
struct lineFile *lf = NULL;
DNA *dna;
struct block *block;


readAllWords(listFile, &fileNames, &fileCount, &fileNamesBuf);
for (i=0; i<fileCount; ++i)
    {
    lf = lineFileOpen(fileNames[i], TRUE);
    while (faSpeedReadNextKeepCase(lf, &dna, &dnaSize, &dnaName))
        {
	if (dnaSize != blockSize)
	    {
	    warn("%s is %d bases not %d file %s\n",
	    	dnaName, dnaSize, blockSize, lf->fileName);
	    }
	AllocVar(block);
#ifdef SOON
	block->maskedOut = (ucCount(dna, dnaSize) <= minUniq);
#endif
	slAddHead(pBlockList, block);
	hashAddSaveName(blockHash, dnaName, block, &block->name);
	block->size = dnaSize;
	}
    lineFileClose(&lf);
    }
slReverse(pBlockList);
freez(&fileNamesBuf);
}

void reviewOne(char *dir)
/* Review sanity files in one contig dir. */
{
char fileName[512];
struct block *blockList = NULL, *block;
struct hash *blockHash = newHash(16);
struct psl *psl;
struct lineFile *lf;
int i, aliSize;

sprintf(fileName, "%s/break.lst", dir);
if (!fileExists(fileName))
    return;
readBlockInfo(fileName, blockHash, &blockList);
if (blockList == NULL)
    return;

sprintf(fileName, "%s/sanity.psl", dir);
lf = lineFileOpen(fileName, TRUE);
while ((psl = pslNext(lf)) != NULL)
    {
    if (pslCalcMilliBad(psl, FALSE) < 20)
	{
	aliSize = psl->match + psl->repMatch;
	block = hashMustFindVal(blockHash, psl->qName);
	for (i=0; i<10; ++i)
	    {
	    if (aliSize <= thresholds[i+1])
		{
		++block->hitCount[i];
		break;
		}
	    }
	if (aliSize >= blockSize-2)
	    ++block->perfectCount;
	}
    else
        ++weakAliCount;
    pslFree(&psl);
    }
lineFileClose(&lf);

/* Loop through list gathering statistics on how blocks
 * hit genome. */
for (block = blockList; block != NULL; block = block->next)
    {
    int numBest = 0;
    int numGood = 0;
    if (block->maskedOut)
        ++repMaskedCount;
    else
	{
	++blockCount;
	if (block->perfectCount)
	    ++perfectCount;
	for (i=9; i >= 0; --i)
	    {
	    if ((numBest = block->hitCount[i]) > 0)
		break;
	    }
	if (numBest == 0)
	    {
	    ++missCount;
	    fprintf(missLog, "%s\n", block->name);
	    }
	else
	    ++hitCount[i];
	for (i=7; i<10; ++i)
	    numGood += block->hitCount[i];
	if (numGood > 1)
	    ++dupeCount;
	}
    }
freeHash(&blockHash);
slFreeList(&blockList);
}

double percent(double p, double q)
/* Return p/q as a percent. */
{
return 100.0 * p / q;
}

void reviewSanity(int dirCount, char *dirs[])
/* reviewSanity - Look through sanity files and make sure things are ok.. */
{
int i;

missLog = mustOpen("miss.log", "w");
for (i=0; i<=10; ++i)
    thresholds[i] = round((double)i*blockSize/10.0);
for (i=0; i<dirCount; ++i)
    reviewOne(dirs[i]);
printf("%d blocks, %d duplicates (%4.2f%%)\n",
	blockCount, dupeCount, percent(dupeCount, blockCount));
#ifdef OLD
printf("%d perfect (%4.2f%%)\n", perfectCount, 
	percent(perfectCount, blockCount));
printf("%d repeat masked out blocks\n", repMaskedCount);
#endif /* OLD */
printf("\n");
printf("Ali Size\tNumber\tPercent\n");
printf("-------------------------\n");
printf("   none \t%d\t%4.2f%%\n", 
    missCount, percent(missCount, blockCount));
for (i=0; i<10; ++i)
    {
    printf(" %3d-%d\t%d\t%4.2f%%\n", thresholds[i]+1, thresholds[i+1],
    	hitCount[i], percent(hitCount[i], blockCount));
    }
}


int main(int argc, char *argv[])
/* Process command line. */
{
if (argc < 2)
    usage();
reviewSanity(argc-1, argv+1);
return 0;
}
