/* cdwAddStep - Add a step to pipeline. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "cdw.h"
#include "cdwLib.h"
#include "cdwStep.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwAddStep - Add a step to pipeline\n"
  "usage:\n"
  "   cdwAddStep name 'description in quotes'\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void cdwAddStep(char *name, char *description)
/* cdwAddStep - Add a step to pipeline. */
{
struct sqlConnection *conn = cdwConnectReadWrite();
char query[4*1024];
sqlSafef(query, sizeof(query), "insert into cdwStepDef (name, description) values('%s','%s')",
    name, description);
sqlUpdate(conn, query);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
cdwAddStep(argv[1], argv[2]);
return 0;
}
