/* freen - My Pet Freen. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "encode3/encode3Valid.h"

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen md5 size\n");
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void freen(char *md5, char *sizeString)
{
int size = atoi(sizeString);
printf("%s\n", encode3CalcValidationKey(md5, size));
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
freen(argv[1], argv[2]);
return 0;
}
