/* construct altGraph structures for genePredictions using information in est and mrna alignments */
#include "common.h"
#include "geneGraph.h"
#include "genePred.h"
#include "altGraph.h"
#include "ggMrnaAli.h"
#include "psl.h"
#include "dnaseq.h"
#include "hdb.h"
#include "jksql.h"
#include "cheapcgi.h"


void usage()
/* print usage and quit */
{
errAbort("altSplice - constructs altSplice graphs using information alignments\n"
	 "in est and mrna databases. At first only for chr22\n"
	 "usage:\n\taltSplice <db> <outputFile.AltGraph.tab>\n");
}

struct psl *loadPslsFromDb(struct sqlConnection *conn, int numTables, char **tables, char *chrom, unsigned int chromStart, unsigned int chromEnd)
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

struct altGraph *agFromGp(struct genePred *gp, struct sqlConnection *conn, int maxGap)
{
struct altGraph *ag = NULL;
struct ggMrnaCluster *mcList=NULL, *mc=NULL;
struct ggMrnaInput *ci = NULL;
struct geneGraph *gg = NULL;
struct dnaSeq *genoSeq = NULL;
char *tablePrefixes[] = {"_mrna", "_intronEst"};
char **tables = NULL;
int i;
char buff[256];
struct ggMrnaAli *maList=NULL, *ma=NULL, *maNext=NULL, *maSameStrand=NULL;
struct psl *pslList = NULL;
/* make the tables */
tables = needMem(sizeof(char*) * ArraySize(tablePrefixes));
for(i=0; i< ArraySize(tablePrefixes); i++)
    {
    snprintf(buff, sizeof(buff),"%s%s", gp->chrom, tablePrefixes[i]);
    tables[i] = cloneString(buff);
    }

/* load the psls */
pslList = loadPslsFromDb(conn, ArraySize(tablePrefixes), tables, gp->chrom, gp->txStart, gp->txEnd);

/* get the sequence */
genoSeq = hDnaFromSeq(gp->chrom, gp->txStart, gp->txEnd, dnaLower);
maList = pslListToGgMrnaAliList(pslList, gp->chrom, gp->txStart, gp->txEnd, genoSeq, maxGap);
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
if(maSameStrand != NULL)
    {
    ci = ggMrnaInputFromAlignments(maSameStrand, genoSeq);
    mcList = ggClusterMrna(ci);
    gg = ggGraphCluster(mcList, ci);
    ag = ggToAltGraph(gg);
    
    freeGgMrnaInput(&ci);
    freeGeneGraph(&gg);
    pslFreeList(&pslList);
    }
else
    ag = NULL;
/* genoSeq and maList are freed with ci and gg */
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


void createAltSplices(char *outFile)
/* top level routine, gets genePredictions and runs through them to 
   build altSplice graphs */
{
struct genePred *gp = NULL, *gpList = NULL;
struct altGraph *ag=NULL, *agList= NULL;
FILE *out = NULL;
struct sqlConnection *conn = hAllocConn();
gpList = loadRefGene22Annotations(conn);
out = mustOpen(outFile, "w");
for(gp = gpList; gp != NULL; gp = gp->next)
    {
    ag = agFromGp(gp, conn, 5);
    if(ag != NULL)
	{
	altGraphTabOut(ag, out);
	slAddHead(&agList, ag);
	}
    }
slReverse(&agList);
altGraphFreeList(&agList);
hFreeConn(&conn);
}


int main(int argc, char *argv[])
/* main routine, calls cgiSpoof and sets database */
{
char *outFile = NULL;
if(argc != 3)
    usage();
hSetDb(cloneString(argv[1]));
outFile = cloneString(argv[2]);
cgiSpoof(&argc, argv);
createAltSplices(outFile);
return 0;
}

