/** 
 \page altSplice altSplice - construct alt-splicing graphs using est and mrna alignments.

 \brief Construct altGraphX structures for genePredictions using information in est and mrna alignments.

 <p><b>usage:</b> <pre>altSplice <db> <outputFile.AltGraph.tab> <optional:bedFile></pre>

 <p>altSplice.c uses code from Jim's geneGraph.h library to cluster mrna's
 and ests onto genomic sequence and try to determine various types of
 alternative splicing. Currently cassette exons are particularly
 highlighted. Only spliced ests are used, as they tend to be less noisy
 and are also easier to orient correctly using splice sites from genomic
 sequence.

 <p>The basic algorithm is as follows: 
 
 - Load refGene table from chromosome 22, or any specified bed file, 
 into a genePred structure.

 - Sort the genePred's by size, smallest first. This is to avoid 
 doubling up on small genes that overlap or are internal to another
 gene.

 - For each genePred determine alternative splicing as follows:
 
    - Load all mrna and est alignments for that portion of genomic sequence.

    - Throw away mrnas and ests that don't have at least their first
    and last exon within the genePred txStart and txEnd. For example
    <pre>
    1) ---------------########-----#######------######----------- genePred
    2) -------------##########-----#######------########--------- est1 alignment 
    3) --#####-----------------------------####-------------##### est2 alignment
    </pre>

    est1 alignment would be kept but est2 would be removed from
    consideration.  Thus the 5' and '3 UTRs can grow using est/mrna
    evidence but not the number of exons.

    - The genePred boundaries are extended to include the most upstream and
    most downstream alignments.

    - Dna from genome is cut out and used to orient ests using splice 
    sites, gt->ag, and less the common gc->ag.

    - The psl alignments are converted to ggMrnaAli structures and in the
    process inserts/deletions smaller than 5bp are removed. 

    - These ggMrnaAli structures are converted to ggMrnaInput
    structures and clustered together into ggMrnaCluster structures.

    - The largest ggMrnaCluster structure is used to create a geneGraph.
    Where each splice site and/or exon boundary forms a vertex and the
    introns and exons are represented by edges. The supporting mrna/est
    evidence for each edge is remembered for use later.

    - The geneGraph is converted to an altGraphX structre for storage
    into a database. In this process unused vertices are removed, vertices
    are sorted by genomic position, and cassette exons are flagged.

 */
#include "common.h"
#include "geneGraph.h"
#include "genePred.h"
#include "altGraphX.h"
#include "ggMrnaAli.h"
#include "psl.h"
#include "dnaseq.h"
#include "hdb.h"
#include "jksql.h"
#include "cheapcgi.h"
#include "bed.h"

int cassetteCount = 0; /* Number of cassette exons counted. */
int misSense = 0;      /* Number of cassette exons that would introduce a missense mutation. */
int clusterCount = 0;  /* Number of gene clusters identified. */

void usage()
/* print usage and quit */
{
errAbort("altSplice - constructs altSplice graphs using information alignments\n"
	 "in est and mrna databases. At first only for chr22\n"
	 "usage:\n\taltSplice <db> <outputFile.AltGraph.tab> <optional:bedFile>\n");
}

struct psl *loadPslsFromDb(struct sqlConnection *conn, int numTables, char **tables, 
			   char *chrom, unsigned int chromStart, unsigned int chromEnd)
/* load up all of the psls that align on a given section of the database */
{
struct sqlResult *sr = NULL;
char **row = NULL;
int rowOffset = -100;
struct psl *pslList = NULL;
int i=0;
/* for each table load up the relevant psls */
for(i = 0; i < numTables; i++)
    {
    sr = hRangeQuery(conn, tables[i], chrom, chromStart, chromEnd, NULL, &rowOffset);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	struct psl *psl = pslLoad(row+rowOffset);
	if( (psl->tStarts[0] + psl->blockSizes[0] >= chromStart) && 
	    (psl->tStarts[psl->blockCount -1] <= chromEnd) )
	    {
	    slAddHead(&pslList, psl);
	    }
	else
	    {
	    pslFree(&psl);
	    }
	}
    }
slReverse(&pslList);
return pslList;
}

void expandToMaxAlignment(struct psl *pslList, char *chrom, int *chromStart, int *chromEnd)
/* finds the farthest up stream and downstream boundaries of alignments */
{
struct psl *psl=NULL;
for(psl = pslList; psl != NULL; psl = psl->next)
    {
    if(differentString(psl->tName, chrom))
	errAbort("test_geneGraph::expandToMrnaAlignment() - psls on chrom: %s, need chrom: %s.", 
		 psl->tName, chrom);
    else 
	{
	if(psl->tStart < *chromStart)
	    *chromStart = psl->tStart;
	if(psl->tEnd > *chromEnd)
	    *chromEnd = psl->tEnd;
	}
    }
}

int agIndexFromEdge(struct altGraphX *ag, struct ggEdge *edge)
/* Find the index in edgeStarts & edgeEnds for edge. Returns -1 if
 * not found. */
{
int i;
assert(edge->vertex1 < ag->vertexCount && edge->vertex2 < ag->vertexCount);
for(i=0; i<ag->edgeCount; i++)
    {
    if(edge->vertex1 == ag->edgeStarts[i] && edge->vertex2 == ag->edgeEnds[i])
	return i;
    }
return -1;
}

int mcLargestFirstCmp(const void *va, const void *vb)
/* Compare to sort based sizes of chromEnd - chromStart, largest first. */
{
const struct ggMrnaCluster *a = *((struct ggMrnaCluster **)va);
const struct ggMrnaCluster *b = *((struct ggMrnaCluster **)vb);
int dif;
dif = -1 * (abs(a->tEnd - a->tStart) - abs(b->tEnd - b->tStart));
return dif;
}


struct altGraphX *agFromGp(struct genePred *gp, struct sqlConnection *conn, int maxGap, FILE *out)
{
struct altGraphX *ag = NULL;
struct ggMrnaCluster *mcList=NULL, *mc=NULL;
struct ggMrnaInput *ci = NULL;
struct geneGraph *gg = NULL;
struct dnaSeq *genoSeq = NULL;
char *tablePrefixes[] = {"_mrna", "_intronEst"};
char **tables = NULL;
int i,j,k,l;
char buff[256];
struct ggMrnaAli *maList=NULL, *ma=NULL, *maNext=NULL, *maSameStrand=NULL;
struct psl *pslList = NULL;
char *chrom = gp->chrom;
int chromStart = gp->txStart;
int chromEnd = gp->txEnd;
boolean keepGoing = TRUE;
/* make the tables */
tables = needMem(sizeof(char*) * ArraySize(tablePrefixes));
for(i=0; i< ArraySize(tablePrefixes); i++)
    {
    snprintf(buff, sizeof(buff),"%s%s", gp->chrom, tablePrefixes[i]);
    tables[i] = cloneString(buff);
    }

/* load the psls */
pslList = loadPslsFromDb(conn, ArraySize(tablePrefixes), tables, gp->chrom, chromStart, chromEnd);

/* expand to find the furthest boundaries of alignments */
expandToMaxAlignment(pslList, chrom, &chromStart, &chromEnd);

/* get the sequence */
genoSeq = hDnaFromSeq(gp->chrom, chromStart, chromEnd, dnaLower);

/* load and check the alignments */
maList = pslListToGgMrnaAliList(pslList, gp->chrom, chromStart, chromEnd, genoSeq, maxGap);

/* BAD_CODE: Not sure if this step is necessary... */
for(ma = maList; ma != NULL; ma = maNext)
    {
    maNext = ma->next;
    if( (ma->strand[0] == '+' && ma->orientation == 1) ||
	(ma->strand[0] == '-' && ma->orientation == -1)) 
	{
	slSafeAddHead(&maSameStrand, ma);
	}
    else
	ggMrnaAliFree(&ma);
    }
slReverse(&maSameStrand);

/* If there is a cluster to work with create an geneGraph */
if(maSameStrand != NULL)
    {
    ci = ggMrnaInputFromAlignments(maSameStrand, genoSeq);
    mcList = ggClusterMrna(ci);
    /* Get the largest cluster, gene predictions come sorted
     * smallest first. By looking at only the largest cluster
     * we can thus avoid duplicates. */
    slSort(&mcList, mcLargestFirstCmp);
    mc = mcList;
    clusterCount++;
    gg = ggGraphCluster(mc, ci);
    assert(checkEvidenceMatrix(gg));
    ggFillInTissuesAndLibraries(gg, conn);
    ag = ggToAltGraphX(gg);
    if(ag != NULL)
	{
	struct geneGraph *gTemp = NULL;
	struct ggEdge *cassettes = NULL;
	
	/* Convert back to genomic coordinates. */
	altGraphXoffset(ag, chromStart);
	/* Sort vertices so that they are chromosomal order */
	altGraphXVertPosSort(ag);
	
	/* See if we have any cassette exons */
	gTemp = altGraphXToGG(ag);
	cassettes = ggFindCassetteExons(gTemp);
	if(cassettes != NULL)
	    {
	    struct ggEdge *edge = NULL;
	    warn("Cassette %s:%d-%d", gTemp->tName, gTemp->tStart, gTemp->tEnd);
	    for(edge = cassettes; edge != NULL; edge = edge->next)
		{
		int length = ag->vPositions[edge->vertex2] - ag->vPositions[edge->vertex1];
		int edgeNum = -1;
		if(length % 3 != 0)
		    {
		    warn("Exon changes coding. Length is %d, mod 3 is %d", length, length %3);
		    misSense++;
		    }
		warn("Exon %s:%d-%d", gTemp->tName, gTemp->vertices[edge->vertex1].position, gTemp->vertices[edge->vertex2].position);
		edgeNum = agIndexFromEdge(ag, edge);
		assert(edgeNum != -1);
		ag->edgeTypes[edgeNum] = ggCassette;
		cassetteCount++;
		}
	    slFreeList(&cassettes);
	    }
	freeGeneGraph(&gTemp);
	/* write to file */
	altGraphXTabOut(ag, out);    /* genoSeq and maList are freed with ci and gg */
	}
    ggFreeMrnaClusterList(&mcList);
    freeGeneGraph(&gg);
    freeGgMrnaInput(&ci);
    pslFreeList(&pslList);
    }
else
    ag = NULL;
return ag;
}

struct genePred *loadSanger22Annotations(struct sqlConnection *conn)
/* load all of the sanger 22 gene annotations for chr22 */
{
char buff[256];
struct sqlResult *sr = NULL;
struct genePred *gp=NULL, *gpList = NULL;
char **row;
int rowOffset;
sr = hRangeQuery(conn, "sanger22", "chr22", 0, 47748585, NULL, &rowOffset);
while((row = sqlNextRow(sr)) != NULL)
    {
    gp = genePredLoad(row+rowOffset);
    slAddHead(&gpList, gp);
    }
slReverse(&gpList);
sqlFreeResult(&sr);
return gpList;
}

struct genePred *loadRefGene22Annotations(struct sqlConnection *conn)
/* load all of the refSeq gene annotations for chr22 */
{
char buff[256];
struct sqlResult *sr = NULL;
struct genePred *gp=NULL, *gpList = NULL;
char **row;
int rowOffset;
sr = hRangeQuery(conn, "refGene", "chr22", 0, 47748585, NULL, &rowOffset);
while((row = sqlNextRow(sr)) != NULL)
    {
    gp = genePredLoad(row+rowOffset);
    slAddHead(&gpList, gp);
    }
slReverse(&gpList);
sqlFreeResult(&sr);
return gpList;
}

struct genePred *convertBedsToGps(char *bedFile)
/* Load beds from a file and convert to bare bones genePredictions. */
{
struct genePred *gpList = NULL, *gp =NULL;
struct bed bed;
char *row[4];
struct lineFile *lf = lineFileOpen(bedFile, TRUE);
while(lineFileRow(lf, row))
    {
    bedStaticLoad(row, &bed);
    AllocVar(gp);
    gp->chrom = cloneString(bed.chrom);
    gp->txStart = bed.chromStart;
    gp->txEnd = bed.chromEnd;
    slAddHead(&gpList, gp);
    }
return gpList;
}
int gpSmallestFirstCmp(const void *va, const void *vb)
/* Compare to sort based on chrom and
 * sizes of chromEnd - chromStart, smallest first. */
{
const struct genePred *a = *((struct genePred **)va);
const struct genePred *b = *((struct genePred **)vb);
int dif;
dif = strcmp(a->chrom, b->chrom);
if (dif == 0)
    dif = abs(a->txEnd -a->txStart) - abs(b->txEnd - b->txStart);
return dif;
}

void createAltSplices(char *outFile, char *bedFile)
/* Top level routine, gets genePredictions and runs through them to 
   build altSplice graphs. */
{
struct genePred *gp = NULL, *gpList = NULL;
struct altGraphX *ag=NULL, *agList= NULL;
FILE *out = NULL;
struct sqlConnection *conn = hAllocConn();
if(bedFile != NULL)
    gpList = convertBedsToGps(bedFile);
else
    gpList = loadRefGene22Annotations(conn);
slSort(&gpList, gpSmallestFirstCmp);
out = mustOpen(outFile, "w");
for(gp = gpList; gp != NULL; gp = gp->next)
    {
    ag = agFromGp(gp, conn, 5, out);
    if(ag != NULL)
	{

	slAddHead(&agList, ag);
	}
    }
slReverse(&agList);
altGraphXFreeList(&agList);
hFreeConn(&conn);
uglyf("%d genePredictions with %d clusters, %d cassette exons, %d of which introduce missense mutations.\n",
      slCount(gpList), clusterCount, cassetteCount, misSense);
}


int main(int argc, char *argv[])
/* main routine, calls cgiSpoof and sets database */
{
char *outFile = NULL;
char *bedFile = NULL;
if(argc != 3 && argc != 4)
    usage();
if(argc == 4)
    bedFile = argv[3];
hSetDb(cloneString(argv[1]));
outFile = cloneString(argv[2]);
cgiSpoof(&argc, argv);
createAltSplices(outFile,bedFile);
return 0;
}

