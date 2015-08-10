/* hgsql - Execute some sql code using connection info .hg.conf. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "options.h"
#include "hgConfig.h"
#include "sqlProg.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgsql - Execute some sql code using connection information in .hg.conf\n"
  "usage:\n"
  "   hgsql [mysqlOptions] [database]\n"
  "or:\n"
  "   hgsql [mysqlOptions] [database] < file.sql\n"
  "Generally anything in command line is passed to mysql\n"
  "after an implicit '-A -u user -ppassword'.  If no options\n"
  "or database is specified, this usage message is printed."
  "\n"
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
if (argc < 1)
    usage();
hgsql(argc-1, argv+1);
return 0;  /* never reaches here */
}
