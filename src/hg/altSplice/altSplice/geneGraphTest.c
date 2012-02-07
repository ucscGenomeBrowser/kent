/* some code to test out the functionaliy of geneGraph.c */

#include "common.h"
#include "geneGraph.h"
#include "altGraphX.h"
#include "ggMrnaAli.h"
#include "psl.h"
#include "dnaseq.h"
#include "hdb.h"
#include "jksql.h"
#include "cheapcgi.h"



void usage()
{
errAbort("geneGraphTest - some code to test out the functionality of geneGraph.c and\n"
	 "altGraphX.c.\n"
	 "usage:\n"
	 "\tgeneGraphTest <optional: coord chrN:10-20> <optional: numtimes for memory leaks\n");
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


void outputBed(struct altGraphX *ag, int chromStart, int chromEnd, FILE *out)
{
int i;
char buff[256];
/* output for visualization */
if(ag != NULL)
    {
    altGraphXoffset(ag, chromStart);
    }
fprintf(out, "track name=exons\n");
for(i=0; i < ag->edgeCount; i++)
    {
    sprintf(buff, "%d-%d", ag->edgeStarts[i], ag->edgeEnds[i]); 
    if( (ag->vTypes[ag->edgeStarts[i]] == ggHardStart || ag->vTypes[ag->edgeStarts[i]] == ggSoftStart)  && ag->vTypes[ag->edgeEnds[i]] == ggHardEnd)
	fprintf(out, "%s\t%d\t%d\t%s\n", ag->tName, (ag->vPositions[ag->edgeStarts[i]] + ag->tStart), (ag->vPositions[ag->edgeEnds[i]] + ag->tStart),buff);
    }
fprintf(out, "track name=introns\n");
for(i=0; i < ag->edgeCount; i++)
    {
    sprintf(buff, "%d-%d", ag->edgeStarts[i], ag->edgeEnds[i]); 
    if(ag->vTypes[ag->edgeStarts[i]] == ggHardEnd && ag->vTypes[ag->edgeEnds[i]] == ggHardStart)
	fprintf(out, "%s\t%d\t%d\t%s\n", ag->tName, (ag->vPositions[ag->edgeStarts[i]] + ag->tStart), (ag->vPositions[ag->edgeEnds[i]] + ag->tStart), buff);
    }
} 

void runTests(char *chrom, int chromStart, int chromEnd, char *db,  char **tables, int numTables, FILE *out)
{
int maxGap = 10, pathCount = 0;
struct dnaSeq *genoSeq = NULL;
struct psl *pslList = NULL, *psl=NULL;
struct sqlConnection *conn = NULL;
struct ggMrnaAli *maList = NULL, *ma=NULL, *maNext = NULL, *maSameStrand=NULL;
struct ggMrnaCluster *mcList=NULL, *mc=NULL;
struct ggMrnaInput *ci = NULL;
struct geneGraph *gg = NULL, *gg2=NULL, *gTemp = NULL;
struct altGraphX *ag = NULL, *ag2=NULL;
boolean sameGg = FALSE; /* flag for same geneGraph */
int i,j,k,l;
/* load the psls */
pslList = loadPslsFromDb(conn, numTables, tables, chrom, chromStart, chromEnd);

/* expand to find the furthest boundaries of alignments */
expandToMaxAlignment(pslList, chrom, &chromStart, &chromEnd);

/* get the sequence */
genoSeq = hDnaFromSeq(chrom, chromStart, chromEnd, dnaLower);

/* load and check the alignments */
maList = pslListToGgMrnaAliList(pslList, chrom, chromStart, chromEnd, genoSeq, maxGap);

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
    if(mcList == NULL)
	{	
	ggFreeMrnaClusterList(&mcList);
	freeGeneGraph(&gg);
	freeGgMrnaInput(&ci);
	pslFreeList(&pslList);
	return NULL;
	}
    
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
    }

outputBed(ag,out);
altGraphXTabOut(ag,stdout);
altGraphXFree(&ag);
ggFreeMrnaClusterList(&mcList);
freeGeneGraph(&gg);
freeGgMrnaInput(&ci);
pslFreeList(&pslList);

}


/* void runTests(char *chrom, int chromStart, int chromEnd, char *db,  char **tables, int numTables, FILE *out) */
/* { */
/* int maxGap = 10, pathCount = 0; */
/* struct dnaSeq *genoSeq = NULL; */
/* struct psl *pslList = NULL, *psl=NULL; */
/* struct sqlConnection *conn = NULL; */
/* struct ggMrnaAli *maList = NULL, *ma=NULL, *maNext = NULL, *maSameStrand=NULL; */
/* struct ggMrnaCluster *mcList=NULL, *mc=NULL; */
/* struct ggMrnaInput *ci = NULL; */
/* struct geneGraph *gg = NULL, *gg2=NULL, *gTemp = NULL; */
/* struct altGraphX *ag = NULL, *ag2=NULL; */
/* boolean sameGg = FALSE; /\* flag for same geneGraph *\/ */
/* int i,j,k,l; */
/* boolean keepGoing = TRUE; */
/* fprintf(out, "browser position %s:%d-%d\n", chrom, chromStart, chromEnd); */
/* warn("getting database connection."); */
/* hSetDb(db); */
/* conn = hAllocConn(); */
/* warn("getting genomic sequence."); */

/* warn("loading psls from %s with chrom=%s chromStart=%d chromEnd=%d", tables[0], chrom, chromStart, chromEnd); */
/* pslList = loadPslsFromDb(conn, numTables, tables, chrom, chromStart, chromEnd); */
/* expandToMaxAlignment(pslList, chrom, &chromStart, &chromEnd); */
/* warn("expanded alighnment to %s:%d-%d", chrom, chromStart, chromEnd); */
/* genoSeq = hDnaFromSeq(chrom, chromStart, chromEnd, dnaLower); */
/* maList = pslListToGgMrnaAliList(pslList, chrom, chromStart, chromEnd, genoSeq, maxGap); */
/* /\* check the orientation of our stuff to make sure it agrees. */
/*    should I take this out? *\/ */
/* for(ma = maList; ma != NULL; ma = maNext) */
/*     { */
/*     maNext = ma->next; */
/*     if( (ma->strand[0] == '+' && ma->orientation == 1) || */
/* 	(ma->strand[0] == '-' && ma->orientation == -1))  */
/* 	{ */
/* 	slSafeAddHead(&maSameStrand, ma); */
/* 	} */
/*     } */
/* slReverse(&maSameStrand); */
/* ci = ggMrnaInputFromAlignments(maSameStrand, genoSeq); */
/* mcList = ggClusterMrna(ci); */
/* for(mc = mcList; mc != NULL; mc = mc->next) */
/*     { */
/*     gg = ggGraphCluster(mc, ci); */
/*     ggFillInTissuesAndLibraries(gg, conn); */
/*     ag = ggToAltGraphX(gg); */
    
/*     if(ag != NULL) */
/* /\* check to make sure that we can convert back and forth between  */
/*    gene graph and altGraph *\/ */
/*     altGraphXVertPosSort(ag); */
/*     gg2 = altGraphXToGG(ag); */
/*     ag2 = ggToAltGraphX(gg2); */
/*     gTemp = altGraphXToGG(ag2); */
/*     sameGg = isSameGeneGraph(gTemp, gg2); */
/*     for(i=0; i<gTemp->vertexCount && keepGoing; i++) */
/* 	{ */
/* 	for(j=0; j<gTemp->vertexCount && keepGoing; j++) */
/* 	    { */
/* 	    if(gTemp->edgeMatrix[i][j] && gTemp->vertices[i].type == ggHardStart && gTemp->vertices[j].type == ggHardEnd) */
/* 		{ */
/* 		struct ggEdge v1; */
/* 		struct ggEdge v2; */
/* 		v1.vertex1 = i; */
/* 		v1.vertex2 = j; */
/* 		v1.type = ggExon; */
/* 		if(ggIsCassetteExonEdge(gTemp, i,j)) */
/* 		    uglyf("%s:%d-%d\n", gTemp->tName, gTemp->tStart, gTemp->tEnd); */
		
/* 		} */
/* 	    } */
/* 	} */
    
/*     if(!sameGg) */
/* 	warn("Appear to have lost something in gg -> ag -> gg transition."); */
/*     fprintf(out, "num alt splices is: %d\n", pathCount); */
/*     altGraphXTabOut(ag, out); */
/*     altGraphXFree(&ag); */
/*     freeGeneGraph(&gg); */
/*     freeGeneGraph(&gTemp); */
/*     ggFreeMrnaClusterList(&mcList); */
/*     freeGgMrnaInput(&ci); */
/*     } */
/* hFreeConn(&conn); */
/* ggMrnaAliFreeList(&maList); */
/* } */

int main(int argc, char *argv[])
{
char *db = "hg8";
char *chrom = "chr20";
char buff[256];
char *tablePrefixes[] = {"_mrna", "_intronEst"};
char **tables = NULL;
unsigned int chromStart = 59297050; //45347279;
unsigned int chromEnd = 59302330; //45428372;
int numTimes = 1;
int i;

FILE *out = mustOpen("geneGraphTest.bed", "w");
/* parse arguments ... */
if(argc == 2)
    {
    char *pos1 = NULL, *pos2 = NULL;
    pos1 = strstr(argv[1], ":");
    chrom = argv[1];
    *pos1 = '\0';
    pos1++;
    pos2 = strstr(pos1, "-");
    *pos2 = '\0';
    pos2++;
    chromStart = atoi(pos1);
    chromEnd = atoi(pos2);
    }
else if(argc == 3)
    {
    numTimes = atoi(argv[2]);
    }
else
    usage();
cgiSpoof(&argc, argv);	

tables = needMem(sizeof(char*) * ArraySize(tablePrefixes));
for(i=0; i< ArraySize(tablePrefixes); i++)
    {
    snprintf(buff, sizeof(buff),"%s%s", chrom, tablePrefixes[i]);
    tables[i] = cloneString(buff);
    }

for(i=0; i<numTimes; i++)
    runTests(chrom, chromStart, chromEnd, db, tables, ArraySize(tablePrefixes), out);

carefulClose(&out);

return 0;
}


