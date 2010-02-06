/* freen - My Pet Freen. */
#include <unistd.h>
#include <math.h>

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "udc.h"
#include "localmem.h"
#include "bigWig.h"
#include "bigBed.h"
#include "memalloc.h"

#define TRAVERSE FALSE

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen input\n");
}



void freen(char *a)
/* Test some hair-brained thing. */
{
struct bbiFile        *bb;
struct lm             *lm;

pushCarefulMemHandler(1000000000);
printf("%d blocks %ld bytes after pushCarefulMemHandler\n", carefulCountBlocksAllocated(), carefulTotalAllocated());
bb = bigBedFileOpen(a);
printf("%d blocks %ld bytes after bigBedFileOpen\n", carefulCountBlocksAllocated(), carefulTotalAllocated());
int i;
for (i=0; i<2; ++i)
    {
    lm = lmInit(0);
    printf("%d blocks %ld bytes after lmInit\n", carefulCountBlocksAllocated(), carefulTotalAllocated());
    bigBedIntervalQuery(bb,"chr1",1,20000000,0,lm);
    printf("%d blocks %ld bytes allocated after intervalQuery\n", carefulCountBlocksAllocated(), carefulTotalAllocated());
    lmCleanup(&lm);
    printf("%d blocks %ld bytes allocated after lmCleanup\n", carefulCountBlocksAllocated(), carefulTotalAllocated());
    }
bbiFileClose(&bb);
printf("%d blocks %ld bytes allocated after bbiFileClose\n", carefulCountBlocksAllocated(), carefulTotalAllocated());
popMemHandler();
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
