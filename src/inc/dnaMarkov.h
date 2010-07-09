/* dnaMarkov - stuff to build 1st, 2nd, 3rd, and coding
 * 3rd degree Markov models for DNA. */

#ifndef DNAMARKOV_H
#define DNAMARKOV_H

void dnaMark0(struct dnaSeq *seqList, double mark0[5], int slogMark0[5]);
/* Figure out frequency of bases in input.  Results go into
 * mark0 and optionally in scaled log form into slogMark0.
 * Order is N, T, C, A, G.  (TCAG is our normal order) */

void dnaMark1(struct dnaSeq *seqList, double mark0[5], int slogMark0[5], 
	double mark1[5][5], int slogMark1[5][5]);
/* Make up 1st order Markov model - probability that one nucleotide
 * will follow another. Input is sequence and 0th order Markov models.
 * Output is first order Markov model. slogMark1 can be NULL. */

void dnaMark2(struct dnaSeq *seqList, double mark0[5], int slogMark0[5],
	double mark1[5][5], int slogMark1[5][5],
	double mark2[5][5][5], int slogMark2[5][5][5]);
/* Make up 1st order Markov model - probability that one nucleotide
 * will follow the previous two. */

void dnaMarkTriple(struct dnaSeq *seqList, 
    double mark0[5], int slogMark0[5],
    double mark1[5][5], int slogMark1[5][5],
    double mark2[5][5][5], int slogMark2[5][5][5],
    int offset, int advance, int earlyEnd);
/* Make up a table of how the probability of a nucleotide depends on the previous two.
 * Depending on offset and advance parameters this could either be a straight 2nd order
 * Markov model, or a model for a particular coding frame. */

char *dnaMark2Serialize(double mark2[5][5][5]);
// serialize a 2nd order markov model

void dnaMark2Deserialize(char *buf, double mark2[5][5][5]);
// deserialize a 2nd order markov model from buf which was serialized with dnaMark2Serialize

void dnaMarkMakeLog2(double mark2[5][5][5]);
// convert a 2nd-order markov array to log2

#endif /* DNAMARKOV_H */

