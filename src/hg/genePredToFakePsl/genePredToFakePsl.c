/* genePredToFakePsl - create fake .psl of mRNA aligned to dna from genePred file or table. */
#include "common.h"
#include "options.h"
#include "portable.h"
#include "hdb.h"
#include "genePred.h"
#include "genePredReader.h"
#include "psl.h"

static char const rcsid[] = "$Id: genePredToFakePsl.c,v 1.3 2008/09/03 19:18:39 markd Exp $";

/* Command line option specifications */
static struct optionSpec optionSpecs[] = {
    {NULL, 0}
};

void usage()
/* Explain usage and exit. */
{
errAbort(
  "genePredToFakePsl - Create a psl of fake-mRNA aligned to gene-preds from a file or table.\n"
  "usage:\n"
  "   genePredToFakePsl db fileTbl pslOut cdsOut\n"
  "\n"
  "If fileTbl is an existing file, then it is used.\n"
  "Otherwise, the table by this name is used.\n"
  "\n"
  "pslOut specifies the fake-mRNA output psl filename.\n"
  "\n"
  "cdsOut specifies the output cds tab-separated file which contains\n"
  "genbank-style CDS records showing cdsStart..cdsEnd\n"  
  "e.g. NM_123456 34..305\n"
  "\n");
}

void fakePslFromGenePred(char *db, char *fileTbl, char *pslOut, char *cdsOut)
/* check a genePred */
{
struct sqlConnection *conn = NULL;
struct genePredReader *gpr;
struct genePred *gp;
int iRec = 0;
int chromSize = -1; 
FILE *out = mustOpen(pslOut, "w");
FILE *cds = mustOpen(cdsOut, "w");

conn = hAllocConn(db);

if (fileExists(fileTbl))
    {
    gpr = genePredReaderFile(fileTbl, NULL);
    }
else
    {
    gpr = genePredReaderQuery(conn, fileTbl, NULL);
    }

while ((gp = genePredReaderNext(gpr)) != NULL)
    {
    struct psl *psl;
    int e = 0, qSize=0, qCdsStart=0, qCdsEnd=0;
    
    iRec++;
    chromSize = hChromSize(db, gp->chrom);
    for (e = 0; e < gp->exonCount; ++e)
	qSize+=(gp->exonEnds[e] - gp->exonStarts[e]);
    psl = pslNew(gp->name, qSize, 0, qSize,
                 gp->chrom, chromSize, gp->txStart, gp->txEnd,
                 gp->strand, gp->exonCount, 0);
    psl->blockCount = gp->exonCount;		    
    for (e = 0; e < gp->exonCount; ++e)
	{
	psl->blockSizes[e] = (gp->exonEnds[e] - gp->exonStarts[e]);
	psl->qStarts[e] = e==0 ? 0 : psl->qStarts[e-1] + psl->blockSizes[e-1];
	psl->tStarts[e] = gp->exonStarts[e];
	}
    psl->match = qSize;	
    psl->tNumInsert = psl->blockCount-1; 
    psl->tBaseInsert = (gp->txEnd - gp->txStart) - qSize;
    pslTabOut(psl, out);
    qCdsStart = gp->cdsStart - gp->txStart;
    qCdsEnd = gp->cdsEnd - gp->txStart;
    
    for (e = 1; e < gp->exonCount; ++e)
	{
	int intronSize = gp->exonStarts[e] - gp->exonEnds[e-1];
	if (gp->exonStarts[e] <= gp->cdsStart)
	    qCdsStart -= intronSize;
	if (gp->exonStarts[e] <= gp->cdsEnd)
	    qCdsEnd -= intronSize;
	} 
    
    if (gp->strand[0] == '-')
	{
	int temp = qCdsEnd;
	qCdsEnd = qSize - qCdsStart;
	qCdsStart = qSize - temp;
	}
	
    fprintf(cds,"%s\t", gp->name);
    //if (gp->strand[0] == '-')
    //	fprintf(cds,"complement(");
    
    fprintf(cds,"%d..%d", qCdsStart+1, qCdsEnd); /* genbank cds is closed 1-based */
    
    //if (gp->strand[0] == '-')
    //	fprintf(cds,")");
    fprintf(cds,"\n");
    
    pslFree(&psl);
    }
genePredReaderFree(&gpr);
if (conn != NULL)
    hFreeConn(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 5)
    usage();
fakePslFromGenePred(argv[1],argv[2],argv[3],argv[4]);
return 0;
}
