/* freen - My Pet Freen. */
#include "common.h"
#include "linefile.h"
#include "obscure.h"
#include "portable.h"

void usage()
/* Print usage and exit. */
{
errAbort("usage: freen power");
}

struct numList
/* A list of numbers. */
    {
    struct numList *next;
    int num;
    };

void freen(char *s)
/* Print status code. */
{
double power = 1.0/atof(s);
int i;
for (i=1; i<=10; ++i)
    printf("%d %f\n", i, pow(i, power));
for (i=10; i<1000000; i *= 10)
    printf("%d %f\n", i, pow(i, power));
}


int main(int argc, char *argv[])
/* Process command line. */
{
long t1, t2;
t1 = clock1000();
if (argc != 2)
   usage();
freen(argv[1]);
sleep(2);
t2 = clock1000();
printf("time %ld %ld %ld\n", t2-t1, t1, t2);
}
