/* pslSplitOnTarget - Split psl files into one per target.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "psl.h"

static char const rcsid[] = "$Id: pslSplitOnTarget.c,v 1.1 2004/10/21 07:53:07 kent Exp $";

int maxTargetCount = 300;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslSplitOnTarget - Split psl files into one per target.\n"
  "usage:\n"
  "   pslSplitOnTarget inFile.psl outDir\n"
  "options:\n"
  "   -maxTargetCount=N - Maximum allowed targets (default is %s).\n"
  "        This implementation keeps an open file handle for each target.\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

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
if (argc != 3)
    usage();
pslSplitOnTarget(argv[1], argv[2]);
return 0;
}
