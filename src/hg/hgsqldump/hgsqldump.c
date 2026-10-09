/* hgsqldump - Execute mariadb-dump using passwords from .hg.conf. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "options.h"
#include "sqlProg.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgsqldump - Execute mariadb-dump using passwords from .hg.conf\n"
  "usage:\n"
  "   hgsqldump [OPTIONS] database [tables]\n"
  "or:\n"
  "   hgsqldump [OPTIONS] --databases [OPTIONS] DB1 [DB2 DB3 ...]\n"
  "or:\n"
  "   hgsqldump [OPTIONS] --all-databases [OPTIONS]\n"
  "Generally anything in command line is passed to mariadb-dump\n"
  "\tafter an implicit '-u user -ppassword\n"
  "See also: mariadb-dump\n"
  "Note: directory for results must be writable by mariadb.  i.e. 'chmod 777 .'\n"
  "Which is a security risk, so remember to change permissions back after use.\n"
  "e.g.: hgsqldump --all -c --tab=. cb1"
  "\n"
  );
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc <= 1)
    usage();

sqlExecProg("mariadb-dump", NULL, argc-1, argv+1);
return 0;  /* never reaches here */
}
