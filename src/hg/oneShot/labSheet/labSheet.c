/* labSheet - Make a tab-separated file for turning into a lab spreadsheet.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

static char const rcsid[] = "$Id: labSheet.c,v 1.1 2004/09/24 15:20:32 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "labSheet - Make a tab-separated file for turning into a lab spreadsheet.\n"
  "usage:\n"
  "   labSheet XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void labSheet(char *XXX)
/* labSheet - Make a tab-separated file for turning into a lab spreadsheet.. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
labSheet(argv[1]);
return 0;
}
