/* hgTracksRandom - For a given organism, view hgTracks (default tracks) in random position.
                    Record display time. */

#include "common.h"

#include "chromInfo.h"
#include "dystring.h"
#include "hash.h"
#include "hdb.h"
#include "htmlPage.h"
#include "jksql.h"
#include "options.h"
#include "portable.h"

static char const rcsid[] = "$Id: hgTracksRandom.c,v 1.4 2005/10/17 22:08:25 heather Exp $";

static char *database = NULL;
static struct hash *chromHash = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgTracksRandom - Time default view for random position\n"
  "usage:\n"
  "  hgTracksRandom url \n");
}

/* Copied from hgLoadWiggle. */
static struct hash *loadAllChromInfo()
/* Load up all chromosome infos. */
{
struct chromInfo *el;
struct sqlConnection *conn = sqlConnect(database);
struct sqlResult *sr = NULL;
struct hash *ret;
char **row;

ret = newHash(0);

sr = sqlGetResult(conn, "select * from chromInfo");
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = chromInfoLoad(row);
    verbose(4, "Add hash %s value %u (%#lx)\n", el->chrom, el->size, (unsigned long)&el->size);
    hashAdd(ret, el->chrom, (void *)(& el->size));
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
return ret;
}

/* also copied from hgLoadWiggle. */
static unsigned getChromSize(char *chrom)
/* Return size of chrom.  */
{
struct hashEl *el = hashLookup(chromHash,chrom);

if (el == NULL)
    errAbort("Couldn't find size of chrom %s", chrom);
return *(unsigned *)el->val;
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
int chromSize = 0;
int windowSize = 100000;

if (argc != 2)
    usage();

srand( (unsigned) time(NULL) );
database = needMem(16);
strcpy(database, "hg17");
hSetDb(database);
chromHash = loadAllChromInfo();
chromSize = getChromSize(chrom);
startPos = getStartPos(chromSize, windowSize);

dy = newDyString(256);
dyStringPrintf(dy, "%s?db=hg17&position=%s:%d-%d", argv[1], chrom, startPos, startPos + windowSize);
printf("url = %s\n", dy->string);

hgTracksRandom(dy->string);
return 0;
}
