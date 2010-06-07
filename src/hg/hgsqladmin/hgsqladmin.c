/* hgsqladmin - Wrapper around mysqladmin using passwords in .hg.conf. */
#include "common.h"
#include "options.h"
#include "sqlProg.h"

static char const rcsid[] = "$Id: hgsqladmin.c,v 1.2 2005/10/27 23:36:52 markd Exp $";

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
