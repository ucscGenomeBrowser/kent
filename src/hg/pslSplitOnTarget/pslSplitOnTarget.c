/* pslSplitOnTarget - Split psl files into one per target.. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "psl.h"


boolean lump = FALSE;
int maxTargetCount = 300;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslSplitOnTarget - Split psl files into one per target.\n"
  "usage:\n"
  "   pslSplitOnTarget inFile.psl outDir\n"
  "options:\n"
  "   -maxTargetCount=N - Maximum allowed targets (default is %d).\n"
  "        This implementation keeps an open file handle for each target.\n"
  "   -lump - useful with scaffolds, hashes on targ name to lump together.\n"
  "           (creates maxTargetCount lumps off scaffold name hash).\n"
  , maxTargetCount
  );
}

static struct optionSpec options[] = {
   {"maxTargetCount", OPTION_INT},
   {"lump", OPTION_BOOLEAN},
   {NULL, 0},
};

char *lumpName(char *name)
/* Look for integer part of name,  then do mod operation 
 * on it to assign to lump. */
{
int lump = maxTargetCount;
char *s = name, c;
for (;;)
    {
    c = *s;
    if (c == 0)
        errAbort("No digits in %s,  need digits in name for lump optoin", 
		name);
    if (isdigit(c))
        {
	static char buf[32];
	int lumpIx = atoi(s) % lump;
	lumpIx %= lump;
	safef(buf, sizeof(buf), "%03d", lumpIx);
	return buf;
	}
    ++s;
    }
}

void pslSplitOnTarget(char *inFile, char *outDir)
/* pslSplitOnTarget - Split psl files into one per target.. */
{
struct hash *targetHash = hashNew(0);
int targetCount = 0;
struct lineFile *lf = pslFileOpen(inFile);
int lineSize, wordCount;
FILE *f;
char *line, *row[14], lineBuf[1024*64];

makeDir(outDir);
while (lineFileNext(lf, &line, &lineSize))
    {
    char *target;
    if (lineSize >= sizeof(lineBuf))
         errAbort("Line %d of %s is too long (%d bytes)", 
	 	lf->lineIx, lf->fileName, lineSize);
    strcpy(lineBuf, line);
    wordCount = chopLine(line, row);
    if (wordCount < 14)
        errAbort("Truncated line %d of %s", lf->lineIx, lf->fileName);
    target = row[13];
    if (lump)
	target = lumpName(target);
    f = hashFindVal(targetHash, target);
    if (f == NULL)
        {
	char fileName[PATH_LEN];
	targetCount += 1;
	if (targetCount > maxTargetCount)
	    errAbort("Too many targets (more than %d) at %s line %d of %s",
	    	maxTargetCount, target, lf->lineIx, lf->fileName);
	safef(fileName, sizeof(fileName), "%s/%s.psl", outDir, target);
	f = mustOpen(fileName, "w");
	hashAdd(targetHash, target, f);
	}
    mustWrite(f, lineBuf, lineSize-1);
    fputc('\n', f);
    }

/* Now close all just for error reporting purposes. */
    {
    struct hashEl *el, *list;
    list = hashElListHash(targetHash);
    for (el = list; el != NULL; el = el->next)
        {
	FILE *f = el->val;
	carefulClose(&f);
	}
    hashElFreeList(&list);
    hashFree(&targetHash);
    }

}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
maxTargetCount = optionInt("maxTargetCount",maxTargetCount);
lump = optionExists("lump");
if (argc != 3)
    usage();
pslSplitOnTarget(argv[1], argv[2]);
return 0;
}
