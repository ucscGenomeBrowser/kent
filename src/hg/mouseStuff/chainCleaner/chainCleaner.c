/* Michael Hiller, 2015, MPI-CBG/MPI-PKS */
/* This code is based on the UCSC kent source code, with the UCSC copyright below */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */



/*   
Definitions: 

A break is a case where a chain is broken into >=2 pieces in the nets by a fill block of a higher scoring chain.
breaking chain: chain that contains a local alignment block that breaks another lower-scoring chain in the nets into two or more pieces
broken chain: lower-scoring chain that is broken into two or more pieces
suspect: (also called "chain-breaking alignment" or CBA in the paper) the local alignment block(s) in the higher-scoring chain that causes the broken chain to be broken into pieces. 
         This block(s) is suspected to be a random or paralogous alignment. The breaking chain can have more than one suspect. 

Example:
X = fill (aligning region); - = gap (unaligning region)

breaking chain: XXXXXXXXXXXXXXX-----------------------XX-------------------XXXXXXXXXXXXXXXXXXXXXXXXX
broken chain:                   XXXXXXXXXXXXXXXXX----------XXXXXXXXXXXXX

Nets:
depth 1:        XXXXXXXXXXXXXXX-----------------------XX-------------------XXXXXXXXXXXXXXXXXXXXXXXXX
depth 2:                        XXXXXXXXXXXXXXXXX          XXXXXXXXXXXXX


Most important data structures:
breaking chain: XXXXXXXXXXXXXXX-----------------------XX-------------------XXXXXXXXXXXXXXXXXXXXXXXXX
broken chain:                   XXXXXXXXXXXXXXXXX----------XXXXXXXXXXXXX
fillGapInfo                    <--------------------->  <----------------->
breakInfo                      <------------------------------------------>

*/

#include <limits.h>
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "localmem.h"
#include "options.h"
#include "chainNet.h"
#include "chain.h"
#include "rbTree.h"
#include "obscure.h"
#include "gapCalc.h"
#include "dnaseq.h"
#include "chainConnect.h"
#include "twoBit.h"
#include "axt.h"
#include "nib.h"
#include "dystring.h"
#include "basicBed.h"
#include "genomeRangeTree.h"

/* experimental. Quickly just run it on the chain with this ID and a suspect with these coords. */
char *onlyThisChr;
int onlyThisStart;
int onlyThisEnd;

/* Information about a net fill region at depth>1 and its enclosing gap. */
struct fillGapInfo
    {
    struct fillGapInfo *next;
    int depth;                    /* depth of the fill region */
    int gapDepth;                 /* depth of the enclosing gap region */
    int chainId;                  /* chainId of the fill */
    int parentChainId;            /* ID of parent chain */
    char *chrom;                  /* reference chrom */
    int fillStart, fillEnd;       /* start and end of the fill in the reference assembly */
    int gapStart, gapEnd;         /* start and end of the gap in the reference assembly */
    char *gapInfo;                /* temporarily used: tab-separated string containing gapStart, gapEnd, parentChainId. After we found a broken chain, chop this string and fill gapStart/gapEnd/parentChainId */
    struct chain *chain;          /* pointer to the chain */
    struct chain *parentChain;    /* pointer to the parent chain */
    };


/* Information a break. That is the left and right aligning (fill) region of the broken chain and the suspect in the breaking chain. */
struct breakInfo
    {
    struct breakInfo *next;
    struct breakInfo *prev;       /* double linked list */
    int depth;                    /* depth of the fill region */
    int chainId;                  /* chainId of the broken chain (the one with the fill region) */
    int parentChainId;            /* ID of parent chain, i.e. the breaking chain */
    char *chrom;                  /* reference chrom */
    int LfillStart, LfillEnd;     /* start and end of the left fill of the broken chain in the reference assembly */
    int RfillStart, RfillEnd;     /* start and end of the right fill in the reference assembly */
    int LgapStart, LgapEnd;       /* start and end of the left gap in the reference assembly */
    int RgapStart, RgapEnd;       /* start and end of the right gap in the reference assembly */
    int suspectStart, suspectEnd; /* start and end of the suspect (the aligning blocks between the 2 gaps in the breaking chain) in the reference assembly */
    struct chain *chain;          /* pointer to the chain */
    struct chain *parentChain;    /* pointer to the parent chain */
    };

/* conditions for removing a suspect */
/* score of the left and right fill of the brokenChain / suspect score must be at least as high this ratio to remove the suspect */
double LRfoldThreshold = 2.5;
/* brokenChain score / suspect score is at least this ratio to remove the suspect */
double foldThreshold = 0;
/* do not remove suspect if it has more than that many bases in the target in aligning blocks (suspect is not a random ali) */
double maxSuspectBases = INT_MAX;
/* do not remove suspect if the suspect subChain has a score higher than this threshold (suspect is not a random ali) */
double maxSuspectScore = 100000;
/* do not remove suspect if the score of the broken chain is less than this threshold (broken chain is not a high-scoring alignment) */
double minBrokenChainScore = 50000;
/* threshold for min size of left/right gap. If lower, do not remove suspect (too close to one side of the rest of the breaking chain) */
int minLRGapSize = 0;
/* flag: if set, do not test if pairs of suspects can be removed */
boolean doPairs = FALSE;
/* left/right score ratio applied to pairs. Is only used with -doPairs */
double LRfoldThresholdPairs = 10;
/* only consider pairs of suspects where the distance between the end of the upstream suspect and the start of the downstream suspect is at most that many bp */
int maxPairDistance = 10000;

/* flag: if set, output 'newChainID{tab}breakingChainID' to this file. Gives a dictionary of the new IDs of chains representing removed suspects and the chain ID of the breaking chain that had the suspect before */
char *newChainIDDict = NULL;
FILE *newChainIDDictFile = NULL;

/* flag: if set, output all the data for suspects to this file. If set, we do not clean any suspect as this would lead to updating the suspect values (updating the L/R fill region) */
char *suspectDataFile = NULL;
FILE *suspectDataFilePointer = NULL;
int suspectID = 0;   /* used to assign a unique ID to each suspect */

/* final output file for the chains that will contain
    - the untouched chains
    - the broken chains (untouched unless they contain suspects themself)
    - the new breaking chains (potentially without some suspects)
    - the new chains representing the removed suspects
*/
FILE *finalChainOutFile = NULL;
/* final output bed file, containing the coordinates of the removed suspects + the subchain scores and the foldRatio */
FILE *suspectsRemovedOutBedFile = NULL;
/* counts how often we see a chain ID at depth > 1 in the net file */
struct hash *chainId2Count = NULL;
/* hash keyed by chainId that holds a list of fillGapInfo structs for all fills of that chain at depth>1 */
struct hash *fillGapInfoHash = NULL;

/* size of the depth2* arrays. 64 should be more than enough */
int maxNetDepth = 64;
/* tab-separated string containing start, end of the current gap at each depth (array index) and the chainId of the chain having the gap */
struct dyString **depth2gap = NULL;
/* chain ID that has the fill at each depth (array index) */
int *depth2chainId = NULL;  

/* genomeRangeTree containing all net aligning blocks (can span gap that are not filled by children) */
struct genomeRangeTree *rangeTreeAliBlocks = NULL;

/* keyed by the breaking chain ID that contains one more more suspects. Contains list of breaks for this chain. */
struct hash *breakHash = NULL;
/* keyed by all chainIds that are either breaking or broken chains. Contains TRUE */
struct hash *chainId2IsOfInterest = NULL;
/* keyed by all chainIds that are modified breaking chains and thus needs rescoring before writing. Contains TRUE */
struct hash *chainId2NeedsRescoring = NULL;
/* pointer to chain keyed by chainId */
struct hash *chainId2chain = NULL;
int maxChainId = -1;

/* for debugging */
boolean debug = FALSE; 
FILE *suspectChainFile = NULL;
FILE *brokenChainLfillChainFile = NULL;
FILE *brokenChainRfillChainFile = NULL;
FILE *brokenChainfillChainFile = NULL;
FILE *suspectFillBedFile = NULL;


/* for chain scoring */
char *gapFileName = NULL;
struct gapCalc *gapCalc = NULL;   /* Gap scoring scheme to use. */
struct axtScoreScheme *scoreScheme = NULL;

/* for the target and query DNA seqs */
char *tNibDir = NULL;    /* t and q nib or 2bit file */
char *qNibDir = NULL;
boolean tIsTwoBit;       /* flags */
boolean qIsTwoBit;
/* chrom.sizes for target and query. Only needed if the user does not provide an input net */
char *tSizes = NULL;    
char *qSizes = NULL;
/* contains pointers to the open 2bit files */
struct hash *twoBitFileHash = NULL;
/* hash keyed by target chromname, holding pointers to the seq */
struct hash *tSeqHash = NULL;
/* hash keyed by query chromname, holding pointers to the seq */
struct hash *qSeqHash = NULL;
/* hash keyed by query chromname, will hold a pointer to the reverse complemented seq. But we fill it on demand */
struct hash *qSeqMinusStrandHash = NULL;

/* input / output files */
char *inChainFile = NULL;
char *outChainFile = NULL;
char *inNetFile = NULL;
char *outRemovedSuspectsFile = NULL;

/* Explain usage and exit. */
void usage() {
  errAbort(
  "chainCleaner - Remove chain-breaking alignments from chains that break nested chains.\n"
  "\n"
  "NOTATION: The \"breaking chain\" contains a local alignment block (called \"chain-breaking alignment\" (CBA) or \"suspect\") that breaks a nested chain (\"broken chain\") into two nets.\n"
  "\n"
  "usage:\n"
  "   chainCleaner in.chain tNibDir qNibDir out.chain out.bed -net=in.net \n"
  " OR \n"
  "   chainCleaner in.chain tNibDir qNibDir out.chain out.bed -tSizes=/dir/to/target/chrom.sizes -qSizes=/dir/to/query/chrom.sizes \n"
  " First option:   you have netted the chains and specify the net file via -net=netFile\n"
  " Second option:  you have not netted the chains. Then chainCleaner will net them. In this case, you must specify the chrom.sizes file for the target and query with -tSizes/-qSizes\n"
  " tNibDir/qNibDir are either directories with nib files, or the name of a .2bit file\n\n"
  "\n"
  "output:\n"
  "   out.chain      output file in chain format containing the untouched chains, the original broken chain and the modified breaking chains. NOTE: this file is chainSort-ed.\n"
  "   out.bed        output file in bed format containing the coords and information about the removed chain-breaking alignments.\n"
  "\n"
  "Most important options for deciding which chain-breaking alignments (CBA) to remove:\n"
  "   -LRfoldThreshold=N        threshold for removing local alignment blocks if the score of the left and right fill of brokenChain / CBA score is at least this fold threshold. Default %1.1f\n"
  "   -doPairs                  flag: if set, do test if pairs of CBAs can be removed\n"
  "   -LRfoldThresholdPairs=N   threshold for removing local alignment blocks if the score of the left and right fill of brokenChain / CBA score is at least this fold threshold. Default %1.1f\n"
  "   -maxPairDistance=N        only consider pairs of CBAs where the distance between the end of the upstream CBA and the start of the downstream CBA is at most that many bp (Default %d)\n"
  "\n"
  "   -scoreScheme=fileName       Read the scoring matrix from a blastz-format file\n"
  "   -linearGap=<medium|loose|filename> Specify type of linearGap to use.\n"
  "              *Must* specify this argument to one of these choices.\n"
  "              loose is chicken/human linear gap costs.\n"
  "              medium is mouse/human linear gap costs.\n"
  "              Or specify a piecewise linearGap tab delimited file.\n"
  "   sample linearGap file (loose)\n"
  "%s"
  "\n"
  "\n"
  "Other options for deciding which suspects to remove: \n"
  "   -foldThreshold=N          threshold for removing local alignment blocks if the brokenChain score / suspect score is at least this fold threshold. Default %1.1f\n"
  "   -maxSuspectBases=N        threshold for number of target bases in aligning blocks of the suspect subChain. If higher, do not remove suspect. Default %d\n"
  "   -maxSuspectScore=N        threshold for score of suspect subChain. If higher, do not remove suspect. Default %d\n"
  "   -minBrokenChainScore=N    threshold for minimum score of the entire broken chain. If the broken chain scores lower, it is less likely to be a real alignment and we will not remove the suspect. Default %d\n"
  "   -minLRGapSize=N           threshold for min size of left/right gap (how far the suspect is away from other blocks in the breaking chain). If lower, do not remove suspect (suspect to close to left or right part of breaking chain). Default %d\n"
  "\n"
  "\n"
  "Debug and testing options: \n"
  "   -newChainIDDict=fileName  output 'newChainID{tab}breakingChainID' to this file. Gives a dictionary of the new IDs of chains representing removed suspects and the chain ID of the breaking chain that had the suspect before.\n"
  "   -suspectDataFile=fileName output all the data for suspects to this file in bed format. If set, we do not clean any suspect as this would lead to updating the suspect values (updating the L/R fill region).\n"
  "   -debug                    produces output chain files with the suspect and broken chains, and a bed file with information about all possible suspects. For debugging.\n"
  , LRfoldThreshold, LRfoldThresholdPairs, maxPairDistance, gapCalcSampleFileContents(), foldThreshold, (int)maxSuspectBases, (int)maxSuspectScore, (int)minBrokenChainScore, minLRGapSize);
}

/* command line options */
struct optionSpec options[] = {
   {"net", OPTION_STRING},
   {"tSizes", OPTION_STRING},
   {"qSizes", OPTION_STRING},
   {"scoreScheme", OPTION_STRING},
   {"linearGap", OPTION_STRING},
   {"debug", OPTION_BOOLEAN},
   {"foldThreshold", OPTION_DOUBLE},
   {"LRfoldThreshold", OPTION_DOUBLE},
   {"LRfoldThresholdPairs", OPTION_DOUBLE},
   {"maxSuspectBases", OPTION_DOUBLE}, 
   {"maxSuspectScore", OPTION_DOUBLE},
   {"minBrokenChainScore", OPTION_DOUBLE},
   {"minLRGapSize", OPTION_INT},
   {"doPairs", OPTION_BOOLEAN},
   {"maxPairDistance", OPTION_INT},
   {"newChainIDDict", OPTION_STRING},
   {"suspectDataFile", OPTION_STRING},
   {"onlyThisChr", OPTION_STRING},
   {"onlyThisStart", OPTION_INT},
   {"onlyThisEnd", OPTION_INT},
   {NULL, 0},
};



/* ################################################################################### */
/*   small helper functions to deal with integers as keys for a hash                   */
/* ################################################################################### */

/****************************************************************
   Increment integer value in hash, where key is a int
****************************************************************/
void hashIncInt_IntKey(struct hash *hash, int key) {
   char keyBuf [30];
   safef(keyBuf, sizeof(keyBuf), "%d", key);
   hashIncInt(hash, keyBuf);
}

/****************************************************************
   add a key to the hash (with value TRUE), where key is a int
****************************************************************/
void hashAddIntTrue(struct hash *hash, int key) {
   char keyBuf [30];
   safef(keyBuf, sizeof(keyBuf), "%d", key);
   if (hashLookup(hash, keyBuf) != NULL)
      return;
   hashAddInt(hash, keyBuf, TRUE);
}

/****************************************************************
   add a key-value pair to the hash, where key is a int
****************************************************************/
void hashAddIntKey(struct hash *hash, int key, void *val) {
   char keyBuf [30];
   safef(keyBuf, sizeof(keyBuf), "%d", key);
   hashAdd(hash, keyBuf, val);
}

/****************************************************************
   returns TRUE if chainID is a breaking or broken chain
****************************************************************/
boolean chainIsOfInterest(int chainId) {
   char keyBuf [30];
   safef(keyBuf, sizeof(keyBuf), "%d", chainId);
   if (hashFindVal(chainId2IsOfInterest, keyBuf) == NULL)
      return FALSE;
   else
      return TRUE;
}

/****************************************************************
   returns value of hash, where key is a int
****************************************************************/
void *hashFindValIntKey(struct hash *hash, int key) {
   char keyBuf [30];
   safef(keyBuf, sizeof(keyBuf), "%d", key);
   return hashFindVal(hash, keyBuf);
}



/****************************************************************
   returns chain pointer from the hash
****************************************************************/
struct chain *getChainPointerFromHash(int chainId) 
{
   char keyBuf [30];
   safef(keyBuf, sizeof(keyBuf), "%d", chainId);
   return hashFindVal(chainId2chain, keyBuf);
}


/* ################################################################################### */
/* functions for verbose output                                                        */
/* ################################################################################### */

/****************************************************************
   Print out memory used and other stuff from linux. Taken from chainNet.c
****************************************************************/
void printMem()
{
struct lineFile *lf = lineFileOpen("/proc/self/stat", TRUE);
char *line, *words[50];
int wordCount;
if (lineFileNext(lf, &line, NULL))
    {
    wordCount = chopLine(line, words);
    if (wordCount >= 23)
        verbose(1, "memory usage %s, utime %s s/100, stime %s\n", 
      words[22], words[13], words[14]);
    }
lineFileClose(&lf);
}

/****************************************************************
   verbose output of a single fillGapInfo
****************************************************************/
void printSingleFillGap(struct fillGapInfo *fillGap) {
   char *gapInfo;
   char *words[5];
   int wordCount;

   /* lets split gapInfo string into its components. But clone it before, as we will  */
   gapInfo = cloneString(fillGap->gapInfo);
   wordCount = chopLine(gapInfo, words);
   if (wordCount != 5)
      errAbort("Expecting 5 tab-separated words in %s but can parse only %d\n", fillGap->gapInfo, wordCount);
   verbose(3, "\t\tfillGap: depth %d  fill coords:  %s:%d-%d   enclosing gap: %s:%s-%s   (breaking chainID %s, broken chainID %d)\n", fillGap->depth, fillGap->chrom, fillGap->fillStart, fillGap->fillEnd, words[0], words[1], words[2], words[3], fillGap->chainId);
   freeMem(gapInfo);
}

/****************************************************************
   verbose output about how often we see each chain at depth > 1 together with the fillGapInfo
****************************************************************/
void printFillGapInfoList(struct hashEl *el) {
   struct fillGapInfo *fillGapList = NULL, *fillGap;
   
   verbose(3, "\tchainID %s seen %d times in net file\n", el->name, (int)ptToInt(el->val));
   if ((int)ptToInt(el->val) == 1) return;

   fillGapList = hashMustFindVal(fillGapInfoHash, el->name);
   verbose(3, "\t\t%d Entries in fillGapList\n", slCount(fillGapList));
   for (fillGap = fillGapList; fillGap != NULL; fillGap = fillGap->next) 
      printSingleFillGap(fillGap);
   
   /* sanity check */
   if (slCount(fillGapList) != (int)ptToInt(el->val))
      errAbort("ERROR: chainId %s has a different count vs entries in fillGapList  %d vs %d \n", el->name,slCount(fillGapList),(int)ptToInt(el->val));
}

/****************************************************************
   print single break information
****************************************************************/
void printSingleBreak(struct breakInfo *breakP) {
   if (breakP == NULL)
      return;
   verbose(3, "\t\t\tbreak: depth %d  breaking chainID %6d  broken chainID %6d  chrom %s  Lfill %d-%d  Rfill %d-%d  suspect %d-%d  (Rgap %d-%d  Lgap %d-%d)\n", 
      breakP->depth, breakP->parentChainId, breakP->chainId, breakP->chrom, 
      breakP->LfillStart, breakP->LfillEnd, breakP->RfillStart, breakP->RfillEnd, breakP->suspectStart, breakP->suspectEnd,
      breakP->LgapStart, breakP->LgapEnd, breakP->RgapStart, breakP->RgapEnd);
}


/****************************************************************
   loop over all entries in breakHash (i.e. loop over all breaking chains)
   and print every break
****************************************************************/
void printAllBreaks() {
   struct breakInfo *breakP = NULL, *breakList = NULL; 
   struct hashEl *hel, *helList = hashElListHash(breakHash);
   
   /* hel is a pointer to a list of breaks for one parentChain */
   for (hel = helList; hel != NULL; hel = hel->next)  {
      breakList = hel->val;
      if (breakList == NULL) 
         errAbort("ERROR: breakList for chain %6d is NULL\n", breakList->parentChainId); 
      verbose(3, "\tlist of breaks for breaking chainID %d\n", breakList->parentChainId); 
      for (breakP = breakList; breakP != NULL; breakP = breakP->next) 
         printSingleBreak(breakP);
   }
}



/* ################################################################################### */
/* functions for loading and getting DNA seqs                                          */
/* ################################################################################### */

/****************************************************************
   Load sequence and add to hash, unless it is already loaded.  Do not reverse complement.
****************************************************************/
void loadSeq(char *seqPath, boolean isTwoBit, char *newName, struct hash *hash) {
   struct dnaSeq *seq;

   if (hashFindVal(hash, newName) == NULL)  {
      /* need to load */
      if (isTwoBit) {
         struct twoBitFile *tbf = hashMustFindVal(twoBitFileHash, seqPath);
         seq = twoBitReadSeqFrag(tbf, newName, 0, 0);
         verbose(3, "\t\t\tLoaded %d bases of %s from %s\n", seq->size, newName, seqPath);
      } else {
         char fileName[512];
         snprintf(fileName, sizeof(fileName), "%s/%s.nib", seqPath, newName);
         seq = nibLoadAllMasked(NIB_MASK_MIXED, fileName);
         verbose(3, "\t\t\tLoaded %d bases in %s\n", seq->size, fileName);
      }
      hashAdd(hash, newName, seq);
   }
}

/****************************************************************
   will be called for every key in the chainId2IsOfInterest hash. 
   Loads the t and q seq by calling loadSeq
****************************************************************/
void loadTandQSeqs(struct hashEl *el) {
   struct chain *chain = NULL;
   
   verbose(5, "\t\tload t and q seq for chain Id %s\n", el->name);
   chain = hashFindVal(chainId2chain, el->name);
   if (chain == NULL) 
      errAbort("ERROR: cannot get chain with Id %s from chainId2chain hash\n", el->name);

   loadSeq(tNibDir, tIsTwoBit, chain->tName, tSeqHash);
   loadSeq(qNibDir, qIsTwoBit, chain->qName, qSeqHash);
}


/****************************************************************
   return dnaSeq struct from hash 
   abort if the dnaSeq is not in the hash
   if strand is - (must be a query seq), look up in qSeqMinusStrandHash and fill if not already present 
****************************************************************/
struct dnaSeq *getSeqFromHash(char *chrom, char strand, struct hash *hash) {
   struct dnaSeq *seq = hashFindVal(hash, chrom);

   if (seq == NULL)
      errAbort("ERROR: seq %s is not loaded in hash\n", chrom);

   if (strand == '+') {
      verbose(6, "\t\t\t\t\tgetSeqFromHash: load %s %c from hash\n", chrom, strand); fflush(stdout);
      return seq;
   } else {
      /* must be query seq */
      /* check if we have this seq in the qSeqMinusStrandHash */
      struct dnaSeq *rcSeq = hashFindVal(qSeqMinusStrandHash, chrom);
      if (rcSeq != NULL) {
         verbose(6, "\t\t\t\t\tgetSeqFromHash: load %s %c from revcomp hash\n", chrom, strand); fflush(stdout);
         return rcSeq;
      } else {
         /* get rc seq, store in hash and return */
         rcSeq = cloneDnaSeq(seq);
         reverseComplement(rcSeq->dna, rcSeq->size);
         hashAdd(qSeqMinusStrandHash, chrom, rcSeq);
         verbose(6, "\t\t\t\t\tgetSeqFromHash: add %s %c to revcomp hash\n", chrom, strand); fflush(stdout);
         return rcSeq;
      }
   }
   return NULL;
}


/****************************************************************
   free all dnaSeq struct from hash 
****************************************************************/
void freeDnaSeqHash(struct hash *hash) {
   struct hashEl *hel, *helList = hashElListHash(hash);
   
   for (hel = helList; hel != NULL; hel = hel->next)  {
      freeDnaSeq(hel->val);
   }
}


/* ################################################################################### */
/* functions for calculating chain scores                                              */
/* ################################################################################### */

/****************************************************************
  Calculate chain score locally. 
  That means, we reset score = 0 if score < 0 and return the max of the score that we reach for this chain.
  This is nearly identical to chainCalcScore() from chainConnect.c just a few modifications for the local score. 
****************************************************************/
double chainCalcScoreLocal(struct chain *chain, struct axtScoreScheme *ss, struct gapCalc *gapCalc, struct dnaSeq *query, struct dnaSeq *target) {
   struct cBlock *b1, *b2;
   double score = 0;
   double maxScore = 0;
   for (b1 = chain->blockList; b1 != NULL; b1 = b2) {
      score += chainScoreBlock(query->dna + b1->qStart, target->dna + b1->tStart, b1->tEnd - b1->tStart, ss->matrix);
//      verbose(2, "  score %f  (max %f)\n ", score,maxScore);

      if (score > maxScore) 
         maxScore = score; 

      b2 = b1->next;
      if (b2 != NULL) {
         score -=  gapCalcCost(gapCalc, b2->qStart - b1->qEnd, b2->tStart - b1->tEnd);
//         verbose(2, "     after gap: score %f  (max %f)\n ", score,maxScore);
         if (score < 0) 
            score = 0;  
      }
   }
   return maxScore;
}


/****************************************************************
   calculate the score of the given chain, both the local (always >0) and the global score
   sets chain->score to the global score (needed for final rescoring of the breaking chains)
   returns both global and local score in the double pointers
****************************************************************/
double getChainScore (struct chain *chain, double *globalScore, double *localScore) {
   struct dnaSeq *qSeq = NULL, *tSeq = NULL;
//   printf("\tcalc score for chain with ID %d\n", chain->id);fflush(stdout);

   /* load the seqs */
   qSeq = getSeqFromHash(chain->qName, chain->qStrand, qSeqHash);
   tSeq = getSeqFromHash(chain->tName, '+', tSeqHash);

   chain->score = chainCalcScore(chain, scoreScheme, gapCalc, qSeq, tSeq);
   *globalScore = chain->score;
   *localScore = chainCalcScoreLocal(chain, scoreScheme, gapCalc, qSeq, tSeq);
   return chain->score;
}



/* ################################################################################### */
/* functions for reading, writing chains and removing blocks from chains               */
/* ################################################################################### */


/****************************************************************
   Read in all chains. 
   Write those immediately to outFile that are neither breaking nor broken chains
****************************************************************/
void readChainsOfInterest(char *chainFile) {
   struct lineFile *lf = NULL;
   FILE *f = NULL; 
   struct chain *chainList = NULL, *chain;

   lf = lineFileOpen(chainFile, TRUE);
   lineFileSetMetaDataOutput(lf, finalChainOutFile);
   if (debug)
      f = mustOpen("chainsOfInterest.chain", "w");
   chainId2chain = newHash(0); 

   while ((chain = chainRead(lf)) != NULL) {
      /* need to keep track of the highest chain Id for later when we create new chains representing the removed blocks */
      if (maxChainId < chain->id)
         maxChainId = chain->id;
      /* experimental. Quickly just run it on the chain with this ID and a suspect with these coords. */
      if (onlyThisChr != NULL && (! sameString(onlyThisChr, chain->tName))) 
         continue;

      if (chainIsOfInterest(chain->id)) {
         verbose(4, "\t\tread chain ID %d  --> is a breaking or broken chain (chainOfInterest)\n", chain->id);
         slAddHead(&chainList, chain);
         /* also store a pointer to this chain in the hash keyed by chainId */
         hashAddIntKey(chainId2chain, chain->id, chain);
         if (debug)
            chainWrite(chain, f);
      } else {
         verbose(4, "\t\tread chain ID %d\n", chain->id);
         chainWrite(chain, finalChainOutFile);
      }
   }
   lineFileClose(&lf);
   if (debug)
      carefulClose(&f);
}


/****************************************************************
   write all the (new) breaking and broken chains to the final chain output file
   Will be called for every entry in the chainId2IsOfInterest hash
****************************************************************/
void writeAndFreeChainsOfInterest(struct hashEl *el) {
   struct chain *chain = NULL;
   double globalScore, localScore; 
   
   verbose(4, "\t\twrite breaking or broken chain with ID %s to final chain output file ... ", el->name); 
   chain = hashFindVal(chainId2chain, el->name);
   if (chain == NULL) 
      errAbort("\nERROR: cannot get chain with Id %s from chainId2chain hash\n", el->name);

   if (hashFindVal(chainId2NeedsRescoring, el->name) != NULL) {
      verbose(4, "\n\t\t\trecompute score for this modified chain:   old score %6d   ", (int)chain->score); 
      getChainScore(chain, &globalScore, &localScore);
      verbose(4, "new score %6d\n\t\t", (int)chain->score); 
   }

   chainWrite(chain, finalChainOutFile);
   chainFree(&chain);
   verbose(4, "DONE\n"); 
}


/****************************************************************
   Remove blocks from the chain between target coords tStart and tEnd (the suspect coords). 
****************************************************************/
void chainRemoveBlocks(struct chain *chain, int tStart, int tEnd) {
   struct cBlock *curBlock = NULL, *nextBlock = NULL, *firstBlock = NULL, *lastBlock = NULL;

   /* find the last block upstream of tStart */
   firstBlock = chain->blockList;
   for (curBlock = chain->blockList; curBlock != NULL; curBlock = curBlock->next){
      if (curBlock->tStart >= tStart)
         break;
      firstBlock = curBlock;
   }
   if (firstBlock == curBlock) 
      errAbort("ERROR in chainRemoveBlocks: boundaries imply that we remove the first block of chain Id %d (tStart %d - tEnd %d)\n", chain->id, tStart, tEnd);

   /* now find the first block downstream of tEnd */
   for (curBlock = firstBlock->next; curBlock != NULL; curBlock = curBlock->next){
      if (curBlock->tStart >= tEnd)
         break;
   }
   lastBlock = curBlock;
   if (lastBlock == NULL) 
      errAbort("ERROR in chainRemoveBlocks: boundaries imply that we remove the last block of chain Id %d (tStart %d - tEnd %d)\n", chain->id, tStart, tEnd);
   
//   printf("found block upstream %d - %d  and downstream %d - %d\n", firstBlock->tStart, firstBlock->tEnd, lastBlock->tStart, lastBlock->tEnd); fflush(stdout);


   /* delete the blocks in the tStart - tEnd region */
   curBlock = firstBlock->next;
   for (;;){
      nextBlock = curBlock->next;
//      printf("\tdel block %d - %d  \n", curBlock->tStart, curBlock->tEnd); fflush(stdout);
      freeMem(curBlock);
      curBlock = nextBlock;
      if (curBlock == lastBlock)
         break;
   }
   /* firstBlock must now point to lastBlock */
   firstBlock->next = lastBlock;
}




/* ################################################################################### */
/* functions for getting aligning blocks from the nets                                 */
/* and adding them to genomeRangTree                                                   */
/* ################################################################################### */

/****************************************************************
   Exact copy from netChainSubset.c 
   Find next in list that has a non-empty child.   
****************************************************************/
struct cnFill *nextGapWithInsert(struct cnFill *gapList)
{
struct cnFill *gap;
for (gap = gapList; gap != NULL; gap = gap->next)
    {
    if (gap->children != NULL)
        break;
    }
return gap;
}


/****************************************************************
   create a bed from the coordinates and add it to the genomeRangeTree
****************************************************************/
void addRangeToGenomeRangeTree(char *tName, int start, int end, int chainId) {
   struct bed4 *bed = NULL;
   char chainIdBuf[30];

   AllocVar(bed);
   bed->chrom = cloneString(tName);
   bed->chromStart = start;
   bed->chromEnd = end; 
   safef(chainIdBuf, sizeof(chainIdBuf), "%d", chainId);
   bed->name = cloneString(chainIdBuf);
   genomeRangeTreeAddValList(rangeTreeAliBlocks, bed->chrom, bed->chromStart, bed->chromEnd, bed);
}

/****************************************************************
   modified function splitWrite() from netChainSubset.c 
   adds alignment blocks of a net that do not have children (NOTE: they can span gaps) to the genomeRangeTree
   
   X = fill, - = gap
   XXXXXXXX---XXXXXX-------------------XXXXXXXXXX
                     XXXX-----XXX--XXX
                          XXX    
   gives the following ranges that will be added to genomeRangeTree
   <---------------> <--> <-> <------> <-------->
   NOTE: The first and the fourth contain gaps that are not filled by children
****************************************************************/
void addAliBlocksToGenomeRangeTree(struct cnFill *fill, char *tName) {
   int tStart = fill->tStart;
   struct cnFill *child = fill->children;

   for (;;) {
      child = nextGapWithInsert(child);
      if (child == NULL)
         break;

      verbose(3, "\t\tadd alignment block to genomeRangeTree: %s\t%d\t%d\t%d\n", tName, tStart, child->tStart, fill->chainId);
      addRangeToGenomeRangeTree(tName, tStart, child->tStart, fill->chainId);
      tStart = child->tStart + child->tSize;
      child = child->next;
   }
   verbose(3, "\t\tadd alignment block to genomeRangeTree: %s\t%d\t%d\t%d\n", tName, tStart, fill->tStart + fill->tSize, fill->chainId);
   addRangeToGenomeRangeTree(tName, tStart, fill->tStart + fill->tSize, fill->chainId);
}


/****************************************************************
   modified function rConvert() from netChainSubset.c 
   It calls addAliBlocksToGenomeRangeTree() to write the aligning blocks for this fill. 
   Then recursively calls itself to handle the children. 
****************************************************************/
void rConvert(struct cnFill *fillList, char *tName) {
   struct cnFill *fill;

   for (fill = fillList; fill != NULL; fill = fill->next) {
      if (fill->chainId) 
         addAliBlocksToGenomeRangeTree(fill, tName);
      if (fill->children) 
         rConvert(fill->children, tName);
   }
}


/* ################################################################################### */
/* functions for dealing with fills/gaps from the nets and with breaks                 */
/* they represent the core functionality                                               */
/* ################################################################################### */

/****************************************************************
   Recursively parse the fill list. 
   Keep track of the chainId that has the fill region at each depth, the start-end+chainId of each gap and fill the fillGapInfo struct for every fill that is at depth > 1
   Also count how often we see each chain at depth > 1 --> we only care for those that we see more than once
****************************************************************/
void parseFill(struct cnFill *fillList, int depth, char *tName) {
   struct cnFill *fill;
   struct fillGapInfo *fillGapList = NULL, *fillGap = NULL;
   int i;

   for (fill = fillList; fill != NULL; fill = fill->next) {
      char *type = (fill->chainId ? "fill" : "gap");

      /* fill */
      if (sameString(type, "fill")) {
         /* update chain ID if a fill */
         depth2chainId[depth] = fill->chainId;

         /* new fillGap if depth > 1 */
         if (depth > 1) {
            /* increase chainId2Counter */
            hashIncInt_IntKey(chainId2Count, fill->chainId);

            /* save the information about the fill and its enclosing gap */
            AllocVar(fillGap);
            fillGap->depth = depth;
            fillGap->chainId = fill->chainId;
            fillGap->chrom = cloneString(tName);
            fillGap->fillStart = fill->tStart;
            fillGap->fillEnd = fill->tStart+fill->tSize;
            fillGap->gapInfo = cloneString(depth2gap[depth-1]->string);

            /* if the chainId does not have a list of fillGap blocks, create one and add to hash */
            fillGapList = hashFindValIntKey(fillGapInfoHash, fill->chainId);
            if (fillGapList == NULL)  {
               slAddHead(&fillGapList, fillGap);
               hashAddIntKey(fillGapInfoHash, fill->chainId, fillGapList);
            } else {
            /* otherwise just add to this fillGap to the end of the list */
               slAddTail(&fillGapList, fillGap);
            }

            /* verbose output */
            if (verboseLevel() >= 4) {
               verbose(4, "\t");
               for (i=0; i<depth; i++) verbose(4, "  ");
               verbose(4, "new fillGapInfo: depth %d   fill coords:  %s:%d-%d  chainID %d    enclosing gap: %s       [chain ID %d occurred so far %d times in net]\n", 
                  depth, tName, fill->tStart, fill->tStart+fill->tSize, fill->chainId, depth2gap[depth-1]->string, fill->chainId,ptToInt(hashFindValIntKey(chainId2Count, fill->chainId)));
            }
        } else {
            /* verbose output */
            if (verboseLevel() >= 4) {
               verbose(4, "\t");
                 for (i=0; i<depth; i++) verbose(4, "  ");
               verbose(4, "new fill at depth %d:  fill coords:  %s:%d-%d  chainID %d\n", depth, tName, fill->tStart, fill->tStart+fill->tSize, fill->chainId);
            }
         }

      /* update the gap information at the current depth */
      }else if (sameString(type, "gap")) {
         dyStringClear(depth2gap[depth]);
         dyStringPrintf(depth2gap[depth], "%s\t%d\t%d\t%d\t%d", tName, fill->tStart, fill->tStart+fill->tSize, depth2chainId[depth-1], depth);

         /* verbose output */
         if (verboseLevel() >= 4) {
              verbose(4, "\t");
            for (i=0; i<depth; i++) verbose(4, "  ");
            verbose(4, "current gap:  depth %d   gap coords: %s:%d-%d  chainID %d\n", depth, tName, fill->tStart, fill->tStart+fill->tSize, depth2chainId[depth-1]);
         }
         
      /* should not happen if we are reading a proper net file */
      } else {
         errAbort("ERROR: current entry in fillList is neither type fill nor gap: %s\n", type);
      }

      /* recursion */
      if (fill->children)
         parseFill(fill->children, depth+1, tName);
   }
}



/****************************************************************
   check if the given coordinates overlap an aligning block from a higher scoring chain (lower chainId) that is not the breaking chain
   If this is the case, the broken chain will still be broken by the even-higher-scoring chain, even if we remove the suspect of the breaking chain. 
****************************************************************/
boolean isBrokenByAnotherHigherScoringChain(char *chrom, int intervalStart, int intervalEnd, int chainId, struct fillGapInfo *fillGap) {
   boolean isOverlapped = FALSE;

   verbose(4, "\t\t\tTest if interval %s:%d-%d for chainID %d overlaps with a higher-scoring chain in the genomeRangeTree:\n", chrom, intervalStart, intervalEnd, chainId);
   struct range *range = genomeRangeTreeAllOverlapping(rangeTreeAliBlocks, chrom, intervalStart, intervalEnd);
   for (; range != NULL; range = range->next) {
      struct bed4 *bed = (struct bed4 *) range->val;
      for (; bed != NULL; bed = bed->next) {
         verbose(5, "\t\t\t\tOverlaps with %s:%d-%d of chainID %s\n", bed->chrom, bed->chromStart, bed->chromEnd, bed->name);
         if (atoi(bed->name) < chainId && atoi(bed->name) != fillGap->parentChainId) {
            verbose(5, "\t\t\t\t==> HIGHER-scoring chain (lower chain ID and different from breaking chain)\n");
            isOverlapped = TRUE;
         }
      }
   }
   verbose(4, "\t\t\t==>%s a higher-scoring chain\n", (isOverlapped ? "DOES OVERLAP" : "does not overlap"));
   return isOverlapped;
}

/****************************************************************
   the tab-sep string gapInfo of a fillGapInfo struct contains gapStart, gapEnd, parentChainId
   this function chops the string and fills the fillGapInfo struct variable gapStart, gapEnd, parentChainId
   does it for the entire list of fillGaps for one chain
****************************************************************/
void chopGapInfo(struct fillGapInfo *fillGapList) {
   char *words[5];
   int wordCount;
   struct fillGapInfo *fillGap = NULL;
   
   for (fillGap = fillGapList; fillGap != NULL; fillGap = fillGap->next) {
      wordCount = chopLine(fillGap->gapInfo, words);
      if (wordCount != 5)
         errAbort("chopGapInfo: Expecting 5 tab-separated words in %s but can parse only %d\n", fillGap->gapInfo, wordCount);
      fillGap->gapStart = atoi(words[1]);
      fillGap->gapEnd = atoi(words[2]);
      fillGap->parentChainId = atoi(words[3]);
      fillGap->gapDepth = atoi(words[4]);
   }
}

/****************************************************************
   create a new breakInfo struct with the given values and return it
****************************************************************/
struct breakInfo *newBreak(int depth, int chainId, int parentChainId, char *chrom, int LfillStart, int LfillEnd, int RfillStart, int RfillEnd, 
   int LgapStart, int LgapEnd, int RgapStart, int RgapEnd) {

   struct breakInfo *breakP;
   AllocVar(breakP);
   breakP->depth = depth;
   breakP->chainId = chainId;
   breakP->parentChainId = parentChainId;
   breakP->chrom = cloneString(chrom);
   breakP->LfillStart = LfillStart;
   breakP->LfillEnd = LfillEnd;
   breakP->RfillStart = RfillStart;
   breakP->RfillEnd = RfillEnd;
   breakP->LgapStart = LgapStart;
   breakP->LgapEnd = LgapEnd;
   breakP->RgapStart = RgapStart;
   breakP->RgapEnd = RgapEnd;
   breakP->suspectStart = LgapEnd;
   breakP->suspectEnd = RgapStart;

   breakP->suspectStart = breakP->LgapEnd;
   breakP->suspectEnd = breakP->RgapStart;

   /* sanity checks */
   assert(breakP->suspectStart < breakP->suspectEnd);
   assert(breakP->LfillStart < breakP->suspectStart);
   assert(breakP->LfillEnd <= breakP->suspectStart);
   assert(breakP->RfillStart >= breakP->suspectEnd);
   assert(breakP->RfillEnd > breakP->suspectEnd);

   return breakP;
}


/****************************************************************
   create a new breakInfo struct from two breaks that are adjacent (see isValidPair())

breaking chain: XXXXXXXXXXXXXXX-----------------------XX-------------------XXXXXX--------------XXXXXXXXXXXXX
broken chain:                   XXXXXXXXXXXXXXXXX----------XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
breakInfo upstream             <------------------------------------------>
breakInfo downstream                                    <------------------------------------->
new break pair                 <-------------------------------------------------------------->
                                <-- left fill -->     <------ suspect-----------><right fill>
                               <--- left gap -------->                           <- right gap->

****************************************************************/
struct breakInfo *newBreakPair(struct breakInfo *upstreamBreak, struct breakInfo *downstreamBreak) {
   return newBreak(
      upstreamBreak->depth, upstreamBreak->chainId, upstreamBreak->parentChainId, upstreamBreak->chrom,
      upstreamBreak->LfillStart, upstreamBreak->LfillEnd, downstreamBreak->RfillStart, downstreamBreak->RfillEnd,
      upstreamBreak->LgapStart, upstreamBreak->LgapEnd, downstreamBreak->RgapStart, downstreamBreak->RgapEnd
   );
}

/****************************************************************
   get a list of pairwise breaks for all chains that come in >1 pieces at depth > 1
   input parameter is a hash element (chainID, how_often_it_occurred_in_net)
****************************************************************/
void getValidBreaks(struct hashEl *el)
{
   struct fillGapInfo *fillGapList = NULL, *fillGap;
   int count = (int)ptToInt(el->val);      /* number of times we have seen the chain at depth > 1 */
   struct breakInfo *breakP = NULL, *breakList = NULL; 

   if (count == 1) {
      verbose(3, "\tchainID %s seen only once in the net\n", el->name);
      /* free fillGapList if chain occurs only once */
      fillGapList = hashMustFindVal(fillGapInfoHash, el->name);
      freeMem(fillGapList->chrom);
      freeMem(fillGapList->gapInfo);
      slFreeList(&fillGapList);
      hashRemove(fillGapInfoHash, el->name);
   } else {
      /* this chain is broken into pieces */
      if (verboseLevel() >= 3)
         printFillGapInfoList(el);

      fillGapList = hashMustFindVal(fillGapInfoHash, el->name);

      /* need to split the tab-sep string gapInfo into its fields and fill the gapStart, gapEnd, parentChainId variables of each fillGap for this chain */
      chopGapInfo(fillGapList);
      
      for (fillGap = fillGapList; fillGap != NULL; fillGap = fillGap->next) {
         /* iterate from the first to the last-but-one list element since we are considering pairs of fillGap */
         if (fillGap->next == NULL) 
            break;

         /* experimental. Quickly just run it on the chain with this ID and a suspect with these coords. */
         if (onlyThisChr != NULL && (! sameString(onlyThisChr, fillGap->chrom))) 
            continue;
         if (onlyThisChr != NULL && onlyThisStart != fillGap->gapEnd)
            continue;
         if (onlyThisChr != NULL && onlyThisEnd != fillGap->next->gapStart)
            continue;
         /* */

         verbose(2, "\t\tconsider break candidate:  depth %d  chainID %d  fill: %s:%d-%d  gap: %d-%d %d    AND    depth %d  chainID %d  fill: %s:%d-%d  gap: %d-%d %d\n", 
            fillGap->depth, fillGap->chainId, fillGap->chrom, fillGap->fillStart, fillGap->fillEnd, fillGap->gapStart, fillGap->gapEnd, fillGap->parentChainId, 
            fillGap->next->depth, fillGap->next->chainId, fillGap->next->chrom, fillGap->next->fillStart, fillGap->next->fillEnd, fillGap->next->gapStart, fillGap->next->gapEnd, fillGap->next->parentChainId);

         /* test if the break candidate is valid */
         /* we do not consider breaks at different depths as this involves >1 breaking chains */
         if (fillGap->depth != fillGap->next->depth) {
            verbose(2, "\t\t==> invalid: different depths\n");
            continue;
         }
         /* we do not consider breaks by different breaking chains */
         if (fillGap->parentChainId != fillGap->next->parentChainId) {
            verbose(2, "\t\t==> invalid: at different parent IDs\n");
            continue;
         }
         /* check if there is another higher-scoring chain between the two fill blocks. 
            If this is the case, the broken chain will still be broken by the even-higher-scoring chain, even if we remove the suspect of the breaking chain. 
          */
         if (isBrokenByAnotherHigherScoringChain(fillGap->chrom, fillGap->fillEnd, fillGap->next->fillStart, fillGap->chainId, fillGap)) {
            verbose(2, "\t\t==> invalid: there is a higher-scoring same-level chain block in between. Removing the suspect will not connect the two fills.\n");
            continue;
         }
         /* possible that a chain is broken but the gap is exactly the same 
            Case 1: The fill of the higher level chain that breaks the lower-level chain is filtered from the nets. 
             111111111111111111111----------------------------------1111111111111111111111111111
                                                   222----------------------------------------------222222222222222222222222222222222222222222222
                                      333333------------3333333333
             If the left 222 fill at depth 2 is filtered out, the third-level chain is still broken into two pieces. Ignore (we could stitch this chain though). 
            Case 2: The lower-level chain is broken by a chain that has is completely contained . 
                                        11111---111------11111111111111111
             222222--------------------------------------------------------------22222222222
            Regardless of the reason, nothing to do for us. 
         */
         if (fillGap->gapStart == fillGap->next->gapStart && fillGap->gapEnd == fillGap->next->gapEnd) {
            verbose(2, "\t\t==> invalid: same gap %d - %d  and %d - %d \n", fillGap->gapStart, fillGap->gapEnd, fillGap->next->gapStart, fillGap->next->gapEnd);
            continue;
         }

         /* valid: new break */
         verbose(2, "\t\t==> VALID: ");
         breakP = newBreak(fillGap->depth, fillGap->chainId, fillGap->parentChainId, fillGap->chrom, 
            fillGap->fillStart, fillGap->fillEnd, fillGap->next->fillStart, fillGap->next->fillEnd, 
            fillGap->gapStart, fillGap->gapEnd, fillGap->next->gapStart, fillGap->next->gapEnd);

         /* add both chains to chainId2IsOfInterest */
         /* And add this break to hash breakHash. Value is a linked list to breakInfo structs */
         hashAddIntTrue(chainId2IsOfInterest, fillGap->chainId);
         hashAddIntTrue(chainId2IsOfInterest, fillGap->parentChainId);

         breakList = hashFindValIntKey(breakHash, fillGap->parentChainId);
         if (breakList == NULL)  {
            slAddHead(&breakList, breakP);
            breakP->prev = NULL;
            hashAddIntKey(breakHash, fillGap->parentChainId, breakList);
         } else {
            /* otherwise just add to this break to the end of the list */
            breakP->prev = slLastEl(breakList);
            slAddTail(&breakList, breakP);
         }
         verbose(2, "added break to the list of the breaking chain %d:  [depth %d  breaking chainID %d  broken chainID %d  chrom %s  Lfill %d-%d  Rfill %d-%d  suspect %d-%d]\n", 
            breakP->parentChainId, breakP->depth, breakP->parentChainId, breakP->chainId, breakP->chrom, breakP->LfillStart, breakP->LfillEnd, breakP->RfillStart, breakP->RfillEnd, breakP->suspectStart, breakP->suspectEnd);
      }

      /* free */
      for (fillGap = fillGapList; fillGap != NULL; fillGap = fillGap->next) {
         freeMem(fillGap->chrom);
         freeMem(fillGap->gapInfo);
      }
      slFreeList(&fillGapList);
      hashRemove(fillGapInfoHash, el->name);
   }
   fflush(stdout);
}


/****************************************************************
   called from main: 
   read the net file, then parse all the fill and gap lines into the fillGapInfo struct list by calling parseFill()
   then get valid breaks by calling getValidBreaks()
   free a bunch of memory
****************************************************************/
void getFillGapAndValidBreaks(char *netFile) {
   struct lineFile *lf = NULL;
   struct chainNet *netList = NULL, *net;
   char *tName = NULL;
   int i;

   /* open net and read net file */
   verbose(1, "1.1 read net file %s into memory ...\n", netFile);
   lf = lineFileOpen(netFile, TRUE);
   while ((net = chainNetRead(lf)) != NULL) {
      slAddHead(&netList, net);
   }
   slReverse(&netList);
   lineFileClose(&lf);
   verbose(1, "DONE\n\n");


   /* we need a init and allocate a few hashes and arrays. 
      Arrays are used with net depth being the index to keep track of which chain and which gap we currently have at each depth 
   */
   verbose(1, "1.2 get fills/gaps from %s ...\n", netFile);
   chainId2Count = newHash(0);              /* counts how often we see a chain ID at depth > 1 */
   fillGapInfoHash = newHash(0);            /* Hash keyed chainId, stores a list of fillGapInfo structs */
   AllocArray(depth2gap, maxNetDepth);      /* information about the current gap at each depth (hash key) */
   for (i=0; i<maxNetDepth; i++)
      depth2gap[i] = newDyString(200);
   AllocArray(depth2chainId, maxNetDepth);  /* information about which chain ID is the fill at each depth (hash key) */

   /* now parse all the nets, fill the fillGapInfo struct for every chain at depth>1 */
   for (net = netList; net != NULL; net = net->next) {
      if (onlyThisChr != NULL && (! sameString(onlyThisChr, net->name)))
         continue;
      verbose(2, "\tparse net %s of size %d\n", net->name, net->size);
      tName = net->name;         /* need to keep track of the current target chrom/scaffold */
      parseFill(net->fillList, 1, tName);
    }
   verbose(1, "DONE\n\n");

   /* parse the nets once and add all aligning blocks of a net that do not have children (NOTE: they can span gaps) to the genomeRangeTree 
      We will use this in isBrokenByAnotherHigherScoringChain() to check if the broken chain is also broken by another even-higher-scoring chain 
   */
   rangeTreeAliBlocks = genomeRangeTreeNew();
   verbose(1, "1.3 get aligning regions from %s ...\n", netFile);
   for (net = netList; net != NULL; net = net->next) {
      if (onlyThisChr != NULL && (! sameString(onlyThisChr, net->name)))
         continue;
      verbose(2, "\tget aligning regions from net %s\n", net->name);
      tName = net->name;         /* need to keep track of the current target chrom/scaffold */
      rConvert(net->fillList, tName);
    }
   verbose(1, "DONE\n\n");

   /* output all fillGap if verbose>=3 */
   if (verboseLevel() >= 3) {
      verbose(3, "\n\nVERBOSE OUTPUT: Here are all the fillGap structs (printFillGapInfoList())\n");
      hashTraverseEls(chainId2Count, printFillGapInfoList);
      verbose(3, "\n");
   }
   
   /* free net */
   chainNetFreeList(&netList);

   /* get valid breaks */
   verbose(1, "1.4 get valid breaks ...\n");
   breakHash = newHash(0);
   chainId2IsOfInterest = newHash(0);
   hashTraverseEls(chainId2Count, getValidBreaks);
   verbose(1, "DONE\n");

   if (verboseLevel() >= 3) {
      verbose(3, "\n\nVERBOSE OUTPUT: Here are all the breaks (printAllBreaks())\n");
      printAllBreaks();
   }

   /* free arrays that hold values per depth */
   for (i=0; i<maxNetDepth; i++)
      freeDyString(&depth2gap[i]);
   freez(&depth2gap);
   freez(&depth2chainId);

   genomeRangeTreeFree(&rangeTreeAliBlocks);
}

/****************************************************************
  sum up the bases in aligning blocks of the chain
****************************************************************/
int getSubChainBases(struct chain *chain) {
   struct cBlock *b;
   int numBases = 0;
   for (b = chain->blockList; b != NULL; b = b->next) {
      numBases += (b->tEnd - b->tStart);
   }
   return numBases;
}


/****************************************************************
  test if suspect passes our thresholds and should be removed
  remove the suspect blocks from the breaking chain and update the boundaries of up/downstream breaks
  the breaksUpdated parameter is set to TRUE if a suspect is removed and boundaries or the up- and/or downstream breaks were updated (is passed on to calling function)
  returns TRUE if this suspect is removed 
  (debugInfo is just a string passed on to the output files created with -debug)
****************************************************************/
boolean testAndRemoveSuspect(struct breakInfo *breakP, struct breakInfo *upstreamBreak, struct breakInfo *downstreamBreak, boolean *breaksUpdated, char *debugInfo, boolean IsPair) {

   struct chain *breakingChain = NULL, *brokenChain = NULL;
   double breakingChainScore = 0, brokenChainScore = 0;
   struct chain *subChainSuspect = NULL, *subChainLfill = NULL, *subChainRfill = NULL, *subChainfill = NULL;
   struct chain *chainToFree1 = NULL, *chainToFree2 = NULL, *chainToFree3 = NULL, *chainToFree4 = NULL;
   double subChainSuspectLocalScore, dummy;
   double subChainfillLocalScore, subChainLfillLocalScore, subChainRfillLocalScore;
   boolean isRemoved = FALSE;
   *breaksUpdated = FALSE;
   int subChainSuspectBases = -1;

   /* get the breaking and the broken chain */
   breakingChain = getChainPointerFromHash(breakP->parentChainId);
   if (breakingChain == NULL) 
      errAbort("ERROR: cannot get breaking chain with Id %d from chainId2chain hash\n", breakP->parentChainId);
   breakingChainScore = breakingChain->score;
   brokenChain = getChainPointerFromHash(breakP->chainId);
   if (brokenChain == NULL)
       errAbort("ERROR: cannot get broken chain with Id %d from chainId2chain hash\n", breakP->chainId);
   brokenChainScore = brokenChain->score;

   /* get 4 subchains. the suspect, left/right and entire fill */
   chainSubsetOnT(breakingChain, breakP->suspectStart, breakP->suspectEnd, &subChainSuspect, &chainToFree1);
   chainSubsetOnT(brokenChain, breakP->LfillStart, breakP->RfillEnd, &subChainfill, &chainToFree2);
   chainSubsetOnT(brokenChain, breakP->LfillStart, breakP->suspectEnd, &subChainLfill, &chainToFree3);
   chainSubsetOnT(brokenChain, breakP->suspectStart, breakP->RfillEnd, &subChainRfill, &chainToFree4);

   /* only way that the suspect subChain is NULL if it occurred more than once in the list and was deleted in the same iteration by another chain */   
   if (subChainSuspect == NULL) {
      verbose(3, "\t\tSuspect %d-%d is apparently already deleted as the suspect subChain is NULL\n", breakP->suspectStart, breakP->suspectEnd);
      return FALSE;
   }

   /* compute the scores of the subChains */
   getChainScore(subChainSuspect, &dummy, &subChainSuspectLocalScore);
   getChainScore(subChainfill,  &dummy, &subChainfillLocalScore);
   getChainScore(subChainLfill, &dummy, &subChainLfillLocalScore);
   getChainScore(subChainRfill, &dummy, &subChainRfillLocalScore);
   /* ratio between the global score of the broken chain and the local score of the suspect */
   double ratio  = subChainfill->score  / subChainSuspectLocalScore;
   double ratioL = subChainLfill->score / subChainSuspectLocalScore;
   double ratioR = subChainRfill->score / subChainSuspectLocalScore;

   subChainSuspectBases = getSubChainBases(subChainSuspect);

   verbose(3, "\t\t\tsuspect subChain            %d - %d   gets score %7d   (local score %d, suspect subChain bases %d, left gap size %d, right gap size %d)\n", 
      breakP->suspectStart, breakP->suspectEnd, (int)subChainSuspect->score, (int)subChainSuspectLocalScore, subChainSuspectBases, 
      breakP->LgapEnd - breakP->LgapStart, breakP->RgapEnd - breakP->RgapStart);
   verbose(3, "\t\t\tleft-to-right fill subChain %d - %d   gets score %7d   (local %7d)  ratio %1.2f\n", breakP->LfillStart, breakP->RfillEnd, (int)subChainfill->score, (int)subChainfillLocalScore, ratio); 
   verbose(3, "\t\t\tleft fill subChain          %d - %d   gets score %7d   (local %7d)  ratio %1.2f\n", breakP->LfillStart, breakP->LfillEnd, (int)subChainLfill->score, (int)subChainLfillLocalScore, ratioL); 
   verbose(3, "\t\t\tright fill subChain         %d - %d   gets score %7d   (local %7d)  ratio %1.2f\n", breakP->RfillStart, breakP->RfillEnd, (int)subChainRfill->score, (int)subChainRfillLocalScore, ratioR); 



   /* decision */
   if (! IsPair) {
      if (
            /* left/right score ratio */
            ratioL >= LRfoldThreshold && ratioR >= LRfoldThreshold && 
            /* overall score ratio */
            ratio >= foldThreshold && 
            /* goodness of the suspect (if it scores too high) */
            subChainSuspectLocalScore <= maxSuspectScore && subChainSuspectBases <= maxSuspectBases && 
            /* goodness of the broken chain (if it scores too little) */
            brokenChainScore >= minBrokenChainScore && 
            /* size of the gap */
            (breakP->LgapEnd - breakP->LgapStart) >= minLRGapSize  &&  (breakP->RgapEnd - breakP->RgapStart) >= minLRGapSize
         ) {
         isRemoved = TRUE;
      }
   /* use LRfoldThresholdPairs for a pair of suspects */
   }else{
      if (
            /* left/right score ratio */
            ratioL >= LRfoldThresholdPairs && ratioR >= LRfoldThresholdPairs && 
            /* overall score ratio */
            ratio >= foldThreshold && 
            /* goodness of the suspect (if it scores too high) */
            subChainSuspectLocalScore <= maxSuspectScore && subChainSuspectBases <= maxSuspectBases && 
            /* goodness of the broken chain (if it scores too little) */
            brokenChainScore >= minBrokenChainScore && 
            /* size of the gap */
            (breakP->LgapEnd - breakP->LgapStart) >= minLRGapSize  &&  (breakP->RgapEnd - breakP->RgapStart) >= minLRGapSize
         ) {
         isRemoved = TRUE;
      }
   }


   /* if set, do not clean any suspect. Only output all suspect data in bed format to this file */
   if (suspectDataFile != NULL) {
      isRemoved = FALSE;
      suspectID ++;
      
      /* format: comma-separated
      0:  suspectID
      1:  breakingChainID
      2:  breakingChainTotalScore
      3:  brokenChainID
      4:  brokenChainTotalScore
      5:  suspectLocalScore
      6:  brokenChainGlobalScore
      7:  LEFTbrokenChainGlobalScore
      8:  RIGHTbrokenChainGlobalScore
      9:  numSuspectBases
      10: sizeOfLeftGap
      11: sizeOfRightGap
      12: LEFTbrokenChainLOCALScore
      13: RIGHTbrokenChainLOCALScore
      */
      fprintf(suspectDataFilePointer, "%s\t%d\t%d\t%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
         breakP->chrom, breakP->suspectStart, breakP->suspectEnd,
         suspectID, breakP->parentChainId, (int)breakingChainScore, breakP->chainId, (int)brokenChainScore,
         (int)subChainSuspectLocalScore, (int)subChainfill->score,
         (int)subChainLfill->score, (int)subChainRfill->score,
         subChainSuspectBases, (breakP->LgapEnd - breakP->LgapStart), (breakP->RgapEnd - breakP->RgapStart),
         (int)subChainLfillLocalScore, (int)subChainRfillLocalScore);
   }

   /* write the subChains and the bed coordinates of the suspect and the fills */
   if (debug) {
      chainWrite(subChainSuspect, suspectChainFile);
      chainWrite(subChainLfill, brokenChainLfillChainFile);
      chainWrite(subChainRfill, brokenChainRfillChainFile);
      chainWrite(subChainfill, brokenChainfillChainFile);
      fprintf(suspectFillBedFile, "%s\t%d\t%d\t%s%sSuspect__score_%1.0f__Rleft_%1.2f__Rright_%1.2f\t1000\t+\t%d\t%d\t255,0,0\n", breakP->chrom, breakP->suspectStart, breakP->suspectEnd, (isRemoved ? "REMOVED_" : ""), debugInfo, subChainSuspectLocalScore, ratioL, ratioR, breakP->suspectStart, breakP->suspectEnd);
      fprintf(suspectFillBedFile, "%s\t%d\t%d\t%sFill__score_%1.0f\t1000\t+\t%d\t%d\t0,0,255\n",    breakP->chrom, breakP->LfillStart, breakP->RfillEnd, debugInfo, subChainfill->score, breakP->LfillStart, breakP->RfillEnd);
      fprintf(suspectFillBedFile, "%s\t%d\t%d\t%sLfill__score_%1.0f\t1000\t+\t%d\t%d\t0,125,255\n", breakP->chrom, breakP->LfillStart, breakP->suspectEnd, debugInfo, subChainLfill->score, breakP->LfillStart, breakP->LfillEnd);
      fprintf(suspectFillBedFile, "%s\t%d\t%d\t%sRfill__score_%1.0f\t1000\t+\t%d\t%d\t0,125,255\n", breakP->chrom, breakP->suspectStart, breakP->RfillEnd, debugInfo, subChainRfill->score, breakP->RfillStart, breakP->RfillEnd);
   }


   /* now remove the suspect if the above thresholds are met */
   if (isRemoved) {
      isRemoved = TRUE;
      hashAddIntTrue(chainId2NeedsRescoring, breakingChain->id);
      verbose(3, "\t\t\t===> REMOVE suspect from breaking chainID %d (this chain will be rescored before writing)\n", breakingChain->id);

      /* output removed suspect to the bed file */
      fprintf(suspectsRemovedOutBedFile, "%s\t%d\t%d\tbreakingChainID_%d_Score_%d_brokenChainID_%d_Score_%d_suspectLocalScore_%d_RatioL_%1.2f_RatioR_%1.2f\t1000\t+\t%d\t%d\t%s\n", 
         breakP->chrom, breakP->suspectStart, breakP->suspectEnd, 
         breakP->parentChainId, (int)breakingChainScore, breakP->chainId, (int)brokenChainScore, 
         (int)subChainSuspectLocalScore, ratioL, ratioR, 
         breakP->suspectStart, breakP->suspectEnd, (IsPair ? "0,100,255" : "0,0,153"));

      /* remove suspect blocks from the chain */
      chainRemoveBlocks(breakingChain, breakP->suspectStart, breakP->suspectEnd);

      /* give the new chain a new ID */
      maxChainId ++;
      subChainSuspect->id = maxChainId;
      if (newChainIDDictFile != NULL) {
         fprintf(newChainIDDictFile, "%d\t%d\n", subChainSuspect->id, breakingChain->id);
      }
      chainWrite(subChainSuspect, finalChainOutFile);
      verbose(4, "\t\t\twrote new chain representing the removed suspect with ID %d\n", subChainSuspect->id); 

      /* now update the fill coordinates for the up/downstream break */
      if (upstreamBreak != NULL) {
         /* only update upstream break if 
            - it has the same breaking and broken chain ID
            - if the upstream left fill == current right fill (otherwise there was an invalid break candidate between the upstream and the current break) */
         if (breakP->chainId == upstreamBreak->chainId && breakP->parentChainId == upstreamBreak->parentChainId &&
             upstreamBreak->RfillStart == breakP->LfillStart && upstreamBreak->RfillEnd == breakP->LfillEnd) {
            *breaksUpdated = TRUE;
            verbose(5, "\t\t\tUpdate upstream break fill boundaries\n\t\t\t\tUpstream break:  suspect %d - %d  fill %d - %d\n\t\t\t\tCurrent break:   suspect %d - %d  fill %d - %d\n", 
               upstreamBreak->suspectStart, upstreamBreak->suspectEnd, upstreamBreak->LfillStart, upstreamBreak->RfillEnd,
               breakP->suspectStart, breakP->suspectEnd, breakP->LfillStart, breakP->RfillEnd);
            assert(upstreamBreak->LfillEnd < breakP->LfillStart);  /* left fill upstream must be < left fill */
            assert(upstreamBreak->suspectEnd < breakP->suspectStart);  /* suspect end upstream must be < suspect start */
            /* update RfillEnd and RgapEnd */
            verbose(5, "\t\t\t\t--> set upstream RfillEnd from %d to %d\n\t\t\t\t--> set upstream RgapEnd  from %d to %d\n", upstreamBreak->RfillEnd, breakP->RfillEnd, upstreamBreak->RgapEnd, breakP->RgapEnd);
            upstreamBreak->RfillEnd = breakP->RfillEnd;
            upstreamBreak->RgapEnd = breakP->RgapEnd;
         }
      }

      if (downstreamBreak != NULL) {
         /* only update downstream break if 
            - it has the same breaking and broken chain ID
            - if the current right fill == downstream left fill (otherwise there was an invalid break candidate between the current and the downstream break) */
         if (breakP->chainId == downstreamBreak->chainId && breakP->parentChainId == downstreamBreak->parentChainId &&
             downstreamBreak->LfillStart == breakP->RfillStart && downstreamBreak->LfillEnd == breakP->RfillEnd) {
            *breaksUpdated = TRUE;
            verbose(5, "\t\t\tUpdate downstream break fill boundaries\n\t\t\t\tCurrent break:     suspect %d - %d  fill %d - %d\n\t\t\t\tDownstream break:  suspect %d - %d  fill %d - %d\n", 
               breakP->suspectStart, breakP->suspectEnd, breakP->LfillStart, breakP->RfillEnd, 
               downstreamBreak->suspectStart, downstreamBreak->suspectEnd, downstreamBreak->LfillStart, downstreamBreak->RfillEnd); 
            assert(downstreamBreak->RfillStart > breakP->RfillEnd);      /* right fill downstream must be > right fill */
            assert(downstreamBreak->suspectStart > breakP->suspectEnd);  /* suspect start downstream must be > suspect end */
            /* update LfillStart and LgapEnd */
            verbose(5, "\t\t\t\t--> set downstream LfillStart from %d to %d\n\t\t\t\t--> set downstream LgapEnd  from %d to %d\n", downstreamBreak->LfillStart, breakP->LfillStart, downstreamBreak->LgapStart, breakP->LgapStart);
            downstreamBreak->LfillStart = breakP->LfillStart;
            downstreamBreak->LgapStart = breakP->LgapStart;
         }
      }

   } else {
      verbose(3, "\t\t\t===> do not remove suspect from breaking chainID %d\n", breakingChain->id);
   }

   chainFree(&chainToFree1);
   chainFree(&chainToFree2);
   chainFree(&chainToFree3);
   chainFree(&chainToFree4);

   return isRemoved;
}


/****************************************************************
  test if the two breaks would form a valid pair. 
  That is: 
   - they have the same breaking and broken chain ID 
   - they have the same depth
   - they are adjacent (upstream right gap == downstream left gap) 
****************************************************************/
boolean isValidBreakPair (struct breakInfo *upstreamBreak,  struct breakInfo *downstreamBreak) {
    
    verbose(4, "\t\tAre the following two breaks a valid pair?\n");
    verbose(4, "\t\t\tupstream break    :  depth %d  breaking chainID %6d  broken chainID %6d  chrom %s  Lgap %d-%d  Rgap %d-%d  suspect %d-%d\n", 
       upstreamBreak->depth, upstreamBreak->parentChainId, upstreamBreak->chainId, upstreamBreak->chrom, 
       upstreamBreak->LgapStart, upstreamBreak->LgapEnd, upstreamBreak->RgapStart, upstreamBreak->RgapEnd, upstreamBreak->suspectStart, upstreamBreak->suspectEnd);
    verbose(4, "\t\t\tdownstream break  :  depth %d  breaking chainID %6d  broken chainID %6d  chrom %s  Lgap %d-%d  Rgap %d-%d  suspect %d-%d\n", 
       downstreamBreak->depth, downstreamBreak->parentChainId, downstreamBreak->chainId, downstreamBreak->chrom, 
       downstreamBreak->LgapStart, downstreamBreak->LgapEnd, downstreamBreak->RgapStart, downstreamBreak->RgapEnd, downstreamBreak->suspectStart, downstreamBreak->suspectEnd);

   if (upstreamBreak->parentChainId != downstreamBreak->parentChainId || upstreamBreak->chainId != downstreamBreak->chainId) {
      verbose(4, "\t\t\t --> INVALID (different chain IDs)\n");
      return FALSE;
   }

   if (upstreamBreak->depth != downstreamBreak->depth) {
      verbose(4, "\t\t\t --> INVALID (different chain IDs)\n");
      return FALSE;
   }

   if (downstreamBreak->suspectStart - upstreamBreak->suspectEnd > maxPairDistance) {
      verbose(4, "\t\t\t --> INVALID (distance between upstream suspectEnd and downstream suspectStart %d is greater than maxPairDistance %d\n", downstreamBreak->suspectStart - upstreamBreak->suspectEnd, maxPairDistance);
      return FALSE;
   }

   if (upstreamBreak->RgapStart == downstreamBreak->LgapStart && upstreamBreak->RgapEnd == downstreamBreak->LgapEnd) {
      verbose(4, "\t\t\t --> Valid pair\n");
      return TRUE;
   }else{
      verbose(4, "\t\t\t --> INVALID (gap ends to not match)\n");
      return FALSE;
   }
}


/****************************************************************
  go through all the breaks. 
  For one breaking chain, 
   - iteratively consider all breaks
   - remove the suspects if they pass our thresholds
   - continue iterating until (i) no break remains or (ii) no suspect was removed in the last round
  Do that for single breaks and pairs of breaks
  Calls testAndRemoveSuspect()
****************************************************************/
void loopOverBreaks() {
   struct breakInfo *breakP = NULL, *nextBreak = NULL, *breakList = NULL; 
   struct hashEl *hel, *helList = hashElListHash(breakHash);
   int parentChainId;
   char debugInfo[256];
   boolean currentSuspectRemoved = FALSE;
   boolean anyBreaksUpdatedSingle = FALSE;
   boolean anyBreaksUpdatedPair = FALSE;
   boolean currentBreaksUpdated = FALSE;
   
   /* hel is a pointer to a list of breaks for one parentChain */
   for (hel = helList; hel != NULL; hel = hel->next)  {
      breakList = hel->val;  /* start of the list */
      if (breakList == NULL) 
         errAbort("ERROR: breakList for breaking chainID %s is NULL\n", hel->name); 

      parentChainId = breakList->parentChainId;
      verbose(3, "\t############ Test if suspects of breaking chain %d can be removed\n", parentChainId); 

      /* try to remove chain-breaking suspects in the list until we do not remove anything in the entire pass through the list 
         NOTE: After removing one suspect, up- and downstream breaks now may get larger fill regions if the broken chain was broken into >2 pieces. 
         Therefore, we iterate until no break remains or nothing can be removed anymore.
      */
      int iteration = 0;
      int totalNumIteration = 0;
      for (;;) {
         iteration++;
         verbose(3, "\tOVERALL ITERATION %d for breaking chain %d\n", iteration, parentChainId); 
         verbose(3, "\tTEST SINGLE BREAKS\n"); 

         int iterationSingle = 0;
         for (;;) {
            iterationSingle++;
            totalNumIteration++;

            /* verbose output */
            if (verboseLevel() >= 3) {
               verbose(3, "\tIterationSingle %d for breaking chain %d\n\t\tList of remaining breaks\n", iterationSingle, parentChainId); 
               for (breakP = breakList; breakP != NULL; breakP = breakP->next)
                  printSingleBreak(breakP);
            }

            anyBreaksUpdatedSingle = FALSE;

            /* consider single breaks */
            breakP = breakList;
            /* now loop over all breaks in the list for this breaking chain */
            for(;;) {
               if (breakP == NULL) 
                  break;

               verbose(3, "\t\tconsider break:  depth %d  breaking chainID %6d  broken chainID %6d  chrom %s  Lfill %d-%d  Rfill %d-%d  suspect %d-%d\n", 
                  breakP->depth, breakP->parentChainId, breakP->chainId, breakP->chrom, 
                  breakP->LfillStart, breakP->LfillEnd, breakP->RfillStart, breakP->RfillEnd, breakP->suspectStart, breakP->suspectEnd);

               /* call testAndRemoveSuspect: This will remove the suspect blocks from the chain, output the new chain representing the removed suspect and updating the boundaries of up/downstream breaks */
               sprintf(debugInfo, "SINGLE_%d",totalNumIteration);
               currentSuspectRemoved = testAndRemoveSuspect(breakP, breakP->prev, breakP->next, &currentBreaksUpdated, debugInfo, FALSE);   /* IsPair is FALSE */

               /* just keep track of if a break is updated (then we have to iterate again) */
               if (currentBreaksUpdated) 
                  anyBreaksUpdatedSingle = TRUE;

               nextBreak = breakP->next;
               /* remove this break from the linked list, also update the prev pointer 
                  free break
               */
               if (currentSuspectRemoved) {
                  if (! slRemoveEl(&breakList, breakP))
                     errAbort("ERROR: cannot remove break from the list: suspect %s:%d-%d\n", breakP->chrom, breakP->suspectStart, breakP->suspectEnd);
                  if (breakP->next != NULL)
                     breakP->next->prev = breakP->prev;
                  freeMem(breakP->chrom);
                  freeMem(breakP);
               }
               /* continue looping with the next element in the list */
               breakP = nextBreak;
            }

            if (! anyBreaksUpdatedSingle) {
               verbose(3, "\t==> no single break is updated \n"); 
               break;
            }
            if (breakList == NULL) {
               verbose(3, "\t==> ALL suspect removed --> done with breaking chain %d\n", parentChainId); 
               break;
            }
         }

         anyBreaksUpdatedPair = FALSE;
         if (doPairs) {
            /* now loop over all break pairs in the list of remaining breaks for this breaking chain */
            verbose(3, "\tTEST PAIRS\n"); 
            totalNumIteration++;

            /* verbose output */
            if (verboseLevel() >= 3) {
               verbose(3, "\tIteration %d for breaking chain %d\n\t\tList of remaining breaks\n", iteration, parentChainId); 
               for (breakP = breakList; breakP != NULL; breakP = breakP->next)
                  printSingleBreak(breakP);
            }

            /* start again at the beginning of the list */
            breakP = breakList;

            for(;;) {
               if (breakP == NULL)
                  break;
               if (breakP->next == NULL) 
                  break;

               /* lets define pointers to the two breaks in the pair and the break up/downstream of the pair */
               struct breakInfo *upstreamBreakInPair = breakP;
               struct breakInfo *downstreamBreakInPair = breakP->next;
               struct breakInfo *breakAfterPair = breakP->next->next;
               struct breakInfo *breakBeforePair = breakP->prev; 
   
               if (isValidBreakPair(upstreamBreakInPair, downstreamBreakInPair)) {
                  struct breakInfo *breakPair = newBreakPair(upstreamBreakInPair, downstreamBreakInPair);
               
                  verbose(3, "\t\t\tconsider breakPair:  depth %d  breaking chainID %6d  broken chainID %6d  chrom %s  Lfill %d-%d  Rfill %d-%d  suspect %d-%d\n", 
                     breakPair->depth, breakPair->parentChainId, breakPair->chainId, breakPair->chrom, 
                     breakPair->LfillStart, breakPair->LfillEnd, breakPair->RfillStart, breakPair->RfillEnd, breakPair->suspectStart, breakPair->suspectEnd);

                  /* call testAndRemoveSuspect: This will remove the suspect blocks from the chain, output the new chain representing the removed suspect and updating the boundaries of up/downstream breaks */
                  sprintf(debugInfo, "PAIR_%d",totalNumIteration);
                  currentSuspectRemoved = testAndRemoveSuspect(breakPair, breakBeforePair, breakAfterPair, &currentBreaksUpdated, debugInfo, TRUE);   /* IsPair is TRUE */
                  if (currentSuspectRemoved) 
                     verbose(3, "\t\t\t===> PAIR REMOVED\n");


                  /* just keep track of if a break is updated (then we have to iterate again) */
                  if (currentBreaksUpdated) 
                     anyBreaksUpdatedPair = TRUE;

                  /* remove both breaks from the pair from the linked list, also update the prev pointer 
                     free both breaks
                  */
                  if (currentSuspectRemoved) {
                     if (! slRemoveEl(&breakList, upstreamBreakInPair))
                        errAbort("ERROR: cannot remove break from the list: suspect %s:%d-%d\n", upstreamBreakInPair->chrom, upstreamBreakInPair->suspectStart, upstreamBreakInPair->suspectEnd);
                     if (! slRemoveEl(&breakList, downstreamBreakInPair))
                        errAbort("ERROR: cannot remove break from the list: suspect %s:%d-%d\n", downstreamBreakInPair->chrom, downstreamBreakInPair->suspectStart, downstreamBreakInPair->suspectEnd);

                     if (breakAfterPair != NULL)
                        breakAfterPair->prev = breakBeforePair;
                     freeMem(upstreamBreakInPair->chrom);
                     freeMem(upstreamBreakInPair);
                     freeMem(downstreamBreakInPair->chrom);
                     freeMem(downstreamBreakInPair);
                  }
               
                  /* free the pair */
                  freeMem(breakPair->chrom); 
                  freeMem(breakPair);
               
                  /* continue looping with the next element after the pair if we removed the pair. Otherwise, next element in the list */
                  if (currentSuspectRemoved) 
                     breakP = breakAfterPair;
                  else
                       breakP = downstreamBreakInPair;
               }else{
                  /* not a valid pair. Continue looping with the next element in the list */
                    breakP = downstreamBreakInPair;
               }
            }
         }

         if (! anyBreaksUpdatedPair) {
            verbose(3, "\t==> no pair of breaks is updated --> done with breaking chain %d\n", parentChainId); 
            break;
         }
         if (breakList == NULL) {
            verbose(3, "\t==> ALL suspect removed --> done with breaking chain %d\n", parentChainId); 
            break;
         }
      }
   }
   hashElFreeList(&helList);
}


/****************************************************************
  net the chains if no net file is given
  Netting will be done by chainNet minScore=0 followed by non-nested net filtering with a minscore of 3000
  A tempfile will be created for the nets
****************************************************************/
void netInputChains (char *chainFile) {
   int fd = 0, retVal = 0;
   struct dyString* cmd = newDyString(500);
   char netFile[200];

   /* must have the t/q sizes */
   if (tSizes == NULL)
      errAbort("You must specifiy -tSizes /dir/to/target/chrom.sizes if you do not provide a net file with -net in.net\n");
   if (qSizes == NULL)
      errAbort("You must specifiy -qSizes /dir/to/query/chrom.sizes if you do not provide a net file with -net in.net\n");

   /* create a unique tempFile (and close it because mkstemp opens it */
   safef(netFile, sizeof(netFile), "tmp.chainCleaner.XXXXXXX.net"); 
   if ((fd = mkstemps(netFile,4)) < 0 ) {
      errAbort("ERROR: cannot create a tempfile for netting the chain file: %s\n",strerror(errno));
   } else {
      verbose(1, "\t\ttempfile for netting: %s\n", netFile);
      close(fd);
   }

   dyStringClear(cmd);
   dyStringPrintf(cmd, "set -o pipefail; chainNet -minScore=0 %s %s %s stdout /dev/null | NetFilterNonNested.perl /dev/stdin -minScore1 3000 > %s", chainFile, tSizes, qSizes, netFile);
   verbose(3, "\t\trunning netting command: %s\n", cmd->string);
   retVal = system(cmd->string);
   if (0 != retVal)
      errAbort("ERROR: chainNet | NetFilterNonNested.perl failed. Cannot net the chains. Command: %s\n", cmd->string);
   verbose(3, "\t\tnetting done\n");

   inNetFile = cloneString(netFile);
}






/* ################################################################################### */
/*   main                                                                              */
/* ################################################################################### */
int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
char *scoreSchemeName = NULL;
optionHash(&argc, argv);
struct twoBitFile *tbf;
boolean didNetMyself = FALSE;  /* flag. True if we did the netting */
struct dyString* cmd = newDyString(500);   /* for chainSort */
int retVal = 0;
char outChainFileUnsorted [500];


if (argc != 6)
    usage();

if (debug)
   printf("### DEBUG mode ###\n");
/* get all the options */
inChainFile = argv[1];
tNibDir = argv[2];
qNibDir = argv[3];
outChainFile = argv[4];
safef(outChainFileUnsorted, sizeof(outChainFileUnsorted), "%s.unsorted", outChainFile);
outRemovedSuspectsFile = argv[5];
tIsTwoBit = twoBitIsFile(tNibDir);
qIsTwoBit = twoBitIsFile(qNibDir);
inNetFile = optionVal("net", NULL);
tSizes = optionVal("tSizes", NULL);
qSizes = optionVal("qSizes", NULL);
gapFileName = optionVal("linearGap", NULL);
scoreSchemeName = optionVal("scoreScheme", NULL);
debug = optionExists("debug");
foldThreshold = optionDouble("foldThreshold", foldThreshold);
LRfoldThreshold = optionDouble("LRfoldThreshold", LRfoldThreshold);
LRfoldThresholdPairs = optionDouble("LRfoldThresholdPairs", LRfoldThresholdPairs);
maxSuspectBases = optionDouble("maxSuspectBases", maxSuspectBases);
maxSuspectScore = optionDouble("maxSuspectScore", maxSuspectScore);
minBrokenChainScore = optionDouble("minBrokenChainScore", minBrokenChainScore);
minLRGapSize = optionInt("minLRGapSize", minLRGapSize);
doPairs = optionExists("doPairs");
maxPairDistance = optionInt("maxPairDistance", maxPairDistance);
newChainIDDict = optionVal("newChainIDDict", NULL);
suspectDataFile = optionVal("suspectDataFile", NULL);

onlyThisChr = optionVal("onlyThisChr", NULL);
onlyThisStart = optionInt("onlyThisStart", -1);
onlyThisEnd = optionInt("onlyThisEnd", -1);

if (onlyThisChr != NULL)
   verbose(1, "ONLY %s %d %d\n", onlyThisChr, onlyThisStart, onlyThisEnd);

verbose(1, "Verbosity level: %d\n", verboseLevel());
verbose(1, "foldThreshold: %f    LRfoldThreshold: %f   maxSuspectBases: %d  maxSuspectScore: %d  minBrokenChainScore: %d  minLRGapSize: %d", foldThreshold, LRfoldThreshold, (int)maxSuspectBases, (int)maxSuspectScore, (int)minBrokenChainScore, minLRGapSize);
if (doPairs) {
   verbose(1, " doPairs with LRfoldThreshold: %f   maxPairDistance %d\n", LRfoldThresholdPairs, maxPairDistance);
}else{
   verbose(1, "\n");
}

/* load score scheme */
if (scoreSchemeName != NULL) {
   verbose(1, "Reading scoring matrix from %s\n", scoreSchemeName);
   scoreScheme = axtScoreSchemeRead(scoreSchemeName);
} else {
   scoreScheme = axtScoreSchemeDefault();
}
/* load gap costs */
if (gapFileName == NULL)
    errAbort("Must specify linear gap costs.  Use 'loose' or 'medium' for defaults\n");
gapCalc = gapCalcFromFile(gapFileName);

/* test if the seq files exist.  */
if (! fileExists(tNibDir))
   errAbort("ERROR: target 2bit file or nib directory %s does not exist\n", tNibDir);
if (! fileExists(qNibDir))
   errAbort("ERROR: query 2bit file or nib directory %s does not exist\n", qNibDir);

/* if the net is not given via -net in.net  then net the chains */
if (inNetFile == NULL) {
   /* test if chainNet and NetFilterNonNested.perl are installed and in $PATH */
   dyStringClear(cmd);
   dyStringPrintf(cmd, "which chainNet > /dev/null");
   retVal = system(cmd->string);
   if (0 != retVal)
      errAbort("ERROR: chainNet (kent source code) is not a binary in $PATH. Either install the kent source code or provide the nets as input.\n");
   dyStringClear(cmd);
   dyStringPrintf(cmd, "which NetFilterNonNested.perl > /dev/null");
   retVal = system(cmd->string);
   if (0 != retVal)
      errAbort("ERROR: NetFilterNonNested.perl (comes with the chainCleaner source code) is not a binary in $PATH. Either install it or provide the nets as input.\n");
   
   /* now net */
   verbose(1, "0. need to net the input chains %s (no net file given) ...\n", inChainFile);
   didNetMyself = TRUE;
   netInputChains(inChainFile);
   verbose(1, "DONE (nets in %s)\n", inNetFile);
}


/* 1. read the net and the chain file */ 
verbose(1, "1. parsing fills/gaps from %s and getting valid breaks ...\n", inNetFile);
getFillGapAndValidBreaks(inNetFile);
/* remove netfile if we created it above */
if (didNetMyself) {
   verbose(1, "Remove temporary netfile %s\n", inNetFile);
   unlink(inNetFile);
}
verbose(1, "DONE (parsing fills/gaps and getting valid breaks)\n\n");


/* 2. read the chain file. Write out those that are neither breaking nor broken chains immediately */ 
finalChainOutFile = mustOpen(outChainFileUnsorted, "w");
verbose(1, "2. reading breaking and broken chains from %s and write irrelevant chains to %s ...\n", inChainFile, outChainFileUnsorted);
readChainsOfInterest(inChainFile);
verbose(1, "DONE\n\n");

/* 3. load the sequences for all t and q chroms from the chains of interest */
verbose(1, "3. reading target and query DNA sequences for breaking and broken chains ...\n");
/* open the 2bit files if t or q seq is given as a 2bit file */
twoBitFileHash = newHash(8);
if (tIsTwoBit) {
   tbf = twoBitOpen(tNibDir);
   hashAdd(twoBitFileHash, tNibDir, tbf);
}
if (qIsTwoBit) {
   tbf = twoBitOpen(qNibDir);
   hashAdd(twoBitFileHash, qNibDir, tbf);
}
tSeqHash = newHash(0);
qSeqHash = newHash(0);
qSeqMinusStrandHash = newHash(0);
dnaUtilOpen();
hashTraverseEls(chainId2IsOfInterest, loadTandQSeqs);
verbose(1, "DONE\n\n");


/* 4. loop over all breaks and see if we can remove them accoring to the thresholds */
verbose(1, "4. loop over all breaks. Remove suspects if they pass our filters and write out deleted suspects to %s ...\n", outRemovedSuspectsFile);
if (debug) {
   suspectChainFile=mustOpen("suspect.chain", "w");
   brokenChainLfillChainFile=mustOpen("brokenChainLfill.chain", "w");
   brokenChainRfillChainFile=mustOpen("brokenChainRfill.chain", "w");
   brokenChainfillChainFile=mustOpen("brokenChainfill.chain", "w");
   suspectFillBedFile = mustOpen("suspectsAndFills.bed", "w");
}
suspectsRemovedOutBedFile=mustOpen(outRemovedSuspectsFile, "w");   /* contains in bed format the deleted suspects */
/* output file that has the newChainIDs and the breaking chainIDs that contained the suspects */
if (newChainIDDict != NULL) {
   newChainIDDictFile = mustOpen(newChainIDDict, "w");
}
if (suspectDataFile != NULL) {
   suspectDataFilePointer = mustOpen(suspectDataFile, "w");
   /* also set doPairs to FALSE */
   doPairs = FALSE;
}
chainId2NeedsRescoring = newHash(0);
loopOverBreaks();
carefulClose(&suspectsRemovedOutBedFile);
if (newChainIDDict != NULL) {
   carefulClose(&newChainIDDictFile);
}
if (suspectDataFile != NULL) {
   carefulClose(&suspectDataFilePointer);
}
if (debug) {
   carefulClose(&suspectFillBedFile);
   carefulClose(&suspectChainFile);
   carefulClose(&brokenChainLfillChainFile);
   carefulClose(&brokenChainRfillChainFile);
   carefulClose(&brokenChainfillChainFile);
}
verbose(1, "DONE\n\n");


/* write the breaking or broken chains. */
verbose(1, "5. write the (new) breaking and the broken chains to %s ...\n", outChainFileUnsorted);
hashTraverseEls(chainId2IsOfInterest, writeAndFreeChainsOfInterest);
carefulClose(&finalChainOutFile);
verbose(1, "DONE\n\n");


/* write the breaking or broken chains. */
verbose(1, "6. chainSort %s %s ...\n", outChainFileUnsorted, outChainFile);
dyStringClear(cmd);
dyStringPrintf(cmd, "chainSort %s %s", outChainFileUnsorted, outChainFile);
retVal = system(cmd->string);
if (0 != retVal)
   errAbort("ERROR: chainSort failed. Command: %s\n", cmd->string);
unlink(outChainFileUnsorted);
verbose(1, "DONE\n\n");


/* free the loaded DNAseqs */
verbose(1, "7. free memory ...\n");
freeDnaSeqHash(tSeqHash);
freeDnaSeqHash(qSeqHash);
freeDnaSeqHash(qSeqMinusStrandHash); 
freeHash(&tSeqHash);
freeHash(&qSeqHash);
freeHash(&qSeqMinusStrandHash);
freeHash(&chainId2Count);
freeHash(&chainId2IsOfInterest);
freeHash(&chainId2NeedsRescoring);
verbose(1, "DONE\n\n");


printMem();
verbose(1, "\nALL DONE. New chains are in %s. Deleted suspects in %s\n", outChainFile, outRemovedSuspectsFile);
if (debug) {
   verbose(1, "Debug mode created those output files: \n"
"\tchainsOfInterest.chain     all breaking and broken chains (chain format)\n"
"\tsuspect.chain              subChains of all suspects (chain format)\n"
"\tbrokenChainfill.chain      subChains of all left and right parts of broken chains (chain format)\n"
"\tbrokenChainLfill.chain     subChains of all left parts of broken chains (chain format)\n"
"\tbrokenChainRfill.chain     subChains of all right parts of broken chains (chain format)\n"
"\tsuspectsAndFills.bed       coordinates, scores and ratios of the suspects, the left/right parts of broken chains (bed9 format)\n");
}


return 0;
}

