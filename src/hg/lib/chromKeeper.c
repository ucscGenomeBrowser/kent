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
names = hAllChromNames(db);
chromCount = slCount(names);
assert(chromNames == NULL && chromRanges == NULL);
AllocArray(chromNames, chromCount);
AllocArray(chromRanges, chromCount);
for(name=names; name != NULL; name = name->next)
    {
    int size = hChromSize(db, name->name);
    chromRanges[count] = binKeeperNew(0,size);
    chromNames[count] = cloneString(name->name);
    count++;
    }
slFreeList(&names);
}

void chromKeeperInitChroms(struct slName *nameList, int maxChromSize)
/* Initialize a chrom keeper with a list of names and a size that
   will be used for each one. */
{
struct slName *name = NULL;
int count=0;
chromCount = slCount(nameList);
if(chromCount == 0)
    return;
assert(chromNames == NULL && chromRanges == NULL);
AllocArray(chromNames, chromCount);
AllocArray(chromRanges, chromCount);
for(name=nameList; name != NULL; name = name->next)
    {
    chromRanges[count] = binKeeperNew(0,maxChromSize);
    chromNames[count] = cloneString(name->name);
    count++;
    }
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
static boolean warned = FALSE;
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
if(!found && !warned)
    {
    warn("chromKeeper::chromKeeperFind() - Don't recognize chrom %s", chrom);
    warned = TRUE;
    }
return be;
}

void chromKeeperRemove(char *chrom, int chromStart, int chromEnd, void *val)
/* Remove the item from the proper chromKeeper. */
{
int i=0;
for(i=0; i<chromCount; i++)
    {
    if(sameString(chrom,chromNames[i])) 
	{
	binKeeperRemove(chromRanges[i], chromStart, chromEnd, val);
	break;
	}
    }
}
