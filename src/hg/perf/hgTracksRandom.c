/* hgTracksRandom - For a given organism, view hgTracks (default tracks) in random position.
                    Record display time. */

#include "common.h"
#include "dystring.h"
#include "options.h"
#include "portable.h"
#include "qa.h"

static char const rcsid[] = "$Id: hgTracksRandom.c,v 1.1 2005/10/14 04:01:46 heather Exp $";


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgTracksRandom - Time default view for random position\n"
  "usage:\n"
  "  hgTracksRandom url \n");
}

void hgTracksRandom(char *url)
/* hgTracksRandom - Time default view for random position. */
/* The URL can include position. */
{
long startTime, endTime;
struct htmlPage *rootPage;

startTime = clock1000();
rootPage = htmlPageGet(url);
endTime = clock1000();
printf("time = %ld\n", endTime - startTime);
}

int main(int argc, char *argv[])
{
struct dyString *dy = NULL;
if (argc != 2)
    usage();
dy = newDyString(256);
dyStringPrintf(dy, "%s?db=hg17&position=chr1:1-100000", argv[1]);
printf("url = %s\n", dy->string);
hgTracksRandom(dy->string);
return 0;
}
