/* chromKeeper.c - Wrapper around binKeepers for whole genome. */
#include "common.h"
#include "hdb.h"
#include "chromKeeper.h"
#include "binRange.h"
#include "jksql.h"

static int chromCount=0;
static char **chromNames = NULL;
struct binKeeper **chromRanges = NULL;

void chromKeeperInit(char *db)
/* Initialize the chromKeeper to a given database (hg15,mm2, etc). */
{
struct slName *names = NULL, *name = NULL;
int count=0;
hSetDb(db);
names = hAllChromNames();
chromCount = slCount(names);
AllocArray(chromNames, chromCount);
AllocArray(chromRanges, chromCount);
for(name=names; name != NULL; name = name->next)
    {
    int size = hChromSize(name->name);
    chromRanges[count] = binKeeperNew(0,size);
    chromNames[count] = cloneString(name->name);
    count++;
    }
slFreeList(&names);
}

void chromKeeperAdd(char *chrom, int chromStart, int chromEnd, void *val)
/* Add an item to the chromKeeper. */
{
int i=0;
boolean added = FALSE;
for(i=0; i<chromCount; i++)
    {
    if(sameString(chrom,chromNames[i])) 
	{
	binKeeperAdd(chromRanges[i], chromStart, chromEnd, val);
	added=TRUE;
	break;
	}
    }
if(!added)
    errAbort("chromKeeper::chromKeeperAdd() - Don't recognize chrom %s", chrom);
}

struct binElement *chromKeeperFind(char *chrom, int chromStart, int chromEnd)
/* Return a list of all items in chromKeeper that intersect range.
   Free this list with slFreeList. */
{
int i;
struct binElement *be = NULL;
boolean found = FALSE;
for(i=0; i<chromCount; i++)
    {
    if(sameString(chromNames[i], chrom))
	{
	be = binKeeperFind(chromRanges[i], chromStart, chromEnd);
	found = TRUE;
	break;
	}
    }
if(!found)
    errAbort("chromKeeper::chromKeeperFind() - Don't recognize chrom %s", chrom);
return be;
}
