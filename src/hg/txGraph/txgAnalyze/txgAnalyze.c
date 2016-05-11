/* txgAnalyze - Analyse transcription graph for alt exons, alt 3', alt 5', 
 * retained introns, alternative promoters, etc.. */

/* Copyright (C) 2007 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "localmem.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "nibTwo.h"
#include "ggTypes.h"
#include "txGraph.h"
#include "rangeTree.h"

char *refType = "refSeq";
FILE *fConst = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txgAnalyze - Analyse transcription graph for alt exons, alt 3', alt 5', \n"
  "retained introns, alternative promoters, etc.\n"
  "usage:\n"
  "   txgAnalyze in.txg genome.2bit out.ana\n"
  "options:\n"
  "   refType=xxx - The type for the reference type of evidence, used for\n"
  "                 the refJoined records indicating transcription across\n"
  "                 what's usually considered 2 genes.  Default is %s.\n"
  "   constExon=out.bed - Write out constituative exons here.  This are\n"
  "                 refSeq exons that don't overlap introns, and that\n"
  "                 are supported by at least 10 lines of evidence.\n"
  , refType
  );
}

static struct optionSpec options[] = {
   {"refType", OPTION_STRING},
   {"constExon", OPTION_STRING},
   {NULL, 0},
};

struct range *retainedIntrons(struct txGraph *graph, FILE *f)
/* Write out instances of retained introns. Returns a list of exons
 * harboring them. */
{
struct txEdge *intron, *exon;
struct txVertex *v = graph->vertices;
struct range *exonsWithIntrons = NULL;
for (intron = graph->edgeList; intron != NULL; intron = intron->next)
    {
    if (intron->type == ggIntron)
        {
	int iStart = v[intron->startIx].position;
	int iEnd = v[intron->endIx].position;
	for (exon = graph->edgeList; exon != NULL; exon = exon->next)
	    {
	    if (exon->type == ggExon)
		{
		int eStart = v[exon->startIx].position;
		int eEnd = v[exon->endIx].position;
		if (eStart < iStart && iEnd < eEnd)
		     {
		     struct range *affectedExon;
		     AllocVar(affectedExon);
		     affectedExon->start = eStart;
		     affectedExon->end = eEnd;
		     slAddHead(&exonsWithIntrons, affectedExon);
		     fprintf(f, "%s\t%d\t%d\t%s\t0\t%s\n", graph->tName,
			    iStart, iEnd, "retainedIntron", graph->strand);
		     }
		}
	    }
	}
    }
return exonsWithIntrons;
}

void cassetteExons(struct txGraph *graph, FILE *f)
/* Write out instances of cassette exons */
{
struct txEdge *intron, *exon;
struct txVertex *v = graph->vertices;
for (exon = graph->edgeList; exon != NULL; exon = exon->next)
    {
    if (exon->type == ggExon)
        {
	struct txVertex *vs = &v[exon->startIx];
	struct txVertex *ve = &v[exon->endIx];
	if (vs->type == ggHardStart && ve->type == ggHardEnd)
	    {
	    int eStart = vs->position;
	    int eEnd = ve->position;
	    for (intron = graph->edgeList; intron != NULL; intron = intron->next)
		{
		if (intron->type == ggIntron)
		    {
		    int iStart = v[intron->startIx].position;
		    int iEnd = v[intron->endIx].position;
		    if (iStart < eStart && eEnd < iEnd)
			 fprintf(f, "%s\t%d\t%d\t%s\t0\t%s\n", graph->tName,
				eStart, eEnd, "cassetteExon", graph->strand);
		    }
		}
	    }
	}
    }
}

boolean inRangeList(struct range *rangeList, int start, int end)
/* Return TRUE if interval from start to end is in range list */
{
struct range *range;
for (range = rangeList; range != NULL; range = range->next)
    if (range->start == start && range->end == end)
        return TRUE;
return FALSE;
}

void altThreePrime(struct txGraph *graph, struct range *exonsWithIntrons, FILE *f)
/* Write out instances of alt 3' prime splice sites on plus
 * strand (and alt 5' on the minus strand). */
{
struct txEdge *e1, *e2;
struct txVertex *v = graph->vertices;
struct lm *lm = lmInit(0);
struct rbTree *tree = rangeTreeNew();
struct range *range, *rangeList = NULL;
for (e1 = graph->edgeList; e1 != NULL; e1 = e1->next)
    {
    if (e1->type == ggExon)
        {
	int e1Start = v[e1->startIx].position;
	int e1End = v[e1->endIx].position;
	boolean  e1HardEnd = (v[e1->endIx].type == ggHardEnd);
	if (e1HardEnd)
	    {
	    for (e2 = graph->edgeList; e2 != NULL; e2 = e2->next)
		{
		if (e2->type == ggExon)
		    {
		    int e2Start = v[e2->startIx].position;
		    int e2End = v[e2->endIx].position;
		    boolean  e2HardEnd = (v[e2->endIx].type == ggHardEnd);
		    if (e2HardEnd && e1Start == e2Start && e1End != e2End)
			 {
			 int aStart = min(e1End, e2End);
			 int aEnd = max(e1End, e2End);
			 if (!inRangeList(exonsWithIntrons, e1Start, e1End) 
			 	&& !inRangeList(exonsWithIntrons, e2Start, e2End) 
			 	&& !inRangeList(rangeList, aStart, aEnd))
			     {
			     lmAllocVar(lm, range);
			     range->start = aStart;
			     range->end = aEnd;
			     slAddHead(&rangeList, range);
			     fprintf(f, "%s\t%d\t%d\t%s\t0\t%s\n", graph->tName,
				    aStart, aEnd, 
				    (graph->strand[0] == '+' ? "altFivePrime" : "altThreePrime"),
				    graph->strand);
			     }
			 }
		    }
		}
	    }
	}
    }
rbTreeFree(&tree);
lmCleanup(&lm);
}

void altFivePrime(struct txGraph *graph, struct range *exonsWithIntrons, FILE *f)
/* Write out instances of alt 5' prime splice sites on plus strand
 * (and alt 3' splice sites on minus strand). */
{
struct txEdge *e1, *e2;
struct txVertex *v = graph->vertices;
struct lm *lm = lmInit(0);
struct rbTree *tree = rangeTreeNew();
struct range *range, *rangeList = NULL;
for (e1 = graph->edgeList; e1 != NULL; e1 = e1->next)
    {
    if (e1->type == ggExon)
        {
	int e1Start = v[e1->startIx].position;
	int e1End = v[e1->endIx].position;
	boolean  e1HardStart = (v[e1->startIx].type == ggHardStart);
	if (e1HardStart)
	    {
	    for (e2 = graph->edgeList; e2 != NULL; e2 = e2->next)
		{
		if (e2->type == ggExon)
		    {
		    int e2Start = v[e2->startIx].position;
		    int e2End = v[e2->endIx].position;
		    boolean  e2HardStart = (v[e2->startIx].type == ggHardStart);
		    if (e2HardStart && e1Start != e2Start && e1End == e2End)
			 {
			 int aStart = min(e1Start, e2Start);
			 int aEnd = max(e1Start, e2Start);
			 if (!inRangeList(exonsWithIntrons, e1Start, e1End) 
			 	&& !inRangeList(exonsWithIntrons, e2Start, e2End) 
			 	&& !inRangeList(rangeList, aStart, aEnd))
			     {
			     lmAllocVar(lm, range);
			     range->start = aStart;
			     range->end = aEnd;
			     slAddHead(&rangeList, range);
			     fprintf(f, "%s\t%d\t%d\t%s\t0\t%s\n", graph->tName,
				    aStart, aEnd, 
				    (graph->strand[0] == '-' ? "altFivePrime" : "altThreePrime"),
				    graph->strand);
			     }
			 }
		    }
		}
	    }
	}
    }
rbTreeFree(&tree);
lmCleanup(&lm);
}

void checkBleedBefore(struct txGraph *graph, struct txVertex *start, struct txVertex *end,
	FILE *f)
/* Start/end is a half-hard edge with soft start. Look for exons that share
 * hard end, and note how far the soft start extends past their hard start. */
{
int minBleed = BIGNUM;
struct txVertex *hardStart = NULL;
struct txEdge *edge;
for (edge = graph->edgeList; edge != NULL; edge = edge->next)
    {
    if (edge->type == ggExon)
        {
	struct txVertex *eStart = &graph->vertices[edge->startIx];
	struct txVertex *eEnd = &graph->vertices[edge->endIx];
	if (eEnd == end && eStart != start && eStart->type == ggHardStart)
	    {
	    int bleed = eStart->position - start->position;
	    if (bleed > 0 && bleed < minBleed)
		 {
	         minBleed = bleed;
		 hardStart = eStart;
		 }
	    }
	}
    }
if (hardStart != NULL)
    {
     fprintf(f, "%s\t%d\t%d\t%s\t0\t%s\n", graph->tName,
	    start->position, hardStart->position, "bleedingExon", graph->strand);
    }
}

void checkBleedAfter(struct txGraph *graph, struct txVertex *start, struct txVertex *end,
	FILE *f)
/* Start/end is a half-hard edge with soft end. Look for exons that share
 * hard start, and note how far the soft end extends past their hard start. */
{
int minBleed = BIGNUM;
struct txVertex *hardEnd = NULL;
struct txEdge *edge;
for (edge = graph->edgeList; edge != NULL; edge = edge->next)
    {
    if (edge->type == ggExon)
        {
	struct txVertex *eStart = &graph->vertices[edge->startIx];
	struct txVertex *eEnd = &graph->vertices[edge->endIx];
	if (eStart == start && eEnd != end && eEnd->type == ggHardEnd)
	    {
	    int bleed = end->position - eEnd->position;
	    if (bleed > 0 && bleed < minBleed)
		 {
	         minBleed = bleed;
		 hardEnd = eEnd;
		 }
	    }
	}
    }
if (hardEnd != NULL)
    {
     fprintf(f, "%s\t%d\t%d\t%s\t0\t%s\n", graph->tName,
	    hardEnd->position, end->position, "bleedingExon", graph->strand);
    }
}

void bleedsIntoIntrons(struct txGraph *graph, FILE *f)
/* Write out cases where a soft start or end bleeds into
 * an intron. */
{
struct txEdge *edge;
for (edge = graph->edgeList; edge != NULL; edge = edge->next)
    {
    if (edge->type == ggExon)
        {
	struct txVertex *start = &graph->vertices[edge->startIx];
	struct txVertex *end = &graph->vertices[edge->endIx];
	if (start->type == ggSoftStart && end->type == ggHardEnd)
	    checkBleedBefore(graph, start, end, f);
	else if (start->type == ggHardStart && end->type == ggSoftEnd)
	    checkBleedAfter(graph, start, end, f);
	}
    }
}

void altPromoter(struct txGraph *graph, FILE *f)
/* Write out alternative promoters. */
{
/* Set up an array to keep track of whether any edge ends in vertex. */
boolean vNotStart[graph->vertexCount];	
boolean vNotEnd[graph->vertexCount];
struct lm *lm = lmInit(0);
struct range *promoterList = NULL;
struct range *polyaList = NULL;
int i;
for (i=0; i<graph->vertexCount; ++i)
    vNotStart[i] = FALSE;
struct txEdge *edge;
for (edge = graph->edgeList; edge != NULL; edge = edge->next)
    {
    if (graph->strand[0] == '+')
	{
        vNotStart[edge->endIx] = TRUE;
	vNotEnd[edge->startIx] = TRUE;
	}
    else
	{
        vNotStart[edge->startIx] = TRUE;
	vNotEnd[edge->endIx] = TRUE;
	}
    }
for (i=0; i<graph->vertexCount; ++i)
    {
    int start = 0, end = 0;
    boolean isTxStart = FALSE, isTxEnd = FALSE;
    int pos = graph->vertices[i].position;
    if (graph->strand[0] == '+')
	{
	if (graph->vertices[i].type == ggSoftStart)
	    {
	    start = pos - 100;
	    end = pos + 50;
	    isTxStart = TRUE;
	    }
	else if (graph->vertices[i].type == ggSoftEnd)
	    {
	    start = pos-1;
	    end = pos;
	    isTxEnd = TRUE;
	    }
	}
    else
	{
	if (graph->vertices[i].type == ggSoftEnd)
	    {
	    start = pos - 50;
	    end = pos + 100;
	    isTxStart = TRUE;
	    }
	else if (graph->vertices[i].type == ggSoftStart)
	    {
	    start = pos;
	    end = pos+1;
	    isTxEnd = TRUE;
	    }
	}
    if (isTxStart || isTxEnd)
	 {
	 struct range *range;
	 lmAllocVar(lm, range);
	 range->start = start;
	 range->end = end;
	 if (isTxStart)
	    {
	    if (!vNotStart[i])
		 slAddHead(&promoterList, range);
	    }
	 else
	     {
	     if (!vNotEnd[i])
		 slAddHead(&polyaList, range);
	     }
	 }
    }
if (slCount(promoterList) > 1)
    {
    struct range *range;
    for (range = promoterList; range != NULL; range = range->next)
	{
	fprintf(f, "%s\t%d\t%d\t%s\t0\t%s\n", graph->tName,
	    range->start, range->end, "altPromoter", graph->strand);
	}
    }
if (slCount(polyaList) > 1)
    {
    struct range *range;
    for (range = polyaList; range != NULL; range = range->next)
	{
	fprintf(f, "%s\t%d\t%d\t%s\t0\t%s\n", graph->tName,
	    range->start, range->end, "altFinish", graph->strand);
	}
    }
lmCleanup(&lm);
}


boolean checkEnds(struct dnaSeq *chrom, int start, int end, char *ends, char *strand)
/* Return TRUE if the ends of intron match the input ends. */
{
char *s = chrom->dna + start;
char *e = chrom->dna + end;
char iEnds[5];
iEnds[0] = s[0];
iEnds[1] = s[1];
iEnds[2] = e[-2];
iEnds[3] = e[-1];
iEnds[4] = 0;
toLowerN(iEnds, 4);
if (strand[0] == '-')
    reverseComplement(iEnds, 4);
return sameString(ends, iEnds);
}

void strangeSplices(struct txGraph *graph, struct dnaSeq *chrom, FILE *f)
/* Write out introns with non-cannonical ends. */
{
struct txEdge *edge;
for (edge = graph->edgeList; edge != NULL; edge = edge->next)
    {
    if (edge->type == ggIntron)
        {
	int start = graph->vertices[edge->startIx].position;
	int end = graph->vertices[edge->endIx].position;
        if (checkEnds(chrom, start, end, "atac", graph->strand))
	    {
	    fprintf(f, "%s\t%d\t%d\t%s\t0\t%s\n", graph->tName,
		start, end, "atacIntron", graph->strand);
	    }
	else if (!checkEnds(chrom, start, end, "gtag", graph->strand) &&
	        !checkEnds(chrom, start, end, "gcag", graph->strand) )
	    {
	    fprintf(f, "%s\t%d\t%d\t%s\t0\t%s\n", graph->tName,
		start, end, "strangeSplice", graph->strand);
	    }
	}
    }
}

boolean evOfSourceOnList(struct txEvidence *evList, int sourceId)
/* Return TRUE if sourceId is found on evList. */
{
struct txEvidence *ev;
for (ev = evList; ev != NULL; ev = ev->next)
    if (ev->sourceId == sourceId)
        return TRUE;
return FALSE;
}

void refSeparateButJoined(struct txGraph *graph, FILE *f)
/* Flag graphs that have two non-overlapping refSeqs. */
{
int sourceIx;
boolean foundIt = FALSE;
struct lm *lm = lmInit(0);
struct rbTreeNode **stack;
lmAllocArray(lm, stack, 128);

/* Loop through sources looking for reference type. */
for (sourceIx=0; sourceIx<graph->sourceCount; ++sourceIx)
    {
    struct txSource *source = &graph->sources[sourceIx];
    if (sameString(source->type, refType))
        {
	/* Create a rangeTree including all exons of source. */
	struct rbTree *tree = rangeTreeNewDetailed(lm, stack);
	struct txEdge *edge;
	for (edge = graph->edgeList; edge != NULL; edge = edge->next)
	    {
	    if (edge->type == ggExon && evOfSourceOnList(edge->evList, sourceIx))
	        rangeTreeAdd(tree, graph->vertices[edge->startIx].position,
				   graph->vertices[edge->endIx].position);
	    }

	/* Go through remaining reference sources looking for no overlap. */
	int i;
	for (i=0; i<graph->sourceCount; ++i)
	    {
	    if (i == sourceIx)
	        continue;
	    struct txSource *s = &graph->sources[i];
	    if (sameString(s->type, refType))
	        {
		boolean gotOverlap = FALSE;
		for (edge = graph->edgeList; edge != NULL; edge = edge->next)
		    {
		    if (edge->type == ggExon && evOfSourceOnList(edge->evList, i))
		        {
			if (rangeTreeOverlaps(tree,
					graph->vertices[edge->startIx].position,
				        graph->vertices[edge->endIx].position))
			    {
			    gotOverlap = TRUE;
			    break;
			    }
			}
		    }
		if (!gotOverlap)
		     {
		     foundIt = TRUE;
		     break;
		     }
		}
	    }
	freez(&tree);
	}
    if (foundIt)
        break;
    }
if (foundIt)
    {
    fprintf(f, "%s\t%d\t%d\t%s\t0\t%s\n", graph->tName,
	graph->tStart, graph->tEnd, "refJoined", graph->strand);
    }
lmCleanup(&lm);
}

char *refSourceAcc(struct txGraph *graph, struct txEdge *edge)
/* Return refseq support for edge if any. */
{
struct txEvidence *ev;
for (ev = edge->evList; ev != NULL; ev = ev->next)
    {
    struct txSource *source = &graph->sources[ev->sourceId];
    if (sameString(source->type, refType))
        return source->accession;
    }
return NULL;
}

void constExons(struct txGraph *graph, FILE *f)
/* Write out constituitive exons. */
{
/* Create a tree with all introns. */
struct rbTree *tree = rangeTreeNew();
struct txEdge *edge;
for (edge = graph->edgeList; edge != NULL; edge = edge->next)
    {
    if (edge->type == ggIntron)
        {
	rangeTreeAdd(tree, graph->vertices[edge->startIx].position,
			   graph->vertices[edge->endIx].position);
	}
    }

/* Scan through all exons looking for ones that don't intersect 
 * introns. */
int eId = 0;
for (edge = graph->edgeList; edge != NULL; edge = edge->next)
    {
    if (edge->type == ggExon)
        {
	struct txVertex *s = &graph->vertices[edge->startIx];
	struct txVertex *e = &graph->vertices[edge->endIx];
	if (s->type == ggHardStart && e->type == ggHardEnd)
	    {
	    int start = s->position;
	    int end = e->position;
	    if (!rangeTreeOverlaps(tree, start, end))
		{
		char *refSource = refSourceAcc(graph, edge);
		if (refSource != NULL && edge->evCount >= 10)
		    {
		    /* Do one more scan making sure that it doesn't
		     * intersect any exons except for us. */
		    boolean anyOtherExon = FALSE;
		    struct txEdge *ed;
		    for (ed = graph->edgeList; ed != NULL; ed = ed->next)
		        {
			if (ed != edge)
			    {
			    int edStart = graph->vertices[ed->startIx].position;
			    int edEnd = graph->vertices[ed->endIx].position;
			    if (rangeIntersection(edStart, edEnd, start, end) > 0)
				{
			        anyOtherExon = TRUE;
				break;
				}
			    }
			}
		    if (!anyOtherExon)
			fprintf(f, "%s\t%d\t%d\t%s.%d\t0\t%s\n",
			    graph->tName, start, end, refSource, ++eId, graph->strand);
		    }
		}
	    }
	}
    }
rangeTreeFree(&tree);
}

void txgAnalyze(char *inTxg, char *dnaPath, char *outFile)
/* txgAnalyze - Analyse transcription graph for alt exons, alt 3', alt 5', 
 * retained introns, alternative promoters, etc.. */
{
struct lineFile *lf = lineFileOpen(inTxg, TRUE);
FILE *f = mustOpen(outFile, "w");
char *row[TXGRAPH_NUM_COLS];
struct nibTwoCache *ntc = nibTwoCacheNew(dnaPath);
struct dnaSeq *chrom = NULL;
while ( lineFileChopNextTab(lf, row, ArraySize(row)))
    {
    struct txGraph *txg = txGraphLoad(row);
    if (chrom == NULL || !sameString(chrom->name, txg->tName))
        {
	dnaSeqFree(&chrom);
	chrom = nibTwoCacheSeq(ntc, txg->tName);
	verbose(2, "Loaded %s into %s\n", txg->tName, chrom->name);
	}
    struct range *exonsWithIntrons = retainedIntrons(txg, f);
    cassetteExons(txg, f);
    altThreePrime(txg, exonsWithIntrons, f);
    altFivePrime(txg, exonsWithIntrons, f);
    altPromoter(txg, f);
    strangeSplices(txg, chrom, f);
    bleedsIntoIntrons(txg, f);
    refSeparateButJoined(txg, f);
    if (fConst != NULL)
        constExons(txg, fConst);
    slFreeList(&exonsWithIntrons);
    txGraphFree(&txg);
    }
carefulClose(&f);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
refType = optionVal("refType", refType);
char *fileName = optionVal("constExon", NULL);
if (fileName != NULL)
    fConst = mustOpen(fileName, "w");
txgAnalyze(argv[1], argv[2], argv[3]);
carefulClose(&fConst);
return 0;
}
