/* cdwFreen - Temporary scaffolding frequently repurposed. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "intValTree.h"
#include "cdw.h"
#include "cdwLib.h"
#include "tagStorm.h"
#include "fieldedTable.h"
#include "rql.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwFreen - Temporary scaffolding frequently repurposed\n"
  "usage:\n"
  "   cdwFreen output\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};


void cdwFreen(char *table)
/* cdwFreen - Temporary scaffolding frequently repurposed. */
{
struct sqlConnection *conn = cdwConnect();
struct sqlResult *sr = sqlDescribe(conn, table);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    printf("%s\t%s\n", row[0], row[1]);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
cdwFreen(argv[1]);
return 0;
}
