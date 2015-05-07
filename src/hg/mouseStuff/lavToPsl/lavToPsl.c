/* lavToPsl - Convert blastz lav to psl format. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "lav.h"
#include "hash.h"
#include "options.h"
#include "psl.h"
#include "bed.h"

/* strand to us for target */
char* targetStrand = "+";
boolean bed = FALSE;
char* lavScoreFile = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "lavToPsl - Convert blastz lav to psl format\n"
  "usage:\n"
  "   lavToPsl in.lav out.psl\n"
  "options:\n"
  "   -target-strand=c set the target strand to c (default is no strand)\n"
  "   -bed output bed instead of psl\n"
  "   -scoreFile=filename  output lav scores to side file, such that\n"
  "                        each psl line in out.psl is matched by a score line.\n"
  );
}

void outputBlocks(struct block *blockList, FILE *f, boolean isRc, 
	char *qName, int qSize, char *tName, int tSize, boolean psl)
/* Output block list to file. */
{
int qNumInsert = 0, qBaseInsert = 0, tNumInsert = 0, tBaseInsert = 0;
int matchOne, match = 0, mismatch = 0, bases;
double scale;
struct block *lastBlock = NULL;
int blockCount = 0;
int qTotalStart = BIGNUM, qTotalEnd = 0, tTotalStart = BIGNUM, tTotalEnd = 0;
struct block *block;

if (blockList == NULL)
    return;

/* Count up overall statistics. */
for (block = blockList; block != NULL; block = block->next)
    {
    ++blockCount;
    scale = 0.01 * block->percentId;
    bases = block->qEnd - block->qStart;
    matchOne = round(scale*bases);
    match += matchOne;
    mismatch += bases - matchOne;
    if (lastBlock != NULL)
	{
	if (block->qStart != lastBlock->qEnd)
	    {
	    qNumInsert += 1;
	    qBaseInsert += block->qStart - lastBlock->qEnd;
	    }
	if (block->tStart != lastBlock->tEnd)
	    {
	    tNumInsert += 1;
	    tBaseInsert += block->tStart - lastBlock->tEnd;
	    }
	qTotalEnd = block->qEnd;
	tTotalEnd = block->tEnd;
	}
    else
	{
	qTotalStart = block->qStart;
	qTotalEnd = block->qEnd;
	tTotalStart = block->tStart;
	tTotalEnd = block->tEnd;
	}
    lastBlock = block;
    }

int i;
struct psl *pslRecord;
AllocVar(pslRecord);
pslRecord->match = match;
pslRecord->misMatch = mismatch;
pslRecord->repMatch = 0;
pslRecord->nCount = 0;
pslRecord->qNumInsert = qNumInsert;
pslRecord->qBaseInsert = qBaseInsert;
pslRecord->tNumInsert = tNumInsert;
pslRecord->tBaseInsert = tBaseInsert;
pslRecord->strand[0] = isRc ? '-' : '+';
pslRecord->strand[1] = targetStrand[0];
pslRecord->strand[2] = 0;
pslRecord->qName = cloneString(qName);
pslRecord->qSize = qSize;
pslRecord->qStart = isRc ? (qSize - qTotalEnd) : qTotalStart;
pslRecord->qEnd = isRc ? (qSize - qTotalStart) : qTotalEnd;
pslRecord->tName = cloneString(tName);
pslRecord->tSize = tSize;
pslRecord->tStart = tTotalStart;
pslRecord->tEnd = tTotalEnd;
pslRecord->blockCount = blockCount;
AllocArray(pslRecord->blockSizes, blockCount);
AllocArray(pslRecord->qStarts, blockCount);
AllocArray(pslRecord->tStarts, blockCount);
for (block = blockList, i = 0; block != NULL; i++, block = block->next)
    pslRecord->blockSizes[i] = block->tEnd - block->tStart;
for (block = blockList, i = 0; block != NULL; i++, block = block->next)
    pslRecord->qStarts[i] = block->qStart;
for (block = blockList, i = 0; block != NULL; i++, block = block->next)
    pslRecord->tStarts[i] = block->tStart;

if (psl)
    {
    pslTabOut(pslRecord, f);
    }
else  /* Output bed line. */
    {
    struct bed *bed = bedFromPsl(pslRecord);
    bedTabOutN(bed, 12, f);
    bedFree(&bed);
    }
pslFree(&pslRecord);
}

struct block *parseA(struct lineFile *lf, FILE* ff)
/* Parse an alignment stanza into a block list. */
{
struct block *block = NULL, *blockList = NULL;
char *line, *words[6];
int score = 0;
int wordCount,lavScore;

while (lineFileNext(lf, &line, NULL))
    {
    if (line[0] == '#')
	continue;
    if (line[0] == '}')
	break;
    wordCount = chopLine(line, words);
    if (wordCount == 0)
	continue;
    if (words[0][0] == 's')
        {
	lineFileExpectWords(lf, 2, wordCount) ;
	score  = lineFileNeedNum(lf, words, 1) - 1 ;
        }
    if (words[0][0] == 'l')
	{
	AllocVar(block);
	lineFileExpectWords(lf, 6, wordCount);
        block->score = score;
	block->tStart = lineFileNeedNum(lf, words, 1) - 1;
	block->tEnd = lineFileNeedNum(lf, words, 3);
	block->qStart = lineFileNeedNum(lf, words, 2) - 1;
	block->qEnd = lineFileNeedNum(lf, words, 4);
	if (block->qEnd - block->qStart != block->tEnd - block->tStart)
	    errAbort("Block size mismatch line %d of %s", lf->lineIx, lf->fileName);
	block->percentId = lineFileNeedNum(lf, words, 5);
        if ((block->qEnd == block->qStart) && (block->tEnd == block->tStart))
            {
            verbose(2, "# length zero block at line %d: t %d-%d q %d-%d\n", lf->lineIx, block->tEnd,block->tStart,block->qEnd,block->qStart);
            freeMem(block);    // ignore zero length records
            }
        else
	    slAddHead(&blockList, block);
	}
    if ((ff!=NULL) && (words[0][0] == 's'))
       {
       lavScore = lineFileNeedNum(lf, words, 1);
       fprintf(ff,"%d\n", lavScore);
       }
    }
slReverse(&blockList);
blockList = removeFrayedEnds(blockList);
return blockList;
}

void parseIntoPsl(char *lavFile, FILE *f, FILE* ff)
/* Parse a blastz lav file and put it into something .psl like. */
{
struct lineFile *lf = lineFileOpen(lavFile, TRUE);
char *line;
struct block *blockList = NULL;
boolean isRc = FALSE;
char *tName = NULL, *qName = NULL;
char *matrix = NULL, *command = NULL;
int qSize = 0, tSize = 0;

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
	blockList = parseA(lf,ff);
        bed = optionExists("bed");
	outputBlocks(blockList, f, isRc, qName, qSize, tName, tSize, !bed);
	slFreeList(&blockList);
	}
    }
lineFileClose(&lf);
}


void lavToPsl(char *lavIn, char *pslOut)
/* lavToPsl - Convert blastz lav to psl format. */
{
FILE *f = mustOpen(pslOut, "w");
FILE *ff = NULL;


if (lavScoreFile!=NULL)
    ff = mustOpen(lavScoreFile, "w");

parseIntoPsl(lavIn, f, ff);
carefulClose(&f);

if (lavScoreFile!=NULL)
    carefulClose(&ff);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3)
    usage();
targetStrand = optionVal("target-strand", "");
lavScoreFile = optionVal("scoreFile",lavScoreFile);
lavToPsl(argv[1], argv[2]);
return 0;
}
