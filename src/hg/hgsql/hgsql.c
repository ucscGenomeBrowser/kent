/* hgsql - Execute some sql code using passwords in .hg.conf. */
#include "common.h"
#include "options.h"
#include "hgConfig.h"
#include "sqlProg.h"

static char const rcsid[] = "$Id: hgsql.c,v 1.8 2005/10/27 23:36:52 markd Exp $";

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
  );
}

void hgsql(int argc, char *argv[])
/* hgsql - Execute some sql code using passwords in .hg.conf. */
{
static char *progArgs[] = {"-A", NULL};
sqlExecProg("mysql", progArgs, argc, argv);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc <= 1)
    usage();
hgsql(argc-1, argv+1);
return 0;  /* never reaches here */
}
