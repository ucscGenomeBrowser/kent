/* faSplit - Split an fa file into several files.. */
#include "common.h"
#include "portable.h"
#include "linefile.h"
#include "dystring.h"
#include "dnautil.h"
#include "fa.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "faSplit - Split an fa file into several files.\n"
  "usage:\n"
  "   faSplit how input.fa count outRoot\n"
  "where how is either 'base' or 'sequence'.  Files\n"
  "split by sequence will be broken at the nearest\n"
  "fa record boundary, while those split by base will\n"
  "be broken at any base.\n"
  "Examples:\n"
  "   faSplit sequence estAll.fa 100 est\n"
  "This will break up estAll.fa into 100 files\n"
  "(numbered est001.fa est002.fa, ... est100.fa\n"
  "Files will only be broken at fa record boundaries\n"
  "   faSplit base chr1.fa 10 1_\n"
  "This will break up chr1.fa into 10 files\n");
}

long estimateFaSize(char *fileName)
/* Estimate number of bases from file size. */
{
long size = fileSize(fileName);
return 0.5 + size * 0.98;
}

long calcNextEnd(int fileIx, int totalFiles, int estSize)
/* Return next end to break at. */
{
if (fileIx == totalFiles)
     return 0x7fffffff;	/* bignum */
else
    return round((double)fileIx*(double)estSize/(double)totalFiles);
}

void splitByBase(char *inName, int splitCount, char *outRoot, long estSize)
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
long nextEnd = 0;
long curPos = 0;
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
	    sprintf(outPathName, "%s%s%02d.fa", dir, outFile, fileCount);
	    printf("writing %s\n", outPathName);
	    f = mustOpen(outPathName, "w");
	    fprintf(f, ">%s%02d\n", outFile, fileCount);
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

void splitByRecord(char *inName, int splitCount, char *outRoot, long estSize)
/* Split into a file base by base. */
{
struct dnaSeq *seq;
int len;
char *headLine;
FILE *in = mustOpen(inName, "r");
int digits = digitsBaseTen(splitCount);
long nextEnd = 0;
long curPos = 0;
int fileCount = 0;
FILE *f = NULL;
char outDir[256], outFile[128], ext[64], outPath[512];

splitPath(outRoot, outDir, outFile, ext);
while (faReadNext(in, inName, TRUE, &headLine, &seq))
    {
    len = strlen(headLine);
    headLine = trimSpaces(headLine);
    curPos += seq->size;
    if (curPos > nextEnd)
        {
	carefulClose(&f);
	sprintf(outPath, "%s%s%d.fa", outDir, outFile, fileCount++);
	printf("writing %s\n", outPath);
	f = mustOpen(outPath, "w");
	nextEnd = calcNextEnd(fileCount, splitCount, estSize);
	}
    faWriteNext(f, headLine+1, seq->dna, seq->size);
    freeDnaSeq(&seq);
    }
carefulClose(&f);
}

void faSplit(char *how, char *inName, int count, char *outRoot)
/* faSplit - Split an fa file into several files.. */
{
long estSize = estimateFaSize(inName);

if (sameWord(how, "sequence"))
    splitByRecord(inName, count, outRoot, estSize);
else if (sameWord(how, "base"))
    splitByBase(inName, count, outRoot, estSize);
else
    usage();
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 5 || !isdigit(argv[3][0]))
    usage();
dnaUtilOpen();
faSplit(argv[1], argv[2], atoi(argv[3]), argv[4]);
return 0;
}
