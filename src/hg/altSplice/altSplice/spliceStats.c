#include "common.h"
#include "altGraphX.h"
#include "dnaseq.h"
#include "fa.h"
#include "hdb.h"
#include "geneGraph.h"


void usage()
{
errAbort("spliceStats - counts the number of cassette exons from an altGraphX file\n"
	 "which occur in a number of different libraries(confidence). Optionally\n"
	 "outputs sequence from said exons to a fasta file.\n"
	 "usage:\n\t"
	 "spliceStats <altGraphXFile> <confidence> <optional:fastaFileOut>\n");
}

boolean altGraphXEdgeSeen(struct altGraphX *ag, int *seen, int *seenCount, int mrnaIx)
/* is the mrnaIx already in seen? */
{
int i=0;
boolean result = FALSE;
for(i=0; i<*seenCount; i++)
    {
    if(ag->mrnaTissues[seen[i]] == ag->mrnaTissues[mrnaIx] ||
       ag->mrnaLibs[seen[i]] == ag->mrnaLibs[mrnaIx])
	{
	result = TRUE;
	break;
	}
    }
if(!result)
    {
    seen[*seenCount++] = mrnaIx;
    }
return result;
}

int altGraphConfidenceForEdge(struct altGraphX *ag, int eIx)
/* count how many unique libraries or tissues contain a given edge */
{
struct evidence *ev = slElementFromIx(ag->evidence, eIx);
int *seen = NULL;
int seenCount = 0,i;
int conf = 0;
AllocArray(seen, ag->edgeCount);
for(i=0; i<ag->edgeCount; i++)
    seen[i] = -1;
for(i=0; i<ev->evCount; i++)
    if(!altGraphXEdgeSeen(ag, seen, &seenCount, ev->mrnaIds[i]))
	conf++;
freez(&seen);
return conf;
}

boolean altGraphXInEdges(struct ggEdge *edges, int v1, int v2)
/* Return TRUE if a v1-v2 edge is in the list FALSE otherwise. */
{
struct ggEdge *e = NULL;
for(e = edges; e != NULL; e = e->next)
    {
    if(e->vertex1 == v1 && e->vertex2 == v2)
	return TRUE;
    }
return FALSE;
}

int countCassetteExons(struct altGraphX *agList, int minConfidence, FILE *outfile)
/* count up the number of cassette exons that have a certain
   confidence, returns number of edges. If outfile != NULL will output fasta sequences
   to outfile. */
{
struct altGraphX *ag = NULL;
int edge =0;
int cassetteCount = 0;
int i =0;
int mod3 = 0;
boolean outputted = FALSE;
for(ag = agList; ag != NULL; ag = ag->next)
    {
    outputted = FALSE;
    for(i=0;i<ag->edgeCount; i++)
	{
	if(ag->edgeTypes[i] == ggCassette)
	    {
	    int conf = altGraphConfidenceForEdge(ag, i);
	    if(conf >= minConfidence) 
		{
		cassetteCount++;
		if(!outputted)
		    {
		    altGraphXTabOut(ag, stdout);
		    outputted = TRUE;
		    }
		if(((ag->vPositions[ag->edgeEnds[i]] - ag->vPositions[ag->edgeStarts[i]]) % 3) == 0)
		    mod3++;
		if(outfile != NULL)
		    {
		    struct dnaSeq *seq = hChromSeq(ag->tName, ag->vPositions[ag->edgeStarts[i]], ag->vPositions[ag->edgeEnds[i]]);
		    if(sameString(ag->strand , "+")) 
			reverseComplement(seq->dna, seq->size);
		    if(seq->size < 200)
			faWriteNext(outfile, seq->name, seq->dna, seq->size);
		    freeDnaSeq(&seq);
		    }
		}
	    }
	}
    }
warn("%d cassettes are mod 3", mod3);
return cassetteCount;
}

int main(int argc, char *argv[])
{
struct altGraphX *agList = NULL;
int cassetteCount = 0;
int minConfidence = 0;
if(argc < 3)
    usage();
warn("Loading graphs.");
agList = altGraphXLoadAll(argv[1]);
minConfidence = atoi(argv[2]);
warn("Counting cassette exons from %d clusters above confidence: %d", slCount(agList), minConfidence);
if(argc == 3)
    cassetteCount = countCassetteExons(agList, minConfidence, NULL);
else
    {
    FILE *out = mustOpen(argv[3], "w");
    cassetteCount = countCassetteExons(agList, minConfidence, out);
    carefulClose(&out);
    }
warn("%d cassette exons out of %d clusters in %s", cassetteCount, slCount(agList), argv[1]);
altGraphXFreeList(&agList);
return 0;
}
