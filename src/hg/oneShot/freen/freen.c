/* freen - My Pet Freen. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "htmlPage.h"

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen fileName\n");
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void freen(char *url)
{
int i;
for (i=45547; i<= 45580; ++i)
    printf(" %d", i);
printf("\n");
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
