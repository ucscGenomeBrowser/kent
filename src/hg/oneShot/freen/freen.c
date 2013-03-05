/* freen - My Pet Freen. */
#include "common.h"
#include "linefile.h"
#include "localmem.h"
#include "hash.h"
#include "options.h"
#include "ra.h"
#include "basicBed.h"

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen val desiredVal\n");
}

struct thisAndThat
   {
   struct thisAndThat *next;
   char *this;
   int that;
   boolean and;
   };


void freen(char *input)
/* Test some hair-brained thing. */
{
struct thisAndThat tat = {.this = "Hello", .and=FALSE};
printf(".next=%p .this = %s, that = %d, and = %d\n", tat.next, tat.this, tat.that, tat.and);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
freen(argv[1]);
return 0;
}
