#include "common.h"
#include "altGraphX.h"
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
	 "spliceStats <altGraphXFile> <confidence> <db>  <optional:fastaFileOut>\n");
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


int altGraphTimesSeenEdge(struct altGraphX *ag, int eIx) 
/* Count how many times we see evidence for a particular edge. */
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

int assignToArray(int *array, int arraySize, int count, int val)
{
if(count >= arraySize)
    errAbort("Can't have count: %d greater than array size: %d", count, arraySize);
array[count] = val;
}

boolean uniqeInArray(int *array, int size, int val)
{
int i;
for(i=0;i<size; i++)
    if(array[i] == val)
	return FALSE;
return TRUE;
}

int altGraphTimesNotSeenEdge(struct altGraphX *ag, int eIx) 
/* Discover how many times there is a transcript that uses an alternative
   to the edge eIx. */
{
int eStart =0, eEnd = 0;
int *intStartEdges = NULL, *intEndEdges =NULL, *altEdges = NULL;
int intStartCount = 0, intEndCount =0, altCount =0;
int i,j,k;
int conf = 0;
int eCount = ag->edgeCount;
AllocArray(intStartEdges, ag->edgeCount);
AllocArray(intEndEdges, ag->edgeCount);
AllocArray(altEdges, ag->edgeCount);
eStart = ag->edgeStarts[eIx];
eEnd = ag->edgeEnds[eIx];
/* First find the introns that connect to our edge of interest. */
for(i = 0; i < ag->edgeCount; i++)
    {
    if(ag->edgeEnds[i] == eStart)
	assignToArray(intStartEdges, eCount, intStartCount++, i);
    if(ag->edgeStarts[i] == eEnd)
	assignToArray(intEndEdges, eCount, intEndCount++, i);
    }

/* for each intron that connects to our exon. */
for(i = 0; i < intStartCount; i++)
    {
    for(j =0; j < ag->edgeCount; j++)
	{
	/* Look for and edge that starts at the same place as our introns. */
	if(intStartEdges[i] != j && (ag->edgeStarts[j] == ag->edgeStarts[intStartEdges[i]]))
	    {
	    for(k=0; k < intEndCount; k++) 
		{
		/* Then connects to one of the same ends. */
		if(ag->edgeEnds[j] == ag->edgeEnds[intEndEdges[k]])
		    if(uniqeInArray(altEdges, altCount, j))
			assignToArray(altEdges, eCount, altCount++, j);
		}
	    }
	}
    }

for(i=0; i< altCount; i++)
    {
    conf += altGraphTimesSeenEdge(ag, altEdges[i]);
    }

freez(&altEdges);
freez(&intStartEdges);
freez(&intEndEdges);
return conf;
}

float altGraphConfidenceForEdge(struct altGraphX *ag, int eIx)
/* Return the score for this cassette exon. Want to have cassette exons
that are present in multiple transcripts and that are not present in multiple
exons. We want to see both forms of the cassette exon, we don't want to have
one outlier be chosen. Thus we count the times that the exon is seen, we
count the times that the exon isn't seen and we calculate a final score by:
(seen + notseen)/(abs(seen - notSeen) + 1) . Thus larger scores are better. */
{
int seen = altGraphTimesSeenEdge(ag, eIx);
int notSeen = altGraphTimesNotSeenEdge(ag, eIx);
float conf = 0;
conf = (float)(seen + notSeen + 10)/(float)(abs(seen - notSeen) + 10);
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
	    float conf = altGraphConfidenceForEdge(ag, i);
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
