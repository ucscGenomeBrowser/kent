/* freen - My Pet Freen. */
#include "common.h"
#include "memalloc.h"
#include "dystring.h"
#include "linefile.h"
#include "hash.h"


static char const rcsid[] = "$Id: freen.c,v 1.89 2009/09/14 18:13:15 kent Exp $";

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen file\n");
}


void freen(char *input)
/* Test some hair-brained thing. */
{
struct tm tm;
ZeroVar(&tm);
char *res = strptime(input, "%d/%b/%Y:%T", &tm);
time_t tick = mktime(&tm);
printf("res = %s\n", res);
printf("%ld ticks\n", (long)tick);
printf("%s\n", asctime(&tm));
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
freen(argv[1]);
return 0;
}
