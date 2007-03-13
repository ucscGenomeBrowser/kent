/* txCdsGoodBed - Create positive example training set for SVM. This is based on
 * the refSeq reviewed genes, but we fragment a certain percentage of them so as 
 * not to end up with a SVM that *requires* a complete transcript. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bed.h"
#include "genePred.h"
#include "obscure.h"
#include "jksql.h"

static char const rcsid[] = "$Id: txCdsGoodBed.c,v 1.1 2007/03/13 05:33:28 kent Exp $";

double frag = 0.15;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txCdsGoodBed - Create positive example training set for SVM. This is \n"
  "based on the refSeq reviewed genes, but we fragment a certain percentage \n"
  "of them so as not to end up with a SVM that *requires* a complete \n"
  "transcript.\n"
  "usage:\n"
  "   txCdsGoodBed database output.bed\n"
  "options:\n"
  "   -frag=0.N - Amount to fragment. Default %g\n"
  , frag
  );
}

static struct optionSpec options[] = {
   {"frag", OPTION_DOUBLE},
   {NULL, 0},
};

void gpFragLimits(struct genePred *gp, double startRatio, double endRatio, 
	int *retStart, int *retEnd)
/* Convert floating point range from 0-1 representing the cDNA position
 * to genomic start/end. */
{
int geneBases = genePredBases(gp);
int cdnaStart = geneBases * startRatio;
int cdnaEnd = geneBases * endRatio;
int i;
int cdnaPos = 0;
if (cdnaStart <= 0)
    *retStart = gp->txStart;
if (cdnaEnd >= geneBases)
    *retEnd = gp->txEnd;
for (i=0; i<gp->exonCount; ++i)
    {
    int exonStart = gp->exonStarts[i];
    int exonEnd = gp->exonEnds[i];
    int exonSize = exonEnd - exonStart;
    if (cdnaPos <= cdnaStart && cdnaStart < cdnaPos + exonSize)
        *retStart = exonStart + (cdnaStart - cdnaPos);
    if (cdnaPos < cdnaEnd && cdnaEnd <= cdnaPos + exonSize)
        *retEnd = exonStart + (cdnaEnd - cdnaPos);
    cdnaPos += exonSize;
    }
}

void txCdsGoodBed(char *database, char *out)
/* txCdsGoodBed - Create positive example training set for SVM. This is based on
 * the refSeq reviewed genes, but we fragment a certain percentage of them so as 
 * not to end up with a SVM that *requires* a complete transcript. */
{
struct sqlConnection *conn = sqlConnect(database);
char *refTrack = "refGene";
char *statusTable = "refSeqStatus";
if (!sqlTableExists(conn, refTrack))
    errAbort("table %s doesn't exist in %s", refTrack, database);
if (!sqlTableExists(conn, statusTable))
    errAbort("table %s doesn't exist in %s", statusTable, database);
FILE *f = mustOpen(out, "w");
char *query =
   "select name,chrom,strand,txStart,txEnd,cdsStart,cdsEnd,exonCount,exonStarts,exonEnds "
   "from refGene r,refSeqStatus s where r.name=s.mrnaAcc and s.status='Reviewed'";
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
double randScale = 1.0/RAND_MAX;
int id = 0;
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct genePred *gp = genePredLoad(row);
    int start = gp->txStart, end = gp->txEnd;
    char *type = "refReviewed";
    if (rand()*randScale < frag)
        {
	double midRatio = rand()*randScale;
	if (midRatio > 0.5)
	     gpFragLimits(gp, 0, midRatio, &start, &end);
	else
	     gpFragLimits(gp, midRatio, 1.0, &start, &end);
	type = "refFrag";
	}
    gpPartOutAsBed(gp, start, end, f, type, ++id, 0);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
frag = optionDouble("frag", frag);
txCdsGoodBed(argv[1], argv[2]);
return 0;
}
