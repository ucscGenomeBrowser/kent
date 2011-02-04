/* freen - My Pet Freen. */
#include "common.h"
#include "linefile.h"
#include "localmem.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "samAlignment.h"
#include "dnaseq.h"
#include "bamFile.h"

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen input\n");
}

void freen(char *url, char *chrom, int start, int end)
/* Test some hair-brained thing. */
{
struct lm *lm = lmInit(0);
struct samAlignment *el, *list;
list = bamFetchSamAlignment(url, chrom, start, end, lm);
printf("Got %d aligmnents\n", slCount(list));
for (el = list; el != NULL; el = el->next)
    {
    samAlignmentTabOut(el, stdout);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
// optionInit(&argc, argv, options);

if (argc != 5)
    usage();
freen(argv[1], argv[2], atoi(argv[3]), atoi(argv[4]));
return 0;
}
