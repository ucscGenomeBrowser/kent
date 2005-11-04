/* cactiSine - A little cacti data source.  Called by cacti every 5 minutes.  Returns a sine wave over time. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include <math.h>

static char const rcsid[] = "$Id: cactiSine.c,v 1.1 2005/11/04 19:38:13 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cactiSine - A little cacti data source.  Called by cacti every 5 minutes.  Returns a sine wave over time\n"
  "usage:\n"
  "   cactiSine XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void cactiSine(char *XXX)
/* cactiSine - A little cacti data source.  Called by cacti every 5 minutes.  Returns a sine wave over time. */
{
double seconds = time(NULL);
double hours = seconds/3600;
double radians = hours * 2 * 3.14;
printf("sin: %4.2f\n", sin(radians)*100);
printf("cos: %4.2f\n", cos(radians)*100);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
cactiSine(argv[1]);
return 0;
}
