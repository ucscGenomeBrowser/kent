/* hgsqladmin - Wrapper around mysqladmin using passwords in .hg.conf. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "options.h"
#include "sqlProg.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgsqladmin - Wrapper around mysqladmin using passwords in .hg.conf\n"
  );
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc < 2)
    usage();
sqlExecProg("mysqladmin", NULL, argc-1, argv+1);
return 0;  /* never reaches here */
}
