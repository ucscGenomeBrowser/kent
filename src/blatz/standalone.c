/* Blatz standalone main module.  Handle command-line processing, load sequence 
 * and call alignment routines. */
/* Copyright 2005 Jim Kent.  All rights reserved. */

#include "common.h"
#include "hash.h"
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
#include "dynamic.h" // LX Sep 06 2005

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

static void alignAll(struct bzp *bzp, struct blatzIndex *indexList, 
        struct dnaLoad *queryDl, char *outFile)
/* Make up neighorhood index for queryList, and use it to scan
 * targetList.  Put output in outFile */
{
FILE *f = mustOpen(outFile, "w");
struct dnaSeq *query;

// LX BEG
int b, bend, printing; 
FILE *bedfp = NULL;
int j; 
int i;
// See if bed file output of the mask was requested
if(strcmp(bzp->dynaBedFile,"") != 0){
  bedfp = mustOpen(bzp->dynaBedFile, "w");
}
// Counts all the query-target hits encountered by the program inside the 
// loops of gapless.c
dynaHits = 0;
// Counts how many target and query positions reached the limit
dynaCountTarget = 0;
dynaCountQuery = 0;
// This is the limit used by the program, currently just bzp->dynaLimit(QT)
// but should be useful for scaling to sequence size
targetHitDLimit = VERY_LARGE_NUMBER; // perhaps unnecessary default
queryHitDLimit = VERY_LARGE_NUMBER; // perhaps unnecessary default
// LX END

while ((query = dnaLoadNext(queryDl)) != NULL)
    {
    double bestScore = 0;
    struct chain *chainList;
    // LX BEG
    if(bzp->dynaLimitQ<VERY_LARGE_NUMBER){
      queryHitDLimit = bzp->dynaLimitQ;
      // allocate zeroed memory for hit counters
      dynaCountQ = calloc(query->size, sizeof(COUNTER_TYPE));
    }
    // LX END
    if (bzp->unmask || bzp->rna)
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
    blatzWriteChains(bzp, &chainList, query, 
    	dnaLoadCurStart(queryDl), dnaLoadCurEnd(queryDl),
	dnaLoadCurSize(queryDl), indexList, f);
    // LX BEG
    // This prints the contents of the mask into the .bed file opened above
    if(bedfp != NULL){
      if(bzp->dynaLimitQ<VERY_LARGE_NUMBER){
        printing = 0;
        for(b=0;b<query->size;b++){
          if(dynaCountQ[b] > queryHitDLimit){
            if(printing == 0){
              printing = 1;
              fprintf(bedfp,"%s %d ",query->name,b);
            }
          }
          if(dynaCountQ[b] <= queryHitDLimit){
            if(printing == 1){
              printing = 0;
              bend = b-1;
              fprintf(bedfp,"%d\n",bend);
            }
          }
        }
      } else {
        fprintf(bedfp,"No dynamic masking data to print.\n");
      }
    }
    // LX END
    dnaSeqFree(&query);
    }
    // LX BEG
    // Statistics to print about how many hits were dropped (ignored)
    dynaDrops = dynaCountTarget + dynaCountQuery;
    dynaDropsPerc = (float)100*dynaDrops/dynaHits+0.5;
    printf("final chaining %d dynaDrops (%f%%) at T=%d Q=%d \n", dynaDrops, (double)dynaDropsPerc, targetHitDLimit, queryHitDLimit);
   // Free dynamic memory used for the sequence-length-dependent counter arrays
   if(dynaCountQ != NULL) free(dynaCountQ);
   if(bedfp != NULL){
     carefulClose(&bedfp);
   }
   //if(dynaWordCount != NULL) free(dynaWordCount); // ?! I don't know why 
   // this causes segmentation fault
   // LX END
carefulClose(&f);
}

static void loadAndAlignAll(struct bzp *bzp, 
        char *target, char *query, char *output)
/* blatz - Align genomic dna across species. */
{
// LX BEG
if(bzp->dynaWordCoverage > 0){
  dynaNumWords = (pow(4,bzp->weight)); // ?? check with Jim if this is correct
  dynaWordCount = calloc(dynaNumWords,sizeof(unsigned int));
  printf("Allocated word count table of size %d\n",dynaNumWords);
  dynaWordLimit = bzp->dynaWordCoverage; // cheating, should be more like:
  //dynaWordLimit = bzp->dynaWordCoverage*dynaSequenceSize/dynaNumWords;
  printf("Set word limit to  %d\n",dynaWordLimit);
}
// LX END
struct dnaLoad *queryDl = dnaLoadOpen(query);
struct dnaLoad *targetDl = dnaLoadOpen(target);
struct blatzIndex *indexList = blatzIndexDl(targetDl, bzp->weight, bzp->unmask);
bzpTime("loaded and indexed target DNA");

verbose(2, "Loaded %d in %s, opened %s\n", slCount(indexList), target,
        query);
alignAll(bzp, indexList, queryDl, output);
}

int main(int argc, char *argv[])
/* Process command line. */
{
struct bzp *bzp = bzpDefault();

/* Do initialization. */
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
