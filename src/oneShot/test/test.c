/* test - Test something. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "memalloc.h"
#include "obscure.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "test - Test something\n"
  "usage:\n"
  "   test something\n"
  "options:\n"
  );
}

void test(char *s)
/* test - Test something. */
{
double a1[3] = {1, 2, 3, };
double a2[4] = {1, 2, 3, 4};
double a3[5] = {5, 1, 2, 3, 4};

printf("%f %f %f\n",
	doubleMedian(3, a1),
	doubleMedian(4, a2),
	doubleMedian(5, a3)
	);
}

int main(int argc, char *argv[])
/* Process command line. */
{
pushCarefulMemHandler(100000);
optionHash(&argc, argv);
if (argc != 2)
    usage();
test(argv[1]);
carefulCheckHeap();
return 0;
}
