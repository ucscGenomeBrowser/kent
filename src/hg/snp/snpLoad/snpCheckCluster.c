/* snpCheckCluster -- check for clustering errors. */
/* First version checks insertions only. */
/* assuming input already filtered on locType = 'between' and class = 'insertion' */
/* write to standard out */

/* assert insertions in sorted order */
/* assert end == start */

/* SKIPPING chrY because of those damned PAR SNPs */

#include "common.h"
#include "hdb.h"

static char const rcsid[] = "$Id: snpCheckCluster.c,v 1.2 2006/08/31 23:27:22 heather Exp $";

static char *database = NULL;
static char *snpTable = NULL;
static FILE *outputFileHandle = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpCheckCluster - check for clustering errors\n"
    "usage:\n"
    "    snpCheckCluster database snpTable\n");
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
char *observed = NULL;
char *observedPrevious = NULL;

struct slName *observedList = NULL;
struct slName *nameList = NULL;
struct slName *element = NULL;

safef(query, sizeof(query), "select chromStart, chromEnd, name, observed from %s where chrom = '%s'", 
      snpTable, chromName);

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    start = sqlUnsigned(row[0]);
    end = sqlUnsigned(row[1]);
    rsId = cloneString(row[2]);
    observed = cloneString(row[3]);

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
            fprintf(outputFileHandle, "   multiple insertions at position %d\n", posPrevious);
	    /* print list of all observed */
	    for (element = observedList; element !=NULL; element = element->next)
	        fprintf(outputFileHandle, "   observed = %s\n", element->name);
	    for (element = nameList; element !=NULL; element = element->next)
	        fprintf(outputFileHandle, "   rsId = %s\n", element->name);
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
    fprintf(outputFileHandle, "   multiple insertions at position %d\n", start);
    /* print list of all observed */
    for (element = observedList; element !=NULL; element = element->next)
        fprintf(outputFileHandle, "   observed = %s\n", element->name);
    for (element = nameList; element !=NULL; element = element->next)
        fprintf(outputFileHandle, "   rsId = %s\n", element->name);
    }

sqlFreeResult(&sr);
hFreeConn(&conn);
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

outputFileHandle = mustOpen("snpCheckCluster.out", "w");
for (chromPtr = chromList; chromPtr != NULL; chromPtr = chromPtr->next)
    {
    if (sameString(chromPtr->name, "chrY")) continue;
    verbose(1, "chrom = %s\n", chromPtr->name);
    fprintf(outputFileHandle, "chrom = %s\n", chromPtr->name);
    totalErrors = totalErrors + checkCluster(chromPtr->name);
    }

carefulClose(&outputFileHandle);
verbose(1, "TOTAL errors = %d\n", totalErrors);
return 0;
}
