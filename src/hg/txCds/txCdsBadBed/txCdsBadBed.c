/* txCdsBadBed - Create a bed file with regions that don't really have CDS, 
 * but that might look like it.. */

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


void usage()
/* Explain usage and exit. */
{
errAbort(
  "txCdsBadBed - Create a bed file with regions that don't really have CDS, but\n"
  "that might look like it.\n"
  "usage:\n"
  "   txCdsBadBed db altSplice.bed output.bed\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

int id = 0;	/* Just count up to give each item a unique ID. */

static struct optionSpec options[] = {
   {NULL, 0},
};

struct bed *loadRetainedAndBleeding(char *fileName)
/* Load up all of the retained introns and bleeding exons. */
{
struct bed *bed, *bedList = NULL;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[6];
while (lineFileRow(lf, row))
    {
    char *type = row[3];
    if (sameString(type, "retainedIntron") || sameString(type, "bleedingExon"))
        {
	bed = bedLoad6(row);
	slAddHead(&bedList, bed);
	}
    }
lineFileClose(&lf);
slReverse(&bedList);
return bedList;
}

void txCdsBadBed(char *database, 
	char *altSpliceBed, char *outBed)
/* txCdsBadBed - Create a bed file with regions that don't really have CDS, 
 * but that might look like it.. */
{
/* Open up database and make sure all the tables we want are there. */
char *refTrack = "refGene";
char *vegaPseudo = "vegaPseudoGene";
char *retroPseudo = "retroMrnaInfo";
struct sqlConnection *conn = sqlConnect(database);
if (!sqlTableExists(conn, refTrack))
    errAbort("table %s doesn't exist in %s", refTrack, database);
if (!sqlTableExists(conn, vegaPseudo))
    errAbort("table %s doesn't exist in %s", vegaPseudo, database);
if (!sqlTableExists(conn, retroPseudo))
    errAbort("table %s doesn't exist in %s", retroPseudo, database);

/* Read in alt file and output larger retained and bleeding introns. */
struct bed *bed, *intronyList = loadRetainedAndBleeding(altSpliceBed);
FILE *f = mustOpen(outBed, "w");
for (bed = intronyList; bed != NULL; bed = bed->next)
    {
    int size = bed->chromEnd - bed->chromStart;
    if (size > 400)
	{
	fprintf(f, "%s\t%d\t%d\t", bed->chrom, bed->chromStart, bed->chromEnd);
	fprintf(f, "%s%d\t", bed->name, ++id);
	fprintf(f, "%d\t%s\t", bed->score, bed->strand);
	fprintf(f, "0\t0\t0\t1\t");
	fprintf(f, "%d,\t%d,\n", bed->chromEnd - bed->chromStart, 0);
	}
    }

/* Read in refGene, and write out larger 3' UTRs, and occassional antisense copies.  */
char query[512];
sqlSafef(query, sizeof(query), "select * from %s", refTrack);
int rowOffset = 0;
if (sqlFieldIndex(conn, refTrack, "bin") == 0)
    rowOffset = 1;
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct genePred *gp = genePredLoad(row + rowOffset);
    int start, end;
    if (gp->strand[0] == '+')
        {
	start = gp->cdsEnd;
	end = gp->txEnd;
	}
    else
        {
	start = gp->txStart;
	end = gp->cdsStart;
	}
    if (end - start > 400)
        {
	gpPartOutAsBed(gp, start, end, f, "utr", ++id, 400);
	}
    if (rand()%20 == 0)
        {
	gp->strand[0] = (gp->strand[0] == '+' ? '-' : '+');
	gpPartOutAsBed(gp, gp->txStart, gp->txEnd, f, "anti", ++id, 0);
	}
    }
sqlFreeResult(&sr);

/* Write out vega pseudo-genes. */
sqlSafef(query, sizeof(query), "select * from %s", vegaPseudo);
rowOffset = 0;
if (sqlFieldIndex(conn, vegaPseudo, "bin") == 0)
    rowOffset = 1;
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct genePred *gp = genePredLoad(row + rowOffset);
    gpPartOutAsBed(gp, gp->txStart, gp->txEnd, f, "vega", ++id, 0);
    }

/* Write out retroGenes. */
sqlSafef(query, sizeof(query), "select * from %s where score > 600", retroPseudo);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct bed *bed = bedLoad12(row);
    char name[128];
    safef(name, sizeof(name), "retro_%d_%s", ++id, bed->name);
    bed->name = name;
    bedTabOutN(bed, 12, f);
    }

carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
txCdsBadBed(argv[1], argv[2], argv[3]);
return 0;
}
