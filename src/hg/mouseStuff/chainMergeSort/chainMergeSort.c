/* chainMergeSort - Combine sorted files into larger sorted file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dlist.h"
#include "chainBlock.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "chainMergeSort - Combine sorted files into larger sorted file\n"
  "usage:\n"
  "   chainMergeSort file(s)\n"
  "Output goes to standard output\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}


struct chainFile 
/* A line file and the current chain from it. */
    {
    struct lineFile *lf;
    struct chain *chain;
    };

void chainMergeSort(int fileCount, char *files[])
/* chainMergeSort - Combine sorted files into larger sorted file. */
{
int i;
struct dlList *list = newDlList();	/* List of lineFiles */
struct dlNode *node, *next;
struct chainFile *cf;
int id = 0;

/* Open up all input files and read first chain. */
for (i=0; i<fileCount; ++i)
    {
    AllocVar(cf);
    cf->lf = lineFileOpen(files[i], TRUE);
    cf->chain = chainRead(cf->lf);
    if (cf->chain != NULL)
	dlAddValTail(list, cf);
    }

while (!dlEmpty(list))
    {
    struct dlNode *bestNode = NULL;
    double bestScore = -BIGNUM;
    for (node = list->head; !dlEnd(node); node = node->next)
        {
	cf = node->val;
	if (cf->chain->score > bestScore)
	    {
	    bestScore = cf->chain->score;
	    bestNode = node;
	    }
	}
    cf = bestNode->val;
    cf->chain->id = ++id;		/* We reset id's here. */
    chainWrite(cf->chain, stdout);
    chainFree(&cf->chain);
    if ((cf->chain = chainRead(cf->lf)) == NULL)
	dlRemove(bestNode);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc < 2)
    usage();
chainMergeSort(argc-1, argv+1);
return 0;
}
