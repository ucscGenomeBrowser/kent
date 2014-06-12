/* freen - My Pet Freen. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "expData.h"
#include "sqlList.h"

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen output\n");
}

static struct optionSpec options[] = {
   {NULL, 0},
};


void freen(char *output)
/* Do something, who knows what really */
{
FILE *f = mustOpen(output, "w");
struct sqlConnection *conn = sqlConnect("hgFixed");
char query[512];
sqlSafef(query, sizeof(query), "select * from gnfHumanAtlas2Median limit 2");
uglyf("query: %s\n", query);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
struct expData *list = NULL;
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct expData *exp = expDataLoad(row);
    fprintf(f, "%s %d\n", exp->name, exp->expCount);
    int i;
    for (i=0; i<exp->expCount; ++i)
        fprintf(f, "    %g\n", exp->expScores[i]);
    slAddHead(&list, exp);
    }
slReverse(&list);

uglyf("%d on list\n", slCount(list));
expDataFreeList(&list);
carefulClose(&f);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
freen(argv[1]);
return 0;
}
