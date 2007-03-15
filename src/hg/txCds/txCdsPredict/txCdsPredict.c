/* txCdsPredict - Somewhat simple-minded ORF predictor using a weighting scheme.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "orfInfo.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "bed.h"
#include "fa.h"
#include "rangeTree.h"

static char const rcsid[] = "$Id: txCdsPredict.c,v 1.3 2007/03/15 08:16:33 kent Exp $";

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
  "   -nmd=in.bed - Use in.bed to look for evidence of nonsense mediated decay.\n"
  );
}

static struct optionSpec options[] = {
   {"nmd", OPTION_STRING},
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


double orfScore(struct orfInfo *orf, struct dnaSeq *seq, int *upAtgCount, int *upKozakCount,
	int lastIntronPos)
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

/* Penalize by upstream bases. */
score -= upAtgCount[orf->start]*0.5;
score -= upKozakCount[orf->start]*0.5;

/* Penalize NMD */
if (lastIntronPos > 0)
    {
    int nmdDangle = lastIntronPos - orf->end;
    if (nmdDangle >= 55)
        score -= 400;
    }
return score;
}

void setArrayCountsFromRangeTree(struct rbTree *rangeTree, int *array, int seqSize)
/* Given a range tree that covers stuff from 0 to seqSize, 
 * fill in array with amount of bases covered by range tree
 * before a given position in array. */
{
struct range *range, *rangeList = rangeTreeList(rangeTree);
if (rangeList == NULL)
    return;
range = rangeList;
int i, count = 0;
for (i=0; i<seqSize; ++i)
    {
    array[i] = count;
    if (i >= range->end)
	{
        range = range->next;
	if (range == NULL)
	    {
	    for (;i<seqSize; ++i)
	       array[i] = count; 
	    return;
	    }
	}
    if (range->start <= i)
        ++count;
    }
}


void calcUpstreams(struct dnaSeq *seq, int *upAtgCount, int *upKozakCount)
/* Count up upstream ATG and Kozak */
{
struct rbTree *upAtgRanges = rangeTreeNew(), *upKozakRanges = rangeTreeNew();
int endPos = seq->size-3;
int i;
for (i=0; i<=endPos; ++i)
    {
    if (startsWith("atg", seq->dna + i))
        {
        int orfEnd = findOrfEnd(seq, i);
	rangeTreeAdd(upAtgRanges, i, orfEnd);
        if (isKozak(seq->dna, seq->size, i))
	    rangeTreeAdd(upKozakRanges, i, orfEnd);
        }
    }
setArrayCountsFromRangeTree(upAtgRanges, upAtgCount, seq->size);
setArrayCountsFromRangeTree(upKozakRanges, upKozakCount, seq->size);
rangeTreeFree(&upAtgRanges);
rangeTreeFree(&upKozakRanges);
}

struct orfInfo *orfInfoNew(struct dnaSeq *seq, int start, int end,
	int *upAtgCount, int *upKozakCount, int lastIntronPos)
/* Return new orfInfo on given sequence at given position. */
{
struct orfInfo *orf;
AllocVar(orf);
orf->rnaName = cloneString(seq->name);
orf->rnaSize = seq->size;
orf->start = start;
orf->end = end;
orf->score = orfScore(orf, seq, upAtgCount, upKozakCount, lastIntronPos);
return orf;
}

int findLastIntronPos(struct hash *bedHash, char *name)
/* Find last intron position in RNA coordinates if we have
 * a bed for this mRNA.  Otherwise (or if it's single exon)
 * return 0. */
{
struct bed *bed = hashFindVal(bedHash, name);
if (bed == NULL)
    return 0;
if (bed->blockCount < 2)
    return 0;
int rnaSize = bedTotalBlockSize(bed);
if (bed->strand[0] == '+')
    return rnaSize - bed->blockSizes[bed->blockCount-1];
else
    return rnaSize - bed->blockSizes[0];
}

struct orfInfo *orfsOnRna(struct dnaSeq *seq, struct hash *nmdHash)
/* Return scored list of all ORFs on RNA. */
{
DNA *dna = seq->dna;
int lastPos = seq->size - 3;
int startPos;
struct orfInfo *orfList = NULL, *orf;
int lastIntronPos = findLastIntronPos(nmdHash, seq->name);

/* Allocate some arrays that keep track of bases in
 * upstream.  This dramatically speeds up processing
 * of TTN and other long transcripts which otherwise
 * can take almost a minute each. */
int *upAtgCount, *upKozakCount;
AllocArray(upAtgCount, seq->size);
AllocArray(upKozakCount, seq->size);
calcUpstreams(seq, upAtgCount, upKozakCount);

/* Go through sequence making up a record for each 
 * start codon we find. */
for (startPos=0; startPos<=lastPos; ++startPos)
    {
    if (startsWith("atg", dna+startPos))
        {
	int stopPos = findOrfEnd(seq, startPos);
	orf = orfInfoNew(seq, startPos, stopPos, upAtgCount, upKozakCount, lastIntronPos);
	slAddHead(&orfList, orf);
	}
    }
slReverse(&orfList);

/* Clean up and go home. */
freeMem(upAtgCount);
freeMem(upKozakCount);
return orfList;
}

void txCdsPredict(char *inFa, char *outCds, char *nmdBed)
/* txCdsPredict - Somewhat simple-minded ORF predictor using a weighting scheme.. */
{
struct dnaSeq *rna, *rnaList = faReadAllDna(inFa);

/* Make up hash of bed records for NMD analysis. */
struct hash *nmdHash = hashNew(18);
if (nmdBed != NULL)
    {
    struct bed *bed, *bedList = bedLoadNAll(nmdBed, 12);
    for (bed = bedList; bed != NULL; bed = bed->next)
        hashAdd(nmdHash, bed->name, bed);
    }

FILE *f = mustOpen(outCds, "w");
verbose(2, "read %d seqs from %s\n", slCount(rnaList), inFa);
for (rna = rnaList; rna != NULL; rna = rna->next)
    {
    verbose(3, "%s\n", rna->name);
    struct orfInfo *orfList = orfsOnRna(rna, nmdHash);
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
txCdsPredict(argv[1], argv[2], optionVal("nmd", NULL));
return 0;
}
