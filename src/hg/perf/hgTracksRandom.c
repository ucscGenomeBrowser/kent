/* hgTracksRandom - For a given organism, view hgTracks (default tracks) in random position.
                    Record display time. */

#include "common.h"

#include "chromInfo.h"
#include "dystring.h"
#include "hash.h"
#include "hdb.h"
#include "htmlPage.h"
#include "linefile.h"
#include "jksql.h"
#include "options.h"
#include "portable.h"

static char const rcsid[] = "$Id: hgTracksRandom.c,v 1.13 2008/09/03 19:20:53 markd Exp $";

static char *database = NULL;
static struct hash *chromHash = NULL;

struct machine 
    {
    struct machine *next;
    char *name;
    };

struct machine *machineList;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgTracksRandom - Time default view for random position\n"
  "machines is a file listing the machines to test\n"
  "usage:\n"
  "  hgTracksRandom machines\n");
}

void getMachines(char *filename)
/* Read in list of machines to use. */
{
struct lineFile *lf = lineFileOpen(filename, TRUE);
char *line;
int lineSize;
struct machine *machine;

while (lineFileNext(lf, &line, &lineSize))
    {
    AllocVar(machine);
    // is this equivalent to slAddHead?
    machine->name = line;
    machine->next = machineList;
    machineList = machine;
    }
/* could reverse order here */
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



long hgTracksRandom(char *url)
/* hgTracksRandom - Time default view for random position. */
/* The URL can include position. */
{
long startTime, endTime;
struct htmlPage *rootPage;

startTime = clock1000();
rootPage = htmlPageGet(url);
endTime = clock1000();

htmlPageValidateOrAbort(rootPage);
if (rootPage->status->status != 200)
   errAbort("%s returned with status code %d", url, rootPage->status->status);

return endTime - startTime;
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
struct machine *machinePos;
time_t now;
char testTime[100];
char testDate[100];
long elapsedTime = 0;

now = time(NULL);
strftime(testTime, 100, "%H:%M", localtime(&now));
strftime(testDate, 100, "%B %d, %Y", localtime(&now));
printf("%s %s\n", testDate, testTime);

if (argc != 2)
    usage();

srand( (unsigned) time(NULL) );

database = needMem(16);
strcpy(database, "hg17");

chromHash = loadAllChromInfo();
chromSize = getChromSize(chrom);
startPos = getStartPos(chromSize, windowSize);
printf("%s:%d-%d\n\n", chrom, startPos, startPos + windowSize);

getMachines(argv[1]);

for (machinePos = machineList; machinePos != NULL; machinePos = machinePos->next)
    {
    dy = newDyString(256);
    dyStringPrintf(dy, "%s/cgi-bin/hgTracks?db=hg17&position=%s:%d-%d", machinePos->name, 
                   chrom, startPos, startPos + windowSize);
    elapsedTime = hgTracksRandom(dy->string);
    if (elapsedTime > 5000)
        printf("%s %ld <---\n", machinePos->name, elapsedTime);
    else
        printf("%s %ld\n", machinePos->name, elapsedTime);
    }
printf("----------------------------------\n");

/* free machine list */
return 0;
}
