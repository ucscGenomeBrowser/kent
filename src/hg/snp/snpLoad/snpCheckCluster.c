/* snpCheckCluster -- check for clustering errors. */
/* First version checks insertions only. */
/* assuming input already filtered on locType = 'between' and class = 'insertion' */
/* write to standard out */

/* assert insertions in sorted order */
/* assert end == start */

/* SKIPPING chrY because of those damned PAR SNPs */

#include "common.h"
#include "dystring.h"
#include "hdb.h"

static char const rcsid[] = "$Id: snpCheckCluster.c,v 1.4 2006/09/01 09:09:56 heather Exp $";

static char *database = NULL;
static char *snpTable = NULL;
static FILE *exceptionFileHandle = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpCheckCluster - check for clustering errors\n"
    "usage:\n"
    "    snpCheckCluster database snpTable\n");
}

boolean isValidObserved(char *observed)
{
int slashCount = 0;

if (strlen(observed) < 2) 
    return FALSE;

if (observed[0] != '-') 
    return FALSE;
    
if (observed[1] != '/') 
    return FALSE;

slashCount = chopString(observed, "/", NULL, 0);
if (slashCount > 2) 
    return FALSE;

return TRUE;
}

boolean listAllEqual(struct slName *list)
/* return TRUE if all elements in the list are the same */
/* list must have at least 2 elements */
{
struct slName *element = NULL;
char *example = NULL;

assert (list != NULL);
assert (list->next != NULL);

example = cloneString(list->name);
list = list->next;
for (element = list; element !=NULL; element = element->next)
    if (!sameString(element->name, example))
        return FALSE;
  
return TRUE;
}

void printException(char *chromName, int pos, struct slName *observedList, struct slName *nameList)
{
char exception[32];
struct slName *element = NULL;

if (listAllEqual(observedList))
    safef(exception, sizeof(exception), "DuplicateObserved");
else
    safef(exception, sizeof(exception), "MixedObserved");

for (element = nameList; element !=NULL; element = element->next)
    fprintf(exceptionFileHandle, "%s\t%d\t%d\t%s\t%s\n", chromName, pos, pos, element->name, exception);
}

int checkCluster(char *chromName)
/* detect if more than one insertion at the same location */
/* detect if all the observed strings match or not */
/* return error count */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;

boolean clusterFound = FALSE;
boolean observedMismatch = FALSE;

int pos = 0;
int posPrevious = 0;
int start = 0;
int end = 0;
int errors = 0;

char *rsId = NULL;
char *rsIdPrevious = NULL;
char *strand = NULL;
char *observed = NULL;
char *observedPrevious = NULL;
char *subString = NULL;
struct dyString *newObserved = NULL;

struct slName *observedList = NULL;
struct slName *nameList = NULL;
struct slName *element = NULL;

safef(query, sizeof(query), "select chromStart, chromEnd, name, strand, observed from %s where chrom = '%s'", 
      snpTable, chromName);

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    start = sqlUnsigned(row[0]);
    end = sqlUnsigned(row[1]);
    rsId = cloneString(row[2]);
    strand = cloneString(row[3]);
    observed = cloneString(row[4]);

    if (!isValidObserved(observed)) continue;

    if (sameString(strand, "-"))
        {
	subString = cloneString(observed);
	subString = subString + 2;
        reverseComplement(subString, strlen(subString));
        newObserved = newDyString(1024);
	dyStringAppend(newObserved, "-/");
	dyStringAppend(newObserved, subString);
	observed = cloneString(newObserved->string);
	}

    /* end must equal start */
    assert (end == start);

    /* SNPs per chrom must be sorted */
    assert (start >= pos);

    if (start > pos)
        {
	if (clusterFound)
	    {
	    errors++;
	    clusterFound = FALSE;
            printException(chromName, posPrevious, observedList, nameList);
	    }
        pos = start;
	observedPrevious = cloneString(observed);
	rsIdPrevious = cloneString(rsId);
	slFreeList(&observedList);
	slFreeList(&nameList);
	continue;
	}

    /* are we in a cluster? */
    if (start == pos) 
        {
	/* if first time through, pick up previous values */
	if (!clusterFound)
	    {
	    posPrevious = pos;
	    clusterFound = TRUE;
	    element = slNameNew(observedPrevious);
	    slAddHead(&observedList, element);
	    element = slNameNew(rsIdPrevious);
	    slAddHead(&nameList, element);
	    }
	element = slNameNew(observed);
	slAddHead(&observedList, element);
	element = slNameNew(rsId);
	slAddHead(&nameList, element);
	}

    }

if (clusterFound)
    {
    errors++;
    printException(chromName, posPrevious, observedList, nameList);
    }

sqlFreeResult(&sr);
hFreeConn(&conn);
if (errors > 0)
    verbose(1, "  %d clustered positions detected\n", errors);
return errors;
}


int main(int argc, char *argv[])
/* read snpTable, report positions with more than one insertion */
{
struct slName *chromList = NULL;
struct slName *chromPtr = NULL;
int totalErrors = 0;

if (argc != 3)
    usage();

database = argv[1];
hSetDb(database);

snpTable = argv[2];
if (!hTableExists(snpTable)) 
    errAbort("no %s table\n", snpTable);

chromList = hAllChromNames();

exceptionFileHandle = mustOpen("snpCheckCluster.tab", "w");
for (chromPtr = chromList; chromPtr != NULL; chromPtr = chromPtr->next)
    {
    if (sameString(chromPtr->name, "chrY")) continue;
    verbose(1, "chrom = %s\n", chromPtr->name);
    totalErrors = totalErrors + checkCluster(chromPtr->name);
    }

carefulClose(&exceptionFileHandle);
verbose(1, "TOTAL errors = %d\n", totalErrors);
return 0;
}
