/* chainSort - Sort chains. */

/* Copyright (C) 2012 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "chain.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "chainSort - Sort chains.  By default sorts by score.\n"
  "Note this loads all chains into memory, so it is not\n"
  "suitable for large sets.  Instead, run chainSort on\n"
  "multiple small files, followed by chainMergeSort.\n"
  "usage:\n"
  "   chainSort inFile outFile\n"
  "Note that inFile and outFile can be the same\n"
  "options:\n"
  "   -target sort on target start rather than score\n"
  "   -query sort on query start rather than score\n"
  "   -index=out.tab build simple two column index file\n"
  "                    <out file position>  <value>\n"
  "                  where <value> is score, target, or query \n"
  "                  depending on the sort.\n"
  );
}

static struct optionSpec options[] = {
   {"target", OPTION_BOOLEAN},
   {"query", OPTION_BOOLEAN},
   {"index", OPTION_STRING},
   {NULL, 0},
};


void chainSort(char *inFile, char *outFile)
/* chainSort - Sort chains. */
{
struct chain *chainList = NULL, *chain;
struct lineFile *lf = lineFileOpen(inFile, TRUE);
FILE *f = mustOpen(outFile, "w");
FILE *index = NULL;
char *indexName;
boolean isQuery = optionExists("query");
boolean isTarget = optionExists("target");
double lastScore = -1;
char *lastTarget = "";
char *lastQuery = "";

indexName = optionVal("index", NULL);
if (indexName != NULL)
    index = mustOpen(indexName, "w");
lineFileSetMetaDataOutput(lf, f);

verbose(2, "indexName %s, index %p\n", indexName, index);

/* Read in all chains. */
while ((chain = chainRead(lf)) != NULL)
    {
    slAddHead(&chainList, chain);
    }
lineFileClose(&lf);

/* Sort. */
if (isTarget)
    slSort(&chainList, chainCmpTarget);
else if (isQuery)
    slSort(&chainList, chainCmpQuery);
else
    slSort(&chainList, chainCmpScore);

/* Output. */
for (chain = chainList; chain != NULL; chain = chain->next)
    {
    if (index != NULL)
        {
	if (isTarget)
	    {
	    if (!sameString(chain->tName, lastTarget))
		{
		lastTarget = chain->tName;
		fprintf(index, "%lx\t", ftell(f));
		fprintf(index, "%s\n", chain->tName);
		}
	    }
	else if (isQuery)
	    {
	    if (!sameString(chain->qName, lastQuery))
		{
		lastQuery = chain->qName;
		fprintf(index, "%lx\t", ftell(f));
		fprintf(index, "%s\n", chain->qName);
		}
	    }
	else
	    {
	    if (chain->score != lastScore)
		{
		lastScore = chain->score;
		fprintf(index, "%lx\t", ftell(f));
		fprintf(index, "%1.0f\n", chain->score);
		}
	    }
	}
    chainWrite(chain, f);
    }
carefulClose(&index);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
chainSort(argv[1], argv[2]);
return 0;
}
