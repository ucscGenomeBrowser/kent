/* test - Test something. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"
#include "memgfx.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "test - Test something\n"
  "usage:\n"
  "   test a \n"
  "options:\n"
  );
}

#include <termios.h>

int testInt(int a, int b, int c, int d)
{
return (a + b)*c/d;
}

double testFloat(double a, double b, double c, double d)
{
return (a + b)*c/d;
}


void test(char *in)
/* test - Test something. */
{
printf("%d\n", testInt(1,2,3,4));
printf("%f\n", testFloat(1.0,2.0,3.0,4.0));

}

int main(int argc, char *argv[], char *env[])
/* Process command line. */
{
int i;
if (argc != 2)
   usage();
test(argv[1]);
return 0;
}
