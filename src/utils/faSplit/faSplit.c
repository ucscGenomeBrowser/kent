/* faSplit - Split an fa file into several files.. */
#include "common.h"
#include "portable.h"
#include "linefile.h"
#include "dystring.h"
#include "dnautil.h"
#include "obscure.h"
#include "fa.h"
#include "options.h"
#include "bits.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "faSplit - Split an fa file into several files.\n"
  "usage:\n"
  "   faSplit how input.fa count outRoot\n"
  "where how is either 'base' 'sequence' or 'size'.  Files\n"
  "split by sequence will be broken at the nearest\n"
  "fa record boundary, while those split by base will\n"
  "be broken at any base.  Files broken by size will\n"
  "be broken every count bases.\n"
  "Examples:\n"
  "   faSplit sequence estAll.fa 100 est\n"
  "This will break up estAll.fa into 100 files\n"
  "(numbered est001.fa est002.fa, ... est100.fa\n"
  "Files will only be broken at fa record boundaries\n"
  "   faSplit base chr1.fa 10 1_\n"
  "This will break up chr1.fa into 10 files\n"
  "   faSplit size input.fa 2000 outRoot\n"
  "This breaks up input.fa into 2000 base chunks\n"
  "   faSplit about est.fa 20000 outRoot\n"
  "This will break up est.fa into files of about 20000 bytes each.\n"
  "\n"
  "Options:\n"
  "    -maxN=N - Suppress pieces with more than maxN n's.  Only used with size.\n"
  "              default is size/2\n"
  "    -oneFile - Put output in one file. Only used with size\n"
  "    -out=outFile Get masking from outfile.  Only used with size.\n"
  "    -lift=file.lft Put info on how to reconstruct sequence from\n"
  "                   pieces in file.lft.  Only used with size\n");

}

unsigned long estimateFaSize(char *fileName)
/* Estimate number of bases from file size. */
{
unsigned long size = fileSize(fileName);
return 0.5 + size * 0.99;
}

unsigned long calcNextEnd(int fileIx, int totalFiles, unsigned long estSize)
/* Return next end to break at. */
{
if (fileIx == totalFiles)
     return 0xefffffff;	/* bignum */
else
    {
    unsigned long nextEnd = round((double)fileIx*(double)estSize/(double)totalFiles/16.0);
    return nextEnd<<4;
    }
}

void splitByBase(char *inName, int splitCount, char *outRoot, unsigned long estSize)
/* Split into a file base by base. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
int lineSize;
char *line;
char c;
DNA b;
char dir[256], outFile[128], seqName[128], ext[64];
char outPathName[512];
int digits = digitsBaseTen(splitCount);
boolean warnedBadBases = FALSE;
boolean warnedMultipleRecords = FALSE;
int fileCount = 0;
unsigned long nextEnd = 0;
unsigned long curPos = 0;
FILE *f = NULL;
int linePos = 0;
int outLineSize = 50;


if (!lineFileNext(lf, &line, &lineSize))
    errAbort("%s is empty", inName);
if (line[0] == '>')
    {
    line = firstWordInLine(line+1);
    if (line == NULL)
        errAbort("Empty initial '>' line in %s", inName);
    strncpy(seqName, line, sizeof(seqName));
    }
else
    {
    splitPath(inName, dir, seqName, ext);
    lineFileReuse(lf);
    }
splitPath(outRoot, dir, outFile, ext);
while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] == '>')
        {
	if (!warnedMultipleRecords)
	    {
	    warnedMultipleRecords = TRUE;
	    warn("More than one record in FA file line %d of %s", 
	    	lf->lineIx, lf->fileName);
	    continue;
	    }
	}
    while ((c = *line++) != 0)
        {
	if (isdigit(c) || isspace(c))
	    continue;
	if (!isalpha(c))
	    errAbort("Weird %c (0x%x) line %d of %s", c, c, lf->lineIx, lf->fileName);
	if (++curPos >= nextEnd)
	    {
	    if (f != NULL)
	        {
		if (linePos != 0)
		    fputc('\n', f);
		fclose(f);
		}
	    sprintf(outPathName, "%s%s%0*d.fa", dir, outFile, 
	    	digits, fileCount);
	    printf("writing %s\n", outPathName);
	    f = mustOpen(outPathName, "w");
	    fprintf(f, ">%s%0*d\n", outFile, digits, fileCount);
	    ++fileCount;
	    linePos = 0;
	    nextEnd = calcNextEnd(fileCount, splitCount, estSize);
	    }
	fputc(c, f);
	if (++linePos >= outLineSize)
	    {
	    fputc('\n', f);
	    linePos = 0;
	    }
	}
    }
if (f != NULL)
    {
    if (linePos != 0)
	fputc('\n', f);
    fclose(f);
    }
lineFileClose(&lf);
}

void splitByRecord(char *inName, int splitCount, char *outRoot, unsigned long estSize)
/* Split into a file base by base. */
{
struct dnaSeq seq;
int len;
struct lineFile *lf = lineFileOpen(inName, TRUE);
int digits = digitsBaseTen(splitCount);
unsigned long nextEnd = 0;
unsigned long curPos = 0;
int fileCount = 0;
FILE *f = NULL;
char outDir[256], outFile[128], ext[64], outPath[512];
ZeroVar(&seq);

splitPath(outRoot, outDir, outFile, ext);
while (faMixedSpeedReadNext(lf, &seq.dna, &seq.size, &seq.name))
    {
    curPos += seq.size;
    if (curPos > nextEnd)
        {
	carefulClose(&f);
	sprintf(outPath, "%s%s%0*d.fa", outDir, outFile, digits, fileCount++);
	printf("writing %s\n", outPath);
	f = mustOpen(outPath, "w");
	nextEnd = calcNextEnd(fileCount, splitCount, estSize);
	}
    faWriteNext(f, seq.name, seq.dna, seq.size);
    }
carefulClose(&f);
lineFileClose(&lf);
}

void splitAbout(char *inName, unsigned long approxSize, char *outRoot)
/* Split into chunks of about approxSize.  Don't break up
 * sequence though. */
{
struct dnaSeq seq;
int len;
struct lineFile *lf = lineFileOpen(inName, TRUE);
int digits = 2;
unsigned long curPos = approxSize;
int fileCount = 0;
FILE *f = NULL;
char outDir[256], outFile[128], ext[64], outPath[512];
ZeroVar(&seq);

splitPath(outRoot, outDir, outFile, ext);
while (faMixedSpeedReadNext(lf, &seq.dna, &seq.size, &seq.name))
    {
    if (curPos >= approxSize)
        {
	carefulClose(&f);
	curPos = 0;
	sprintf(outPath, "%s%s%0*d.fa", outDir, outFile, digits, fileCount++);
	printf("writing %s\n", outPath);
	f = mustOpen(outPath, "w");
	}
    curPos += seq.size;
    faWriteNext(f, seq.name, seq.dna, seq.size);
    }
carefulClose(&f);
lineFileClose(&lf);
}

int countN(char *s, int size)
/* Count number of N's from s[0] to s[size-1].
 * Treat any parts past end of string as N's. */
{
int goodCount = 0;
int i;
char c;

for (i=0; i<size; ++i)
    {
    c = s[i];
    if (c == 0)
        break;
    if (c != 'n')
        ++goodCount;
    }
return size - goodCount;
}

void bitsForOut(char *fileName, int seqSize, Bits *bits)
/* Get bitmap that corresponds to outFile. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line, *words[8];
int wordCount;
boolean firstTime = TRUE;
int start,end;

/* Check and skip over three line header */
if (!lineFileNext(lf, &line, NULL))
    errAbort("%s is empty", fileName);
line = skipLeadingSpaces(line);
if (!startsWith("SW", line))
    errAbort("%s is not a RepeatMasker .out file", fileName);
lineFileNext(lf, &line, NULL);
if (!startsWith("score", line))
    errAbort("%s is not a RepeatMasker .out file", fileName);
lineFileNext(lf, &line, NULL);

for (;;)
    {
    if (!lineFileNext(lf, &line, NULL))
        break;
    wordCount = chopLine(line, words);
    if (wordCount < 8)
	errAbort("Short line %d of %s\n", lf->lineIx, lf->fileName);
    start = lineFileNeedNum(lf, words, 5) - 1;
    end = lineFileNeedNum(lf, words, 6);
    if (start > end)
        errAbort("Start after end line %d of %s", lf->lineIx, lf->fileName);
    if (firstTime)
        {
	char *s = words[7];
	if (s[0] != '(' || !isdigit(s[1]))
	    errAbort("Expected parenthesized number line %d of %s", lf->lineIx, lf->fileName);
	if (seqSize != end + atoi(s+1))
	    errAbort("Size mismatch line %d of %s", lf->lineIx, lf->fileName);
	firstTime = FALSE;
	}
    if (end > seqSize)
        errAbort("End past bounds line %d of %s", lf->lineIx, lf->fileName);
    bitSetRange(bits, start, end-start);
    }
lineFileClose(&lf);
}

void setBitsN(DNA *dna, int size, Bits *bits)
/* Set bits in bitmap where there are N's in DNA. */
{
int i;
for (i=0; i<size; ++i)
    {
    if (dna[i] == 'n' || dna[i] == 'N')
	bitSetOne(bits, i);
    }
}

void splitByCount(char *inName, int pieceSize, char *outRoot, unsigned long estSize)
/* Split up file into pieces pieceSize long. */
{
unsigned long pieces = (estSize + pieceSize-1)/pieceSize;
int digits = digitsBaseTen(pieces);
int maxN = optionInt("maxN", pieceSize);
boolean oneFile = optionExists("oneFile");
char fileName[512];
char dirOnly[256], noPath[128];
char numOut[128];
int pos, pieceIx = 0, writeCount = 0;
struct dnaSeq seq;
struct lineFile *lf = lineFileOpen(inName, TRUE);
FILE *f = NULL;
Bits *bits = NULL;
int seqCount = 0;
char *outFile = optionVal("out", NULL);
char *liftFile = optionVal("lift", NULL);
FILE *lift = NULL;
ZeroVar(&seq);

splitPath(outRoot, dirOnly, noPath, NULL);
if (oneFile)
    {
    sprintf(fileName, "%s.fa", outRoot);
    f = mustOpen(fileName, "w");
    }
if (liftFile)
    lift = mustOpen(liftFile, "w");


/* Count number of N's from s[0] to s[size-1].
 * Treat any parts past end of string as N's. */
while (faMixedSpeedReadNext(lf, &seq.dna, &seq.size, &seq.name))
    {
    bits = bitAlloc(seq.size);
    setBitsN(seq.dna, seq.size, bits);
    ++seqCount;
    if (outFile != NULL)
        {
	if (seqCount > 1)
	    errAbort("Can only handle in files with one sequence using out option");
	bitsForOut(outFile, seq.size, bits);
	}
    for (pos = 0; pos < seq.size; pos += pieceSize)
        {
	char numOut[128];
	int thisSize = seq.size - pos;
	if (thisSize > pieceSize) 
	    thisSize = pieceSize;
	sprintf(numOut, "%s%0*d", noPath, digits, pieceIx++);
	if (bitCountRange(bits, pos, thisSize) <= maxN)
	    {
	    if (!oneFile)
	        {
		sprintf(fileName, "%s%s.fa", dirOnly, numOut);
		f = mustOpen(fileName, "w");
		}
	    faWriteNext(f, numOut, seq.dna + pos, thisSize);
	    if (lift)
	        fprintf(lift, "%d\t%s\t%d\t%s\t%d\n",
		    pos, numOut, thisSize, seq.name, seq.size);
	    ++writeCount;
	    if (!oneFile)
	        carefulClose(&f);
	    }
	}
    bitFree(&bits);
    }
carefulClose(&f);
carefulClose(&lift);
lineFileClose(&lf);
printf("%d pieces of %d written\n", writeCount, pieceIx);
}

void faSplit(char *how, char *inName, int count, char *outRoot)
/* faSplit - Split an fa file into several files.. */
{
unsigned long estSize = estimateFaSize(inName);

if (sameWord(how, "sequence"))
    splitByRecord(inName, count, outRoot, estSize);
else if (sameWord(how, "base"))
    splitByBase(inName, count, outRoot, estSize);
else if (sameWord(how, "size"))
    splitByCount(inName, count, outRoot, estSize);
else if (sameWord(how, "about"))
    splitAbout(inName, count, outRoot);
else
    usage();
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 5 || !isdigit(argv[3][0]))
    usage();
dnaUtilOpen();
faSplit(argv[1], argv[2], atoi(argv[3]), argv[4]);
return 0;
}
