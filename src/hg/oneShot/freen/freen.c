/* freen - My Pet Freen. */
#include <unistd.h>
#include <math.h>

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "net.h"

#define TRAVERSE FALSE

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen input\n");
}


void freen(char *input)
/* Test some hair-brained thing. */
{
struct lineFile *lf = netLineFileOpen(input);
char *line;
if (lineFileNext(lf, &line, NULL))
    printf("%s\n", line);
else
    printf("%s is empty\n", input);
lineFileClose(&lf);
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
