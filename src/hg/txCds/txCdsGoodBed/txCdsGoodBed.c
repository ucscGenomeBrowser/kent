/* txCdsGoodBed - Create positive example training set for SVM. This is based on
 * the refSeq reviewed genes, but we fragment a certain percentage of them so as 
 * not to end up with a SVM that *requires* a complete transcript. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bed.h"
#include "genePred.h"
#include "obscure.h"
#include "jksql.h"


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
  "   txCdsGoodBed database output.bed output.cds\n"
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



void gpPartOutAsCds(struct genePred *gp, int partStart, int partEnd, FILE *f,
	char *type, int id)
/* Write out CDS region of this fragment of gp.  It may in fact be empty,
 * which we'll represent as start=0, end=0 */
{
/* Truncate cds to fit inside of the interval we're actually outputting. 
 * If this results in us losing the CDS, just output a stub quickly. */
fprintf(f, "%s_%d_%s\t", type, id, gp->name);
int genoCdsStart = max(partStart, gp->cdsStart);
int genoCdsEnd = min(partEnd, gp->cdsEnd);
if (genoCdsStart >= genoCdsEnd)
    {
    fprintf(f, "0\t0\n");
    return;
    }

/*  Figure out amount of exonic sequence that is inside of our part
 *  and also in the first UTR or the CDS. */
int i;
int cdnaUtrUpSize = 0;
int cdnaUtrDownSize = 0;
int cdnaCdsSize = 0;
for (i=0; i<gp->exonCount; ++i)
    {
    int exonStart = gp->exonStarts[i];
    int exonEnd = gp->exonEnds[i];
    int utrUpStart = max(gp->txStart, exonStart);
    int utrUpEnd = min(gp->cdsStart, exonEnd);
    int utrDownStart = max(gp->cdsEnd, exonStart);
    int utrDownEnd = min(gp->txEnd, exonEnd);
    int cdsStart = max(gp->cdsStart, exonStart);
    int cdsEnd = min(gp->cdsEnd, exonEnd);
    cdnaUtrUpSize += positiveRangeIntersection(utrUpStart, utrUpEnd, partStart, partEnd);
    cdnaUtrDownSize += positiveRangeIntersection(utrDownStart, utrDownEnd, partStart, partEnd);
    cdnaCdsSize += positiveRangeIntersection(cdsStart, cdsEnd, partStart, partEnd);
    }

/* Output CDS start/end. */
if (gp->strand[0] == '+')
    fprintf(f, "%d\t%d\n", cdnaUtrUpSize, cdnaUtrUpSize+cdnaCdsSize);
else
    fprintf(f, "%d\t%d\n", cdnaUtrDownSize, cdnaUtrDownSize+cdnaCdsSize);
}

void txCdsGoodBed(char *database, char *outBed, char *outCds)
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
FILE *fBed = mustOpen(outBed, "w");
FILE *fCds = mustOpen(outCds, "w");
char *query =
   NOSQLINJ "select name,chrom,strand,txStart,txEnd,cdsStart,cdsEnd,exonCount,exonStarts,exonEnds "
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
    gpPartOutAsBed(gp, start, end, fBed, type, ++id, 0);
    gpPartOutAsCds(gp, start, end, fCds, type, id);
    }
carefulClose(&fBed);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
frag = optionDouble("frag", frag);
txCdsGoodBed(argv[1], argv[2], argv[3]);
return 0;
}
