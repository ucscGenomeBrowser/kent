/* Blatz standalone main module.  Handle command-line processing, load sequence 
 * and call alignment routines. */
/* Copyright 2005 Jim Kent.  All rights reserved. */

#include "common.h"
#include "options.h"
#include "errabort.h"
#include "memalloc.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "chain.h"
#include "fa.h"
#include "dnaLoad.h"
#include "bzp.h"
#include "blatz.h"

void usage(struct bzp *bzp)
/* Explain usage and exit. */
{
printf("blatz version %d - Align dna across species\n", bzpVersion());
printf("usage:\n");
printf("   blatz target query output\n");
printf("where target and query are either fasta files, nib files, 2bit files\n");
printf("or a text files containing the names of the above files one per line.\n");
printf("It's important that the sequence be repeat masked with repeats in\n");
printf("lower case.\n");
printf("Options: (defaults are shown for numerical parameters)\n");
bzpServerOptionsHelp(bzp);
bzpClientOptionsHelp(bzp);
noWarnAbort();
}

static struct optionSpec options[] = {
   BZP_SERVER_OPTIONS
   BZP_CLIENT_OPTIONS
   {NULL, 0},
};

static void alignAll(struct bzp *bzp, struct dnaSeq *targetList, 
        struct dnaLoad *queryDl, char *outFile)
/* Make up neighorhood index for queryList, and use it to scan
 * targetList.  Put output in outFile */
{
FILE *f = mustOpen(outFile, "w");
struct blatzIndex *indexList = blatzIndexAll(targetList, bzp->weight);
struct dnaSeq *query;

bzpTime("Made index");
while ((query = dnaLoadNext(queryDl)) != NULL)
    {
    double bestScore = 0;
    struct chain *chainList;
    if (bzp->unmask)
        toUpperN(query->dna, query->size);
    if (bzp->rna)
        maskTailPolyA(query->dna, query->size);
    chainList = blatzAlign(bzp, indexList, query);
    if (chainList != NULL) 
    	bestScore = chainList->score;
    else
        {
	if (seqIsLower(query))
	    warn("Sequence %s is all lower case, and thus ignored. Use -unmask "
	         "flag to unmask lower case sequence.");
	}
    verbose(1, "%s (%d bases) score %2.0f\n", 
            query->name, query->size, bestScore);
    blatzWriteChains(bzp, chainList, query, indexList, f);
    chainFreeList(&chainList);
    dnaSeqFree(&query);
    }
carefulClose(&f);
}

static void loadAndAlignAll(struct bzp *bzp, 
        char *target, char *query, char *output)
/* blatz - Align genomic dna across species. */
{
struct dnaSeq *targetList = dnaLoadAll(target);
struct dnaLoad *queryDl = dnaLoadOpen(query);
struct blatzIndex *index;
bzpTime("loaded DNA");

verbose(2, "Loaded %d in %s, opened %s\n", slCount(targetList), target,
        query);
if (bzp->unmask)
    {
    struct dnaSeq *seq;
    for (seq = targetList; seq != NULL; seq = seq->next)
	toUpperN(seq->dna, seq->size);
    }
alignAll(bzp, targetList, queryDl, output);
}

int main(int argc, char *argv[])
/* Process command line. */
{
struct bzp *bzp = bzpDefault();

/* Do initialiazation. */
bzpTime(NULL);
dnaUtilOpen();
optionInit(&argc, argv, options);
if (argc != 4)
    usage(bzp);

/* Fill in parameters from command line options. */
bzpSetOptions(bzp);
loadAndAlignAll(bzp, argv[1], argv[2], argv[3]);

/* If you were a turtle you'd be home right now. */
return 0;
}
