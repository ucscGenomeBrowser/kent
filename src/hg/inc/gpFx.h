#ifndef GPFX_H
#define GPFX_H

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
	gpFxIntron 
	} gpFxType; ;

int    gpFxNumber;        // exon or intron number
int    gpFxTransOffset;    //offset in transcript
char   *gpFxBaseChange;     //base change in transcript
int    gpFxCodonChange;    //codon triplet change in transcript
int    gpFxProteinOffset;  //offset in protein
int    gpFxProteinChange;  //peptide change in protein
};

struct gpFx *gpFxPredEffect(struct pgSnp *pgSnp, struct genePred *pred);
// return the predicted effect(s) of a variation on a genePred

#endif /* GPFX_H */
