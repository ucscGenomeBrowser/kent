/* sim4big - A wrapper for Sim4 that runs it repeatedly on a multi-sequence .fa file. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "bed.h"
#include "dnaseq.h"
#include "fa.h"


char *exePath = "sim4";	/*  executable name. */
char *tempDir = NULL;	/* Temporary dir. */
boolean pslOut;		/* PSL format output. */
int dotEvery = 1;		/* Output dots every so often. */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "sim4big - A wrapper for Sim4 that runs it repeatedly on a multi-sequence .fa file\n"
  "usage:\n"
  "   sim4big genomic.fa mrna.fa output.bed\n"
  "options:\n"
  "   -tempDir=someDir\n"
  "   -exe=/path/to/sim4\n"
  "   -psl  output is .psl instead of bed format\n"
  "   -dots=N Output a dot every N sequences aligned\n"
  );
}

void tempFile(char *dir, char *root, char *suffix, char result[512])
/* Make a temporary file name. */
{
if (dir == NULL)
    sprintf(result, "%sXXXXXX%s", root, suffix);
else
    sprintf(result, "%s/%sXXXXXX%s", dir, root, suffix);
if (mkstemp(result) < 0)
    errAbort("tempFile: mkstemp failed: %s", strerror(errno));
}

void sim4BadLine(struct lineFile *lf)
/* Complain about bad line in sim4 file. */
{
errAbort("Can't parse line %d of %s.  Not usual sim4 format.",
	lf->lineIx, lf->fileName);
}

struct block
/* A block of an alignment. */
    {
    struct block *next;
    int qStart, qEnd;	/* Query position. */
    int tStart, tEnd;	/* Target position. */
    int percentId;	/* Percentage identity. */
    };

void outputBlocks(struct block *blockList, FILE *f, boolean isRc, 
	char *qName, int qSize, char *tName, int tSize, boolean psl)
/* Output block list to file. */
{
int qNumInsert = 0, qBaseInsert = 0, tNumInsert = 0, tBaseInsert = 0;
int matchOne, match = 0, mismatch = 0, bases;
double scale;
struct block *lastBlock = NULL;
int score, blockCount = 0;
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

if (psl)
    {
    fprintf(f, "%d\t%d\t0\t0\t", match, mismatch);
    fprintf(f, "%d\t%d\t%d\t%d\t", qNumInsert, qBaseInsert, tNumInsert, tBaseInsert);
    if (isRc)
	fprintf(f, "-\t");
    else
	fprintf(f, "+\t");
    fprintf(f, "%s\t%d\t%d\t%d\t", qName, qSize, qTotalStart, qTotalEnd);
    fprintf(f, "%s\t%d\t%d\t%d\t", tName, tSize, tTotalStart, tTotalEnd);
    fprintf(f, "%d\t", blockCount);
    for (block = blockList; block != NULL; block = block->next)
	fprintf(f, "%d,", block->tEnd - block->tStart);
    fprintf(f, "\t");
    for (block = blockList; block != NULL; block = block->next)
	fprintf(f, "%d,", block->qStart);
    fprintf(f, "\t");
    for (block = blockList; block != NULL; block = block->next)
	fprintf(f, "%d,", block->tStart);
    fprintf(f, "\n");
    }
else  /* Output bed line. */
    {
    score = (match - mismatch - qNumInsert*2) * 1000 / (qTotalEnd - qTotalStart);
    fprintf(f, "%s\t%d\t%d\t%s\t%d\t", tName, tTotalStart, tTotalEnd,
	qName, score);
    if (isRc)
	fprintf(f, "-\t");
    else
	fprintf(f, "+\t");
    fprintf(f, "%d\t%d\t0\t%d\t", tTotalStart, tTotalEnd, blockCount);
    for (block = blockList; block != NULL; block = block->next)
	fprintf(f, "%d,", block->tEnd - block->tStart);
    fprintf(f, "\t");
    for (block = blockList; block != NULL; block = block->next)
	fprintf(f, "%d,", block->tStart);
    fprintf(f, "\n");
    }
}

void parseIntoBed(char *sim4out, 
	char *qName, int qSize, char *tName, int tSize, FILE *f)
/* Parse a sim4 output file and put it into bed file. */
{
struct lineFile *lf = lineFileOpen(sim4out, TRUE);
char *line, *row[3], *parts[3];
int partCount;
boolean isRc = FALSE;
struct block *blockList = NULL, *block;

/* Read header and remember if complemented. */
if (!lineFileNext(lf, &line, NULL))
   errAbort("%s is empty", lf->fileName);
if (!lineFileNext(lf, &line, NULL) || !startsWith("seq1", line))
   errAbort("%s doesn't seem to be a sim4 output file", lf->fileName);
lineFileNext(lf, &line, NULL);
lineFileNext(lf, &line, NULL);
lineFileNext(lf, &line, NULL);
if (startsWith("(complement)", line))
   isRc = TRUE;
else
   lineFileReuse(lf);

/* Parse rest of file into blockList. */
while (lineFileRow(lf, row))
    {
    AllocVar(block);
    partCount = chopString(row[0], "-", parts, ArraySize(parts));
    if (partCount != 2 || !isdigit(parts[0][0]) || !isdigit(parts[1][0]))
        sim4BadLine(lf);
    block->qStart = atoi(parts[0]) - 1;
    block->qEnd = atoi(parts[1]);
    partCount = chopString(row[1], "(-)", parts, ArraySize(parts));
    if (partCount != 2 || !isdigit(parts[0][0]) || !isdigit(parts[1][0]))
        sim4BadLine(lf);
    block->tStart = atoi(parts[0]) - 1;
    block->tEnd = atoi(parts[1]);
    if (!isdigit(row[2][0]))
        sim4BadLine(lf);
    block->percentId = atoi(row[2]);
    slAddHead(&blockList, block);
    }
lineFileClose(&lf);
slReverse(&blockList);
outputBlocks(blockList, f, isRc, qName, qSize, tName, tSize, FALSE);
slFreeList(&blockList);
}

void parseIntoPsl(char *lavFile, 
	char *qName, int qSize, char *tName, int tSize, FILE *f)
/* Parse a sim4 lav file and put it into something .psl like. */
{
struct lineFile *lf = lineFileOpen(lavFile, TRUE);
char *line, *words[10];
boolean gotAli = FALSE;
int wordCount;
struct block *blockList = NULL, *block;
boolean isRc = FALSE;

/* Check header. */
if (!lineFileNext(lf, &line, NULL))
   errAbort("%s is empty", lf->fileName);
if (!startsWith("#:lav", line))
   errAbort("%s is not a sim4 lav file\n", lf->fileName);

/* Search for alignment stanza. */
while (lineFileNext(lf, &line, NULL))
    {
    if (startsWith("a {", line))
        {
	gotAli = TRUE;
	break;
	}
    if (startsWith("(complement)", line))
        isRc = TRUE;
    }
if (gotAli)
    {
    while (lineFileNext(lf, &line, NULL))
        {
	if (line[0] == '#')
	    continue;
	if (line[0] == '}')
	    break;
	wordCount = chopLine(line, words);
	if (wordCount == 0)
	    continue;
	if (words[0][0] == 'l')
	    {
	    lineFileExpectWords(lf, 6, wordCount);
	    AllocVar(block);
	    block->qStart = lineFileNeedNum(lf, words, 1) - 1;
	    block->qEnd = lineFileNeedNum(lf, words, 3);
	    block->tStart = lineFileNeedNum(lf, words, 2) - 1;
	    block->tEnd = lineFileNeedNum(lf, words, 4);
	    if (block->qEnd - block->qStart != block->tEnd - block->tStart)
	        errAbort("Block size mismatch line %d of %s", lf->lineIx, lf->fileName);
	    block->percentId = lineFileNeedNum(lf, words, 5);
	    slAddHead(&blockList, block);
	    }
	}
    }
lineFileClose(&lf);
slReverse(&blockList);
outputBlocks(blockList, f, isRc, qName, qSize, tName, tSize, TRUE);
slFreeList(&blockList);
}

void dotOut()
/* Put out a dot every now and then if user want's to. */
{
static int mod = 1;
if (dotEvery > 0)
    {
    if (--mod <= 0)
	{
	fputc('.', stdout);
	fflush(stdout);
	mod = dotEvery;
	}
    }
}

void sim4big(char *genomicFile, char *mrnaFile, char *outputFile)
/* sim4big - A wrapper for Sim4 that runs it repeatedly on a multi-sequence .fa file. */
{
struct lineFile *gLf = lineFileOpen(genomicFile, TRUE), *mLf = NULL;
struct dnaSeq gSeq, mSeq;
char gTempName[512], mTempName[512], sTempName[512];
char gSeqName[512];
struct dyString *command = newDyString(512);
FILE *f = mustOpen(outputFile, "w");
ZeroVar(&gSeq);
ZeroVar(&mSeq);

tempFile(tempDir, "g", ".fa", gTempName);
tempFile(tempDir, "m", ".fa", mTempName);
tempFile(tempDir, "s", ".sim4", sTempName);
dyStringPrintf(command, "%s %s %s", exePath, mTempName, gTempName);
if (pslOut)
   dyStringPrintf(command, " A=2");
dyStringPrintf(command, " > %s", sTempName);

while (faMixedSpeedReadNext(gLf, &gSeq.dna, &gSeq.size, &gSeq.name))
    {
    strcpy(gSeqName, gSeq.name); /* Save name because calling speedRead again. */
    toUpperN(gSeq.dna, gSeq.size);
    faWrite(gTempName, gSeqName, gSeq.dna, gSeq.size);
    faFreeFastBuf();	/* Free potentially large buffer. */
    mLf = lineFileOpen(mrnaFile, TRUE);
    while (faMixedSpeedReadNext(mLf, &mSeq.dna, &mSeq.size, &mSeq.name))
        {
	dotOut();
	toUpperN(mSeq.dna, mSeq.size);
	faWrite(mTempName, mSeq.name, mSeq.dna, mSeq.size);
	mustSystem(command->string);
	if (pslOut)
	    parseIntoPsl(sTempName, mSeq.name, mSeq.size, gSeqName, gSeq.size, f);
	else
	    parseIntoBed(sTempName, mSeq.name, mSeq.size, gSeqName, gSeq.size, f);
	}
    lineFileClose(&mLf);
    }
if (dotEvery > 0)
    printf("\n");
lineFileClose(&gLf);
freeDyString(&command);
carefulClose(&f);
remove(gTempName);
remove(mTempName);
remove(sTempName);
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 4)
    usage();
tempDir = cgiUsualString("tempDir", tempDir);
exePath = cgiUsualString("exe", exePath);
pslOut = cgiBoolean("psl");
dotEvery = cgiUsualInt("dots", dotEvery);
sim4big(argv[1], argv[2], argv[3]);
return 0;
}
