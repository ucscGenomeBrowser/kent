/* freen - My Pet Freen. */
#include "common.h"
#include "options.h"
#include "zlibFace.h"
#include "memalloc.h"
#include "dystring.h"
#include "linefile.h"
#include "hash.h"
#include "obscure.h"

char *simplifyPathToDir(char *path);

static char const rcsid[] = "$Id: freen.c,v 1.93 2009/11/22 00:18:05 kent Exp $";

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen file\n");
}

static struct optionSpec options[] = {
   {NULL, 0},
};


void freen(char *input)
/* Test some hair-brained thing. */
{
extern void simplifyPathToDirSelfTest();
simplifyPathToDirSelfTest();
printf("%s\n", simplifyPathToDir(input));
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
freen(argv[1]);
return 0;
}
