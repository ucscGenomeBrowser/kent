#include "common.h"
#include "altGraphX.h"
#include "cheapcgi.h"
#include "dnaseq.h"
#include "fa.h"
#include "hdb.h"
#include "bed.h"
#include "geneGraph.h"

char *db = NULL;
void usage()
{
errAbort("spliceStats - counts the number of cassette exons from an altGraphX file\n"
	 "which occur in a number of different libraries(confidence). Optionally\n"
	 "outputs sequence from said exons to a fasta file.\n"
	 "usage:\n\t"
	 "spliceStats <altGraphXFile> <confidence> <db>  <optional:fastaFileOut> <optional: estPrior=10.0>\n"
	 "\t\t<optional: bedFile=bedFileName>\n");
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

int bedCmpMaxScore(const void *va, const void *vb)
/* Compare to sort based on score - highest first. */
{
const struct bed *a = *((struct bed **)va);
const struct bed *b = *((struct bed **)vb);
return b->score - a->score;
}

void writeCassetteExon(struct bed *bedList, struct altGraphX *ag, int eIx, boolean *outputted, 
		       FILE *bedOutFile, FILE *outfile, FILE *html, float conf )
/* Write out the information for a cassette exon. */
{
int i = eIx;
struct bed *bed=NULL;
if(bedOutFile != NULL)
    for(bed=bedList; bed != NULL; bed = bed->next)
	bedTabOutN(bed,12, bedOutFile);
writeBrowserLink(html, ag, conf, i);
if(!outputted)
    {
    altGraphXTabOut(ag, stdout);
    *outputted = TRUE;
    }
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

int countCassetteExons(struct altGraphX *agList, float minConfidence, FILE *outfile, FILE *bedOutFile)
/* count up the number of cassette exons that have a certain
   confidence, returns number of edges. If outfile != NULL will output fasta sequences
   to outfile. */
{
struct altGraphX *ag = NULL;
int edge =0;
int cassetteCount = 0;
int i =0;
int mod3 = 0;
int counter =0;
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
	    float conf = altGraphCassetteConfForEdge(ag, i, estPrior);
	    struct bed *bed, *bedList = altGraphGetExonCassette(ag, i);
	    char buff[256];
	    if(ag->name == NULL)
		ag->name = cloneString("");
	    snprintf(buff, sizeof(buff), "%s.%d", ag->name, counter);
	    slSort(&bedList, bedCmpMaxScore);
	    for(bed=bedList; bed != NULL; bed = bed->next)
		bed->name = cloneString(buff);
	    fprintf(log, "%f\n", conf);
	    if(conf >= minConfidence) 
		{
		writeCassetteExon(bedList, ag, i, &outputted, bedOutFile, outfile, html, conf);
		}
	    counter++;
	    bedFreeList(&bedList);
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
char *bedFileName = NULL;
if(argc < 4)
    usage();
cgiSpoof(&argc, argv);
warn("Loading graphs.");
agList = altGraphXLoadAll(argv[1]);
bedFileName = cgiOptionalString("bedFile");
minConfidence = atof(argv[2]);
db = argv[3];
warn("Counting cassette exons from %d clusters above confidence: %f", slCount(agList), minConfidence);
if(argc == 4)
    cassetteCount = countCassetteExons(agList, minConfidence, NULL, NULL);
else
    {
    FILE *out = mustOpen(argv[4], "w");    
    FILE *bedOut = NULL;
    if(bedFileName != NULL)
	bedOut = mustOpen(bedFileName, "w");
    cassetteCount = countCassetteExons(agList, minConfidence, out,bedOut );
    carefulClose(&out);
    }
warn("%d cassette exons out of %d clusters in %s", cassetteCount, slCount(agList), argv[1]);
altGraphXFreeList(&agList);
return 0;
}
