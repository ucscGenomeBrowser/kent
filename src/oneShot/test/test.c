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


void test(char *in)
/* test - Test something. */
{
clock_t now = clock();
long nowl = now;
time_t nowt = time(NULL);
char *times = ctime(&nowt);
struct tm *lt = localtime(&nowt);
char *atime = asctime(lt);
printf("now as long %ld\n", now);
printf("ctime '%s'\n", times);
printf("asctime '%s'\n", atime);
printf("time in components:\n");
printf("  year %d\n", 1900 + lt->tm_year);
printf("  month %d\n", lt->tm_mon);
printf("  day %d\n", lt->tm_mday);
printf("  hour %d\n", lt->tm_hour);
printf("  minute %d\n", lt->tm_min);
printf("  sec %d\n", lt->tm_sec);
printf("  zone %s\n", lt->tm_zone);
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
