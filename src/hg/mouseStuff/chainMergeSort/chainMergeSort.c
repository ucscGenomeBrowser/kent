/* chainMergeSort - Combine sorted files into larger sorted file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dlist.h"
#include "chainBlock.h"
#include "verbose.h"
#include "portable.h"

static char const rcsid[] = "$Id: chainMergeSort.c,v 1.8 2005/10/20 21:29:42 galt Exp $";

#define MAXFILES 250

boolean saveId = FALSE;
char *inputList = NULL;
char *tempDir = "./";

struct tempName tempName;


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


static void cfEof(struct chainFile *cf, int level)
/* deal with EOF */
{
char fname[256];  /* order important next few lines */
safef(fname,sizeof(fname),"%s",cf->lf->fileName);
lineFileClose(&cf->lf);
if (level > 0)          /* if not original input level, then */
    remove(fname);      /* remove temp file */
AllocVar(cf->chain);    /* make a sentinel stand-in  */
cf->chain->score = -1;  /* special handling like EOF */
}

void chainMergeSort(int fileCount, char *files[], FILE *out, int level)
/* chainMergeSort - Combine sorted files into larger sorted file. */
{
int i, n, p, c1, c2;   /* node, parent, child1, child2 */
struct chainFile *cf;
int id = 0;
struct chainFile **cfHeap = NULL;

cfHeap = needMem(fileCount*sizeof(struct chainFile*));

/* Open up all input files and read first chain. */
for (i=0; i<fileCount; ++i)
    {
    AllocVar(cf);
    cf->lf = lineFileOpen(files[i], TRUE);
    lineFileSetMetaDataOutput(cf->lf, out);
    cf->chain = chainRead(cf->lf);
    if (!cf->chain)
	{
	cfEof(cf,level);  /* deal with EOF */
	}
    cfHeap[i] = cf;
    /* preserve heap property as it grows */
    n = i;
    p = (n-1)/2;
    while (n > 0 && cfHeap[p]->chain->score < cfHeap[n]->chain->score)
	{
	struct chainFile *temp = cfHeap[p];
	cfHeap[p] = cfHeap[n];
	cfHeap[n] = temp;
	n = p;
	p = (n-1)/2;
	}
    }

/* cf heap maintains its structure - static during processing,
 * even eof on an input file just keeps score -1.
 * We are done when top of heap has score -1, 
 * i.e. all file data has been used up, all eof, all -1 
 * Otherwise, maintaining the heap is quick and easy since
 * we just use an array with the following relationships:
 * children(n) == 2n+1,2n+2. parent(n) == (n-1)/2.
 * The highest score is always at the top of the heap in 
 * position 0. When the top file's chain is consumed and 
 * sent to output and the next chain for the previously top file is 
 * read in, we simply balance the heap to maintain the heap property
 * by swapping with the largest child until both children 
 * are smaller, or we hit the end of the array (==fileCount).
 * Also note that scores are not unique, two heap members
 * of the heap may have equal score.  So A parent need not
 * be greater than the children, it may also be equal.
 */

while (cfHeap[0]->chain->score != -1)
    {
    cf = cfHeap[0];
    if (!saveId)
	cf->chain->id = ++id;		/* We reset id's here. */
    chainWrite(cf->chain, out);
    chainFree(&cf->chain);
    if ((cf->chain = chainRead(cf->lf)) == NULL)
	{
	cfEof(cf,level);  /* deal with EOF */
	}
    /* balance heap - swap node with biggest child until both children are 
     * either less or equal or hit end of heap (no more children) */
    n = 0;
    c1 = 2*n+1;
    c2 = 2*n+2;
    while (TRUE)
	{
	struct chainFile *temp = NULL;
	int bestc = n;
	if (c1 < fileCount && cfHeap[c1]->chain->score > cfHeap[bestc]->chain->score)
	    bestc = c1;
	if (c2 < fileCount && cfHeap[c2]->chain->score > cfHeap[bestc]->chain->score)
	    bestc = c2;
	if (bestc == n)
	    break;
	temp = cfHeap[bestc];
	cfHeap[bestc] = cfHeap[n];
	cfHeap[n] = temp;
	n = bestc;
	c1 = 2*n+1;
    	c2 = 2*n+2;
	}
    }

}

void hierSort(char *inputList)
/* Do a hierarchical merge sort so we don't run out of system file handles */
{
int level = 0;
char thisName[256]; 
char nextName[256]; 
char sortName[256]; 
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
    safef(nextName, sizeof(nextName), "%sinputList%d-", tempDir, level+1);
    makeTempName(&tempName, nextName, ".tmp");
    safef(nextName, sizeof(nextName), "%s", tempName.forCgi);
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
	    safef(sortName, sizeof(sortName), "%ssort%d-", tempDir, sortCount++);
	    makeTempName(&tempName, sortName, ".tmp");
	    safef(sortName, sizeof(sortName), "%s", tempName.forCgi);
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
if (argc < 2 && !inputList || argc > 1 && inputList)
    usage();
if (tempDir[0]!=0 && lastChar(tempDir) != '/')
    tempDir = addSuffix(tempDir,"/");
if (argc-1 <= MAXFILES && !inputList)
    {
    chainMergeSort(argc-1, argv+1, stdout, 0);
    }
else 
    {
    char *inp0 = addSuffix(tempDir,"inputList0-");
    makeTempName(&tempName, inp0, ".tmp");
    freez(&inp0);
    inp0 = cloneString(tempName.forCgi);
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
    freez(&inp0);	    
    }
return 0;
}
