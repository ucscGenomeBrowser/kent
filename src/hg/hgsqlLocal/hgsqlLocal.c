/* hgsql - Execute some sql code using passwords in .hg.conf. */
#include "common.h"
#include "options.h"
#include "hgConfig.h"
#include "sqlProg.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgsqlLocal - Execute some sql code using localDb.XXX in .hg.conf\n"
  "usage:\n"
  "   hgsqlLocal [mysqlOptions] [database]\n"
  "or:\n"
  "   hgsqlLocal [mysqlOptions] [database] < file.sql\n"
  "Generally anything in command line is passed to mysql\n"
  "after an implicit '-A -u user -ppassword'.  If no options\n"
  "or database is specified, this usage message is printed."
  "\n"
  );
}

void hgsqlLocal(int argc, char *argv[])
/* hgsql - Execute some sql code using passwords in .hg.conf. */
{
static char *progArgs[] = {"-A", NULL};
sqlExecProgLocal("mysql", progArgs, argc, argv);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc <= 1)
    usage();
hgsqlLocal(argc-1, argv+1);
return 0;  /* never reaches here */
}
