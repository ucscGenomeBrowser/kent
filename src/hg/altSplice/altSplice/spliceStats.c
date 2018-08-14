#include "common.h"
#include "altGraphX.h"
#include "cheapcgi.h"
#include "dnaseq.h"
#include "fa.h"
#include "hash.h"
#include "hdb.h"
#include "bed.h"
#include "geneGraph.h"


char *db = NULL;
struct hash *mrnaHash = NULL;

void usage()
{
errAbort("spliceStats - counts the number of cassette exons from an altGraphX file\n"
	 "which occur in a number of different libraries(confidence see below). Optionally\n"
	 "outputs sequence from said exons to a fasta file.\n"
	 "usage:\n\t"
	 "spliceStats <altGraphXFile> <minConf=5> <db=hgN> <optional: faFile=fastaFileOut> <optional: estPrior=10.0>\n"
	 "\t\t<optional: bedFile=bedFileName>\n"
	 "\n"
	 "Optional filters:\n"
	 "<minSize=N size of cassette exon>\n"
	 "<minFlankingSize=N size of flanking exons>\n"
	 "<minFlankingNum=N minimum number of transcripts supporting flanking exons\n"
	 "<mrnaFilter={on|off} trump the minFlankingNum if mrna is one transcript.\n"
	 "\n\nConfidence note from the function that returns confidence:\n"
	 "Return the score for this cassette exon. Want to have cassette exons\n"
	 "that are present in multiple transcripts and that are not present in multiple\n"
	 "transcripts. We want to see both forms of the cassette exon, we don't want to have\n"
	 "one outlier be chosen. Thus we count the times that the exon is seen, we\n"
	 "count the times that the exon isn't seen and we calculate a final score by:\n"
	 "(seen + notseen + prior)/(abs(seen - notSeen+prior) + 1) . Thus larger scores are\n"
	 "better, but tend to range between 1-2.\n");
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
fprintf(html, "<a href=\"http://genome-test.gi.ucsc.edu/cgi-bin/hgTracks?db=%s&position=%s:%d-%d\">%f     %s:%d-%d</a><br>\n",
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
    bedTabOutN(bedList,12, bedOutFile);
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

void hashRow0(struct sqlConnection *conn, char *query, struct hash *hash)
/** Return hash of row0 of return from query. */
{
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
int count = 0;
while ((row = sqlNextRow(sr)) != NULL)
    {
    hashAdd(hash, row[0], NULL);
    ++count;
    }
sqlFreeResult(&sr);
uglyf("%d match %s\n", count, query);
}

void addMrnaAccsToHash(struct hash *hash)
/** Load up all the accessions from the database that are mRNAs. */
{
char query[256];
struct sqlConnection *conn = NULL;
conn = hAllocConn();
sqlSafef(query, sizeof(query), "select acc from mrna where type = 2");
hashRow0(conn, query, hash);
}

void loadMrnaHash() 
{
if(mrnaHash != 0)
    errAbort("spliceStats::loadMrnaHash() - mrnaHash has already been created.\n");
mrnaHash = newHash(10);
addMrnaAccsToHash(mrnaHash);
}

boolean passFilter(struct bed *bed, int blockNum, struct altGraphX *ag, int minNum, int minFlankingSize, boolean mrnaFilter)
{
struct evidence *ev = slElementFromIx(ag->evidence,bed->expIds[blockNum]);
int i =0;
boolean pass = FALSE;
if(minFlankingSize > bed->blockSizes[blockNum])
    return FALSE;
if(ev->evCount >= minNum)
    return TRUE;
if(mrnaFilter)
    {
    for(i =0; i<ev->evCount; i++)
	{
	if(hashLookup(mrnaHash, ag->mrnaRefs[ev->mrnaIds[i]]))
	    return TRUE;
	}
    }
return FALSE;
}

boolean bedPassFilters(struct bed *bed, struct altGraphX *ag, int cassetteEdge)
{
int minFlankingNum = cgiUsualInt("minFlankingNum", 2);
int minFlankingSize = cgiUsualInt("minFlankingSize", 0);
boolean mrnaFilter = cgiBoolean("mrnaFilter");
boolean passed = TRUE;
int i =0;
for(i = 0; i<bed->blockCount; i++)
    {
    if(bed->expIds[i] != cassetteEdge)
	{
	passed &= passFilter(bed, i, ag, minFlankingNum, minFlankingSize, mrnaFilter);
	}
    }
return passed;
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
FILE *sizes = mustOpen("sizes.log", "w");
int minSize = cgiOptionalInt("minSize", 0);
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
	    int size = ag->vPositions[ag->edgeEnds[i]] - ag->vPositions[ag->edgeStarts[i]];
	    boolean filtersOk = FALSE;
	    if(ag->name == NULL)
		ag->name = cloneString("");

	    slSort(&bedList, bedCmpMaxScore);
	    for(bed=bedList; bed != NULL; bed = bed->next)
		{
		snprintf(buff, sizeof(buff), "%s.%d", ag->name, counter);
		bed->name = cloneString(buff);
		fprintf(log, "%f\n", conf);
		fprintf(sizes, "%d\n%d\n%d\n", bed->blockSizes[0], bed->blockSizes[1], bed->blockSizes[2]);
		filtersOk = bedPassFilters(bed, ag, i);
		if(conf >= minConfidence && size >= minSize && filtersOk) 
		    {
		    writeCassetteExon(bed, ag, i, &outputted, bedOutFile, outfile, html, conf);
		    cassetteCount++;
		    if((size % 3) == 0)
			mod3++;
		    }
		counter++;
		}
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

void printCommandState(int argc, char *argv[], FILE *out)
{
struct cgiVar *c = NULL, *cList = NULL;
int i;
fprintf(out, "#");
for(i=0; i<argc; i++)
    fprintf(out, "%s ", argv[i]);
cList = cgiVarList();
for(c = cList; c != NULL; c = c->next)
    {
    fprintf(out, "%s=\"%s\" ", c->name, c->val);
    }
fprintf(out, "\n");
}
    
int main(int argc, char *argv[])
{
struct altGraphX *agList = NULL;
int cassetteCount = 0;
float minConfidence = 0;
char *bedFileName = NULL;
char *faFile = NULL;
FILE *faOut = NULL;
FILE *bedOut = NULL;
boolean mrnaFilter = FALSE;
float estPrior = 0.0;
int minSize = 0;
if(argc < 4)
    usage();
cgiSpoof(&argc, argv);
warn("Loading graphs.");
agList = altGraphXLoadAll(argv[1]);
bedFileName = cgiOptionalString("bedFile");
minConfidence = cgiDouble("minConf");
db = cgiString("db");
faFile = cgiOptionalString("faFile");
estPrior = cgiOptionalDouble("estPrior", 10);
minSize = cgiOptionalInt("minSize", 0);
mrnaFilter = cgiBoolean("mrnaFilter");
if(mrnaFilter)
    loadMrnaHash();
warn("Counting cassette exons from %d clusters above confidence: %f", slCount(agList), minConfidence);
if(bedFileName != NULL)
    {
    bedOut = mustOpen(bedFileName, "w");
    printCommandState(argc, argv, bedOut);
    fprintf(bedOut, "track name=cass_conf-%4.2f_est-%3.2f description=\"spliceStats minConf=%4.2f estPrior=%3.2f minSize=%d\"\n", 
	    minConfidence, estPrior, minConfidence, estPrior, minSize);
    }
if(faFile != NULL)
    faOut = mustOpen(faFile, "w");
cassetteCount = countCassetteExons(agList, minConfidence, faOut,bedOut );
carefulClose(&faOut);
carefulClose(&bedOut);
warn("%d cassette exons out of %d clusters in %s", cassetteCount, slCount(agList), argv[1]);
altGraphXFreeList(&agList);
return 0;
}
