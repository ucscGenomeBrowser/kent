/* chainToAxt - Convert from chain to axt file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

static char const rcsid[] = "$Id: chainToAxt.c,v 1.1 2003/06/14 00:26:23 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "chainToAxt - Convert from chain to axt file\n"
  "usage:\n"
  "   chainToAxt XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void chainToAxt(char *XXX)
/* chainToAxt - Convert from chain to axt file. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
chainToAxt(argv[1]);
return 0;
}
