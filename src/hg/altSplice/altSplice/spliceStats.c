#include "common.h"
#include "altGraphX.h"
#include "cheapcgi.h"
#include "dnaseq.h"
#include "fa.h"
#include "hdb.h"
#include "geneGraph.h"

char *db = NULL;
void usage()
{
errAbort("spliceStats - counts the number of cassette exons from an altGraphX file\n"
	 "which occur in a number of different libraries(confidence). Optionally\n"
	 "outputs sequence from said exons to a fasta file.\n"
	 "usage:\n\t"
	 "spliceStats <altGraphXFile> <confidence> <db>  <optional:fastaFileOut> <optional: estPrior=10.0>\n");
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

void startHtml(FILE *html)
{
fprintf(html, "<HTML><BODY>\n");
}

void endHtml(FILE *html)
{
fprintf(html, "</BODY></HTML>\n");
}

void writeBrowserLink(FILE *html, struct altGraphX *ag, float conf, int eIx)
{
int start = ag->vPositions[ag->edgeStarts[eIx]];
int end = ag->vPositions[ag->edgeEnds[eIx]];
char *chrom = ag->tName;
fprintf(html, "<a href=\"http://genome-test.cse.ucsc.edu/cgi-bin/hgTracks?db=%s&position=%s:%d-%d\">%f     %s:%d-%d</a><br>\n",
	db, ag->tName, start, end, conf, ag->tName, start, end);
}

int countCassetteExons(struct altGraphX *agList, float minConfidence, FILE *outfile)
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
float estPrior = cgiOptionalDouble("estPrior", 10);
FILE *log = mustOpen("confidences.log", "w");
FILE *html = mustOpen("confidences.html", "w");
startHtml(html);
for(ag = agList; ag != NULL; ag = ag->next)
    {
    outputted = FALSE;
    for(i=0;i<ag->edgeCount; i++)
	{
	if(ag->edgeTypes[i] == ggCassette)
	    {
	    float conf = altGraphConfidenceForEdge(ag, i, estPrior);
	    fprintf(log, "%f\n", conf);
	    if(conf >= minConfidence) 
		{
		writeBrowserLink(html, ag, conf, i);
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
endHtml(html);
carefulClose(&html);
carefulClose(&log);
warn("%d cassettes are mod 3", mod3);
return cassetteCount;
}

int main(int argc, char *argv[])
{
struct altGraphX *agList = NULL;
int cassetteCount = 0;
float minConfidence = 0;
if(argc < 4)
    usage();
warn("Loading graphs.");
agList = altGraphXLoadAll(argv[1]);
minConfidence = atof(argv[2]);
db = argv[3];
warn("Counting cassette exons from %d clusters above confidence: %f", slCount(agList), minConfidence);
if(argc == 4)
    cassetteCount = countCassetteExons(agList, minConfidence, NULL);
else
    {
    FILE *out = mustOpen(argv[4], "w");
    cassetteCount = countCassetteExons(agList, minConfidence, out);
    carefulClose(&out);
    }
warn("%d cassette exons out of %d clusters in %s", cassetteCount, slCount(agList), argv[1]);
altGraphXFreeList(&agList);
return 0;
}
