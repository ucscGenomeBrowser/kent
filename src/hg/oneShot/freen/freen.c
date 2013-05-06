/* freen - My Pet Freen. */
#include <sys/statvfs.h>
#include <uuid/uuid.h>
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "cheapcgi.h"

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen input\n");
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void freen(char *input)
{
puts(cgiEncode(input));
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
