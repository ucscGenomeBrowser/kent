/* hgLoadBlastTab - Load blast table into database. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

static char const rcsid[] = "$Id: hgLoadBlastTab.c,v 1.1 2003/06/10 22:35:34 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgLoadBlastTab - Load blast table into database\n"
  "usage:\n"
  "   hgLoadBlastTab XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void hgLoadBlastTab(char *XXX)
/* hgLoadBlastTab - Load blast table into database. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
hgLoadBlastTab(argv[1]);
return 0;
}
