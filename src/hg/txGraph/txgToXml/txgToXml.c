/* txgToXml - Convert txg to an XML format.. */
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

char *ggVertexTypeAsString(enum ggVertexType type)
/* Return string corresponding to vertex type. */
{
switch(type)
    {
    case ggSoftStart:
        return "(";
    case ggHardStart:
        return "[";
    case ggSoftEnd:
        return ")";
    case ggHardEnd:
        return "]";
    default:
        errAbort("Unknown type %d", type);
	return "unknown";
    }
}

void txgWriteXml(struct txGraph *graph, FILE *f)
/* Write out graph as xml. */
{
fprintf(f, "<txGraph>\n");
int i;
fprintf(f, "  <vertices count=\"%d\">\n", graph->vertexCount);
for (i=0; i<graph->vertexCount; ++i)
    {
    fprintf(f, "    <vertex type=\"%s\" pos=\"%d\" i=\"%d\"/>\n",
    	ggVertexTypeAsString(graph->vTypes[i]),
	graph->vPositions[i], i);
    }
fprintf(f, "  </vertices>\n");
fprintf(f, "  <edges>\n");
struct txEvList *evList =  graph->evidence;
for (i=0; i<graph->edgeCount; ++i)
    {
    int i1 = graph->edgeStarts[i];
    int i2 = graph->edgeEnds[i];
    char *t1 = ggVertexTypeAsString(graph->vTypes[i1]);
    char *t2 = ggVertexTypeAsString(graph->vTypes[i2]);
    int x1 = graph->vPositions[i1];
    int x2 = graph->vPositions[i2];
    fprintf(f, "    <edge type=\"%s\" t1=\"%s\" t2=\"%s\" x1=\"%d\" x2=\"%d\" i1=\"%d\" i2=\"%d\"", 
        (graph->edgeTypes[i] == ggExon ? "exon  " : "intron"),
	t1, t2, x1, x2, i1, i2);
    if (showEvidence)
        {
	fprintf(f, ">\n");
	struct txEvidence *ev;	
	for (ev = evList->evList; ev != NULL; ev = ev->next)
	    {
	    struct txSource *source = slElementFromIx(graph->sources,ev->sourceId);
	    fprintf(f, "      <ev type=\"%s\" acc=\"%s\" x1=\"%d\" x2=\"%d\"/>\n",
	    	source->type, source->accession, ev->start, ev->end);
	    }
	fprintf(f, "    </edge>\n");
	}
    else
        {
	fprintf(f, "/>\n");
	}
    evList = evList->next;
    }
fprintf(f, "  </edges>\n");
fprintf(f, "</txGraph>\n");
}

void txgToXml(char *inTxg, char *outXml)
/* txgToXml - Convert txg to an XML format.. */
{
struct txGraph *txg, *txgList = txGraphLoadAll(inTxg);
FILE *f = mustOpen(outXml, "w");
for (txg = txgList; txg != NULL; txg = txg->next)
    {
    txgWriteXml(txg, f);
    }
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
