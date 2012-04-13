#ifndef GPFX_H
#define GPFX_H

#include "variant.h"

struct gpFx
{
struct gpFx *next;
enum gpFxType { 
	gpFxNone, 
	gpFxUtr5, 
	gpFxUtr3, 
	gpFxSynon, 
	gpFxNonsynon, 
	gpFxSplice5, 
	gpFxUpstream, 
	gpFxDownstream, 
	gpFxIntron 
	} gpFxType; ;

int    gpFxNumber;        // exon or intron number
int    gpFxTransOffset;    //offset in transcript
char   *gpFxBaseChange;     //base change in transcript
int    gpFxCodonChange;    //codon triplet change in transcript
int    gpFxProteinOffset;  //offset in protein
int    gpFxProteinChange;  //peptide change in protein
};

struct gpFx *gpFxPredEffect(struct variant *variant, struct genePred *pred,
    char **returnTranscript, char **returnCoding);
// return the predicted effect(s) of a variation list on a genePred

// number of bases up or downstream that we flag
#define GPRANGE 500

#endif /* GPFX_H */
