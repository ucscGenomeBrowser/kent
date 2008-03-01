/* hgsql - Execute some sql code using passwords in .hg.conf. */
#include "common.h"
#include "options.h"
#include "hgConfig.h"
#include "sqlProg.h"

static char const rcsid[] = "$Id: hgsql.c,v 1.9 2008/03/01 00:20:53 jzhu Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgsql - Execute some sql code using passwords in .hg.conf\n"
  "usage:\n"
  "   hgsql [mysqlOptions] [database]\n"
  "or:\n"
  "   hgsql [mysqlOptions] [database] < file.sql\n"
  "Generally anything in command line is passed to mysql\n"
  "after an implicit '-A -u user -ppassword'.  If no options\n"
  "or database is specified, this usage message is printed."
  "\n\n"
  "Options:\n"
  "  -local - connect to local host, instead of default host, using localDb.XXX variables defined in .hg.conf.\n"
  );
}

static struct optionSpec optionSpecs[] = {
    {"local", OPTION_BOOLEAN},
};

void hgsql(int argc, char *argv[], boolean localDb)
/* hgsql - Execute some sql code using passwords in .hg.conf. */
{
static char *progArgs[] = {"-A", NULL};
if (localDb)
    sqlExecProgLocal("mysql", progArgs, argc, argv);
else
    sqlExecProg("mysql", progArgs, argc, argv);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc <= 1)
    usage();
boolean localDb = optionExists("local");

hgsql(argc-1, argv+1, localDb);
return 0;  /* never reaches here */
}
