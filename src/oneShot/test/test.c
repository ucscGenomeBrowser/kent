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

void testMemLevels(int level)
/* Test memory at a certain level. */
{
if (level <= 0)
    return;
printf("start level %d - %d blocks allocated\n", level, carefulCountBlocksAllocated());
memTrackerStart();
    {
    char *a = needMem(10000);
    char *b = needMem(10000);
    char *c, *d;
    testMemLevels(level-1);
    c = needMem(20000);
    freez(&b);
    printf("mid level %d - %d blocks allocated\n", level, carefulCountBlocksAllocated());
    b = needMem(40000);
    d = needMem(123);
    printf("late level %d - %d blocks allocated\n", level, carefulCountBlocksAllocated());
    }
memTrackerEnd();
printf("end level %d - %d blocks allocated\n", level, carefulCountBlocksAllocated());
}

void test(char *s)
/* test - Test something. */
{
testMemLevels(1);
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
