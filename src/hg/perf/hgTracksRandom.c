/* hgTracksRandom - For a given organism, view hgTracks (default tracks) in random position.
                    Record display time. */

#include "common.h"
#include "dystring.h"
#include "htmlPage.h"
#include "options.h"
#include "portable.h"

static char const rcsid[] = "$Id: hgTracksRandom.c,v 1.3 2005/10/17 15:55:39 heather Exp $";


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

int getStartPos(int chromSize, int windowSize)
/* return a random start position */
{
if (windowSize >= chromSize)
    return 0;
return rand() % (chromSize - windowSize);
}

int main(int argc, char *argv[])
{
struct dyString *dy = NULL;
int startPos = 1;
char *chrom = "chr1";
int chromSize = 245522847;
int windowSize = 100000;

if (argc != 2)
    usage();

srand( (unsigned) time(NULL) );

startPos = getStartPos(chromSize, windowSize);

dy = newDyString(256);
dyStringPrintf(dy, "%s?db=hg17&position=%s:%d-%d", argv[1], chrom, startPos, startPos + windowSize);
printf("url = %s\n", dy->string);

hgTracksRandom(dy->string);
return 0;
}
