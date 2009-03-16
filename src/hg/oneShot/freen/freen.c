/* freen - My Pet Freen. */
#include "common.h"
#include "memalloc.h"
#include "dystring.h"
#include "linefile.h"
#include "hash.h"
#include "bed.h"
#include "asParse.h"
#include "jksql.h"


static char const rcsid[] = "$Id: freen.c,v 1.87 2009/03/16 01:01:20 kent Exp $";

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen file\n");
}


void freen(char *s)
/* Test some hair-brained thing. */
{
int count = atoi(s);
char *asDef = bedAsDef(count);
printf("%s",  bedAsDef(count));
struct asObject *as = asParseText(asDef);
printf("%d rows\n", slCount(as->columnList));
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
freen(argv[1]);
return 0;
}
