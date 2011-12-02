/* hgsqldump - Execute mysqldump using passwords from .hg.conf. */
#include "common.h"
#include "options.h"
#include "sqlProg.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgsqldumpLocal - Execute mysqldump using passwords from .hg.conf\n"
  "usage:\n"
  "   hgsqldumpLocal [OPTIONS] database [tables]\n"
  "or:\n"
  "   hgsqldumpLocal [OPTIONS] --databases [OPTIONS] DB1 [DB2 DB3 ...]\n"
  "or:\n"
  "   hgsqldumpLocal [OPTIONS] --all-databases [OPTIONS]\n"
  "Generally anything in command line is passed to mysqldump\n"
  "\tafter an implicit '-u user -ppassword\n"
  "See also: mysqldump\n"
  "Note: directory for results must be writable by mysql.  i.e. 'chmod 777 .'\n"
  "Which is a security risk, so remember to change permissions back after use.\n"
  "e.g.: hgsqldump --all -c --tab=. cb1"
  "\n\n"
  );
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc <= 1)
    usage();

sqlExecProgLocal("mysqldump", NULL, argc-1, argv+1);
return 0;  /* never reaches here */
}
