/* dnaMotifFind - Locate preexisting motifs in DNA sequence. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"
#include "slog.h"
#include "jksql.h"
#include "dnaMarkov.h"
#include "dnaMotif.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "dnaMotifFind - Locate preexisting motifs in DNA sequence\n"
  "usage:\n"
  "   dnaMotifFind motifFile sequence.fa output.tab\n"
  "options:\n"
  "   -markov=level  Level of Markov background model - 0 1 or 2\n"
  "   -background=seq.fa  Sequence to use for background model\n"
  "   -threshold=N  significance threshold (ln based, default 8.0)\n"
  );
}

double threshold = 8.0;	/* Significance threshold. */
int slogThreshold;	/* Log significance. */
int markovLevel = 0;    /* Which order Markov null model to use. */

/* Zeroth order markov model. */
double mark0[5] = {1.0, 0.25, 0.25, 0.25, 0.25};
int slogMark0[5];

double mark1[5][5];
int slogMark1[5][5];
/* First order Markov model - probability that one nucleotide will follow another. */
#define slogMark1Prob(ntVal1, ntVal2) (slogMark1[(ntVal1)+1][(ntVal2)+1])

double mark2[5][5][5];
int slogMark2[5][5][5];
/* Second order Markov model - probability that a nucleotide follows two previous. */
#define slogMark2Prob(v1,v2,v3) (slogMark2[(v1)+1][(v2)+1][(v3)+1])

double codingMark2[3][5][5][5];
int slogCodingMark2[3][5][5][5];
#define slogCodingProb(frame, v1, v2, v3) (slogCodingMark2[frame][(v1)+1][(v2)+1][(v3)+1])

void getBackground(char *seqFile)
/* Figure out background model. */
{
struct dnaSeq *seqList = NULL;
seqFile = optionVal("background", seqFile);
markovLevel = optionInt("markov", markovLevel);
if (markovLevel < 0 || markovLevel > 2)
   errAbort("markov value must be 0, 1, or 2");
uglyf("Reading %s\n", seqFile);
seqList = faReadAllDna(seqFile);
dnaMark0(seqList, mark0, slogMark0);
if (markovLevel >= 1)
    dnaMark1(seqList, mark0, slogMark0, mark1, slogMark1);
if (markovLevel >= 2)
    dnaMark2(seqList, mark0, slogMark0, mark1, slogMark1, mark2, slogMark2);
dnaSeqFreeList(&seqList);
uglyf("Made background model\n");
}

struct slogMotif
/* A motif converted to scaled log. */
    {
    struct slogMotif *next;  /* Next in singly linked list. */
    char *name;	/* Motif name. */
    int columnCount;	/* Count of columns in motif. */
    int *aProb;	/* Probability of A's in each column. */
    int *cProb;	/* Probability of C's in each column. */
    int *gProb;	/* Probability of G's in each column. */
    int *tProb;	/* Probability of T's in each column. */
    };

struct slogMotif *slogMotifFromDnaMotif(struct dnaMotif *old)
/* Make a slogMotif that represent old floating point motif. */
{
struct slogMotif *m;
int i, colCount;

AllocVar(m);
m->name = cloneString(old->name);
colCount = m->columnCount = old->columnCount;
AllocArray(m->aProb, colCount);
AllocArray(m->cProb, colCount);
AllocArray(m->gProb, colCount);
AllocArray(m->tProb, colCount);
for (i=0; i<colCount; ++i)
    {
    m->aProb[i] = fSlogScale * old->aProb[i];
    m->cProb[i] = fSlogScale * old->cProb[i];
    m->gProb[i] = fSlogScale * old->gProb[i];
    m->tProb[i] = fSlogScale * old->tProb[i];
    }
return m;
}

struct slogMotif *fixMotifs(struct dnaMotif *oldList)
/* Convert list of motifs to fixed point. */
{
struct dnaMotif *old;
struct slogMotif *smList = NULL, *sm;
for (old = oldList; old != NULL; old = old->next)
    {
    sm = slogMotifFromDnaMotif(old);
    slAddHead(&smList, sm);
    }
slReverse(&smList);
return smList;
}

double motifScore(struct dnaMotif *motif, DNA *dna, int pos)
/* Score hit to DNA according to motif. */
{
int col, colCount = motif->columnCount;
double score = 1.0;
DNA base;

dna += pos;

for (col=0; col < colCount; ++col)
    {
    base = dna[col];
    if (base == 'a')
        score *= motif->aProb[col];
    else if (base == 'c')
        score *= motif->cProb[col];
    else if (base == 'g')
        score *= motif->gProb[col];
    else if (base == 't')
        score *= motif->tProb[col];
    }
return score;
}

double backgroundScore(DNA *dna, int pos, int width)
/* Return background probability for dna at position. */
{
double score = 1.0;
int i;

if (markovLevel == 0)
    {
    dna += pos;
    for (i=0; i<width; ++i)
	score *= mark0[ntVal[dna[i]]+1];
    }
else if (markovLevel == 1)
    {
    if (pos == 0 && width > 0)
        {
	score *= mark0[ntVal[dna[0]]+1];
	pos += 1;
	width -= 1;
	}
    dna += pos;
    for (i=0; i<width; ++i)
        score *= mark1[ntVal[dna[i-1]]+1][ntVal[dna[i]]+1];
    }
else if (markovLevel == 2)
    {
    if (pos == 0 && width > 0)
        {
	score *= mark0[ntVal[dna[0]]+1];
	pos += 1;
	width -= 1;
	}
    if (pos == 1 && width > 0)
        {
        score *= mark1[ntVal[dna[0]]+1][ntVal[dna[1]]+1];
	pos += 1;
	width -= 1;
	}
    for (i=0; i<width; ++i)
        score *= mark2[ntVal[dna[i-2]]+1][ntVal[dna[i-1]]+1][ntVal[dna[i]]+1];
    }
return score;
}

void findHits(FILE *f, struct dnaSeq *seq, char strand, struct dnaMotif *motif)
/* Find hits to motif that are past threshold. */
{
DNA *dna = seq->dna;
int width = motif->columnCount;
int lastPos = seq->size - width;
int pos;
double score;

for (pos=0; pos<lastPos; pos += 1)
    {
    score = log(motifScore(motif, dna, pos)/backgroundScore(dna, pos, width));
    if (score > threshold)
        {
	int bedScore = round((score - 5.0)*100);
	if (bedScore < 0) bedScore = 0;
	if (bedScore > 1000) bedScore = 1000;
	fprintf(f, "%s\t%d\t%d\t%s\t%d\t%c\n",
	     seq->name, pos, pos+width, motif->name, bedScore, strand);
	}
    }
}

void dnaMotifFind(char *motifFile, char *seqFile, char *outFile)
/* dnaMotifFind - Locate preexisting motifs in DNA sequence. */
{
FILE *f = mustOpen(outFile, "w");
struct dnaSeq *seqList = NULL, *seq;
struct dnaMotif *motifList = NULL, *motif;

threshold = optionFloat("threshold", threshold);
slogThreshold = threshold * fSlogScale;
uglyf("Threshold %f,  slogThreshold %d\n", threshold, slogThreshold);
getBackground(seqFile);

seqList = faReadAllDna(seqFile);
motifList = dnaMotifLoadAll(motifFile);
for (seq = seqList; seq != NULL; seq = seq->next)
    {
    for (motif = motifList; motif != NULL; motif = motif->next)
	findHits(f, seq, '+', motif);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
dnaUtilOpen();
if (argc != 4)
    usage();
dnaMotifFind(argv[1], argv[2], argv[3]);
return 0;
}
