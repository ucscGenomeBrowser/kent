/* dnaMarkov - stuff to build 1st, 2nd, 3rd, and coding
 * 3rd degree Markov models for DNA. */

#include "common.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "slog.h"
#include "dnaMarkov.h"


void dnaMark0(struct dnaSeq *seqList, double mark0[5], int slogMark0[5])
/* Figure out frequency of bases in input.  Results go into
 * mark0 and optionally in scaled log form into slogMark0.
 * Order is N, T, C, A, G.  (TCAG is our normal order) */
{
struct dnaSeq *seq;
int histo[4];
int oneHisto[4];
double total;
int i;
double *freq = mark0+1;

zeroBytes(histo, sizeof(histo));
for (seq = seqList; seq != NULL; seq = seq->next)
    {
    dnaBaseHistogram(seq->dna, seq->size, oneHisto);
    for (i=0; i<4; ++i)
        histo[i] += oneHisto[i];
    }
total = histo[0] + histo[1] + histo[2] + histo[3];
freq[-1] = 1.0;
for (i=0; i<4; ++i)
    freq[i] = (double)histo[i] / total;
if (slogMark0 != NULL)
    {
    int *slogFreq = slogMark0 + 1;
    slogFreq[-1] = 0;
    for (i=0; i<4; ++i)
	slogFreq[i] = slog(freq[i]);
    }
}


void dnaMark1(struct dnaSeq *seqList, double mark0[5], int slogMark0[5], 
	double mark1[5][5], int slogMark1[5][5])
/* Make up 1st order Markov model - probability that one nucleotide
 * will follow another. Input is sequence and 0th order Markov models.
 * Output is first order Markov model. slogMark1 can be NULL. */
{
struct dnaSeq *seq;
DNA *dna, *endDna;
int i,j;
int histo[5][5];
int hist1[5];

zeroBytes(histo, sizeof(histo));
zeroBytes(hist1, sizeof(hist1));
for (seq = seqList; seq != NULL; seq = seq->next)
    {
    dna = seq->dna;
    endDna = dna + seq->size-1;
    for (;dna < endDna; ++dna)
        {
        i = ntVal[(int)dna[0]];
        j = ntVal[(int)dna[1]];
        hist1[i+1] += 1;
        histo[i+1][j+1] += 1;
        }
    }
for (i=0; i<5; ++i)
    {
    for (j=0; j<5; ++j)
        {
        double mark1Val;
        int matVal = histo[i][j] + 1;
        mark1Val = ((double)matVal)/(hist1[i]+5);
        mark1[i][j] = mark1Val;
	if (slogMark1 != NULL)
	    slogMark1[i][j] = slog(mark1Val);
        }
    }
for (i=0; i<5; ++i)
    {
    mark1[i][0] = 1;
    mark1[0][i] = mark0[i];
    if (slogMark1 != NULL)
	{
	slogMark1[i][0] = 0;
	slogMark1[0][i] = slogMark0[i];
	}
    }
}

void dnaMarkTriple(struct dnaSeq *seqList, 
    double mark0[5], int slogMark0[5],
    double mark1[5][5], int slogMark1[5][5],
    double mark2[5][5][5], int slogMark2[5][5][5],
    int offset, int advance, int earlyEnd)
/* Make up a table of how the probability of a nucleotide depends on the previous two.
 * Depending on offset and advance parameters this could either be a straight 2nd order
 * Markov model, or a model for a particular coding frame. */
{
struct dnaSeq *seq;
DNA *dna, *endDna;
int i,j,k;
int histo[5][5][5];
int hist2[5][5];
int total = 0;
zeroBytes(histo, sizeof(histo));
zeroBytes(hist2, sizeof(hist2));
for (seq = seqList; seq != NULL; seq = seq->next)
    {
    dna = seq->dna;
    endDna = dna + seq->size - earlyEnd - 2;
    dna += offset;
    for (;dna < endDna; dna += advance)
        {
        i = ntVal[(int)dna[0]];
        j = ntVal[(int)dna[1]];
        k = ntVal[(int)dna[2]];
        hist2[i+1][j+1] += 1;
        histo[i+1][j+1][k+1] += 1;
        total += 1;
        }
    }
for (i=0; i<5; ++i)
    {
    for (j=0; j<5; ++j)
        {
        for (k=0; k<5; ++k)
            {
            double markVal;
            int matVal = histo[i][j][k]+1;
            if (i == 0 || j == 0 || k == 0)
                {
                if (k == 0)
                    {
                    mark2[i][j][k] = 1;
		    if (slogMark2 != NULL)
			slogMark2[i][j][k] = 0;
                    }
                else if (j == 0)
                    {
                    mark2[i][j][k] = mark0[k];
		    if (slogMark2 != NULL)
			slogMark2[i][j][k] = slogMark0[k];
                    }
                else if (i == 0)
                    {
                    mark2[i][j][k] = mark1[j][k];
		    if (slogMark2 != NULL)
			slogMark2[i][j][k] = slogMark1[j][k];
                    }
                }
            else
                {
                markVal = ((double)matVal)/(hist2[i][j]+5);
                mark2[i][j][k] = markVal;
		if (slogMark2 != NULL)
		    slogMark2[i][j][k] = slog(markVal);
                }
            }
        }
    }
}

void dnaMark2(struct dnaSeq *seqList, double mark0[5], int slogMark0[5],
	double mark1[5][5], int slogMark1[5][5],
	double mark2[5][5][5], int slogMark2[5][5][5])
/* Make up 1st order Markov model - probability that one nucleotide
 * will follow the previous two. */
{
dnaMarkTriple(seqList, mark0, slogMark0, mark1, slogMark1, 
	mark2, slogMark2, 0, 1, 0);
}

#define SIG 10

char *dnaMark2Serialize(double mark2[5][5][5])
// serialize a 2nd order markov model
{
int i, j, k;
int offset = 0;
char *buf = NULL;
int bufLen = 5*5*5 * (SIG + 3) + 1;
buf = needMem(bufLen);
for(i = 0; i < 5; i++)
    for(j = 0; j < 5; j++)
        for(k = 0; k < 5; k++)
            {
            if(offset)
                {
                sprintf(buf + offset, ";%1.*f", SIG, mark2[i][j][k]);
                offset += (SIG + 3);
                }
            else
                {
                sprintf(buf + offset, "%1.*f", SIG, mark2[i][j][k]);
                offset += (SIG + 2);
                }
            }
buf[offset] = 0;
return buf;
}

void dnaMark2Deserialize(char *buf, double mark2[5][5][5])
// deserialize a 2nd order markov model
{
int i, j, k;
int offset = 0;
for(i = 0; i < 5; i++)
    for(j = 0; j < 5; j++)
        for(k = 0; k < 5; k++)
            {
            float f;
            if(offset)
                {
                sscanf(buf + offset, ";%f", &f);
                mark2[i][j][k] = f;
                offset += (SIG + 3);
                }
            else
                {
                sscanf(buf + offset, "%f", &f);
                mark2[i][j][k] = f;
                offset += (SIG + 2);
                }
            }
}

void dnaMarkMakeLog2(double mark2[5][5][5])
// convert a 2nd-order markov array to log2
{
int i, j, k;
for(i = 0; i < 5; i++)
    for(j = 0; j < 5; j++)
        for(k = 0; k < 5; k++)
            mark2[i][j][k] = logBase2(mark2[i][j][k]);
}
