/* freen - My Pet Freen. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "jsHelper.h"

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen input\n");
}

void freen(char *input)
/* Test some hair-brained thing. */
{
size_t size;
char *buf;
readInGulp(input, &buf, &size);
char *untagged = stripRegEx(buf, "<[^>]*>", REG_ICASE);
puts(untagged);
}

int main(int argc, char *argv[])
/* Process command line. */
{
// optionInit(&argc, argv, options);

if (argc != 2)
    usage();
freen(argv[1]);
return 0;
}
