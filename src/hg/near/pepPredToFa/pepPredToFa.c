/* pepPredToFa - Convert a pepPred table to fasta format. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "pepPred.h"
#include "fa.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "pepPredToFa - Convert a pepPred table to fasta format\n"
  "usage:\n"
  "   pepPredToFa database table output.fa\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void pepPredToFa(char *database, char *table, char *outName)
/* pepPredToFa - Convert a pepPred table to fasta format. */
{
FILE *f = mustOpen(outName, "w");
struct sqlConnection *conn = sqlConnect(database);
struct sqlResult *sr;
char **row;
char query[512];
struct pepPred pp;

sqlSafef(query, sizeof(query), "select * from %s", table);
sr = sqlGetResult(conn, query);
while((row = sqlNextRow(sr)) != NULL)
    {
    pepPredStaticLoad(row, &pp);
    faWriteNext(f, pp.name, pp.seq, strlen(pp.seq));
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
pepPredToFa(argv[1], argv[2], argv[3]);
return 0;
}
