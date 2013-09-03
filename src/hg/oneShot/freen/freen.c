/* freen - My Pet Freen. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "encode3/encode3Valid.h"

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen fileName\n");
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void freen(char *fileName)
{
boolean isGzipped = encode3IsGzipped(fileName);
printf("%s %s gzipped\n", fileName, (isGzipped ? "is" : "is not"));
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
