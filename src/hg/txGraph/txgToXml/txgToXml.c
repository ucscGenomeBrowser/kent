/* txgToXml - Convert txg to an XML format.. */

/* Copyright (C) 2007 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "geneGraph.h"
#include "txGraph.h"

boolean showEvidence = TRUE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txgToXml - Convert txg to an XML format.\n"
  "usage:\n"
  "   txgToXml in.txg out.xml\n"
  "options:\n"
  "   -noEvidence\n"
  );
}

static struct optionSpec options[] = {
   {"noEvidence", OPTION_BOOLEAN},
   {NULL, 0},
};

void txgWriteXml(struct txGraph *graph, FILE *f)
/* Write out graph as xml. */
{
fprintf(f, "  <txGraph name=\"%s\" tName=\"%s\" strand=\"%s\" tStart=\"%d\" tEnd=\"%d\" vertexCount=\"%d\" edgeCount=\"%d\" sourceCount=\"%d\">\n",
	graph->name, graph->tName, graph->strand, graph->tStart, graph->tEnd, graph->vertexCount, graph->edgeCount,
	graph->sourceCount);
int i;
for (i=0; i<graph->vertexCount; ++i)
    {
    struct txVertex *v = &graph->vertices[i];
    fprintf(f, "    <vertex type=\"%s\" pos=\"%d\" i=\"%d\"/>\n",
    	ggVertexTypeAsString(v->type), v->position, i);
    }
struct txEdge *edge;
for (edge = graph->edgeList; edge != NULL; edge = edge->next)
    {
    int i1 = edge->startIx;
    int i2 = edge->endIx;
    char *t1 = ggVertexTypeAsString(graph->vertices[i1].type);
    char *t2 = ggVertexTypeAsString(graph->vertices[i2].type);
    int x1 = graph->vertices[i1].position;
    int x2 = graph->vertices[i2].position;
    fprintf(f, "    <edge type=\"%s\" t1=\"%s\" t2=\"%s\" size=\"%d\" x1=\"%d\" x2=\"%d\" i1=\"%d\" i2=\"%d\"", 
        (edge->type == ggExon ? "exon  " : "intron"),
	t1, t2, x2-x1, x1, x2, i1, i2);
    if (showEvidence)
        {
	fprintf(f, ">\n");
	struct txEvidence *ev;	
	for (ev = edge->evList; ev != NULL; ev = ev->next)
	    {
	    struct txSource *source = &graph->sources[ev->sourceId];
	    fprintf(f, "      <ev acc=\"%s\" x1=\"%d\" x2=\"%d\"/>\n",
	    	source->accession, ev->start, ev->end);
	    }
	fprintf(f, "    </edge>\n");
	}
    else
        {
	fprintf(f, "/>\n");
	}
    }
for (i=0; i<graph->sourceCount; ++i)
    {
    struct txSource *source = &graph->sources[i];
    fprintf(f, "    <source type=\"%s\" acc=\"%s\"/>\n", 
    	source->type, source->accession);
    }
fprintf(f, "  </txGraph>\n");
}

void txgToXml(char *inTxg, char *outXml)
/* txgToXml - Convert txg to an XML format.. */
{
struct txGraph *txg, *txgList = txGraphLoadAll(inTxg);
verbose(1, "loaded %d from %s\n", slCount(txgList), inTxg);
FILE *f = mustOpen(outXml, "w");
fprintf(f, "<txGraphList count=\"%d\">\n", slCount(txgList));
for (txg = txgList; txg != NULL; txg = txg->next)
    {
    txgWriteXml(txg, f);
    }
fprintf(f, "</txGraphList>\n");
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
showEvidence = !optionExists("noEvidence");
if (argc != 3)
    usage();
txgToXml(argv[1], argv[2]);
return 0;
}
