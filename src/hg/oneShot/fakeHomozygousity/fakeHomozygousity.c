/* fakeHomozygousity - Fake homozygousity data. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "hdb.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "fakeHomozygousity - Fake homozygousity data\n"
  "usage:\n"
  "   fakeHomozygousity XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void fakeHomozygousity(char *output)
/* fakeHomozygousity - Fake homozygousity data. */
{
FILE *f = mustOpen(output, "w");
struct sqlConnection *conn = sqlConnect("hg18");

char query[1024];
sqlSafef(query, sizeof query, "select chrom,chromStart,chromEnd from ctgPos");
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *chrom = row[0];
    int start = sqlUnsigned(row[1]);
    int end = sqlUnsigned(row[2]);
    int pos;
    for (pos = start; pos <end; pos += 10000)
	{
	char *homozygous = (((rand()&3)==0) ? "1" : "0");
	fprintf(f, "%s\t%d\t%s\n", chrom, pos, homozygous);
	}
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
fakeHomozygousity(argv[1]);
return 0;
}
