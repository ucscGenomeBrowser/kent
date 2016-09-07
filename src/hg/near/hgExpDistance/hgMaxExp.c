/* hgExpDistance - Create table that measures expression distance between pairs. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "jksql.h"
#include "bed.h"
#include "hgRelate.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgMaxExp - Output maximum and minimum scores found in an exp table\n"
  "usage:\n"
  "   hgMaxExp database expTable\n"
  );
}

void hgMaxExp(char *database, char *table)
/* Output maximum expScore in table */
{
struct sqlConnection *conn = sqlConnect(database);
struct sqlResult *sr;
char query[256];
char **row;

/* Get list of all items with expression values. */
char *fieldNames = "name, expCount, expScores";
sqlSafef(query, sizeof(query), "select %s from %s", fieldNames, table);
sr = sqlGetResult(conn, query);
float maxScore = 0.0;
float minScore = 100000.0;
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *name = row[0];
    int expCount = sqlUnsigned(row[1]);
    int commaCount;
    float *expScores = NULL;
    sqlFloatDynamicArray(row[2], &expScores, &commaCount);
    if (expCount != commaCount)
        errAbort("expCount and expScores don't match on %s in %s", name, table);
    int i;
    for (i=0; i<expCount; i++)
        {
        maxScore = max(maxScore, expScores[i]);
        minScore = min(minScore, expScores[i]);
        }
    }
sqlFreeResult(&sr);
conn = sqlConnect(database);
sqlDisconnect(&conn);	/* Disconnect because next step is slow. */
printf("max: %0.2f  min: %0.2f\n", maxScore, minScore);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
hgMaxExp(argv[1], argv[2]);
return 0;
}
