/* txCdsPredict - Somewhat simple-minded ORF predictor using a weighting scheme.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "orfInfo.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"
#include "rangeTree.h"

static char const rcsid[] = "$Id: txCdsPredict.c,v 1.1 2007/03/15 05:53:48 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txCdsPredict - Somewhat simple-minded ORF predictor using a weighting scheme.\n"
  "usage:\n"
  "   txCdsPredict in.fa out.cds\n"
  "Output is five columns:\n"
  "   mrna name, mrna size, cds start, cds end, score\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

int orfInfoCmpScore(const void *va, const void *vb)
/* Compare to sort based on score (descending). */
{
const struct orfInfo *a = *((struct orfInfo **)va);
const struct orfInfo *b = *((struct orfInfo **)vb);
double diff = b->score - a->score;
if (diff < 0)
    return -1;
else if (diff > 0)
    return 1;
else
    return 0;
}

int findOrfEnd(struct dnaSeq *seq, int start)
/* Figure out end of orf that starts at start */
{
int lastPos = seq->size-3;
int i;
for (i=start+3; i<lastPos; i += 3)
    {
    if (isStopCodon(seq->dna+i))
        return i+3;
    }
return seq->size;
}

double orfScore(struct orfInfo *orf, struct dnaSeq *seq)
/* Return a fairly ad-hoc score for orf. Each base in ORF
 * is worth one point, and we go from there.... */
{
double score = orf->end - orf->start;
DNA *dna = seq->dna;

/* If we really have start, that's worth 50 bases */
if (startsWith("atg", dna + orf->start))
    {
    score += 50;

    /* Kozak condition worth 100 more. */
    if ((orf->start + 4 <= seq->size && dna[orf->start+3] == 'g') ||
        (orf->start >=3 && (dna[orf->start-3] == 'a' || dna[orf->start-3] == 'g')))
	score += 100;
    }

/* A stop codon is also worth 50 */
if (isStopCodon(dna + orf->end - 3))
    score += 50;

/* Count up upstream ATG and Kozak */
struct rbTree *upAtgRanges = rangeTreeNew(), *upKozakRanges = rangeTreeNew();
int upAtg = 0, upKozak = 0;
int i;
for (i=0; i<orf->start; ++i)
    {
    if (startsWith("atg", seq->dna + i))
        {
        int orfEnd = findOrfEnd(seq, i);
        if (orfEnd < orf->start)
            rangeTreeAdd(upAtgRanges, i, orfEnd);
        ++upAtg;
        if (isKozak(seq->dna, seq->size, i))
            {
            ++upKozak;
            if (orfEnd < orf->start)
                rangeTreeAdd(upKozakRanges, i, orfEnd);
            }
        }
    }
int upAtgBases = rangeTreeOverlapSize(upAtgRanges, 0, orf->start);
int upKozakBases = rangeTreeOverlapSize(upKozakRanges, 0, orf->start);

/* Penalize by upstream bases. */
score -= upAtgBases*0.5;
score -= upKozakBases*0.5;

/* Clean up and return total score. */
rangeTreeFree(&upAtgRanges);
rangeTreeFree(&upKozakRanges);
return score;
}

struct orfInfo *orfInfoNew(struct dnaSeq *seq, int start, int end)
/* Return new orfInfo on given sequence at given position. */
{
struct orfInfo *orf;
AllocVar(orf);
orf->rnaName = cloneString(seq->name);
orf->rnaSize = seq->size;
orf->start = start;
orf->end = end;
orf->score = orfScore(orf, seq);
return orf;
}

struct orfInfo *orfsOnRna(struct dnaSeq *seq)
/* Return scored list of all ORFs on RNA. */
{
DNA *dna = seq->dna;
int lastPos = seq->size - 3;
int startPos;
struct orfInfo *orfList = NULL, *orf;
for (startPos=0; startPos<=lastPos; ++startPos)
    {
    if (startsWith("atg", dna+startPos))
        {
	int stopPos = findOrfEnd(seq, startPos);
	orf = orfInfoNew(seq, startPos, stopPos);
	slAddHead(&orfList, orf);
	}
    }
slReverse(&orfList);
return orfList;
}

void txCdsPredict(char *inFa, char *outCds)
/* txCdsPredict - Somewhat simple-minded ORF predictor using a weighting scheme.. */
{
struct dnaSeq *rna, *rnaList = faReadAllDna(inFa);
FILE *f = mustOpen(outCds, "w");
verbose(2, "read %d seqs from %s\n", slCount(rnaList), inFa);
for (rna = rnaList; rna != NULL; rna = rna->next)
    {
    verbose(3, "%s\n", rna->name);
    struct orfInfo *orfList = orfsOnRna(rna);
    if (orfList != NULL)
	{
	slSort(&orfList, orfInfoCmpScore);
	orfInfoTabOut(orfList, f);
	}
    orfInfoFreeList(&orfList);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
txCdsPredict(argv[1], argv[2]);
return 0;
}
