/* hgsqlimport - Execute mysqlimport using passwords from .hg.conf. */
#include "common.h"
#include "options.h"
#include "sqlProg.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgsqlimport - Execute mysqlimport using passwords from .hg.conf\n"
  "usage:\n"
  "   hgsqlimport [OPTIONS] database textfile ...\n"
  "\n"
  );
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc <= 1)
    usage();
sqlExecProg("mysqlimport", NULL, argc-1, argv+1);
return 0;  /* never reaches here */
}
