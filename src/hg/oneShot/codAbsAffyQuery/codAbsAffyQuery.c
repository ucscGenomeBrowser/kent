/* codAbsAffyQuery - C version of a SQL query.  Bigger code but faster.. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "codAbsAffyQuery - C version of a SQL query.  Bigger code but faster.\n"
  "usage:\n"
  "   codAbsAffyQuery XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void codAbsAffyQuery(char *XXX)
/* codAbsAffyQuery - C version of a SQL query.  Bigger code but faster.. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
codAbsAffyQuery(argv[1]);
return 0;
}
