/* agxToXml - Convert axt format splicing graph to XML format.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "geneGraph.h"
#include "altGraphX.h"

boolean showEvidence = TRUE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "agxToXml - Convert axt format splicing graph to XML format.\n"
  "usage:\n"
  "   agxToXml in.agx out.xml\n"
  "options:\n"
  "   -noEvidence\n"
  );
}

static struct optionSpec options[] = {
   {"noEvidence", OPTION_BOOLEAN},
   {NULL, 0},
};

void agxWriteXml(struct altGraphX *graph, FILE *f)
/* Write out graph as xml. */
{
fprintf(f, "  <altGraphX name=\"%s\" tName=\"%s\" strand=\"%s\" tStart=\"%d\" tEnd=\"%d\" vertexCount=\"%d\" edgeCount=\"%d\">\n",
	graph->name, graph->tName, graph->strand, graph->tStart, graph->tEnd, graph->vertexCount, graph->edgeCount);
int i;
for (i=0; i<graph->vertexCount; ++i)
    {
    fprintf(f, "    <vertex type=\"%s\" pos=\"%d\" i=\"%d\"/>\n",
    	ggVertexTypeAsString(graph->vTypes[i]),
	graph->vPositions[i], i);
    }
struct evidence *evList =  graph->evidence;
for (i=0; i<graph->edgeCount; ++i)
    {
    int i1 = graph->edgeStarts[i];
    int i2 = graph->edgeEnds[i];
    char *t1 = ggVertexTypeAsString(graph->vTypes[i1]);
    char *t2 = ggVertexTypeAsString(graph->vTypes[i2]);
    int x1 = graph->vPositions[i1];
    int x2 = graph->vPositions[i2];
    fprintf(f, "    <edge type=\"%s\" t1=\"%s\" t2=\"%s\" size=\"%d\" x1=\"%d\" x2=\"%d\" i1=\"%d\" i2=\"%d\"", 
        (graph->edgeTypes[i] == ggExon ? "exon  " : "intron"),
	t1, t2, x2-x1, x1, x2, i1, i2);
    if (showEvidence)
        {
	fprintf(f, ">\n");
	int j;
	for (j=0; j<evList->evCount; ++j)
	    {
	    int mrnaId = evList->mrnaIds[j];
	    fprintf(f, "      <ev acc=\"%s\"/>\n", graph->mrnaRefs[mrnaId]);
	    }
	fprintf(f, "    </edge>\n");
	}
    else
        {
	fprintf(f, "/>\n");
	}
    evList = evList->next;
    }
fprintf(f, "  </altGraphX>\n");
}

void agxToXml(char *inTxg, char *outXml)
/* agxToXml - Convert agx to an XML format.. */
{
struct altGraphX *agx, *agxList = altGraphXLoadAll(inTxg);
FILE *f = mustOpen(outXml, "w");
fprintf(f, "<agxList>\n");
for (agx = agxList; agx != NULL; agx = agx->next)
    {
    agxWriteXml(agx, f);
    }
fprintf(f, "</agxList>\n");
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
showEvidence = !optionExists("noEvidence");
if (argc != 3)
    usage();
agxToXml(argv[1], argv[2]);
return 0;
}
