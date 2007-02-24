/* txgAnalyze - Analyse transcription graph for alt exons, alt 3', alt 5', 
 * retained introns, alternative promoters, etc.. */
#include "common.h"
#include "linefile.h"
#include "localmem.h"
#include "hash.h"
#include "options.h"
#include "ggTypes.h"
#include "txGraph.h"
#include "rangeTree.h"

char *refType = "refSeq";
void usage()
/* Explain usage and exit. */
{
errAbort(
  "txgAnalyze - Analyse transcription graph for alt exons, alt 3', alt 5', \n"
  "retained introns, alternative promoters, etc.\n"
  "usage:\n"
  "   txgAnalyze in.txg out.ana\n"
  "options:\n"
  "   refType=xxx - The type for the reference type of evidence, used for\n"
  "                 the refJoined records indicating transcription across\n"
  "                 what's usually considered 2 genes.  Default is %s.\n"
  , refType
  );
}

static struct optionSpec options[] = {
   {"refType", OPTION_STRING},
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

void altPromoter(struct txGraph *graph, FILE *f)
/* Write out alternative promoters. */
{
/* Set up an array to keep track of whether any edge ends in vertex. */
boolean vNotStart[graph->vertexCount];	
struct lm *lm = lmInit(0);
struct range *promoterList = NULL;
int i;
for (i=0; i<graph->vertexCount; ++i)
    vNotStart[i] = FALSE;
struct txEdge *edge;
for (edge = graph->edgeList; edge != NULL; edge = edge->next)
    {
    if (graph->strand[0] == '+')
        vNotStart[edge->endIx] = TRUE;
    else
        vNotStart[edge->startIx] = TRUE;
    }
for (i=0; i<graph->vertexCount; ++i)
    {
    if (!vNotStart[i])
        {
	int start = 0, end = 0;
	boolean isTxStart = FALSE;
	int pos = graph->vertices[i].position;
	if (graph->strand[0] == '+')
	    {
	    if (graph->vertices[i].type == ggSoftStart)
	        {
		start = pos - 100;
		end = pos + 50;
		isTxStart = TRUE;
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
	    }
	if (isTxStart)
	     {
	     struct range *range;
	     lmAllocVar(lm, range);
	     range->start = start;
	     range->end = end;
	     slAddHead(&promoterList, range);
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
lmCleanup(&lm);
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


void txgAnalyze(char *inTxg, char *outFile)
/* txgAnalyze - Analyse transcription graph for alt exons, alt 3', alt 5', 
 * retained introns, alternative promoters, etc.. */
{
struct lineFile *lf = lineFileOpen(inTxg, TRUE);
FILE *f = mustOpen(outFile, "w");
char *row[TXGRAPH_NUM_COLS];
while (lineFileRow(lf, row))
    {
    struct txGraph *txg = txGraphLoad(row);
    struct range *exonsWithIntrons = retainedIntrons(txg, f);
    cassetteExons(txg, f);
    altThreePrime(txg, exonsWithIntrons, f);
    altFivePrime(txg, exonsWithIntrons, f);
    altPromoter(txg, f);
    refSeparateButJoined(txg, f);
    slFreeList(&exonsWithIntrons);
    txGraphFree(&txg);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
refType = optionVal("refType", refType);
txgAnalyze(argv[1], argv[2]);
return 0;
}
