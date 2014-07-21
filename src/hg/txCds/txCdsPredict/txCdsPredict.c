/* txCdsPredict - Somewhat simple-minded ORF predictor using a weighting scheme.. */

/* Copyright (C) 2012 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "memalloc.h"
#include "localmem.h"
#include "cdsEvidence.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "bed.h"
#include "fa.h"
#include "rangeTree.h"
#include "maf.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "txCdsPredict - Somewhat simple-minded ORF predictor using a weighting scheme.\n"
  "usage:\n"
  "   txCdsPredict in.fa out.cds\n"
  "Output is 11 columns, some constant, but vary in other CDS predictors:\n"
  "1) name - name of sequence in in.fa\n"
  "2) start - CDS start within transcript, zero based\n"
  "3) end - CDS end, non-inclusive\n"
  "4) source - always 'txCdsPredict'\n"
  "5) accession - always '.'\n"
  "6) score - numbers over 800 are 90%% likely to be a protein, over 1000 almost certain\n"
  "7) startComplete - 1 if open reading frame starts with ATG\n"
  "8) endComplete - 1 if open reading frame ends with stop codon\n"
  "9) cdsCount - always 1 for txCdsPredict\n"
  "10) cdsStarts - comma separated list, always same as start for txCdsPredict\n"
  "11) cdsSizes - comma separated list, same as start-end for txCdsPredict\n"
  "options:\n"
  "   -nmd=in.bed - Use in.bed to look for evidence of nonsense mediated decay.\n"
  "   -maf=in.maf - Look for conservation of orf in other species.  Beware each\n"
  "                 species is considered independent evidence. Recommend that you\n"
  "                 use maf with just 3-5 well chosen species.  For human a good mix\n"
  "                 is human/rhesus/dog/mouse.  Use mafFrags and mafSpeciesSubset to\n"
  "                 generate this peculiar maf.\n"
  "   -anyStart - Allow ORFs that begin at first, second, or third bases in seq as well\n"
  "               as atg\n"
  );
}

static struct optionSpec options[] = {
   {"nmd", OPTION_STRING},
   {"maf", OPTION_STRING},
   {"anyStart", OPTION_BOOLEAN},
   {NULL, 0},
};

int findOrfEnd(char *dna, int dnaSize, int start)
/* Figure out end of orf that starts at start */
{
int lastPos = dnaSize-3;
int i;
for (i=start+3; i<lastPos; i += 3)
    {
    if (isStopCodon(dna+i))
        return i+3;
    }
return dnaSize;
}

int orfEndInSeq(struct dnaSeq *seq, int start)
/* Figure out end of orf that starts at start */
{
return findOrfEnd(seq->dna, seq->size, start);
}

struct orthoCds
/* Information on a single CDS at a single position. */
    {
    int start;	        /* CDS end */
    int end;            /* CDS start */
    char base;		/* Base in other species at this position. Used for debugging only. */
    };

struct orthoCdsArray
/* Information on all CDS's in other species for one RNA. */
    {
    struct orthoCdsArray *next;
    char *species;	/* Species (or database) name */
    struct orthoCds *cdsArray;  /* Array of CDSs. */
    int arraySize;	/* Size of array */
    };

void dumpOrthoArray(struct orthoCdsArray *ortho, FILE *f)
/* Print out debugging info about orthoArray. */
{
fprintf(f, "%s %d\n", ortho->species, ortho->arraySize);
int i;
for (i=0; i<ortho->arraySize; ++i)
    {
    struct orthoCds *oc = &ortho->cdsArray[i];
    char base = oc->base;
    if (base == 0) base = '?';
    fprintf(f, "   %d: %d-%d (%d) %c\n", i, oc->start, oc->end, oc->end - oc->start, base);
    }
}

int orthoScore(struct orthoCdsArray *ortho, struct cdsEvidence *orf)
/* Return score consisting of 1 per real base in overlapping orthologous CDS */
{
int biggestSize = 0;
int biggestStart = -1, biggestEnd = -1;
int i;
int score = 0;
for (i=orf->start; i<orf->end; i += 3)
    {
    struct orthoCds *cds = &ortho->cdsArray[i];
    int size = rangeIntersection(cds->start, cds->end, orf->start, orf->end);
    if (size > biggestSize)
        {
	biggestSize = size;
	biggestStart = cds->start;
	biggestEnd = cds->end;
	}
    }
biggestStart = max(biggestStart, orf->start);
biggestEnd = min(biggestEnd, orf->end);
for (i=biggestStart; i < biggestEnd; ++i)
    {
    char base = ortho->cdsArray[i].base;
    if (base != '.' && base != '-')
        score += 1;
    }
// uglyf("orthoScore for %s %d-%d %s is %d\n", orf->name, orf->start, orf->end, ortho->species, score);
return score;
}

double orfScore(struct cdsEvidence *orf, struct dnaSeq *seq, int *upAtgCount, int *upKozakCount,
	int lastIntronPos, struct orthoCdsArray *orthoList, double orthoWeightPer)
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

/* Add in bits from ortho species. */
struct orthoCdsArray *ortho;
for (ortho = orthoList; ortho != NULL; ortho = ortho->next)
    {
    score += orthoWeightPer * orthoScore(ortho, orf);
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
        int orfEnd = orfEndInSeq(seq, i);
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

struct cdsEvidence *createCds(struct dnaSeq *seq, int start, int end,
	int *upAtgCount, int *upKozakCount, int lastIntronPos,
	struct orthoCdsArray *orthoList, double orthoWeightPer)
/* Return new cdsEvidence on given sequence at given position. */
{
struct cdsEvidence *orf;
AllocVar(orf);
int size = end - start;
size -= size % 3;
end = start + size;
orf->name = cloneString(seq->name);
orf->start = start;
orf->end = end;
orf->source = cloneString("txCdsPredict");
orf->accession = cloneString(".");
orf->score = orfScore(orf, seq, upAtgCount, upKozakCount, lastIntronPos,
	orthoList, orthoWeightPer);
orf->startComplete = startsWith("atg", seq->dna + start);
orf->endComplete = isStopCodon(seq->dna + end - 3);
orf->cdsCount = 1;
AllocArray(orf->cdsStarts, 1);
orf->cdsStarts[0] = start;
AllocArray(orf->cdsSizes, 1);
orf->cdsSizes[0] = size;
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

void applyOrf(int start, int end, char *xDna, int *xToN, struct orthoCds *array, int arraySize)
/* Given start/end in xeno coordinates, map to native coordinates and save info
 * to for all codons in frame in array. */
{
int xIx;
int nStart = xToN[start], nEnd = xToN[end];
// uglyf("applyOrf %d %d (native %d %d of %d)\n", start, end, nStart, nEnd, arraySize);
for (xIx=start; xIx<end; xIx += 3)
    {
    int nIx = xToN[xIx];
    if (nIx < arraySize)
	{
	struct orthoCds *oc = &array[nIx];
	oc->start = nStart;
	oc->end = nEnd;
	}
    }
}

void fillInArrayFromPair(struct lm *lm, struct mafComp *native, struct mafComp *xeno,
	struct orthoCds *array, int arraySize, int symCount)
/* Figure out the CDS in xeno for each position in native. */
{
char *nText = native->text, *xText = xeno->text;
int nSize = arraySize, xSize = symCount - countChars(xText, '-');

/* Create an array that for each point in native gives you the index of corresponding
 * point in xeno, and another array that does the opposite. */
int *nToX, *xToN;
lmAllocArray(lm, nToX, nSize+1);
lmAllocArray(lm, xToN, xSize+1);
int i;
int nIx = 0, xIx = 0;
for (i=0; i<symCount; ++i)
    {
    char n = nText[i], x = xText[i];
    if (n == '.')
       errAbort("Dot in native component %s of maf. Can't handle it.", native->src);
    nToX[nIx] = xIx;
    xToN[xIx] = nIx;
    if (n != '-')
	{
	array[nIx].base = x;
	nToX[nIx] = xIx;
	++nIx;
	}
    if (x != '-')
       ++xIx;
    }
assert(xIx == xSize);
assert(nIx == nSize);

/* Put an extra value at end of arrays to simplify logic. */
nToX[nSize] = xSize;
xToN[xSize] = nSize;

/* Create xeno sequence without the '-' chars */
char *xDna = lmCloneString(lm, xText);
tolowers(xDna);
stripChar(xDna, '-');

#ifdef DEBUG
uglyf("xToN:");
for (i=0; i<xSize; ++i) uglyf(" %d", xToN[i]);
uglyf("\n");
#endif /* DEBUG */

/* Step through this, one frame at a time, looking for best ORF */
int frame;
for (frame=0; frame<3; ++frame)
    {
    /* Calculate some things constant for this frame, and deal with
     * ORF that starts at beginning (may not have ATG) */
    int lastPos = xSize-3;
    int frameDnaSize = xSize-frame;
    int start = frame, end = findOrfEnd(xDna, frameDnaSize, frame);
    applyOrf(start, end, xDna, xToN, array, arraySize);
    for (start = end; start<=lastPos; )
        {
	// uglyf("start %d %c%c%c\n", start, xDna[start], xDna[start+1], xDna[start+2]);
	if (startsWith("atg", xDna+start))
	    {
	    end = findOrfEnd(xDna, frameDnaSize, start);
	    applyOrf(start, end, xDna, xToN, array, arraySize);
	    start = end;
	    }
	else
	    start += 3;
	}
    }

}

struct orthoCdsArray *calcOrthoList(struct mafAli *maf, struct lm *lm)
/* Given maf, figure out orthoCdsArray list, one for each other
 * species.  (Assume first species is native.) */
{
struct orthoCdsArray *array, *arrayList = NULL;
struct mafComp *nativeComp = maf->components;
int nativeSize = nativeComp->size;
struct mafComp *comp;
for (comp = maf->components->next; comp != NULL; comp = comp->next)
    {
    AllocVar(array);
    array->species = lmCloneString(lm, comp->src);
    array->arraySize = nativeSize;
    lmAllocArray(lm, array->cdsArray, nativeSize);
    fillInArrayFromPair(lm, nativeComp, comp, array->cdsArray, nativeSize, maf->textSize);
    slAddHead(&arrayList, array);
    }
slReverse(&arrayList);
return arrayList;
}

struct cdsEvidence *orfsOnRna(struct dnaSeq *seq, struct hash *nmdHash, struct hash *mafHash,
	int otherSpeciesCount, boolean anyStart)
/* Return scored list of all ORFs on RNA. */
{
DNA *dna = seq->dna;
int lastPos = seq->size - 3;
int startPos;
struct cdsEvidence *orfList = NULL, *orf;
struct lm *lm = lmInit(64*1024);

/* Figure out the key piece of info for NMD. */
int lastIntronPos = findLastIntronPos(nmdHash, seq->name);
double orthoWeightPer = 0;
struct orthoCdsArray *orthoList = NULL;

/* Calculate stuff useful for orthology */
if (otherSpeciesCount > 0)
    {
    orthoWeightPer = 1.0/otherSpeciesCount;
    struct mafAli *maf = hashFindVal(mafHash, seq->name);
    if (maf != NULL)
	{
	orthoList = calcOrthoList(maf, lm);
	// uglyf("%s: ", seq->name);
	// dumpOrthoArray(orthoArray, uglyOut);
	}
    }

/* Allocate some arrays that keep track of bases in
 * upstream.  This dramatically speeds up processing
 * of TTN and other long transcripts which otherwise
 * can take almost a minute each. */
int *upAtgCount, *upKozakCount;
lmAllocArray(lm, upAtgCount, seq->size);
lmAllocArray(lm, upKozakCount, seq->size);
calcUpstreams(seq, upAtgCount, upKozakCount);

/* Go through sequence making up a record for each 
 * start codon we find. */
for (startPos=0; startPos<=lastPos; ++startPos)
    {
    if (startsWith("atg", dna+startPos) || (anyStart && startPos < 3))
        {
	int stopPos = orfEndInSeq(seq, startPos);
	orf = createCds(seq, startPos, stopPos, upAtgCount, upKozakCount, 
		lastIntronPos, orthoList, orthoWeightPer);
	slAddHead(&orfList, orf);
	}
    }
slReverse(&orfList);

/* Clean up and go home. */
lmCleanup(&lm);
return orfList;
}

void txCdsPredict(char *inFa, char *outCds, char *nmdBed, char *mafFile, boolean anyStart)
/* txCdsPredict - Somewhat simple-minded ORF predictor using a weighting scheme.. */
{
struct dnaSeq *rna, *rnaList = faReadAllDna(inFa);
verbose(2, "Read %d sequences from %s\n", slCount(rnaList), inFa);

/* Make up hash of bed records for NMD analysis. */
struct hash *nmdHash = hashNew(18);
if (nmdBed != NULL)
    {
    struct bed *bed, *bedList = bedLoadNAll(nmdBed, 12);
    for (bed = bedList; bed != NULL; bed = bed->next)
        hashAdd(nmdHash, bed->name, bed);
    verbose(2, "Read %d beds from %s\n", nmdHash->elCount, nmdBed);
    }

/* Make up hash of maf records for conservation analysis. */
struct hash *mafHash = hashNew(18);
int otherSpeciesCount = 0;
if (mafFile != NULL)
    {
    struct mafFile *mf = mafReadAll(mafFile);
    struct mafAli *maf;
    for (maf = mf->alignments; maf != NULL; maf = maf->next)
	hashAdd(mafHash, maf->components->src, maf);
    verbose(2, "Read %d alignments from %s\n", mafHash->elCount, mafFile);

    struct hash *uniqSpeciesHash = hashNew(0);
    for (maf = mf->alignments; maf != NULL; maf = maf->next)
        {
	struct mafComp *comp;
	// Skip over first component since it's the reference
	for (comp = maf->components->next;  comp != NULL; comp = comp->next)
	    hashStore(uniqSpeciesHash, comp->src);
	}
    otherSpeciesCount = uniqSpeciesHash->elCount;
    verbose(2, "%d other species in %s\n", otherSpeciesCount, mafFile);
    }

FILE *f = mustOpen(outCds, "w");
for (rna = rnaList; rna != NULL; rna = rna->next)
    {
    verbose(3, "%s\n", rna->name);
    struct cdsEvidence *orfList = orfsOnRna(rna, nmdHash, mafHash, otherSpeciesCount, anyStart);
    if (orfList != NULL)
	{
	slSort(&orfList, cdsEvidenceCmpScore);
	cdsEvidenceTabOut(orfList, f);
	}
    cdsEvidenceFreeList(&orfList);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
// pushCarefulMemHandler(1024*1024*512*3);
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
txCdsPredict(argv[1], argv[2], optionVal("nmd", NULL), 
	optionVal("maf", NULL), optionExists("anyStart"));
return 0;
}
