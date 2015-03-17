/* freen - My Pet Freen. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "genePred.h"
#include "rangeTree.h"

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen input\n");
}

//static struct optionSpec options[] = {
 //  {NULL, 0},
//};

void freen(char *chrom)
/* Test something */
{
uglyTime(NULL);
struct sqlConnection *conn = sqlConnect("hg19");
uglyTime("connect");
char query[512];
sqlSafef(query, sizeof(query), "select * from knownGene where chrom='%s'", chrom);
struct sqlResult *sr = sqlGetResult(conn, query);
uglyTime("get result");
char **row;
struct rbTree *rt = rangeTreeNew();
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct genePred *gp = genePredLoad(row);
    int i;
    int exonCount = gp->exonCount;
    for (i=0; i<exonCount; ++i)
        rangeTreeAdd(rt, gp->exonStarts[i], gp->exonEnds[i]);
    }
uglyTime("Add rows");
struct range *list = rangeTreeList(rt);
uglyTime("Did list");
uglyf("%d items in chrom %s\n", slCount(list), chrom);
}

int main(int argc, char *argv[])
/* Process command line. */
{
// optionInit(&argc, argv, options);
if (argc != 2)
    usage();
freen(argv[1]);
return 0;
}

