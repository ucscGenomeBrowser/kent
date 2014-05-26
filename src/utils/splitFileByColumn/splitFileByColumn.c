/* splitFileByColumn - Split text input into files named by column value. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "options.h"
#include "obscure.h"

/* Option variables: */
int col = 1;
char *headerText = NULL;
char *tailerText = NULL;
boolean chromDirs = FALSE;
char *ending = NULL;
boolean tab = FALSE;

/* Make this global since we will need it when traversing a hash: */
char *outDirName = NULL;

#define MAX_COL_OFFSET 32

void usage()
/* Explain usage and exit. */
{
errAbort(
"splitFileByColumn - Split text input into files named by column value\n"
"usage:\n"
"   splitFileByColumn source outDir\n"
"options:\n"
"   -col=N      - Use the Nth column value (default: N=1, first column)\n"
"   -head=file  - Put head in front of each output\n"
"   -tail=file  - Put tail at end of each output\n"
"   -chromDirs  - Split into subdirs of outDir that are distilled from chrom\n"
"                 names, e.g. chr3_random -> outDir/3/chr3_random.XXX .\n"
"   -ending=XXX - Use XXX as the dot-suffix of split files (default: taken\n"
"                 from source).\n"
"   -tab        - Split by tab characters instead of whitespace.\n"
"Split source into multiple files in outDir, with each filename determined\n"
"by values from a column of whitespace-separated input in source.\n"
"If source begins with a header, you should pipe \"tail +N source\" to this\n"
"program where N is number of header lines plus 1, or use some similar\n"
"method to strip the header from the input.\n"
);
}

static struct optionSpec options[] = {
    {"col", OPTION_INT},
    {"head", OPTION_STRING},
    {"tail", OPTION_STRING},
    {"chromDirs", OPTION_BOOLEAN},
    {"ending", OPTION_STRING},
    {"tab", OPTION_BOOLEAN},
    {NULL, 0},
};


char *getFileName(char *baseName)
/* Return a complete path given dir and basename (add chromDir if applicable 
 * and ending). */
{
struct dyString *dy = dyStringNew(0);
dyStringPrintf(dy, "%s/", outDirName);
if (chromDirs)
    {
    char *trunc = cloneString(baseName);
    char *ptr = strstr(trunc, "_random");
    if (ptr != NULL)
	*ptr = '\0';
    if (startsWith("chr", trunc))
	strcpy(trunc, trunc+strlen("chr"));
    dyStringPrintf(dy, "%s/", trunc);
    freeMem(trunc);
    makeDirs(dy->string);
    }
dyStringPrintf(dy, "%s%s", baseName, ending);
return dyStringCannibalize(&dy);
}


FILE *getFp(struct hash *chromFpHash, char *baseName)
/* Return an open file pointer for file baseName. */
{
/* Assume that the input will usually contain contiguous chunks of rows 
 * destined for the same output file, so we won't waste too much time if
 * we close and open a file each time baseName changes.  Having only one 
 * open file pointer, instead of keeping all files open, prevents us from 
 * running into the operating system limit on open file pointers. */
static char *prevBaseName = NULL;
static int fileCount = 0;
FILE *f = NULL;
struct hashEl *hel = hashLookup(chromFpHash, baseName);
struct hashEl *prevHel = NULL;

if (hel == NULL)
    {
    /* Brand new baseName - close prevFile, open new file and store in hash. */
    char *outFileName = getFileName(baseName);
    if (prevBaseName != NULL)
	{
	prevHel = hashLookup(chromFpHash, prevBaseName);
	if (prevHel != NULL)
	    carefulClose((FILE **)&(prevHel->val));
	}
    verbose(1, "Creating %s\n", outFileName);
    if (fileCount == 1001)
	warn("Warning: greater than 1000 files have been created in %s",
	     outDirName);
    f = mustOpen(outFileName, "w");
    if (headerText != NULL)
	fprintf(f, "%s", headerText);
    hashAdd(chromFpHash, baseName, f);
    freez(&outFileName);
    fileCount++;
    }
else if (hel->val == NULL)
    {
    /* File has been opened before but is closed -- close prevFile, append. */
    char *outFileName = getFileName(baseName);
    if (prevBaseName != NULL)
	{
	prevHel = hashLookup(chromFpHash, prevBaseName);
	if (prevHel != NULL)
	    carefulClose((FILE **)&(prevHel->val));
	}
    hel->val = f = mustOpen(outFileName, "a");
    freez(&outFileName);
    }
else if (!sameString(baseName, prevBaseName))
    {
    errAbort("program error: a cached file pointer is open but baseName (%s)"
	     " != prevBaseName (%s)", baseName, prevBaseName);
    }
else
    {
    /* Write to open file pointer. */
    f = hel->val;
    }

if (prevBaseName == NULL || !sameString(baseName, prevBaseName))
    {
    freez(&prevBaseName);
    prevBaseName = cloneString(baseName);
    }

return f;
}


void addTailAndClose(struct hashEl *hel)
/* Given an element of chromFpHash, add tailerText if specified (which may
 * require reopening the file) and close the file pointer. */
{
if (tailerText != NULL)
    {
    FILE *f = (FILE *)(hel->val);
    if (f == NULL)
	{
	char *outFileName = getFileName(hel->name);
	hel->val = f = mustOpen(outFileName, "a");
	}
    fprintf(f, "%s", tailerText);
    }
carefulClose((FILE **)&(hel->val));
}

void closeFiles(struct hash *chromFpHash)
/* Apply addTailAndClose to all items in chromFpHash. */
{
hashTraverseEls(chromFpHash, addTailAndClose);
}


void splitFileByColumn(char *inFileName)
/* splitFileByColumn - Split text input into files named by column value. */
{
struct lineFile *lf = lineFileOpen(inFileName, TRUE);
struct hash *chromFpHash = hashNew(10);
char *line = NULL;

if (ending == NULL)
    {
    char *ptr = inFileName + strlen(inFileName) - 1;
    while (ptr > inFileName && *ptr != '/')
	ptr--;
    ptr = strchr(ptr, '.');
    if (ptr == NULL)
	ending = "";
    else
	ending = ptr;
    }
else if (ending[0] != '.')
    {
    /* If -ending is given without initial ., prepend .: */
    size_t deSize = sizeof(char) * strlen(ending) + 2;
    char *dotEnding = cloneStringZ(ending, deSize);
    safef(dotEnding, deSize, ".%s", ending);
    ending = dotEnding;
    }

makeDirs(outDirName);
while (lineFileNext(lf, &line, NULL))
    {
    char *lineClone = cloneString(line);
    char *words[MAX_COL_OFFSET+1];
    int wordCount = 0;
    char *colVal = NULL;
    FILE *f = NULL;
    
    if (tab)
	wordCount = chopTabs(lineClone, words);
    else
	wordCount = chopLine(lineClone, words);
    if (wordCount < col)
	errAbort("Too few columns line %d of %s (%d, need at least %d)",
		 lf->lineIx, lf->fileName, wordCount, col);

    colVal = words[col-1];
    f = getFp(chromFpHash, colVal);
    fprintf(f, "%s\n", line);
    freeMem(lineClone);
    }
lineFileClose(&lf);
closeFiles(chromFpHash);
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *headFileName = NULL, *tailFileName = NULL;
optionInit(&argc, argv, options);
col = optionInt("col", col);
headFileName = optionVal("head", headFileName);
tailFileName = optionVal("tail", tailFileName);
chromDirs = optionExists("chromDirs");
ending = optionVal("ending", ending);
tab = optionExists("tab");
if (headFileName != NULL)
    {
    readInGulp(headFileName, &headerText, NULL);
    }
if (tailFileName != NULL)
    {
    readInGulp(tailFileName, &tailerText, NULL);
    }
if (argc != 3)
    usage();
if (col > MAX_COL_OFFSET)
    errAbort("Sorry, max -col offset currently supported is %d",
	     MAX_COL_OFFSET);
outDirName = argv[2];
splitFileByColumn(argv[1]);
return 0;
}
