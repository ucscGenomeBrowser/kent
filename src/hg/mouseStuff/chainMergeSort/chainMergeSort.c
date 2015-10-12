/* chainMergeSort - Combine sorted files into larger sorted file. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dlist.h"
#include "chainBlock.h"
#include "verbose.h"
#include "portable.h"
#include "quickHeap.h"


#define MAXFILES 400

boolean saveId = FALSE;
char *inputList = NULL;
char *tempDir = "./";


void usage()
/* Explain usage and exit. */
{
errAbort(
  "chainMergeSort - Combine sorted files into larger sorted file\n"
  "usage:\n"
  "   chainMergeSort file(s)\n"
  "Output goes to standard output\n"
  "options:\n"
  "   -saveId - keep the existing chain ids.\n"
  "   -inputList=somefile - somefile contains list of input chain files.\n"
  "   -tempDir=somedir/ - somedir has space for temporary sorting data, default ./\n"
  );
}

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"saveId"   , OPTION_BOOLEAN},
    {"inputList", OPTION_STRING},
    {"tempDir"  , OPTION_STRING},
    {NULL, 0}
};


struct chainFile 
/* A line file and the current chain from it. */
    {
    struct lineFile *lf;
    struct chain *chain;
    };


static void cfEof(struct chainFile **pCf, int level)
/* deal with EOF */
{
struct chainFile *cf = *pCf;
char fname[PATH_LEN];  /* order important next few lines */
safef(fname,sizeof(fname),"%s",cf->lf->fileName);
lineFileClose(&cf->lf);
if (level > 0)          /* if not original input level, then */
    remove(fname);      /* remove temp file */
freez(pCf);
}

static int cmpChainScores(const void *elem1,  const void *elem2)
/* Compare two chains by score, return elem1-score - elem2-score,
 * where the scores will be positive or 0. 
 * However since the scores are doubles, this is larger than the 
 * return range of int, so return 1, 0, or -1.
 */
{
struct chainFile *e1 = (struct chainFile *) elem1;
struct chainFile *e2 = (struct chainFile *) elem2;
double diff = e1->chain->score - e2->chain->score;
if (diff > 0.0) 
    return 1;
if (diff < 0.0) 
    return -1;
return  0;
}

void chainMergeSort(int fileCount, char *files[], FILE *out, int level)
/* chainMergeSort - Combine sorted files into larger sorted file. */
{
int i;
struct chainFile *cf;
int id = 0;
struct quickHeap *h = NULL;

h = newQuickHeap(fileCount, &cmpChainScores);

/* Open up all input files and read first chain. */
for (i=0; i<fileCount; ++i)
    {
    AllocVar(cf);
    cf->lf = lineFileOpen(files[i], TRUE);
    lineFileSetMetaDataOutput(cf->lf, out);
    cf->chain = chainRead(cf->lf);
    if (cf->chain)
    	addToQuickHeap(h, cf);
    else
	cfEof(&cf,level);  /* deal with EOF */
    }

while (!quickHeapEmpty(h))
    {
    cf = peekQuickHeapTop(h);
    if (!saveId)
	cf->chain->id = ++id;		/* We reset id's here. */
    chainWrite(cf->chain, out);
    chainFree(&cf->chain);
    if ((cf->chain = chainRead(cf->lf)))
	{
	quickHeapTopChanged(h);
	}
    else
	{ /* deal with EOF */
	if (!removeFromQuickHeapByElem(h, cf))
	    errAbort("unexpected error: chainFile not found on heap");
	cfEof(&cf,level);  
	}
    }

freeQuickHeap(&h);

}

void hierSort(char *inputList)
/* Do a hierarchical merge sort so we don't run out of system file handles */
{
int level = 0;
char thisName[PATH_LEN]; 
char nextName[PATH_LEN]; 
char sortName[PATH_LEN]; 
char tmpNameBuf[PATH_LEN]; 
struct lineFile *thisLf = NULL;
FILE *nextF = NULL;
int sortCount = 0;
FILE *sortF = NULL;
int fileCount = 0; 
char *files[MAXFILES];
boolean more = FALSE;
int block=0;
char *line=NULL;
safef(nextName, sizeof(nextName), "%s", inputList);
do
    {
    block=0;
    safef(thisName, sizeof(thisName), "%s", nextName);
    safef(tmpNameBuf, sizeof(tmpNameBuf), "inputList%d-", level+1);
    safecpy(nextName, sizeof(nextName), rTempName(tempDir, tmpNameBuf, ".tmp"));
    thisLf = lineFileOpen(thisName,TRUE);
    if (!thisLf)
	errAbort("error  lineFileOpen(%s) returned NULL\n",thisName);
    more = lineFileNext(thisLf, &line, NULL);
    while (more)
	{
	int i=0;
	fileCount = 0;
	while (more && fileCount < MAXFILES) 
	    {
	    files[fileCount++]=cloneString(line);
	    more = lineFileNext(thisLf, &line, NULL);
    	    }
	if (!more && block==0)
	    { /* last level */
	    sortF = stdout;
	    }
	else
	    {
	    if (!nextF)
		nextF = mustOpen(nextName,"w");
	    safef(tmpNameBuf, sizeof(tmpNameBuf), "sort%d-", sortCount++);
	    safecpy(sortName, sizeof(sortName), rTempName(tempDir, tmpNameBuf, ".tmp"));
	    fprintf(nextF, "%s\n", sortName);
	    sortF = mustOpen(sortName,"w");
	    }
    	chainMergeSort(fileCount, files, sortF, level);
	if (sortF != stdout)
    	    carefulClose(&sortF);
	for(i=0;i<fileCount;++i)	    
    	    freez(&files[i]);
	verboseDot();
	verbose(2,"block=%d\n",block);
	++block;
	}
    lineFileClose(&thisLf);
    if (nextF)
	carefulClose(&nextF);
    if (level > 0)
	{
	remove(thisName);
	}
    verbose(1,"\n");
    verbose(2,"level=%d, block=%d\n",level,block);
    ++level;
    } while (block > 1);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
saveId = optionExists("saveId");
inputList = optionVal("inputList",inputList);
tempDir = optionVal("tempDir",tempDir);
if ((argc < 2 && !inputList) || (argc > 1 && inputList))
    usage();
if (tempDir[0]!=0 && lastChar(tempDir) != '/')
    tempDir = addSuffix(tempDir,"/");
if (argc-1 <= MAXFILES && !inputList)
    {
    chainMergeSort(argc-1, argv+1, stdout, 0);
    }
else 
    {
    char inp0[PATH_LEN];
    safecpy(inp0, sizeof(inp0), rTempName(tempDir, "inputList0-", ".tmp"));
    if (!inputList)
	{
	FILE *f = mustOpen(inp0,"w");
	int i=0;
	for (i=1; i<argc; ++i)
	    {
	    fprintf(f, "%s\n", argv[i]);
	    }
	carefulClose(&f);
	inputList = inp0;
	}
    hierSort(inputList);
    if (sameString(inputList,inp0))
	remove(inp0);
    }
return 0;
}
