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

void freen(char *url)
/* Test some hair-brained thing. */
{
struct lm *lm = lmInit(0);
samfile_t *fh = bamOpen(url, NULL);
struct samAlignment *el, *list;
list = bamReadNextSamAlignments(fh, 10, lm);
printf("Got %d aligmnents\n", slCount(list));
bamClose(&fh);
for (el = list; el != NULL; el = el->next)
    {
    samAlignmentTabOut(el, stdout);
    }
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
