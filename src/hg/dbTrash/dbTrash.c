/* dbTrash - drop tables from a database older than specified N hours. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

static char const rcsid[] = "$Id: dbTrash.c,v 1.1 2006/05/22 18:43:07 hiram Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "dbTrash - drop tables from a database older than specified N hours\n"
  "usage:\n"
  "   dbTrash XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void dbTrash(char *XXX)
/* dbTrash - drop tables from a database older than specified N hours. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
dbTrash(argv[1]);
return 0;
}
